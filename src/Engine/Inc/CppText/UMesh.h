    // the instance used when an actor can't have its own. only kept by archives that are not package files.
    UMeshInstance* DefMeshInstance;

    void Serialize(FArchive& Ar) override;
    // 0x00070cad. the class of the instances MeshGetInstance makes; none means UMeshInstance.
    virtual UClass* MeshGetInstanceClass() { return nullptr; }
    // 0x0007bd00. the owner's instance of this mesh, made when it has none for this mesh yet.
    virtual UMeshInstance* MeshGetInstance(const AActor* Owner);
    // 0x00052c7d. with bRandom, a sequence in a group with this name is picked at random.
    virtual const FMeshAnimSeq* GetAnimNamed(FName Name, UBOOL bRandom) { return nullptr; }
