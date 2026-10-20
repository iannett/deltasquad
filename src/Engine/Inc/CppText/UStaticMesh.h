    TArray<FStaticMeshSection>           Sections;
    FStaticMeshVertexStream              VertexStream;
    FStaticMeshColorStream               ColorStream;
    FStaticMeshColorStream               AlphaStream;
    TArray<FStaticMeshUVStream>          UVStreams;
    FStaticMeshTangentStream             TangentStream;
    FRawIndexBuffer                      IndexBuffer;
    FRawIndexBuffer                      WireframeIndexBuffer;
    TArray<FStaticMeshMaterial>          Materials;
    TArray<FStaticMeshCollisionTriangle> CollisionTriangles;
    TArray<FStaticMeshCollisionNode>     CollisionNodes;
    BYTE                                 Unknown138[12];
    UObject*                             CollisionModel;
    UBOOL                                UseSimpleLineCollision;
    UBOOL                                UseSimpleBoxCollision;
    UBOOL                                UseVertexColor;
    UBOOL                                UseSimpleKarmaCollision;
    UBOOL                                UseCollisionForBakedShadowsOnly;
    UBOOL                                LoadVertexColorInGame;
    UBOOL                                AlwaysBuildBumpData;
    BYTE                                 ShaderUsage;
    TLazyArray<FStaticMeshTriangle>      RawTriangles;
    // 9 when saved by this engine, -1 for packages before version 81
    INT                                  InternalVersion;
    INT                                  Unknown184;
    // object references from older package versions
    UObject*                             Unknown188;
    UObject*                             Unknown18C;
    UObject*                             Unknown190;
    UObject*                             Unknown194;
    BYTE                                 TypeOfMaterial;

    void StaticConstructor();
    void Serialize(FArchive& Ar) override;
