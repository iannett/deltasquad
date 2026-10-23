#include "EngineClasses.h"
#include "UnEngineGlobals.h"
#include "Karma.h"

void KInitActorKarma(AActor* Actor);
void KInitLevelKarma(ULevel* Level);
void KTickLevelKarma(ULevel* Level, FLOAT DeltaSeconds);
void KEndLevelTick();
UBOOL KExec(const TCHAR* Cmd, FOutputDevice& Ar);

// 0x000a975c
FArchive& operator<<(FArchive& Ar, FURL& U)
{
    Ar << U.Protocol << U.Host << U.Map << U.Portal << U.Op;
    Ar.Serialize(&U.Port, sizeof(U.Port));
    DWORD Valid = U.Valid;
    Ar.Serialize(&Valid, sizeof(Valid));
    U.Valid = Valid & 1;
    return Ar;
}

/*-----------------------------------------------------------------------------
    ULevelBase.
-----------------------------------------------------------------------------*/

ULevelBase::ULevelBase()
    : URL(nullptr)
{
    Actors.Owner = this;
    Unknown2C = nullptr;
}

void ULevelBase::Destroy()
{
    if (NetDriver)
    {
        delete NetDriver;
        NetDriver = nullptr;
    }
    if (DemoRecDriver)
    {
        delete DemoRecDriver;
        DemoRecDriver = nullptr;
    }
    UObject::Destroy();
}

void ULevelBase::Serialize(FArchive& Ar)
{
    UObject::Serialize(Ar);
    if (Ar.Ver() < 0x99)
    {
        if (!Ar.IsLoading())
            goto Actors;
        Unknown2C = nullptr;
    }
    else
    {
        Ar << Unknown2C;
        if (!Ar.IsLoading())
            goto Actors;
        if (Unknown2C && Unknown2C->GetFName() == FName(TEXT("myLevel"), FNAME_Add))
            Unknown2C = nullptr;
    }
    if (Ar.IsLoading() && !Unknown2C)
        Unknown2C = GetOuter();
Actors:
    if (Ar.Ver() < 0x8b)
    {
        // old packages store the array raw: its count twice, then the elements
        if (!Ar.ArSkipObjectRefs)
        {
            INT Num = Actors.Num();
            INT Max = Num;
            Actors.CountBytes(Ar);
            Ar.Serialize(&Num, sizeof(Num));
            Ar.Serialize(&Max, sizeof(Max));
            if (Ar.IsLoading())
            {
                Actors.Empty(Num);
                Actors.AddZeroed(Num);
            }
            for (INT i = 0; i < Actors.Num(); i++)
                Ar << Actors[i];
        }
    }
    else if (!Ar.ArSkipObjectRefs)
    {
        Ar << static_cast<TArray<AActor*>&>(Actors);
    }
    Ar << URL;
    if (!Ar.IsLoading() && !Ar.IsSaving())
    {
        Ar << NetDriver;
        Ar << DemoRecDriver;
    }
}

void ULevelBase::NotifyProgress(const TCHAR* Str1, const TCHAR* Str2, FLOAT Seconds)
{
    Engine->SetProgress(Str1, Str2, Seconds);
}

/*-----------------------------------------------------------------------------
    ULevel.
-----------------------------------------------------------------------------*/

ULevel::ULevel()
{
    Unknown1118 = Unknown111C = Unknown1120 = Unknown1124 = 0;
}

void ULevel::Destroy()
{
    if (Hash)
    {
        delete Hash;
        Hash = nullptr;
    }
    if (Unknown1108.Num())
        NOT_DECOMPILED(0x000a6475);
    if (KarmaWorld && KarmaBridge)
        NOT_DECOMPILED(0x0009085e);
    GRenderUnknown.Reset();
    ULevelBase::Destroy();
}

void ULevel::PostLoad()
{
    UObject::PostLoad();
    for (INT i = 0; i < GObjObjects.Num(); i++)
    {
        AActor* Actor = Cast<AActor>(GObjObjects[i]);
        if (!Actor || Actor->GetOuter() != GetOuter())
            continue;
        APlayerController* PC = Cast<APlayerController>(Actor);
        if (!PC || !(PC->ShowFlags & 0x100))
            Actor->XLevel = this;
    }
    if (!GetLevelInfo()->bKNoInit)
        KInitLevelKarma(this);
    Model->BuildRenderData();
}

void ULevel::Serialize(FArchive& Ar)
{
    ULevelBase::Serialize(Ar);
    // the level info's time is written but ignored when read
    FLOAT TimeSeconds = 0.f;
    if (Actors.Num())
        TimeSeconds = GetLevelInfo()->TimeSeconds;
    Ar << Model;
    Ar.Serialize(&TimeSeconds, sizeof(TimeSeconds));
    Ar << FirstDeleted;
    for (INT i = 0; i < 16; i++)
        Ar << TextBlocks[i];
    if (Ar.Ver() > 0x3e)
    {
        Ar << TravelInfo;
    }
    else if (Ar.Ver() >= 0x3d)
    {
        TArray<FString> Keys, Values;
        Ar << Keys << Values;
        TravelInfo = TMap<FString, FString>();
        for (INT i = 0; i < Keys.Num(); i++)
            TravelInfo.Set(Keys[i], Values[i]);
    }
    if (Model && !Ar.IsTrans())
        Ar.Preload(Model);
    if (Ar.IsCountingMem())
    {
        if (Hash)
            Hash->Serialize(Ar);
        Unknown10F0.CountBytes(Ar);
        Unknown10F8.CountBytes(Ar);
        for (INT i = 0; i < Unknown10F8.Num(); i++)
            Unknown10F8[i].CountBytes(Ar);
        Unknown1100.CountBytes(Ar);
        for (INT i = 0; i < Unknown1100.Num(); i++)
            Unknown1100[i].CountBytes(Ar);
        Unknown1108.CountBytes(Ar);
        Unknown1110.CountBytes(Ar);
    }
}

void ULevel::Modify(UBOOL DoTransArrays)
{
    UObject::Modify();
    Model->Modify();
}

