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
        INT           Unknown15018;
        // the length of the current karma step
        FLOAT         TimeStep;
        // debug drawing flags toggled by KDRAW
        DWORD         Unknown15020;
        // steps the world this tick. toggled by KSTEP, set by each tick that is not stopped and cleared after it.
        UBOOL         Unknown15024;
        // toggled by KSTOP; each karma tick sets it to whether the level is not players only
        UBOOL         Unknown15028;
        // set by KSAFETIME
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

// ---------------------------------------------------------------------------------------------- level karma

// the level's asset factory and the callbacks it builds karma objects with (name invented)
struct KLevelFactory
{
    MeAssetFactory* Factory;
    INT             Unknown4;
    INT             Unknown8;
    McdModel*       (*CreateModel)(void* Unknown1, void* Unknown2, void* Unknown3, void* Unknown4);
    INT             Unknown10;
    INT             Unknown14;
    INT             (*Unknown18)(void* Unknown1, void* Unknown2, void* Unknown3, void* Unknown4, void* Unknown5);
    INT             Unknown1C;
    INT             Unknown20;
};

namespace
{
    // 0x00431130. the [Karma] settings of System.ini, read again when each level starts.
    struct KarmaSettings
    {
        FLOAT UnitLength           = 0.95f;
        FLOAT UnitMass             = 0.95f;
        FLOAT GammaPerSec          = 6.f;
        FLOAT GammaMin             = 0.1f;
        FLOAT GammaMax             = 0.5f;
        FLOAT Epsilon              = 0.001f;
        FLOAT PenOffset            = 0.015f;
        FLOAT PenScale             = 1.f;
        FLOAT ContactSoft          = 0.01f;
        FLOAT FrictionScale        = 0.5f;
        FLOAT MaxTimestep          = 0.04f;
        FLOAT DoDoubleTickTimestep = 0.025f;
        FLOAT MaxPenetration       = 0.1f;
    } KSettings;

    // what the level model's user data points at (name invented)
    struct KModelData
    {
        INT Unknown0;
        INT Unknown4;
        INT Unknown8;
        INT UnknownC;
        INT Unknown10;
    };

    // 0x0008bc6b
    void KBodyEnableCB(void* Body)
    {
        NOT_DECOMPILED(0x0008bc6b);
    }

    // 0x0008bc28
    void KBodyDisableCB(void* Body)
    {
        NOT_DECOMPILED(0x0008bc28);
    }

    // 0x0007d4c6
    void KBodyUnknownCB(void* Body)
    {
        NOT_DECOMPILED(0x0007d4c6);
    }

    // 0x00082f5b
    McdModel* KFactoryCreateModel(void* Unknown1, void* Unknown2, void* Unknown3, void* Unknown4)
    {
        NOT_DECOMPILED(0x00082f5b);
        return nullptr;
    }

    // 0x00196f30
    INT KFactoryUnknown18(void* Unknown1, void* Unknown2, void* Unknown3, void* Unknown4, void* Unknown5)
    {
        NOT_DECOMPILED(0x00196f30);
        return 0;
    }

    // 0x0008ec55. hands the level's triangles near a model to the collision.
    INT KTriListGenerator(void* Pair, void* Triangles, FLOAT* Position, FLOAT Radius, INT MaxTriangles)
    {
        NOT_DECOMPILED(0x0008ec55);
        return 0;
    }

    // 0x00086311
    INT KIntersectCB(void* Unknown1, void* Unknown2, void* Unknown3)
    {
        NOT_DECOMPILED(0x00086311);
        return 0;
    }

    // 0x000b3c92, which the xbox shares with UMapProperty::Identical. keeps every contact.
    INT KPerContactCB(void* Unknown1, void* Unknown2)
    {
        return 1;
    }
}

