    void Serialize(FArchive& Ar) override;
    // 0x0009dc77. xbox slot 0x78, shared by every material: frees the cached render passes. its real name is not known.
    virtual void ClearCachedPasses();
    // 0x0009fbb0
    virtual UBOOL CheckCircularReferences(TArray<UMaterial*>& History);
    // 0x0002b695
    virtual UBOOL GetValidated() { return Validated; }
    // 0x0002b69e
    virtual void SetValidated(UBOOL InValidated) { Validated = InValidated; }
    // 0x0005ec28
    virtual void PreSetMaterial(FLOAT TimeSeconds) {}
    // the base class has no size
    virtual INT MaterialUSize() { return 0; }
    virtual INT MaterialVSize() { return 0; }
