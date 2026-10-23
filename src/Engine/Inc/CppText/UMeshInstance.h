    // the mesh drawn; a ULodMeshInstance keeps only a ULodMesh here (name invented)
    UMesh* Mesh;

    // 0x00038369
    FBox GetCollisionBoundingBox(const AActor* Owner) const override;
    // 0x0003831d
    UBOOL PointCheck(FCheckResult& Result, AActor* Owner, const FVector& Location, const FVector& Extent, DWORD ExtraNodeFlags) override;
    // 0x00070cad
    virtual AActor* GetActor() { return nullptr; }
    // 0x0005ec28 (empty)
    virtual void SetActor(AActor* InActor) {}
    // 0x0003830e
    virtual void SetMesh(UMesh* InMesh) { Mesh = InMesh; }
    // 0x0005ec28 (empty)
    virtual void SetStatus(INT InStatus) {}
    // 0x00070cad
    virtual INT GetStatus() { return 0; }
    // 0x000418d6
    virtual UBOOL PlayAnim(const FPlayAnim& Anim) { return 0; }
    // 0x0005ec28 (empty)
    virtual void UpdateAnimation(FLOAT DeltaSeconds) {}
    // 0x000418d6
    virtual UBOOL IsAnimating(INT Channel) { return 0; }
    // 0x0005ec28 (empty)
    virtual void DebugAnim(FOutputDevice& Out) {}
    // 0x0005ec28 (empty)
    virtual void StopAnimating(UBOOL ClearAllButBase) {}
    // 0x00070cad
    virtual UBOOL IsPlayingAnim(FName Sequence, FName Bone) { return 0; }
    // 0x000418d6
    virtual UBOOL IsAnimLooping(INT Channel) { return 0; }
    // 0x000418d6
    virtual UBOOL IsAnimPastLastFrame(INT Channel) { return 0; }
    // 0x000418d6
    virtual UBOOL AnimStopLooping(INT Channel) { return 0; }
    // 0x00038bce
    virtual FPlayAnim GetActiveAnim(FName Bone) { return FPlayAnim(); }
    // 0x00038318. the root bone's move for the current animation, in the actor's space when bWorld is set.
    virtual UBOOL GetMoveCoords(FMatrix* Out, UBOOL bWorld) { return 0; }
    // 0x00052bf0 (empty). moves the actors attached to the owner's bones.
    virtual void UpdateAttachments() {}
    // 0x00070cb0
    virtual FAnimChannel* GetChannel(const FPlayAnim& Anim) { return nullptr; }