void ULevel::SetActorCollision(UBOOL bCollision, UBOOL Unknown)
{
    if (bCollision)
    {
        if (Hash)
            return;
        Hash = GNewCollisionHash();
        for (INT i = 0; i < Actors.Num(); i++)
            if (Actors[i] && Actors[i]->bCollideActors)
                Hash->AddActor(Actors[i]);
        return;
    }
    if (!Hash)
        return;
    if (!Unknown)
    {
        for (INT i = 0; i < Actors.Num(); i++)
            if (Actors[i] && Actors[i]->bCollideActors)
                Hash->RemoveActor(Actors[i]);
    }
    delete Hash;
    Hash = nullptr;
}

// 0x00462110 and 0x00462114: the time and delta of the last level tick
FLOAT GUnknown462110;
FLOAT GUnknown462114;

void ULevel::Tick(ELevelTick TickType, FLOAT DeltaSeconds)
{
    ALevelInfo* Info = GetLevelInfo();
    FMemMark MemMark(GMem);
    FMemMark EngineMemMark(GEngineMem);
    InTick = 1;

    // shadows follow the client's setting
    if (GEngine->Client)
        NOT_DECOMPILED(0x00131ceb);
    else
        Info->ShadowsEnabled = 0;

    // receive what came in
    if (DemoRecDriver)
        NOT_DECOMPILED(0x00053d7b);
    if (NetDriver)
    {
        NetDriver->TickDispatch(DeltaSeconds);
        if (NetDriver->ServerConnection)
            TickNetClient(DeltaSeconds);
    }
    if (Hash)
        Hash->Tick();

    if (DeltaSeconds < 0.0005f || DeltaSeconds >= 0.4f)
        DeltaSeconds = DeltaSeconds < 0.0005f ? 0.0005f : 0.4f;
    if (!GIsBenchmarking)
        DeltaSeconds *= Info->TimeDilation;
    if (!IsPaused())
        Info->TimeSeconds += DeltaSeconds;
    GUnknown462110 = Info->TimeSeconds;
    GUnknown462114 = DeltaSeconds;
    UpdateTime(Info);

    if (Info->bPlayersOnly)
        TickType = LEVELTICK_ViewportsOnly;
    // a client whose connection isn't open yet only ticks like a paused level
    if (NetDriver && NetDriver->ServerConnection)
        NOT_DECOMPILED(0x0007d13c);

    if (TickType == LEVELTICK_TimeOnly || IsPaused())
    {
        // paused: player controllers with a viewport and actors marked bAlwaysTick still tick
        if (IsPaused())
        {
            NewlySpawned = nullptr;
            INT Updated = 1;
            for (INT i = iFirstDynamicActor; i < Actors.Num(); i++)
            {
                AActor* Actor = Actors[i];
                APlayerController* PC = Cast<APlayerController>(Actor);
                if (PC && Cast<UViewport>(PC->Player))
                {
                    NOT_DECOMPILED(0x00131fda);
                }
                else if (Actor)
                {
                    if (Actor->bAlwaysTick && !Actor->bDeleteMe)
                        Actor->Tick(DeltaSeconds, TickType);
                    else
                        Actor->bTicked = Ticked;
                }
            }
            while (NewlySpawned && Updated)
            {
                FActorLink* Link = NewlySpawned;
                NewlySpawned = nullptr;
                Updated = 0;
                for (; Link; Link = Link->Next)
                {
                    AActor* Actor = Link->Actor;
                    if (Actor->bTicked != static_cast<UBOOL>(Ticked) && !Actor->bDeleteMe)
                    {
                        if (Actor->bAlwaysTick)
                            Updated += Actor->Tick(DeltaSeconds, TickType);
                        else
                            Actor->bTicked = Ticked;
                    }
                }
            }
        }
    }
    else
    {
        // tick every dynamic actor once, then the actors they spawn
        NewlySpawned = nullptr;
        INT Updated = 1;
        KTickLevelKarma(this, DeltaSeconds);
        for (INT i = iFirstDynamicActor; i < Actors.Num(); i++)
        {
            AActor* Actor = Actors[i];
            if (Actor && !Actor->bDeleteMe && Actor->bTicked != static_cast<UBOOL>(Actor->XLevel->Ticked))
                Updated += Actor->Tick(DeltaSeconds, TickType);
        }
        while (NewlySpawned && Updated)
        {
            FActorLink* Link = NewlySpawned;
            NewlySpawned = nullptr;
            Updated = 0;
            for (; Link; Link = Link->Next)
            {
                AActor* Actor = Link->Actor;
                if (Actor && !Actor->bDeleteMe && Actor->bTicked != static_cast<UBOOL>(Actor->XLevel->Ticked))
                    Updated += Actor->Tick(DeltaSeconds, TickType);
            }
        }
        KEndLevelTick();
    }

    // send what is due
    if (NetDriver)
    {
        if (!NetDriver->ServerConnection)
            TickNetServer(DeltaSeconds);
        NetDriver->TickFlush();
    }
    if (DemoRecDriver)
        NOT_DECOMPILED(0x00131d06);

    InTick = 0;
    Ticked = !Ticked;
    MemMark.Pop();
    EngineMemMark.Pop();
    CleanupDestroyed(0);
}
void ULevel::TickNetClient(FLOAT DeltaSeconds) { NOT_DECOMPILED(0x001332cb); }
void ULevel::TickNetServer(FLOAT DeltaSeconds)
{
    // an actor whose replicated properties changed is marked for every open connection's channel
    ALevelInfo* Info = GetLevelInfo();
    if (Info && Info->bNetDirty)
    {
        for (INT i = NetDriver->ClientConnections.Num() - 1; i >= 0; i--)
            NOT_DECOMPILED(0x001319f7);
        Info->bNetDirty = 0;
    }
    for (INT iActor = iFirstNetRelevantActor; iActor < Actors.Num(); iActor++)
    {
        AActor* Actor = Actors[iActor];
        if (Actor && Actor->bNetDirty)
        {
            for (INT i = NetDriver->ClientConnections.Num() - 1; i >= 0; i--)
                NOT_DECOMPILED(0x001319f7);
            Actor->bNetDirty = 0;
        }
    }

    const INT Updated = ServerTickClients(DeltaSeconds);

    // the title line once a second
    Info = GetLevelInfo();
    if (static_cast<INT>(Info->TimeSeconds - DeltaSeconds) != static_cast<INT>(Info->TimeSeconds))
    {
        const INT NumPlayers = Info->Game ? Info->Game->NumPlayers : NetDriver->ClientConnections.Num();
        GLog->Logf(NAME_Title, LocalizeProgress(TEXT("RunningNet"), TEXT("Engine")), *Info->Title, *URL.Map, NumPlayers);
    }

    // the stat lines each open connection asked for
    if (Updated)
    {
        for (INT i = 0; i < NetDriver->ClientConnections.Num(); i++)
            NOT_DECOMPILED(0x001335d9);
    }
}
INT ULevel::ServerTickClients(FLOAT DeltaSeconds)
{
    // replicates the relevant actors to each client; returns how many clients were updated
    if (NetDriver->ClientConnections.Num() == 0)
        return 0;
    NOT_DECOMPILED(0x00131109);
    return 0;
}
void ULevel::ReconcileActors() { NOT_DECOMPILED(0x000a6b89); }
void ULevel::RememberActors() { NOT_DECOMPILED(0x000a5f39); }
UBOOL ULevel::Exec(const TCHAR* Cmd, FOutputDevice& Ar)
{
    const TCHAR* Str = Cmd;
    if (NetDriver && NetDriver->Exec(Cmd, Ar))
        return 1;
    if (DemoRecDriver && DemoRecDriver->Exec(Cmd, Ar))
        return 1;
    if (ParseCommand(&Cmd, TEXT("SHOWEXTENTLINECHECK")))
        Unknown1120 = !Unknown1120;
    else if (ParseCommand(&Cmd, TEXT("SHOWLINECHECK")))
        Unknown111C = !Unknown111C;
    else if (ParseCommand(&Cmd, TEXT("SHOWPOINTCHECK")))
        Unknown1124 = !Unknown1124;
    else if (ParseCommand(&Str, TEXT("DEBUGREGIONMATRIX")))
    {
        // the xbox tests the whole command for being empty, which it never is
        INT Index = 0;
        if (appStrcmp(Cmd, TEXT("")))
            Index = appAtoi(Str);
        UBitArray2D* Matrix = GetLevelInfo()->RegionConnection[Index];
        GLog->Logf(TEXT("Region Connection Matrix %d, Total regions: %d"), Index, Matrix->Unknown34);
        Matrix->DebugPrint();
    }
    else
    {
        return KExec(Cmd, Ar) != 0;
    }
    return 1;
}
void ULevel::ShrinkLevel() { NOT_DECOMPILED(0x000ab8a2); }
void ULevel::CompactActors()
{
    // packs the live dynamic actors down over the empty slots; one marked deleted but still listed is dropped
    INT c = iFirstDynamicActor;
    for (INT i = iFirstDynamicActor; i < Actors.Num(); i++)
    {
        AActor* Actor = Actors[i];
        if (!Actor)
            continue;
        if (Actor->bDeleteMe)
        {
            GLog->Logf(TEXT("Undeleted %s"), Actor->GetFullName());
            continue;
        }
        // the undo hooks of TTransArray::ModifyItem and Remove
        if (c != i && GUndo)
            NOT_DECOMPILED(0x00129602);
        Actors[c++] = Actors[i];
    }
    if (c != Actors.Num())
    {
        if (GUndo)
            NOT_DECOMPILED(0x0012caae);
        Actors.Remove(c, Actors.Num() - c);
    }
}
UBOOL ULevel::Listen(FString& Error)
{
    if (NetDriver)
    {
        Error = LocalizeError(TEXT("NetAlready"), TEXT("Engine"));
        return 0;
    }

    UClass* NetDriverClass = StaticLoadClass(UNetDriver::StaticClass(), nullptr, TEXT("ini:Engine.Engine.NetworkDevice"), nullptr, LOAD_NoFail, nullptr);
    NetDriver = static_cast<UNetDriver*>(StaticConstructObject(NetDriverClass, GetTransientPackage(), NAME_None, 0, nullptr, GError));
    if (!NetDriver->InitListen(this, URL, Error))
    {
        GLog->Logf(TEXT("Failed to listen: %s"), *Error);
        delete NetDriver;
        NetDriver = nullptr;
        return 0;
    }

    // internet servers send at most the internet rate, unless it is unusably low
    static UBOOL LanPlay = ParseParam(appCmdLine(), TEXT("lanplay"));
    if (!LanPlay && NetDriver->MaxInternetClientRate < NetDriver->MaxClientRate && NetDriver->MaxInternetClientRate > 2500)
        NetDriver->MaxClientRate = NetDriver->MaxInternetClientRate;

    UGameEngine* GameEngine = CastChecked<UGameEngine>(Engine);
    GameEngine->BuildServerMasterMap(NetDriver, this);

    // the server actors, each optionally followed by Key=Value config properties
    for (INT i = 0; i < GameEngine->ServerActors.Num(); i++)
    {
        TCHAR Str[240];
        const TCHAR* Ptr = *GameEngine->ServerActors[i];
        if (!ParseToken(Ptr, Str, ARRAY_COUNT(Str), 1))
            continue;
        GLog->Logf(TEXT("Spawning: %s"), Str);
        UClass* HelperClass = StaticLoadClass(AActor::StaticClass(), nullptr, Str, nullptr, LOAD_NoFail, nullptr);
        AActor* Actor = SpawnActor(HelperClass);
        while (Actor && ParseToken(Ptr, Str, ARRAY_COUNT(Str), 1))
        {
            TCHAR* Value = appStrchr(Str, '=');
            if (!Value)
                continue;
            *Value++ = 0;
            for (TFieldIterator<UProperty> It(Actor->GetClass()); It; ++It)
            {
                if (appStricmp(It->GetName(), Str) == 0 && (It->PropertyFlags & CPF_Config))
                    It->ImportText(Value, reinterpret_cast<BYTE*>(Actor) + It->Offset, 0);
            }
        }
    }

    // listen server with a client, else dedicated server
    GetLevelInfo()->NetMode = Engine->Client ? 2 : 1;
    GetLevelInfo()->NextSwitchCountdown = NetDriver->ServerTravelPause;
    return 1;
}

