#include "EngineClasses.h"

void KSetActorListed(AActor* Actor, UBOOL bListed);

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

// 0x0007e27c
McdModel* AActor::getKModel() const
{
    return KParams ? KParams->KarmaData : nullptr;
}

// 0x0010b947
void AActor::Serialize(FArchive& Ar)
{
    // tagged properties only hold LightRadius when it differs from the defaults, so a negative marker shows whether
    // it was loaded
    const FLOAT OldLightRadius = LightRadius;
    if (Ar.IsLoading())
        LightRadius = -1.f;
    if (Ar.IsSaving() && Cast<ULodMeshInstance>(MeshInstance))
        NOT_DECOMPILED(0x00108ef0); // stores the mesh instance's animation channels in SimAnim
    Super::Serialize(Ar);
    if (!Ar.IsLoading())
    {
        if (!Ar.IsSaving())
            Ar << Projectors << StaticMeshProjectors;
    }
    else
    {
        if (LightRadius < 0.f)
            LightRadius = OldLightRadius;
        else if (Ar.Ver() < 129)
            LightRadius *= 25.f;
        // only LE_StaticSpot and LE_Spotlight keep their cone from older packages
        if (Ar.Ver() < 129 && LightEffect != 12 && LightEffect != 8)
            LightCone = 0;
    }
    if (ActorRenderData)
        Ar.CountBytes(ActorRenderData, 0x144);
    if (LightRenderData)
        Ar.CountBytes(LightRenderData, 0x4c);
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

void AActor::UpdateRelativeRotation()
{
    if (!Base || Base->bWorldGeometry || AttachmentBone != NAME_None)
        return;
    const FMatrix BaseMatrix = FRotationTranslationMatrix(Base->Rotation, FVector(0.f, 0.f, 0.f));
    FMatrix InvBase;
    for (INT i = 0; i < 4; i++)
        for (INT j = 0; j < 4; j++)
            InvBase.M[i][j] = BaseMatrix.M[j][i];
    RelativeRotation = (FRotationTranslationMatrix(Rotation, FVector(0.f, 0.f, 0.f)) * InvBase).Rotator();
}

UBOOL AActor::Move(const FVector& Delta)
{
    FCheckResult Hit(1.f);
    return XLevel->MoveActor(this, Delta, Rotation, Hit);
}

// the xbox builds the coordinates one rotation axis at a time; the rotation matrix gives the same axes
FCoords AActor::ToWorld() const
{
    const FMatrix M = FRotationTranslationMatrix(Rotation, FVector(0.f, 0.f, 0.f));
    const FVector Origin(-(M.M[0][0] * Location.X + M.M[0][1] * Location.Y + M.M[0][2] * Location.Z),
        -(M.M[1][0] * Location.X + M.M[1][1] * Location.Y + M.M[1][2] * Location.Z),
        -(M.M[2][0] * Location.X + M.M[2][1] * Location.Y + M.M[2][2] * Location.Z));
    return FCoords(Origin, FVector(M.M[0][0], M.M[1][0], M.M[2][0]), FVector(M.M[0][1], M.M[1][1], M.M[2][1]),
        FVector(M.M[0][2], M.M[1][2], M.M[2][2]));
}

UBOOL AActor::SetRotation(const FRotator& NewRotation)
{
    if (NewRotation == Rotation)
        return 1;
    FCheckResult Hit(1.f);
    return XLevel->MoveActor(this, FVector(0.f, 0.f, 0.f), NewRotation, Hit);
}

void APawn::NotifyBumpEx(AActor* Other)
{
    // the xbox posts an AI stimulus and asks the controller's NotifyBump first
    if (Controller)
        NOT_DECOMPILED(0x0012c069);
    eventBump(Other);
}

UPrimitive* ABrush::GetPrimitive()
{
    if (Brush)
        return Brush;
    return XLevel->Engine ? XLevel->Engine->Cylinder : nullptr;
}

UPrimitive* AFluidSurfaceInfo::GetPrimitive()
{
    if (!Primitive)
    {
        Primitive = static_cast<UFluidSurfacePrimitive*>(
            StaticConstructObject(UFluidSurfacePrimitive::StaticClass(), GetOuter(), NAME_None, 0, nullptr, GError));
        Primitive->Info = this;
    }
    return Primitive;
}

namespace
{
    // 0x00461d20. one primitive shared by every projector.
    UPrimitive* GProjectorPrimitive;
}

UPrimitive* AProjector::GetPrimitive()
{
    if (!GProjectorPrimitive)
        GProjectorPrimitive = static_cast<UPrimitive*>(StaticConstructObject(UProjectorPrimitive::StaticClass(), ANY_PACKAGE, NAME_None, 0, nullptr, GError));
    return GProjectorPrimitive;
}

UPrimitive* ATerrainInfo::GetPrimitive()
{
    if (!Sectors.Num())
        return AActor::GetPrimitive();
    if (!Primitive)
        Primitive = new(this) UTerrainPrimitive(this);
    return Primitive;
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
    if ((!bShadowCast && (TraceFlags & 0x100)) || (bOnlyAffectPawns && SourceActor && !SourceActor->IsAPawn()))
        return 0;
    if (TraceFlags & 0x8000)
        return bAcceptsProjectors;
    if ((TraceFlags & 0x2000) && StaticMesh && DrawType == 8)
        return 1;
    // pawns need trace flag 1
    return TraceFlags & 0x1;
}

void APawn::PostScriptDestroyed()
{
    NOT_DECOMPILED(0x00179c7f);
}

// movers need trace flag 2
UBOOL AMover::ShouldTrace(AActor* SourceActor, DWORD TraceFlags)
{
    return TraceFlags & 0x2;
}

UBOOL AProjector::ShouldTrace(AActor* SourceActor, DWORD TraceFlags)
{
    if (TraceFlags & 0x4000)
        return 1;
    return AActor::ShouldTrace(SourceActor, TraceFlags);
}

void AProjector::Attach()
{
    // the xbox first checks a render global at 0x00460af0, which is null without a render device
    if (GetProjTexture() && Level->NetMode != 1)
        NOT_DECOMPILED(0x0007b7fc);
}

void AProjector::execAttachProjector(FFrame& Stack, RESULT_DECL)
{
    P_FINISH;
    Attach();
}

UBOOL ATurret::ShouldTrace(AActor* SourceActor, DWORD TraceFlags)
{
    if (TraceFlags & 0x10000)
        return 1;
    return APawn::ShouldTrace(SourceActor, TraceFlags);
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
    if (!Mesh)
        return;
    const FAnimRep& Rep = SimAnim[Channel];
    FName Bone = NAME_None;
    if (USkeletalMesh* SkelMesh = Cast<USkeletalMesh>(Mesh))
    {
        if (Rep.Bone < SkelMesh->RefSkeleton.Num())
            Bone = SkelMesh->RefSkeleton[Rep.Bone].Name;
    }
    Mesh->MeshGetInstance(this);
    FPlayAnim Anim;
    Anim.Sequence = Rep.AnimSequence;
    Anim.bLoop = Rep.Priority >> 7;
    Anim.Bone = Bone;
    Anim.Channel = Rep.Priority & 0x7f;
    Anim.Rate = (Rep.RateScale - 124.f) * (1.f / 31.f);
    Anim.Frame = (Rep.AnimFrame - 127.f) * (1.f / 127.f);
    MeshInstance->PlayAnim(Anim);
}

void AActor::ReplicateAnim(INT Index, INT Channel, const FAnimChannel& Anim, UBOOL bCheckChanged)
{
    // NM_Standalone is 0
    if (Index >= 6 || (bCheckChanged && (!bReplicateAnimations || Level->NetMode == 0 || Anim.Channel >= 3)))
        return;
    FName Sequence = NAME_None;
    if (ULodMeshInstance* Instance = Cast<ULodMeshInstance>(MeshInstance))
    {
        if (const FMeshAnimSeq* Seq = Instance->GetSequence(Channel))
            Sequence = Seq->Name;
    }
    // the rate is kept in -4..4 and the frame in -1..1, each as a byte
    FAnimRep Rep;
    Rep.AnimSequence = Sequence;
    Rep.Bone = static_cast<BYTE>(Anim.StartBone);
    Rep.Priority = (Anim.bLoop ? 0x80 : 0) | static_cast<BYTE>(Anim.Channel);
    const FLOAT Rate = Anim.LastFrame == 0.f ? 124.f : (Clamp(Anim.Rate, -4.f, 4.f) + 4.f) * 31.f;
    Rep.RateScale = static_cast<BYTE>(static_cast<INT>(Rate));
    Rep.AnimFrame = static_cast<BYTE>(static_cast<INT>((Clamp(Anim.Frame, -1.f, 1.f) + 1.f) * 127.f));
    if (bCheckChanged)
    {
        const FAnimRep& Old = SimAnim[Index];
        if (Old.AnimSequence == Rep.AnimSequence && Old.Bone == Rep.Bone && Old.Priority == Rep.Priority && Old.RateScale == Rep.RateScale)
            return;
        bNetDirty = 1;
    }
    SimAnim[Index] = Rep;
}

void AActor::PlayAnim(const FPlayAnim& Anim)
{
    if (!Mesh)
    {
        if (!bHidden)
            GLog->Logf(TEXT("%s playanim with no mesh!"), GetName());
        return;
    }
    Mesh->MeshGetInstance(this);
    MeshInstance->PlayAnim(Anim);
}

void APawn::NotifyAnimEnd(INT Channel)
{
    if (Controller && Controller->IsProbing(FName(NAME_AnimEnd)))
        Controller->AActor::NotifyAnimEnd(Channel);
    else
        AActor::NotifyAnimEnd(Channel);
}

void AActor::StartAnimPoll()
{
    if (!Mesh)
        return;
    Mesh->MeshGetInstance(this);
    const INT Channel = static_cast<INT>(LatentFloat);
    if (MeshInstance->IsAnimLooping(Channel))
        MeshInstance->AnimStopLooping(Channel);
    // EPOLL_FinishAnim
    if (IsAnimating(Channel) && !MeshInstance->IsAnimPastLastFrame(Channel))
        GetStateFrame()->LatentAction = EPOLL_FinishAnim;
}

UBOOL AActor::CheckAnimFinished(INT Channel)
{
    if (Mesh)
    {
        Mesh->MeshGetInstance(this);
        if (IsAnimating(Channel) && !MeshInstance->IsAnimPastLastFrame(Channel))
            return 0;
    }
    return 1;
}

void AController::StartAnimPoll()
{
    if (!Pawn || !Pawn->Mesh)
        return;
    Pawn->Mesh->MeshGetInstance(this);
    const INT Channel = static_cast<INT>(LatentFloat);
    if (Pawn->MeshInstance->IsAnimLooping(Channel))
        Pawn->MeshInstance->AnimStopLooping(Channel);
    if (IsAnimating(Channel) && !Pawn->MeshInstance->IsAnimPastLastFrame(Channel))
        GetStateFrame()->LatentAction = EPOLL_FinishAnim;
}

UBOOL AController::CheckAnimFinished(INT Channel)
{
    if (Pawn && Pawn->Mesh)
    {
        Pawn->Mesh->MeshGetInstance(this);
        if (IsAnimating(Channel) && !Pawn->MeshInstance->IsAnimPastLastFrame(Channel))
            return 0;
    }
    return 1;
}

UBOOL AActor::IsAnimating(INT Channel)
{
    // DT_Mesh
    if (DrawType != 2 || !Mesh)
        return 0;
    Mesh->MeshGetInstance(this);
    return MeshInstance->IsAnimating(Channel);
}

void AActor::StopAnimating(UBOOL ClearAllButBase)
{
    if (!Mesh)
        return;
    Mesh->MeshGetInstance(this);
    if (MeshInstance)
        MeshInstance->StopAnimating(ClearAllButBase);
}

UBOOL AActor::IsPlayingAnim(const FName& Sequence, const FName& Bone)
{
    if (!Mesh)
        return 0;
    Mesh->MeshGetInstance(this);
    return MeshInstance->IsPlayingAnim(Sequence, Bone);
}

// 0x000e5587
void AActor::execPlayAnim(FFrame& Stack, RESULT_DECL)
{
    P_GET_NAME(Sequence);
    P_GET_NAME_OPTX(Bone, NAME_None);
    P_GET_FLOAT_OPTX(Rate, 1.f);
    P_GET_FLOAT_OPTX(StartFrame, -1.f);
    P_GET_INT_OPTX(Channel, 0);
    P_FINISH;
    FPlayAnim Anim;
    Anim.Sequence = Sequence;
    Anim.Bone = Bone;
    Anim.Channel = Channel;
    Anim.Rate = Rate;
    Anim.Frame = StartFrame;
    PlayAnim(Anim);
}

// 0x000e89f7
void AActor::execLoopAnim(FFrame& Stack, RESULT_DECL)
{
    P_GET_NAME(Sequence);
    P_GET_NAME_OPTX(Bone, NAME_None);
    P_GET_FLOAT_OPTX(Rate, 1.f);
    P_GET_FLOAT_OPTX(StartFrame, -1.f);
    P_GET_INT_OPTX(Channel, 0);
    P_FINISH;
    FPlayAnim Anim;
    Anim.Sequence = Sequence;
    Anim.bLoop = 1;
    Anim.Bone = Bone;
    Anim.Channel = Channel;
    Anim.Rate = Rate;
    Anim.Frame = StartFrame;
    PlayAnim(Anim);
}

// 0x000df76e
void AActor::execIsAnimating(FFrame& Stack, RESULT_DECL)
{
    P_GET_INT_OPTX(Channel, 0);
    P_FINISH;
    *static_cast<DWORD*>(Result) = IsAnimating(Channel);
}

// 0x000df52f
void AActor::execFinishAnim(FFrame& Stack, RESULT_DECL)
{
    P_GET_INT_OPTX(Channel, 0);
    P_FINISH;
    LatentFloat = static_cast<FLOAT>(Channel);
    StartAnimPoll();
}

// 0x000df737
void AActor::execPollFinishAnim(FFrame& Stack, RESULT_DECL)
{
    if (CheckAnimFinished(static_cast<INT>(LatentFloat)))
        GetStateFrame()->LatentAction = 0;
}
IMPLEMENT_FUNCTION(AActor, EPOLL_FinishAnim, execPollFinishAnim)

// 0x000df7c6
void AActor::execStopAnimating(FFrame& Stack, RESULT_DECL)
{
    P_GET_UBOOL_OPTX(ClearAllButBase, 0);
    P_FINISH;
    StopAnimating(ClearAllButBase);
}

// 0x000e56f2
void AActor::execIsPlayingAnim(FFrame& Stack, RESULT_DECL)
{
    P_GET_NAME(Sequence);
    P_GET_NAME_OPTX(Bone, NAME_None);
    P_FINISH;
    *static_cast<DWORD*>(Result) = IsPlayingAnim(Sequence, Bone);
}

// world geometry bases don't track what stands on them
void AActor::SetBase(AActor* NewBase, const FVector& NewFloor, UBOOL bNotifyActor)
{
    for (AActor* A = NewBase; A; A = A->Base)
        if (A == this)
            return;

    if (NewBase != Base)
    {
        if (Base && !Base->bWorldGeometry)
        {
            Base->Attached.RemoveItem(this);
            Base->eventDetach(this);
        }
        Base = NewBase;
        // ROLE_Authority
        if (Base && !Base->bWorldGeometry && AttachmentBone == NAME_None && !bOnlyDrawIfAttached && Role == 4)
        {
            RelativeLocation = Location - Base->Location;
            UpdateRelativeRotation();
        }
        if (Base && !Base->bWorldGeometry)
        {
            Base->Attached.AddItem(this);
            Base->eventAttach(this);
        }
        if (bNotifyActor)
            eventBaseChange();
    }

    // NM_Client
    if ((!Base || Base->bWorldGeometry) && Level->NetMode != 3)
        AttachmentBone = NAME_None;
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

UBOOL ALevelInfo::IsSplitScreen() const
{
    if (!XLevel || !XLevel->Engine)
        return 0;
    UGameEngine* GameEngine = Cast<UGameEngine>(XLevel->Engine);
    // port: a dedicated server has no client; the xbox reads it unchecked
    if (!GameEngine->Client)
        return 0;
    // the client's viewport count
    NOT_DECOMPILED(0x000a69cb);
    return 0;
}

UBOOL ALevelInfo::IsSystemLink()
{
    UGameEngine* GameEngine = CastChecked<UGameEngine>(XLevel->Engine);
    return GameEngine && GameEngine->LastURL.HasOption(TEXT("LAN"));
}

void ALevelInfo::execIsSplitScreen(FFrame& Stack, RESULT_DECL)
{
    P_FINISH;
    *static_cast<DWORD*>(Result) = IsSplitScreen();
}

void ALevelInfo::execIsEntry(FFrame& Stack, RESULT_DECL)
{
    P_FINISH;
    *static_cast<DWORD*>(Result) = 0;
    if (!XLevel || !XLevel->Engine)
        return;
    UGameEngine* GameEngine = Cast<UGameEngine>(XLevel->Engine);
    if (GameEngine && GameEngine->GLevel == GameEngine->GEntry)
        *static_cast<DWORD*>(Result) = 1;
}

void ALevelInfo::execIsDedicatedServer(FFrame& Stack, RESULT_DECL)
{
    P_FINISH;
    *static_cast<DWORD*>(Result) = 0;
    if (!XLevel || !XLevel->Engine)
        return;
    UGameEngine* GameEngine = Cast<UGameEngine>(XLevel->Engine);
    if (GameEngine)
        *static_cast<DWORD*>(Result) = GameEngine->GLevel->IsDedicatedServer();
}

void ALevelInfo::execIsSystemLink(FFrame& Stack, RESULT_DECL)
{
    P_FINISH;
    *static_cast<DWORD*>(Result) = IsSystemLink();
}

void ALevelInfo::execIsDemoBuild(FFrame& Stack, RESULT_DECL)
{
    P_FINISH;
    *static_cast<DWORD*>(Result) = 0;
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

// the actors after the static ones whose tag or hidden tag matches
void AActor::execDynamicActors(FFrame& Stack, RESULT_DECL)
{
    P_GET_OBJECT(UClass, BaseClass);
    P_GET_OBJECT_REF(AActor, OutActor);
    P_GET_NAME(MatchTag);
    P_FINISH;

    if (!BaseClass)
        BaseClass = AActor::StaticClass();
    INT iActor = XLevel->iFirstDynamicActor;
    PRE_ITERATOR;
    for (;;)
    {
        *OutActor = nullptr;
        while (iActor < XLevel->Actors.Num() && !*OutActor)
        {
            AActor* TestActor = XLevel->Actors[iActor++];
            if (TestActor && !TestActor->bDeleteMe && TestActor->IsA(BaseClass)
                && (MatchTag == NAME_None || TestActor->Tag == MatchTag || TestActor->HiddenTag == MatchTag))
                *OutActor = TestActor;
        }
        if (!*OutActor)
            break;
        POST_ITERATOR;
    }
    END_ITERATOR;
}

// the actors this one owns, directly or further down; includes itself when it is a BaseClass
void AActor::execChildActors(FFrame& Stack, RESULT_DECL)
{
    P_GET_OBJECT(UClass, BaseClass);
    P_GET_OBJECT_REF(AActor, OutActor);
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
            if (TestActor && !TestActor->bDeleteMe && TestActor->IsA(BaseClass))
            {
                for (AActor* Test = TestActor; Test; Test = Test->Owner)
                {
                    if (Test == this)
                    {
                        *OutActor = TestActor;
                        break;
                    }
                }
            }
        }
        if (!*OutActor)
            break;
        POST_ITERATOR;
    }
    END_ITERATOR;
}

void AActor::execBasedActors(FFrame& Stack, RESULT_DECL)
{
    P_GET_OBJECT(UClass, BaseClass);
    P_GET_OBJECT_REF(AActor, OutActor);
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
            if (TestActor && !TestActor->bDeleteMe && TestActor->IsA(BaseClass) && TestActor->Base == this)
                *OutActor = TestActor;
        }
        if (!*OutActor)
            break;
        POST_ITERATOR;
    }
    END_ITERATOR;
}

