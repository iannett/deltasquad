    // 0x000763dc
    FBox GetCollisionBoundingBox(const AActor* Owner) const override;
    // 0x00076d18
    UBOOL PointCheck(FCheckResult& Result, AActor* Owner, const FVector& Location, const FVector& Extent, DWORD ExtraNodeFlags) override;
    // 0x000955c3. half the collision box.
    FVector GetEncroachExtent(AActor* Owner) override { return GetCollisionBoundingBox(Owner).GetExtent(); }
    // 0x00076cc4. the collision box's centre.
    FVector GetEncroachCenter(AActor* Owner) override { return GetCollisionBoundingBox(Owner).GetCenter(); }
