#include "Core.h"

INT GPackageFileMinVersion = 60;
UBOOL GIsClient;
UBOOL GIsServer;

// 0x000b17ef
FArchive& operator<<(FArchive& Ar, FGuid& G)
{
    return Ar << G.A << G.B << G.C << G.D;
}

// 0x000c3165
FArchive& operator<<(FArchive& Ar, FGenerationInfo& Info)
{
    Ar.Serialize(&Info.ExportCount, 4);
    Ar.Serialize(&Info.NameCount, 4);
    return Ar;
}

// 0x000c3cf9
FArchive& operator<<(FArchive& Ar, FPackageFileSummary& Sum)
{
    Ar.Serialize(&Sum.Tag, 4);
    Ar.Serialize(&Sum.FileVersion, 4);
    Ar.Serialize(&Sum.PackageFlags, 4);
    Ar.Serialize(&Sum.NameCount, 4);
    Ar.Serialize(&Sum.NameOffset, 4);
    Ar.Serialize(&Sum.ExportCount, 4);
    Ar.Serialize(&Sum.ExportOffset, 4);
    Ar.Serialize(&Sum.ImportCount, 4);
    Ar.Serialize(&Sum.ImportOffset, 4);

    if ((Sum.FileVersion & 0xFFFF) < 68)
    {
        // old packages had a list of heritage guids; the last one is the package guid
        INT HeritageCount, HeritageOffset;
        Ar.Serialize(&HeritageCount, 4);
        Ar.Serialize(&HeritageOffset, 4);
        const INT SavedPos = Ar.Tell();
        if (HeritageCount)
        {
            Ar.Seek(HeritageOffset);
            for (INT i = 0; i < HeritageCount; i++)
                Ar << Sum.Guid;
        }
        Ar.Seek(SavedPos);

        if (Ar.IsLoading())
        {
            Sum.Generations.Empty(1);
            Sum.Generations.AddItem({ Sum.ExportCount, Sum.NameCount });
        }
    }
    else
    {
        Ar << Sum.Guid;
        INT GenerationCount = Sum.Generations.Num();
        Ar.Serialize(&GenerationCount, 4);
        if (Ar.IsLoading())
        {
            Sum.Generations.Empty(GenerationCount);
            Sum.Generations.AddZeroed(GenerationCount);
        }
        for (INT i = 0; i < GenerationCount; i++)
            Ar << Sum.Generations[i];
    }
    return Ar;
}

// 0x000c310e
FArchive& operator<<(FArchive& Ar, FObjectImport& Import)
{
    Ar << Import.ClassPackage << Import.ClassName;
    Ar.Serialize(&Import.PackageIndex, 4);
    Ar << Import.ObjectName;
    if (Ar.IsLoading())
    {
        Import.SourceIndex = INDEX_NONE;
        Import.SourceLinker = nullptr;
        Import.XObject = nullptr;
    }
    return Ar;
}

// 0x000c3288
FArchive& operator<<(FArchive& Ar, FObjectExport& Export)
{
    Ar << AR_INDEX(Export.ClassIndex);
    Ar << AR_INDEX(Export.SuperIndex);
    Ar.Serialize(&Export.PackageIndex, 4);
    if (Ar.Ver() < 159)
        Export.Unknown0C = 0;
    else
        Ar << AR_INDEX(Export.Unknown0C);
    Ar << Export.ObjectName;
    Ar.Serialize(&Export.ObjectFlags, 4);

    if (Ar.Ver() < 151)
    {
        Ar << AR_INDEX(Export.SerialSize);
        if (Export.SerialSize)
            Ar << AR_INDEX(Export.SerialOffset);
    }
    else
    {
        Ar.Serialize(&Export.SerialSize, 4);
        Ar.Serialize(&Export.SerialOffset, 4);
    }
    return Ar;
}

// 0x000c35e9
ULinker::ULinker(UObject* InRoot, const TCHAR* InFilename)
    : LinkerRoot(InRoot)
    , Summary()
    , Success(123456)
    , Filename(InFilename)
    , ContextFlags(0)
    , Unknown90{}
{
    if (GIsClient)
        ContextFlags = RF_LoadForClient;
    if (GIsServer)
        ContextFlags |= RF_LoadForServer;
}

