#include "EngineClasses.h"

UPrimitive* AActor::GetPrimitive()
{
    if (Mesh)
        return Mesh;
    if (StaticMesh)
        return StaticMesh;
    if (AntiPortal)
        return AntiPortal;
    return XLevel->Engine ? XLevel->Engine->Cylinder : nullptr;
}

void AActor::SetOwner(AActor* NewOwner)
{
    if (Owner)
        Owner->eventLostChild(this);
    Owner = NewOwner;
    if (NewOwner)
        NewOwner->eventGainedChild(this);
    bNetDirty = 1;
}

void AActor::ClearRenderData()
{
    RenderRevision++;
    if (AntiPortal)
    {
        for (INT i = 0; i < Leaves.Num(); i++)
            XLevel->Unknown1100[XLevel->Model->UnknownA4[Leaves[i]].Unknown0].RemoveItem(this);
    }
}

UPrimitive* ABrush::GetPrimitive()
{
    if (Brush)
        return Brush;
    return XLevel->Engine ? XLevel->Engine->Cylinder : nullptr;
}

UPrimitive* AFluidSurfaceInfo::GetPrimitive()
{
    NOT_DECOMPILED(0x000a2409);
    return nullptr;
}

UPrimitive* AProjector::GetPrimitive()
{
    NOT_DECOMPILED(0x000779f8);
    return nullptr;
}

UPrimitive* ATerrainInfo::GetPrimitive()
{
    NOT_DECOMPILED(0x00070064);
    return nullptr;
}

FLOAT UPawnAudioTable::Unknown431EC0[193];

void UPawnAudioTable::Reset()
{
    for (INT i = 0; i < 193; i++)
        Unknown431EC0[i] = -10000.f;
}

namespace
{
    // the zone part every SetZone version inlines; zone events only fire when not testing
    void UpdateZone(AActor* This, UBOOL bTest, UBOOL bForceRefresh)
    {
        if (bForceRefresh)
        {
            This->Region.Zone = This->Level;
            This->Region.iLeaf = INDEX_NONE;
            This->Region.ZoneNumber = 0;
        }

        const FPointRegion NewRegion = This->XLevel->Model->PointRegion(This->Level, This->Location);
        if (NewRegion.Zone == This->Region.Zone)
        {
            This->Region = NewRegion;
        }
        else
        {
            if (!bTest)
            {
                This->Region.Zone->eventActorLeaving(This);
                This->eventZoneChange(NewRegion.Zone);
            }
            This->Region = NewRegion;
            if (!bTest)
                This->Region.Zone->eventActorEntered(This);
        }
    }
}

void AActor::SetZone(UBOOL bTest, UBOOL bForceRefresh)
{
    if (bDeleteMe || !Level)
        return;
    UpdateZone(this, bTest, bForceRefresh);

    // the touch list only knows the volumes of a colliding actor that has been placed already
    const UBOOL bUseTouch = bCollideActors && !bTest && !bForceRefresh
        && FVector(CollisionRadius, CollisionRadius, CollisionHeight) != FVector(0.f, 0.f, 0.f);
    APhysicsVolume* NewVolume = Level->GetPhysicsVolume(Location, this, bUseTouch);
    if (bTest)
    {
        CurrentPhysicsVolume = NewVolume;
    }
    else if (NewVolume != CurrentPhysicsVolume)
    {
        if (CurrentPhysicsVolume)
        {
            CurrentPhysicsVolume->eventActorLeavingVolume(this);
            eventPhysicsVolumeChange(NewVolume);
        }
        CurrentPhysicsVolume = NewVolume;
        CurrentPhysicsVolume->eventActorEnteredVolume(this);
    }
}

void APhysicsVolume::SetZone(UBOOL bTest, UBOOL bForceRefresh)
{
    if (bDeleteMe)
        return;
    UpdateZone(this, bTest, bForceRefresh);
    // a physics volume is always in itself
    CurrentPhysicsVolume = this;
}

void ALevelInfo::SetZone(UBOOL bTest, UBOOL bForceRefresh)
{
    if (bDeleteMe)
        return;
    Region.Zone = Level;
    Region.iLeaf = INDEX_NONE;
    Region.ZoneNumber = 0;
}