// 0x0008f45a. makes the level's dynamics world, its collision bridge and a model for the level's own triangles.
void KInitLevelKarma(ULevel* Level)
{
    if (!KGData || Level->KarmaWorld || !KGData->Framework)
        return;
    GLog->Log(TEXT("(Karma): Initialising Karma for Level."));
    GConfig->GetFloat(TEXT("Karma"), TEXT("ME_UNIT_LENGTH"), KSettings.UnitLength, TEXT("System.ini"));
    GConfig->GetFloat(TEXT("Karma"), TEXT("ME_UNIT_MASS"), KSettings.UnitMass, TEXT("System.ini"));
    GConfig->GetFloat(TEXT("Karma"), TEXT("ME_GAMMA_PERSEC"), KSettings.GammaPerSec, TEXT("System.ini"));
    GConfig->GetFloat(TEXT("Karma"), TEXT("ME_GAMMA_MIN"), KSettings.GammaMin, TEXT("System.ini"));
    GConfig->GetFloat(TEXT("Karma"), TEXT("ME_GAMMA_MAX"), KSettings.GammaMax, TEXT("System.ini"));
    GConfig->GetFloat(TEXT("Karma"), TEXT("ME_EPSILON"), KSettings.Epsilon, TEXT("System.ini"));
    GConfig->GetFloat(TEXT("Karma"), TEXT("ME_PENOFFSET"), KSettings.PenOffset, TEXT("System.ini"));
    GConfig->GetFloat(TEXT("Karma"), TEXT("ME_PENSCALE"), KSettings.PenScale, TEXT("System.ini"));
    GConfig->GetFloat(TEXT("Karma"), TEXT("ME_CONTACTSOFT"), KSettings.ContactSoft, TEXT("System.ini"));
    GConfig->GetFloat(TEXT("Karma"), TEXT("ME_FRICTIONSCALE"), KSettings.FrictionScale, TEXT("System.ini"));
    GConfig->GetFloat(TEXT("Karma"), TEXT("ME_MAX_TIMESTEP"), KSettings.MaxTimestep, TEXT("System.ini"));
    GConfig->GetFloat(TEXT("Karma"), TEXT("ME_DO_DOUBLETICK_TIMESTEP"), KSettings.DoDoubleTickTimestep, TEXT("System.ini"));
    GConfig->GetFloat(TEXT("Karma"), TEXT("ME_MAX_PENETRATION"), KSettings.MaxPenetration, TEXT("System.ini"));

    MdtWorld* World = MdtWorldCreate(1, 1, KSettings.UnitLength, KSettings.UnitMass);
    Level->KarmaWorld = World;
    MdtWorldSetUnknown22C(World, 1);
    MdtWorldSetAutoDisable(Level->KarmaWorld, 1);
    MdtWorldSetEpsilon(Level->KarmaWorld, KSettings.Epsilon);
    FLOAT LinearVelocity = 0.02f;
    FLOAT LinearAcceleration = 0.05f;
    FLOAT AngularVelocity = 0.001f;
    FLOAT AngularAcceleration = 0.05f;
    FLOAT AliveTime = 1.f;
    GConfig->GetFloat(TEXT("Karma"), TEXT("AutoDisableLinearVelocityThreshold"), LinearVelocity, TEXT("System.ini"));
    GConfig->GetFloat(TEXT("Karma"), TEXT("AutoDisableLinearAccelerationThreshold"), LinearAcceleration, TEXT("System.ini"));
    GConfig->GetFloat(TEXT("Karma"), TEXT("AutoDisableAngularVelocityThreshold"), AngularVelocity, TEXT("System.ini"));
    GConfig->GetFloat(TEXT("Karma"), TEXT("AutoDisableAngularAccelerationThreshold"), AngularAcceleration, TEXT("System.ini"));
    GConfig->GetFloat(TEXT("Karma"), TEXT("AutoDisableAliveTime"), AliveTime, TEXT("System.ini"));
    MdtWorldSetAutoDisableVelocityThreshold(Level->KarmaWorld, LinearVelocity);
    MdtWorldSetAutoDisableAccelerationThreshold(Level->KarmaWorld, LinearAcceleration);
    MdtWorldSetAutoDisableAngularVelocityThreshold(Level->KarmaWorld, AngularVelocity);
    MdtWorldSetAutoDisableAngularAccelerationThreshold(Level->KarmaWorld, AngularAcceleration);
    MdtWorldSetAutoDisableAliveWindow(Level->KarmaWorld, AliveTime);
    Level->KarmaWorld->BodyDisableCB = KBodyDisableCB;
    Level->KarmaWorld->BodyEnableCB = KBodyEnableCB;
    Level->KarmaWorld->Unknown1D4 = KBodyUnknownCB;

    Level->KarmaBridge = MstBridgeCreate(KGData->Framework, 2);
    MstBridgeSetUnknown0C(Level->KarmaBridge, 0x400);

    Level->KarmaFactory = static_cast<KLevelFactory*>(appMalloc(sizeof(KLevelFactory)));
    Level->KarmaFactory->Factory = KGData->AssetFactory;
    Level->KarmaFactory->Unknown4 = 0;
    Level->KarmaFactory->Unknown8 = 0;
    Level->KarmaFactory->CreateModel = KFactoryCreateModel;
    Level->KarmaFactory->Unknown10 = 0;
    Level->KarmaFactory->Unknown14 = 0;
    Level->KarmaFactory->Unknown18 = KFactoryUnknown18;
    Level->KarmaFactory->Unknown1C = 0;
    Level->KarmaFactory->Unknown20 = 0;

    // the level model sits at the origin and its list covers everything
    FLOAT* TM = static_cast<FLOAT*>(MeMemory.CreateAligned(16 * sizeof(FLOAT), 0x10));
    for (INT i = 0; i < 16; i++)
        TM[i] = i % 5 == 0 ? 1.f : 0.f;
    const FLOAT Max[3] = { 524288.f, 524288.f, 524288.f };
    const FLOAT Min[3] = { -524288.f, -524288.f, -524288.f };
    McdTriangleList* TriList = McdTriangleListCreate(KGData->Framework, Min, Max, 0x400, KTriListGenerator);
    KGData->Unknown15038++;
    McdTriangleListSetUserData(TriList, Level);
    Level->KarmaLevelModel = McdModelCreate(&TriList->Base);
    KGData->Unknown15034++;
    McdModelSetTransformPtr(Level->KarmaLevelModel, TM);
    McdModelUpdate(Level->KarmaLevelModel);
    KModelData* Data = static_cast<KModelData*>(KMemCreate(sizeof(KModelData)));
    Data->Unknown4 = 0;
    Data->Unknown8 = 0;
    Data->UnknownC = 0;
    Data->Unknown10 = 0;
    Data->Unknown0 = 0;
    McdModelSetUserData(Level->KarmaLevelModel, Data);

    INT* Request = McdFrameworkGetDefaultRequestPtr(KGData->Framework);
    Request[1] = 1;
    Request[0] = 10;
    MstBridgeSetIntersectCB(Level->KarmaBridge, 0, 0, KIntersectCB);
    MstBridgeSetPerContactCB(Level->KarmaBridge, 0, 0, KPerContactCB);
    MdtContactParams* Params = MstBridgeGetContactParams(Level->KarmaBridge, 0, 0);
    MdtContactParamsSetType(Params, 2);
    MdtContactParamsSetFrictionModel(Params, 1);
    MdtContactParamsSetPrimarySlip(Params, 0.5f);
    MdtContactParamsSetSoftness(Params, KSettings.ContactSoft);
    MdtContactParamsSetFriction(Params, 10.f);
}

