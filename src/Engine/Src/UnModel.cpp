#include "EngineClasses.h"

// ---------------------------------------------------------------------------------------------- bsp

// 0x000ab6c2
FArchive& operator<<(FArchive& Ar, FBspNode& N)
{
    Ar << N.Plane << N.ZoneMask << N.NodeFlags;
    Ar << AR_INDEX(N.iVertPool) << AR_INDEX(N.iSurf) << AR_INDEX(N.iChild[0]) << AR_INDEX(N.iChild[1]) << AR_INDEX(N.iPlane);
    Ar << AR_INDEX(N.iCollisionBound) << AR_INDEX(N.iRenderBound);
    Ar << N.Unknown2C;
    if (Ar.Ver() < 135)
    {
        FPlane Unused(0.f, 0.f, 0.f, 0.f);
        Ar << Unused;
    }
    Ar << N.iZone[0] << N.iZone[1] << N.NumVertices << N.iLeaf[0] << N.iLeaf[1];

    if (Ar.Ver() < 92)
    {
        N.iSection = N.iFirstVertex = N.iLightMap = INDEX_NONE;
    }
    else if (Ar.Ver() < 101)
    {
        // one dropped value at version 92, two up to 100
        INT Unused = INDEX_NONE;
        if (Ar.Ver() >= 93)
            Ar << Unused;
        Ar << Unused;
        N.iSection = N.iFirstVertex = N.iLightMap = INDEX_NONE;
    }
    else
    {
        Ar << N.iSection << N.iFirstVertex << N.iLightMap;
        if (Ar.IsLoading() && Ar.Ver() < 108)
            N.iLightMap = INDEX_NONE;
    }

    if (!Ar.IsSaving() && !Ar.IsLoading())
        N.Unknown5C.CountBytes(Ar);
    if (Ar.IsLoading())
        N.NodeFlags &= 0x1f;
    return Ar;
}

// 0x000ab204
FArchive& operator<<(FArchive& Ar, FBspSurf& S)
{
    Ar << S.Material << S.PolyFlags;
    Ar << AR_INDEX(S.pBase) << AR_INDEX(S.vNormal) << AR_INDEX(S.vTextureU) << AR_INDEX(S.vTextureV);
    if (Ar.Ver() < 101)
    {
        INT Unused = INDEX_NONE;
        Ar << AR_INDEX(Unused);
    }
    Ar << AR_INDEX(S.Unknown18);
    if (Ar.Ver() < 78)
    {
        _WORD Unused1 = 0;
        _WORD Unused2 = 0;
        Ar << Unused1 << Unused2;
    }
    Ar << S.Unknown1C;
    if (Ar.Ver() > 86)
        Ar << S.Unknown28;
    if (Ar.Ver() >= 106)
        Ar << S.Unknown38;
    else if (Ar.IsLoading())
        S.Unknown38 = 32.f;
    S.Unknown20.CountBytes(Ar);
    return Ar;
}

// 0x000abd6a
void FPoly::Serialize(FArchive& Ar)
{
    // before version 139 the vertex count comes first and the vertices after the texture axes
    if (Ar.Ver() < 139)
    {
        INT NumVertices = Vertices.Num();
        Ar << AR_INDEX(NumVertices);
        Vertices.Set(NumVertices);
        Vertices.CountBytes(Ar);
    }
    else
    {
        Ar << Vertices;
    }
    Ar << Base << Unknown0C << TextureU << TextureV;
    if (Ar.Ver() < 139)
    {
        for (INT i = 0; i < Vertices.Num(); i++)
            Ar << Vertices[i];
    }
    Ar << Unknown38 << Unknown3C << Unknown40 << Unknown44;
    Ar << AR_INDEX(Unknown48) << AR_INDEX(Unknown4C);
    if (Ar.Ver() < 78)
    {
        // texture pans, folded into the texture origin
        SWORD PanU = 0;
        SWORD PanV = 0;
        Ar << PanU << PanV;
        if (Ar.IsLoading())
        {
            Base = Base - TextureU / TextureU.SizeSquared() * PanU;
            Base = Base - TextureV / TextureV.SizeSquared() * PanV;
        }
    }
    if (Ar.Ver() >= 106)
        Ar << Unknown88;
    else if (Ar.IsLoading())
        Unknown88 = 32.f;
}

