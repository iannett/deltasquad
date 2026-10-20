#pragma once

#include "Core.h"

// at 0x0045a770 on the xbox. every render resource takes the next number for its cache id.
extern QWORD GRenderResourceCount;

// data the renderer caches by id. the xbox gives these a vtable for the renderer; the port adds virtuals
// when that code is decompiled.
class FRenderResource
{
public:
    FRenderResource() : CacheId(0), Revision(1) {}

    // the low byte of a cache id is the resource type
    static QWORD MakeCacheId(BYTE Type)
    {
        return GRenderResourceCount++ * 0x100 + Type;
    }

    void Serialize(FArchive& Ar);

    QWORD CacheId;
    INT   Revision;
};

// render resource types in the low byte of a cache id
enum ERenderResourceType
{
    RRT_Texture      = 0xe0,
    RRT_VertexStream = 0xe1,
    RRT_IndexBuffer  = 0xe2,
};

// a unit vector packed as 11, 11 and 10 bit signed fractions. packages store it as a full FVector.
struct FPackedNormal
{
    DWORD Packed;

    FPackedNormal() = default;
    // 0x00061ce7
    FPackedNormal(FLOAT X, FLOAT Y, FLOAT Z)
    {
        const FVector N = FVector(X, Y, Z).SafeNormal();
        Packed = (static_cast<DWORD>(appRound(N.X * 1023.f)) & 0x7ff)
            | ((static_cast<DWORD>(appRound(N.Y * 1023.f)) << 11) & 0x3ff800)
            | (static_cast<DWORD>(appRound(N.Z * 511.f)) << 22);
    }

    FVector Unpack() const
    {
        return FVector(
            static_cast<FLOAT>(static_cast<INT>(Packed << 21) >> 21) * (1.f / 1023.f),
            static_cast<FLOAT>(static_cast<INT>(Packed << 10) >> 21) * (1.f / 1023.f),
            static_cast<FLOAT>(static_cast<INT>(Packed) >> 22) * (1.f / 511.f));
    }

    // 0x0006688c
    friend FArchive& operator<<(FArchive& Ar, FPackedNormal& N)
    {
        FVector V;
        if (Ar.IsLoading())
        {
            Ar << V;
            N = FPackedNormal(V.X, V.Y, V.Z);
        }
        else if (Ar.IsSaving())
        {
            V = N.Unpack();
            Ar << V;
        }
        else
        {
            Ar << N.Packed;
        }
        return Ar;
    }
};

// a vertex with a normal and one uv
struct FUntransformedVertex
{
    FVector Position;
    FVector Normal;
    FLOAT   U;
    FLOAT   V;

    // 0x000383bb
    friend FArchive& operator<<(FArchive& Ar, FUntransformedVertex& Vert)
    {
        return Ar << Vert.Position << Vert.Normal << Vert.U << Vert.V;
    }
};

// 16-bit vertex indices
class FRawIndexBuffer : public FRenderResource
{
public:
    FRawIndexBuffer() { CacheId = MakeCacheId(RRT_IndexBuffer); Revision = 0; }

    // 0x00039b44
    friend FArchive& operator<<(FArchive& Ar, FRawIndexBuffer& B)
    {
        B.FRenderResource::Serialize(Ar);
        Ar << B.Indices;
        Ar << B.Revision;
        return Ar;
    }

    TArray<_WORD> Indices;
};

// 0x0009869b for static mesh vertices, 0x0009833a for colours, 0x00097f43 for uvs and 0x00044d09 for skin vertices.
// the xbox keeps these arrays in memory the gpu reads and locks it around the load; the port uses plain arrays.
template<typename T>
void SerializeStreamData(FArchive& Ar, TArray<T>& Data)
{
    if (Ar.IsLoading())
    {
        if (Ar.ArSkipObjectRefs)
        {
            Ar << Data;
            return;
        }
        INT Count;
        Ar << AR_INDEX(Count);
        // an empty stream leaves the array as it was
        if (Count > 0)
        {
            Data.Empty(Count);
            Data.AddZeroed(Count);
            for (INT i = 0; i < Count; i++)
                Ar << Data[i];
        }
    }
    else if (Ar.IsSaving())
    {
        Ar << Data;
    }
    if (Ar.IsCountingMem())
        Data.CountBytes(Ar);
}
