    // 0x00129884
    void eventActorEnteredVolume(AActor* Other)
    {
        if (IsProbing(FName(NAME_ActorEnteredVolume)))
        {
            struct { AActor* Other; } Parms{Other};
            ProcessEvent(FindFunctionChecked(FName(NAME_ActorEnteredVolume)), &Parms);
        }
    }
    // 0x001298fc
    void eventActorLeavingVolume(AActor* Other)
    {
        if (IsProbing(FName(NAME_ActorLeavingVolume)))
        {
            struct { AActor* Other; } Parms{Other};
            ProcessEvent(FindFunctionChecked(FName(NAME_ActorLeavingVolume)), &Parms);
        }
    }
