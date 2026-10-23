#pragma once

#include "UnRenderResource.h"

// ---------------------------------------------------------------------------------------------- terrain

// a terrain vertex as the renderer streams it (name from CT's ATerrainInfo::GetVertexFromStreamData)
struct FTerrainVertex
{
    FVector       Position;
    FPackedNormal Normal;
    FColor        Color;
    FLOAT         U;
    FLOAT         V;

    // 0x000671a5
    friend FArchive& operator<<(FArchive& Ar, FTerrainVertex& Vert)
    {
        return Ar << Vert.Position << Vert.Normal << Vert.Color << Vert.U << Vert.V;
    }
};

// name invented
class FTerrainVertexStream : public FRenderResource
{
public:
    // 0x000692ec
    FTerrainVertexStream() { CacheId = MakeCacheId(RRT_VertexStream); Revision = 0; }

    // 0x0006b5a9
    friend FArchive& operator<<(FArchive& Ar, FTerrainVertexStream& S)
    {
        S.FRenderResource::Serialize(Ar);
        return Ar << S.Vertices << S.Revision;
    }

    TArray<FTerrainVertex> Vertices;
};

// an element of ATerrainInfo::RenderCombinations (name invented)
struct FTerrainRenderCombination
{
    TArray<INT> Unknown0;
    BYTE        Unknown8;
    UObject*    UnknownC;

    // 0x0006955b
    friend FArchive& operator<<(FArchive& Ar, FTerrainRenderCombination& C)
    {
        return Ar << C.Unknown0 << C.Unknown8 << C.UnknownC;
    }
};

// an element of ATerrainInfo::SelectedVertices (name invented)
struct FSelectedTerrainVertex
{
    INT Unknown0;
    INT Unknown4;
    INT Unknown8;
    INT UnknownC;
    INT Unknown10;

    // 0x000621b6
    friend FArchive& operator<<(FArchive& Ar, FSelectedTerrainVertex& V)
    {
        return Ar << V.Unknown0 << V.Unknown4 << V.UnknownC << V.Unknown8 << V.Unknown10;
    }
};

// an element of UTerrainSector::Unknown28
struct FTerrainSectorUnknown28
{
    INT           Unknown0;
    INT           Unknown4;
    TArray<_WORD> Unknown8;
    INT           Unknown10;
    INT           Unknown14;
    INT           Unknown18;
    INT           Unknown1C;

    FTerrainSectorUnknown28() : Unknown4(INDEX_NONE), Unknown10(0), Unknown14(0), Unknown18(0), Unknown1C(0) {}

    // 0x00069584
    friend FArchive& operator<<(FArchive& Ar, FTerrainSectorUnknown28& U)
    {
        return Ar << U.Unknown4 << U.Unknown8 << U.Unknown10 << U.Unknown14 << U.Unknown18 << U.Unknown1C;
    }
};

// an element of UTerrainSector::Unknown30. entries whose object is gone are dropped after loading.
struct FTerrainSectorUnknown30
{
    UObject*     Unknown0;
    TArray<BYTE> Unknown4;

    // 0x000695d9
    friend FArchive& operator<<(FArchive& Ar, FTerrainSectorUnknown30& U)
    {
        return Ar << U.Unknown0 << U.Unknown4;
    }
};
