#include "Core.h"

// 0x0009973e
UField::UField(ENativeConstructor, UClass* InClass, const TCHAR* InName, const TCHAR* InPackageName, DWORD InFlags)
    : UObject(EC_NativeConstructor, InClass, InName, InPackageName, InFlags)
    , Next(nullptr)
{
}

// 0x00099bc2
UStruct::UStruct(ENativeConstructor, INT InSize, const TCHAR* InName, const TCHAR* InPackageName, DWORD InFlags, UStruct* InSuperStruct)
    : UField(EC_NativeConstructor, UClass::StaticClass(), InName, InPackageName, InFlags)
    , HashNext(nullptr)
    , SuperField(InSuperStruct)
    , ScriptText(nullptr)
    , CppText(nullptr)
    , Children(nullptr)
    , PropertiesSize(InSize)
    , MinAlignment(1)
    , TextPos(0)
    , Line(0)
    , PropertyLink(nullptr)
{
}

// 0x00099c1b
UStruct::UStruct(UStruct* InSuperStruct)
    : HashNext(nullptr)
    , SuperField(InSuperStruct)
    , PropertiesSize(InSuperStruct ? InSuperStruct->GetPropertiesSize() : 0)
    , MinAlignment(InSuperStruct ? InSuperStruct->MinAlignment : 1)
    , FriendlyName(GetFName())
{
}

// 0x0009a5f0
UState::UState(UState* InSuperState)
    : UStruct(InSuperState)
    , FuncHash(nullptr)
{
}

// 0x0009a60f
UState::UState(ENativeConstructor, INT InSize, const TCHAR* InName, const TCHAR* InPackageName, DWORD InFlags, UState* InSuperState)
    : UStruct(EC_NativeConstructor, InSize, InName, InPackageName, InFlags, InSuperState)
    , ProbeMask(0)
    , IgnoreMask(0)
    , StateFlags(0)
    , LabelTableOffset(0)
    , FuncHash(nullptr)
{
}

// 0x0009a85b
UClass::UClass(ENativeConstructor, DWORD InSize, DWORD InClassFlags, UClass* InSuperClass, UClass* InWithinClass, FGuid InGuid,
    const TCHAR* InName, const TCHAR* InPackageName, const TCHAR* InConfigName, DWORD InFlags,
    FClassConstructor InClassConstructor, FStaticConstructor InClassStaticConstructor)
    : UState(EC_NativeConstructor, InSize, InName, InPackageName, InFlags, InSuperClass != this ? InSuperClass : nullptr)
    , ClassFlags(InClassFlags | CLASS_Parsed | CLASS_Compiled)
    , ClassUnique(0)
    , ClassGuid(InGuid)
    , UnknownAC(0)
    , ClassWithin(InWithinClass)
    , NativePropertiesSize(InSize)
    , NativeFunctionTable(nullptr)
    , ClassConstructor(InClassConstructor)
    , ClassStaticConstructor(InClassStaticConstructor)
    , DefaultsClass(this)
{
    // until Register runs, the config name is kept as a raw string
    *reinterpret_cast<const TCHAR**>(&ClassConfigName) = InConfigName;
}

// 0x0009a802
UClass::UClass()
    : UnknownAC(0)
    , ClassWithin(UObject::StaticClass())
    , DefaultsClass(this)
{
}

// 0x0009ba6f
void UClass::Register()
{
    UStruct::Register();

    ClassConfigName = FName(*reinterpret_cast<const TCHAR**>(&ClassConfigName), FNAME_Add);

    // the class default object lives in Defaults
    const INT OldNum = Defaults.Num();
    const INT NewNum = GetPropertiesSize();
    if (NewNum < OldNum)
    {
        Defaults.Remove(NewNum, OldNum - NewNum);
    }
    else
    {
        Defaults.Add(NewNum - OldNum);
        memset(Defaults.GetData() + OldNum, 0, NewNum - OldNum);
    }
    GetDefaultObject()->InitClassDefaultObject(this, 0);

    // classes that share their parent's static constructor already ran it through the parent
    if (!SuperField || GetSuperClass()->ClassStaticConstructor != ClassStaticConstructor)
        (GetDefaultObject()->*ClassStaticConstructor)();

    if (SuperField)
        ClassFlags |= GetSuperClass()->ClassFlags & CLASS_Inherit;

    FArchive ArDummy;
    Link(ArDummy, 0);

    GetDefaultObject()->LoadConfig();
    GetDefaultObject()->LoadLocalized();
}

// 0x000248c3
void UObject::InitClassDefaultObject(UClass* InClass, UBOOL SetOuter)
{
    // the defaults block borrows the class's vtable and gets a bare object header
    memset(static_cast<void*>(this), 0, sizeof(UObject));
    *reinterpret_cast<void**>(this) = *reinterpret_cast<void**>(InClass);
    Class = InClass;
    Index = INDEX_NONE;
    Outer = InClass->GetOuter();
    if (SetOuter)
        Outer = InClass->GetOuter();

    InitProperties(reinterpret_cast<BYTE*>(this), InClass->GetPropertiesSize(), InClass->GetSuperClass(), nullptr, 0,
        SetOuter ? this : nullptr, nullptr);
}

IMPLEMENT_CLASS(UObject, Core)
extern FNativeEntry GObjectNatives[];
IMPLEMENT_NATIVE_TABLE(UObject, GObjectNatives)
IMPLEMENT_CLASS(UField, Core)
IMPLEMENT_CLASS(UStruct, Core)
IMPLEMENT_CLASS(UState, Core)
IMPLEMENT_CLASS(UClass, Core)
IMPLEMENT_CLASS(UFunction, Core)
IMPLEMENT_CLASS(UEnum, Core)
IMPLEMENT_CLASS(UPackage, Core)
IMPLEMENT_CLASS(UTextBuffer, Core)
IMPLEMENT_CLASS(UConst, Core)
IMPLEMENT_CLASS(ULinker, Core)
IMPLEMENT_CLASS(ULinkerLoad, Core)
IMPLEMENT_CLASS(UPackageMap, Core)
IMPLEMENT_CLASS(UProperty, Core)
IMPLEMENT_CLASS(UByteProperty, Core)
IMPLEMENT_CLASS(UIntProperty, Core)
IMPLEMENT_CLASS(UBoolProperty, Core)
IMPLEMENT_CLASS(UFloatProperty, Core)
IMPLEMENT_CLASS(UObjectProperty, Core)
IMPLEMENT_CLASS(UClassProperty, Core)
IMPLEMENT_CLASS(UNameProperty, Core)
IMPLEMENT_CLASS(UStrProperty, Core)
IMPLEMENT_CLASS(UArrayProperty, Core)
IMPLEMENT_CLASS(UMapProperty, Core)
IMPLEMENT_CLASS(UStructProperty, Core)
IMPLEMENT_CLASS(UDelegateProperty, Core)