namespace
{
    // 0x000905e9. refreshes the triangles a ragdoll collides with; the name is from its log lines.
    void KUpdateRagdollTrilist(AActor* Actor)
    {
        NOT_DECOMPILED(0x000905e9);
    }

    // 0x0008fa66. collides the listed actors with each other and the level (name invented).
    void KUpdateContacts(TArray<AActor*>& Actors, ULevel* Level)
    {
        if (!Level->Hash)
            return;
        FMemMark Mark(GMem);
        // the xbox also builds a TMap here that only the actor loop uses
        MeUnknown1CReset(KGData->Unknown15054);
        if (Actors.Num())
            NOT_DECOMPILED(0x0008fac3);
        Mark.Pop();
        if (MeUnknown1CCount14(KGData->Unknown15054) + MeUnknown1CCount10(KGData->Unknown15054) > 0)
            NOT_DECOMPILED(0x00084631);
    }

    // 0x00081efa. called after each partition is built; Karma's MdtAutoDisableLastPartition does the same.
    void KAutoDisableLastPartition(MdtPartitionOutput* Output, void* Params)
    {
        NOT_DECOMPILED(0x00081efa);
    }

    // 0x0007d90a. splits the enabled bodies into partitions of bodies joined by constraints; the name is from its
    // error messages. the xbox passes Output in ESI.
    void KBuildPartitions(MeDict* Bodies, void (*EndCB)(MdtPartitionOutput* Output, void* Params), void* Params,
        MdtPartitionOutput* Output)
    {
        Output->NumPartitions = 0;
        Output->TotalBodies = 0;
        Output->TotalConstraints = 0;
        Output->Unknown38 = 0;
        Output->Unknown3C = 0;
        Output->Unknown34 = 0;
        // 0x0007d8e9 skips bodies that are already in a partition
        if (MeDictFirst(Bodies))
            NOT_DECOMPILED(0x0007d970);
    }

