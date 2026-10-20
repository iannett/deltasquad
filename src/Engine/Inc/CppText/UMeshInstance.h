    // 0x00038369
    FBox GetCollisionBoundingBox(const AActor* Owner) const override;
    // 0x0003831d
    UBOOL PointCheck(FCheckResult& Result, AActor* Owner, const FVector& Location, const FVector& Extent, DWORD ExtraNodeFlags) override;
