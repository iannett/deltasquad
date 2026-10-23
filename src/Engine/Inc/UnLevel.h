#pragma once

class ULevel;
class UNetConnection;
class UChannel;
class AActor;
class UPrimitive;
class UMaterial;

// how a level answers a client asking to connect
enum EAcceptConnection
{
    ACCEPTC_Reject = 0,
    ACCEPTC_Accept = 1,
    ACCEPTC_Ignore = 2, // no reply, while the server travels
};

// what a level updates in a tick
enum ELevelTick
{
    LEVELTICK_TimeOnly      = 0,
    LEVELTICK_ViewportsOnly = 1,
    LEVELTICK_All           = 2,
};

// the latent actions a state frame can wait in; each is the index of the native that polls it
enum EPollSlotType
{
    EPOLL_Sleep      = 384,
    EPOLL_FinishAnim = 385,
};

// the net code's callbacks into a level or pending level
class FNetworkNotify
{
public:
    virtual EAcceptConnection NotifyAcceptingConnection() = 0;
    virtual void NotifyAcceptedConnection(UNetConnection* Connection) = 0;
    virtual UBOOL NotifyAcceptingChannel(UChannel* Channel) = 0;
    virtual ULevel* NotifyGetLevel() = 0;
    virtual void NotifyReceivedText(UNetConnection* Connection, const TCHAR* Text) = 0;
    virtual UBOOL NotifySendingFile(UNetConnection* Connection, FGuid GUID) = 0;
    virtual void NotifyReceivedFile(UNetConnection* Connection, INT PackageIndex, const TCHAR* Error, UBOOL Skipped) = 0;
    virtual void NotifyProgress(const TCHAR* Str1, const TCHAR* Str2, FLOAT Seconds) = 0;
};

// one hit of a collision check. checks chain their hits through Next.
struct FCheckResult
{
    FCheckResult* Next;
    AActor*       Actor;
    FVector       Location;
    FVector       Normal;
    UPrimitive*   Primitive;
    FLOAT         Time;
    INT           Item;
    UMaterial*    Material;
    BYTE          Unknown30;

    explicit FCheckResult(FLOAT InTime = 1.f)
        : Next(nullptr), Actor(nullptr), Location(0.f, 0.f, 0.f), Normal(0.f, 0.f, 0.f), Primitive(nullptr), Time(InTime),
          Item(INDEX_NONE), Material(nullptr), Unknown30(0) {}

    FCheckResult* GetNext() const { return Next; }
};

// the actor hash that answers collision queries. the checks' parameters are not known yet.
class FCollisionHashBase
{
public:
    virtual ~FCollisionHashBase() {}
    virtual void Tick() = 0;
    virtual void AddActor(AActor* Actor) = 0;
    virtual void RemoveActor(AActor* Actor) = 0;
    virtual FCheckResult* ActorLineCheck() = 0;
    virtual FCheckResult* ActorPointCheck(FMemStack& Mem, const FVector& Location, const FVector& Extent, DWORD TraceFlags,
        DWORD ExtraNodeFlags, UBOOL bSingleResult) = 0;
    virtual FCheckResult* ActorRadiusCheck() = 0;
    virtual FCheckResult* ActorEncroachmentCheck(FMemStack& Mem, AActor* Actor, const FVector& Location, const FRotator& Rotation,
        DWORD TraceFlags, DWORD ExtraNodeFlags) = 0;
    virtual FCheckResult* ActorOverlapCheck() = 0;
    virtual void CheckActorNotReferenced(AActor* Actor) = 0;
    virtual void CheckIsEmpty() = 0;
    virtual void CheckActorLocations(ULevel* Level) = 0;
    virtual void Serialize(FArchive& Ar) = 0;
};

// 0x0019a7be
FCollisionHashBase* GNewCollisionHash();

class FCollisionOctree;

// a cube of the octree. actors are stored in the smallest node that holds them, or in every leaf they touch.
class FOctreeNode
{
public:
    TArray<AActor*> Actors;
    // eight children, or null for a leaf
    FOctreeNode*    Children;

    // 0x002865ed
    FOctreeNode() : Children(nullptr) {}
    // 0x001d193a
    ~FOctreeNode();

    // 0x001d2774. stores the actor in the smallest node that contains its whole box.
    void SingleNodeFilter(AActor* Actor, FCollisionOctree* Octree, const FPlane* Bounds);
    // 0x001cfd6f. stores the actor in every node its box overlaps, unless it fills most of this one.
    void MultiNodeFilter(AActor* Actor, FCollisionOctree* Octree, const FPlane* Bounds);
    // 0x001d1f55
    void RemoveAllActors(FCollisionOctree* Octree);
    // 0x001d1ee6
    void CheckIsEmpty();
    // 0x001d1fdc
    void Serialize(FArchive& Ar);
    // 0x001d0e6b
    void ActorPointCheck(FCollisionOctree* Octree, const FPlane* Bounds);
    // 0x001d0b64
    void ActorEncroachmentCheck(FCollisionOctree* Octree, const FPlane* Bounds);

private:
    // 0x001d2618. a full node splits into children and filters its actors again.
    void StoreActor(AActor* Actor, FCollisionOctree* Octree, const FPlane* Bounds);
};

// the collision hash levels use: an octree over the whole world
class FCollisionOctree : public FCollisionHashBase
{
public:
    FOctreeNode* RootNode;
    // bumped by each check so an actor in several nodes is tested once
    DWORD        CollisionTag;
    // the current check's state; parts only other checks use are not known yet
    FCheckResult* ChkResult;
    FMemStack*    ChkMem;
    INT           Unknown14[3];
    FVector       ChkLocation;
    INT           Unknown2C[9];
    FVector       ChkExtent;
    DWORD         ChkTraceFlags;
    DWORD         ChkExtraNodeFlags;
    AActor*       ChkActor;
    INT           Unknown68;
    FBox          ChkBox;
    INT           Unknown88[7];
    INT           UnknownA4[27];

    // 0x001d1957
    FCollisionOctree();
    // 0x001d201a
    ~FCollisionOctree() override;

    // 0x00052bf0
    void Tick() override {}
    // 0x001d2877
    void AddActor(AActor* Actor) override;
    // 0x001d2042
    void RemoveActor(AActor* Actor) override;
    FCheckResult* ActorLineCheck() override;
    // 0x001d161c
    FCheckResult* ActorPointCheck(FMemStack& Mem, const FVector& Location, const FVector& Extent, DWORD TraceFlags,
        DWORD ExtraNodeFlags, UBOOL bSingleResult) override;
    FCheckResult* ActorRadiusCheck() override;
    // 0x001d17ba. the actors that Actor would overlap at Location and Rotation.
    FCheckResult* ActorEncroachmentCheck(FMemStack& Mem, AActor* Actor, const FVector& Location, const FRotator& Rotation,
        DWORD TraceFlags, DWORD ExtraNodeFlags) override;
    FCheckResult* ActorOverlapCheck() override;
    // 0x0005ec28
    void CheckActorNotReferenced(AActor* Actor) override {}
    // 0x001d1f4d
    void CheckIsEmpty() override { RootNode->CheckIsEmpty(); }
    void CheckActorLocations(ULevel* Level) override;
    // 0x001d25b8. counts memory only.
    void Serialize(FArchive& Ar) override;
};

// a node in the level's list of newly spawned actors
struct FActorLink
{
    AActor*     Actor;
    FActorLink* Next;
};

// a 76 byte element of ULevel's array at 0x10f0; its fields are not known yet
struct FLevelUnknown10F0
{
    BYTE Data[76];
};