UBOOL ULevel::IsServer()
{
    // a client's drivers have a server connection
    if (NetDriver && NetDriver->ServerConnection)
        return 0;
    if (DemoRecDriver && DemoRecDriver->ServerConnection)
        return 0;
    return 1;
}

UBOOL ULevel::IsDedicatedServer()
{
    ALevelInfo* Info = GetLevelInfo();
    if (Info && Info->GRI)
        return Info->GRI->bDedicatedServer;
    return 0;
}

namespace
{
    // whether Other is Actor or one of its bases
    UBOOL IsBaseChainOf(const AActor* Actor, const AActor* Other)
    {
        for (const AActor* A = Actor; A; A = A->Base)
            if (A == Other)
                return 1;
        return 0;
    }

    // a vector in the frame of a rotation, and back out of one
    FVector WorldToLocal(const FVector& V, const FRotator& R)
    {
        const FMatrix M = FRotationTranslationMatrix(R, FVector(0.f, 0.f, 0.f));
        return FVector(V.X * M.M[0][0] + V.Y * M.M[0][1] + V.Z * M.M[0][2], V.X * M.M[1][0] + V.Y * M.M[1][1] + V.Z * M.M[1][2],
            V.X * M.M[2][0] + V.Y * M.M[2][1] + V.Z * M.M[2][2]);
    }
    FVector LocalToWorld(const FVector& V, const FRotator& R)
    {
        const FMatrix M = FRotationTranslationMatrix(R, FVector(0.f, 0.f, 0.f));
        return FVector(V.X * M.M[0][0] + V.Y * M.M[1][0] + V.Z * M.M[2][0], V.X * M.M[0][1] + V.Y * M.M[1][1] + V.Z * M.M[2][1],
            V.X * M.M[0][2] + V.Y * M.M[1][2] + V.Z * M.M[2][2]);
    }
}