void AActor::execTouchingActors(FFrame& Stack, RESULT_DECL)
{
    P_GET_OBJECT(UClass, BaseClass);
    P_GET_OBJECT_REF(AActor, OutActor);
    P_FINISH;

    if (!BaseClass)
        BaseClass = AActor::StaticClass();
    INT iTouching = 0;
    PRE_ITERATOR;
    for (;;)
    {
        *OutActor = nullptr;
        while (iTouching < Touching.Num() && !*OutActor)
        {
            AActor* TestActor = Touching[iTouching++];
            if (TestActor && !TestActor->bDeleteMe && TestActor->IsA(BaseClass))
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
            MeshInstance->UpdateAttachments();
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
        Mesh->MeshGetInstance(this)->UpdateAnimation(DeltaSeconds);
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
    // rotating actors nobody has seen for a second stop
    if (Physics == 5 && !bInterpolating && Level->TimeSeconds - LastRenderTime > 1.f)
        return;

    switch (Physics)
    {
    case 2:
        physFalling(DeltaSeconds, 0);
        break;
    case 5:
        break;
    case 6:
        physProjectile(DeltaSeconds, 0);
        break;
    case 10:
        physTrailer(DeltaSeconds);
        break;
    case 13:
        physKarma(DeltaSeconds);
        break;
    case 14:
        physKarmaRagDoll(DeltaSeconds);
        break;
    default:
        physRootMotion(DeltaSeconds);
        break;
    }

    if (!RotationRate.IsZero() && !bInterpolating)
        physicsRotation(DeltaSeconds);

    if (PendingTouch)
    {
        PendingTouch->eventPostTouch(this);
        AActor* Other = PendingTouch;
        PendingTouch = Other->PendingTouch;
        Other->PendingTouch = nullptr;
    }
}

void AActor::physFalling(FLOAT DeltaTime, INT Iterations)
{
    NOT_DECOMPILED(0x00114ea1);
}

UBOOL AActor::physRootMotion(FLOAT DeltaTime)
{
    FMatrix Root;
    if (!MeshInstance || !MeshInstance->GetMoveCoords(&Root, 1))
        return 0;
    SetRotation(Root.Rotator());
    moveSmooth(FVector(Root.M[3][0], Root.M[3][1], Root.M[3][2]) - Location);
    return 1;
}

void AActor::physKarma(FLOAT DeltaTime)
{
    NOT_DECOMPILED(0x000871bc);
}

void AActor::physProjectile(FLOAT DeltaTime, INT Iterations)
{
    NOT_DECOMPILED(0x00116259);
}

void AActor::physTrailer(FLOAT DeltaTime)
{
    NOT_DECOMPILED(0x001129d0);
}

void AActor::physKarmaRagDoll(FLOAT DeltaTime)
{
    NOT_DECOMPILED(0x0008d209);
}

void AActor::physicsRotation(FLOAT DeltaTime)
{
    if (!bRotateToDesired && !bFixedRotationDir)
        return;
    if (bRotateToDesired && Rotation == DesiredRotation)
        return;

    FRotator NewRotation = Rotation;
    const FRotator DeltaRotation = RotationRate * DeltaTime;
    if (DeltaRotation.Yaw && (!bRotateToDesired || DesiredRotation.Yaw != NewRotation.Yaw))
        NewRotation.Yaw = fixedTurn(NewRotation.Yaw, DesiredRotation.Yaw, DeltaRotation.Yaw);
    if (DeltaRotation.Pitch && (!bRotateToDesired || DesiredRotation.Pitch != NewRotation.Pitch))
        NewRotation.Pitch = fixedTurn(NewRotation.Pitch, DesiredRotation.Pitch, DeltaRotation.Pitch);
    if (DeltaRotation.Roll && (!bRotateToDesired || DesiredRotation.Roll != NewRotation.Roll))
        NewRotation.Roll = fixedTurn(NewRotation.Roll, DesiredRotation.Roll, DeltaRotation.Roll);

    if (NewRotation != Rotation)
    {
        FCheckResult Hit(1.f);
        XLevel->MoveActor(this, FVector(0.f, 0.f, 0.f), NewRotation, Hit);
    }
    if (bRotateToDesired && Rotation == DesiredRotation)
        eventEndedRotation();
}

// a fixed direction turns one way (towards Desired only when rotating to it); otherwise it takes the short way
INT AActor::fixedTurn(INT Current, INT Desired, INT DeltaRate)
{
    if (DeltaRate == 0)
        return Current & 0xffff;
    Current &= 0xffff;
    Desired &= 0xffff;

    INT Result = Current;
    if (bFixedRotationDir)
    {
        if (!bRotateToDesired)
            Result = Current + DeltaRate;
        else if (DeltaRate > 0)
        {
            const INT Delta = (Current > Desired ? Desired + 0x10000 : Desired) - Current;
            Result = Current + (DeltaRate <= Delta ? DeltaRate : Delta);
        }
        else
        {
            const INT Delta = Desired - (Current < Desired ? Current + 0x10000 : Current);
            Result = Current + (DeltaRate >= Delta ? DeltaRate : Delta);
        }
    }
    else if (bRotateToDesired)
    {
        const INT Rate = Abs(DeltaRate);
        if (Current > Desired)
        {
            if (Current - Desired < 0x8000)
                Result = Current - Min(Current - Desired, Rate);
            else
                Result = Current + Min(Desired - Current + 0x10000, Rate);
        }
        else
        {
            if (Desired - Current < 0x8000)
                Result = Current + Min(Desired - Current, Rate);
            else
                Result = Current - Min(Current - Desired + 0x10000, Rate);
        }
    }
    return Result & 0xffff;
}

void APawn::physFalling(FLOAT DeltaTime, INT Iterations)
{
    NOT_DECOMPILED(0x0011a057);
}

UBOOL APawn::physRootMotion(FLOAT DeltaTime)
{
    NOT_DECOMPILED(0x00112cfd);
    return 0;
}

void AKConstraint::physKarma(FLOAT DeltaTime)
{
    NOT_DECOMPILED(0x00086d0f);
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

void APlayerController::PostScriptDestroyed()
{
    delete PlayerInput;
    PlayerInput = nullptr;
    delete CheatManager;
    CheatManager = nullptr;
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

void AActor::ProcessEvent(UFunction* Function, void* Parms, void* Result)
{
    if (Level->bBegunPlay && !GIsGarbageCollecting)
        UObject::ProcessEvent(Function, Parms, Result);
}

void AActor::SetCollision(UBOOL NewCollideActors, UBOOL NewBlockActors, UBOOL NewBlockPlayers)
{
    const UBOOL OldCollideActors = bCollideActors;
    if (OldCollideActors && XLevel->Hash)
        XLevel->Hash->RemoveActor(this);
    if (bCollideActors && !NewCollideActors)
    {
        for (INT i = 0; i < Touching.Num();)
        {
            if (Touching[i])
                Touching[i]->EndTouch(this, 0);
            else
                i++;
        }
    }
    bCollideActors = NewCollideActors;
    bBlockActors = NewBlockActors;
    bBlockPlayers = NewBlockPlayers;
    if (bCollideActors && XLevel->Hash)
        XLevel->Hash->AddActor(this);

    // touch what it overlaps now
    if (NewCollideActors && !OldCollideActors)
    {
        FMemMark Mark(GMem);
        FCheckResult* FirstHit = XLevel->Hash
            ? XLevel->Hash->ActorEncroachmentCheck(GMem, this, Location, Rotation, 0x9f, 0)
            : nullptr;
        for (FCheckResult* Test = FirstHit; Test; Test = Test->Next)
        {
            AActor* Other = Test->Actor;
            if (Other != this && !Other->IsJoinedTo(this) && Other != Level && !IsBlockedBy(Other))
                BeginTouch(Other);
        }
        Mark.Pop();
    }
    bNetDirty = 1;

    if (bBlockKarma)
    {
        // physics 14 is ragdoll, 13 is karma
        if (bCollideActors && !OldCollideActors && (Physics == 14 || Physics == 13))
            KSetActorListed(this, 1);
        else if (!bCollideActors && OldCollideActors)
        {
            KSetActorListed(this, 0);
            NOT_DECOMPILED(0x00088f50);
        }
    }
}

void AActor::execSetCollision(FFrame& Stack, RESULT_DECL)
{
    P_GET_UBOOL_OPTX(NewCollideActors, bCollideActors);
    P_GET_UBOOL_OPTX(NewBlockActors, bBlockActors);
    P_GET_UBOOL_OPTX(NewBlockPlayers, bBlockPlayers);
    P_FINISH;
    SetCollision(NewCollideActors, NewBlockActors, NewBlockPlayers);
}

void AActor::execMove(FFrame& Stack, RESULT_DECL)
{
    P_GET_STRUCT(FVector, Delta);
    P_FINISH;
    *static_cast<DWORD*>(Result) = Move(Delta);
}

void AActor::execSetRotation(FFrame& Stack, RESULT_DECL)
{
    P_GET_STRUCT(FRotator, NewRotation);
    P_FINISH;
    *static_cast<DWORD*>(Result) = SetRotation(NewRotation);
}

// pushes the controller onto the level's list
void AController::execAddController(FFrame& Stack, RESULT_DECL)
{
    P_FINISH;
    nextController = Level->ControllerList;
    Level->ControllerList = this;
}

void AController::execRemoveController(FFrame& Stack, RESULT_DECL)
{
    P_FINISH;
    if (Level->ControllerList == this)
    {
        Level->ControllerList = nextController;
        return;
    }
    for (AController* C = Level->ControllerList; C; C = C->nextController)
    {
        if (C->nextController == this)
        {
            C->nextController = nextController;
            return;
        }
    }
}

void AActor::execSetOwner(FFrame& Stack, RESULT_DECL)
{
    P_GET_OBJECT(AActor, NewOwner);
    P_FINISH;
    SetOwner(NewOwner);
}

UBOOL ASquadMarker::IsSetUpProperly()
{
    if (!Anchor)
        return 0;
    for (INT i = 0; i < Anchor->NavPts.Num(); i++)
    {
        if (!Anchor->NavPts[i])
            Anchor->NavPts.Remove(i--);
    }
    return Anchor->NavPtLocs.Num() <= Anchor->NavPts.Num();
}

void ASquadMarker::execIsSetUpProperly(FFrame& Stack, RESULT_DECL)
{
    P_FINISH;
    *static_cast<DWORD*>(Result) = IsSetUpProperly();
}

void APawn::execUseWoundedAnims(FFrame& Stack, RESULT_DECL)
{
    P_FINISH;
    *static_cast<DWORD*>(Result) = UseWoundedAnims();
}

// takes the actor out of the collision hash while the flag changes
void AActor::execOnlyAffectPawns(FFrame& Stack, RESULT_DECL)
{
    P_GET_UBOOL(B);
    P_FINISH;
    if (bCollideActors && B != bOnlyAffectPawns)
    {
        SetCollision(0, bBlockActors, bBlockPlayers);
        bOnlyAffectPawns = B;
        SetCollision(1, bBlockActors, bBlockPlayers);
    }
}

void AActor::execDestroy(FFrame& Stack, RESULT_DECL)
{
    P_FINISH;
    *static_cast<DWORD*>(Result) = XLevel->DestroyActor(this);
}

void AActor::execLinkAnimToMesh(FFrame& Stack, RESULT_DECL)
{
    P_GET_OBJECT(UMeshAnimation, Anim);
    P_GET_OBJECT(UMesh, TargetMesh);
    P_FINISH;
    if (Anim && TargetMesh && TargetMesh->IsA(USkeletalMesh::StaticClass()))
        static_cast<USkeletalMesh*>(TargetMesh)->AddSkelAnim(Anim, 0);
}

// 0x000e576f
void AActor::execHasAnim(FFrame& Stack, RESULT_DECL)
{
    P_GET_NAME(Sequence);
    P_FINISH;
    if (Mesh)
        *static_cast<DWORD*>(Result) = Mesh->GetAnimNamed(Sequence, 1) != nullptr;
    else
        Stack.Logf(TEXT("HasAnim: No mesh"));
}

void AActor::execConsoleCommand(FFrame& Stack, RESULT_DECL)
{
    P_GET_STR(Command);
    P_FINISH;
    FStringOutputDevice StrOut;
    XLevel->Engine->Exec(*Command, StrOut);
    *static_cast<FString*>(Result) = *StrOut;
}

void APlayerController::execConsoleCommand(FFrame& Stack, RESULT_DECL)
{
    P_GET_STR(Command);
    P_FINISH;
    *static_cast<FString*>(Result) = TEXT("");
    FStringOutputDevice StrOut;
    if (Player)
        // the player's FExec at +0x2c; UPlayer isn't an FExec in the port yet
        NOT_DECOMPILED(0x00103028);
    else
        XLevel->Engine->Exec(*Command, StrOut);
    *static_cast<FString*>(Result) = *StrOut;
}
