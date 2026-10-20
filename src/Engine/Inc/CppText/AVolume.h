    // 0x00032743
    UBOOL IsAVolume() override { return 1; }
    // 0x0010a1e9. volumes are hit by volume traces.
    UBOOL ShouldTrace(AActor* SourceActor, DWORD TraceFlags) override;
    // 0x0011213e
    UBOOL Encompasses(FVector Point);
