#pragma once

#include "UnObjBas.h"
#include "UnArc.h"

// lazy loaders read their data right away while this is 0. it is 0 unless code sets it around a load.
extern UBOOL GLazyLoad;
// flag 8 makes lazy arrays load at once and skips some serialization. the xbox never sets it.
extern DWORD GUglyHackFlags;

// data loaded from a package file only when first used
class FLazyLoader
{
public:
    virtual void Load() {}
    virtual void Unload() {}

    FArchive* SavedAr;
    INT       SavedPos; // file offset of the data, negated once it is loaded
};

// an array whose contents stay in the package file until Load is called
template<typename T>
class TLazyArray : public FLazyLoader, public TArray<T>
{
public:
    TLazyArray()
    {
        SavedAr = nullptr;
        SavedPos = 0;
    }
    TLazyArray(const TLazyArray&) = delete;
    TLazyArray& operator=(const TLazyArray&) = delete;

    // 0x0005dee9 for bytes
    ~TLazyArray()
    {
        if (SavedAr)
            SavedAr->DetachLazyLoader(this);
    }

    // 0x0005f297 for bytes
    void Load() override
    {
        if (SavedPos > 0)
        {
            const INT PushedPos = SavedAr->Tell();
            SavedAr->Seek(SavedPos);
            *SavedAr << static_cast<TArray<T>&>(*this);
            SavedPos = -SavedPos;
            SavedAr->Seek(PushedPos);
        }
    }

    // 0x0005f2d0 for bytes
    void Unload() override
    {
        if (SavedPos < 0)
        {
            TArray<T>::Empty();
            SavedPos = -SavedPos;
        }
    }

    // 0x0005f1a5 for bytes. the data is stored after the file offset just past it, so loading can skip it.
    void Serialize(FArchive& Ar)
    {
        if (Ar.IsLoading() && !Ar.ArUnknown16)
        {
            INT SeekPos = 0;
            if (Ar.Ver() <= 61)
            {
                // older packages store the element count instead of the data's end
                Ar.AttachLazyLoader(this);
                INT Count;
                Ar << AR_INDEX(Count);
                SeekPos = Ar.Tell() + Count * static_cast<INT>(sizeof(T));
            }
            else
            {
                Ar << SeekPos;
                if (GUglyHackFlags & 8)
                    Ar << static_cast<TArray<T>&>(*this);
                else
                    Ar.AttachLazyLoader(this);
            }
            if (!GLazyLoad)
                Load();
            Ar.Seek(SeekPos);
        }
        else if (Ar.IsSaving() && !Ar.ArUnknown16)
        {
            INT CountPos = Ar.Tell();
            Ar << CountPos;
            Ar << static_cast<TArray<T>&>(*this);
            INT EndPos = Ar.Tell();
            Ar.Seek(CountPos);
            Ar << EndPos;
            Ar.Seek(EndPos);
        }
        else
        {
            Ar << static_cast<TArray<T>&>(*this);
        }
    }
};

// first four bytes of every package file
enum : DWORD { PACKAGE_FILE_TAG = 0x9E2A83C1 };

// oldest package version the engine loads without asking
extern INT GPackageFileMinVersion;

// package flags (FPackageFileSummary::PackageFlags and UPackage::PackageFlags)
enum EPackageFlags : DWORD
{
    PKG_AllowDownload  = 0x00000001, // clients may download the file
    PKG_ServerSideOnly = 0x00000004, // never sent to clients
    PKG_BrokenLinks = 0x00000008, // some imports failed to resolve
    PKG_Unknown20   = 0x00000020, // copied into FArchive::ArUnknown16 by the linkers
    PKG_NeedsVerify = 0x00000040, // GetPackageLinker verifies the imports right away
};

// load flags for GetPackageLinker and StaticLoadObject
enum ELoadFlagsLinker : DWORD
{
    LOAD_NoFail    = 0x00000001, // passed by the game's main when it loads the engine class
    LOAD_Unknown2  = 0x00000002, // passed by DynamicLoadObject
    LOAD_Throw     = 0x00000008, // throw on failure instead of returning null
    LOAD_Verify    = 0x00000010, // only check that things exist: skips import verification and object creation
    LOAD_AllowDll  = 0x00000020, // a package with native code needs no file
    LOAD_NoFiles   = 0x00000040, // StaticLoadObject only looks for objects already in memory
    LOAD_NoVerify  = 0x00000080, // leaves the imports unverified
    LOAD_Forgiving = 0x00001000, // a missing import marks the package broken instead of throwing
    LOAD_NoThrow   = 0x00002000, // StaticLoadObject returns null for a missing object instead of throwing
    LOAD_Quiet     = 0x00008000, // fail silently
};

// object and name counts of an earlier save of the same package
struct FGenerationInfo
{
    INT ExportCount;
    INT NameCount;

