    TArray<FTerrainSectorUnknown28>         Unknown28;
    TArray<FTerrainSectorUnknown30>         Unknown30;
    ATerrainInfo*                           Info;
    FBox                                    BoundingBox;
    INT                                     Unknown58;
    INT                                     Unknown5C;
    INT                                     Unknown60;
    INT                                     Unknown64;
    TArray<FStaticMeshProjectorRenderInfo*> Projectors;
    INT                                     Unknown80;
    FRawIndexBuffer                         Unknown88;
    INT                                     UnknownA0;
    INT                                     UnknownA4;
    INT                                     UnknownA8;
    INT                                     UnknownAC;

    void Serialize(FArchive& Ar) override;
    void PostLoad() override;
