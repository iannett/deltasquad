#include <alloca.h>
#include <cstring>

#include "Core.h"

TArray<FNameEntry*> FName::Names;
TArray<INT>         FName::Available;
FNameEntry*         FName::NameHash[NAME_HASH_SIZE];
INT                 FName::MaxNamesUsed;
UBOOL               FName::Initialized;

// 0x000925d3
FNameEntry* AllocateNameEntry(const TCHAR* Name, INT Index, DWORD Flags, FNameEntry* HashNext)
{
    // only allocate as much of Name[] as the string needs
    const size_t Size = offsetof(FNameEntry, Name) + appStrlen(Name) + 1;
    FNameEntry* Entry = static_cast<FNameEntry*>(appMalloc(Size));

    Entry->Index = Index;
    Entry->Flags = Flags;
    if (Flags & RF_Native)
        Entry->Flags |= NF_CaseDefined;
    Entry->HashNext = HashNext;
    appStrcpy(Entry->Name, Name);
    return Entry;
}

// 0x0009263c
FArchive& operator<<(FArchive& Ar, FNameEntry& Entry)
{
    if (Ar.Ver() < 64)
    {
        // old packages store a bare null terminated string
        INT Length = 0;
        BYTE Ch;
        do
        {
            Ar << Ch;
            Entry.Name[Length++] = Ch;
        } while (Ch);
    }
    else if (!Ar.IsLoading())
    {
        INT Length = appStrlen(Entry.Name) + 1;
        Ar << AR_INDEX(Length);
        Ar.Serialize(Entry.Name, Length);
    }
    else
    {
        INT Length;
        Ar << AR_INDEX(Length);
        if (Length <= NAME_SIZE)
        {
            Ar.Serialize(Entry.Name, Length);
        }
        else
        {
            // too long to fit, keep the first NAME_SIZE - 1 characters
            TCHAR* Long = static_cast<TCHAR*>(alloca(Length));
            Ar.Serialize(Long, Length);
            memcpy(Entry.Name, Long, NAME_SIZE - 1);
            Entry.Name[NAME_SIZE - 1] = 0;
        }
    }
    Ar << Entry.Flags;
    return Ar;
}

FName::FName(EName Index)
    : Entry(Index == NAME_None ? nullptr : Names[Index])
{
}

const TCHAR* FName::operator*() const
{
    return Entry ? Entry->Name : Names[NAME_None]->Name;
}

// 0x00092771
FName::FName(const TCHAR* Name, EFindName FindType)
{
    if (!Initialized)
        appErrorf(TEXT("FName '%s' constructed before FName::StaticInit"), Name);

    if (!*Name)
    {
        Entry = nullptr;
        return;
    }

    DWORD NewFlags = 0;
    if (FindType == FNAME_Intrinsic)
        NewFlags = RF_Native | NF_CaseDefined;
    else if (FindType == FNAME_Defined)
        NewFlags = NF_CaseDefined;

    FNameEntry** Bucket = &NameHash[appStrihash(Name) & (NAME_HASH_SIZE - 1)];
    for (FNameEntry* Hash = *Bucket; Hash; Hash = Hash->HashNext)
    {
        if (appStricmp(Name, Hash->Name) != 0)
            continue;

        // index 0 is NAME_None, which is always stored as null
        Entry = Hash->Index ? Hash : nullptr;
        if (!Entry || !(NewFlags & NF_CaseDefined))
            return;

        // a defining add fixes the spelling, and two defining adds must agree on it
        if (appStrcmp(Entry->Name, Name) != 0)
        {
            if (Entry->Flags & NF_CaseDefined)
                appErrorf(TEXT("Mismatched case in name definitions: '%s' and '%s'"), Entry->Name, Name);
            appStrcpy(Entry->Name, Name);
        }
        Entry->Flags |= NewFlags;
        return;
    }

    if (FindType == FNAME_Find)
    {
        Entry = nullptr;
        return;
    }

    // reuse a freed slot if there is one
    INT Index;
    if (Available.Num())
    {
        Index = Available.Pop();
    }
    else
    {
        Index = Names.Add();
        Names[Index] = nullptr;
        if (MaxNamesUsed < Names.Num())
            MaxNamesUsed = Names.Num();
    }

    *Bucket = AllocateNameEntry(Name, Index, NewFlags, *Bucket);
    Names[Index] = *Bucket;
    Entry = *Bucket;
}

