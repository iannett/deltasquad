#pragma once

#include "Core.h"

struct McdModel;

// karma works in metres; one is 50 unreal units
const FLOAT K_ME2UScale = 50.f;

// 0x0007ea38. a karma transform: the rotation and the position in metres.
inline void KU2METransform(FLOAT* TM, const FVector& Position, const FRotator& Rotation)
{
    FMatrix& M = *reinterpret_cast<FMatrix*>(TM);
    M = FRotationTranslationMatrix(Rotation, Position);
    M.M[3][0] *= 1.f / K_ME2UScale;
    M.M[3][1] *= 1.f / K_ME2UScale;
    M.M[3][2] *= 1.f / K_ME2UScale;
    M.M[3][3] = 1.f;
}

// 0x0007eb49
inline FVector KME2UPosition(const FLOAT* V)
{
    return FVector(V[0] * K_ME2UScale, V[1] * K_ME2UScale, V[2] * K_ME2UScale);
}

struct FKSphereElem
{
    FMatrix Unknown0;
    FLOAT   Unknown40;

    // 0x0008efbe
    friend FArchive& operator<<(FArchive& Ar, FKSphereElem& E)
    {
        return Ar << E.Unknown0 << E.Unknown40;
    }
};

struct FKBoxElem
{
    FMatrix Unknown0;
    FLOAT   Unknown40;
    FLOAT   Unknown44;
    FLOAT   Unknown48;

    // 0x0008f17d
    friend FArchive& operator<<(FArchive& Ar, FKBoxElem& E)
    {
        return Ar << E.Unknown0 << E.Unknown40 << E.Unknown44 << E.Unknown48;
    }
};

struct FKCylinderElem
{
    FMatrix Unknown0;
    FLOAT   Unknown40;
    FLOAT   Unknown44;

    // 0x0008f1b7
    friend FArchive& operator<<(FArchive& Ar, FKCylinderElem& E)
    {
        return Ar << E.Unknown0 << E.Unknown40 << E.Unknown44;
    }
};

struct FKConvexElem
{
    // 0x0008f3b2
    FKConvexElem() = default;

    // 0x0008f3c3
    friend FArchive& operator<<(FArchive& Ar, FKConvexElem& E)
    {
        return Ar << E.Unknown0 << E.Unknown40;
    }

    FMatrix         Unknown0;
    TArray<FVector> Unknown40;
    // never serialized
    TArray<FPlane>  Unknown48;
};

// the collision shapes of a mesh
struct FKAggregateGeom
{
    friend FArchive& operator<<(FArchive& Ar, FKAggregateGeom& G);

    TArray<FKSphereElem>   SphereElems;
    TArray<FKBoxElem>      BoxElems;
    TArray<FKCylinderElem> CylinderElems;
    TArray<FKConvexElem>   ConvexElems;
    TArray<FKCylinderElem> Unknown20;
};