// 0x000c391f
void ULinker::Serialize(FArchive& Ar)
{
    UObject::Serialize(Ar);

    ImportMap.CountBytes(Ar);
    ExportMap.CountBytes(Ar);
    Ar << NameMap;
    Ar << LinkerRoot;

    for (INT i = 0; i < ExportMap.Num(); i++)
        Ar << ExportMap[i].ObjectName;

    for (INT i = 0; i < ImportMap.Num(); i++)
    {
        FObjectImport& Import = ImportMap[i];
        Ar << reinterpret_cast<UObject*&>(Import.SourceLinker);
        Ar << Import.ClassPackage << Import.ClassName;
    }
}

// 0x000c3367
FName ULinker::GetExportClassPackage(INT i) const
{
    const INT ClassIndex = ExportMap[i].ClassIndex;
    if (ClassIndex < 0)
    {
        // the class is an import: its package is the import's package
        const INT PackageIndex = ImportMap[-ClassIndex - 1].PackageIndex;
        return PackageIndex < 0 ? ImportMap[-PackageIndex - 1].ObjectName : ExportMap[PackageIndex - 1].ObjectName;
    }
    if (ClassIndex > 0)
        return LinkerRoot->GetFName();
    return FName(NAME_Core);
}

// 0x000c33c0
FName ULinker::GetExportClassName(INT i) const
{
    const INT ClassIndex = ExportMap[i].ClassIndex;
    if (ClassIndex < 0)
        return ImportMap[-ClassIndex - 1].ObjectName;
    if (ClassIndex > 0)
        return ExportMap[ClassIndex - 1].ObjectName;
    return FName(NAME_Class);
}

// 0x000c43c1
ULinkerLoad::ULinkerLoad(UObject* InParent, const TCHAR* InFilename, DWORD InLoadFlags)
    : ULinker(InParent, InFilename)
    , LoadFlags(InLoadFlags)
    , Verified(0)
    , ExportHash{}
    , Loader(nullptr)
    , ChildLinker(nullptr)
{
    Loader = GFileManager->CreateFileReader(InFilename, 0, GError);
    if (!Loader)
        appThrowf(LocalizeError(TEXT("OpenFailed"), TEXT("Core")));

    for (INT i = 0; i < GObjLoaders.Num(); i++)
    {
        if (GetLoader(i)->LinkerRoot == LinkerRoot)
            appThrowf(LocalizeError(TEXT("LinkerExists"), TEXT("Core")), LinkerRoot->GetName());
    }

    ArVer = GPackageFileVersion;
    ArLicenseeVer = GPackageFileLicenseeVersion;
    ArIsPersistent = true;
    ArIsLoading = true;
    ArForEdit = false;
    ArForClient = true;
    ArForServer = true;

    *this << Summary;
    ArVer = Summary.FileVersion & 0xFFFF;
    ArUnknown16 = (Summary.PackageFlags & PKG_Unknown20) != 0;
    ArLicenseeVer = (Summary.FileVersion >> 16) & 0xFFFF;

    if (LinkerRoot && LinkerRoot->IsA(UPackage::StaticClass()))
        static_cast<UPackage*>(LinkerRoot)->PackageFlags = Summary.PackageFlags;

    if (static_cast<DWORD>(Summary.Tag) != PACKAGE_FILE_TAG)
    {
        GWarn->Logf(LocalizeError(TEXT("BinaryFormat"), TEXT("Core")), *Filename);
        appThrowf(LocalizeError(TEXT("Aborted"), TEXT("Core")));
    }
    if ((Summary.FileVersion & 0xFFFF) < GPackageFileMinVersion)
    {
        // the xbox asks before loading old packages; with no one to ask, the port refuses
        GWarn->Logf(LocalizeError(TEXT("OldVersion"), TEXT("Core")), *Filename);
        appThrowf(LocalizeError(TEXT("Aborted"), TEXT("Core")));
    }

    ImportMap.Empty(Summary.ImportCount);
    ExportMap.Empty(Summary.ExportCount);
    NameMap.Empty(Summary.NameCount);

    // names not meant for this context (client, server) load as None
    if (Summary.NameCount > 0)
    {
        Loader->Seek(Summary.NameOffset);
        for (INT i = 0; i < Summary.NameCount; i++)
        {
            FNameEntry NameEntry;
            *this << NameEntry;
            NameMap.AddItem((NameEntry.Flags & ContextFlags) ? FName(NameEntry.Name, FNAME_Add) : FName(NAME_None));
        }
    }

    if (Summary.ImportCount > 0)
    {
        Loader->Seek(Summary.ImportOffset);
        for (INT i = 0; i < Summary.ImportCount; i++)
        {
            ImportMap.AddZeroed();
            *this << ImportMap.Last();
        }
    }

    if (Summary.ExportCount > 0)
    {
        Loader->Seek(Summary.ExportOffset);
        for (INT i = 0; i < Summary.ExportCount; i++)
        {
            ExportMap.AddZeroed();
            ExportMap.Last()._iHashNext = INDEX_NONE;
            ExportMap.Last()._Object = nullptr;
            *this << ExportMap.Last();
        }
    }

    // exports hash by class package, class name and object name
    for (INT& Hash : ExportHash)
        Hash = INDEX_NONE;
    for (INT i = 0; i < ExportMap.Num(); i++)
    {
        const INT iHash = (GetExportClassPackage(i).GetIndex() * 31 + GetExportClassName(i).GetIndex() * 7
            + ExportMap[i].ObjectName.GetIndex()) & 0xFF;
        ExportMap[i]._iHashNext = ExportHash[iHash];
        ExportHash[iHash] = i;
    }

    GObjLoaders.AddItem(this);

    // a package whose first import is another package (version 158 and up) registers itself with that package's linker
    if ((Summary.FileVersion & 0xFFFF) >= 158 && ImportMap.Num() && ImportMap[0].ClassName == NAME_Package
        && ImportMap[0].PackageIndex == 0)
    {
        const FName ParentName = ImportMap[0].ObjectName;
        if (ParentName != NAME_None && LinkerRoot && LinkerRoot->GetFName() != ParentName)
        {
            UObject* ParentPackage = CreatePackage(nullptr, *ParentName);
            if (ULinkerLoad* Parent = GetPackageLinker(ParentPackage, nullptr, 0, nullptr, nullptr))
                Parent->ChildLinker = this;
        }
    }

    Success = 1;
}

