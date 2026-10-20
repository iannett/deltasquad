    UPolys*                      Polys;
    TTransArray<FBspNode>        Nodes;
    TTransArray<FModelUnknown64> Unknown64;
    TTransArray<FVector>         Unknown70;
    TTransArray<FVector>         Unknown7C;
    TTransArray<FBspSurf>        Surfs;
    TArray<FBox>                 Unknown94;
    TArray<INT>                  Unknown9C;
    TArray<FModelUnknownA4>      UnknownA4;
    TArray<UObject*>             UnknownAC;
    TArray<FBspSection>          Sections;
    TArray<FLightMapTexture>     LightMapTextures;
    TArray<FLightMap>            LightMaps;
    // never serialized
    TArray<INT>                  UnknownCC;
    INT                          UnknownD4;
    INT                          UnknownD8;
    INT                          UnknownDC;
    INT                          UnknownE0;
    INT                          NumZones;
    FZoneProperties              Zones[64];

    void Serialize(FArchive& Ar) override;