// moves an actor, stopping at the first thing that blocks it, and drags its attached actors along.
// returns whether it moved at all.
UBOOL ULevel::MoveActor(AActor* Actor, const FVector& Delta, const FRotator& NewRotation, FCheckResult& Hit, UBOOL Test,
    UBOOL IgnorePawns, UBOOL bIgnoreBases, UBOOL bNoFail, DWORD ExtraNodeFlags)
{
    if (Actor->bStatic || !Actor->bMovable)
        return 0;

    // actors with nothing attached that only turn, or that don't collide, just move
    const UBOOL bNoAttached = Actor->Attached.Num() == 0;
    UBOOL bSimple = bNoAttached;
    if (Delta.IsZero())
    {
        if (Actor->Rotation == NewRotation)
            return 1;
        if (bNoAttached && Actor->GetPrimitive()->UseCylinderCollision(Actor))
        {
            Actor->Rotation = NewRotation;
            if (Test)
                return 1;
            Actor->UpdateRelativeRotation();
            Actor->ClearRenderData();
            return 1;
        }
    }
    if (bSimple && !Actor->bCollideActors && !Actor->bCollideWorld)
    {
        Actor->Location = Actor->Location + Delta;
        if (Actor->Base && Actor->AttachmentBone == NAME_None)
            Actor->RelativeLocation = Actor->Location - Actor->Base->Location;
        if (!Test)
            Actor->ClearRenderData();
        if (Actor->Rotation == NewRotation)
            return 1;
        Actor->Rotation = NewRotation;
        if (Test)
            return 1;
        Actor->UpdateRelativeRotation();
        return 1;
    }

    FMemMark Mark(GMem);
    Hit = FCheckResult(1.f);

    // trace a little further than the move so the actor stops short of what it hits
    FLOAT DeltaSize = 0.f;
    FVector DeltaDir = Delta;
    if (fabsf(Delta.X) >= 0.0001f || fabsf(Delta.Y) >= 0.0001f || fabsf(Delta.Z) >= 0.0001f)
    {
        DeltaSize = Delta.Size();
        DeltaDir = Delta * (1.f / DeltaSize);
    }
    const FVector TestAdjust = DeltaDir * 2.f;
    const FVector TestDelta = Delta + TestAdjust;

    UBOOL bHitSomething = 0;
    FCheckResult* FirstHit = nullptr;
    UBOOL bTouchNotify = 1;
    if (Actor->bCollideActors || Actor->bCollideWorld)
    {
        if (!(Actor->bCollideActors && Actor->IsEncroacher()) && DeltaSize != 0.f)
        {
            bTouchNotify = 0;
            DWORD TraceFlags = 0;
            if (Actor->bCollideActors)
                TraceFlags = (IgnorePawns ? 0 : 0x1) | 0x18;
            if (Actor->bCollideWorld)
                TraceFlags |= 0x86;
            ALevelInfo* Info = Actor->bCollideWorld ? GetLevelInfo() : nullptr;
            const FVector Extent(Actor->CollisionRadius, Actor->CollisionRadius, Actor->CollisionHeight);
            FirstHit = MultiLineCheck(GMem, Actor->Location + TestDelta, Actor->Location, Extent, Info, TraceFlags | ExtraNodeFlags, Actor);
            if (Actor->bCollideWorld || Actor->bBlockActors || Actor->bBlockPlayers)
            {
                for (FCheckResult* Test2 = FirstHit; Test2; Test2 = Test2->GetNext())
                {
                    if ((bIgnoreBases && IsBaseChainOf(Actor, Test2->Actor)) || IsBaseChainOf(Test2->Actor, Actor))
                        continue;
                    bHitSomething = 1;
                    if (Actor->IsBlockedBy(Test2->Actor))
                    {
                        Hit = *Test2;
                        break;
                    }
                }
            }
        }
    }

    // stop at the hit
    FVector FinalDelta = Delta;
    if (Hit.Time < 1.f && !bNoFail)
    {
        const FLOAT DistanceToHit = (DeltaSize + 2.f) * Hit.Time;
        if (DistanceToHit > 2.f)
        {
            FinalDelta = TestDelta * Hit.Time - TestAdjust;
            Hit.Time = (DistanceToHit - 2.f) / DeltaSize;
        }
        else
        {
            FinalDelta = FVector(0.f, 0.f, 0.f);
            Hit.Time = 0.f;
        }
    }

    // an encroacher fails the move if it would push into something that won't move
    if (!Test && !bNoFail && Actor->bCollideActors && Actor->IsEncroacher()
        && CheckEncroachment(Actor, Actor->Location + FinalDelta, NewRotation, bTouchNotify))
    {
        Mark.Pop();
        return 0;
    }
    if (Actor->bCollideActors && Hash)
        Hash->RemoveActor(Actor);

    Actor->Location = Actor->Location + FinalDelta;
    const FRotator OldRotation = Actor->Rotation;
    Actor->Rotation = NewRotation;

    if (!Test && Actor->Attached.Num() > 0)
    {
        FRotator RotationDelta(0, 0, 0);
        if (Actor->Rotation != OldRotation)
            RotationDelta = FRotator((Actor->Rotation.Pitch & 0xfffc) - (OldRotation.Pitch & 0xfffc),
                (Actor->Rotation.Yaw & 0xfffc) - (OldRotation.Yaw & 0xfffc), (Actor->Rotation.Roll & 0xfffc) - (OldRotation.Roll & 0xfffc));

        for (INT i = 0; i < Actor->Attached.Num(); i++)
        {
            AActor* Other = Actor->Attached[i];
            if (!Other)
                continue;
            FCheckResult AttachHit(1.f);
            if (Other->AttachmentBone == NAME_None)
            {
                FRotator OtherRotation = Other->Rotation + RotationDelta;
                FVector OffsetDelta(0.f, 0.f, 0.f);
                if (Actor->Rotation != OldRotation)
                {
                    // pawns turn their controller too, and keep their roll unless they crawl
                    APawn* Pawn = Other->IsAPawn() ? Cast<APawn>(Other) : nullptr;
                    if (Pawn)
                    {
                        INT ControllerRoll = 0;
                        if (Pawn->Controller)
                        {
                            ControllerRoll = Pawn->Controller->Rotation.Roll;
                            Pawn->Controller->Rotation = Pawn->Controller->Rotation + RotationDelta;
                        }
                        if (!Pawn->bCrawler)
                        {
                            OtherRotation.Roll = Other->Rotation.Roll;
                            if (Pawn->Controller)
                                Pawn->Controller->Rotation.Roll = ControllerRoll;
                        }
                    }
                    // the attached actor's offset turns with the base
                    const FVector Offset = Other->RelativeLocation;
                    OffsetDelta = LocalToWorld(WorldToLocal(Offset, OldRotation), Actor->Rotation) - Offset;
                }
                MoveActor(Other, OffsetDelta + FinalDelta, OtherRotation, AttachHit, 0, 0, 1, 0, 0);
            }

            if (!bNoFail && Other->IsBlockedBy(Actor))
            {
                // an attached actor that ends up inside its base can stop the base's move
                FCheckResult Test2(1.f);
                UBOOL bOverlap;
                if (Other->Brush && Other->IsAVolume())
                {
                    const FVector Extent(Actor->CollisionRadius, Actor->CollisionRadius, Actor->CollisionHeight);
                    bOverlap = !Other->GetPrimitive()->PointCheck(Test2, Other, Actor->Location, Extent, 0);
                }
                else
                {
                    const FVector Extent(Other->CollisionRadius, Other->CollisionRadius, Other->CollisionHeight);
                    bOverlap = !Actor->GetPrimitive()->PointCheck(Test2, Actor, Other->Location, Extent, 0);
                }
                if (bOverlap && Actor->eventEncroachingOn(Other))
                {
                    Actor->Location = Actor->Location - FinalDelta;
                    Actor->Rotation = OldRotation;
                    if (Actor->bCollideActors && Hash)
                        Hash->AddActor(Actor);
                    for (INT j = 0; j < Actor->Attached.Num(); j++)
                        if (Actor->Attached[j])
                            MoveActor(Actor->Attached[j], FinalDelta * -1.f, Actor->Attached[j]->Rotation, AttachHit, 0, 0, 1, 0, 0);
                    Mark.Pop();
                    return 0;
                }
            }
        }
        if (!InTick && Actor->MeshInstance)
            Actor->MeshInstance->UpdateAttachments();
    }

    if (!Test && !Actor->bOnlyDrawIfAttached)
    {
        if (Actor->Base && Actor->AttachmentBone == NAME_None)
            Actor->RelativeLocation = Actor->Location - Actor->Base->Location;
        if (Actor->Rotation != OldRotation)
            Actor->UpdateRelativeRotation();
    }

    if (Actor->bCollideActors && Hash)
        Hash->AddActor(Actor);

    // a karma model that isn't driven by a body follows the actor
    if (McdModel* Model = Actor->getKModel())
    {
        if (!McdModelGetBody(Model) && !Actor->bStatic
            && (!Actor->IsAPawn() || NewRotation.Pitch != OldRotation.Pitch || NewRotation.Roll != OldRotation.Roll))
        {
            KU2METransform(McdModelGetTransformPtr(Model), Actor->Location, Actor->Rotation);
            if (static_cast<BYTE>(McdModelGetGeometry(Model)->TypeId))
                McdModelUpdate(Model);
        }
    }

    if (!Test)
    {
        AActor* HitActor = Hit.Actor;
        if (HitActor && !HitActor->bWorldGeometry && !IsBaseChainOf(Actor, HitActor))
        {
            HitActor->NotifyBumpEx(Actor);
            Actor->NotifyBumpEx(Hit.Actor);
        }

        // touch what was passed through on the way
        if (bHitSomething || !Actor->bBlockActors || !Actor->bBlockPlayers)
        {
            for (FCheckResult* Test2 = FirstHit; Test2 && Test2->Time < Hit.Time; Test2 = Test2->GetNext())
            {
                if ((!bIgnoreBases || !Actor->IsJoinedTo(Test2->Actor)) && !Actor->IsBlockedBy(Test2->Actor) && Actor != Test2->Actor)
                    Actor->BeginTouch(Test2->Actor);
            }
        }

        // untouch what it no longer overlaps
        for (INT i = 0; i < Actor->Touching.Num();)
        {
            if (Actor->Touching[i] && !Actor->IsOverlapping(Actor->Touching[i]))
                Actor->EndTouch(Actor->Touching[i], 0);
            else
                i++;
        }
    }

    Actor->SetZone(Test, 0);
    Mark.Pop();
    if (!Test)
        Actor->ClearRenderData();
    return Hit.Time > 0.f;
}