// 0x00092a17
void FName::Hardcode(FNameEntry* AutoName)
{
    FNameEntry** Bucket = &NameHash[appStrihash(AutoName->Name) & (NAME_HASH_SIZE - 1)];
    AutoName->HashNext = *Bucket;
    *Bucket = AutoName;

    while (Names.Num() <= AutoName->Index)
        Names.AddItem(nullptr);

    if (Names[AutoName->Index])
        appErrorf(TEXT("Hardcoded name %i was duplicated"), AutoName->Index);
    Names[AutoName->Index] = AutoName;
}

// 0x00092afc
void FName::StaticInit()
{
    Names.Empty(45000);
    for (FNameEntry*& Bucket : NameHash)
        Bucket = nullptr;

    #define REGISTER_NAME(num, name, flags) Hardcode(AllocateNameEntry(#name, num, RF_Native | (flags), nullptr));
    #include "UnNames.h"

    for (FNameEntry* Bucket : NameHash)
    {
        for (FNameEntry* Hash = Bucket; Hash; Hash = Hash->HashNext)
        {
            for (FNameEntry* Other = Hash->HashNext; Other; Other = Other->HashNext)
            {
                if (appStricmp(Hash->Name, Other->Name) == 0)
                    appErrorf(TEXT("Name '%s' was duplicated"), Hash->Name);
            }
        }
    }

    GLog->Logf(NAME_Init, TEXT("Name subsystem initialized"));
    Initialized = 1;
}

// 0x00092950. the xbox totals the table's memory with FArchiveCountMem, which asks its allocator; here the
// requested and reserved sizes are added up directly. entry 0 is never freed, as on the xbox.
void FName::StaticExit()
{
    DWORD Used = Names.Num() * sizeof(FNameEntry*) + Available.Num() * sizeof(INT);
    DWORD Reserved = Names.GetMax() * sizeof(FNameEntry*) + Available.GetMax() * sizeof(INT);
    for (INT i = 0; i < Names.Num(); i++)
    {
        if (Names[i])
        {
            const DWORD EntrySize = static_cast<DWORD>(offsetof(FNameEntry, Name) + appStrlen(Names[i]->Name) + 1);
            Used += EntrySize;
            Reserved += EntrySize;
        }
    }
    // the first number is the byte count, though the text calls it names
    GLog->Logf(NAME_Exit, TEXT("Name subsystem shut down; usage %d names (%d max), %d KB (%d KB alloc)"),
        Used, Names.Num(), Used >> 10, Reserved >> 10);

    for (INT i = 1; i < Names.Num(); i++)
    {
        if (Names[i])
            appFree(Names[i]);
    }
    Names.Empty();
    Available.Empty();
    Initialized = 0;
}

// 0x00092593
void FName::DisplayHash(FOutputDevice& Ar)
{
    INT UsedBins = 0;
    INT NameCount = 0;
    for (FNameEntry* Bucket : NameHash)
    {
        if (Bucket)
            UsedBins++;
        for (FNameEntry* Hash = Bucket; Hash; Hash = Hash->HashNext)
            NameCount++;
    }
    Ar.Logf(TEXT("Hash: %i names, %i/%i hash bins"), NameCount, UsedBins, NAME_HASH_SIZE);
}

// 0x00092621
INT FName::GetNumNames()
{
    return Names.Num() - Available.Num();
}

// 0x00094f45
void FName::DeleteEntry(INT Index)
{
    FNameEntry* Entry = Names[Index];
    FNameEntry** HashLink = &NameHash[appStrihash(Entry->Name) & (NAME_HASH_SIZE - 1)];
    while (*HashLink && *HashLink != Entry)
        HashLink = &(*HashLink)->HashNext;
    if (!*HashLink)
        appErrorf(TEXT("Unhashed name '%s'"), Entry->Name);
    *HashLink = (*HashLink)->HashNext;

    appFree(Entry);
    Names[Index] = nullptr;
    Available.AddItem(Index);
}