    // 0x00089d21. steps the world's dynamics by TimeStep (name invented).
    void KWorldStep(MdtWorld* World, FLOAT TimeStep, ULevel* Level)
    {
        MdtPartitionOutput* Partitions = MdtPartitionOutputCreateInChunk(&World->Unknown14C, World->UnknownB8, World->UnknownF8);
        KBuildPartitions(&World->EnabledBodies, KAutoDisableLastPartition, &World->AutoDisable, Partitions);
        // each partition's step, and the postKarmaStep and DestroyActor passes over the actors it collects
        if (Partitions->NumPartitions > 0)
            NOT_DECOMPILED(0x00089d88);
        MeChunkPutMem(&World->Unknown14C, Partitions);
        // clears each enabled body's forces
        if (MeDictFirst(&World->EnabledBodies))
            NOT_DECOMPILED(0x0008a4c5);
    }
}

// 0x00090a71. steps the level's karma world, twice when the frame is long.
void KTickLevelKarma(ULevel* Level, FLOAT DeltaSeconds)
{
    if (!KGData || !Level->KarmaWorld)
        return;

    INT Steps = 1;
    if (Level->GetLevelInfo())
        KGData->Unknown15028 = !Level->GetLevelInfo()->bPlayersOnly;
    FLOAT Time = 0.03f;
    if (KGData->Unknown15028)
    {
        KGData->Unknown15024 = 1;
        Time = Level->GetLevelInfo()->KarmaTimeScale * DeltaSeconds;
    }
    if (KSettings.DoDoubleTickTimestep <= Time)
        Steps = 2;

    const FLOAT StepTime = Time / Steps;
    for (; Steps; Steps--)
    {
        const FLOAT TimeStep = KSettings.MaxTimestep >= StepTime ? StepTime : KSettings.MaxTimestep;
        const FLOAT Gamma = KSettings.GammaPerSec * TimeStep;
        const FLOAT Capped = KSettings.GammaMax > Gamma ? Gamma : KSettings.GammaMax;
        MdtWorldSetGamma(Level->KarmaWorld, KSettings.GammaMin > Capped ? KSettings.GammaMin : Capped);
        KGData->TimeStep = TimeStep;
        for (INT i = 0; i < Level->Unknown1138.Num(); i++)
            KUpdateRagdollTrilist(Level->Unknown1138[i]);
        KUpdateContacts(Level->Unknown1148, Level);
        if (KGData->Unknown15024)
            KWorldStep(Level->KarmaWorld, TimeStep, Level);
    }
}

// ULevel::Tick clears this inline once its actors have ticked
void KEndLevelTick()
{
    if (KGData)
        KGData->Unknown15024 = 0;
}

