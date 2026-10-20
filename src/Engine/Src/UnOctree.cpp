#include <cmath>

#include "EngineClasses.h"

namespace
{
    // 0x004329f8. the root node's centre and half size.
    const FPlane RootNodeBounds(0.f, 0.f, 0.f, 262144.f);

    // a node holds this many actors before it splits, unless its children would be this small
    const INT   MaxActorsPerNode = 3;
    const FLOAT MinNodeSize = 100.f;

    UBOOL IsNan(const FVector& V)
    {
        return std::isnan(V.X) || std::isnan(V.Y) || std::isnan(V.Z);
    }

    // children are numbered by side: 4 for +X, 2 for +Y, 1 for +Z
    FPlane ChildBounds(const FPlane& Bounds, INT Child)
    {
        const FLOAT HalfSize = Bounds.W * 0.5f;
        return FPlane(static_cast<FLOAT>(((Child >> 1) & 2) - 1) * HalfSize + Bounds.X,
                      static_cast<FLOAT>((Child & 2) - 1) * HalfSize + Bounds.Y,
                      static_cast<FLOAT>((Child & 1) * 2 - 1) * HalfSize + Bounds.Z,
                      HalfSize);
    }

    // the child that holds the whole box, or INDEX_NONE when it crosses a centre plane
    INT FindChild(const FPlane& Bounds, const FBox& Box)
    {
        INT Child = 0;
        if (Box.Min.X > Bounds.X)
            Child |= 4;
        else if (Box.Max.X > Bounds.X)
            return INDEX_NONE;
        if (Box.Min.Y > Bounds.Y)
            Child |= 2;
        else if (Box.Max.Y > Bounds.Y)
            return INDEX_NONE;
        if (Box.Min.Z > Bounds.Z)
            Child |= 1;
        else if (Box.Max.Z > Bounds.Z)
            return INDEX_NONE;
        return Child;
    }

    // 0x001cfaf0. touching counts.
    UBOOL BoxesIntersect(const FBox& A, const FBox& B)
    {
        return A.Min.X <= B.Max.X && B.Min.X <= A.Max.X && A.Min.Y <= B.Max.Y && B.Min.Y <= A.Max.Y
            && A.Min.Z <= B.Max.Z && B.Min.Z <= A.Max.Z;
    }

    UBOOL ChildOverlaps(const FPlane& Bounds, const FBox& Box, INT Child)
    {
        return ((Child & 4) ? Box.Max.X > Bounds.X : Box.Min.X < Bounds.X)
            && ((Child & 2) ? Box.Max.Y > Bounds.Y : Box.Min.Y < Bounds.Y)
            && ((Child & 1) ? Box.Max.Z > Bounds.Z : Box.Min.Z < Bounds.Z);
    }
}

FCollisionHashBase* GNewCollisionHash()
{
    return new FCollisionOctree;
}

// ---------------------------------------------------------------------------------------------- nodes

FOctreeNode::~FOctreeNode()
{
    delete[] Children;
    Children = nullptr;
}

void FOctreeNode::SingleNodeFilter(AActor* Actor, FCollisionOctree* Octree, const FPlane* Bounds)
{
    const INT Child = FindChild(*Bounds, Actor->OctreeBox);
    if (!Children || Child == INDEX_NONE)
    {
        StoreActor(Actor, Octree, Bounds);
        return;
    }
    const FPlane Sub = ChildBounds(*Bounds, Child);
    Children[Child].SingleNodeFilter(Actor, Octree, &Sub);
}

void FOctreeNode::MultiNodeFilter(AActor* Actor, FCollisionOctree* Octree, const FPlane* Bounds)
{
    if (Children)
    {
        // the part of the box inside this node, against the node's volume
        const FBox& Box = Actor->OctreeBox;
        const FLOAT MaxZ = Min(Box.Max.Z, Bounds->Z + Bounds->W);
        const FLOAT MaxY = Min(Box.Max.Y, Bounds->W + Bounds->Y);
        const FLOAT MaxX = Min(Box.Max.X, Bounds->X + Bounds->W);
        const FLOAT MinZ = Max(Box.Min.Z, Bounds->Z - Bounds->W);
        const FLOAT MinY = Max(Box.Min.Y, Bounds->Y - Bounds->W);
        const FLOAT MinX = Max(Box.Min.X, Bounds->X - Bounds->W);
        const FLOAT W = Bounds->W;
        if ((MaxZ - MinZ) * (MaxY - MinY) * (MaxX - MinX) / (W * W * W * 8.f) < 0.125f)
        {
            for (INT Child = 7; Child >= 0; Child--)
            {
                if (ChildOverlaps(*Bounds, Box, Child))
                {
                    const FPlane Sub = ChildBounds(*Bounds, Child);
                    Children[Child].MultiNodeFilter(Actor, Octree, &Sub);
                }
            }
            return;
        }
    }
    StoreActor(Actor, Octree, Bounds);
}

