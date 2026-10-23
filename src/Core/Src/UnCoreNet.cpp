#include "Core.h"

// ---------------------------------------------------------------------------------------------- package info

FPackageInfo::FPackageInfo(ULinkerLoad* Linker)
    : URL()
    , Parent(Linker ? Linker->LinkerRoot : nullptr)
    , Guid(Linker ? Linker->Summary.Guid : FGuid{ 0, 0, 0, 0 })
    , FileSize(Linker ? Linker->Loader->TotalSize() : 0)
    , Unknown20(Linker ? Linker->Loader->TotalSize() : 0)
    , ObjectBase(0)
    , ObjectCount(0)
    , NameBase(0)
    , NameCount(0)
    , LocalGeneration(0)
    , RemoteGeneration(0)
    , PackageFlags(Linker ? Linker->Summary.PackageFlags : 0)
{
    if (Linker && (Linker->Summary.PackageFlags & PKG_AllowDownload))
        URL = Linker->Filename;
}

ULinkerLoad* FPackageInfo::GetLinker(DWORD LoadFlags) const
{
    FGuid LinkerGuid = Guid;
    return GetPackageLinker(Parent, nullptr, LoadFlags | LOAD_Unknown2 | LOAD_Throw | LOAD_NoVerify, nullptr, &LinkerGuid);
}

// ---------------------------------------------------------------------------------------------- package map

void UPackageMap::Serialize(FArchive& Ar)
{
    Super::Serialize(Ar);
    Ar << List;
    if (!Ar.IsPersistent())
        Ar << LinkerMap << ClassFieldIndices << NameIndices;
    if (Ar.IsCountingMem())
    {
        Ar << NamesByIndex;
        ObjectsByIndex.CountBytes(Ar);
    }
}

void UPackageMap::Destroy()
{
    Super::Destroy();
    for (INT i = 0; i < ClassFieldIndices.Num(); i++)
        delete ClassFieldIndices.ValueAt(i);
}

UBOOL UPackageMap::CanSerializeObject(UObject* Obj)
{
    GError->Logf(TEXT("Unexpected UPackageMap::CanSerializeObject"));
    return 1;
}

UBOOL UPackageMap::SerializeObject(FArchive& Ar, UClass* Class, UObject*& Obj)
{
    GError->Logf(TEXT("Unexpected UPackageMap::SerializeObject"));
    return 1;
}

UBOOL UPackageMap::SerializeName(FArchive& Ar, FName& Name)
{
    if (Unknown30)
        return Unknown30->SerializeName(Ar, Name);
    const INT NameIndex = Name.GetIndex();
    DWORD Index = (NameIndex >= 0 && NameIndex < NameIndices.Num()) ? NameIndices[NameIndex] : NamesByIndex.Num();
    Ar.SerializeInt(Index, NamesByIndex.Num() + 1);
    if (!Ar.IsLoading())
        return Index != static_cast<DWORD>(NamesByIndex.Num());
    if (static_cast<INT>(Index) < 0 || static_cast<INT>(Index) >= NamesByIndex.Num() || Ar.IsError())
        Name = NAME_None;
    else
        Name = NamesByIndex[Index];
    return 1;
}

INT UPackageMap::ObjectToIndex(UObject* Object)
{
    INT Result = INDEX_NONE;
    if (!Object || Object->GetLinkerIndex() == INDEX_NONE)
        return Result;
    if (Unknown30)
        return Unknown30->ObjectToIndex(Object);

    // packages with flag 0x20 are checked object by object first
    for (INT i = 0; i < List.Num(); i++)
    {
        const FPackageInfo& Info = List[i];
        if ((Info.PackageFlags & PKG_Unknown20) && Object->GetLinkerIndex() < Info.ObjectCount
            && ObjectsByIndex[Info.ObjectBase + Object->GetLinkerIndex()] == Object)
        {
            Result = Info.ObjectBase + Object->GetLinkerIndex();
            if (Result >= 0)
                return Result;
            break;
        }
    }
    if (INT* Found = LinkerMap.Find(Object->GetOutermostPackage()))
    {
        const FPackageInfo& Info = List[*Found];
        if (Object->GetLinkerIndex() < Info.ObjectCount)
            Result = Info.ObjectBase + Object->GetLinkerIndex();
    }
    return Result;
}