// 0x000c39e3
void ULinkerLoad::Serialize(FArchive& Ar)
{
    ULinker::Serialize(Ar);
    LazyLoaders.CountBytes(Ar);
}

// 0x000c3a59
void ULinkerLoad::Destroy()
{
    GLog->Logf(TEXT("Unloading: %s"), GetFullName());

    DetachAllLazyLoaders(0);
    for (INT i = 0; i < ExportMap.Num(); i++)
    {
        if (ExportMap[i]._Object)
            DetachExport(i);
    }

    for (INT i = 0; i < GObjLoaders.Num(); i++)
    {
        if (GObjLoaders[i] == this)
            GObjLoaders.Remove(i--);
    }

    delete Loader;
    Loader = nullptr;

    UObject::Destroy();
}

// 0x000c385b
void ULinkerLoad::DetachExport(INT i)
{
    FObjectExport& Export = ExportMap[i];
    if (!IsValid(Export._Object))
        appErrorf(TEXT("Linker object %s %s.%s is invalid"), *GetExportClassName(i), LinkerRoot->GetName(), *Export.ObjectName);

    if (Export._Object->GetLinker() == this && Export._Object->GetLinkerIndex() == i)
        Export._Object->SetLinker(nullptr, INDEX_NONE);
    else
        Export._Object = nullptr;
}

// 0x000c31c8
void ULinkerLoad::Serialize(void* V, INT Length)
{
    Loader->Serialize(V, Length);
}

// 0x000c31b2
INT ULinkerLoad::Tell()
{
    return Loader->Tell();
}

// 0x000c31bd
INT ULinkerLoad::TotalSize()
{
    return Loader->TotalSize();
}

// 0x000c31a7
void ULinkerLoad::Seek(INT Pos)
{
    Loader->Seek(Pos);
}

// 0x000270c6
FArchive& ULinkerLoad::operator<<(FName& Name)
{
    INT NameIndex;
    *Loader << AR_INDEX(NameIndex);
    if (NameIndex < 0 || NameIndex >= NameMap.Num())
        appErrorf(TEXT("Bad name index %i/%i"), NameIndex, NameMap.Num());
    Name = NameMap[NameIndex];
    return *this;
}