void APawn::SetZone(UBOOL bTest, UBOOL bForceRefresh)
{
    if (bDeleteMe)
        return;
    UpdateZone(this, bTest, bForceRefresh);

    // the head volume is the one at eye height
    const UBOOL bUseTouch = bCollideActors && !bTest && !bForceRefresh;
    APhysicsVolume* NewVolume = Level->GetPhysicsVolume(Location, this, bUseTouch);
    APhysicsVolume* NewHeadVolume = Level->GetPhysicsVolume(Location + FVector(0.f, 0.f, BaseEyeHeight), this, bUseTouch);
    if (NewVolume != CurrentPhysicsVolume)
    {
        if (!bTest)
        {
            if (CurrentPhysicsVolume)
            {
                CurrentPhysicsVolume->eventPawnLeavingVolume(this);
                eventPhysicsVolumeChange(NewVolume);
            }
            if (Controller)
                Controller->eventNotifyPhysicsVolumeChange(NewVolume);
        }
        CurrentPhysicsVolume = NewVolume;
        if (!bTest)
            CurrentPhysicsVolume->eventPawnEnteredVolume(this);
    }
    if (NewHeadVolume != HeadVolume)
    {
        // the controller can take the head volume change instead
        if (!bTest && (!Controller || !Controller->eventNotifyHeadVolumeChange(NewHeadVolume)))
            eventHeadVolumeChange(NewHeadVolume);
        HeadVolume = NewHeadVolume;
    }
}

UBOOL AActor::IsEncroacher()
{
    if (!bCollideActors)
        return 0;
    // physics 14 is ragdoll, 13 is karma
    return (StaticMesh && IsAMover()) || Physics == 14 || (Physics == 13 && !IsA(AKConstraint::StaticClass()));
}

UBOOL AActor::IsJoinedTo(const AActor* Other) const
{
    for (const AActor* Test = this; Test; Test = Test->Base)
    {
        if (Test == Other || (Other && Test->JoinedTag == Other->JoinedTag && Test->JoinedTag != 0))
            return 1;
    }
    return 0;
}

UBOOL AActor::IsBlockedBy(const AActor* Other) const
{
    AActor* This = const_cast<AActor*>(this);
    AActor* Blocker = const_cast<AActor*>(Other);
    const UBOOL bIsPlayer = This->IsAPlayerController() || This->GetPlayerPawn();
    const UBOOL bOtherBlocks = bIsPlayer ? Other->bBlockPlayers : Other->bBlockActors;
    if (Other->bWorldGeometry && bOtherBlocks)
        return bCollideWorld;

    // an actor ignoring encroachers lets movers and karma bodies through, both ways round
    if (This->IsEncroacher() && Other->bIgnoreEncroachers)
        return 0;
    if (Blocker->IsEncroacher() && bIgnoreEncroachers)
        return 0;

    // brushes and encroachers block what collides with the world; movers let player controllers through
    if ((Other->Brush && Blocker->IsABrush()) || Blocker->IsEncroacher())
    {
        if (Other->StaticMesh && Blocker->IsAMover() && This->IsAPlayerController())
            return 0;
        return bCollideWorld && bOtherBlocks;
    }
    if ((Brush && This->IsABrush()) || This->IsEncroacher())
    {
        if ((StaticMesh && This->IsAMover() && This->IsAPlayerController()) || !Other->bCollideWorld)
            return 0;
        return bIsPlayer ? bBlockPlayers : bBlockActors;
    }
    if (!bOtherBlocks)
        return 0;
    return Blocker->GetPlayerPawn() ? bBlockPlayers : bBlockActors;
}