    friend FArchive& operator<<(FArchive& Ar, FGenerationInfo& Info);
};

// the header of a package file
struct FPackageFileSummary
{
    INT   Tag;
    INT   FileVersion; // version in the low word, licensee version in the high word
    DWORD PackageFlags;
    INT   NameCount;
    INT   NameOffset;
    INT   ExportCount;
    INT   ExportOffset;
    INT   ImportCount;
    INT   ImportOffset;
    FGuid Guid;
    TArray<FGenerationInfo> Generations;

    friend FArchive& operator<<(FArchive& Ar, FPackageFileSummary& Sum);
};

// an object a package file needs from another package
struct FObjectImport
{
    FName        ClassPackage;
    FName        ClassName;
    INT          PackageIndex;
    FName        ObjectName;
    UObject*     XObject;
    ULinkerLoad* SourceLinker;
    INT          SourceIndex;

    friend FArchive& operator<<(FArchive& Ar, FObjectImport& Import);
};

// an object a package file contains
struct FObjectExport
{
    INT      ClassIndex;
    INT      SuperIndex;
    INT      PackageIndex;
    INT      Unknown0C; // added in version 159: if set, exports Unknown0C-1 up to this one are created and loaded first
    FName    ObjectName;
    DWORD    ObjectFlags;
    INT      SerialSize;
    INT      SerialOffset;
    UObject* _Object;
    INT      _iHashNext;

    friend FArchive& operator<<(FArchive& Ar, FObjectExport& Export);
};

FArchive& operator<<(FArchive& Ar, FGuid& G);

// what ULinkerLoad and ULinkerSave share: the tables of a package file
class ULinker : public UObject
{
    DECLARE_CLASS(ULinker, UObject, CLASS_Transient, UObject)
public:
    ULinker() = default;
    ULinker(UObject* InRoot, const TCHAR* InFilename);

    void Serialize(FArchive& Ar) override;

    FName GetExportClassPackage(INT i) const;
    FName GetExportClassName(INT i) const;

    UObject*              LinkerRoot;
    FPackageFileSummary   Summary;
    TArray<FName>         NameMap;
    TArray<FObjectImport> ImportMap;
    TArray<FObjectExport> ExportMap;
    INT                   Success;
    FString               Filename;
    DWORD                 ContextFlags;
    BYTE                  Unknown90[16]; // printed as hex by a helper at 0x000c4335
};

// reads objects out of a package file
class ULinkerLoad : public ULinker, public FArchive
{
    DECLARE_CLASS(ULinkerLoad, ULinker, CLASS_Transient, UObject)
public:
    ULinkerLoad() = default;
    ULinkerLoad(UObject* InParent, const TCHAR* InFilename, DWORD InLoadFlags);

    void Serialize(FArchive& Ar) override;
    void Destroy() override;

    void DetachExport(INT i);
    void DetachAllLazyLoaders(UBOOL Load);
    // finds an export by class, name and optionally outer and creates it; (UObject*)-1 for LOAD_Verify
    UObject* Create(UClass* ObjectClass, FName ObjectName, UObject* Outer, DWORD LoadFlags, UBOOL Checked);
    void LoadAllObjects();
    // "ClassName Package.Name" of an import, for messages
    FString GetImportFullName(INT i);

    // FArchive
    void Serialize(void* V, INT Length) override;
    FArchive& operator<<(FName& Name) override;
    FArchive& operator<<(UObject*& Object) override;
    INT Tell() override;
    INT TotalSize() override;
    void Seek(INT Pos) override;
    void Preload(UObject* Object) override;
    void AttachLazyLoader(FLazyLoader* Loader) override;
    void DetachLazyLoader(FLazyLoader* Loader) override;

    UObject* IndexToObject(INT Index);
    UObject* CreateExport(INT Index);
    UObject* CreateImport(INT Index);
    void VerifyImport(INT Index);
    void Verify();

    DWORD                 LoadFlags;
    // Verify sets it; Preload also stores the loader's position here
    UBOOL                 Verified;
    INT                   ExportHash[256];
    TArray<FLazyLoader*>  LazyLoaders;
    FArchive*             Loader;
    // a linker whose package names this one as its first import; CreateExport looks there first
    ULinkerLoad*          ChildLinker;
};

// searches the package paths for a package file; Out gets the path found
UBOOL appFindPackageFile(const TCHAR* In, const FGuid* Guid, TCHAR* Out);

// finds or opens the linker for a package
ULinkerLoad* GetPackageLinker(UObject* InOuter, const TCHAR* InFilename, DWORD LoadFlags, UPackageMap* Sandbox, FGuid* CompatibleGuid);
// 0x00022f2e. reports a caught load error as the flags ask: a warning, passing Error on, or a fatal error.
void SafeLoadError(DWORD LoadFlags, const TCHAR* Error, const TCHAR* Fmt, ...);
