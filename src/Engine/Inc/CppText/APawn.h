    // 0x00032743
    UBOOL IsAPawn() override { return 1; }
    // 0x000eb641
    APawn* GetPlayerPawn() const override;
    // 0x000eb755
    UBOOL ShouldTrace(AActor* SourceActor, DWORD TraceFlags) override;
    // 0x0012c908. also tracks the head volume and tells the controller.
    void SetZone(UBOOL bTest, UBOOL bForceRefresh) override;
    // 0x00129ce3
    void eventHeadVolumeChange(APhysicsVolume* NewHeadVolume)
    {
        if (IsProbing(FName(NAME_HeadVolumeChange)))
        {
            struct { APhysicsVolume* NewHeadVolume; } Parms{NewHeadVolume};
            ProcessEvent(FindFunctionChecked(FName(NAME_HeadVolumeChange)), &Parms);
        }
    }