UObject* UPackageMap::IndexToObject(INT Index, UBOOL Load)
{
    if (Unknown30)
        return Unknown30->IndexToObject(Index, Load);
    if (Index < 0 || Index >= ObjectsByIndex.Num())
        return nullptr;
    if (!ObjectsByIndex[Index] && Load)
    {
        INT Remaining = Index;
        for (INT i = 0; i < List.Num(); i++)
        {
            const FPackageInfo& Info = List[i];
            if (Remaining < Info.ObjectCount)
            {
                BeginLoad();
                ULinkerLoad* Linker = Info.GetLinker(LOAD_Verify);
                ObjectsByIndex[Index] = Linker->CreateExport(Remaining);
                EndLoad();
                GLog->Logf(NAME_Warning, TEXT("Net Dynamic load %s"), ObjectsByIndex[Index]->GetPathName());
                break;
            }
            Remaining -= Info.ObjectCount;
        }
    }
    return ObjectsByIndex[Index];
}

void UPackageMap::Compute()
{
    if (Unknown30)
        return;
    BeginLoad();
    for (INT i = 0; i < List.Num(); i++)
        List[i].GetLinker(0);
    NameIndices.Empty(FName::GetMaxNames());
    NameIndices.Add(FName::GetMaxNames());
    for (INT i = 0; i < NameIndices.Num(); i++)
        NameIndices[i] = INDEX_NONE;
    ClassFieldIndices.Empty();
    NamesByIndex.Empty();
    ObjectsByIndex.Empty();

    for (INT i = 0; i < List.Num(); i++)
    {
        FPackageInfo& Info = List[i];
        Info.ObjectBase = ObjectsByIndex.Num();
        Info.NameBase = NamesByIndex.Num();
        ULinkerLoad* Linker = Info.GetLinker(0);
        if (Linker->Summary.PackageFlags & PKG_AllowDownload)
            Info.URL = Linker->Filename;
        if (Info.PackageFlags & PKG_Unknown20)
            Linker->LoadAllObjects();

        // an older remote generation only sees the objects and names that generation had
        INT ExportCount = Linker->ExportMap.Num();
        INT NameCount = Linker->NameMap.Num();
        Info.LocalGeneration = Linker->Summary.Generations.Num();
        if (Info.RemoteGeneration == 0)
            Info.RemoteGeneration = Info.LocalGeneration;
        if (Info.RemoteGeneration < Info.LocalGeneration)
        {
            const FGenerationInfo& Generation = Linker->Summary.Generations[Info.RemoteGeneration - 1];
            ExportCount = Min(ExportCount, Generation.ExportCount);
            NameCount = Min(NameCount, Generation.NameCount);
        }

        ObjectsByIndex.AddZeroed(ExportCount);
        for (INT j = 0; j < ExportCount; j++)
        {
            UObject* Object = Linker->ExportMap[j]._Object;
            ObjectsByIndex[Info.ObjectBase + j] = (Object && Object->GetLinker() == Linker) ? Object : nullptr;
        }
        for (INT j = 0; j < NameCount; j++)
        {
            const FName& Name = Linker->NameMap[j];
            if (NameIndices[Name.GetIndex()] == INDEX_NONE)
            {
                NameIndices[Name.GetIndex()] = NamesByIndex.Num();
                NamesByIndex.AddItem(Name);
            }
        }
        Info.ObjectCount = ObjectsByIndex.Num() - Info.ObjectBase;
        Info.NameCount = NamesByIndex.Num() - Info.NameBase;
        LinkerMap.Set(Info.Parent, i);
    }
    EndLoad();
}