void FOctreeNode::StoreActor(AActor* Actor, FCollisionOctree* Octree, const FPlane* Bounds)
{
    if (Actors.Num() < MaxActorsPerNode || Children || Bounds->W * 0.5f <= MinNodeSize)
    {
        Actors.AddItem(Actor);
        Actor->OctreeNodes.AddItem(this);
        return;
    }

    // split, then filter this node's actors and the new one into the children
    Children = new FOctreeNode[8];
    TArray<AActor*> PendingActors(Actors);
    PendingActors.AddItem(Actor);
    Actors.Empty();
    for (INT i = 0; i < PendingActors.Num(); i++)
    {
        AActor* Pending = PendingActors[i];
        Pending->OctreeNodes.RemoveItem(this);
        if (Pending->bWasSNFiltered)
            SingleNodeFilter(Pending, Octree, Bounds);
        else
            MultiNodeFilter(Pending, Octree, Bounds);
    }
}

void FOctreeNode::RemoveAllActors(FCollisionOctree* Octree)
{
    while (Actors.Num())
    {
        AActor* Actor = Actors[0];
        if (Actor->OctreeNodes.Num() < 1)
        {
            Actors.RemoveItem(Actor);
            GLog->Logf(TEXT("Actor (%s) in Octree, but Actor->OctreeNodes empty."), Actor->GetName());
        }
        else
        {
            Octree->RemoveActor(Actor);
        }
    }
    if (Children)
    {
        for (INT i = 0; i < 8; i++)
            Children[i].RemoveAllActors(Octree);
    }
}

void FOctreeNode::CheckIsEmpty()
{
    for (INT i = 0; i < Actors.Num(); i++)
        GLog->Logf(TEXT("Octree Warning (~FOctreeNode): %s Still In Node."), Actors[i]->GetName());
    if (Children)
    {
        for (INT i = 0; i < 8; i++)
            Children[i].CheckIsEmpty();
    }
}

void FOctreeNode::Serialize(FArchive& Ar)
{
    Actors.CountBytes(Ar);
    if (Children)
    {
        Ar.CountBytes(Children, 8 * sizeof(FOctreeNode));
        for (INT i = 0; i < 8; i++)
            Children[i].Serialize(Ar);
    }
}

void FOctreeNode::ActorPointCheck(FCollisionOctree* Octree, const FPlane* Bounds)
{
    for (INT i = 0; i < Actors.Num(); i++)
    {
        AActor* Actor = Actors[i];
        // each actor is tested once per check, however many nodes hold it
        if (!Actor->bBlockNonZeroExtentTraces || Actor->CollisionTag == static_cast<INT>(Octree->CollisionTag)
            || !Actor->ShouldTrace(nullptr, Octree->ChkTraceFlags))
            continue;
        Actor->CollisionTag = Octree->CollisionTag;
        if (!BoxesIntersect(Actor->OctreeBox, Octree->ChkBox))
            continue;
        FCheckResult TestHit(1.f);
        if (Actor->GetPrimitive()->PointCheck(TestHit, Actor, Octree->ChkLocation, Octree->ChkExtent, 0) == 0)
        {
            FCheckResult* NewResult = new (Octree->ChkMem->PushBytes(sizeof(FCheckResult), 16)) FCheckResult(TestHit);
            NewResult->Next = Octree->ChkResult;
            Octree->ChkResult = NewResult;
            // 0x200 asks for one hit
            if (Octree->ChkTraceFlags & 0x200)
                return;
        }
    }
    if (Children)
    {
        for (INT Child = 7; Child >= 0; Child--)
        {
            if (ChildOverlaps(*Bounds, Octree->ChkBox, Child))
            {
                const FPlane Sub = ChildBounds(*Bounds, Child);
                Children[Child].ActorPointCheck(Octree, &Sub);
            }
        }
    }
}

// ---------------------------------------------------------------------------------------------- octree

FCollisionOctree::FCollisionOctree()
    : RootNode(new FOctreeNode)
    , CollisionTag(0x1fffffff)
    , UnknownA4()
{
}

FCollisionOctree::~FCollisionOctree()
{
    RootNode->RemoveAllActors(this);
    delete RootNode;
}

