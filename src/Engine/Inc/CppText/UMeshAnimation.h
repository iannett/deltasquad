    INT                  Version;
    TArray<FNamedBone>   RefBones;
    TArray<FSkelAnimSeq> Moves;

    void Serialize(FArchive& Ar) override;