// ---------------------------------------------------------------------------------------------- render data

// 0x000ae870
FArchive& operator<<(FArchive& Ar, FBspSection& S)
{
    Ar << S.VertexStream << S.Material << S.NumNodes << S.PolyFlags << S.iLightMapTexture;
    if (Ar.Ver() < 107 && Ar.IsLoading())
        S.iLightMapTexture = INDEX_NONE;
    return Ar;
}

// 0x000abb8e
FArchive& operator<<(FArchive& Ar, FModelUnknown30& U)
{
    Ar << U.Unknown0 << U.Unknown4 << U.UnknownC << U.Unknown10;
    if (Ar.Ver() > 90)
        Ar << U.Unknown2C;
    if (Ar.Ver() > 106)
        Ar << U.Unknown14 << U.Unknown18 << U.Unknown1C << U.Unknown20;
    return Ar;
}

// 0x000ade7f
FArchive& operator<<(FArchive& Ar, FLightMap& L)
{
    Ar << AR_INDEX(L.iTexture) << AR_INDEX(L.Unknown18) << AR_INDEX(L.Unknown1C) << AR_INDEX(L.OffsetX);
    Ar << AR_INDEX(L.OffsetY) << AR_INDEX(L.Unknown28) << AR_INDEX(L.Unknown2C);
    Ar << L.WorldToLightMap << L.Unknown70 << L.Unknown7C << L.Unknown88;
    if (Ar.Ver() >= 107)
    {
        Ar << L.Unknown94;
        if (Ar.Ver() < 108)
        {
            INT Unused = 0;
            Ar << Unused;
        }
    }
    if (Ar.Ver() >= 110)
    {
        Ar << L.Unknown10 << L.Revision;
    }
    else if (Ar.IsLoading())
    {
        L.Unknown10 = nullptr;
        L.Revision = 0;
    }
    return Ar;
}

// 0x000ac7b3
FArchive& operator<<(FArchive& Ar, FStaticLightMapTexture& T)
{
    T.FRenderResource::Serialize(Ar);
    T.Unknown10[0].Serialize(Ar);
    T.Unknown10[1].Serialize(Ar);
    return Ar << T.Unknown38 << T.Unknown3C << T.Unknown40 << T.Revision;
}

// 0x000adf7d
FArchive& operator<<(FArchive& Ar, FLightMapTexture& T)
{
    T.FRenderResource::Serialize(Ar);
    Ar << T.Unknown10 << T.Unknown14 << T.CacheId << T.Revision;
    if (Ar.Ver() >= 116)
        Ar << T.StaticTexture;
    return Ar;
}

// ---------------------------------------------------------------------------------------------- old formats

// 0x000aad21
FArchive& operator<<(FArchive& Ar, FModelLegacyC0& L)
{
    if (Ar.Ver() < 92)
    {
        INT Unused1;
        FVector Unused2;
        Ar << Unused1 << Unused2;
    }
    else
    {
        Ar << L.Unknown0 << L.Unknown4 << L.Unknown8 << L.Unknown48 << L.UnknownAC << L.UnknownAD;
    }
    if (Ar.Ver() >= 96)
    {
        Ar << L.Unknown88 << L.Unknown94 << L.UnknownA0;
    }
    else if (Ar.IsLoading())
    {
        // the origin and the first two axes of the second matrix
        const FMatrix& M = L.Unknown48;
        const FVector X(M.M[0][0], M.M[0][1], M.M[0][2]);
        const FVector Y(M.M[1][0], M.M[1][1], M.M[1][2]);
        const FVector Z(M.M[2][0], M.M[2][1], M.M[2][2]);
        const FVector W(M.M[3][0], M.M[3][1], M.M[3][2]);
        L.Unknown88 = (Z + Y + X) * 0.f + W;
        L.Unknown94 = (Z + Y) * 0.f + X;
        L.UnknownA0 = (Z + X) * 0.f + Y;
    }
    Ar << AR_INDEX(L.UnknownB0) << AR_INDEX(L.UnknownB4);
    if (Ar.Ver() < 92)
    {
        INT Unused1, Unused2, Unused3 = 0;
        Ar << Unused1 << Unused2 << Unused3;
    }
    else
    {
        Ar << AR_INDEX(L.UnknownB8) << AR_INDEX(L.UnknownBC);
    }
    return Ar;
}

