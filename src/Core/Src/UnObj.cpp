#include <alloca.h>
#include <cstdio>

#include "Core.h"

TArray<UObject*>     GObjObjects;
TArray<ULinkerLoad*> GObjLoaders;
INT                  GObjBeginLoadCount;
INT                  GImportCount;
UBOOL                GIsLevelLoading;
FPeriodicCallbacks   GPeriodicCallbacks;
UBOOL                GIsGarbageCollecting;
UBOOL                GExitPurge;
FTransactionBase*    GUndo;
UObject*             GObjHash[OBJECT_HASH_BINS];
TArray<INT>          GObjAvailable;
TArray<UObject*>     GObjRoot;
UObject*             GObjTransientPkg;
INT                  GObjMaxIndex;
UBOOL                GCheckConflicts;
UObject*             GObjProfiler;
UBOOL                GObjInitialized;
UObject*             GAutoRegister;
TArray<UObject*>     GObjRegistrants;
INT                  GObjRegisterCount;
FConfigCache*        GConfig;
UBOOL                GNoGC;
DOUBLE               GTime;
FExec*               GExec;
INT                  GGarbageRefCount;
// the preference and driver lists aren't decompiled yet; StaticExit stops if they were ever filled
INT                  GObjPreferencesNum;
INT                  GObjDriversNum;
TArray<UObject*>     GObjLoaded;
// set first thing in StaticInit and cleared while a package binds its native code; exact role unknown
UBOOL                GObjStaticInitStarted;
TMap<FName, FName>*  GObjPackageRemap;

namespace
{
    // shared result buffers for the path functions when the caller passes none
    TCHAR* StaticString1024()
    {
        static TCHAR Strings[8][1024];
        static INT Next = 0;
        return Strings[Next++ & 7];
    }
}

// 0x00022c53
UObject::UObject()
{
}

// 0x0002b541
UObject::UObject(ENativeConstructor, UClass* InClass, const TCHAR* InName, const TCHAR* InPackageName, DWORD InFlags)
    : Index(INDEX_NONE)
    , HashNext(nullptr)
    , StateFrame(nullptr)
    , _Linker(nullptr)
    , Outer(nullptr)
    , ObjectFlags(InFlags | RF_Native)
    , Class(InClass)
{
    // until Register runs, the name and package are kept as raw strings
    *reinterpret_cast<const TCHAR**>(&Name) = InName;
    *static_cast<const TCHAR**>(static_cast<void*>(&Outer)) = InPackageName;

    _AutoRegisterNext = GAutoRegister;
    GAutoRegister = this;

    if (GObjInitialized && GetClass() == UObject::StaticClass())
        Register();
}

// 0x0002617d
UObject::~UObject()
{
    if (GObjProfiler)
        NOT_DECOMPILED(0x00023741);

    if (Index != INDEX_NONE && GObjInitialized && !GIsCriticalError)
    {
        ConditionalDestroy();
        UnhashObject();
        GObjObjects[Index] = nullptr;
        GObjAvailable.AddItem(Index);
    }
    delete StateFrame;
}

// 0x00025344
void UObject::Destroy()
{
    SetFlags(RF_DebugDestroy);
    ExitProperties(reinterpret_cast<BYTE*>(this), GetClass());

    if (GObjInitialized && !GIsCriticalError)
        GLogDetail->Logf(NAME_DevKill, TEXT("Destroying %s"), GetFullName());

    SetLinker(nullptr, INDEX_NONE);
    // destroyed objects keep their outer's index here; what reads it is unknown
    _LinkerIndex = Outer ? Outer->Index : 0;
}

// 0x00022d6a
void UObject::ExitProperties(BYTE* Data, UClass* Class)
{
    for (UStruct* S = Class; S; S = S->GetInheritanceSuper())
    {
        for (UProperty* P = S->PropertyLink; P; P = static_cast<UProperty*>(P->Next))
        {
            if (P->PropertyFlags & CPF_NeedCtorLink)
                P->DestroyValue(Data + P->Offset);
        }
    }
}

// 0x00022c61
void UObject::DebugWindowSizeGet(INT& Width, INT& Height) const
{
    Width = 512;
    Height = 320;
}

// 0x00022c78
UBOOL UObject::IsRuntimeStatic()
{
    return (Class->ClassFlags & CLASS_RuntimeStatic) != 0;
}

// 0x00022c88
UBOOL UObject::ConditionalDestroy()
{
    if (Index == INDEX_NONE || (ObjectFlags & (RF_Destroyed | RF_NeedLoad)))
        return 0;

    ObjectFlags = (ObjectFlags & ~RF_DebugDestroy) | RF_Destroyed;
    Destroy();
    return 1;
}

// 0x00022cb1
void UObject::ConditionalRegister()
{
    if (Index == INDEX_NONE)
        Register();
}

// 0x00022cbd
void UObject::Modify()
{
    if (GUndo && (ObjectFlags & RF_Transactional))
        GUndo->SaveObject(this);
}

// 0x00022cd4
void UObject::InitExecution()
{
    delete StateFrame;
    StateFrame = new FStateFrame(this);
    ObjectFlags |= RF_HasStack;
}

// 0x00022e00
void UObject::ConditionalShutdownAfterError()
{
    if (!(ObjectFlags & RF_ErrorShutdown))
    {
        ObjectFlags |= RF_ErrorShutdown;
        ShutdownAfterError();
    }
}

// 0x00023042
UBOOL UStruct::IsChildOf(const UStruct* SomeBase) const
{
    for (const UStruct* Struct = this; Struct; Struct = Struct->SuperField)
    {
        if (Struct == SomeBase)
            return 1;
    }
    return 0;
}

// inlined at every use in the xbox build
UBOOL UObject::IsA(UClass* SomeBase) const
{
    return Class && Class->IsChildOf(SomeBase);
}

// 0x00025258. a member on the xbox, called on possibly null pointers; static here since a null this is undefined.
UBOOL UObject::IsValid(const UObject* Object)
{
    if (!Object)
    {
        GLog->Logf(NAME_Warning, TEXT("NULL object"));
        return 0;
    }
    if (Object->Index < 0 || Object->Index >= GObjObjects.Num())
    {
        GLog->Logf(NAME_Warning, TEXT("Invalid object index %i"), Object->Index);
        GLog->Logf(NAME_Warning, TEXT("This is: %s"), Object->GetFullName());
        return 0;
    }
    if (!GObjObjects[Object->Index])
    {
        GLog->Logf(NAME_Warning, TEXT("Empty slot %i"), Object->Index);
        GLog->Logf(NAME_Warning, TEXT("This is: %s"), Object->GetFullName());
        return 0;
    }
    if (GObjObjects[Object->Index] != Object)
    {
        GLog->Logf(NAME_Warning, TEXT("Other object in slot %i"), Object->Index);
        GLog->Logf(NAME_Warning, TEXT("This is: %s"), Object->GetFullName());
        GLog->Logf(NAME_Warning, TEXT("Other is: %s"), GObjObjects[Object->Index]->GetFullName());
        return 0;
    }
    return 1;
}

// 0x000230f9
UPackage* UObject::GetOutermostPackage() const
{
    const UObject* Top = this;
    while (Top->Outer)
        Top = Top->Outer;

    return Top->IsA(UPackage::StaticClass()) ? static_cast<UPackage*>(const_cast<UObject*>(Top)) : nullptr;
}

// 0x00023117
void UObject::PostEditChange()
{
    Modify();
    if (UPackage* Package = GetOutermostPackage())
        Package->bDirty = 1;
}

// 0x0002313a
void UObject::SetLinker(ULinkerLoad* Linker, INT LinkerIndex)
{
    if (_Linker)
        _Linker->ExportMap[_LinkerIndex]._Object = nullptr;

    _Linker = Linker;
    _LinkerIndex = LinkerIndex;

    if (_Linker && _LinkerIndex >= 0)
        _Linker->ExportMap[_LinkerIndex]._Object = this;
}

// 0x000231f2
ULinkerLoad* UObject::GetLoader(INT i)
{
    return GObjLoaders[i];
}

// 0x00023b3e
const TCHAR* UObject::GetName() const
{
    // unregistered native classes still hold their name as a raw string
    if (Index == INDEX_NONE)
        return *reinterpret_cast<const TCHAR* const*>(&Name);
    return *Name;
}

