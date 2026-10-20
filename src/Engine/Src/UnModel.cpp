#include "EngineClasses.h"

// ---------------------------------------------------------------------------------------------- bsp

// 0x000ab6c2
FArchive& operator<<(FArchive& Ar, FBspNode& N)
{
    Ar << N.Plane << N.Unknown10 << N.Unknown47;
    Ar << AR_INDEX(N.Unknown18) << AR_INDEX(N.Unknown1C) << AR_INDEX(N.Unknown20) << AR_INDEX(N.Unknown24) << AR_INDEX(N.Unknown28);
    Ar << AR_INDEX(N.Unknown3C) << AR_INDEX(N.Unknown40);
    Ar << N.Unknown2C;
    if (Ar.Ver() < 135)
    {
        FPlane Unused(0.f, 0.f, 0.f, 0.f);
        Ar << Unused;
    }
    Ar << N.Unknown44 << N.Unknown45 << N.Unknown46 << N.Unknown48 << N.Unknown4C;

    if (Ar.Ver() < 92)
    {
        N.Unknown50 = N.Unknown54 = N.Unknown58 = INDEX_NONE;
    }
    else if (Ar.Ver() < 101)
    {
        // one dropped value at version 92, two up to 100
        INT Unused = INDEX_NONE;
        if (Ar.Ver() >= 93)
            Ar << Unused;
        Ar << Unused;
        N.Unknown50 = N.Unknown54 = N.Unknown58 = INDEX_NONE;
    }
    else
    {
        Ar << N.Unknown50 << N.Unknown54 << N.Unknown58;
        if (Ar.IsLoading() && Ar.Ver() < 108)
            N.Unknown58 = INDEX_NONE;
    }

    if (!Ar.IsSaving() && !Ar.IsLoading())
        N.Unknown5C.CountBytes(Ar);
    if (Ar.IsLoading())
        N.Unknown47 &= 0x1f;
    return Ar;
}

// 0x000ab204
FArchive& operator<<(FArchive& Ar, FBspSurf& S)
{
    Ar << S.Unknown0 << S.Unknown4;
    Ar << AR_INDEX(S.Unknown8) << AR_INDEX(S.UnknownC) << AR_INDEX(S.Unknown10) << AR_INDEX(S.Unknown14);
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
    Ar << S.VertexStream << S.Unknown1C << S.Unknown18 << S.Unknown20 << S.Unknown24;
    if (Ar.Ver() < 107 && Ar.IsLoading())
        S.Unknown24 = INDEX_NONE;
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
    Ar << AR_INDEX(L.Unknown14) << AR_INDEX(L.Unknown18) << AR_INDEX(L.Unknown1C) << AR_INDEX(L.Unknown20);
    Ar << AR_INDEX(L.Unknown24) << AR_INDEX(L.Unknown28) << AR_INDEX(L.Unknown2C);
    Ar << L.Unknown30 << L.Unknown70 << L.Unknown7C << L.Unknown88;
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
        Ar << Unknown70 << Unknown7C << Nodes << Surfs << Unknown64;
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
