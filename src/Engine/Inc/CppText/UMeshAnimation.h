    INT                  Version;
    TArray<FNamedBone>   RefBones;
    TArray<FSkelAnimSeq> Moves;
    // built after loading (name invented)
    TArray<FMeshMultiAnim> MultiAnims;

    void Serialize(FArchive& Ar) override;
    // 0x00051f89
    void PostLoad() override;
    // 0x000517b9
    void ConstructMultiAnims();
