    UPolys*                      Polys;
    TTransArray<FBspNode>        Nodes;
    TTransArray<FVert>           Verts;
    TTransArray<FVector>         Vectors;
    TTransArray<FVector>         Points;
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

    // 0x000ae025. the render device's copies are only flushed when one is given.
    void ClearRenderData(URenderDevice* RenDev);
    // 0x000ae0bd. groups the nodes into sections by material, poly flags and light map texture and builds their vertices.
    void BuildRenderData();
    // 0x0017b9a0
    FBox GetCollisionBoundingBox(const AActor* Owner) const override;
    // 0x0017e441
    UBOOL PointCheck(FCheckResult& Result, AActor* Owner, const FVector& Location, const FVector& Extent, DWORD ExtraNodeFlags) override;
    // 0x0017b8d8. the leaf and zone holding the location; Zone when the zone has no actor.
    FPointRegion PointRegion(AZoneInfo* Zone, const FVector& Location) const;
    // 0x000955c3. half the collision box.
    FVector GetEncroachExtent(AActor* Owner) override { return GetCollisionBoundingBox(Owner).GetExtent(); }
    // 0x00076cc4. the collision box's centre.
    FVector GetEncroachCenter(AActor* Owner) override { return GetCollisionBoundingBox(Owner).GetCenter(); }
    // 0x00070cb0
    UBOOL UseCylinderCollision(const AActor* Owner) override { return 0; }
