#pragma once

#include "UnRenderResource.h"

// ---------------------------------------------------------------------------------------------- bsp

struct FBspNode
{
    friend FArchive& operator<<(FArchive& Ar, FBspNode& N);

    FPlane Plane;
    QWORD  Unknown10;
    INT    Unknown18;
    INT    Unknown1C;
    INT    Unknown20;
    INT    Unknown24;
    INT    Unknown28;
    FPlane Unknown2C;
    INT    Unknown3C;
    INT    Unknown40;
    BYTE   Unknown44;
    BYTE   Unknown45;
    BYTE   Unknown46;
    // only the low 5 bits survive loading
    BYTE   Unknown47;
    INT    Unknown48;
    INT    Unknown4C;
    INT    Unknown50;
    INT    Unknown54;
    INT    Unknown58;
    // reference counted objects, released by the model when it empties; never serialized
    TArray<void*> Unknown5C;
};

struct FBspSurf
{
    friend FArchive& operator<<(FArchive& Ar, FBspSurf& S);

    UObject*    Unknown0;
    DWORD       Unknown4;
    INT         Unknown8;
    INT         UnknownC;
    INT         Unknown10;
    INT         Unknown14;
    INT         Unknown18;
    UObject*    Unknown1C;
    // only counted, never serialized
    TArray<INT> Unknown20;
    FPlane      Unknown28;
    FLOAT       Unknown38;
};

struct FModelUnknown64
{
    INT Unknown0;
    INT Unknown4;

    // 0x000aaf5c
    friend FArchive& operator<<(FArchive& Ar, FModelUnknown64& U)
    {
        return Ar << AR_INDEX(U.Unknown0) << AR_INDEX(U.Unknown4);
    }
};

struct FZoneProperties
{
    UObject* Unknown0;
    INT      Unknown4;
    QWORD    Unknown8;
    QWORD    Unknown10;

    // 0x000aa79f
    friend FArchive& operator<<(FArchive& Ar, FZoneProperties& Z)
    {
        return Ar << Z.Unknown0 << Z.Unknown8 << Z.Unknown10 << Z.Unknown4;
    }
};

struct FModelUnknownA4
{
    INT   Unknown0;
    INT   Unknown4;
    QWORD Unknown8;

    // 0x000aacd9. packages before version 156 have a third index, dropped here.
    friend FArchive& operator<<(FArchive& Ar, FModelUnknownA4& U)
    {
        Ar << AR_INDEX(U.Unknown0) << AR_INDEX(U.Unknown4);
        if (Ar.Ver() < 156)
        {
            INT Unused;
            Ar << AR_INDEX(Unused);
        }
        return Ar << U.Unknown8;
    }
};

// a polygon of a brush or of the polys a model was built from
struct FPoly
{
    // 0x00084610
    FPoly() = default;

    void Serialize(FArchive& Ar);

    friend FArchive& operator<<(FArchive& Ar, FPoly& P)
    {
        P.Serialize(Ar);
        return Ar;
    }

    // the origin and axes of the texture mapping
    FVector         Base;
    FVector         Unknown0C;
    FVector         TextureU;
    FVector         TextureV;
    TArray<FVector> Vertices;
    DWORD           Unknown38;
    UObject*        Unknown3C;
    UObject*        Unknown40;
    FName           Unknown44;
    INT             Unknown48;
    INT             Unknown4C;
    INT             Unknown50[4];
    FVector         Unknown60[3];
    INT             Unknown84;
    FLOAT           Unknown88;
};

// ---------------------------------------------------------------------------------------------- render data

struct FBspVertex
{
    FVector       Position;
    FPackedNormal Normal;
    FLOAT         Unknown10;
    FLOAT         Unknown14;
    FLOAT         Unknown18;
    FLOAT         Unknown1C;

