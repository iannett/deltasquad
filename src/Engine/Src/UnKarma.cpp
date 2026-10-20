#include <cstdio>
#include <cstring>
#include <new>

#include "EngineClasses.h"
#include "Karma.h"

// 0x0008ee9e
FArchive& operator<<(FArchive& Ar, FKAggregateGeom& G)
{
    if (Ar.Ver() < 113)
    {
        // the sphere array is read twice; the second read replaces the first
        Ar << G.SphereElems << G.BoxElems << G.SphereElems << G.CylinderElems;
    }
    else
    {
        Ar << G.SphereElems << G.BoxElems << G.CylinderElems << G.ConvexElems;
        if (Ar.Ver() > 156)
            Ar << G.Unknown20;
    }
    return Ar;
}

// 0x0008ee27
void UKMeshProps::Serialize(FArchive& Ar)
{
    Super::Serialize(Ar);
    for (INT i = 0; i < 6; i++)
        Ar << Unknown28[i];
    Ar << Unknown40 << AggGeom;
}

// ---------------------------------------------------------------------------------------------- karma globals

namespace
{
    // memory counted against karma: blocks and bytes. the xbox adds each size to both size fields.
    struct FKarmaMemStat
    {
        INT Count;
        INT Size;
        INT Size2;

        void Add(void* Ptr, INT Sign)
        {
            const INT Bytes = static_cast<INT>(appMallocSize(Ptr));
            Size2 += Sign * Bytes;
            Count += Sign;
            Size += Sign * Bytes;
        }
    };

    // the game's karma state. most of it is not known yet.
    struct KarmaGlobals
    {
        // 0x000845e4
        KarmaGlobals() : Unknown15058(0), Unknown1505C(0)
        {
            MemStat = { 0, 0, 0 };
            MemStat2 = { 0, 0, 0 };
        }

        BYTE          Unknown0[0x1500c];
        McdFramework* Framework;
        MeAssetDB*      AssetDB;
        MeAssetFactory* AssetFactory;
        INT           Unknown15018[3];
        UBOOL         Unknown15024;
        UBOOL         Unknown15028;
        UBOOL         Unknown1502C;
        // counts karma memory in the second stat instead of the first
        UBOOL         UseMemStat2;
        INT           Unknown15034;
        INT           Unknown15038;
        FKarmaMemStat MemStat;
        FKarmaMemStat MemStat2;
        MeUnknown1C*  Unknown15054;
        INT           Unknown15058;
        INT           Unknown1505C;
    };

    // 0x004722a0
    KarmaGlobals* KGData;

    FKarmaMemStat& KarmaMemStat()
    {
        return KGData->UseMemStat2 ? KGData->MemStat2 : KGData->MemStat;
    }

    // ------------------------------------------------------------------------------------------ memory

    // 0x0007e6cf
    void* KMemCreate(INT Size)
    {
        void* Ptr = appMalloc(Size);
        KarmaMemStat().Add(Ptr, 1);
        return Ptr;
    }

    // 0x0007e717
    void* KMemCreateZeroed(INT Size)
    {
        void* Ptr = KMemCreate(Size);
        memset(Ptr, 0, Size);
        return Ptr;
    }

    // 0x00084136. rounds the size up to the alignment but does not align the block.
    void* KMemCreateAligned(INT Size, INT Alignment)
    {
        return KMemCreate((Size + Alignment - 1) & ~(Alignment - 1));
    }

    // 0x0007e741
    void KMemDestroy(void* Ptr)
    {
        KarmaMemStat().Add(Ptr, -1);
        appFree(Ptr);
    }

    // 0x0007e781. the xbox counts the new size from the old block; the port asks the new one.
    void* KMemResize(void* Ptr, INT Size)
    {
        if (!Ptr)
            return KMemCreate(Size);
        KarmaMemStat().Add(Ptr, -1);
        void* NewPtr = appRealloc(Ptr, Size);
        KarmaMemStat().Add(NewPtr, 1);
        return NewPtr;
    }

    // ------------------------------------------------------------------------------------------ pools