UBOOL ULevel::FarMoveActor(AActor* Actor, const FVector& DestLocation, UBOOL Test, UBOOL bNoCheck, UBOOL bAttachedMove)
{
    NOT_DECOMPILED(0x00129074);
    return 0;
}

INT ULevel::GetActorIndex(AActor* Actor)
{
    for (INT i = 0; i < Actors.Num(); i++)
        if (Actors[i] == Actor)
            return i;
    GError->Logf(TEXT("Actor not found: %s"), Actor->GetFullName());
    return -1;
}

UBOOL ULevel::DestroyActor(AActor* Actor, UBOOL bNetForce)
{
    if (Actor->bStatic || Actor->bNoDelete)
        return 0;
    if (Actor->bDeleteMe)
        return 1;
    // only the authority destroys actors, unless forced or the actor is temporary
    if (Actor->Role != 4 && !bNetForce && !Actor->bNetTemporary)
        return 0;

    // a remote player's controller closes its connection first (sets bPendingDestroy, closes channel 0, returns 0)
    if (Actor->IsAPlayerController())
    {
        APlayerController* PC = Cast<APlayerController>(Actor);
        if (PC && Cast<UNetConnection>(PC->Player))
            NOT_DECOMPILED(0x00129dbd);
    }

    // the xbox also tells an object at 0x00459600; nothing ever sets it
    const INT ActorIndex = GetActorIndex(Actor);
    // the undo hook of TTransArray::ModifyItem
    if (GUndo)
        NOT_DECOMPILED(0x00129602);
    Actor->Modify();

    // the script events may destroy it again; the inner call finishes the job
    Actor->bPendingDelete = 1;
    if (Actor->GetStateFrame() && Actor->GetStateFrame()->StateNode)
    {
        Actor->eventEndState();
        if (Actor->bDeleteMe)
            return 1;
    }
    Actor->eventDestroyed();
    Actor->PostScriptDestroyed();

    if (Actor->Base)
    {
        Actor->SetBase(nullptr, FVector(0, 0, 1), 1);
        if (Actor->bDeleteMe)
            return 1;
    }
    for (INT i = 0; i < Actor->Attached.Num(); i++)
        if (Actor->Attached[i])
            Actor->Attached[i]->SetBase(nullptr, FVector(0, 0, 1), 1);
    if (Actor->bDeleteMe)
        return 1;
    for (INT i = 0; i < Actor->Touching.Num(); i++)
    {
        AActor* Other = Actor->Touching[i];
        INT OtherIndex = 0;
        if (Other && Other->Touching.FindItem(Actor, OtherIndex))
        {
            Actor->EndTouch(Other, 1);
            i--;
            if (Actor->bDeleteMe)
                return 1;
        }
    }
    if (Actor->Owner)
    {
        Actor->Owner->eventLostChild(Actor);
        if (Actor->bDeleteMe)
            return 1;
    }

    if (NetDriver)
        NetDriver->NotifyActorDestroyed(Actor);
    if (DemoRecDriver && !DemoRecDriver->ServerConnection)
        DemoRecDriver->NotifyActorDestroyed(Actor);
    if (Hash && Actor->bCollideActors)
        Hash->RemoveActor(Actor);
    Actor->ClearRenderData();
    if (ActorIndex >= 0)
        Actors[ActorIndex] = nullptr;
    Actor->bDeleteMe = 1;
    // stops the actor's sounds
    if (Engine->Audio)
        NOT_DECOMPILED(0x00074ed7);
    Actor->ConditionalDestroy();

    // CleanupDestroyed deletes it later
    Actor->Deleted = FirstDeleted;
    FirstDeleted = Actor;
    return 1;
}