// 0x00024715
void UObject::InitProperties(BYTE* Data, INT DataCount, UClass* DefaultsClass, BYTE* Defaults, INT DefaultsCount,
    UObject* DestObject, UObject* SubobjectRoot)
{
    // copy the defaults past the object header, zero the rest
    INT Inited = sizeof(UObject);
    if (!Defaults && DefaultsClass && DefaultsClass->Defaults.Num())
    {
        Defaults = DefaultsClass->Defaults.GetData();
        DefaultsCount = DefaultsClass->Defaults.Num();
    }
    if (Defaults)
    {
        memcpy(Data + sizeof(UObject), Defaults + sizeof(UObject), DefaultsCount - sizeof(UObject));
        Inited = DefaultsCount;
    }
    if (Inited < DataCount)
        memset(Data + Inited, 0, DataCount - Inited);

    // subobjects take CPF_Transient values from their own class's defaults
    if (SubobjectRoot)
    {
        BYTE* ClassDefaults = DefaultsClass->DefaultsClass->Defaults.GetData();
        for (UStruct* S = DestObject->GetClass(); S; S = S->GetInheritanceSuper())
        {
            for (UProperty* P = S->PropertyLink; P; P = static_cast<UProperty*>(P->Next))
            {
                if (P->PropertyFlags & CPF_Transient)
                    memcpy(Data + P->Offset, ClassDefaults + P->Offset, P->GetElementSize() * P->ArrayDim);
            }
        }
    }

    // strings, arrays and other constructed values need real copies, not the bytes copied above
    if (DefaultsClass)
    {
        for (UStruct* S = DefaultsClass; S; S = S->GetInheritanceSuper())
        {
            for (UProperty* P = S->PropertyLink; P; P = static_cast<UProperty*>(P->Next))
            {
                if ((P->PropertyFlags & CPF_NeedCtorLink) && P->Offset < DefaultsCount)
                {
                    memset(Data + P->Offset, 0, P->GetSize());
                    P->CopyCompleteValue(Data + P->Offset, Defaults + P->Offset, SubobjectRoot ? SubobjectRoot : DestObject);
                }
            }
        }
    }
}

// ---------------------------------------------------------------------------------------------- native layouts (port)

namespace
{
    const FNativeLayout* GNativeLayouts = nullptr;
}

FNativeLayout::FNativeLayout(UClass* (*InStaticClass)(), INT InSize, INT InAlignment, const FNativePropertyOffset* InProperties, INT InCount)
    : StaticClass(InStaticClass)
    , OuterName(nullptr)
    , StructName(nullptr)
    , Size(InSize)
    , Alignment(InAlignment)
    , Properties(InProperties)
    , Count(InCount)
    , Next(GNativeLayouts)
{
    GNativeLayouts = this;
}

FNativeLayout::FNativeLayout(const TCHAR* InOuterName, const TCHAR* InStructName, INT InSize, INT InAlignment,
    const FNativePropertyOffset* InProperties, INT InCount)
    : StaticClass(nullptr)
    , OuterName(InOuterName)
    , StructName(InStructName)
    , Size(InSize)
    , Alignment(InAlignment)
    , Properties(InProperties)
    , Count(InCount)
    , Next(GNativeLayouts)
{
    GNativeLayouts = this;
}

const FNativeLayout* FNativeLayout::Find(const UStruct* Struct)
{
    const UBOOL IsClass = Struct->IsA(UClass::StaticClass());
    // functions and states are structs too; only plain script structs match by name
    const UBOOL IsScriptStruct = Struct->GetClass() == UStruct::StaticClass();
    for (const FNativeLayout* Layout = GNativeLayouts; Layout; Layout = Layout->Next)
    {
        if (Layout->StaticClass)
        {
            if (IsClass && Layout->StaticClass() == Struct)
                return Layout;
        }
        else if (IsScriptStruct && Struct->GetOuter() && !appStricmp(Struct->GetName(), Layout->StructName)
            && !appStricmp(Struct->GetOuter()->GetName(), Layout->OuterName))
        {
            return Layout;
        }
    }
    return nullptr;
}

const FNativeLayout* FNativeLayout::First()
{
    return GNativeLayouts;
}

