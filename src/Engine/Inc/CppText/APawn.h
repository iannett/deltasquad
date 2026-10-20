    // 0x00032743
    UBOOL IsAPawn() override { return 1; }
    // 0x000eb641
    APawn* GetPlayerPawn() const override;
    // 0x000eb755
    UBOOL ShouldTrace(AActor* SourceActor, DWORD TraceFlags) override;
