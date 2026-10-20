#pragma once

// a field of a class as the net code numbers it
struct FFieldNetCache
{
    UField* Field;
    INT     FieldNetIndex;
    // the replication condition the field is sent under
    INT     ConditionIndex;

    FFieldNetCache() = default;
    FFieldNetCache(UField* InField, INT InFieldNetIndex, INT InConditionIndex)
        : Field(InField), FieldNetIndex(InFieldNetIndex), ConditionIndex(InConditionIndex) {}
};

// the net fields of a class, numbered after its super class's
class FClassNetCache
{
public:
    TArray<FFieldNetCache*>         RepProperties;
    INT                             FieldsBase;
    FClassNetCache*                 Super;
    INT                             RepConditionCount;
    UClass*                         Class;
    TArray<FFieldNetCache>          Fields;
    TMap<UObject*, FFieldNetCache*> FieldMap;

    // 0x000c2218
    explicit FClassNetCache(UClass* InClass) : Class(InClass) {}

    INT GetMaxIndex() const { return FieldsBase + Fields.Num(); }

    // 0x000545b8. searches this class and then its supers.
    FFieldNetCache* GetFromField(UObject* Field)
    {
        for (FClassNetCache* C = this; C; C = C->Super)
        {
            FFieldNetCache** Found = C->FieldMap.Find(Field);
            if (Found && *Found)
                return *Found;
        }
        return nullptr;
    }
};

// the caches are rebuilt rather than saved
inline FArchive& operator<<(FArchive& Ar, FClassNetCache*&) { return Ar; }

// a package both sides of a connection need, and where its objects and names start in the net numbering
struct FPackageInfo
{
    FString URL;
    UObject* Parent;
    FGuid   Guid;
    INT     FileSize;
    // set to the file size as well; its use is not known yet
    INT     Unknown20;
    INT     ObjectBase;
    INT     ObjectCount;
    INT     NameBase;
    INT     NameCount;
    INT     LocalGeneration;
    INT     RemoteGeneration;
    DWORD   PackageFlags;

    // 0x000c22bb
    explicit FPackageInfo(ULinkerLoad* Linker = nullptr);

    // 0x000c1b1f. finds or opens the package's linker.
    ULinkerLoad* GetLinker(DWORD LoadFlags) const;

    // only the package is serialized, so it is kept from garbage collection
    friend FArchive& operator<<(FArchive& Ar, FPackageInfo& Info)
    {
        return Ar << Info.Parent;
    }
};

// maps objects and names to and from the indices sent over the network
class UPackageMap : public UObject
{
    DECLARE_CLASS(UPackageMap, UObject, CLASS_Transient, UObject)
public:
    UPackageMap() = default;

    // 0x000c2f57
    void Serialize(FArchive& Ar) override;
    // 0x000c26c8
    void Destroy() override;

    // 0x000c1b50
    virtual UBOOL CanSerializeObject(UObject* Obj);
    // 0x000c1b68
    virtual UBOOL SerializeObject(FArchive& Ar, UClass* Class, UObject*& Obj);
    // 0x000c1d8a
    virtual UBOOL SerializeName(FArchive& Ar, FName& Name);
    // 0x000c1e55
    virtual INT ObjectToIndex(UObject* Object);
    // 0x000c1f14. loads the object from its package when Load is set and it is not in memory yet.
    virtual UObject* IndexToObject(INT Index, UBOOL Load);
    // 0x000c2c49. numbers the objects and names of every package in the list.
    virtual void Compute();
    // 0x000c28ee
    virtual FClassNetCache* GetClassNetCache(UClass* Class);
    // 0x000c1c06. whether objects from this package may be sent over the network.
    virtual UBOOL SupportsPackage(UObject* InOuter);
    // 0x000c2fb6
    virtual INT AddPackage(const FPackageInfo& Info);
    // 0x000c23b1. adds the linker's package and every package it imports.
    virtual INT AddLinker(ULinkerLoad* Linker);
    // 0x000c2c38
    virtual void RemovePackage(INT Index);
    // 0x000c24eb
    virtual FPackageInfo& AddNewPackage();

    TArray<FPackageInfo> List;

protected:
    // every call goes to this map instead when it is set. nothing sets it yet.
    UPackageMap*                    Unknown30 = nullptr;
    TMap<UObject*, INT>             LinkerMap;
    TMap<UObject*, FClassNetCache*> ClassFieldIndices;
    // each name's net index, by name index
    TArray<INT>                     NameIndices;
    TArray<FName>                   NamesByIndex;
    TArray<UObject*>                ObjectsByIndex;
};