// 0x00023c26
const TCHAR* UObject::GetPathName(UObject* StopOuter, TCHAR* Str) const
{
    if (!Str)
        Str = StaticString1024();

    if (this == StopOuter)
    {
        appStrcpy(Str, TEXT("None"));
        return Str;
    }

    *Str = 0;
    if (Outer != StopOuter && !GExitPurge)
    {
        Outer->GetPathName(StopOuter, Str);
        strcat(Str, TEXT("."));
    }
    strcat(Str, GetName());
    return Str;
}

// 0x00023c90
const TCHAR* UObject::GetFullName(TCHAR* Str) const
{
    // the xbox version also accepted a null this and printed "None"; callers here must check first
    if (!Str)
        Str = StaticString1024();

    snprintf(Str, 1024, TEXT("%s "), Class->GetName());
    GetPathName(nullptr, Str + appStrlen(Str));
    return Str;
}

// 0x00023ce6
void UObject::ConditionalPostLoad()
{
    if (!(ObjectFlags & RF_NeedPostLoad))
        return;

    ObjectFlags &= ~(RF_NeedPostLoad | RF_DebugPostLoad);
    PostLoad();
    if (!(ObjectFlags & RF_DebugPostLoad))
        appErrorf(TEXT("%s failed to route PostLoad"), GetFullName());
}

// 0x00023d25
void UObject::Serialize(FArchive& Ar)
{
    SetFlags(RF_DebugSerialize);

    if (Class != UClass::StaticClass())
        Ar.Preload(Class);

    // archives that don't load or save (reference collectors) see the object's own links
    if (!Ar.IsLoading() && !Ar.IsSaving())
    {
        Ar << Name;
        Ar << Outer;
        Ar << reinterpret_cast<UObject*&>(Class);
        Ar << reinterpret_cast<UObject*&>(_Linker);
    }

    if (!Ar.IsTrans())
    {
        if (ObjectFlags & RF_HasStack)
        {
            if (!StateFrame)
                StateFrame = new FStateFrame(this);

            Ar << reinterpret_cast<UObject*&>(StateFrame->Node);
            Ar << reinterpret_cast<UObject*&>(StateFrame->StateNode);
            Ar.Serialize(&StateFrame->ProbeMask, sizeof(StateFrame->ProbeMask));
            if (Ar.Ver() < 156)
            {
                INT LatentAction = StateFrame->LatentAction;
                Ar.Serialize(&LatentAction, sizeof(LatentAction));
                StateFrame->LatentAction = static_cast<_WORD>(LatentAction);
            }
            else
            {
                Ar.Serialize(&StateFrame->LatentAction, sizeof(StateFrame->LatentAction));
            }

            if (StateFrame->Node)
            {
                Ar.Preload(StateFrame->Node);
                TArray<BYTE>& Script = StateFrame->Node->Script;
                INT Offset = StateFrame->Code ? static_cast<INT>(StateFrame->Code - Script.GetData()) : INDEX_NONE;
                Ar << AR_INDEX(Offset);
                if (Offset != INDEX_NONE && (Offset < 0 || Offset >= Script.Num()))
                    appErrorf(TEXT("%s: Offset mismatch: %i %i"), GetFullName(), Offset, Script.Num());
                StateFrame->Code = Offset != INDEX_NONE ? &Script[Offset] : nullptr;
            }
            else
            {
                StateFrame->Code = nullptr;
            }
        }
        else if (StateFrame)
        {
            delete StateFrame;
            StateFrame = nullptr;
        }
    }

    // script properties: binary for in-memory archives, tagged for files
    if (Class != UClass::StaticClass())
    {
        if ((!Ar.IsLoading() && !Ar.IsSaving()) || Ar.IsTrans())
            Class->SerializeBin(Ar, reinterpret_cast<BYTE*>(this), 0);
        else
            Class->SerializeTaggedProperties(Ar, reinterpret_cast<BYTE*>(this), Class->DefaultsClass, 1);
    }

    if (Ar.IsCountingMem())
    {
        if (!(ObjectFlags & RF_Native))
            Ar.CountBytes(this, Class->GetPropertiesSize());
        if (StateFrame)
            Ar.CountBytes(StateFrame, 0x28);
    }
}

// 0x0002452f
UObject* UObject::StaticFindObjectFast(UClass* ObjectClass, UObject* InOuter, FName ObjectName, UBOOL ExactClass)
{
    if (ObjectName == NAME_None)
        return nullptr;

    const UBOOL AnyOuter = InOuter == ANY_PACKAGE;
    if (AnyOuter)
        InOuter = nullptr;

    for (UObject* Hash = GObjHash[GetObjectHash(ObjectName)]; Hash; Hash = Hash->HashNext)
    {
        if (Hash->Name != ObjectName || (!AnyOuter && Hash->Outer != InOuter))
            continue;
        if (!ObjectClass)
            return Hash;
        if (ExactClass ? Hash->Class == ObjectClass : Hash->Class->IsChildOf(ObjectClass))
            return Hash;
    }
    return nullptr;
}

// 0x00024ba6
FName UObject::MakeUniqueObjectName(UClass* Class)
{
    TCHAR Base[NAME_SIZE];
    appStrcpy(Base, Class->GetName());

    // drop any trailing digits so names count from the class name
    TCHAR* End = Base + appStrlen(Base);
    while (End > Base && End[-1] >= '0' && End[-1] <= '9')
        End--;
    *End = 0;

    TCHAR Result[NAME_SIZE];
    do
    {
        TCHAR Number[32];
        snprintf(Number, sizeof(Number), TEXT("%i"), Class->ClassUnique++);
        // appStrncpy keeps MaxLen - 1 characters
        snprintf(Result, sizeof(Result), TEXT("%.*s%s"), NAME_SIZE - 2 - appStrlen(Number), Base, Number);
    } while (FName(Result, FNAME_Find) != NAME_None);

    return FName(Result, FNAME_Add);
}

// 0x00024c62
void UObject::HashObject()
{
    // unnamed objects all share one name, so they aren't worth hashing
    if (Name == NAME_InGameTempName)
        return;

    UObject*& Bucket = GObjHash[GetObjectHash(Name)];
    HashNext = Bucket;
    Bucket = this;
}

// 0x00024c9e
void UObject::UnhashObject()
{
    if (Name == NAME_InGameTempName)
        return;

    for (UObject** Hash = &GObjHash[GetObjectHash(Name)]; *Hash; )
    {
        if (*Hash == this)
            *Hash = HashNext;
        else
            Hash = &(*Hash)->HashNext;
    }
}

// 0x00025f70
void UObject::AddObject(INT InIndex)
{
    if (InIndex == INDEX_NONE)
    {
        if (GObjAvailable.Num())
        {
            InIndex = GObjAvailable.Pop();
        }
        else
        {
            InIndex = GObjObjects.AddItem(nullptr);
            if (GObjMaxIndex < GObjObjects.Num())
                GObjMaxIndex = GObjObjects.Num();
        }
    }
    GObjObjects[InIndex] = this;
    Index = InIndex;
    HashObject();
}

// 0x00025f20
void UObject::AddToRoot()
{
    GObjRoot.AddItem(this);
}