UBOOL AActor::IsOverlapping(AActor* Other, FCheckResult* Hit)
{
    if ((Brush && IsABrush() && Other->Brush && Other->IsABrush()) || Other == Level || Other == this)
        return 0;
    if (IsEncroacher() && Other->IsEncroacher())
        return 0;
    if (IsJoinedTo(Other) || Other->IsJoinedTo(this))
        return 0;
    if ((bOnlyAffectPawns && !Other->IsA(APawn::StaticClass())) || (Other->bOnlyAffectPawns && !IsA(APawn::StaticClass())))
        return 0;
    if (!bBlockNonZeroExtentTraces || !Other->bBlockNonZeroExtentTraces)
        return 0;

    // projectors, volumes, fluid surfaces and encroachers test the other actor against their own primitive
    AActor* Checker;
    if (IsA(AProjector::StaticClass()))
        Checker = this;
    else if (Other->IsA(AProjector::StaticClass()))
        Checker = Other;
    else if (Brush && IsAVolume())
        Checker = this;
    else if (IsA(AFluidSurfaceInfo::StaticClass()))
        Checker = this;
    else if (Other->Brush && Other->IsAVolume())
        Checker = Other;
    else if (Other->IsA(AFluidSurfaceInfo::StaticClass()))
        Checker = Other;
    else if (IsEncroacher())
        Checker = this;
    else if (Other->IsEncroacher())
        Checker = Other;
    else
    {
        // two cylinders
        const FLOAT Height = Other->CollisionHeight + CollisionHeight;
        const FLOAT DeltaZ = Location.Z - Other->Location.Z;
        if (Height * Height <= DeltaZ * DeltaZ)
            return 0;
        const FLOAT Radius = Other->CollisionRadius + CollisionRadius;
        const FLOAT DeltaY = Location.Y - Other->Location.Y;
        const FLOAT DeltaX = Location.X - Other->Location.X;
        return Radius * Radius > DeltaY * DeltaY + DeltaX * DeltaX;
    }

    AActor* Target = Checker == this ? Other : this;
    FCheckResult LocalHit(1.f);
    if (!Hit)
        Hit = &LocalHit;
    const FVector Extent = Target->GetPrimitive()->GetEncroachExtent(Target);
    const FVector Center = Target->GetPrimitive()->GetEncroachCenter(Target);
    return Checker->GetPrimitive()->PointCheck(*Hit, Checker, Center, Extent, 0) == 0;
}

namespace
{
    // adds Other to the actor's touch list and sends Touch; whether Other is still touching afterwards
    UBOOL AddTouch(AActor* Actor, AActor* Other)
    {
        INT Index = 0;
        if (Actor->Touching.FindItem(Other, Index))
            return 1;
        Actor->Touching.AddItem(Other);
        Actor->eventTouch(Other);
        Index = 0;
        return Actor->Touching.FindItem(Other, Index);
    }
}

void AActor::BeginTouch(AActor* Other)
{
    if (AddTouch(this, Other))
        AddTouch(Other, this);
}

void AActor::EndTouch(AActor* Other, UBOOL bNoNotifySelf)
{
    INT Index = 0;
    if (!bNoNotifySelf && Touching.FindItem(Other, Index))
        eventUnTouch(Other);
    Touching.RemoveItem(Other);
    if (Other->Touching.FindItem(this, Index))
    {
        Other->eventUnTouch(this);
        Other->Touching.RemoveItem(this);
    }
}

UBOOL AActor::moveSmooth(const FVector& Delta)
{
    NOT_DECOMPILED(0x001109e0);
    return 0;
}

FMatrix AActor::LocalToWorld() const
{
    return FTranslationMatrix(FVector(0.f - PrePivot.X, 0.f - PrePivot.Y, 0.f - PrePivot.Z))
        * FScaleMatrix(DrawScale3D * DrawScale) * FRotationTranslationMatrix(Rotation, Location);
}

FMatrix AActor::WorldToLocal() const
{
    const FLOAT InvDrawScale = 1.f / DrawScale;
    const FVector InvScale((1.f / DrawScale3D.X) * InvDrawScale, (1.f / DrawScale3D.Y) * InvDrawScale, (1.f / DrawScale3D.Z) * InvDrawScale);
    return FTranslationMatrix(FVector(0.f - Location.X, 0.f - Location.Y, 0.f - Location.Z)) * FInverseRotationMatrix(Rotation)
        * FScaleMatrix(InvScale) * FTranslationMatrix(PrePivot);
}

UBOOL AActor::ShouldTrace(AActor* SourceActor, DWORD TraceFlags)
{
    // trace flag 0x100 is for shadows
    if (Level && this != Level && !bShadowCast && (TraceFlags & 0x100))
        return 0;
    if ((SourceActor && SourceActor->bOnlyAffectPawns) || (bOnlyAffectPawns && SourceActor && !SourceActor->IsAPawn()))
        return 0;
    if (TraceFlags & 0x8000)
        return bAcceptsProjectors;
    // DrawType 8 is a static mesh, 2 a mesh
    if (((TraceFlags & 0x2000) && ((StaticMesh && DrawType == 8) || (Mesh && DrawType == 2))) || ((TraceFlags & 0x20000) && bImmobile))
        return 1;
    if (TraceFlags & 0x100)
        return bShadowCast;
    if (bWorldGeometry)
        return TraceFlags & 0x80;
    if (!(TraceFlags & 0x10))
        return 0;
    if (TraceFlags & 0x20)
        return bProjTarget || (bBlockActors && bBlockPlayers);
    if (!(TraceFlags & 0x40))
        return 1;
    if (!SourceActor)
        return 0;
    return SourceActor->IsBlockedBy(this);
}

