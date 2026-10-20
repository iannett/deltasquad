#pragma once

class ULevel;
class UNetConnection;
class UChannel;
class AActor;
struct FCheckResult;

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

// the actor hash that answers collision queries. only its slots are known so far.
class FCollisionHashBase
{
public:
    virtual ~FCollisionHashBase() {}
    virtual void Tick() = 0;
    virtual void AddActor(AActor* Actor) = 0;
    virtual void RemoveActor(AActor* Actor) = 0;
    virtual FCheckResult* ActorLineCheck() = 0;
    virtual FCheckResult* ActorPointCheck() = 0;
    virtual FCheckResult* ActorRadiusCheck() = 0;
    virtual FCheckResult* ActorEncroachmentCheck() = 0;
    virtual void ActorOverlapCheck() = 0;
    virtual void CheckActorNotReferenced(AActor* Actor) = 0;
    virtual void CheckIsEmpty() = 0;
    virtual void CheckActorLocation() = 0;
    virtual void CountBytes(FArchive& Ar) = 0;
};

// a 76 byte element of ULevel's array at 0x10f0; its fields are not known yet
struct FLevelUnknown10F0
{
    BYTE Data[76];
};