// 0x00028087
UObject* UObject::StaticAllocateObject(UClass* InClass, UObject* InOuter, FName InName, DWORD SetFlags, UObject* Template,
    FOutputDevice* Error, UObject* Ptr, UObject* SubobjectRoot)
{
    if (InOuter == reinterpret_cast<UObject*>(-1))
        InOuter = GObjTransientPkg;

    if (!InClass)
    {
        Error->Logf(TEXT("Empty class for object %s"), *InName);
        return nullptr;
    }
    // classes may still be unregistered while registration is running
    if (InClass->GetIndex() == INDEX_NONE && !GObjRegisterCount)
    {
        Error->Logf(TEXT("Unregistered class for %s"), *InName);
        return nullptr;
    }
    if (InClass->ClassFlags & CLASS_Abstract)
    {
        Error->Logf(LocalizeError(TEXT("Abstract"), TEXT("Core")), *InName, InClass->GetName());
        return nullptr;
    }
    if (!InOuter)
    {
        if (InClass != UPackage::StaticClass())
        {
            Error->Logf(LocalizeError(TEXT("NotPackaged"), TEXT("Core")), InClass->GetName(), *InName);
            return nullptr;
        }
    }
    else if (InClass->ClassWithin && !InOuter->IsA(InClass->ClassWithin))
    {
        Error->Logf(LocalizeError(TEXT("NotWithin"), TEXT("Core")), InClass->GetName(), *InName,
            InOuter->GetClass()->GetName(), InClass->ClassWithin->GetName());
        return nullptr;
    }

    if (InClass->ClassFlags & CLASS_Transient)
        SetFlags |= RF_Transient;

    if (InName == NAME_None)
        InName = (InClass->ClassFlags & CLASS_UniqueNames) ? MakeUniqueObjectName(InClass) : FName(NAME_InGameTempName);

    if (GCheckConflicts && InName != NAME_None)
    {
        for (UObject* Hash = GObjHash[GetObjectHash(InName)]; Hash; Hash = Hash->HashNext)
        {
            if (Hash->Name == InName && Hash->Outer == InOuter && Hash->Class != InClass)
                GLog->Logf(NAME_Log, TEXT("CONFLICT: %s - %s"), Hash->GetFullName(), InClass->GetName());
        }
    }

    // an object of the same name and outer gets replaced in place
    UObject* Obj = InName == NAME_InGameTempName ? nullptr : StaticFindObjectFast(InClass, InOuter, InName);
    UClass* Cls = Obj && Obj->IsA(UClass::StaticClass()) ? static_cast<UClass*>(Obj) : nullptr;

    INT Index = INDEX_NONE;
    UClass* ClassWithin = nullptr;
    DWORD ClassFlags = 0;
    INT NativePropertiesSize = 0;
    void (*ClassConstructor)(void*) = nullptr;
    FNativeEntry* NativeFunctionTable = nullptr;

    if (!Obj)
    {
        Obj = Ptr ? Ptr : static_cast<UObject*>(appMalloc(InClass->GetPropertiesSize()));
    }
    else
    {
        if (Obj->Class != InClass)
            appErrorf(LocalizeError(TEXT("NoReplace"), TEXT("Engine")), Obj->GetFullName(), InClass->GetName());

        SetFlags |= Obj->ObjectFlags & (RF_Native | RF_ErrorShutdown | 0x100);
        Index = Obj->Index;
        if (Cls)
        {
            ClassWithin = Cls->ClassWithin;
            ClassFlags = Cls->ClassFlags & CLASS_Abstract;
            NativePropertiesSize = Cls->NativePropertiesSize;
            ClassConstructor = Cls->ClassConstructor;
            NativeFunctionTable = Cls->NativeFunctionTable;
        }

        // destroy the old object in place but keep its vtable, then take back the index it released
        void* VTable = *reinterpret_cast<void**>(Obj);
        Obj->~UObject();
        *reinterpret_cast<void**>(Obj) = VTable;
        GObjAvailable.Pop();
    }

    Obj->Index = INDEX_NONE;
    Obj->_LinkerIndex = INDEX_NONE;
    Obj->HashNext = nullptr;
    Obj->StateFrame = nullptr;
    Obj->_Linker = nullptr;
    Obj->Outer = InOuter;
    Obj->ObjectFlags = SetFlags;
    Obj->Name = InName;
    Obj->Class = InClass;

    InitProperties(reinterpret_cast<BYTE*>(Obj), InClass->GetPropertiesSize(), InClass->DefaultsClass, reinterpret_cast<BYTE*>(Template),
        InClass->GetPropertiesSize(), (SetFlags & RF_NeedLoad) ? nullptr : Obj, SubobjectRoot);

    Obj->AddObject(Index);

    if (InClass->ClassFlags & CLASS_PerObjectConfig)
    {
        Obj->LoadConfig();
        Obj->LoadLocalized();
    }

    if (Cls)
    {
        Cls->ClassWithin = ClassWithin;
        Cls->ClassFlags |= ClassFlags;
        Cls->NativePropertiesSize = NativePropertiesSize;
        Cls->PropertiesSize = NativePropertiesSize;
        Cls->ClassConstructor = ClassConstructor;
        Cls->NativeFunctionTable = NativeFunctionTable;
    }

    if (GObjProfiler)
        NOT_DECOMPILED(0x00023667);

    return Obj;
}

// 0x000284a6
UObject* UObject::StaticConstructObject(UClass* InClass, UObject* InOuter, FName InName, DWORD SetFlags, UObject* Template,
    FOutputDevice* Error, UObject* SubobjectRoot)
{
    UObject* Result = StaticAllocateObject(InClass, InOuter, InName, SetFlags, Template, Error, nullptr, SubobjectRoot);
    if (Result)
        InClass->ClassConstructor(Result);
    return Result;
}

// 0x00029090
void UObject::StaticInit()
{
    GObjStaticInitStarted = 1;
    GCheckConflicts = ParseParam(appCmdLine(), TEXT("CONFLICTS"));
    GNoGC = ParseParam(appCmdLine(), TEXT("NOGC"));

    GObjObjects.Empty(30000);
    for (UObject*& Bucket : GObjHash)
        Bucket = nullptr;
    GObjInitialized = 1;

    ProcessRegistrants();

    // the package unowned objects go into
    UObject* Transient = StaticAllocateObject(UPackage::StaticClass(), nullptr, FName(TEXT("Transient"), FNAME_Add), 0, nullptr, GError);
    GObjTransientPkg = Transient ? ::new (Transient) UPackage : nullptr;
    GObjTransientPkg->AddToRoot();

    GObjPackageRemap = new TMap<FName, FName>;
    GObjPackageRemap->Set(FName(TEXT("UnrealShare"), FNAME_Add), FName(TEXT("UnrealI"), FNAME_Add));

    GLog->Logf(NAME_Init, TEXT("Object subsystem initialized"));
}

// 0x00027453
void UObject::StaticExit()
{
    GObjTransientPkg->RemoveFromRoot();

    // everything goes, natives included
    for (INT i = 0; i < GObjObjects.Num(); i++)
    {
        if (GObjObjects[i])
            GObjObjects[i]->SetFlags(RF_Unreachable | RF_TagGarbage);
    }
    for (INT i = 0; i < FName::GetMaxNames(); i++)
    {
        if (FNameEntry* Entry = FName::GetEntry(i))
            Entry->Flags |= RF_Unreachable;
    }
    GExitPurge = 1;
    PurgeGarbage();

    GObjObjects.Empty();
    GObjLoaded.Empty();
    GObjAvailable.Empty();
    GObjLoaders.Empty();
    GObjRoot.Empty();
    GObjRegistrants.Empty();
    if (GObjPreferencesNum || GObjDriversNum)
        NOT_DECOMPILED(0x00026858);

    delete GObjPackageRemap;
    GObjPackageRemap = nullptr;
    GObjInitialized = 0;
    GLog->Logf(NAME_Exit, TEXT("Object subsystem successfully closed."));
}

// 0x000fa0c0 (TArray::RemoveItem on GObjRoot)
void UObject::RemoveFromRoot()
{
    for (INT i = 0; i < GObjRoot.Num(); i++)
    {
        if (GObjRoot[i] == this)
            GObjRoot.Remove(i--);
    }
}