    // 0x000ab555. the normal is stored from version 109 on.
    friend FArchive& operator<<(FArchive& Ar, FBspVertex& V)
    {
        Ar << V.Position << V.Unknown10 << V.Unknown14 << V.Unknown18 << V.Unknown1C;
        if (Ar.Ver() > 108)
            Ar << V.Normal;
        return Ar;
    }
};

class FBspVertexStream : public FRenderResource
{
public:
    // 0x000ab5e7
    FBspVertexStream() { CacheId = MakeCacheId(RRT_VertexStream); Revision = 0; }

    // 0x000ae52a
    friend FArchive& operator<<(FArchive& Ar, FBspVertexStream& S)
    {
        S.FRenderResource::Serialize(Ar);
        Ar << S.Vertices;
        return Ar << S.Revision;
    }

    TArray<FBspVertex> Vertices;
};

// the bsp surfaces drawn with one material
struct FBspSection
{
    // 0x000abc11
    FBspSection() : Unknown18(0), Unknown1C(nullptr), Unknown20(0), Unknown24(INDEX_NONE) {}

    friend FArchive& operator<<(FArchive& Ar, FBspSection& S);

    FBspVertexStream VertexStream;
    INT              Unknown18;
    UObject*         Unknown1C;
    INT              Unknown20;
    INT              Unknown24;
};

struct FModelUnknown30
{
    friend FArchive& operator<<(FArchive& Ar, FModelUnknown30& U);

    UObject*     Unknown0;
    TArray<BYTE> Unknown4;
    INT          UnknownC;
    INT          Unknown10;
    INT          Unknown14;
    INT          Unknown18;
    INT          Unknown1C;
    INT          Unknown20;
    INT          Unknown24;
    INT          Unknown28;
    INT          Unknown2C;
};

class FLightMap : public FRenderResource
{
public:
    // 0x000ade51
    FLightMap() : Unknown10(nullptr), Unknown9C(0), UnknownA0(0) {}

    friend FArchive& operator<<(FArchive& Ar, FLightMap& L);

    UObject*                Unknown10;
    INT                     Unknown14;
    INT                     Unknown18;
    INT                     Unknown1C;
    INT                     Unknown20;
    INT                     Unknown24;
    INT                     Unknown28;
    INT                     Unknown2C;
    FMatrix                 Unknown30;
    FVector                 Unknown70;
    FVector                 Unknown7C;
    FVector                 Unknown88;
    TArray<FModelUnknown30> Unknown94;
    INT                     Unknown9C;
    INT                     UnknownA0;
};

class FStaticLightMapTexture : public FRenderResource
{
public:
    // 0x000ae78d
    FStaticLightMapTexture() : Unknown38(0), Unknown3C(0), Unknown40(0) { CacheId = MakeCacheId(RRT_Texture); Revision = 0; }

    friend FArchive& operator<<(FArchive& Ar, FStaticLightMapTexture& T);

    TLazyArray<BYTE> Unknown10[2];
    BYTE             Unknown38;
    INT              Unknown3C;
    INT              Unknown40;
};

class FLightMapTexture : public FRenderResource
{
public:
    // 0x000ae81b
    FLightMapTexture() : Unknown10(nullptr) {}

    friend FArchive& operator<<(FArchive& Ar, FLightMapTexture& T);

    UObject*               Unknown10;
    TArray<INT>            Unknown14;
    FStaticLightMapTexture StaticTexture;
};

// ---------------------------------------------------------------------------------------------- old formats

// read and dropped by packages before version 105
struct FModelLegacyC0
{
    friend FArchive& operator<<(FArchive& Ar, FModelLegacyC0& L);

    INT     Unknown0;
    INT     Unknown4;
    FMatrix Unknown8;
    FMatrix Unknown48;
    FVector Unknown88;
    FVector Unknown94;
    FVector UnknownA0;
    BYTE    UnknownAC;
    BYTE    UnknownAD;
    INT     UnknownB0;
    INT     UnknownB4;
    INT     UnknownB8;
    INT     UnknownBC;
};
