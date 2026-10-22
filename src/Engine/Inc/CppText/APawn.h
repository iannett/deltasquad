    // 0x00032743
    UBOOL IsAPawn() override { return 1; }
    // 0x000eb641
    APawn* GetPlayerPawn() const override;
    // 0x000ed673, a jump to 0x00179c7f. takes the pawn out of a list in globals at 0x004ab448.
    void PostScriptDestroyed() override;
    // 0x000eb755
    UBOOL ShouldTrace(AActor* SourceActor, DWORD TraceFlags) override;
    // 0x000ebc36. hurt pawns (health level below 2) limp when health affects gameplay.
    UBOOL UseWoundedAnims() { return HealthAffectsGameplay && HealthLevel < 2; }
    // 0x0011a057
    void physFalling(FLOAT DeltaTime, INT Iterations) override;
    // 0x00112cfd
    UBOOL physRootMotion(FLOAT DeltaTime) override;
    // 0x0012c059. the controller hears about the bump first.
    void NotifyBumpEx(AActor* Other) override;
    // 0x000f059e. a controller that handles AnimEnd gets it instead of the pawn.
    void NotifyAnimEnd(INT Channel) override;
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
