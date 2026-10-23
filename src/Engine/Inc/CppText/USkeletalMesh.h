    INT                             Unknown124;
    TLazyArray<FVector>             Points;
    TLazyArray<FMeshWedge>          RawWedges;
    TLazyArray<FSkelMeshFace>       RawFaces;
    TLazyArray<FVertInfluence>      RawInfluences;
    TLazyArray<_WORD>               Unknown18C;
    TLazyArray<_WORD>               Unknown1A0;
    TArray<FMeshBone>               RefSkeleton;
    INT                             Unknown1BC;
    // the bone root motion comes from: 0 when the root is named Move, else -1 (name invented)
    INT                             MoveBone;
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
    // 0x00051a68
    void PostLoad() override;
    // 0x000516b7
    UClass* MeshGetInstanceClass() override;
    // 0x00045672. the bone index for a bone or attach alias name; an alias also gives its offset.
    INT MatchRefBone(FName Name, FMatrix* AliasOffset = nullptr);
    // 0x0004aa60
    void ActualizeAnimLinkups();
    // 0x000455a6
    const FMeshAnimSeq* GetAnimNamed(FName Name, UBOOL bRandom) override;
    // 0x0016834b
    FBox GetCollisionBoundingBox(const AActor* Owner) const override;
    // 0x001680d8. ragdolls (physics 14) collide with their bodies.
    UBOOL UseCylinderCollision(const AActor* Owner) override { return Owner->Physics != 14; }
    // 0x00168308
    UBOOL PointCheck(FCheckResult& Result, AActor* Owner, const FVector& Location, const FVector& Extent, DWORD ExtraNodeFlags) override;