    // 0x0007e829. every struct is allocated on its own.
    void KPoolInit(MePool* Pool, INT MaxStructs, INT StructSize, INT Alignment)
    {
        Pool->PoolType = 2;
        Pool->Malloc.Used = 0;
        Pool->Malloc.MaxStructs = MaxStructs;
        Pool->Malloc.StructSize = StructSize;
        Pool->Malloc.Alignment = Alignment;
    }

    // 0x0007e84f
    void KPoolDestroy(MePool* Pool)
    {
        if (Pool->Malloc.Used)
            MeFatalError(0, "MePoolMallocDestroy(): %d structs still allocated", Pool->Malloc.Used);
    }

    // 0x0007e812
    void KPoolReset(MePool* Pool)
    {
        MeWarning(1, "%s\n", "MePoolMallocReset() not implemented");
    }

    // 0x0007e86d
    void* KPoolGetStruct(MePool* Pool)
    {
        void* Struct = Pool->Malloc.Alignment == 0 ? MeMemory.Create(Pool->Malloc.StructSize)
                                                    : MeMemory.CreateAligned(Pool->Malloc.StructSize, Pool->Malloc.Alignment);
        if (Struct)
            Pool->Malloc.Used++;
        return Struct;
    }

    // 0x0007e899
    void KPoolPutStruct(MePool* Pool, void* Struct)
    {
        if (Pool->Malloc.Used == 0)
            MeWarning(0, "%s\n", "MePoolMallocPutStruct: Putting structure back into pool 0x%08x with no allocated structs.", &Pool->Malloc.Used);
        if (Struct)
        {
            if (Pool->Malloc.Alignment == 0)
                MeMemory.Destroy(Struct);
            else
                MeMemory.DestroyAligned(Struct);
            if (Pool->Malloc.Used > 0)
                Pool->Malloc.Used--;
        }
    }

    // 0x0007e8e7
    INT KPoolGetUsed(MePool* Pool)
    {
        return Pool->Malloc.Used;
    }

    // 0x0007e8f1
    INT KPoolGetUnused(MePool* Pool)
    {
        return Pool->Malloc.MaxStructs - Pool->Malloc.Used;
    }

    // ------------------------------------------------------------------------------------------ messages

    // 0x0007e8fe. the xbox passes the text as the format string.
    void KMessageOutput(INT Level, const char* Text)
    {
        GLog->Log(Text);
    }

    void KFormatMessage(char* Text, INT Size, const char* Fmt, va_list Args)
    {
        strncpy(Text, "(Karma): ", Size);
        const size_t Prefix = strlen("(Karma): ");
        vsnprintf(Text + Prefix, Size - Prefix, Fmt, Args);
        const size_t Len = strlen(Text);
        if (Len && Text[Len - 1] == '\n')
            Text[Len - 1] = 0;
    }

    // 0x0007e912. every kind of message goes to the info output at the info level.
    void KMessageHandler(INT Level, const char* Fmt, va_list Args)
    {
        if (Level <= MeInfoLevel)
        {
            char Text[5000];
            KFormatMessage(Text, sizeof(Text), Fmt, Args);
            MeMessages.InfoOutput(Level, Text);
        }
    }

    // 0x0007e99e. also stops in the debugger, which on the xbox build is never attached.
    void KFatalHandler(INT Level, const char* Fmt, va_list Args)
    {
        if (Level <= MeInfoLevel)
        {
            char Text[5000];
            KFormatMessage(Text, sizeof(Text), Fmt, Args);
            MeMessages.InfoOutput(Level, Text);
        }
    }

