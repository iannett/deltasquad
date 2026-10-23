    // 0x000779f8
    UPrimitive* GetPrimitive() override;
    // 0x000764ad
    UBOOL ShouldTrace(AActor* SourceActor, DWORD TraceFlags) override;
    // 0x0007b7be. projects onto the actors and terrain in range; dedicated servers skip it.
    virtual void Attach();
    // 0x00076458
    virtual UMaterial* GetProjTexture() { return ProjTexture; }
