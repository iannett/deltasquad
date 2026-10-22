    // the fluid surface this primitive belongs to (name invented)
    AFluidSurfaceInfo* Info;

    // 0x000a2329
    FBox GetCollisionBoundingBox(const AActor* Owner) const override;
    // 0x000a220b
    UBOOL PointCheck(FCheckResult& Result, AActor* Owner, const FVector& Location, const FVector& Extent, DWORD ExtraNodeFlags) override;
    // 0x00070cb0
    UBOOL UseCylinderCollision(const AActor* Owner) override { return 0; }
