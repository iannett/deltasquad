#include "EngineClasses.h"
#include "UnEngineGlobals.h"

void KInitActorKarma(AActor* Actor);

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
    if (Unknown1128 && Unknown112C)
        NOT_DECOMPILED(0x0009085e);
    // also resets a global at 0x004aacb0 (0x00136bce) whose class is not known yet
    NOT_DECOMPILED(0x00136bce);
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
        NOT_DECOMPILED(0x0008f45a);
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
    Ar << UnknownD0;
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

void ULevel::Tick(ELevelTick TickType, FLOAT DeltaSeconds) { NOT_DECOMPILED(0x00131c90); }
void ULevel::TickNetClient(FLOAT DeltaSeconds) { NOT_DECOMPILED(0x001332cb); }
void ULevel::TickNetServer(FLOAT DeltaSeconds) { NOT_DECOMPILED(0x00133478); }
INT ULevel::ServerTickClients(FLOAT DeltaSeconds) { NOT_DECOMPILED(0x00131109); return 0; }
void ULevel::ReconcileActors() { NOT_DECOMPILED(0x000a6b89); }
void ULevel::RememberActors() { NOT_DECOMPILED(0x000a5f39); }
UBOOL ULevel::Exec(const TCHAR* Cmd, FOutputDevice& Ar) { NOT_DECOMPILED(0x000a64c0); return 0; }
void ULevel::ShrinkLevel() { NOT_DECOMPILED(0x000ab8a2); }
void ULevel::CompactActors() { NOT_DECOMPILED(0x0012cec5); }
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
    if (NetDriver || DemoRecDriver)
        NOT_DECOMPILED(0x000a5c32);
    return 1;
}

UBOOL ULevel::IsDedicatedServer()
{
    ALevelInfo* Info = GetLevelInfo();
    if (Info && Info->GRI)
        return Info->GRI->bDedicatedServer;
    return 0;
}

UBOOL ULevel::MoveActor(AActor* Actor, const FVector& Delta, const FRotator& NewRotation, FCheckResult& Hit, UBOOL Test,
    UBOOL IgnorePawns, UBOOL bIgnoreBases, UBOOL bNoFail, DWORD Unknown)
{
    NOT_DECOMPILED(0x0012a005);
    return 0;
}

UBOOL ULevel::FarMoveActor(AActor* Actor, const FVector& DestLocation, UBOOL Test, UBOOL bNoCheck, UBOOL bAttachedMove)
{
    NOT_DECOMPILED(0x00129074);
    return 0;
}

UBOOL ULevel::DestroyActor(AActor* Actor, UBOOL bNetForce) { NOT_DECOMPILED(0x00129d5b); return 0; }
void ULevel::CleanupDestroyed(UBOOL bForce) { NOT_DECOMPILED(0x001284e7); }

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
    NOT_DECOMPILED(0x0012c094);
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
    NOT_DECOMPILED(0x00136bce);
    GetLevelInfo()->DetailMode = NewDetail;
    if (GetLevelInfo()->Game)
        NOT_DECOMPILED(0x000a5f02);
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

UBOOL ULevel::IsPaused() { NOT_DECOMPILED(0x00131c65); return 0; }
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