// 0x0002707f
FArchive& ULinkerLoad::operator<<(UObject*& Object)
{
    INT Index;
    *Loader << AR_INDEX(Index);
    Object = ArSkipObjectRefs ? nullptr : IndexToObject(Index);
    return *this;
}

// 0x000c3a02
void ULinkerLoad::DetachAllLazyLoaders(UBOOL Load)
{
    for (INT i = 0; i < LazyLoaders.Num(); i++)
    {
        FLazyLoader* LazyLoader = LazyLoaders[i];
        if (Load)
            LazyLoader->Load();
        LazyLoader->SavedAr = nullptr;
        LazyLoader->SavedPos = 0;
    }
    LazyLoaders.Empty();
}

// 0x000c412d
void ULinkerLoad::AttachLazyLoader(FLazyLoader* LazyLoader)
{
    LazyLoaders.AddItem(LazyLoader);
    LazyLoader->SavedAr = this;
    LazyLoader->SavedPos = Tell();
}

// 0x000c3aea
void ULinkerLoad::DetachLazyLoader(FLazyLoader* LazyLoader)
{
    const INT RemovedCount = LazyLoaders.RemoveItem(LazyLoader);
    if (RemovedCount != 1)
        appErrorf(TEXT("Detachment inconsistency: %i (%s)"), RemovedCount, *Filename);
    LazyLoader->SavedAr = nullptr;
    LazyLoader->SavedPos = 0;
}

// 0x000c33ff
void ULinkerLoad::Preload(UObject* Object)
{
    if (Object->GetLinker() != this)
    {
        if (Object->GetLinker())
            Object->GetLinker()->Preload(Object);
        return;
    }
    if (!(Object->GetFlags() & RF_NeedLoad))
        return;

    Object->SetFlags(RF_Preloading);
    if (Object->IsA(UStruct::StaticClass()) && static_cast<UStruct*>(Object)->SuperField)
        Preload(static_cast<UStruct*>(Object)->SuperField);

    FObjectExport& Export = ExportMap[Object->GetLinkerIndex()];
    const INT SavedPos = Loader->Tell();

    // the exports this one depends on load first
    if (Export.Unknown0C)
    {
        for (INT i = Export.Unknown0C - 1; i < Object->GetLinkerIndex(); i++)
        {
            UObject* Other = ExportMap[i]._Object;
            if (Other && !(Other->GetFlags() & RF_Preloading))
                Preload(Other);
        }
    }

    Verified = Export.SerialOffset;
    Loader->Seek(Export.SerialOffset);
    if (!GIsLevelLoading)
        GPeriodicCallbacks.Tick();

    Object->ClearFlags(RF_NeedLoad);
    Object->Serialize(*this);
    Object->ClearFlags(RF_Preloading);
    Object->SetFlags(RF_WasLoaded);

    if (Tell() - Export.SerialOffset != Export.SerialSize)
        appErrorf(LocalizeError(TEXT("SerialSize"), TEXT("Core")), Object->GetFullName(), Tell() - Export.SerialOffset, Export.SerialSize);

    Verified = Loader->Tell();
    Loader->Seek(SavedPos);
}

// 0x000c4ebc
UObject* ULinkerLoad::IndexToObject(INT Index)
{
    if (Index > 0)
    {
        if (Index - 1 < 0 || Index - 1 >= ExportMap.Num())
            appErrorf(LocalizeError(TEXT("ExportIndex"), TEXT("Core")), Index - 1, ExportMap.Num());
        return CreateExport(Index - 1);
    }
    if (Index < 0)
    {
        if (-Index - 1 < 0 || -Index - 1 >= ImportMap.Num())
            appErrorf(LocalizeError(TEXT("ImportIndex"), TEXT("Core")), -Index - 1, ImportMap.Num());
        return CreateImport(-Index - 1);
    }
    return nullptr;
}

