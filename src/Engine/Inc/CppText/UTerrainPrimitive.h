    // the terrain this primitive belongs to (name invented)
    ATerrainInfo* Info;

    UTerrainPrimitive() {}
    UTerrainPrimitive(ATerrainInfo* InInfo) : Info(InInfo) {}

    // 0x00069648
    UBOOL PointCheck(FCheckResult& Result, AActor* Owner, const FVector& Location, const FVector& Extent, DWORD ExtraNodeFlags) override;
