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
    UModel*                              CollisionModel;
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
    // 0x001937f7
    FBox GetCollisionBoundingBox(const AActor* Owner) const override;
    // 0x001960bf
    UBOOL PointCheck(FCheckResult& Result, AActor* Owner, const FVector& Location, const FVector& Extent, DWORD ExtraNodeFlags) override;
    // 0x000955c3. half the collision box.
    FVector GetEncroachExtent(AActor* Owner) override { return GetCollisionBoundingBox(Owner).GetExtent(); }
    // 0x00076cc4. the collision box's centre.
    FVector GetEncroachCenter(AActor* Owner) override { return GetCollisionBoundingBox(Owner).GetCenter(); }
