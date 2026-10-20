    // 0x00129bf3
    void eventActorEntered(AActor* Other)
    {
        if (IsProbing(FName(NAME_ActorEntered)))
        {
            struct { AActor* Other; } Parms{Other};
            ProcessEvent(FindFunctionChecked(FName(NAME_ActorEntered)), &Parms);
        }
    }
    // 0x00129c6b
    void eventActorLeaving(AActor* Other)
    {
        if (IsProbing(FName(NAME_ActorLeaving)))
        {
            struct { AActor* Other; } Parms{Other};
            ProcessEvent(FindFunctionChecked(FName(NAME_ActorLeaving)), &Parms);
        }
    }