    // 0x000920fa. loads every karma asset file in ..\KarmaData into a new asset database.
    void KCreateAssetDB(MeAssetDB** AssetDB, MeAssetFactory** AssetFactory)
    {
        GLog->Log(TEXT("(Karma): Creating MeAssetDB."));
        *AssetDB = MeAssetDBCreate();
        MeAssetDBXMLInput* Input = MeAssetDBXMLInputCreate(*AssetDB, nullptr);
        if (*AssetDB)
        {
            *AssetFactory = MeAssetFactoryCreate(KGData->Framework);
            TArray<FString> Files = GFileManager->FindFiles(TEXT("..\\KarmaData\\*.ka"), 1, 0);
            for (INT i = 0; i < Files.Num(); i++)
            {
                const FString Name(*Files[i]);
                GLog->Logf(TEXT("(Karma): Loading: %s"), *Name);
                const FString Copy(Name);
                char* FileName = static_cast<char*>(MeMemory.Create(0x200));
                strncpy(FileName, *Copy, 0x200);
                const FString Folder = FString(appBaseDir()) + TEXT("..\\KarmaData\\");
                // the xbox allows the file name plus 32 bytes, too short for the port's base directory
                char* Path = static_cast<char*>(MeMemory.Create(static_cast<INT>(strlen(*Folder) + strlen(FileName)) + 1));
                Path[0] = 0;
                strcat(Path, *Folder);
                strcat(Path, FileName);
                MeFile* File = static_cast<MeFile*>(MeMemory.Create(sizeof(MeFile)));
                File->Type = 0;
                // stdio on the port wants forward slashes
                for (char* Ch = Path; *Ch; Ch++)
                {
                    if (*Ch == '\\')
                        *Ch = '/';
                }
                File->Handle = fopen(Path, "r");
                if (!File->Handle)
                {
                    MeMemory.Destroy(File);
                }
                else
                {
                    File->Name = Path;
                    if (!MeAssetDBXMLInputRead(Input, File))
                        GLog->Log(TEXT("(Karma): Non-fatal error in MeFileLoad."));
                    MeFileDestroy(File);
                }
                MeMemory.Destroy(Path);
                MeMemory.Destroy(FileName);
            }
        }
        MeAssetDBXMLInputDestroy(Input);
        if (*AssetDB)
            GLog->Logf(TEXT("(Karma): Finished Creating MeAssetDB (%d Assets)."), MeAssetDBGetAssetCount(*AssetDB));
    }
}

// 0x00092356
void KInitGameKarma()
{
    if (KGData)
    {
        GLog->Log(TEXT("(Karma): KInitGameKarma: Already Initialised!"));
        return;
    }
    GLog->Log(TEXT("(Karma): Beginning Karma for game."));
    KGData = new(appMalloc(sizeof(KarmaGlobals))) KarmaGlobals;
    memset(KGData, 0, sizeof(KarmaGlobals));
    KGData->MemStat.Add(KGData, 1);
    KGData->Unknown15028 = 1;
    KGData->Unknown15024 = 1;
    KGData->Unknown1502C = 1;

    MeMemory = { KMemCreate, KMemCreateZeroed, KMemCreateAligned, KMemDestroy, KMemDestroy, KMemResize };
    MePools = { KPoolInit, KPoolDestroy, KPoolReset, KPoolGetStruct, KPoolPutStruct, KPoolGetUsed, KPoolGetUnused };
    MeSetInfoOutput(KMessageOutput);
    MeSetWarningOutput(KMessageOutput);
    MeSetFatalOutput(KMessageOutput);
    MeSetDebugOutput(KMessageOutput);
    MeSetInfoHandler(KMessageHandler);
    MeSetWarningHandler(KMessageHandler);
    MeSetFatalHandler(KFatalHandler);
    MeSetDebugHandler(KMessageHandler);

    McdFramework* Framework = McdInit(3, 2500, 1, 0.95f);
    KGData->Framework = Framework;
    McdPrimitivesRegisterTypes(Framework);
    McdConvexMeshRegisterType(Framework);
    McdAggregateRegisterType(Framework);
    McdNullRegisterType(Framework);
    McdPrimitivesRegisterInteractions(Framework);
    McdConvexMeshRegisterInteractions(Framework);
    McdAggregateRegisterInteractions(Framework);
    KGData->Unknown15054 = MeUnknown1CCreate(1024);
    KGData->Unknown15034 = 0;
    KGData->Unknown15038 = 0;
    KCreateAssetDB(&KGData->AssetDB, &KGData->AssetFactory);
}
