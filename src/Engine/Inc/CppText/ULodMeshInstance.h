    AActor*              Actor;
    // bit 0 while UpdateAnimation runs, bit 1 to delete the instance once it is done
    INT                  Status;
    // lets PlayAnim tween from the previous sequence (name invented)
    BYTE                 bTween;
    BYTE                 Unknown61;
    TArray<FAnimChannel> Channels;
    INT                  Unknown6C[2];
    INT                  Unknown74;
    BYTE                 bIgnoreMeshOffset;

    // 0x00038d10
    AActor* GetActor() override { return Actor; }
    // 0x00038d14
    void SetActor(AActor* InActor) override { Actor = InActor; }
    // 0x000407ee
    void SetMesh(UMesh* InMesh) override;
    // 0x00038d06
    void SetStatus(INT InStatus) override { Status = InStatus; }
    // 0x0007054e
    INT GetStatus() override { return Status; }
    // 0x00040811
    UBOOL PlayAnim(const FPlayAnim& Anim) override;
    // 0x00040b00
    void UpdateAnimation(FLOAT DeltaSeconds) override;
    // 0x00040f4b
    UBOOL IsAnimating(INT Channel) override;
    // 0x0004126a
    void DebugAnim(FOutputDevice& Out) override;
    // 0x00040f8e
    void StopAnimating(UBOOL ClearAllButBase) override;
    // 0x0004100c
    UBOOL IsPlayingAnim(FName Sequence, FName Bone) override;
    // 0x000404d0
    UBOOL IsAnimLooping(INT Channel) override;
    // 0x000404f6
    UBOOL IsAnimPastLastFrame(INT Channel) override;
    // 0x0004052d
    UBOOL AnimStopLooping(INT Channel) override;
    // 0x0004105a
    FPlayAnim GetActiveAnim(FName Bone) override;
    // 0x00038d1e
    virtual void IgnoreMeshOffset(UBOOL bIgnore)
    {
        bIgnoreMeshOffset = bIgnore;
        Unknown61 = 0;
    }
    // 0x0005ec28 (empty). called on a channel PlayAnim starts.
    virtual void PostInitAnim(FAnimChannel& Channel) {}
    // 0x00040556
    const FMeshAnimSeq* GetSequence(INT Channel) { return Channels[Channel].AnimSeq; }