// 0x000c3eaf
UObject* ULinkerLoad::CreateExport(INT Index)
{
    FObjectExport& Export = ExportMap[Index];
    if (Export._Object || !(Export.ObjectFlags & ContextFlags))
        return Export._Object;

    UClass* LoadClass = static_cast<UClass*>(IndexToObject(Export.ClassIndex));
    if (!LoadClass)
    {
        if (Export.ClassIndex)
            return nullptr;
        LoadClass = UClass::StaticClass();
    }

    // the xbox build never creates these
    if (LoadClass->GetFName() == NAME_Camera || LoadClass->GetFName() == NAME_PlayerInput)
        return nullptr;

    Preload(LoadClass);
    UObject* ThisParent = Export.PackageIndex ? IndexToObject(Export.PackageIndex) : LinkerRoot;

    // loading the class may have created this export already
    if (Export._Object)
        return Export._Object;

    if (LoadClass == UClass::StaticClass() && Export.SuperIndex && !IndexToObject(Export.SuperIndex))
    {
        GWarn->Logf(NAME_Warning, TEXT("Class %s.%s not loaded; superclass does not exist"), ThisParent->GetPathName(), *Export.ObjectName);
        return nullptr;
    }

    // top-level objects of a child package come from the child's linker when it has them
    if (ChildLinker && ThisParent == LinkerRoot && LoadClass != UPackage::StaticClass())
    {
        ThisParent = ChildLinker->LinkerRoot;
        if (UObject* Result = ChildLinker->Create(LoadClass, FName(*Export.ObjectName, FNAME_Add), ThisParent, 0x2000, 0))
            return Result;
    }

    // an existing object waiting to be loaded is reused
    Export._Object = StaticFindObjectFast(LoadClass, ThisParent, Export.ObjectName, 1);
    if (Export._Object && (Export._Object->GetFlags() & (RF_NeedLoad | RF_WasLoaded)))
    {
        if (Export._Object->GetLinkerIndex() == INDEX_NONE || (Summary.PackageFlags & PKG_Unknown20))
            Export._Object->SetLinker(this, Index);
        return Export._Object;
    }

    Export._Object = StaticConstructObject(LoadClass, ThisParent, Export.ObjectName,
        (Export.ObjectFlags & RF_Load) | RF_NeedLoad | RF_NeedPostLoad, nullptr, GError);
    Export._Object->SetLinker(this, Index);

    if (Export.Unknown0C)
    {
        for (INT i = Export.Unknown0C - 1; i < Index; i++)
            CreateExport(i);
    }

    GObjLoaded.AddItem(Export._Object);
    GLogDetail->Logf(NAME_DevLoad, TEXT("Created %s"), Export._Object->GetFullName());

    if (Export._Object->IsA(UStruct::StaticClass()) && Export.SuperIndex)
        static_cast<UStruct*>(Export._Object)->SuperField = static_cast<UStruct*>(IndexToObject(Export.SuperIndex));
    if (Export._Object->IsA(UClass::StaticClass()))
        static_cast<UClass*>(Export._Object)->Bind();

    return Export._Object;
}

// 0x000c4e3a
UObject* ULinkerLoad::CreateImport(INT Index)
{
    FObjectImport& Import = ImportMap[Index];
    if (!Import.XObject)
    {
        BeginLoad(FString(TEXT("")));
        VerifyImport(Index);
        EndLoad();

        if (Import.SourceIndex != INDEX_NONE)
        {
            Import.XObject = Import.SourceLinker->CreateExport(Import.SourceIndex);
            GImportCount++;
        }
    }
    return Import.XObject;
}