// 0x0007ebd9. the karma debugging commands; the real name is not known.
UBOOL KExec(const TCHAR* Cmd, FOutputDevice& Ar)
{
    if (ParseCommand(&Cmd, TEXT("KDRAW")))
    {
        if (!KGData)
        {
            Ar.Log(TEXT("Cannot execute Karma debugging options (No KGData)."));
            return 0;
        }
        if (ParseCommand(&Cmd, TEXT("COLLISION")))
            KGData->Unknown15020 ^= 0x1;
        else if (ParseCommand(&Cmd, TEXT("CONTACTS")))
            KGData->Unknown15020 ^= 0x100;
        else if (ParseCommand(&Cmd, TEXT("JOINTS")))
            KGData->Unknown15020 ^= 0x30;
        else if (ParseCommand(&Cmd, TEXT("TRIANGLES")))
            KGData->Unknown15020 ^= 0x200;
        else if (ParseCommand(&Cmd, TEXT("COM")))
            KGData->Unknown15020 ^= 0x2;
        else if (ParseCommand(&Cmd, TEXT("ORIGIN")))
            KGData->Unknown15020 ^= 0x4;
        else if (ParseCommand(&Cmd, TEXT("WHEEL")))
            KGData->Unknown15020 ^= 0x400;
        else if (ParseCommand(&Cmd, TEXT("SAFETIME")))
            KGData->Unknown15020 ^= 0x800;
        else if (ParseCommand(&Cmd, TEXT("REPULSORS")))
            KGData->Unknown15020 ^= 0x1000;
        else
            return 0;
        Ar.Log(TEXT("Karma debugging option recognized"));
        return 1;
    }
    if (ParseCommand(&Cmd, TEXT("KSTEP")))
    {
        if (!KGData)
        {
            Ar.Log(TEXT("Cannot execute Karma debugging options (No KGData)."));
            return 0;
        }
        KGData->Unknown15024 = !KGData->Unknown15024;
        return 1;
    }
    if (ParseCommand(&Cmd, TEXT("KSTOP")))
    {
        if (!KGData)
        {
            Ar.Log(TEXT("Cannot execute Karma debugging options (No KGData)."));
            return 0;
        }
        KGData->Unknown15028 = !KGData->Unknown15028;
        return 1;
    }
    if (ParseCommand(&Cmd, TEXT("KSAFETIME")))
    {
        if (!KGData)
        {
            Ar.Log(TEXT("Cannot execute Karma debugging options (No KGData)."));
            return 0;
        }
        if (ParseCommand(&Cmd, TEXT("0")))
            KGData->Unknown1502C = 0;
        else if (ParseCommand(&Cmd, TEXT("1")))
            KGData->Unknown1502C = 1;
        Ar.Log(KGData->Unknown1502C ? TEXT("Karma Safe-Time Enabled.") : TEXT("Karma Safe-Time Disabled."));
        return 1;
    }
    return 0;
}

// 0x0008946f. name invented. adds the actor to its level's Unknown1148 list once, or takes it out.
void KSetActorListed(AActor* Actor, UBOOL bListed)
{
    TArray<AActor*>& List = Actor->XLevel->Unknown1148;
    if (!bListed)
    {
        List.RemoveItem(Actor);
        return;
    }
    INT Index = 0;
    if (!List.FindItem(Actor, Index))
        List.AddItem(Actor);
}

// 0x0008ca26. builds the ragdoll bodies of an actor whose karma params are UKarmaParamsSkel, once.
void KInitSkeletonKarma(USkeletalMeshInstance* Instance)
{
    if (!KGData)
        return;
    AActor* Actor = Instance->GetActor();
    if (!Actor || !Actor->XLevel)
        return;
    if (Cast<UKarmaParamsSkel>(Actor->KParams))
        NOT_DECOMPILED(0x0008ca26);
}

// 0x00091192. sets up the karma parts of an actor in a level with karma.
void KInitActorKarma(AActor* Actor)
{
    if (!KGData || Actor->bDeleteMe || !Actor->XLevel || !KGData->Framework)
        return;
    // physics 14 is ragdoll, 13 is karma
    if (Actor->Physics == 14)
    {
        if (Actor->Mesh)
            NOT_DECOMPILED(0x0008ce3c);
        return;
    }
    if (AKConstraint* Constraint = Cast<AKConstraint>(Actor))
    {
        if (!Constraint->KConstraintActor1 && !Constraint->KConstraintActor2)
            Constraint->Physics = 0;
        else if (Constraint->Physics == 13)
            NOT_DECOMPILED(0x000912a5);
        return;
    }
    if (Actor->bBlockKarma)
        NOT_DECOMPILED(0x0008c235);
    if (Actor->Physics == 13)
        NOT_DECOMPILED(0x00090c0f);
}
