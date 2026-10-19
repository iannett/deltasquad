#pragma once

#include "CoreTypes.h"
#include "UnTemplate.h"

class FArchive;
class FOutputDevice;

// longest name including the terminator
enum { NAME_SIZE = 64 };

// size of the name hash table, must be a power of two
enum { NAME_HASH_SIZE = 4096 };

// every name hardcoded into the engine
enum EName : INT
{
    #define REGISTER_NAME(num, name, flags) NAME_##name = num,
    #include "UnNames.h"
};

// name flags, stored in FNameEntry::Flags alongside object flags (RF_Native, RF_HighlightedName)
enum ENameFlags : DWORD
{
    // the stored spelling is the official one; adding the same name with different case is an error
    NF_CaseDefined = 0x00000080,
    // on log event names: messages of this event are dropped (System.ini [Core.System] Suppress=)
    NF_Suppress    = 0x00001000,
};

// how FName's string constructor treats a name that doesn't exist yet
enum EFindName
{
    FNAME_Find,      // return NAME_None
    FNAME_Add,       // add it
    FNAME_Intrinsic, // add it as a native name with a fixed spelling
    FNAME_Defined,   // add it with a fixed spelling
};

// one entry in the global name table. allocated with only as many Name bytes as the string needs.
struct FNameEntry
{
    INT         Index;
    DWORD       Flags;
    FNameEntry* HashNext;
    TCHAR       Name[NAME_SIZE];
};

// a case-insensitive string interned in the global name table. holds a pointer to its entry, NAME_None is null.
class FName
{
public:
    // like the engine, a default FName is left uninitialized so it can live inside objects whose
    // properties are set before their constructor runs. use FName(NAME_None) for an empty name.
    FName() = default;
    FName(EName Index);
    FName(const TCHAR* Name, EFindName FindType = FNAME_Add);

    INT GetIndex() const { return Entry ? Entry->Index : NAME_None; }
    const TCHAR* operator*() const;
    DWORD GetFlags() const { return Entry ? Entry->Flags : 0; }

    UBOOL operator==(const FName& Other) const { return Entry == Other.Entry; }
    UBOOL operator!=(const FName& Other) const { return Entry != Other.Entry; }
    UBOOL operator==(EName Other) const { return GetIndex() == Other; }
    UBOOL operator!=(EName Other) const { return GetIndex() != Other; }

    static void StaticInit();
    static void StaticExit();
    static void DisplayHash(FOutputDevice& Ar);
    static void Hardcode(FNameEntry* Entry);
    static void DeleteEntry(INT Index);

    static INT GetMaxNames() { return Names.Num(); }
    static INT GetNumNames();
    static FNameEntry* GetEntry(INT Index) { return Names[Index]; }
    static UBOOL GetInitialized() { return Initialized; }
    // 0x00035b25
    static UBOOL SafeSuppressed(EName Event) { return Initialized && (Names[Event]->Flags & NF_Suppress); }

private:
    FNameEntry* Entry;

    static TArray<FNameEntry*> Names;
    static TArray<INT>         Available;
    static FNameEntry*         NameHash[NAME_HASH_SIZE];
    static INT                 MaxNamesUsed;
    static UBOOL               Initialized;
};

inline DWORD GetTypeHash(const FName& N) { return static_cast<DWORD>(N.GetIndex()); }


FNameEntry* AllocateNameEntry(const TCHAR* Name, INT Index, DWORD Flags, FNameEntry* HashNext);
FArchive& operator<<(FArchive& Ar, FNameEntry& Entry);
