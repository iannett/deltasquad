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
    // 0x0012c85d. a volume's own volume is itself.
    void SetZone(UBOOL bTest, UBOOL bForceRefresh) override;
    // 0x00129974
    void eventPawnEnteredVolume(APawn* Other)
    {
        if (IsProbing(FName(NAME_PawnEnteredVolume)))
        {
            struct { APawn* Other; } Parms{Other};
            ProcessEvent(FindFunctionChecked(FName(NAME_PawnEnteredVolume)), &Parms);
        }
    }
    // 0x001299ec
    void eventPawnLeavingVolume(APawn* Other)
    {
        if (IsProbing(FName(NAME_PawnLeavingVolume)))
        {
            struct { APawn* Other; } Parms{Other};
            ProcessEvent(FindFunctionChecked(FName(NAME_PawnLeavingVolume)), &Parms);
        }
    }
