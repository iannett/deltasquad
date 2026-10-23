    // 0x00129a64
    UBOOL eventNotifyPhysicsVolumeChange(APhysicsVolume* NewVolume)
    {
        struct { APhysicsVolume* NewVolume; UBOOL ReturnValue; } Parms{NewVolume, 0};
        if (IsProbing(FName(NAME_NotifyPhysicsVolumeChange)))
            ProcessEvent(FindFunctionChecked(FName(NAME_NotifyPhysicsVolumeChange)), &Parms);
        return Parms.ReturnValue;
    }
    // 0x00129ae9
    UBOOL eventNotifyHeadVolumeChange(APhysicsVolume* NewVolume)
    {
        struct { APhysicsVolume* NewVolume; UBOOL ReturnValue; } Parms{NewVolume, 0};
        if (IsProbing(FName(NAME_NotifyHeadVolumeChange)))
            ProcessEvent(FindFunctionChecked(FName(NAME_NotifyHeadVolumeChange)), &Parms);
        return Parms.ReturnValue;
    }
    // 0x00070cad. CT's name.
    virtual UBOOL LocalPlayerController() { return 0; }
    // 0x000df614. as the actor's, on the pawn's animation.
    void StartAnimPoll() override;
    // 0x000dee6f
    UBOOL CheckAnimFinished(INT Channel) override;
