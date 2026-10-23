    // the bone matrices of the last pose (name invented)
    TArray<FMatrix>      SpaceBases;
    // per-bone overrides set by script, 0x70 bytes each on the xbox; not decompiled yet
    TArray<BYTE>         Directors;
    // where the actor's root motion started, from PostInitAnim (name invented)
    FMatrix              MoveStart;
    // the ragdoll's collision model and joint of each bone, from KInitSkeletonKarma (names invented)
    TArray<struct McdModel*>      KSkelModels;
    TArray<struct MdtConstraint*> KSkelJoints;
    // the first and last bones with a model
    INT                  KSkelFirstBone;
    INT                  KSkelLastBone;
    // pairs of the ragdoll's models that do not collide
    TMap<FKModelPair, INT> KSkelDisabledPairs;

    // 0x0004aa9c
    void SetMesh(UMesh* InMesh) override;
    // 0x000434c9
    void UpdateAnimation(FLOAT DeltaSeconds) override;
    // 0x00047f6b
    void StopAnimating(UBOOL ClearAllButBase) override;
    // 0x00043bb3
    UBOOL GetMoveCoords(FMatrix* Out, UBOOL bWorld) override;
    // 0x00048cfe
    void UpdateAttachments() override;
    // 0x0004a914. the channel on Anim's bone and channel number, added in order when missing.
    FAnimChannel* GetChannel(const FPlayAnim& Anim) override;
    // 0x000430f4. remembers where root motion starts.
    void PostInitAnim(FAnimChannel& Channel) override;
    // 0x00043af1. whether the channel shows on Bone (any bone when negative): no later channel fully covers it.
    UBOOL IsChannelActive(INT Channel, INT Bone);
    // 0x0004c082
    void GetFrame(AActor* Owner, FLevelSceneNode* SceneNode, FVector* Verts, INT Size, INT& LODRequest, DWORD TaskFlag);
    // 0x0004adcb. poses the bones into SpaceBases.
    void ApplyAnimation(const AActor* Owner, UBOOL bUnknown);