UBOOL AVolume::ShouldTrace(AActor* SourceActor, DWORD TraceFlags)
{
    if (!bShadowCast && (TraceFlags & 0x100))
        return 0;
    if (SourceActor && SourceActor->bOnlyAffectPawns)
        return 0;
    if (bOnlyAffectPawns && SourceActor && !SourceActor->IsAPawn())
        return 0;
    const UBOOL bSourceIsPlayer = SourceActor && (SourceActor->IsAPlayerController() || SourceActor->GetPlayerPawn());
    if (bWorldGeometry && (bSourceIsPlayer ? bBlockPlayers : bBlockActors) && (TraceFlags & 0x80))
        return 1;
    // volumes need trace flag 8
    if (!(TraceFlags & 0x8))
        return 0;
    if (TraceFlags & 0x20)
        return bProjTarget || (bBlockActors && bBlockPlayers);
    if (!(TraceFlags & 0x40))
        return 1;
    if (!SourceActor)
        return 0;
    return SourceActor->IsBlockedBy(this);
}

UBOOL AVolume::Encompasses(FVector Point)
{
    NOT_DECOMPILED(0x0011213e);
    return 0;
}

APawn* APawn::GetPlayerPawn() const
{
    if (Controller && Controller->IsAPlayerController() && static_cast<APlayerController*>(Controller)->Player)
        return const_cast<APawn*>(this);
    return nullptr;
}

UBOOL APawn::ShouldTrace(AActor* SourceActor, DWORD TraceFlags)
{
    NOT_DECOMPILED(0x000eb755);
    return 0;
}

UBOOL AMover::ShouldTrace(AActor* SourceActor, DWORD TraceFlags)
{
    NOT_DECOMPILED(0x0011b54e);
    return 0;
}

UBOOL AProjector::ShouldTrace(AActor* SourceActor, DWORD TraceFlags)
{
    NOT_DECOMPILED(0x000764ad);
    return 0;
}

UBOOL ATurret::ShouldTrace(AActor* SourceActor, DWORD TraceFlags)
{
    NOT_DECOMPILED(0x0010559e);
    return 0;
}

void AActor::SetVolumes()
{
    NOT_DECOMPILED(0x0012cf50);
}

void AActor::Unknown14C()
{
    if (!Mesh)
        return;
    for (INT i = 0; i < 6; i++)
    {
        if (SimAnim[i].AnimSequence != NAME_None)
            PlayReplicatedAnim(i);
    }
}

void AActor::PlayReplicatedAnim(INT Channel)
{
    NOT_DECOMPILED(0x0010b687);
}

void AActor::SetBase(AActor* NewBase, const FVector& NewFloor, UBOOL bNotifyActor)
{
    NOT_DECOMPILED(0x0010c9c0);
}

void AActor::FindBase()
{
    NOT_DECOMPILED(0x00110e8e);
}

APhysicsVolume* ALevelInfo::GetPhysicsVolume(const FVector& Loc, AActor* A, UBOOL bUseTouch)
{
    APhysicsVolume* NewVolume = GetDefaultPhysicsVolume();
    // an actor attached to a bone of a skeletal mesh takes its base's volume
    if (A->Base && A->AttachmentBone != NAME_None && Cast<USkeletalMesh>(A->Base->Mesh))
        return A->Base->CurrentPhysicsVolume ? A->Base->CurrentPhysicsVolume : NewVolume;

    if (bUseTouch)
    {
        for (INT i = 0; i < A->Touching.Num(); i++)
        {
            APhysicsVolume* Volume = Cast<APhysicsVolume>(A->Touching[i]);
            if (Volume && Volume->Priority > NewVolume->Priority && Volume->Encompasses(Loc))
                NewVolume = Volume;
        }
    }
    else if (XLevel->Hash)
    {
        // trace flag 8 finds volumes
        FMemMark Mark(GMem);
        for (FCheckResult* Link = XLevel->Hash->ActorPointCheck(GMem, Loc, FVector(0.f, 0.f, 0.f), 8, 0, 0); Link; Link = Link->GetNext())
        {
            APhysicsVolume* Volume = Cast<APhysicsVolume>(Link->Actor);
            if (Volume && Volume->Priority > NewVolume->Priority)
                NewVolume = Volume;
        }
        Mark.Pop();
    }
    return NewVolume;
}

