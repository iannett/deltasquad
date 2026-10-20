    // 0x000a2329
    FBox GetCollisionBoundingBox(const AActor* Owner) const override;
    // 0x000a220b
    UBOOL PointCheck(FCheckResult& Result, AActor* Owner, const FVector& Location, const FVector& Extent, DWORD ExtraNodeFlags) override;