// 0x000231fb
void UObject::PurgeGarbage(UBOOL OnlyFlagged)
{
    if (GNoGC)
    {
        GLog->Logf(NAME_Log, TEXT("Not purging garbage"));
        return;
    }
    GLog->Logf(NAME_Log, TEXT("Purging garbage"));
    GIsGarbageCollecting = 1;

    auto IsGarbage = [OnlyFlagged](const UObject* Obj, UBOOL AllowNative)
    {
        return Obj && (Obj->ObjectFlags & RF_Unreachable) && (!(Obj->ObjectFlags & RF_Native) || AllowNative)
            && (!OnlyFlagged || (Obj->ObjectFlags & RF_SourceModified));
    };

    // cut linkers loose from the objects about to go
    for (INT i = 0; i < GObjLoaders.Num(); i++)
    {
        ULinkerLoad* Linker = GObjLoaders[i];
        for (INT j = 0; j < Linker->ExportMap.Num(); j++)
        {
            if (IsGarbage(Linker->ExportMap[j]._Object, 0))
                Linker->DetachExport(j);
        }
        for (INT j = 0; j < Linker->ImportMap.Num(); j++)
        {
            FObjectImport& Import = Linker->ImportMap[j];
            if (IsGarbage(Import.XObject, 0))
            {
                Import.XObject = nullptr;
                Import.SourceLinker = nullptr;
                Import.SourceIndex = INDEX_NONE;
            }
        }
    }

    INT CountBefore = 0;
    INT CountPurged = 0;
    for (INT i = 0; i < GObjObjects.Num(); i++)
    {
        UObject* Obj = GObjObjects[i];
        if (Obj)
            CountBefore++;
        if (IsGarbage(Obj, GExitPurge))
        {
            Obj->ConditionalDestroy();
            CountPurged++;
        }
    }
    for (INT i = 0; i < GObjObjects.Num(); i++)
    {
        if (IsGarbage(GObjObjects[i], 0))
            delete GObjObjects[i];
    }
    GIsGarbageCollecting = 0;

    if (!OnlyFlagged)
    {
        for (INT i = 0; i < FName::GetMaxNames(); i++)
        {
            FNameEntry* Entry = FName::GetEntry(i);
            if (Entry && (Entry->Flags & RF_Unreachable) && !(Entry->Flags & RF_Native))
                FName::DeleteEntry(i);
        }
    }

    GLog->Logf(TEXT("Garbage: objects: %i->%i; refs: %i"), CountBefore, CountBefore - CountPurged, GGarbageRefCount);
}

// 0x00029b21
void UObject::Register()
{
    const TCHAR* InName = *reinterpret_cast<const TCHAR**>(&Name);
    const TCHAR* InPackageName = *static_cast<const TCHAR**>(static_cast<void*>(&Outer));

    FName(InPackageName, FNAME_Intrinsic);
    Outer = CreatePackage(nullptr, InPackageName);
    Name = FName(InName, FNAME_Intrinsic);
    _LinkerIndex = INDEX_NONE;

    if (!Outer)
        appErrorf(TEXT("Autoregistered object %s is unpackaged"), GetFullName());
    if (GetFName() == NAME_None)
        appErrorf(TEXT("Autoregistered object %s has invalid name"), GetFullName());
    if (StaticFindObjectFast(nullptr, Outer, GetFName()))
        appErrorf(TEXT("Autoregistered object %s already exists"), GetFullName());

    AddObject(INDEX_NONE);
}

// 0x00025b45
void UObject::ProcessRegistrants()
{
    // a registration can construct more native objects, so the list is only drained at the outermost call
    if (++GObjRegisterCount == 1)
    {
        for (; GAutoRegister; GAutoRegister = GAutoRegister->_AutoRegisterNext)
            GObjRegistrants.AddItem(GAutoRegister);
        GAutoRegister = nullptr;

        for (INT i = 0; i < GObjRegistrants.Num(); i++)
            GObjRegistrants[i]->ConditionalRegister();
        GObjRegistrants.Empty();
    }
    GObjRegisterCount--;
}

// 0x0002b5af
UClass* UObject::StaticLoadClass(UClass* BaseClass, UObject* InOuter, const TCHAR* InName, const TCHAR* Filename, DWORD LoadFlags, UPackageMap* Sandbox)
{
    UClass* Class = static_cast<UClass*>(StaticLoadObject(UClass::StaticClass(), InOuter, InName, Filename, LoadFlags | LOAD_Throw, Sandbox));
    if (Class && !Class->IsChildOf(BaseClass))
        appThrowf(LocalizeError(TEXT("LoadClassMismatch"), TEXT("Core")), Class->GetFullName(), BaseClass->GetFullName());
    return Class;
}

// 0x0002b11b. the xbox caught load errors here; in the port appThrowf is fatal, so there is nothing to catch.
UObject* UObject::StaticLoadObject(UClass* ObjectClass, UObject* InOuter, const TCHAR* InName, const TCHAR* Filename, DWORD LoadFlags, UPackageMap* Sandbox)
{
    if (!InOuter && Filename)
        InOuter = StaticFindObject(UPackage::StaticClass(), nullptr, Filename);

    ResolveName(InOuter, InName, 1, 1);
    UObject* Result = StaticFindObject(ObjectClass, InOuter, InName);
    if (Result && (Result->GetFlags() & (RF_NeedLoad | RF_WasLoaded)))
        return Result;

    BeginLoad();
    Result = nullptr;

    UObject* TopOuter = InOuter;
    while (TopOuter && TopOuter->GetOuter())
        TopOuter = TopOuter->GetOuter();

    if (!(LoadFlags & LOAD_NoFiles))
    {
        // inside a nested load, the newest loader of a flagged package gets the first try
        if (InOuter && !(LoadFlags & LOAD_Verify) && GObjBeginLoadCount > 1)
        {
            for (INT i = GObjLoaders.Num() - 1; i >= 0; i--)
            {
                ULinkerLoad* Linker = GetLoader(i);
                if (Linker->ArUnknown16)
                {
                    Result = Linker->Create(ObjectClass, FName(InName, FNAME_Add), InOuter, LoadFlags, 0);
                    break;
                }
            }
        }

        if (!Result)
        {
            if (ULinkerLoad* Linker = GetPackageLinker(TopOuter, Filename, LoadFlags | LOAD_AllowDll, Sandbox, nullptr))
            {
                const INT LoadedCount = GObjLoaded.Num();
                Result = Linker->Create(ObjectClass, FName(InName, FNAME_Add), InOuter, LoadFlags, 0);
                if (Result && !(LoadFlags & LOAD_Verify))
                {
                    if (GObjBeginLoadCount == 1 && GObjLoaded.Num() > LoadedCount)
                        GLog->Logf(NAME_Warning, TEXT("Dynamic load %s"), Result->GetPathName());
                    if (Linker->ArUnknown16)
                        Linker->LoadAllObjects();
                }
            }
        }
    }

    if (!Result)
    {
        Result = StaticFindObject(ObjectClass, InOuter, InName);
        if (!Result && !(LoadFlags & LOAD_NoThrow))
            appThrowf(LocalizeError(TEXT("ObjectNotFound"), TEXT("Core")), ObjectClass->GetName(), InOuter ? InOuter->GetPathName() : TEXT("None"), InName);
    }

    EndLoad();
    return Result;
}

// 0x0002b3a3. the xbox catches load errors here and reports them as FailedLoad; appThrowf is fatal in the port.
UPackage* UObject::LoadPackage(UObject* InOuter, const TCHAR* Filename, DWORD LoadFlags)
{
    UPackage* Result = nullptr;
    // the xbox assigns over the "package " prefix rather than appending to it
    FString Context(TEXT("package "));
    if (InOuter)
        Context = InOuter->GetPathName();
    else if (Filename && *Filename)
        Context = Filename;
    BeginLoad(Context);

    if (!(LoadFlags & (LOAD_NoThrow | LOAD_Quiet)))
        LoadFlags |= LOAD_Throw;

    if (ULinkerLoad* Linker = GetPackageLinker(InOuter, Filename, LoadFlags, nullptr, nullptr))
    {
        if (Filename && *Filename)
            Result = static_cast<UPackage*>(StaticFindObject(UPackage::StaticClass(), nullptr, Filename));
        if (!Result && Linker->LinkerRoot && Linker->LinkerRoot->IsA(UPackage::StaticClass()))
            Result = static_cast<UPackage*>(Linker->LinkerRoot);
        // a package with bDirty set is not loaded again
        if (Result && !Result->GetOutermostPackage()->bDirty && !(LoadFlags & LOAD_Verify))
            Linker->LoadAllObjects();
    }
    else if (!(LoadFlags & LOAD_Verify))
    {
        if (InOuter)
            Result = InOuter->IsA(UPackage::StaticClass()) ? static_cast<UPackage*>(InOuter) : nullptr;
        else
            Result = CreatePackage(nullptr, Filename);
    }

    EndLoad();
    if ((LoadFlags & LOAD_Quiet) && Result)
        Result->GetOutermostPackage()->bDirty = 1;
    return Result;
}

// 0x000251ba
void UObject::Rename(const TCHAR* InName, UObject* NewOuter)
{
    const FName NewName = InName ? FName(InName, FNAME_Add) : MakeUniqueObjectName(Class);
    UnhashObject();
    if (Name != NewName)
        GLogDetail->Logf(TEXT("Renaming %s to %s"), *Name, *NewName);
    Name = NewName;
    if (NewOuter)
        Outer = NewOuter;
    HashObject();
}