APhysicsVolume* ALevelInfo::GetDefaultPhysicsVolume()
{
    if (!CurrentPhysicsVolume)
    {
        CurrentPhysicsVolume = Cast<APhysicsVolume>(XLevel->SpawnActor(ADefaultPhysicsVolume::StaticClass()));
        CurrentPhysicsVolume->Priority = -1000000;
        CurrentPhysicsVolume->bNoDelete = 1;
    }
    return CurrentPhysicsVolume;
}

// ---------------------------------------------------------------------------------------------- natives

void AActor::execSpawn(FFrame& Stack, RESULT_DECL)
{
    P_GET_OBJECT(UClass, SpawnClass);
    P_GET_OBJECT(AActor, SpawnOwner);
    P_GET_NAME(SpawnTag);
    // location and rotation default to the spawner's
    FVector SpawnLocation = Location;
    Stack.Step(Stack.Object, &SpawnLocation);
    FRotator SpawnRotation = Rotation;
    Stack.Step(Stack.Object, &SpawnRotation);
    P_FINISH;

    AActor* Spawned = SpawnClass
        ? XLevel->SpawnActor(SpawnClass, NAME_None, SpawnLocation, SpawnRotation, nullptr, 0, 0, SpawnOwner, Instigator)
        : nullptr;
    if (Spawned && SpawnTag != NAME_None)
        Spawned->Tag = SpawnTag;
    *static_cast<AActor**>(Result) = Spawned;
}

void AActor::execAllActors(FFrame& Stack, RESULT_DECL)
{
    P_GET_OBJECT(UClass, BaseClass);
    P_GET_OBJECT_REF(AActor, OutActor);
    P_GET_NAME(MatchTag);
    P_FINISH;

    if (!BaseClass)
        BaseClass = AActor::StaticClass();
    INT iActor = 0;
    PRE_ITERATOR;
    for (;;)
    {
        *OutActor = nullptr;
        while (iActor < XLevel->Actors.Num() && !*OutActor)
        {
            AActor* TestActor = XLevel->Actors[iActor++];
            if (TestActor && !TestActor->bDeleteMe && TestActor->IsA(BaseClass) && (MatchTag == NAME_None || TestActor->Tag == MatchTag))
                *OutActor = TestActor;
        }
        if (!*OutActor)
            break;
        POST_ITERATOR;
    }
    END_ITERATOR;
}

// ---------------------------------------------------------------------------------------------- ticking

UBOOL AActor::Tick(FLOAT DeltaSeconds, ELevelTick TickType)
{
    // an actor in stasis rests while nobody has seen it for five seconds in a standalone game (physics 0 none,
    // 5 rotating)
    if (bStasis && (Physics == 0 || Physics == 5) && Level->TimeSeconds - LastRenderTime > 5.f && Level->NetMode == 0)
    {
        bTicked = XLevel->Ticked;
        return 1;
    }

    // the owner and base move first
    if (Owner && !Owner->bDeleteMe && Owner->bTicked != static_cast<UBOOL>(Owner->XLevel->Ticked))
        Owner->Tick(DeltaSeconds, TickType);
    if (Base && !Base->bDeleteMe && Base->bTicked != static_cast<UBOOL>(Base->XLevel->Ticked))
        Base->Tick(DeltaSeconds, TickType);
    bTicked = XLevel->Ticked;

    if (TickType == LEVELTICK_ViewportsOnly && !PlayerControlled())
        return 1;
    UpdateAnimation(DeltaSeconds);

    // roles: 2 simulated proxy, 3 autonomous proxy. physics 2 is falling, 13 karma.
    UBOOL bAuthoritative = 1;
    if (RemoteRole == 3)
    {
        // an actor owned by a remote player runs its script here, but its physics come from the client
        APlayerController* TopPC = GetTopPlayerController();
        if ((!TopPC || !TopPC->LocalPlayerController()) && Physics != 13)
        {
            eventTick(DeltaSeconds);
            ProcessState(DeltaSeconds);
            UpdateTimers(DeltaSeconds);
            bAuthoritative = 0;
        }
    }
    else if (Role < 3)
    {
        if (Role == 2)
        {
            TickSimulated(DeltaSeconds);
        }
        else
        {
            if (bDeleteMe)
                return 1;
            if (Physics == 2 || Physics == 5)
                performPhysics(DeltaSeconds);
        }
        bAuthoritative = 0;
    }
    if (bAuthoritative)
        TickAuthoritative(DeltaSeconds);

    if (!bDeleteMe)
    {
        TickSpecial(DeltaSeconds);
        if (MeshInstance)
            NOT_DECOMPILED(0x00132a9f);
    }
    return 1;
}

