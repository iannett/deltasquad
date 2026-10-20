    // 0x000763dc
    FBox GetCollisionBoundingBox(const AActor* Owner) const override;
    // 0x00076d18
    UBOOL PointCheck(FCheckResult& Result, AActor* Owner, const FVector& Location, const FVector& Extent, DWORD ExtraNodeFlags) override;