// 0x0002921a
UBOOL UObject::ResolveName(UObject*& InPackage, const TCHAR*& InName, UBOOL Create, UBOOL Throw)
{
    // ini:Section.Key and usr:Section.Key read the real name from System.ini or User.ini
    const UBOOL IsIni = !appStrnicmp(InName, TEXT("ini:"), 4);
    const UBOOL IsUsr = !appStrnicmp(InName, TEXT("usr:"), 4);
    if ((IsIni || IsUsr) && appStrlen(InName) < 1024 && appStrstr(InName, TEXT(".")))
    {
        TCHAR Temp[256];
        appStrncpy(Temp, InName + 4, ARRAY_COUNT(Temp));
        TCHAR* Key = Temp;
        while (appStrstr(Key, TEXT(".")))
            Key = appStrstr(Key, TEXT(".")) + 1;
        Key[-1] = 0;

        const TCHAR* Found = GConfig->GetStr(Temp, Key, IsIni ? TEXT("System") : TEXT("User"));
        if (!Found)
        {
            if (Throw)
                appThrowf(LocalizeError(TEXT("ConfigNotFound"), TEXT("Core")), InName);
            return 0;
        }
        InName = Found;
    }

    // each dotted prefix is a package, created or found
    while (const TCHAR* Dot = appStrstr(InName, TEXT(".")))
    {
        TCHAR PartialName[256];
        appStrncpy(PartialName, InName, ARRAY_COUNT(PartialName));
        *appStrstr(PartialName, TEXT(".")) = 0;

        if (Create)
        {
            InPackage = CreatePackage(InPackage, PartialName);
        }
        else
        {
            UObject* NewPackage = StaticFindObject(UPackage::StaticClass(), InPackage, PartialName);
            if (!NewPackage)
                NewPackage = StaticFindObject(UObject::StaticClass(), InPackage, PartialName);
            if (!NewPackage)
                return 0;
            InPackage = NewPackage;
        }
        InName = Dot + 1;
    }
    return 1;
}

// 0x000245c9
UObject* UObject::StaticFindObjectChecked(UClass* ObjectClass, UObject* InOuter, const TCHAR* Name, UBOOL ExactClass)
{
    UObject* Result = StaticFindObject(ObjectClass, InOuter, Name, ExactClass);
    if (!Result)
    {
        const TCHAR* OuterName = InOuter == ANY_PACKAGE ? TEXT("Any") : InOuter ? InOuter->GetName() : TEXT("None");
        GError->Logf(LocalizeError(TEXT("ObjectNotFound"), TEXT("Core")), ObjectClass->GetName(), OuterName, Name);
    }
    return Result;
}

UObject* UObject::StaticFindObject(UClass* ObjectClass, UObject* InObjectPackage, const TCHAR* OrigInName, UBOOL ExactClass)
{
    const UBOOL AnyPackage = InObjectPackage == ANY_PACKAGE;
    UObject* Package = AnyPackage ? nullptr : InObjectPackage;
    const TCHAR* InName = OrigInName;
    if (!ResolveName(Package, InName, 0, 0))
        return nullptr;

    const FName ObjectName(InName, FNAME_Find);
    if (ObjectName == NAME_None)
        return nullptr;

    for (UObject* Hash = GObjHash[GetObjectHash(ObjectName)]; Hash; Hash = Hash->HashNext)
    {
        if (Hash->Name != ObjectName || !((!Package && AnyPackage) || Hash->Outer == Package))
            continue;
        if (!ObjectClass)
            return Hash;
        if (ExactClass ? Hash->Class == ObjectClass : Hash->Class->IsChildOf(ObjectClass))
            return Hash;
    }
    return nullptr;
}

// 0x000291c4
UPackage* UObject::CreatePackage(UObject* InOuter, const TCHAR* InName)
{
    ResolveName(InOuter, InName, 1, 0);
    UPackage* Result = static_cast<UPackage*>(StaticFindObject(UPackage::StaticClass(), InOuter, InName));
    if (!Result)
    {
        Result = new(InOuter, FName(InName, FNAME_Add), RF_Public) UPackage;
    }
    return Result;
}

// 0x000267e3
UPackage::UPackage()
    : DllHandle(nullptr)
    , AttemptedBind(0)
    , PackageFlags(0)
    , bDirty(0)
{
    BindPackage();
}

// 0x00025c10
void UPackage::BindPackage()
{
    if (DllHandle || GetOuter() || AttemptedBind)
        return;

    TCHAR Filename[256];
    appSprintf(Filename, TEXT("%s%s"), appBaseDir(), GetName());
    AttemptedBind = 1;
    GObjStaticInitStarted = 0;
    DllHandle = appGetDllHandle(Filename);
    GObjStaticInitStarted = 1;
    if (DllHandle)
        ProcessRegistrants();
}

// 0x000997ac
void UStruct::AddCppProperty(UProperty* Property)
{
    Property->Next = Children;
    Children = Property;
}

void UField::AddCppProperty(UProperty* Property)
{
    appErrorf(TEXT("UField::AddCppProperty"));
}

// 0x00099768
UClass* UField::GetOwnerClass()
{
    UObject* Obj = this;
    while (Obj->GetClass() != UClass::StaticClass())
        Obj = Obj->GetOuter();
    return static_cast<UClass*>(Obj);
}

INT UField::GetPropertiesSize()
{
    return 0;
}

// 0x0002697f
const TCHAR* UStruct::GetNameCPP()
{
    TCHAR* Result = StaticString1024();
    snprintf(Result, 1024, TEXT("F%s"), GetName());
    return Result;
}

// 0x00024cde
void UTextBuffer::Serialize(FArchive& Ar)
{
    UObject::Serialize(Ar);
    Ar << Pos << Top << Text;
}

// 0x000bd659
UBOOL UObject::IsInState(FName StateName)
{
    if (StateFrame)
    {
        for (UState* State = StateFrame->StateNode; State; State = static_cast<UState*>(State->SuperField))
        {
            if (State->GetFName() == StateName)
                return 1;
        }
    }
    return 0;
}

// 0x000bd85e
UBOOL UObject::GotoLabel(FName Label)
{
    if (!StateFrame)
        return 0;

    StateFrame->LatentAction = 0;
    if (Label != NAME_None)
    {
        for (UState* State = StateFrame->StateNode; State; State = static_cast<UState*>(State->SuperField))
        {
            if (State->LabelTableOffset == 0xFFFF || !State->Script.Num())
                continue;

            for (FLabelEntry* Entry = reinterpret_cast<FLabelEntry*>(&State->Script[State->LabelTableOffset]);
                 Entry->Name != NAME_None; Entry++)
            {
                if (Entry->Name == Label)
                {
                    StateFrame->Node = State;
                    StateFrame->Code = &State->Script[Entry->iCode];
                    return 1;
                }
            }
        }
    }
    StateFrame->Code = nullptr;
    return 0;
}

// 0x000997a7
void UField::Register()
{
    UObject::Register();
}

// 0x0009977a
void UField::PostLoad()
{
    UObject::PostLoad();
    Bind();
}

// 0x000997bc
void UStruct::Register()
{
    UField::Register();
    if (SuperField)
        SuperField->ConditionalRegister();
    FriendlyName = GetFName();
}

// 0x0009982f
void UStruct::PostLoad()
{
    UField::PostLoad();
}

// 0x000be9d1. calls the delegate's function, or the object's own DelegateName when the delegate is unset or its
// object is being destroyed.
void UObject::ProcessDelegate(FName DelegateName, FScriptDelegate* Delegate, void* Parms, void* Result)
{
    if (Delegate->Object)
    {
        if (Delegate->Object->IsPendingKill())
        {
            Delegate->Object = nullptr;
            Delegate->FunctionName = NAME_None;
        }
        if (Delegate->Object)
        {
            Delegate->Object->ProcessEvent(Delegate->Object->FindFunctionChecked(Delegate->FunctionName), Parms, Result);
            return;
        }
    }
    ProcessEvent(FindFunctionChecked(DelegateName), Parms, Result);
}