// 0x000c48d1
void ULinkerLoad::VerifyImport(INT Index)
{
    FObjectImport& Import = ImportMap[Index];
    if ((Import.SourceLinker && Import.SourceIndex != INDEX_NONE) || Import.ClassPackage == NAME_None
        || Import.ClassName == NAME_None || Import.ObjectName == NAME_None || Import.XObject)
        return;

    // packages are created, never loaded from a linker
    if (Import.ClassName == NAME_Package)
    {
        UObject* Outer = Import.PackageIndex ? IndexToObject(Import.PackageIndex) : nullptr;
        Import.XObject = CreatePackage(Outer, *Import.ObjectName);
        return;
    }

    VerifyImport(-Import.PackageIndex - 1);
    if (UObject* Pkg = ImportMap[-Import.PackageIndex - 1].XObject)
    {
        if (UClass* FindClass = static_cast<UClass*>(StaticFindObjectFast(UClass::StaticClass(), ANY_PACKAGE, Import.ClassName, 0)))
        {
            Import.XObject = StaticFindObjectFast(FindClass, Pkg, Import.ObjectName, 0);
            if (Import.XObject && (Import.XObject->GetFlags() & RF_Marked))
                return;

            // only the newest linker of a package with flag 0x20 is searched, and only then is the find repeated
            for (INT i = GObjLoaders.Num() - 1; i >= 0; i--)
            {
                if (!GetLoader(i)->ArUnknown16)
                    continue;

                Import.XObject = GetLoader(i)->Create(FindClass, Import.ObjectName, Pkg, 0, 0);
                if (Import.XObject)
                    return;

                UObject* Found = StaticFindObjectFast(FindClass, Pkg, Import.ObjectName, 0);
                Import.XObject = Found;
                if (Found && (Found->GetFlags() & RF_WasLoaded))
                    return;

                // native classes with class flag 0x1000000 have nothing to load
                UClass* FoundClass = Found && Found->IsA(UClass::StaticClass()) ? static_cast<UClass*>(Found) : nullptr;
                if (FoundClass && (FoundClass->GetFlags() & RF_Native) && (FoundClass->ClassFlags & 0x01000000))
                    return;
                break;
            }
        }
    }

    // the outermost package import owns the linker the object comes from
    FObjectImport* Top = &Import;
    while (Top->PackageIndex < 0)
        Top = &ImportMap[-Top->PackageIndex - 1];
    UPackage* TopPackage = Top->XObject && Top->XObject->IsA(UPackage::StaticClass()) ? static_cast<UPackage*>(Top->XObject) : nullptr;

    if (!Top->SourceLinker)
        Top->SourceLinker = GetPackageLinker(Top->XObject, nullptr, 0, nullptr, nullptr);
    Import.SourceLinker = Top->SourceLinker;

    if (ULinkerLoad* Source = Import.SourceLinker)
    {
        const INT iHash = (Import.ClassPackage.GetIndex() * 31 + Import.ClassName.GetIndex() * 7 + Import.ObjectName.GetIndex()) & 0xFF;
        for (INT j = Source->ExportHash[iHash]; j != INDEX_NONE; j = Source->ExportMap[j]._iHashNext)
        {
            FObjectExport& Export = Source->ExportMap[j];
            if (Export.ObjectName != Import.ObjectName || Source->GetExportClassName(j) != Import.ClassName
                || Source->GetExportClassPackage(j) != Import.ClassPackage)
                continue;

            // the export's outer has to match the import's, unless either is top-level
            UBOOL Match = Import.PackageIndex >= 0;
            if (!Match)
            {
                FObjectImport& OuterImport = ImportMap[-Import.PackageIndex - 1];
                Match = !OuterImport.SourceLinker || Export.PackageIndex == 0
                    || (OuterImport.SourceIndex != INDEX_NONE && OuterImport.SourceIndex + 1 == Export.PackageIndex);
            }
            if (Match)
            {
                Import.SourceIndex = j;
                break;
            }
        }
    }

    UBOOL Warned = 0;
    if (Import.SourceIndex == INDEX_NONE && TopPackage)
    {
        // not in the package file: accept an object that already exists in memory
        UPackage* ClassPackage = static_cast<UPackage*>(StaticFindObjectFast(UPackage::StaticClass(), nullptr, Import.ClassPackage, 0));
        UClass* FindClass = ClassPackage ? static_cast<UClass*>(StaticFindObjectFast(UClass::StaticClass(), ClassPackage, Import.ClassName, 0)) : nullptr;
        if (FindClass)
        {
            UObject* Found = StaticFindObjectFast(FindClass, TopPackage, Import.ObjectName, 0);
            UBOOL Accept;
            if (!Found)
            {
                Found = StaticFindObjectFast(FindClass, ANY_PACKAGE, Import.ObjectName, 0);
                Accept = Found != nullptr;
            }
            else
            {
                Accept = TopPackage->bDirty || (Found->GetFlags() & RF_Transient);
            }

            if (Accept)
            {
                Import.XObject = Found;
                GImportCount++;
            }
            else
            {
                GWarn->Logf(NAME_Warning, TEXT("Missing %s"), *GetImportFullName(Index));
                Warned = 1;
            }
        }
    }

    if (!Import.XObject && !Warned)
    {
        GLog->Logf(TEXT("Failed import: %s (file %s)"), *GetImportFullName(Index), *Import.SourceLinker->Filename);
        if (LoadFlags & LOAD_Forgiving)
        {
            if (LinkerRoot && LinkerRoot->IsA(UPackage::StaticClass()))
                static_cast<UPackage*>(LinkerRoot)->PackageFlags |= PKG_BrokenLinks;
            GLog->Logf(TEXT("Broken import: %s (file %s)"), *GetImportFullName(Index), *Import.SourceLinker->Filename);
        }
        else
        {
            appThrowf(LocalizeError(TEXT("FailedImport"), TEXT("Core")), TEXT(""), *GetImportFullName(Index));
        }
    }
}