void AActor::ProcessState(FLOAT DeltaSeconds)
{
    // state code runs on the authority, and on simulated proxies when the state allows it (role 4 is authority)
    FStateFrame* Frame = GetStateFrame();
    if (!Frame || !Frame->Code || (Role < 4 && !(Frame->StateNode->StateFlags & STATE_Simulated)) || IsPendingKill())
        return;

    UState* OldStateNode = Frame->StateNode;
    if (Frame->LatentAction)
        (this->*GNatives[Frame->LatentAction])(*Frame, &DeltaSeconds);
    if (bDeleteMe)
        return;

    // run until the code ends or waits; give up after the fifth state change in one tick
    INT NumStateChanges = 0;
    while (GetStateFrame()->Code && !GetStateFrame()->LatentAction)
    {
        BYTE Buffer[16];
        GetStateFrame()->Step(this, Buffer);
        if (GetStateFrame()->StateNode != OldStateNode)
        {
            OldStateNode = GetStateFrame()->StateNode;
            if (++NumStateChanges > 4)
                return;
        }
        if (bDeleteMe)
            return;
    }
}

void AActor::UpdateAnimation(FLOAT DeltaSeconds)
{
    if (Mesh)
        NOT_DECOMPILED(0x00105279);
}

void AActor::TickAuthoritative(FLOAT DeltaSeconds)
{
    eventTick(DeltaSeconds);
    ProcessState(DeltaSeconds);
    UpdateTimers(DeltaSeconds);

    // the actor is destroyed when its life span runs out
    if (LifeSpan != 0.f)
    {
        LifeSpan -= DeltaSeconds;
        if (LifeSpan <= 0.0001f)
        {
            XLevel->DestroyActor(this);
            return;
        }
    }
    // physics 0 is none; an autonomous proxy (role 3) moves itself
    if (!bDeleteMe && Physics != 0 && Role != 3)
        performPhysics(DeltaSeconds);
}

void AActor::performPhysics(FLOAT DeltaSeconds)
{
    NOT_DECOMPILED(0x00116acc);
}

APlayerController* AActor::GetTopPlayerController()
{
    AActor* Top = this;
    while (Top->Owner)
        Top = Top->Owner;
    return Top->IsAPlayerController() ? Cast<APlayerController>(Top) : nullptr;
}

void AActor::UpdateTimers(FLOAT DeltaSeconds)
{
    if (TimerRate <= 0.f)
        return;
    TimerCounter += DeltaSeconds;
    if (TimerCounter < TimerRate)
        return;
    // a timer that doesn't loop stops after firing; several elapsed periods still fire once
    const INT Elapsed = static_cast<INT>(TimerCounter / TimerRate);
    TimerCounter -= static_cast<FLOAT>(Elapsed) * TimerRate;
    if (Elapsed && !bTimerLoop)
        TimerRate = 0.f;
    eventTimer();
}

UBOOL APlayerController::LocalPlayerController()
{
    return Player && Player->IsA(UViewport::StaticClass());
}

void AActor::execSleep(FFrame& Stack, RESULT_DECL)
{
    P_GET_FLOAT(Seconds);
    P_FINISH;
    GetStateFrame()->LatentAction = EPOLL_Sleep;
    LatentFloat = Seconds;
}

void AActor::execPollSleep(FFrame& Stack, RESULT_DECL)
{
    // ends when less than half a tick is left
    const FLOAT DeltaSeconds = *static_cast<FLOAT*>(Result);
    LatentFloat -= DeltaSeconds;
    if (LatentFloat < DeltaSeconds * 0.5f)
        GetStateFrame()->LatentAction = 0;
}
IMPLEMENT_FUNCTION(AActor, EPOLL_Sleep, execPollSleep)

void AActor::execSetTimer(FFrame& Stack, RESULT_DECL)
{
    P_GET_FLOAT(NewTimerRate);
    P_GET_UBOOL(bLoop);
    P_FINISH;
    TimerCounter = 0.f;
    bTimerLoop = bLoop;
    TimerRate = NewTimerRate;
}