// ---------------------------------------------------------------------------------------------- models

// 0x000aeda2
void UModel::Serialize(FArchive& Ar)
{
    Super::Serialize(Ar);
    // transactions leave the geometry alone
    if (!Ar.IsTrans())
        Ar << Vectors << Points << Nodes << Surfs << Verts;
    Ar << UnknownE0 << NumZones;
    for (INT i = 0; i < NumZones; i++)
        Ar << Zones[i];
    Ar << Polys;
    if (Polys && !Ar.IsTrans())
        Ar.Preload(Polys);

    if (Ar.Ver() < 92)
    {
        TArray<FModelLegacyC0> Skipped1;
        TArray<BYTE> Skipped2;
        Ar << Skipped1 << Skipped2;
    }
    else if (Ar.Ver() < 105)
    {
        TArray<FModelLegacyC0> Skipped1;
        TArray<FModelUnknown30> Skipped2;
        Ar << Skipped1 << Skipped2;
    }
    Ar << Unknown94 << Unknown9C << UnknownA4 << UnknownAC << UnknownD4 << UnknownD8;

    if (Ar.Ver() >= 92)
    {
        if (Ar.Ver() < 93)
        {
            FBspVertexStream Skipped;
            Ar << Skipped;
        }
        else if (Ar.Ver() < 105)
        {
            TArray<FBspVertexStream> Skipped;
            Ar << Skipped;
        }
        else
        {
            Ar << Sections;
        }
    }
    if (Ar.Ver() >= 105 && Ar.Ver() < 107)
    {
        TArray<UObject*> Skipped;
        Ar << Skipped;
    }
    if (Ar.Ver() >= 105)
    {
        Ar << LightMaps;
        // light maps before version 107 are thrown away
        if (Ar.Ver() < 107 && Ar.IsLoading())
            LightMaps.Empty();
    }
    if (Ar.Ver() >= 107 && Ar.Ver() < 110)
    {
        INT Unused = 0;
        Ar << Unused;
    }
    if (Ar.Ver() >= 110)
        Ar << LightMapTextures;
}

void UModel::ClearRenderData(URenderDevice* RenDev)
{
    if (Sections.Num() == 0)
        return;
    if (RenDev)
        NOT_DECOMPILED(0x000ae025);
    for (INT i = 0; i < Nodes.Num(); i++)
    {
        Nodes[i].iSection = INDEX_NONE;
        Nodes[i].iFirstVertex = INDEX_NONE;
    }
    Sections.Empty();
}

