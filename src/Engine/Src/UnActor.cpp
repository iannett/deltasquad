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

void AActor::SetZone(UBOOL bTest, UBOOL bForceRefresh)
{
    if (bDeleteMe || !Level)
        return;
    if (bForceRefresh)
    {
        Region.Zone = Level;
        Region.iLeaf = INDEX_NONE;
        Region.ZoneNumber = 0;
    }

    // zone events only when not testing
    const FPointRegion NewRegion = XLevel->Model->PointRegion(Level, Location);
    if (NewRegion.Zone == Region.Zone)
    {
        Region = NewRegion;
    }
    else
    {
        if (!bTest)
        {
            Region.Zone->eventActorLeaving(this);
            eventZoneChange(NewRegion.Zone);
        }
        Region = NewRegion;
        if (!bTest)
            Region.Zone->eventActorEntered(this);
    }

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

UBOOL AActor::IsEncroacher()
{
    if (!bCollideActors)
        return 0;
    // physics 14 is ragdoll, 13 is karma
    return (StaticMesh && IsAMover()) || Physics == 14 || (Physics == 13 && !IsA(AKConstraint::StaticClass()));
}

namespace
{
    // the end both ShouldTrace versions share: whether a blocking trace from Source stops at This
    UBOOL ShouldTraceBlocking(AActor* This, AActor* Source)
    {
        const UBOOL bSourceIsPlayer = Source->IsAPlayerController() || Source->GetPlayerPawn();
        const UBOOL bBlocksSource = bSourceIsPlayer ? This->bBlockPlayers : This->bBlockActors;
        // never reached: world geometry returns before this in both callers
        if (This->bWorldGeometry && bBlocksSource)
            return Source->bCollideWorld;

        // an actor ignoring encroachers lets movers and karma bodies through, both ways round
        if (Source->IsEncroacher() && This->bIgnoreEncroachers)
            return 0;
        if (This->IsEncroacher() && Source->bIgnoreEncroachers)
            return 0;

        // brushes and encroachers block what collides with the world; movers let player controllers through
        if ((This->Brush && This->IsABrush()) || This->IsEncroacher())
        {
            if (This->StaticMesh && This->IsAMover() && Source->IsAPlayerController())
                return 0;
            return Source->bCollideWorld && bBlocksSource;
        }
        if ((Source->Brush && Source->IsABrush()) || Source->IsEncroacher())
        {
            if ((Source->StaticMesh && Source->IsAMover() && Source->IsAPlayerController()) || !This->bCollideWorld)
                return 0;
            return bSourceIsPlayer ? Source->bBlockPlayers : Source->bBlockActors;
        }
        if (!bBlocksSource)
            return 0;
        return This->GetPlayerPawn() ? Source->bBlockPlayers : Source->bBlockActors;
    }
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
    return ShouldTraceBlocking(this, SourceActor);
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
    return ShouldTraceBlocking(this, SourceActor);
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
    NOT_DECOMPILED(0x00128721);
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