// 0x000c4f63
void ULinkerLoad::Verify()
{
    if (!Verified)
    {
        if (LinkerRoot && LinkerRoot->IsA(UPackage::StaticClass()))
            static_cast<UPackage*>(LinkerRoot)->PackageFlags &= ~PKG_BrokenLinks;
        for (INT i = 0; i < Summary.ImportCount; i++)
            VerifyImport(i);
    }
    Verified = 1;
}

// 0x000c367c
UObject* ULinkerLoad::Create(UClass* ObjectClass, FName ObjectName, UObject* Outer, DWORD InLoadFlags, UBOOL Checked)
{
    const INT iHash = (ObjectClass->GetOuter()->GetFName().GetIndex() * 31 + ObjectClass->GetFName().GetIndex() * 7
        + ObjectName.GetIndex()) & 0xFF;
    for (INT i = ExportHash[iHash]; i != INDEX_NONE; i = ExportMap[i]._iHashNext)
    {
        FObjectExport& Export = ExportMap[i];
        if (Export.ObjectName == ObjectName && GetExportClassName(i) == ObjectClass->GetFName()
            && GetExportClassPackage(i) == ObjectClass->GetOuter()->GetFName()
            && (!Outer || (Export.PackageIndex ? IndexToObject(Export.PackageIndex) : LinkerRoot) == Outer))
        {
            return (InLoadFlags & LOAD_Verify) ? reinterpret_cast<UObject*>(-1) : CreateExport(i);
        }
    }

    // no exact class match: accept an export of a subclass
    for (INT i = 0; i < ExportMap.Num(); i++)
    {
        FObjectExport& Export = ExportMap[i];
        if (Export.ObjectName != ObjectName)
            continue;
        if (Outer && (Export.PackageIndex ? IndexToObject(Export.PackageIndex) : LinkerRoot) != Outer)
            continue;

        UObject* ClassObject = Export.ClassIndex ? IndexToObject(Export.ClassIndex) : UClass::StaticClass();
        UClass* ExportClass = ClassObject && ClassObject->IsA(UClass::StaticClass()) ? static_cast<UClass*>(ClassObject) : nullptr;
        if (ExportClass && ExportClass->IsChildOf(ObjectClass))
            return (InLoadFlags & LOAD_Verify) ? reinterpret_cast<UObject*>(-1) : CreateExport(i);
    }

    if (Checked)
        appThrowf(LocalizeError(TEXT("FailedCreate"), TEXT("Core")), ObjectClass->GetName(), *ObjectName);
    return nullptr;
}

// 0x000c4deb
void ULinkerLoad::LoadAllObjects()
{
    for (INT i = 0; i < Summary.ExportCount; i++)
    {
        CreateExport(i);
        if (Summary.PackageFlags & PKG_Unknown20)
        {
            GWarn->StatusUpdatef(i, Summary.ExportCount, nullptr);
            Preload(ExportMap[i]._Object);
        }
    }
}

// 0x000c4182
FString ULinkerLoad::GetImportFullName(INT i)
{
    FString Path;
    for (INT j = -i - 1; j != 0; j = ImportMap[-j - 1].PackageIndex)
    {
        if (j != -i - 1)
            Path = FString(TEXT(".")) + Path;
        Path = FString(*ImportMap[-j - 1].ObjectName) + Path;
    }
    return FString(*ImportMap[i].ClassName) + TEXT(" ") + Path;
}

// true for the packages whose guid GetPackageLinker checks against CompatibleGuid
static UBOOL IsGuidCheckedPackage(UObject* Package)
{
    return !appStrstr(Package->GetName(), TEXT("_Voice")) && appStrnicmp(Package->GetName(), TEXT("DM_"), 3)
        && appStrnicmp(Package->GetName(), TEXT("CTF_"), 4);
}