void UModel::BuildRenderData()
{
    ClearRenderData(nullptr);
    for (INT iNode = 0; iNode < Nodes.Num(); iNode++)
    {
        FBspNode& Node = Nodes[iNode];
        FBspSurf& Surf = Surfs[Node.iSurf];
        FLightMap* LightMap = Node.iLightMap != INDEX_NONE ? &LightMaps[Node.iLightMap] : nullptr;
        UMaterial* Material = Surf.Material;
        if (!Material)
            Material = static_cast<UMaterial*>(UMaterial::StaticClass()->GetDefaultObject())->DefaultMaterial;
        const DWORD PolyFlags = Surf.PolyFlags & 0x2400100;
        const INT iLightMapTexture = LightMap ? LightMap->iTexture : INDEX_NONE;
        if (Node.NumVertices == 0)
            continue;

        // a section with the same state and room for the vertices, else a new one
        FBspSection* Section = nullptr;
        for (INT i = 0; i < Sections.Num(); i++)
        {
            FBspSection& Other = Sections[i];
            if (Other.Material == Material && Other.PolyFlags == PolyFlags && Other.iLightMapTexture == iLightMapTexture
                && Other.VertexStream.Vertices.Num() + Node.NumVertices < 0xffff)
            {
                Section = &Other;
                break;
            }
        }
        if (!Section)
        {
            Section = new (&Sections[Sections.Add()]) FBspSection;
            Section->Material = Material;
            Section->PolyFlags = PolyFlags;
            Section->iLightMapTexture = iLightMapTexture;
        }
        Node.iSection = static_cast<INT>(Section - &Sections[0]);

        TArray<FBspVertex>& Vertices = Section->VertexStream.Vertices;
        Vertices.AddZeroed(Node.NumVertices);
        Node.iFirstVertex = Vertices.Num() - Node.NumVertices;

        const FMatrix M = FCoords(Points[Surf.pBase], Vectors[Surf.vTextureU], Vectors[Surf.vTextureV], Vectors[Surf.vNormal]).Matrix();
        const FLOAT InvUSize = 1.f / static_cast<FLOAT>(Material->MaterialUSize());
        const FLOAT InvVSize = 1.f / static_cast<FLOAT>(Material->MaterialVSize());
        FLOAT SumU = 0.f;
        FLOAT SumV = 0.f;
        for (INT i = 0; i < Node.NumVertices; i++)
        {
            FBspVertex& Vertex = Vertices[Node.iFirstVertex + i];
            const FVector P = Points[Verts[Node.iVertPool + i].pVertex];
            Vertex.Position = P;
            Vertex.U = InvUSize * (M.M[2][0] * P.Z + M.M[1][0] * P.Y + M.M[0][0] * P.X + M.M[3][0]);
            Vertex.V = InvVSize * (M.M[2][1] * P.Z + M.M[1][1] * P.Y + M.M[0][1] * P.X + M.M[3][1]);
            SumU += Vertex.U;
            SumV += Vertex.V;
            if (LightMap)
            {
                // light map textures are 512 texels square
                const FMatrix& L = LightMap->WorldToLightMap;
                Vertex.U2 = (static_cast<FLOAT>(LightMap->OffsetX) + (L.M[2][0] * P.Z + L.M[1][0] * P.Y + P.X * L.M[0][0] + L.M[3][0])) * (1.f / 512.f);
                Vertex.V2 = (static_cast<FLOAT>(LightMap->OffsetY) + (L.M[0][1] * P.X + L.M[2][1] * P.Z + L.M[1][1] * P.Y + L.M[3][1])) * (1.f / 512.f);
            }
            Vertex.Normal = FPackedNormal(Node.Plane.X, Node.Plane.Y, Node.Plane.Z);
        }

        // shift the texture coordinates by whole tiles so the node's average sits near 0
        const FLOAT OffsetU = static_cast<FLOAT>(appRound(SumU / Node.NumVertices));
        const FLOAT OffsetV = static_cast<FLOAT>(appRound(SumV / Node.NumVertices));
        for (INT i = 0; i < Node.NumVertices; i++)
        {
            Vertices[Node.iFirstVertex + i].U -= OffsetU;
            Vertices[Node.iFirstVertex + i].V -= OffsetV;
        }
        Section->NumNodes++;
        Section->VertexStream.Revision++;
    }
}

