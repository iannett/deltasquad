    FBox    BoundingBox;
    FSphere BoundingSphere;

    void Serialize(FArchive& Ar) override;
    // 0x000b1abf. the bounding box, or the owner's collision cylinder grown by 1.
    virtual FBox GetCollisionBoundingBox(const AActor* Owner) const;
    // 0x000b1d10. against the owner's collision cylinder; 0 on a hit.
    virtual UBOOL PointCheck(FCheckResult& Result, AActor* Owner, const FVector& Location, const FVector& Extent, DWORD ExtraNodeFlags);
    // 0x000b1ce6. the half size used to test what the owner encroaches on: its collision cylinder.
    virtual FVector GetEncroachExtent(AActor* Owner);
    // 0x000b1aa5. the owner's location.
    virtual FVector GetEncroachCenter(AActor* Owner);
