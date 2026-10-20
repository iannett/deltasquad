#include "EngineClasses.h"

FLOAT UPawnAudioTable::Unknown431EC0[193];

void UPawnAudioTable::Reset()
{
    for (INT i = 0; i < 193; i++)
        Unknown431EC0[i] = -10000.f;
}

void AActor::SetZone(UBOOL bTest, UBOOL bForceRefresh)
{
    NOT_DECOMPILED(0x0012c71b);
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
