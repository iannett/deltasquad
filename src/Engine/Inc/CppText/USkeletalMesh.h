    INT                             Unknown124;
    TLazyArray<FVector>             Points;
    TLazyArray<FMeshWedge>          RawWedges;
    TLazyArray<FSkelMeshFace>       RawFaces;
    TLazyArray<FVertInfluence>      RawInfluences;
    TLazyArray<_WORD>               Unknown18C;
    TLazyArray<_WORD>               Unknown1A0;
    TArray<FMeshBone>               RefSkeleton;
    INT                             Unknown1BC;
    INT                             Unknown1C4;
    TArray<FStaticLODModel>         LODModels;
    TArray<FVector>                 Unknown1D0;
    TArray<FSkeletalMeshUnknown1D8> Unknown1D8;
    TArray<FSkeletalMeshUnknown1E0> Unknown1E0;
    TArray<FMeshAnimLinkup>         AnimLinkups;
    FSkeletalMeshUnknown1F0         Unknown1F0[4];
    TArray<FMatrix>                 Unknown280;
    TArray<FSkeletalMeshUnknown288> Unknown288;
    TArray<FAttachAlias>            AttachAliases;
    UObject*                        Unknown298;
    FVector                         Unknown29C;
    FVector                         Unknown2A8;

    void Serialize(FArchive& Ar) override;
    void AddSkelAnim(UMeshAnimation* Anim, INT Flag);
    // 0x0016834b
    FBox GetCollisionBoundingBox(const AActor* Owner) const override;
    // 0x00168308
    UBOOL PointCheck(FCheckResult& Result, AActor* Owner, const FVector& Location, const FVector& Extent, DWORD ExtraNodeFlags) override;
