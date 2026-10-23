    // 8 when saved by this engine
    INT                  LodMeshVersion;
    INT                  Unknown5C;
    TArray<INT>          Unknown60;
    TArray<UObject*>     Unknown68;
    FVector              Unknown70;
    FVector              Unknown7C;
    FVector              Unknown88;
    TArray<_WORD>        Unknown94;
    TArray<FMeshFace>    Faces;
    TArray<_WORD>        UnknownA4;
    TArray<FMeshWedge>   Wedges;
    TArray<FMeshMaterial> Materials;
    INT                  UnknownBC;
    INT                  UnknownC0;
    INT                  UnknownC4;
    INT                  UnknownC8;
    INT                  UnknownCC;
    INT                  UnknownD0;
    INT                  UnknownD4;
    INT                  UnknownD8;
    INT                  UnknownDC;
    INT                  UnknownE0;
    INT                  UnknownE4;
    FLodMeshUnknownE8    UnknownE8;
    INT                  Unknown120;

    void Serialize(FArchive& Ar) override;
    // 0x00038dca
    UClass* MeshGetInstanceClass() override;