void ULevel::CleanupDestroyed(UBOOL bForce)
{
    if (!bForce)
        CompactActors();
    if (!FirstDeleted)
        return;
    // without bForce, waits until 128 actors are waiting
    if (!bForce)
    {
        INT Count = 0;
        for (AActor* Actor = FirstDeleted; Actor; Actor = Actor->Deleted)
            Count++;
        if (Count < 128)
            return;
    }

    // clear every reference to them, then delete them
    for (INT i = 0; i < Actors.Num(); i++)
    {
        if (AActor* Actor = Actors[i])
        {
            Actor->GetClass()->CleanupDestroyed(reinterpret_cast<BYTE*>(Actor));
            Actor->FixupSubObjectPointers();
        }
    }
    while (FirstDeleted)
    {
        AActor* Actor = FirstDeleted;
        FirstDeleted = Actor->Deleted;
        if (Actor->MeshInstance)
        {
            delete Actor->MeshInstance;
            Actor->MeshInstance = nullptr;
        }
        delete Actor;
    }
}

AActor* ULevel::SpawnActor(UClass* Class, FName InName, const FVector& Location, FRotator Rotation, AActor* Template,
    UBOOL bNoCollisionFail, UBOOL bRemoteOwned, AActor* Owner, APawn* Instigator)
{
    FVector NewLocation = Location;
    if (!Class)
    {
        GLog->Logf(NAME_Warning, TEXT("SpawnActor failed because no class was specified"));
        return nullptr;
    }
    if (Class->ClassFlags & CLASS_Abstract)
    {
        GLog->Logf(NAME_Warning, TEXT("SpawnActor failed because class %s is abstract"), Class->GetName());
        return nullptr;
    }
    if (!Class->IsChildOf(AActor::StaticClass()))
    {
        GLog->Logf(NAME_Warning, TEXT("SpawnActor failed because %s is not an actor class"), Class->GetName());
        return nullptr;
    }
    AActor* Default = static_cast<AActor*>(Class->GetDefaultObject());
    if (Default->bStatic || Default->bNoDelete)
    {
        GLog->Logf(NAME_Warning, TEXT("SpawnActor failed because class %s has bStatic or bNoDelete"), Class->GetName());
        return nullptr;
    }
    // high detail actors are left out at low detail and on dedicated servers
    if (Default->bHighDetail)
    {
        ALevelInfo* Info = GetLevelInfo();
        if (Info->DetailMode == 0 || Info->bDropDetail || Info->NetMode == 1)
            return nullptr;
    }
    if (!Template)
        Template = Default;

    // find a spot the actor fits; clients do not check actors that only collide when placed
    if ((Template->bCollideWorld || (Template->bCollideWhenPlacing && GetLevelInfo()->NetMode != 3)) && !bNoCollisionFail)
    {
        const FVector Extent(Template->CollisionRadius, Template->CollisionRadius, Template->CollisionHeight);
        if (!FindSpot(Extent, NewLocation))
            return nullptr;
    }

    // the new slot is recorded for undo
    const INT iActor = Actors.AddZeroed();
    if (GUndo)
        NOT_DECOMPILED(0x0011f441);
    AActor* Actor = static_cast<AActor*>(StaticConstructObject(Class, GetOuter(), InName, 0, Template, GError));
    Actors[iActor] = Actor;
    Actor->SetFlags(RF_Transactional);
    Actor->Tag = Class->GetFName();
    Actor->Region.Zone = GetLevelInfo();
    Actor->Region.iLeaf = INDEX_NONE;
    Actor->Region.ZoneNumber = 0;
    Actor->Level = GetLevelInfo();
    Actor->XLevel = this;
    Actor->bTicked = !Ticked;
    if (bRemoteOwned)
        Exchange(Actor->Role, Actor->RemoteRole);
    if (Actor->Brush)
        Actor->Brush = nullptr;
    Actor->Location = NewLocation;
    Actor->Rotation = Rotation;
    if (Actor->bCollideActors && Hash)
        Hash->AddActor(Actor);
    Actor->CurrentPhysicsVolume = GetLevelInfo()->CurrentPhysicsVolume;
    Actor->SetOwner(Owner);
    Actor->bSelected = 0;
    Actor->Instigator = Instigator;
    KInitActorKarma(Actor);

    Actor->InitExecution();
    Actor->Spawned();
    Actor->eventPreBeginPlay();
    if (Actor->bDeleteMe)
        return nullptr;
    Actor->eventBeginPlay();
    if (Actor->bDeleteMe)
        return nullptr;
    Actor->SetZone(iActor == 0, 1);
    Actor->ClearRenderData();

    if (!bNoCollisionFail)
    {
        if (Actor->bCollideActors && Hash)
            Hash->RemoveActor(Actor);
        if (CheckEncroachment(Actor, Actor->Location, Actor->Rotation, 1))
        {
            DestroyActor(Actor);
            return nullptr;
        }
        if (Actor->bCollideActors && Hash)
            Hash->AddActor(Actor);
    }

    Actor->eventPostBeginPlay();
    Actor->eventPostLoadBeginPlay();
    Actor->Unknown14C();
    if (Actor->bDeleteMe)
        return nullptr;
    Actor->PostBeginPlay();
    Actor->eventSetInitialState();

    // physics 0 is none, 5 is rotating
    if (!Actor->Base && Actor->bCollideWorld && Actor->bShouldBaseAtStartup && (Actor->Physics == 0 || Actor->Physics == 5))
        Actor->FindBase();

    if (InTick)
    {
        FActorLink* Link = reinterpret_cast<FActorLink*>(GEngineMem.PushBytes(sizeof(FActorLink), 16));
        Link->Actor = Actor;
        Link->Next = NewlySpawned;
        NewlySpawned = Link;
    }
    if (!bRemoteOwned)
        Actor->eventPostNetBeginPlay();
    return Actor;
}