// 0x0002ac13
ULinkerLoad* GetPackageLinker(UObject* InOuter, const TCHAR* InFilename, DWORD LoadFlags, UPackageMap* Sandbox, FGuid* CompatibleGuid)
{
    if (InOuter)
    {
        for (INT i = 0; i < GObjLoaders.Num(); i++)
        {
            ULinkerLoad* Linker = UObject::GetLoader(i);
            if (Linker->LinkerRoot == InOuter)
            {
                if (IsGuidCheckedPackage(InOuter) && CompatibleGuid && Linker->Summary.Guid != *CompatibleGuid)
                    appThrowf(LocalizeError(TEXT("PackageVersion"), TEXT("Core")), InOuter->GetName());
                return Linker;
            }
        }
    }

    ULinkerLoad* Result = nullptr;
    TCHAR NewFilename[256] = TEXT("");
    if (InFilename && *InFilename)
    {
        // the package name is the file name without its directories and extension
        TCHAR PackageName[256];
        appStrncpy(PackageName, InFilename, 256);
        TCHAR* Name = PackageName;
        for (;;)
        {
            if (TCHAR* Found = appStrstr(Name, TEXT("\\")))
                Name = Found + appStrlen(TEXT("\\"));
            else if (TCHAR* Found = appStrstr(Name, TEXT("/")))
                Name = Found + 1;
            else if (TCHAR* Found = appStrstr(Name, TEXT(":")))
                Name = Found + 1;
            else
                break;
        }
        if (TCHAR* Dot = appStrstr(Name, TEXT(".")))
            *Dot = 0;

        UPackage* FilenamePkg = UObject::CreatePackage(nullptr, Name);
        if (!InOuter)
        {
            if (!FilenamePkg)
                appThrowf(LocalizeError(TEXT("FilenameToPackage"), TEXT("Core")), InFilename);
            InOuter = FilenamePkg;
            for (INT i = 0; i < GObjLoaders.Num() && !Result; i++)
            {
                if (UObject::GetLoader(i)->LinkerRoot == InOuter)
                    Result = UObject::GetLoader(i);
            }
        }
        else if (InOuter != FilenamePkg)
        {
            GLog->Logf(TEXT("New File, Existing Package (%s, %s)"), InOuter->GetFullName(), FilenamePkg->GetFullName());
            UObject::ResetLoaders(InOuter, 0, 1);
        }

        if (!Result && !appFindPackageFile(InFilename, CompatibleGuid, NewFilename))
        {
            if (!(LoadFlags & LOAD_Quiet) && (LoadFlags & LOAD_Throw))
                appThrowf(LocalizeError(TEXT("FileNotFound"), TEXT("Core")), InFilename);
            return nullptr;
        }
    }
    else
    {
        if (!InOuter)
            appThrowf(LocalizeError(TEXT("PackageResolveFailed"), TEXT("Core")));

        if (!appFindPackageFile(InOuter->GetName(), CompatibleGuid, NewFilename))
        {
            // a package with native code needs no file
            if ((LoadFlags & LOAD_AllowDll) && InOuter->IsA(UPackage::StaticClass()) && static_cast<UPackage*>(InOuter)->DllHandle)
                return nullptr;
            if (!(LoadFlags & LOAD_Quiet) && (LoadFlags & LOAD_Throw))
                appThrowf(LocalizeError(TEXT("PackageNotFound"), TEXT("Core")), InOuter->GetName());
            return nullptr;
        }
    }

    if (Sandbox && !Sandbox->SupportsPackage(InOuter))
        appThrowf(LocalizeError(TEXT("Sandbox"), TEXT("Core")), InOuter->GetName());

    if (!Result)
    {
        Result = new ULinkerLoad(InOuter, NewFilename, LoadFlags);
        if (Result && !(LoadFlags & LOAD_Verify) && (Result->Summary.PackageFlags & PKG_NeedsVerify))
            Result->Verify();
    }

    if (IsGuidCheckedPackage(InOuter) && CompatibleGuid && Result->Summary.Guid != *CompatibleGuid)
        appThrowf(LocalizeError(TEXT("PackageVersion"), TEXT("Core")), InOuter->GetName());
    return Result;
}