// 0x0009a596. clears references to destroyed objects in every property that can hold one.
void UStruct::CleanupDestroyed(BYTE* Data)
{
    for (UStruct* S = this; S; S = S->GetInheritanceSuper())
    {
        for (UProperty* P = S->PropertyLink; P; P = static_cast<UProperty*>(P->Next))
        {
            if (P->PropertyFlags & CPF_HasObjectRef)
                P->CleanupDestroyed(Data + P->Offset);
        }
    }
}

// 0x000231b1. a state of the class, or of a parent, named StateName.
UState* UObject::FindState(FName StateName)
{
    UStruct* Found = FindFunction(StateName, 1);
    return Found && (Found->GetClass()->ClassFlags & CLASS_IsAUState) ? static_cast<UState*>(Found) : nullptr;
}

// whether calling the probe function ProbeName would do anything. inlined on the xbox.
UBOOL UObject::IsProbing(FName ProbeName)
{
    const INT Index = ProbeName.GetIndex();
    return Index < NAME_PROBEMIN || Index >= NAME_PROBEMAX || !StateFrame
        || (StateFrame->ProbeMask & (static_cast<QWORD>(1) << (Index - NAME_PROBEMIN)));
}

// 0x000bea8a. 'Auto' means the class's first auto state; a missing state leaves the object in no state.
// EndState and BeginState may switch state themselves, which preempts this switch.
EGotoState UObject::GotoState(FName NewState)
{
    if (!StateFrame)
        return GOTOSTATE_NotFound;

    StateFrame->LatentAction = 0;
    const FName OldStateName = StateFrame->StateNode != Class ? StateFrame->StateNode->GetFName() : FName(NAME_None);

    UState* StateNode = nullptr;
    if (NewState == NAME_Auto)
    {
        for (TFieldIterator<UState> It(GetClass()); It && !StateNode; ++It)
        {
            if (It->StateFlags & STATE_Auto)
                StateNode = *It;
        }
    }
    else
    {
        StateNode = FindState(NewState);
    }

    if (!StateNode)
    {
        NewState = NAME_None;
        StateNode = GetClass();
    }
    else if (NewState == NAME_Auto)
    {
        NewState = StateNode->GetFName();
    }

    if (OldStateName != NAME_None && NewState != OldStateName && IsProbing(FName(NAME_EndState)) && !(ObjectFlags & RF_InEndState))
    {
        ObjectFlags = (ObjectFlags & ~RF_StateChanged) | RF_InEndState;
        ProcessEvent(FindFunctionChecked(FName(NAME_EndState)), nullptr);
        ObjectFlags &= ~RF_InEndState;
        if (ObjectFlags & RF_StateChanged)
            return GOTOSTATE_Preempted;
    }

    StateFrame->Node = StateNode;
    StateFrame->StateNode = StateNode;
    StateFrame->Code = nullptr;
    StateFrame->ProbeMask = (GetClass()->ProbeMask | StateNode->ProbeMask) & StateNode->IgnoreMask;

    if (NewState == NAME_None)
        return GOTOSTATE_NotFound;

    if (NewState != OldStateName && IsProbing(FName(NAME_BeginState)))
    {
        ObjectFlags &= ~RF_StateChanged;
        ProcessEvent(FindFunctionChecked(FName(NAME_BeginState)), nullptr);
        if (ObjectFlags & RF_StateChanged)
            return GOTOSTATE_Preempted;
    }
    ObjectFlags |= RF_StateChanged;
    return GOTOSTATE_Success;
}

// 0x00026486
void UObject::LanguageChange()
{
    LoadLocalized();
}

// 0x000273dc. Class'Package.Name'
FString UObject::GetDescription() const
{
    return FString::Printf(TEXT("%s'%s'"), GetClass()->GetName(), GetPathName());
}

// 0x0002318f
UFunction* UObject::FindFunctionGlobal(FName InName, UBOOL Global)
{
    UStruct* Found = FindFunction(InName, Global);
    return Found && (Found->GetClass()->ClassFlags & CLASS_IsAUFunction) ? static_cast<UFunction*>(Found) : nullptr;
}

// 0x0002425a. runs an exec function typed at the console: its name, then each parameter as text. a first
// object parameter that Executor fits is given Executor instead.
UBOOL UObject::ScriptConsoleExec(const TCHAR* Str, FOutputDevice& Ar, UObject* Executor)
{
    if (!GIsScriptable)
        return 0;

    TCHAR MsgStr[64];
    if (!ParseToken(Str, MsgStr, ARRAY_COUNT(MsgStr), 1))
        return 0;
    const FName Message(MsgStr, FNAME_Find);
    if (Message == NAME_None)
        return 0;
    UFunction* Function = FindFunctionGlobal(Message);
    if (!Function || !(Function->FunctionFlags & FUNC_Exec))
        return 0;

    BYTE* Parms = Function->ParmsSize ? static_cast<BYTE*>(alloca(Function->ParmsSize)) : nullptr;
    if (Parms)
        memset(Parms, 0, Function->ParmsSize);

    UBOOL Call = 1;
    INT Count = 0;
    for (TFieldIterator<UProperty> It(Function); It && (It->PropertyFlags & (CPF_Parm | CPF_ReturnParm)) == CPF_Parm; ++It, Count++)
    {
        if (Count == 0 && Executor)
        {
            UObjectProperty* Op = Cast<UObjectProperty>(*It);
            if (Op && (Executor->IsA(Op->PropertyClass) || !Op->PropertyClass))
            {
                *reinterpret_cast<UObject**>(Parms + It->Offset) = Executor;
                continue;
            }
        }
        ParseNext(&Str);
        if (!*Str)
        {
            if (!(It->PropertyFlags & CPF_OptionalParm))
            {
                Ar.Logf(LocalizeError(TEXT("BadProperty"), TEXT("Core")), *Message, It->GetName());
                Call = 0;
            }
            break;
        }
        Str = It->ImportText(Str, Parms + It->Offset, 1);
    }

    if (Call)
        ProcessEvent(Function, Parms);
    for (TFieldIterator<UProperty> It(Function); It && (It->PropertyFlags & (CPF_Parm | CPF_ReturnParm)) == CPF_Parm; ++It)
        It->DestroyValue(Parms + It->Offset);
    return 1;
}

// 0x000259ab. text written to the buffer is appended, whatever its event.
void UTextBuffer::Serialize(const TCHAR* Data, EName Event)
{
    Text += Data;
}

// 0x0009a3f6
void UStruct::Destroy()
{
    Script.Empty();
    UObject::Destroy();
}

// 0x0009b29f
void UClass::Destroy()
{
    NetFields.Empty();
    if (Defaults.GetData())
        ExitProperties(Defaults.GetData(), this);
    Defaults.Empty();
    UState::Destroy();
}

// everything below is waiting to be decompiled

UBOOL UObject::IsDefaultValue(const FPropertyInstance& Property) { NOT_DECOMPILED(0x00023172); return 0; }

// inlined at every use in the xbox build
UObject* UObject::GetTransientPackage()
{
    return GObjTransientPkg;
}

// 0x00036c92
FPeriodicCallbacks::FPeriodicCallbacks()
    : LastTime(appSeconds())
    , Interval(0.0)
{
}

// 0x00036933
void FPeriodicCallbacks::Tick()
{
    const DOUBLE Now = appSeconds();
    if (LastTime > Now || Now - LastTime > Interval)
    {
        for (INT i = 0; i < Callbacks.Num(); i++)
            Callbacks[i]();
        LastTime = appSeconds();
    }
}

// 0x00035b40
const TCHAR* FGuid::String() const
{
    static TCHAR Result[33];
    appSprintf(Result, TEXT("%08X%08X%08X%08X"), A, B, C, D);
    return Result;
}

// the current language's file extension
static TCHAR GLanguage[64] = TEXT("int");

// 0x00022fd5
const TCHAR* UObject::GetLanguage()
{
    return GLanguage;
}

// 0x00027387
void UObject::PostLoad()
{
    ObjectFlags |= RF_DebugPostLoad;
    // swrc localizes every loaded object by its own name, except scripted prompt actions
    if (Class && appStricmp(Class->GetName(), TEXT("ACTION_ShowPrompt")))
        ObjectFlags |= RF_PerObjectLocalized;
    if (ObjectFlags & RF_PerObjectLocalized)
        LoadLocalized();
}