FPointRegion UModel::PointRegion(AZoneInfo* Zone, const FVector& Location) const
{
    FPointRegion Result{ Zone, INDEX_NONE, 0 };
    if (Nodes.Num())
    {
        INT iNode = 0;
        INT iParent;
        INT IsFront;
        do
        {
            iParent = iNode;
            const FPlane& Plane = Nodes[iNode].Plane;
            IsFront = (Plane.Z * Location.Z + Plane.Y * Location.Y + Plane.X * Location.X) - Plane.W >= 0.f;
            iNode = Nodes[iNode].iChild[IsFront];
        } while (iNode != INDEX_NONE);
        Result.iLeaf = Nodes[iParent].iLeaf[IsFront];
        Result.ZoneNumber = NumZones ? Nodes[iParent].iZone[IsFront] : 0;
        Result.Zone = static_cast<AZoneInfo*>(Zones[Result.ZoneNumber].ZoneActor);
        if (!Result.Zone)
            Result.Zone = Zone;
    }
    return Result;
}

// 0x0017b9a0. the model's box turned with the owner.
FBox UModel::GetCollisionBoundingBox(const AActor* Owner) const
{
    if (!Owner)
        return BoundingBox;
    const FMatrix LocalToWorld = Owner->LocalToWorld();
    FBox Box = FBox();
    for (INT X = 0; X < 2; X++)
        for (INT Y = 0; Y < 2; Y++)
            for (INT Z = 0; Z < 2; Z++)
            {
                const FVector Corner(X ? BoundingBox.Max.X : BoundingBox.Min.X, Y ? BoundingBox.Max.Y : BoundingBox.Min.Y,
                    Z ? BoundingBox.Max.Z : BoundingBox.Min.Z);
                Box += LocalToWorld.TransformFVector(Corner);
            }
    return Box;
}

// returns 0 when the point or box is inside the model's solid space
UBOOL UModel::PointCheck(FCheckResult& Result, AActor* Owner, const FVector& Location, const FVector& Extent, DWORD ExtraNodeFlags)
{
    Result.Normal = FVector(0, 0, 0);
    Result.Location = Location;
    Result.Primitive = this;
    Result.Actor = Owner;
    Result.Time = 0.f;
    UBOOL Outside = UnknownD4;
    if (Nodes.Num() == 0)
        return Outside;

    if (!Extent.IsZero())
    {
        // the box walk, through a check info set up by 0x0017bbab
        NOT_DECOMPILED(0x0017bbf4);
        return Outside;
    }

    // walk down the tree with the planes moved to where the owner is
    const FMatrix ToWorld = Owner ? Owner->LocalToWorld() : FMatrix::Identity;
    const FMatrix ToWorldTA = ToWorld.TransposeAdjoint();
    const FLOAT Det = ToWorld.Determinant();
    INT iNode = 0;
    INT IsFront = 0;
    INT iPrevNode;
    do
    {
        const FBspNode& Node = Nodes[iNode];
        FVector Normal = ToWorldTA.TransformNormal(Node.Plane).SafeNormal();
        if (Det < 0.f)
            Normal = Normal * -1.f;
        const FVector Base = ToWorld.TransformFVector(Node.Plane * Node.Plane.W);
        IsFront = ((Location | Normal) - (Base | Normal)) > 0.f;
        Outside = Node.ChildOutside(IsFront, Outside);
        iPrevNode = iNode;
        iNode = Node.iChild[IsFront];
    } while (iNode != INDEX_NONE);
    Result.Item = IsFront + iPrevNode * 2;
    return Outside;
}

// ---------------------------------------------------------------------------------------------- polys

// 0x0009d490
void UPolys::Serialize(FArchive& Ar)
{
    Super::Serialize(Ar);
    if (Ar.IsTrans())
        return;
    if (Ar.Ver() >= 139)
    {
        Ar << Polys;
        return;
    }
    // older packages store the count twice as plain ints and load into zeroed polys
    Polys.CountBytes(Ar);
    INT Num = Polys.Num();
    INT Max = Num;
    Ar << Num << Max;
    if (Ar.IsLoading())
        Polys.Set(Num);
    for (INT i = 0; i < Polys.Num(); i++)
        Ar << Polys[i];
}