INT FNativeLayout::FindOffset(const TCHAR* PropertyName) const
{
    for (INT i = 0; i < Count; i++)
    {
        if (!appStrcmp(Properties[i].Name, PropertyName))
            return Properties[i].Offset;
    }
    return INDEX_NONE;
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"

// the script properties of Core's native classes. ObjectInternal and ExecVtbl stand for C++ data script never
// touches (the object header, the FExec vtable); they are CPF_Native, so nothing copies or serializes them.
struct FCoreNativeLayouts
{
    static const FNativePropertyOffset ObjectProperties[];
    static const FNativePropertyOffset SubsystemProperties[];
    static const FNativePropertyOffset CommandletProperties[];
    static const FNativePropertyOffset HelloWorldCommandletProperties[];
    static const FNativeLayout Object;
    static const FNativeLayout Subsystem;
    static const FNativeLayout Commandlet;
    static const FNativeLayout HelloWorldCommandlet;
};

const FNativePropertyOffset FCoreNativeLayouts::ObjectProperties[] =
{
    { TEXT("ObjectInternal"), 0 },
    { TEXT("Outer"), static_cast<INT>(offsetof(UObject, Outer)) },
    { TEXT("ObjectFlags"), static_cast<INT>(offsetof(UObject, ObjectFlags)) },
    { TEXT("Name"), static_cast<INT>(offsetof(UObject, Name)) },
    { TEXT("Class"), static_cast<INT>(offsetof(UObject, Class)) },
};

const FNativePropertyOffset FCoreNativeLayouts::SubsystemProperties[] =
{
    // where the FExec base, and with it its vtable pointer, sits in a USubsystem
    { TEXT("ExecVtbl"), static_cast<INT>(reinterpret_cast<BYTE*>(static_cast<FExec*>(reinterpret_cast<USubsystem*>(0x1000))) - reinterpret_cast<BYTE*>(0x1000)) },
};

const FNativePropertyOffset FCoreNativeLayouts::CommandletProperties[] =
{
    { TEXT("HelpCmd"), static_cast<INT>(offsetof(UCommandlet, HelpCmd)) },
    { TEXT("HelpOneLiner"), static_cast<INT>(offsetof(UCommandlet, HelpOneLiner)) },
    { TEXT("HelpUsage"), static_cast<INT>(offsetof(UCommandlet, HelpUsage)) },
    { TEXT("HelpWebLink"), static_cast<INT>(offsetof(UCommandlet, HelpWebLink)) },
    { TEXT("HelpParm"), static_cast<INT>(offsetof(UCommandlet, HelpParm)) },
    { TEXT("HelpDesc"), static_cast<INT>(offsetof(UCommandlet, HelpDesc)) },
    // the bools share the bitfield word after HelpDesc
    { TEXT("LogToStdout"), static_cast<INT>(offsetof(UCommandlet, HelpDesc) + sizeof(UCommandlet::HelpDesc)) },
    { TEXT("IsServer"), static_cast<INT>(offsetof(UCommandlet, HelpDesc) + sizeof(UCommandlet::HelpDesc)) },
    { TEXT("IsClient"), static_cast<INT>(offsetof(UCommandlet, HelpDesc) + sizeof(UCommandlet::HelpDesc)) },
    { TEXT("IsEditor"), static_cast<INT>(offsetof(UCommandlet, HelpDesc) + sizeof(UCommandlet::HelpDesc)) },
    { TEXT("LazyLoad"), static_cast<INT>(offsetof(UCommandlet, HelpDesc) + sizeof(UCommandlet::HelpDesc)) },
    { TEXT("ShowErrorCount"), static_cast<INT>(offsetof(UCommandlet, HelpDesc) + sizeof(UCommandlet::HelpDesc)) },
    { TEXT("ShowBanner"), static_cast<INT>(offsetof(UCommandlet, HelpDesc) + sizeof(UCommandlet::HelpDesc)) },
};

const FNativePropertyOffset FCoreNativeLayouts::HelloWorldCommandletProperties[] =
{
    { TEXT("intparm"), static_cast<INT>(offsetof(UHelloWorldCommandlet, intparm)) },
    { TEXT("strparm"), static_cast<INT>(offsetof(UHelloWorldCommandlet, strparm)) },
};

const FNativeLayout FCoreNativeLayouts::Object(UObject::StaticClass, sizeof(UObject), alignof(UObject),
    ObjectProperties, ARRAY_COUNT(ObjectProperties));
const FNativeLayout FCoreNativeLayouts::Subsystem(USubsystem::StaticClass, sizeof(USubsystem), alignof(USubsystem),
    SubsystemProperties, ARRAY_COUNT(SubsystemProperties));
const FNativeLayout FCoreNativeLayouts::Commandlet(UCommandlet::StaticClass, sizeof(UCommandlet), alignof(UCommandlet),
    CommandletProperties, ARRAY_COUNT(CommandletProperties));
const FNativeLayout FCoreNativeLayouts::HelloWorldCommandlet(UHelloWorldCommandlet::StaticClass, sizeof(UHelloWorldCommandlet),
    alignof(UHelloWorldCommandlet), HelloWorldCommandletProperties, ARRAY_COUNT(HelloWorldCommandletProperties));

#pragma clang diagnostic pop

// 0x0009c04c
void UStruct::Link(FArchive& Ar, UBOOL Props)
{
    if (!Props)
    {
        // relink only: let each property recompute its flags but keep its place
        UProperty* Prev = nullptr;
        for (UField* Field = Children; Field; Field = Field->Next)
        {
            if (!Field->IsA(UProperty::StaticClass()))
                continue;
            UProperty* P = static_cast<UProperty*>(Field);
            UBoolProperty* Bool = P->IsA(UBoolProperty::StaticClass()) ? static_cast<UBoolProperty*>(P) : nullptr;
            const _WORD SavedOffset = P->Offset;
            const BYTE SavedBit = Bool ? Bool->BitIndex : 0;
            P->Link(Ar, Prev);
            P->Offset = SavedOffset;
            if (Bool)
                Bool->BitIndex = SavedBit;
            Prev = P;
        }
    }
    else
    {
        PropertiesSize = 0;
        MinAlignment = 1;
        if (UStruct* Inherited = GetInheritanceSuper())
        {
            Ar.Preload(Inherited);
            MinAlignment = Inherited->MinAlignment;
            PropertiesSize = Inherited->AlignedSize;
        }

        // port: native classes put their script properties at the C++ member offsets
        const FNativeLayout* Native = FNativeLayout::Find(this);

        UProperty* Prev = nullptr;
        for (UField* Field = Children; Field; Field = Field->Next)
        {
            Ar.Preload(Field);
            if (Field->GetOuter() != this)
                break;
            if (!Field->IsA(UProperty::StaticClass()))
                continue;
            UProperty* P = static_cast<UProperty*>(Field);
            P->Link(Ar, Prev);
            if (Native)
            {
                const INT NativeOffset = Native->FindOffset(P->GetName());
                if (NativeOffset == INDEX_NONE)
                    appErrorf(TEXT("%s.%s has no native offset"), GetName(), P->GetName());
                P->Offset = static_cast<_WORD>(NativeOffset);
            }
            PropertiesSize = P->Offset + P->GetSize();
            if (MinAlignment < P->GetLinkAlign())
                MinAlignment = P->GetLinkAlign();
            Prev = P;
        }
        if (Native)
        {
            PropertiesSize = Native->Size;
            if (MinAlignment < Native->Alignment)
                MinAlignment = Native->Alignment;
        }

        AlignedSize = (PropertiesSize + 3) & ~3;
        PropertiesSize = (PropertiesSize + MinAlignment - 1) & ~(MinAlignment - 1);
        if (PropertiesSize % 4 == 0)
            MinAlignment = MinAlignment < 4 ? 4 : MinAlignment;
        else if (PropertiesSize % 2 == 0)
            MinAlignment = MinAlignment < 2 ? 2 : MinAlignment;

        // math structs keep their native sizes
        if (GetFName() == FName(TEXT("Vector"), FNAME_Add))
        {
            PropertiesSize = 12;
            MinAlignment = 4;
        }
        else if (GetFName() == FName(TEXT("Plane"), FNAME_Add) || GetFName() == FName(TEXT("Quat"), FNAME_Add))
        {
            PropertiesSize = 16;
            MinAlignment = 4;
        }
    }

    // move the fields that follow the properties to the front, so the Next chain from PropertyLink holds only properties
    PropertyLink = nullptr;
    UField* MovedStart = nullptr;
    UField* Last = nullptr;
    UField* PrevProperty = nullptr;
    for (UField* Field = Children; Field; Field = Field->Next)
    {
        if (Field->IsA(UProperty::StaticClass()))
        {
            if (!PropertyLink)
                PropertyLink = static_cast<UProperty*>(Field);
            PrevProperty = Field;
        }
        else
        {
            Last = Field;
            if (PrevProperty && PrevProperty->Next == Field)
            {
                PrevProperty->Next = nullptr;
                MovedStart = Field;
            }
        }
    }
    if (MovedStart)
    {
        Last->Next = Children;
        Children = MovedStart;
    }

    // replicated properties with identical replication conditions share one RepOwner
    struct FRepEnd { UProperty* Property; INT End; };
    TArray<FRepEnd> RepEnds;
    auto FindEnd = [&RepEnds](UProperty* P) -> INT&
    {
        for (INT i = 0; i < RepEnds.Num(); i++)
        {
            if (RepEnds[i].Property == P)
                return RepEnds[i].End;
        }
        return RepEnds[RepEnds.AddItem({ P, 0 })].End;
    };

    for (UStruct* S = this; S; S = S->GetInheritanceSuper())
    {
        for (UProperty* P = S->PropertyLink; P; P = static_cast<UProperty*>(P->Next))
        {
            if (!(P->PropertyFlags & CPF_Net))
                continue;

            P->RepOwner = P;
            FArchive TempAr;
            INT iCode = P->RepOffset;
            P->GetOwnerClass()->SerializeExpr(iCode, TempAr);
            FindEnd(P) = iCode;
            const INT Length = iCode - P->RepOffset;

            for (UStruct* S2 = this; S2; S2 = S2->GetInheritanceSuper())
            {
                for (UProperty* Q = S2->PropertyLink; Q && Q != P; Q = static_cast<UProperty*>(Q->Next))
                {
                    if (!(Q->PropertyFlags & CPF_Net))
                        continue;
                    if (FindEnd(Q) - Q->RepOffset == Length
                        && !memcmp(&P->GetOwnerClass()->Script[P->RepOffset], &Q->GetOwnerClass()->Script[Q->RepOffset], Length))
                    {
                        Q->RepOwner = P->RepOwner;
                    }
                }
                if (S2 == S)
                    break;
            }
        }
    }
}

// 0x00027b09
void UObject::LoadConfig(UBOOL Propagate, UClass* InClass, const TCHAR* InFilename)
{
    if (!InClass)
        InClass = GetClass();
    if (!(InClass->ClassFlags & CLASS_Config))
        return;

    if (Propagate && InClass->SuperField)
        LoadConfig(Propagate, InClass->GetSuperClass(), InFilename);

    const UBOOL PerObject = (GetClass()->ClassFlags & CLASS_PerObjectConfig) && GetIndex() != INDEX_NONE;
    const TCHAR* Filename = InFilename;
    if (!Filename)
        Filename = PerObject && GetOuter() != GObjTransientPkg ? GetOuter()->GetName() : *GetClass()->ClassConfigName;

    for (UStruct* S = InClass; S; S = S->GetInheritanceSuper())
    {
        for (UProperty* P = S->PropertyLink; P; P = static_cast<UProperty*>(P->Next))
        {
            if (!(P->PropertyFlags & CPF_Config))
                continue;

            TCHAR Value[1024] = {};
            UObject* SectionOwner = (P->PropertyFlags & CPF_GlobalConfig) ? P->GetOwnerClass() : InClass;
            const TCHAR* Section = PerObject ? GetName() : SectionOwner->GetPathName();

            TCHAR TempKey[256];
            const TCHAR* Key = P->ArrayDim == 1 ? P->GetName() : TempKey;

            if (P->IsA(UArrayProperty::StaticClass()))
            {
                UArrayProperty* Array = static_cast<UArrayProperty*>(P);
                FConfigSection* Sec = GConfig->GetSectionPrivate(Section, 0, 1, Filename);
                if (!Sec)
                    continue;

                TArray<FConfigValue> List;
                Sec->MultiFind(FName(Key, FNAME_Add), List);
                if (!List.Num())
                    continue;

                FArray& Data = *reinterpret_cast<FArray*>(reinterpret_cast<BYTE*>(this) + P->Offset);
                const INT Size = Array->Inner->GetElementSize();
                Array->DestroyValue(&Data);

                // values come back newest first, so walk them backwards. an empty entry clears what came before.
                INT Index = 0;
                for (INT i = List.Num() - 1; i >= 0; i--, Index++)
                {
                    Data.AddZeroed(1, Size);
                    if (!**List[i])
                    {
                        Data.Empty(Size, 0);
                        Index = -1;
                    }
                    else
                    {
                        Array->Inner->ImportText(*List[i], static_cast<BYTE*>(Data.GetData()) + Index * Size, 0);
                    }
                }
            }
            else if (P->IsA(UMapProperty::StaticClass()))
            {
                // maps were never finished: the values are looked up and thrown away
                if (FConfigSection* Sec = GConfig->GetSectionPrivate(Section, 0, 1, Filename))
                {
                    TArray<FConfigValue> List;
                    Sec->MultiFind(FName(Key, FNAME_Add), List);
                }
            }
            else
            {
                for (INT i = 0; i < P->ArrayDim; i++)
                {
                    if (P->ArrayDim != 1)
                        appSprintf(TempKey, TEXT("%s[%i]"), P->GetName(), i);
                    if (!GConfig->GetString(Section, Key, Value, ARRAY_COUNT(Value), Filename))
                        continue;

                    P->ImportText(Value, reinterpret_cast<BYTE*>(this) + P->Offset + i * P->GetElementSize(), 0);
                    if ((P->PropertyFlags & CPF_LoadObjectRef) && *Value)
                        StaticLoadObject(UObject::StaticClass(), nullptr, Value, nullptr, LOAD_Unknown2002, nullptr);
                }
            }
        }
    }
}

// 0x00028c2e. writes every property with all of Flags to the ini, then reloads the defaults of every class sharing the config.
void UObject::SaveConfig(DWORD Flags, const TCHAR* InFilename, const TCHAR* ForcedSection)
{
    const UBOOL PerObject = (GetClass()->ClassFlags & CLASS_PerObjectConfig) && GetIndex() != INDEX_NONE;
    const TCHAR* Filename = InFilename;
    if (!Filename)
        Filename = PerObject && GetOuter() != GObjTransientPkg ? GetOuter()->GetName() : *GetClass()->ClassConfigName;

    for (TFieldIterator<UProperty> It(GetClass()); It; ++It)
    {
        UProperty* P = *It;
        if ((P->PropertyFlags & Flags) != Flags)
            continue;

        TCHAR Value[1024] = {};
        const TCHAR* Key = P->GetName();
        const TCHAR* Section = PerObject ? GetName() : ((P->PropertyFlags & CPF_GlobalConfig) ? P->GetOwnerClass() : GetClass())->GetPathName();

        if (UArrayProperty* Array = Cast<UArrayProperty>(P))
        {
            FConfigSection* Sec = GConfig->GetSectionPrivate(Section, 1, 0, Filename);
            Sec->Remove(FName(Key, FNAME_Add));
            FArray* Data = reinterpret_cast<FArray*>(reinterpret_cast<BYTE*>(this) + P->Offset);
            const INT Size = Array->Inner->GetElementSize();
            for (INT i = 0; i < Data->Num(); i++)
            {
                TCHAR Buffer[1024] = {};
                BYTE* Elem = static_cast<BYTE*>(Data->GetData()) + i * Size;
                Array->Inner->ExportTextItem(Buffer, Elem, Elem, 0);
                Sec->Add(FName(Key, FNAME_Add), FConfigValue(Buffer));
            }
        }
        else if (Cast<UMapProperty>(P))
        {
            // maps were never finished: the key is cleared and nothing written
            GConfig->GetSectionPrivate(Section, 1, 0, Filename)->Remove(FName(Key, FNAME_Add));
        }
        else
        {
            TCHAR TempKey[256];
            for (INT i = 0; i < P->ArrayDim; i++)
            {
                if (P->ArrayDim != 1)
                {
                    appSprintf(TempKey, TEXT("%s[%i]"), P->GetName(), i);
                    Key = TempKey;
                }
                P->ExportText(i, Value, reinterpret_cast<BYTE*>(this), reinterpret_cast<BYTE*>(this), 0);
                GConfig->SetString(Section, Key, Value, Filename);
            }
        }
    }

    GConfig->Flush(0, nullptr, ForcedSection);

    UClass* BaseClass = GetClass();
    while (BaseClass->GetSuperClass() && (BaseClass->GetSuperClass()->ClassFlags & CLASS_Config))
        BaseClass = BaseClass->GetSuperClass();
    for (INT i = 0; i < GObjObjects.Num(); i++)
    {
        UClass* Cls = Cast<UClass>(GObjObjects[i]);
        if (Cls && Cls->IsChildOf(BaseClass) && !Cls->IsDefaultsClass())
            Cls->GetDefaultObject()->LoadConfig(1);
    }
}

// 0x00027f27. rewrites the class's section from "System.ini" or "User.ini" into the config file, then reloads.
// both names resolve to the same cached file, so on this build the values are written back unchanged.
void UClass::ResetConfig()
{
    const TCHAR* DefaultFile;
    if (ClassConfigName == NAME_System)
        DefaultFile = TEXT("System.ini");
    else if (ClassConfigName == NAME_User)
        DefaultFile = TEXT("User.ini");
    else
        return;

    TCHAR* Buffer = new TCHAR[32000];
    if (GConfig->GetSection(GetPathName(), Buffer, 32000, DefaultFile))
    {
        TCHAR* Line = Buffer;
        while (*Line)
        {
            TCHAR* Next = Line + appStrlen(Line) + 1;
            if (TCHAR* Equals = const_cast<TCHAR*>(appStrstr(Line, TEXT("="))))
            {
                *Equals = 0;
                GConfig->SetString(GetPathName(), Line, Equals + 1, *ClassConfigName);
            }
            Line = Next;
        }
    }
    delete[] Buffer;

    for (INT i = 0; i < GObjObjects.Num(); i++)
    {
        UClass* Cls = Cast<UClass>(GObjObjects[i]);
        if (Cls && Cls->IsChildOf(this))
            Cls->GetDefaultObject()->LoadConfig(1);
    }
    for (INT i = 0; i < GObjObjects.Num(); i++)
    {
        UObject* Object = GObjObjects[i];
        if (Object && Object->IsA(this))
        {
            Object->LoadConfig(1);
            Object->PostEditChange();
        }
    }
}

// 0x000263cf
void UObject::LoadLocalized()
{
    if (!(GetClass()->ClassFlags & CLASS_Localized))
        return;

    if (GetIndex() != INDEX_NONE && (GetFlags() & RF_PerObjectLocalized) && GetOuter()->GetOuter())
        LoadLocalizedStruct(GetClass(), GetOuter()->GetName(), GetOuter()->GetOuter()->GetName(), GetName(), reinterpret_cast<BYTE*>(this));
    else
        LoadLocalizedStruct(GetClass(), GetClass()->GetOuter()->GetName(),
            GetIndex() != INDEX_NONE ? GetName() : GetClass()->GetName(), nullptr, reinterpret_cast<BYTE*>(this));
}

// 0x0002621e
void UObject::LoadLocalizedStruct(UStruct* Struct, const TCHAR* IntName, const TCHAR* SectionName, const TCHAR* KeyPrefix, BYTE* Data)
{
    for (UStruct* S = Struct; S; S = S->GetInheritanceSuper())
    {
        for (UProperty* P = S->PropertyLink; P; P = static_cast<UProperty*>(P->Next))
        {
            for (INT i = 0; i < P->ArrayDim; i++)
            {
                // keys look like Prefix.Name or Prefix.Name[Index]
                TCHAR Key[1024] = {};
                if (KeyPrefix)
                    appSprintf(Key, TEXT("%s."), KeyPrefix);
                if (P->ArrayDim < 2)
                    strcat(Key, P->GetName());
                else
                    appSprintf(Key + appStrlen(Key), TEXT("%s[%d]"), P->GetName(), i);

                BYTE* Value = Data + P->Offset + i * P->GetElementSize();
                if (P->IsA(UStructProperty::StaticClass()))
                {
                    LoadLocalizedStruct(static_cast<UStructProperty*>(P)->Struct, IntName, SectionName, Key, Value);
                }
                else if (P->PropertyFlags & CPF_Localized)
                {
                    const TCHAR* Text = Localize(SectionName, Key, IntName, nullptr, 1);
                    if (*Text)
                        P->ImportText(Text, Value, 0);
                }
            }
        }
    }
}

// 0x00099cdf
void UConst::Serialize(FArchive& Ar)
{
    UObject::Serialize(Ar);

    // older packages stored a SuperField here
    if (Ar.Ver() < 137)
    {
        UObject* Dummy = nullptr;
        Ar << Dummy;
    }
    Ar << Value;
}

// 0x0009a40d
void UStruct::Serialize(FArchive& Ar)
{
    UField::Serialize(Ar);

    if (Ar.IsLoading())
        HashNext = nullptr;
    Ar << reinterpret_cast<UObject*&>(SuperField) << ScriptText;
    if (SuperField)
        Ar.Preload(SuperField);

    // the children and each one's Next link are stored here, not by the children themselves
    Ar << reinterpret_cast<UObject*&>(Children);
    for (UField* Field = Children; Field; Field = Field->Next)
        Ar << reinterpret_cast<UObject*&>(Field->Next);

    Ar << FriendlyName;
    if (Ar.Ver() >= 120)
        Ar << CppText;
    Ar << Line << TextPos;
    if (Ar.Ver() >= 130)
        Ar << StructFlags;

    INT ScriptSize = Script.Num();
    Ar << ScriptSize;
    if (Ar.IsLoading())
    {
        Script.Empty();
        Script.AddZeroed(ScriptSize);
    }
    INT iCode = 0;
    while (iCode < ScriptSize)
        SerializeExpr(iCode, Ar);
    if (iCode != ScriptSize)
        appErrorf(TEXT("Script serialization mismatch: Got %i, expected %i"), iCode, ScriptSize);
    Script.CountBytes(Ar);

    FMD5Context Context;
    appMD5Init(&Context);
    appMD5Update(&Context, Script.GetData(), Script.Num());
    appMD5Final(ScriptMD5, &Context);

    Ar.ThisContainsCode();
    if (Ar.IsLoading())
        Link(Ar, 1);
}

// 0x0009a9d1. object and name references are kept as 4 byte indices, see ReadScriptObject.
BYTE UStruct::SerializeExpr(INT& iCode, FArchive& Ar)
{
    auto Xfer = [&](INT Size)
    {
        Ar.Serialize(&Script[iCode], Size);
        iCode += Size;
    };
    auto XferObject = [&]()
    {
        UObject* Object = ReadScriptObject(&Script[iCode]);
        Ar << Object;
        WriteScriptObject(&Script[iCode], Object);
        iCode += 4;
    };
    auto XferName = [&]()
    {
        FName Name = ReadScriptName(&Script[iCode]);
        Ar << Name;
        WriteScriptName(&Script[iCode], Name);
        iCode += 4;
    };
    auto XferExpr = [&]() { return SerializeExpr(iCode, Ar); };
    // a call's parameters, then the debug info that may follow it
    auto XferFunctionParms = [&]()
    {
        while (XferExpr() != EX_EndFunctionParms)
            ;
        if (iCode >= Script.Num())
            return;
        const INT SavedPos = Ar.Tell();
        const INT SavedCode = iCode;
        Xfer(1);
        INT Version = -1;
        if (Script[iCode - 1] == EX_DebugInfo)
        {
            Xfer(4);
            memcpy(&Version, &Script[iCode - 4], sizeof(Version));
        }
        iCode = SavedCode;
        Ar.Seek(SavedPos);
        if (Version == DEBUG_INFO_VERSION)
            XferExpr();
    };

    Xfer(1);
    const BYTE Token = Script[iCode - 1];
    if (Token >= EX_FirstNative)
    {
        XferFunctionParms();
    }
    else if (Token >= EX_ExtendedNative)
    {
        Xfer(1);
        XferFunctionParms();
    }
    else switch (Token)
    {
        case EX_LocalVariable:
        case EX_InstanceVariable:
        case EX_DefaultVariable:
        case EX_ObjectConst:
        case EX_NativeParm:
            XferObject();
            break;
        case EX_Return:
        case EX_GotoLabel:
        case EX_EatString:
        case EX_DynArrayLength:
            XferExpr();
            break;
        case EX_Switch:
        case EX_PrimitiveCast:
            Xfer(1);
            XferExpr();
            break;
        case EX_Jump:
            Xfer(2);
            break;
        case EX_JumpIfNot:
        case EX_Assert:
        case EX_Skip:
            Xfer(2);
            XferExpr();
            break;
        case EX_Case:
        {
            Xfer(2);
            _WORD NextOffset;
            memcpy(&NextOffset, &Script[iCode - 2], sizeof(NextOffset));
            if (NextOffset != 0xFFFF)
                XferExpr();
            break;
        }
        case EX_LabelTable:
            // name and code offset pairs, ending with NAME_None
            for (;;)
            {
                const INT Start = iCode;
                XferName();
                Xfer(4);
                if (ReadScriptName(&Script[Start]) == NAME_None)
                    break;
            }
            break;
        case EX_Let:
        case EX_DynArrayElement:
        case EX_LetBool:
        case EX_ArrayElement:
        case EX_LetDelegate:
            XferExpr();
            XferExpr();
            break;
        case EX_New:
            XferExpr();
            XferExpr();
            XferExpr();
            XferExpr();
            break;
        case EX_ClassContext:
        case EX_Context:
            XferExpr();
            Xfer(2);
            Xfer(1);
            XferExpr();
            break;
        case EX_MetaCast:
        case EX_DynamicCast:
        case EX_StructMember:
            XferObject();
            XferExpr();
            break;
        case EX_VirtualFunction:
        case EX_GlobalFunction:
            XferName();
            XferFunctionParms();
            break;
        case EX_FinalFunction:
            XferObject();
            XferFunctionParms();
            break;
        case EX_IntConst:
        case EX_FloatConst:
            Xfer(4);
            break;
        case EX_StringConst:
            do
                Xfer(1);
            while (Script[iCode - 1]);
            break;
        case EX_UnicodeStringConst:
            do
                Xfer(2);
            while (Script[iCode - 1]);
            break;
        case EX_NameConst:
        case EX_DelegateProperty:
            XferName();
            break;
        case EX_RotationConst:
        case EX_VectorConst:
            Xfer(4);
            Xfer(4);
            Xfer(4);
            break;
        case EX_ByteConst:
        case EX_IntConstByte:
            Xfer(1);
            break;
        case EX_Iterator:
            XferExpr();
            Xfer(2);
            break;
        case EX_StructCmpEq:
        case EX_StructCmpNe:
            XferObject();
            XferExpr();
            XferExpr();
            break;
        case EX_DynArrayInsert:
        case EX_DynArrayRemove:
            XferExpr();
            XferExpr();
            XferExpr();
            break;
        case EX_DebugInfo:
            Xfer(4);
            Xfer(4);
            Xfer(4);
            do
                Xfer(1);
            while (Script[iCode - 1]);
            break;
        case EX_DelegateFunction:
            XferObject();
            XferName();
            break;
        case EX_Stop:
        case EX_Nothing:
        case EX_EndFunctionParms:
        case EX_Self:
        case EX_IntZero:
        case EX_IntOne:
        case EX_True:
        case EX_False:
        case EX_NoObject:
        case EX_BoolVariable:
        case EX_IteratorPop:
        case EX_IteratorNext:
            break;
        default:
            appErrorf(TEXT("Bad expr token %02x"), Token);
            break;
    }
    return Token;
}

// 0x0009a225
void UStruct::SerializeBin(FArchive& Ar, BYTE* Data, INT MaxReadBytes)
{
    const INT EndPos = Ar.Tell() + MaxReadBytes;
    // old DecorationLayer structs lack two properties
    const UBOOL DecoHack = Ar.Ver() < 115 && !appStrcmp(GetName(), TEXT("DecorationLayer"));

    for (TFieldIterator<UProperty> It(this); It; ++It)
    {
        UProperty* Property = *It;
        if (DecoHack && (!appStrcmp(Property->GetName(), TEXT("LitDirectional")) || !appStrcmp(Property->GetName(), TEXT("DisregardTerrainLighting"))))
        {
            GLog->Logf(NAME_Warning, TEXT("DecoHack: skipping property %s"), Property->GetName());
            continue;
        }
        if (!Property->ShouldSerializeValue(Ar))
            continue;
        for (INT Index = 0; Index < Property->ArrayDim; Index++)
        {
            if (Ar.Ver() < 118 && MaxReadBytes && Ar.IsLoading() && Ar.Tell() >= EndPos)
                GLog->Logf(NAME_Warning, TEXT("Binary-serialized struct property %s of %s does not exist on disk.  Skipping."), Property->GetName(), GetName());
            else
                Property->SerializeItem(Ar, Data + Property->Offset + Index * Property->GetElementSize(), 0, 0);
        }
    }
}

// 0x0009b2e3
void UStruct::SerializeTaggedProperties(FArchive& Ar, BYTE* Data, UClass* DefaultsClass, UBOOL Unknown)
{
    BYTE* Defaults = nullptr;
    INT DefaultsCount = 0;
    if (DefaultsClass)
    {
        Defaults = DefaultsClass->Defaults.GetData();
        DefaultsCount = DefaultsClass->Defaults.Num();
    }

    if (Ar.IsLoading())
    {
        for (;;)
        {
            FPropertyTag Tag;
            Ar << Tag;
            if (Tag.Name == NAME_None)
                break;

            UProperty* Property = nullptr;
            for (UStruct* Struct = this; Struct && !Property; Struct = Struct->GetInheritanceSuper())
            {
                for (UProperty* P = Struct->PropertyLink; P; P = static_cast<UProperty*>(P->Next))
                {
                    if (P->GetFName() == Tag.Name)
                    {
                        Property = P;
                        break;
                    }
                }
            }

            if (!Property)
            {
                // "HSPass" marks a second pass over the same data
                if (Tag.Name == FName(TEXT("HSPass"), FNAME_Add))
                {
                    if (Tag.ArrayIndex == 0)
                    {
                        SerializeTaggedProperties(Ar, Data, DefaultsClass, Unknown);
                        continue;
                    }
                }
                else
                {
                    GLogDetail->Logf(NAME_Warning, TEXT("Property %s of %s not found"), *Tag.Name, GetFullName());
                }
            }
            else
            {
                UArrayProperty* ArrayProperty = Property->IsA(UArrayProperty::StaticClass()) ? static_cast<UArrayProperty*>(Property) : nullptr;
                if (ArrayProperty && Tag.Type != NAME_ArrayProperty && ArrayProperty->Inner->GetID() == Tag.Type)
                {
                    GLog->Logf(NAME_Warning, TEXT("Converting %s[%d] of %s to dynamic array."), *Tag.Name, Tag.ArrayIndex, GetName());
                    UProperty* Inner = ArrayProperty->Inner;
                    FArray* Array = reinterpret_cast<FArray*>(Data + Property->Offset);
                    if (Array->Num() <= Tag.ArrayIndex)
                        Array->AddZeroed(Tag.ArrayIndex - Array->Num() + 1, Inner->GetElementSize());
                    Tag.SerializeTaggedProperty(Ar, Inner, static_cast<BYTE*>(Array->GetData()) + Tag.ArrayIndex * Inner->GetElementSize(), 0, Unknown);
                    continue;
                }

                if (Tag.Type == NAME_ByteProperty && Property->GetID() == NAME_FloatProperty)
                {
                    BYTE Value;
                    Ar << Value;
                    *reinterpret_cast<FLOAT*>(Data + Property->Offset + Tag.ArrayIndex * Property->GetElementSize()) = Value;
                    continue;
                }

                if (Tag.Type != Property->GetID())
                {
                    GLog->Logf(NAME_Warning, TEXT("Type mismatch in %s of %s: file %i, class %i"), *Tag.Name, GetName(), Tag.Type, Property->GetID());
                }
                else if (Tag.ArrayIndex >= Property->ArrayDim)
                {
                    GLog->Logf(NAME_Warning, TEXT("Array bounds in %s of %s: %i/%i"), *Tag.Name, GetName(), Tag.ArrayIndex, Property->ArrayDim);
                }
                else if (Tag.Type == NAME_StructProperty && Tag.ItemName != CastChecked<UStructProperty>(Property)->Struct->GetFName())
                {
                    GLog->Logf(NAME_Warning, TEXT("Property %s of %s struct type mismatch %s/%s"), *Tag.Name, GetName(), *Tag.ItemName,
                        CastChecked<UStructProperty>(Property)->Struct->GetName());
                }
                else if (!Property->ShouldSerializeValue(Ar))
                {
                    if (appStricmp(*Tag.Name, TEXT("XLevel")))
                        GLog->Logf(NAME_Warning, TEXT("Property %s of %s is not serializable"), *Tag.Name, GetName());
                }
                else if (Unknown && (Property->PropertyFlags & CPF_Static))
                {
                    // the stored value is only compared with the default, which wins
                    static TArray<BYTE> Temp;
                    const INT TempSize = Tag.Size < 4 ? 4 : Tag.Size;
                    Temp.Empty();
                    Temp.AddZeroed(TempSize);
                    Tag.SerializeTaggedProperty(Ar, Property, Temp.GetData(), Tag.Size, Unknown);
                    BYTE* Legacy = Temp.GetData() - (Property->Offset + Tag.ArrayIndex * Property->GetElementSize());
                    if (!Property->Matches(Data, Legacy, Tag.ArrayIndex))
                    {
                        TCHAR LegacyText[256];
                        TCHAR DefaultText[256];
                        Property->ExportText(Tag.ArrayIndex, LegacyText, Legacy, Legacy, PPF_Delimited);
                        Property->ExportText(Tag.ArrayIndex, DefaultText, Data, Data, PPF_Delimited);
                        GLog->Logf(NAME_Warning, TEXT("Static property %s: instance has legacy value %s, does not match default %s"), *Tag.Name, LegacyText, DefaultText);
                    }
                    continue;
                }
                else
                {
                    const INT StartPos = Ar.Tell();
                    Tag.SerializeTaggedProperty(Ar, Property, Data + Property->Offset + Tag.ArrayIndex * Property->GetElementSize(), Tag.Size, Unknown);
                    if (Ar.Ver() < 118 && Ar.Tell() - StartPos != Tag.Size)
                    {
                        GLog->Logf(NAME_Warning, TEXT("Tag %s of %s size mismatch.  Serialized %d but expected %d"), *Tag.Name, GetName(), Ar.Tell() - StartPos, Tag.Size);
                        Ar.Seek(StartPos + Tag.Size);
                    }
                    continue;
                }
            }

            // skip the value
            for (INT i = 0; i < Tag.Size; i++)
            {
                BYTE Skipped;
                Ar << Skipped;
            }
        }
    }
    else
    {
        const UBOOL IsClass = IsA(UClass::StaticClass());
        for (TFieldIterator<UProperty> It(this); It; ++It)
        {
            UProperty* Property = *It;
            if (Unknown && (Property->PropertyFlags & CPF_Static))
                continue;
            if (!Property->ShouldSerializeValue(Ar))
                continue;
            for (INT Index = 0; Index < Property->ArrayDim; Index++)
            {
                const INT Offset = Property->Offset + Index * Property->GetElementSize();
                // only values that differ from the defaults are saved
                if (IsClass || Defaults)
                {
                    BYTE* Default = Offset + Property->GetElementSize() <= DefaultsCount ? Defaults : nullptr;
                    if (Property->Matches(Data, Default, Index))
                        continue;
                }
                FPropertyTag Tag(Ar, Property, Index, Data + Offset);
                Ar << Tag;
                Tag.SerializeTaggedProperty(Ar, Property, Data + Offset, 0, 0);
            }
        }
        FName Temp(NAME_None);
        Ar << Temp;
    }
}

// 0x0009a683
void UState::Serialize(FArchive& Ar)
{
    UStruct::Serialize(Ar);
    Ar << ProbeMask << IgnoreMask << LabelTableOffset << StateFlags;
    if (FuncHash)
        Ar.CountBytes(FuncHash, FUNC_HASH_BUCKETS * sizeof(UStruct*));
}

// 0x0009ae9c
void UFunction::Serialize(FArchive& Ar)
{
    UStruct::Serialize(Ar);

    if (Ar.Ver() <= 63)
        Ar << ParmsSize;
    Ar << iNative;
    if (Ar.Ver() <= 63)
        Ar << NumParms;
    Ar << OperPrecedence;
    if (Ar.Ver() <= 63)
        Ar << ReturnValueOffset;
    Ar << FunctionFlags;
    if (FunctionFlags & FUNC_Net)
        Ar << RepOffset;

    if (Ar.IsLoading())
    {
        // the parameters are the leading properties among the children
        NumParms = 0;
        ParmsSize = 0;
        ReturnValueOffset = 0xFFFF;
        for (UField* Field = Children; Field; Field = Field->Next)
        {
            if (!Field->IsA(UProperty::StaticClass()))
                continue;
            UProperty* Property = static_cast<UProperty*>(Field);
            if (!(Property->PropertyFlags & CPF_Parm))
                break;
            NumParms++;
            ParmsSize = static_cast<_WORD>(Property->Offset + Property->GetSize());
            if (Property->PropertyFlags & CPF_ReturnParm)
                ReturnValueOffset = Property->Offset;
        }
    }
}

// 0x0009a919
UBOOL UClass::IsDefaultsClass()
{
    if (!SuperField)
        return 0;
    static TCHAR DefaultsName[NAME_SIZE * 2];
    appSprintf(DefaultsName, TEXT("%sDefaults"), SuperField->GetName());
    return !appStricmp(GetName(), DefaultsName);
}

// 0x0009bb55
void UClass::Serialize(FArchive& Ar)
{
    UState::Serialize(Ar);

    // read but not kept on the xbox
    TArray<FDependency> Dependencies;
    TArray<FName> PackageImports;
    TArray<FName> HideCategories;

    if (Ar.IsLoading())
    {
        ProbeMask = SuperField ? static_cast<UState*>(SuperField)->ProbeMask : 0;
        for (TFieldIterator<UFunction> Function(this); Function; ++Function)
        {
            const INT NameIndex = Function->GetFName().GetIndex();
            if (Function->GetOwnerClass() == this && NameIndex >= NAME_PROBEMIN && NameIndex < NAME_PROBEMAX && (Function->FunctionFlags & FUNC_Defined))
                ProbeMask |= 1ull << (NameIndex - NAME_PROBEMIN);
        }
        if (!Children && !IgnoreMask)
            IgnoreMask = ~0ull;
    }

    if (Ar.Ver() <= 61)
    {
        INT OldClassRecordSize = 0;
        Ar << OldClassRecordSize;
        SetFlags(RF_Public | RF_Standalone);
    }
    Ar << ClassFlags << ClassGuid;
    Ar << Dependencies << PackageImports;
    if (Ar.Ver() >= 62)
        Ar << reinterpret_cast<UObject*&>(ClassWithin) << ClassConfigName;
    else
        ClassConfigName = FName(TEXT("System"), FNAME_Add);
    if (Ar.Ver() >= 99)
        Ar << HideCategories;

    if (Ar.IsLoading())
    {
        if (IsDefaultsClass())
        {
            // a "<Super>Defaults" class carries its parent's default properties and subobjects
            UClass* Super = GetSuperClass();
            ClassFlags ^= (Super->ClassFlags ^ ClassFlags) & CLASS_Abstract;
            Super->SerializeTaggedProperties(Ar, Super->Defaults.GetData(), this, 0);
            for (INT i = 0; i < GObjObjects.Num(); i++)
            {
                UObject* Object = GObjObjects[i];
                if (Object && Object->GetOuter() == this)
                    Object->Rename(*Object->GetFName(), GetSuperClass());
            }
        }
        else
        {
            const INT Size = GetPropertiesSize();
            if (Defaults.Num() > Size)
                Defaults.Remove(Size, Defaults.Num() - Size);
            else if (Defaults.Num() < Size)
                Defaults.AddZeroed(Size - Defaults.Num());
            GetDefaultObject()->InitClassDefaultObject(this, 0);
            SerializeTaggedProperties(Ar, Defaults.GetData(), GetSuperClass(), 0);

            // defaults can also come from a class in the Properties package
            if (!Ar.ArUnknown16)
            {
                UObject* DefaultsObject = StaticLoadObject(UClass::StaticClass(), nullptr, *FString::Printf(TEXT("Properties.%sDefaults"), GetName()),
                    nullptr, LOAD_Unknown2002, nullptr);
                if (DefaultsObject)
                    DefaultsObject->GetLinker()->Preload(DefaultsObject);
            }
            GetDefaultObject()->LoadConfig(0, nullptr, nullptr);
            GetDefaultObject()->LoadLocalized();
        }
        ClassUnique = 0;
        if (Ar.Ver() <= 61)
            ClassWithin = UObject::StaticClass();
    }
    else if (Ar.IsSaving())
    {
        SerializeTaggedProperties(Ar, Defaults.GetData(), GetSuperClass(), 0);
    }
    else
    {
        if (Defaults.Num())
        {
            Defaults.CountBytes(Ar);
            SerializeBin(Ar, Defaults.GetData(), 0);
        }
        if (Ar.IsCountingMem())
        {
            ClassReps.CountBytes(Ar);
            Ar << reinterpret_cast<TArray<UObject*>&>(NetFields) << UnknownD4;
            // serialized as an object reference although the native function table is kept here
            Ar << reinterpret_cast<UObject*&>(NativeFunctionTable);
        }
    }
}

// 0x00025ffc
void UEnum::Serialize(FArchive& Ar)
{
    UField::Serialize(Ar);

    // older packages stored a SuperField here
    if (Ar.Ver() < 137)
    {
        UObject* Dummy = nullptr;
        Ar << Dummy;
    }
    Ar << Names;
}