ABrush* ULevel::SpawnBrush() { NOT_DECOMPILED(0x00128849); return nullptr; }
void ULevel::SpawnViewActor(UViewport* Viewport) { NOT_DECOMPILED(0x00128898); }

APlayerController* ULevel::SpawnPlayActor(UPlayer* Player, BYTE RemoteRole, const FURL& URL, FString& Error)
{
    NOT_DECOMPILED(0x0012dbbf);
    return nullptr;
}

UBOOL ULevel::FindSpot(const FVector& Extent, FVector& Location) { NOT_DECOMPILED(0x00128d56); return 0; }
UBOOL ULevel::CheckSlice(FVector& Location, const FVector& Extent, INT& bKeepTrying) { NOT_DECOMPILED(0x00128a1b); return 0; }

UBOOL ULevel::CheckEncroachment(AActor* Actor, const FVector& TestLocation, const FRotator& TestRotation, UBOOL bTouchNotify)
{
    if (!Actor->bCollideActors && !Actor->bBlockActors && !Actor->bBlockPlayers)
        return 0;

    // an encroacher carries what it hits along: these map a point from where it is to where it would be
    FMatrix WorldToLocal;
    FMatrix TestLocalToWorld;
    if (Actor->IsEncroacher())
    {
        WorldToLocal = Actor->WorldToLocal();
        const FVector OldLocation = Actor->Location;
        const FRotator OldRotation = Actor->Rotation;
        Actor->Location = TestLocation;
        Actor->Rotation = TestRotation;
        TestLocalToWorld = Actor->LocalToWorld();
        Actor->Location = OldLocation;
        Actor->Rotation = OldRotation;
    }

    FMemMark Mark(GMem);
    FCheckResult* FirstHit = Hash ? Hash->ActorEncroachmentCheck(GMem, Actor, TestLocation, TestRotation, 0x9f, 0) : nullptr;
    for (FCheckResult* Test = FirstHit; Test; Test = Test->GetNext())
    {
        AActor* Other = Test->Actor;
        if (Other == Actor || Other == GetLevelInfo() || Other->IsJoinedTo(Actor) || !Actor->IsBlockedBy(Other))
            continue;

        // an encroacher pushes what isn't one out of its way, except a karma body (physics 13) and pawns
        if (Actor->IsEncroacher() && !Other->IsEncroacher() && !(Actor->Physics == 13 && Other->IsA(APawn::StaticClass())))
        {
            FVector PushVect = TestLocation - Actor->Location;
            if (TestRotation != Actor->Rotation)
            {
                const FVector& Point = Other->Location;
                PushVect = (TestLocalToWorld.TransformFVector(WorldToLocal.TransformFVector(Point)) - Point) * 1.5f + PushVect;
            }
            Other->moveSmooth(PushVect);

            // whether the pushed actor is clear of the moved one
            const FVector OldLocation = Actor->Location;
            const FRotator OldRotation = Actor->Rotation;
            Actor->Location = TestLocation;
            Actor->Rotation = TestRotation;
            FCheckResult Hit(1.f);
            UBOOL bClear;
            if (Other->Brush && Other->IsAVolume())
                bClear = Other->GetPrimitive()->PointCheck(Hit, Other, Actor->Location,
                    FVector(Actor->CollisionRadius, Actor->CollisionRadius, Actor->CollisionHeight), 0);
            else
                bClear = Actor->GetPrimitive()->PointCheck(Hit, Actor, Other->Location,
                    FVector(Other->CollisionRadius, Other->CollisionRadius, Other->CollisionHeight), 0);
            Actor->Location = OldLocation;
            Actor->Rotation = OldRotation;
            if (bClear)
            {
                // moves it back towards the encroacher, which now stands at the test location
                Actor->Location = TestLocation;
                const FVector BackVect = PushVect * -1.f;
                MoveActor(Other, BackVect, Other->Rotation, Hit, 0, 0, 0, 0, 0);
                Actor->Location = OldLocation;
                continue;
            }
        }
        if (Actor->eventEncroachingOn(Other))
        {
            Mark.Pop();
            return 1;
        }
    }

    // untouch what the actor no longer overlaps
    if (bTouchNotify)
    {
        for (INT i = 0; i < Actor->Touching.Num();)
        {
            if (Actor->Touching[i] && !Actor->IsOverlapping(Actor->Touching[i]))
                Actor->EndTouch(Actor->Touching[i], 0);
            else
                i++;
        }
    }
    // what blocks the actor is told it is encroached on; the rest is touched
    for (FCheckResult* Test = FirstHit; Test; Test = Test->GetNext())
    {
        AActor* Other = Test->Actor;
        if (Other == Actor || Other->IsJoinedTo(Actor) || Other == GetLevelInfo())
            continue;
        if (Actor->IsBlockedBy(Other))
            Other->eventEncroachedBy(Actor);
        else if (bTouchNotify)
            Actor->BeginTouch(Other);
    }
    Mark.Pop();
    return 0;
}