// 0x000366ae. looks Section.Key up in Package.LangExt, falling back to the int files.
const TCHAR* Localize(const TCHAR* Section, const TCHAR* Key, const TCHAR* Package, const TCHAR* LangExt, UBOOL Optional)
{
    TCHAR* Result = StaticString1024();
    if (!GIsStarted || !GConfig)
        return Key;
    if (!LangExt)
        LangExt = UObject::GetLanguage();

    TCHAR Filename[256];
    appSprintf(Filename, TEXT("%s.%s"), Package, LangExt);
    if (GConfig->GetString(Section, Key, Result, 1024, Filename))
        return Result;

    while (appStricmp(LangExt, TEXT("int")))
    {
        LangExt = TEXT("int");
        appSprintf(Filename, TEXT("%s.%s"), Package, LangExt);
        if (GConfig->GetString(Section, Key, Result, 1024, Filename))
            return Result;
    }
    if (!Optional)
    {
        GLog->Logf(NAME_Localization, TEXT("No localization: %s.%s.%s (%s)"), Package, Section, Key, LangExt);
        appSprintf(Result, TEXT("<?%s?%s.%s.%s?>"), LangExt, Package, Section, Key);
    }
    return Result;
}

// 0x000367d1
const TCHAR* LocalizeError(const TCHAR* Key, const TCHAR* Package, const TCHAR* LangExt)
{
    return Localize(TEXT("Errors"), Key, Package, LangExt, 0);
}

// 0x000259ba
void UObject::SetLanguage(const TCHAR* LanguageExt)
{
    if (!appStricmp(LanguageExt, GLanguage))
        return;

    appStrcpy(GLanguage, LanguageExt);
    appStrcpy(GNone, LocalizeGeneral(TEXT("None"), TEXT("Core")));
    appStrcpy(GTrue, LocalizeGeneral(TEXT("True"), TEXT("Core")));
    appStrcpy(GFalse, LocalizeGeneral(TEXT("False"), TEXT("Core")));
    appStrcpy(GYes, LocalizeGeneral(TEXT("Yes"), TEXT("Core")));
    appStrcpy(GNo, LocalizeGeneral(TEXT("No"), TEXT("Core")));

    for (INT i = 0; i < GObjObjects.Num(); i++)
    {
        if (GObjObjects[i])
            GObjObjects[i]->LanguageChange();
    }
}

// 0x00024a0c
void UObject::ResetLoaders(UObject* Pkg, UBOOL DynamicOnly, UBOOL ForceLazyLoad)
{
    for (INT i = GObjLoaders.Num() - 1; i >= 0; i--)
    {
        ULinkerLoad* Linker = GetLoader(i);
        if (Pkg && Linker->LinkerRoot != Pkg)
        {
            // other linkers forget imports that resolved through Pkg's linker
            for (INT j = 0; j < Linker->ImportMap.Num(); j++)
            {
                FObjectImport& Import = Linker->ImportMap[j];
                if (Import.SourceLinker && Import.SourceLinker->LinkerRoot == Pkg)
                {
                    Import.SourceLinker = nullptr;
                    Import.SourceIndex = INDEX_NONE;
                }
            }
        }
        else if (DynamicOnly)
        {
            for (INT j = 0; j < Linker->ExportMap.Num(); j++)
            {
                UObject* Object = Linker->ExportMap[j]._Object;
                if (Object && !Object->IsRuntimeStatic())
                    Linker->DetachExport(j);
            }
        }
        else if (ForceLazyLoad)
        {
            Linker->DetachAllLazyLoaders(1);
        }
    }

    if (!DynamicOnly)
    {
        for (INT i = GObjLoaders.Num() - 1; i >= 0; i--)
        {
            ULinkerLoad* Linker = GetLoader(i);
            if ((!Pkg || Linker->LinkerRoot == Pkg) && Linker)
                delete Linker;
        }
    }
}

// 0x000277f7. a non-empty context names a slow task shown while the outermost load runs.
void UObject::BeginLoad(const FString& Context)
{
    if (++GObjBeginLoadCount == 1 && Context.Len())
        GWarn->BeginSlowTask(*(FString(TEXT("Loading ")) + Context), 0);
}

// 0x00025530
void UObject::EndLoad()
{
    if (GObjBeginLoadCount == 1 && GObjLoaded.Num())
    {
        // preloading can create more exports, so repeat until a pass loads nothing
        INT Done = 0;
        for (;;)
        {
            INT Count = 0;
            for (INT i = 0; i < GObjLoaders.Num(); i++)
            {
                ULinkerLoad* Linker = GetLoader(i);
                for (INT j = 0; j < Linker->ExportMap.Num(); j++)
                {
                    UObject* Object = Linker->ExportMap[j]._Object;
                    if (Object && (Object->GetFlags() & RF_NeedLoad))
                    {
                        GWarn->StatusUpdatef(Done + Count, GObjLoaded.Num() + 32, nullptr);
                        Count++;
                        Linker->Preload(Object);
                    }
                }
            }
            if (!Count)
                break;
            Done += Count;
        }

        for (INT i = 0; i < GObjLoaded.Num(); i++)
            GObjLoaded[i]->ConditionalPostLoad();

        GObjLoaded.Empty();
        GImportCount = 0;
    }

    if (--GObjBeginLoadCount == 0)
        GWarn->EndSlowTask();
}

UBOOL ParseObject(const TCHAR* Stream, const TCHAR* Match, UClass* Class, UObject*& DestRes, UObject* InParent)
{
    TCHAR TempStr[256];
    if (!Parse(Stream, Match, TempStr, 64))
        return 0;
    if (!appStricmp(TempStr, TEXT("NONE")))
    {
        DestRes = nullptr;
        return 1;
    }
    UObject* Res = UObject::StaticFindObject(Class, InParent, TempStr);
    if (!Res)
        return 0;
    DestRes = Res;
    return 1;
}

void UObject::GlobalSetProperty(const TCHAR* Value, UClass* Class, UProperty* Property, INT Offset, UBOOL Immediate)
{
    if (Immediate)
    {
        for (INT i = 0; i < GObjObjects.Num(); i++)
        {
            UObject* Obj = GObjObjects[i];
            if (Obj && (Obj->IsA(Class) || !Class))
            {
                Property->ImportText(Value, reinterpret_cast<BYTE*>(Obj) + Offset, 1);
                Obj->PostEditChange();
            }
        }
    }
    Property->ImportText(Value, reinterpret_cast<BYTE*>(Class->GetDefaultObject()) + Offset, 1);
    Class->GetDefaultObject()->SaveConfig(CPF_Config, nullptr, nullptr);
}

namespace
{
    // 0x00025413. logs the class tree under Class, indented by depth; the real name is not known.
    void ShowClasses(UClass* Class, FOutputDevice& Ar, INT Indent)
    {
        Ar.Logf(TEXT("%s%s"), appSpc(Indent), Class->GetName());
        for (INT i = 0; i < GObjObjects.Num(); i++)
        {
            UClass* Child = Cast<UClass>(GObjObjects[i]);
            if (Child && Child->GetSuperClass() == Class)
                ShowClasses(Child, Ar, Indent + 2);
        }
    }
}