void FCollisionOctree::AddActor(AActor* Actor)
{
    if (!Actor || Actor->bDeleteMe || Actor->bPendingDelete)
        return;
    if (IsNan(Actor->Location))
    {
        GLog->Logf(TEXT("Octree Warning (AddActor): %s Has Invalid Location."), Actor->GetName());
        return;
    }
    if (Actor->OctreeNodes.Num() > 0)
    {
        GLog->Logf(TEXT("Octree Warning (AddActor): %s Already In Octree."), Actor->GetName());
        RemoveActor(Actor);
    }

    Actor->OctreeBox = Actor->GetPrimitive()->GetCollisionBoundingBox(Actor);
    Actor->OctreeBox = Actor->OctreeBox.ExpandBy(4.2f);
    const FBox& Box = Actor->OctreeBox;
    const FLOAT WorldMax = RootNodeBounds.W;
    // actors entirely outside the world are left out
    if (Box.Max.X >= -WorldMax && Box.Min.X <= WorldMax && Box.Max.Y >= -WorldMax && Box.Min.Y <= WorldMax
        && Box.Max.Z >= -WorldMax && Box.Min.Z <= WorldMax)
    {
        if (Box.Max.X - Box.Min.X > 100000.f || Box.Max.Y - Box.Min.Y > 100000.f || Box.Max.Z - Box.Min.Z > 100000.f)
        {
            GLog->Logf(TEXT("Collision Geometry Warning: %s Has Bad Geometry."), *Actor->GetDescription());
            return;
        }
        // static actors go in every node they touch, others in one node
        if (!Actor->bStatic)
        {
            Actor->bWasSNFiltered = 1;
            RootNode->SingleNodeFilter(Actor, this, &RootNodeBounds);
        }
        else
        {
            Actor->bWasSNFiltered = 0;
            RootNode->MultiNodeFilter(Actor, this, &RootNodeBounds);
        }
    }
    Actor->ColLocation = Actor->Location;
}

void FCollisionOctree::RemoveActor(AActor* Actor)
{
    if (!Actor)
        return;
    if (!Actor->bDeleteMe && Actor->Location != Actor->ColLocation)
    {
        if (IsNan(Actor->ColLocation))
            GLog->Logf(TEXT("Octree Warning (RemoveActor): %s has invalid ColLocation"), Actor->GetName());
        GLog->Logf(TEXT("Octree Warning (RemoveActor): %s moved without proper hashing"), Actor->GetName());
    }
    for (INT i = 0; i < Actor->OctreeNodes.Num(); i++)
    {
        if (Actor->OctreeNodes[i])
            Actor->OctreeNodes[i]->Actors.RemoveItem(Actor);
    }
    Actor->OctreeNodes.Empty();
}

FCheckResult* FCollisionOctree::ActorLineCheck()
{
    NOT_DECOMPILED(0x001d1a75);
    return nullptr;
}

FCheckResult* FCollisionOctree::ActorPointCheck(FMemStack& Mem, const FVector& Location, const FVector& Extent, DWORD TraceFlags,
    DWORD ExtraNodeFlags, UBOOL bSingleResult)
{
    CollisionTag++;
    ChkResult = nullptr;
    ChkMem = &Mem;
    ChkLocation = Location;
    ChkExtent = Extent;
    ChkTraceFlags = TraceFlags;
    ChkExtraNodeFlags = ExtraNodeFlags;
    if (bSingleResult)
        ChkTraceFlags |= 0x200;
    ChkBox.Min = ChkLocation - ChkExtent;
    ChkBox.Max = ChkExtent + ChkLocation;
    ChkBox.IsValid = 1;
    RootNode->ActorPointCheck(this, &RootNodeBounds);
    return ChkResult;
}

FCheckResult* FCollisionOctree::ActorRadiusCheck()
{
    NOT_DECOMPILED(0x001d16f4);
    return nullptr;
}

FCheckResult* FCollisionOctree::ActorEncroachmentCheck()
{
    NOT_DECOMPILED(0x001d17ba);
    return nullptr;
}

FCheckResult* FCollisionOctree::ActorOverlapCheck()
{
    NOT_DECOMPILED(0x001d186c);
    return nullptr;
}

void FCollisionOctree::CheckActorLocations(ULevel* Level)
{
    NOT_DECOMPILED(0x001d24d4);
}

void FCollisionOctree::Serialize(FArchive& Ar)
{
    Ar.CountBytes(this, sizeof(FCollisionOctree));
    if (!Ar.IsLoading() && !Ar.IsSaving() && RootNode)
    {
        Ar.CountBytes(RootNode, sizeof(FOctreeNode));
        RootNode->Serialize(Ar);
    }
}