UBOOL ULevel::SinglePointCheck(FCheckResult& Hit, const FVector& Location, const FVector& Extent, DWORD ExtraNodeFlags,
    ALevelInfo* Level, UBOOL bActors)
{
    NOT_DECOMPILED(0x0012859f);
    return 0;
}

UBOOL ULevel::EncroachingWorldGeometry(FCheckResult& Hit, const FVector& Location, const FVector& Extent,
    DWORD ExtraNodeFlags, ALevelInfo* Level)
{
    NOT_DECOMPILED(0x0012868f);
    return 0;
}

UBOOL ULevel::SingleLineCheck(FCheckResult& Hit, AActor* SourceActor, const FVector& End, const FVector& Start,
    DWORD TraceFlags, const FVector& Extent)
{
    NOT_DECOMPILED(0x00129313);
    return 0;
}

FCheckResult* ULevel::MultiPointCheck(FMemStack& Mem, const FVector& Location, const FVector& Extent,
    DWORD ExtraNodeFlags, ALevelInfo* Level, UBOOL bActors, UBOOL Unknown1, UBOOL Unknown2)
{
    NOT_DECOMPILED(0x0012d08f);
    return nullptr;
}

FCheckResult* ULevel::MultiLineCheck(FMemStack& Mem, const FVector& End, const FVector& Start, const FVector& Size,
    ALevelInfo* LevelInfo, DWORD ExtraNodeFlags, AActor* SourceActor)
{
    NOT_DECOMPILED(0x0012d353);
    return nullptr;
}

void ULevel::DetailChange(BYTE NewDetail)
{
    GRenderUnknown.Reset();
    GetLevelInfo()->DetailMode = NewDetail;
    if (GetLevelInfo()->Game)
        GetLevelInfo()->Game->eventDetailChange();
    for (INT i = 0; i < Actors.Num(); i++)
        if (Actors[i])
            Actors[i]->StaticFilterState = 0;
}

INT ULevel::TickDemoRecord(FLOAT DeltaSeconds) { NOT_DECOMPILED(0x00131a24); return 0; }
INT ULevel::TickDemoPlayback(FLOAT DeltaSeconds) { NOT_DECOMPILED(0x00131be5); return 0; }

void ULevel::UpdateTime(ALevelInfo* Info)
{
    appSystemTime(Info->Year, Info->Month, Info->DayOfWeek, Info->Day, Info->Hour, Info->Minute, Info->Second,
        Info->Millisecond);
}

UBOOL ULevel::IsPaused()
{
    // a pause takes PauseDelay seconds to start
    ALevelInfo* Info = GetLevelInfo();
    return Info->Pauser && Info->PauseDelay <= Info->TimeSeconds;
}
void ULevel::WelcomePlayer(UNetConnection* Connection, TCHAR* Optional) { NOT_DECOMPILED(0x000a6eea); }
void ULevel::BuildRenderData() { NOT_DECOMPILED(0x000a8815); }

UBOOL ULevel::IsAudibleAt(const FVector& SourceLocation, const FVector& ListenerLocation, AActor* Source)
{
    NOT_DECOMPILED(0x000a5fc9);
    return 0;
}

FLOAT ULevel::CalculateRadiusMultiplier(INT Unknown1, INT Unknown2) { NOT_DECOMPILED(0x0027ab5a); return 0.f; }

EAcceptConnection ULevel::NotifyAcceptingConnection() { NOT_DECOMPILED(0x000a6de2); return ACCEPTC_Reject; }
void ULevel::NotifyAcceptedConnection(UNetConnection* Connection) { NOT_DECOMPILED(0x000a6e7e); }
UBOOL ULevel::NotifyAcceptingChannel(UChannel* Channel) { NOT_DECOMPILED(0x000a5c53); return 0; }
void ULevel::NotifyReceivedText(UNetConnection* Connection, const TCHAR* Text) { NOT_DECOMPILED(0x000a7d4b); }
UBOOL ULevel::NotifySendingFile(UNetConnection* Connection, FGuid GUID) { NOT_DECOMPILED(0x000a5d1b); return 0; }

void ULevel::NotifyReceivedFile(UNetConnection* Connection, INT PackageIndex, const TCHAR* Error, UBOOL Skipped)
{
    GError->Logf(TEXT("Level received unexpected file"));
}