FClassNetCache* UPackageMap::GetClassNetCache(UClass* Class)
{
    if (Unknown30)
        return Unknown30->GetClassNetCache(Class);
    FClassNetCache** Found = ClassFieldIndices.Find(Class);
    FClassNetCache* Result = Found ? *Found : nullptr;
    if (Result || ObjectToIndex(Class) == INDEX_NONE)
        return Result;

    Result = ClassFieldIndices.Set(Class, new FClassNetCache(Class));
    Result->Super = nullptr;
    Result->RepConditionCount = 0;
    Result->FieldsBase = 0;
    if (Class->GetSuperClass())
    {
        Result->Super = GetClassNetCache(Class->GetSuperClass());
        Result->RepProperties = Result->Super->RepProperties;
        Result->RepConditionCount = Result->Super->RepConditionCount;
        Result->FieldsBase = Result->Super->GetMaxIndex();
    }

    // number the fields; a property owning its replication condition gets a new one
    Result->Fields.Empty(Class->NetFields.Num());
    for (INT i = 0; i < Class->NetFields.Num(); i++)
    {
        UField* Field = Class->NetFields[i];
        if (ObjectToIndex(Field) == INDEX_NONE)
            continue;
        const INT ThisIndex = Result->GetMaxIndex();
        INT ConditionIndex = INDEX_NONE;
        UProperty* Property = Cast<UProperty>(Field);
        if (Property && (Property->RepOwner == Property || ObjectToIndex(Property->RepOwner) == INDEX_NONE))
            ConditionIndex = Result->RepConditionCount++;
        Result->Fields.AddItem(FFieldNetCache(Field, ThisIndex, ConditionIndex));
    }
    Result->Fields.Shrink();
    for (INT i = 0; i < Result->Fields.Num(); i++)
        Result->FieldMap.Set(Result->Fields[i].Field, &Result->Fields[i]);

    // properties share their owner's condition, and are replicated by script unless their class does it in C++
    for (INT i = 0; i < Result->Fields.Num(); i++)
    {
        FFieldNetCache* FieldCache = &Result->Fields[i];
        UProperty* Property = Cast<UProperty>(FieldCache->Field);
        if (!Property)
            continue;
        if (FieldCache->ConditionIndex == INDEX_NONE)
            FieldCache->ConditionIndex = Result->GetFromField(Property->RepOwner)->ConditionIndex;
        if (!(Property->GetOwnerClass()->ClassFlags & CLASS_NativeReplication))
            Result->RepProperties.AddItem(FieldCache);
    }
    return Result;
}

UBOOL UPackageMap::SupportsPackage(UObject* InOuter)
{
    for (INT i = 0; i < List.Num(); i++)
    {
        if (List[i].Parent == InOuter)
            return 1;
    }
    return 0;
}

INT UPackageMap::AddPackage(const FPackageInfo& Info)
{
    if (Info.PackageFlags & PKG_ServerSideOnly)
        return INDEX_NONE;
    for (INT i = 0; i < List.Num(); i++)
    {
        if (List[i].Parent == Info.Parent)
            return i;
    }
    return List.AddItem(Info);
}

INT UPackageMap::AddLinker(ULinkerLoad* Linker)
{
    if (Linker->Summary.PackageFlags & PKG_ServerSideOnly)
        return INDEX_NONE;
    for (INT i = 0; i < List.Num(); i++)
    {
        if (List[i].Parent == Linker->LinkerRoot)
            return i;
    }
    const INT Index = List.Num();
    new (&List[List.Add()]) FPackageInfo(Linker);

    // the top level packages it imports, if they are loaded
    for (INT i = 0; i < Linker->ImportMap.Num(); i++)
    {
        const FObjectImport& Import = Linker->ImportMap[i];
        if (Import.ClassName != NAME_Package || Import.PackageIndex != 0 || Import.ObjectName == NAME_InGameTempName)
            continue;
        for (INT j = 0; j < GObjLoaders.Num(); j++)
        {
            if (GetLoader(j)->LinkerRoot->GetFName() == Import.ObjectName)
                AddLinker(GetLoader(j));
        }
    }
    return Index;
}

void UPackageMap::RemovePackage(INT Index)
{
    List.Remove(Index);
}

FPackageInfo& UPackageMap::AddNewPackage()
{
    return *new (&List[List.Add()]) FPackageInfo(nullptr);
}
