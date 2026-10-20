    void Serialize(FArchive& Ar) override;
    // 0x0009dc77. xbox slot 0x78, shared by every material: frees the cached render passes. its real name is not known.
    virtual void ClearCachedPasses();