UBOOL UObject::StaticExec(const TCHAR* Cmd, FOutputDevice& Ar)
{
    const TCHAR* Str = Cmd;
    // the allocator commands call the xbox allocator, which the port does not have
    if (ParseCommand(&Str, TEXT("MEM")))
    {
        NOT_DECOMPILED(0x00029c1c);
    }
    else if (ParseCommand(&Str, TEXT("DUMPALLOCSBYSIZE")))
    {
        INT Count = 4;
        if (appStrcmp(Cmd, TEXT("")))
            Count = appAtoi(Str);
        (void)Count;
        NOT_DECOMPILED(0x00029c61);
    }
    else if (ParseCommand(&Str, TEXT("INCWATERMARK")))
    {
        NOT_DECOMPILED(0x00029c83);
    }
    else if (ParseCommand(&Str, TEXT("DECWATERMARK")))
    {
        NOT_DECOMPILED(0x00029ca4);
    }
    else if (ParseCommand(&Str, TEXT("WATERMARK")))
    {
        NOT_DECOMPILED(0x00029cc8);
    }
    else if (ParseCommand(&Str, TEXT("NAMECOUNT")))
    {
        GLog->Logf(TEXT("Max names: %d, Names in use: %d"), FName::GetMaxNames(), FName::GetNumNames());
    }
    else if (ParseCommand(&Str, TEXT("DUMPALLOCPROFILE")))
    {
        NOT_DECOMPILED(0x00029d5b);
    }
    else if (ParseCommand(&Str, TEXT("RESETALLOCPROFILE")))
    {
        NOT_DECOMPILED(0x00029da8);
    }
    else if (ParseCommand(&Str, TEXT("DUMPOBJECTPROFILE")))
    {
        // the object profiler is not decompiled yet
        NOT_DECOMPILED(0x00022f1f);
    }
    else if (ParseCommand(&Str, TEXT("STARTOBJECTPROFILE")))
    {
        NOT_DECOMPILED(0x00025d13);
    }
    else if (ParseCommand(&Str, TEXT("DUMPFREELIST")))
    {
        NOT_DECOMPILED(0x00029e3f);
    }
    else if (ParseCommand(&Str, TEXT("DUMPNATIVES")))
    {
        for (INT i = 0; i < MAX_NATIVES; i++)
            if (GNatives[i] == &UObject::execUndefined)
                GLog->Logf(TEXT("Native index %i is available"), i);
    }
    else if (ParseCommand(&Str, TEXT("GET")))
    {
        TCHAR ClassName[256];
        TCHAR PropertyName[256];
        UClass* Class;
        UProperty* Property;
        if (!ParseToken(Str, ClassName, ARRAY_COUNT(ClassName), 1)
            || (Class = Cast<UClass>(StaticFindObject(UClass::StaticClass(), ANY_PACKAGE, ClassName))) == nullptr)
        {
            Ar.Logf(NAME_ExecWarning, TEXT("Unrecognized class %s"), ClassName);
        }
        else if (!ParseToken(Str, PropertyName, ARRAY_COUNT(PropertyName), 1)
            || (Property = FindField<UProperty>(Class, PropertyName)) == nullptr)
        {
            Ar.Logf(NAME_ExecWarning, TEXT("Unrecognized property %s"), PropertyName);
        }
        else
        {
            TCHAR Temp[256] = {};
            if (Class->Defaults.Num())
            {
                BYTE* Defaults = reinterpret_cast<BYTE*>(Class->GetDefaultObject());
                Property->ExportText(0, Temp, Defaults, Defaults, 1);
            }
            Ar.Log(Temp);
        }
    }
    else if (ParseCommand(&Str, TEXT("SET")))
    {
        TCHAR ClassName[256];
        TCHAR PropertyName[256];
        UClass* Class;
        UProperty* Property;
        if (!ParseToken(Str, ClassName, ARRAY_COUNT(ClassName), 1)
            || (Class = Cast<UClass>(StaticFindObject(UClass::StaticClass(), ANY_PACKAGE, ClassName))) == nullptr)
        {
            Ar.Logf(NAME_ExecWarning, TEXT("Unrecognized class %s"), ClassName);
        }
        else if (!ParseToken(Str, PropertyName, ARRAY_COUNT(PropertyName), 1)
            || (Property = FindField<UProperty>(Class, PropertyName)) == nullptr)
        {
            Ar.Logf(NAME_ExecWarning, TEXT("Unrecognized property %s"), PropertyName);
        }
        else
        {
            while (*Str == ' ')
                Str++;
            GlobalSetProperty(Str, Class, Property, Property->Offset, 1);
        }
    }
    else if (ParseCommand(&Str, TEXT("OBJ")))
    {
        if (ParseCommand(&Str, TEXT("GARBAGE")))
        {
            // collects even with -NOGC
            const UBOOL SavedNoGC = GNoGC;
            GNoGC = 0;
            // CollectGarbage(RF_Native)
            NOT_DECOMPILED(0x00025946);
            GNoGC = SavedNoGC;
        }
        else if (ParseCommand(&Str, TEXT("MARK")))
        {
            // the mark is the bit ShutdownAfterError also uses
            GLog->Logf(TEXT("Marking objects"));
            for (INT i = 0; i < GObjObjects.Num(); i++)
                if (GObjObjects[i])
                    GObjObjects[i]->ObjectFlags |= RF_ErrorShutdown;
        }
        else if (ParseCommand(&Str, TEXT("MARKCHECK")))
        {
            GLog->Logf(TEXT("Unmarked objects:"));
            for (INT i = 0; i < GObjObjects.Num(); i++)
                if (GObjObjects[i] && !(GObjObjects[i]->ObjectFlags & RF_ErrorShutdown))
                    GLog->Logf(TEXT("%s"), GObjObjects[i]->GetFullName());
        }
        else if (ParseCommand(&Str, TEXT("REFS")))
        {
            UObject* Class;
            UObject* Object;
            if (ParseObject(Str, TEXT("CLASS="), UClass::StaticClass(), Class, ANY_PACKAGE)
                && ParseObject(Str, TEXT("NAME="), static_cast<UClass*>(Class), Object, ANY_PACKAGE))
            {
                // the referencers and the shortest path from the root set
                NOT_DECOMPILED(0x0002a1c5);
            }
        }
        else if (ParseCommand(&Str, TEXT("HASH")))
        {
            // the xbox then walks every object and every object hash chain without printing anything
            FName::DisplayHash(Ar);
        }
        else if (ParseCommand(&Str, TEXT("CLASSES")))
        {
            ShowClasses(UObject::StaticClass(), Ar, 0);
        }
        else if (ParseCommand(&Str, TEXT("DEPENDENCIES")))
        {
            UObject* Pkg;
            if (ParseObject(Str, TEXT("PACKAGE="), UPackage::StaticClass(), Pkg, nullptr))
                NOT_DECOMPILED(0x0002a3c4);
        }
        else if (ParseCommand(&Str, TEXT("MEM")))
        {
            // object memory by class or by object, optionally filtered by CLASS=, PACKAGE= and INSIDE=
            NOT_DECOMPILED(0x0002a4c1);
        }
        else if (ParseCommand(&Str, TEXT("VFHASH")))
        {
            Ar.Logf(TEXT("Class VfHashes:"));
            for (INT i = 0; i < GObjObjects.Num(); i++)
            {
                UClass* Class = Cast<UClass>(GObjObjects[i]);
                if (!Class)
                    continue;
                Ar.Logf(TEXT("%s:"), Class->GetName());
                for (INT j = 0; j < FUNC_HASH_BUCKETS; j++)
                {
                    INT c = 0;
                    for (UStruct* Hash = Class->FuncHash[j]; Hash; Hash = Hash->HashNext)
                        c++;
                    Ar.Logf(TEXT("   %i: %i"), j, c);
                }
            }
        }
        else if (ParseCommand(&Str, TEXT("LINKERS")))
        {
            Ar.Logf(TEXT("Linkers:"));
            for (INT i = 0; i < GObjLoaders.Num(); i++)
            {
                ULinkerLoad* Linker = GObjLoaders[i];
                INT NameSize = 0;
                for (INT j = 0; j < Linker->NameMap.Num(); j++)
                    if (Linker->NameMap[j] != NAME_None)
                        NameSize += appStrlen(*Linker->NameMap[j]) + 13;
                // sizes are the xbox's: 28 bytes per import, 40 per export
                Ar.Logf(TEXT("%s (%s): Names=%i (%iK/%iK) Imports=%i (%iK) Exports=%i (%iK) Gen=%i Lazy=%i"),
                    *Linker->Filename, Linker->LinkerRoot->GetFullName(), Linker->NameMap.Num(),
                    Linker->NameMap.Num() * 4 / 1024, NameSize / 1024, Linker->ImportMap.Num(),
                    Linker->ImportMap.Num() * 28 / 1024, Linker->ExportMap.Num(), Linker->ExportMap.Num() * 40 / 1024,
                    Linker->Summary.Generations.Num(), Linker->LazyLoaders.Num());
            }
        }
        else
        {
            return 0;
        }
    }
    else if (ParseCommand(&Str, TEXT("GTIME")))
    {
        GLog->Logf(TEXT("GTime = %f"), GTime);
    }
    else if (ParseCommand(&Str, TEXT("VERSION")))
    {
        Ar.Logf(TEXT("%s, Engine version %d, compiled %s"), *GBuildLabel, ENGINE_VERSION, TEXT("Jan 25 2005"));
    }
    else
    {
        return 0;
    }
    return 1;
}
