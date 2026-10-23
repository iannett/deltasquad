    // a side of the volume and its polygon (name invented)
    struct FFace
    {
        FPlane          Plane;
        TArray<FVector> Vertices;

        // 0x000b18d2
        friend FArchive& operator<<(FArchive& Ar, FFace& F)
        {
            Ar.Serialize(&F.Plane, sizeof(FPlane));
            return Ar << F.Vertices;
        }
    };

    TArray<FFace>  Faces;
    TArray<FPlane> Unknown5C;
    FBox           Unknown64;

    // 0x000b1a76
    void Serialize(FArchive& Ar) override;
