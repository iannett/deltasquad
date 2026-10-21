#pragma once

#include "UnRenderResource.h"

class UMaterial;

// a run of the index buffer drawn with one material
struct FStaticMeshSection
{
    // 0x00095034
    FStaticMeshSection()
        : Unknown0(0), IsStrip(0), FirstIndex(0), MinVertexIndex(0xffff), MaxVertexIndex(0xffff)
        , NumTriangles(0), NumPrimitives(0), Unknown14(nullptr)
    {}

    friend FArchive& operator<<(FArchive& Ar, FStaticMeshSection& S);

    INT   Unknown0;
    INT   IsStrip;
    _WORD FirstIndex;
    _WORD MinVertexIndex;
    _WORD MaxVertexIndex;
    _WORD NumTriangles;
    _WORD NumPrimitives;
    // an object with a vtable that serializes itself; null after loading
    void* Unknown14;
};

struct FStaticMeshVertex
{
    FVector       Position;
    FPackedNormal Normal;

    // 0x000959eb
    friend FArchive& operator<<(FArchive& Ar, FStaticMeshVertex& V)
    {
        return Ar << V.Position << V.Normal;
    }
};

struct FStaticMeshUV
{
    FLOAT U;
    FLOAT V;

    // 0x000952ee
    friend FArchive& operator<<(FArchive& Ar, FStaticMeshUV& UV)
    {
        return Ar << UV.U << UV.V;
    }
};

struct FStaticMeshTangentBinormal
{
    FPackedNormal Tangent;
    FPackedNormal Binormal;

    // 0x00095a0b
    friend FArchive& operator<<(FArchive& Ar, FStaticMeshTangentBinormal& T)
    {
        return Ar << T.Tangent << T.Binormal;
    }
};

class FStaticMeshVertexStream : public FRenderResource
{
public:
    // 0x00097d52
    FStaticMeshVertexStream() { CacheId = MakeCacheId(RRT_VertexStream); Revision = 0; }

    friend FArchive& operator<<(FArchive& Ar, FStaticMeshVertexStream& S);

    TArray<FStaticMeshVertex> Vertices;
};

class FStaticMeshColorStream : public FRenderResource
{
public:
    // 0x00097de3
    FStaticMeshColorStream() { CacheId = MakeCacheId(RRT_VertexStream); Revision = 0; }

    friend FArchive& operator<<(FArchive& Ar, FStaticMeshColorStream& S);

    TArray<FColor> Colors;
};

class FStaticMeshUVStream : public FRenderResource
{
public:
    // 0x000984d3
    FStaticMeshUVStream() : CoordinateIndex(0) { CacheId = MakeCacheId(RRT_VertexStream); Revision = 0; }

    friend FArchive& operator<<(FArchive& Ar, FStaticMeshUVStream& S);

    TArray<FStaticMeshUV> UVs;
    INT                   CoordinateIndex;
};

// only in packages saved with a licensee version
class FStaticMeshTangentStream : public FRenderResource
{
public:
    // 0x00096d42
    FStaticMeshTangentStream() { CacheId = MakeCacheId(RRT_VertexStream); Revision = 0; }

    friend FArchive& operator<<(FArchive& Ar, FStaticMeshTangentStream& S);

    TArray<FStaticMeshTangentBinormal> Tangents;
};

// the mesh as imported, kept in the package and only loaded on demand
struct FStaticMeshTriangle
{
    // 0x0009537a
    FStaticMeshTriangle()
    {
        for (INT i = 0; i < 3; i++)
            Unknown24[i] = FVector(0.f, 0.f, 0.f);
    }

    friend FArchive& operator<<(FArchive& Ar, FStaticMeshTriangle& T);

    FVector       Vertices[3];
    FVector       Unknown24[3];
    FStaticMeshUV UVs[3][8];
    FColor        Colors[3];
    INT           MaterialIndex;
    DWORD         SmoothingMask;
    INT           NumUVs;
    // packages before version 112 store the material and poly flags here instead of MaterialIndex
    UObject*      LegacyMaterial;
    DWORD         LegacyPolyFlags;
};

struct FStaticMeshCollisionTriangle
{
    FStaticMeshCollisionTriangle() : VertexIndices{0, 0, 0}, Unknown6(0), MaterialIndex(0), UnknownC(0) {}

    // 0x000953ec
    friend FArchive& operator<<(FArchive& Ar, FStaticMeshCollisionTriangle& T)
    {
        return Ar << T.VertexIndices[0] << T.VertexIndices[1] << T.VertexIndices[2] << AR_INDEX(T.MaterialIndex);
    }

    _WORD VertexIndices[3];
    _WORD Unknown6;
    INT   MaterialIndex;
    INT   UnknownC;
};

// a node of the collision tree. the bounds are 16-bit.
struct FStaticMeshCollisionNode
{
    FStaticMeshCollisionNode() : TriangleIndex(0), CoplanarIndex(0), ChildIndices{0, 0}, BoundsMin{0, 0, 0}, BoundsMax{0, 0, 0} {}

    // 0x00095422
    friend FArchive& operator<<(FArchive& Ar, FStaticMeshCollisionNode& N)
    {
        Ar << N.TriangleIndex << N.CoplanarIndex << N.ChildIndices[0] << N.ChildIndices[1];
        // 0x0009506d for each corner
        Ar << N.BoundsMin[0] << N.BoundsMin[1] << N.BoundsMin[2];
        return Ar << N.BoundsMax[0] << N.BoundsMax[1] << N.BoundsMax[2];
    }

    _WORD TriangleIndex;
    _WORD CoplanarIndex;
    _WORD ChildIndices[2];
    _WORD BoundsMin[3];
    _WORD BoundsMax[3];
};

// an element of the Materials array, described to script by the static constructor
struct FStaticMeshMaterial
{
    UMaterial* Material;
    UBOOL      EnableCollision;
};

// materials of static meshes before version 92, and of static mesh instances
struct FStaticMeshLegacyMaterial
{
    UObject* Material;
    INT      Unknown4;

    // 0x0009741b
    friend FArchive& operator<<(FArchive& Ar, FStaticMeshLegacyMaterial& M)
    {
        Ar << M.Material;
        if (Ar.Ver() < 147)
        {
            TArray<BYTE> Skipped;
            Ar << Skipped;
        }
        return Ar << M.Unknown4;
    }
};
