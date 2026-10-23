#pragma once

#include <cmath>

#include "CoreTypes.h"
#include "UnTemplate.h"

// the xbox converts with sse in its default round-to-nearest-even mode, switching to round-down for floor
inline INT appRound(FLOAT F) { return static_cast<INT>(nearbyintf(F)); }
inline INT appFloor(FLOAT F) { return static_cast<INT>(floorf(F)); }
// 0x0003ae89
inline INT appCeil(FLOAT F) { return static_cast<INT>(ceilf(F)); }
// the xbox inlines an sse reciprocal square root estimate with one newton step; exact here
inline FLOAT appSqrt(FLOAT F) { return sqrtf(F); }
inline FLOAT appInvSqrt(FLOAT F) { return 1.f / sqrtf(F); }

// angles are 16-bit fixed point: 65536 is a full turn
enum { NUM_ANGLES = 16384, ANGLE_SHIFT = 2, ANGLE_MASK = NUM_ANGLES - 1, NUM_SQRTS = 16384 };

struct FRotator;

struct FVector
{
    FLOAT X, Y, Z;

    FVector() = default;
    constexpr FVector(FLOAT InX, FLOAT InY, FLOAT InZ) : X(InX), Y(InY), Z(InZ) {}

    FVector operator+(const FVector& V) const { return FVector(X + V.X, Y + V.Y, Z + V.Z); }
    FVector operator-(const FVector& V) const { return FVector(X - V.X, Y - V.Y, Z - V.Z); }
    FVector operator*(FLOAT Scale) const { return FVector(X * Scale, Y * Scale, Z * Scale); }
    // divides by multiplying with the reciprocal, as the xbox does
    FVector operator/(FLOAT Scale) const { const FLOAT RScale = 1.f / Scale; return FVector(X * RScale, Y * RScale, Z * RScale); }
    FVector operator*(const FVector& V) const { return FVector(X * V.X, Y * V.Y, Z * V.Z); }
    FVector operator-() const { return FVector(0.f - X, 0.f - Y, 0.f - Z); }
    // dot product
    FLOAT operator|(const FVector& V) const { return X * V.X + Y * V.Y + Z * V.Z; }
    // cross product
    FVector operator^(const FVector& V) const { return FVector(Y * V.Z - Z * V.Y, Z * V.X - X * V.Z, X * V.Y - Y * V.X); }
    UBOOL operator==(const FVector& V) const { return X == V.X && Y == V.Y && Z == V.Z; }
    UBOOL operator!=(const FVector& V) const { return X != V.X || Y != V.Y || Z != V.Z; }

    FLOAT SizeSquared() const { return X * X + Y * Y + Z * Z; }
    FLOAT Size() const { return appSqrt(SizeSquared()); }
    FLOAT Size2D() const { return appSqrt(X * X + Y * Y); }
    UBOOL IsZero() const { return X == 0.f && Y == 0.f && Z == 0.f; }
    // 0x000384d5
    FLOAT GetAbsMax() const
    {
        FLOAT Max = fabsf(Y) > fabsf(X) ? fabsf(Y) : fabsf(X);
        return fabsf(Z) > Max ? fabsf(Z) : Max;
    }
    // unit length, or unchanged when zero
    FVector SafeNormal() const
    {
        const FLOAT SquareSum = SizeSquared();
        return SquareSum != 0.f ? *this * appInvSqrt(SquareSum) : *this;
    }
    // 0x00133a41
    FRotator Rotation() const;
};

struct FRotator
{
    INT Pitch, Yaw, Roll;

    FRotator() = default;
    constexpr FRotator(INT InPitch, INT InYaw, INT InRoll) : Pitch(InPitch), Yaw(InYaw), Roll(InRoll) {}

    FRotator operator+(const FRotator& R) const { return FRotator(Pitch + R.Pitch, Yaw + R.Yaw, Roll + R.Roll); }
    FRotator operator-(const FRotator& R) const { return FRotator(Pitch - R.Pitch, Yaw - R.Yaw, Roll - R.Roll); }
    FRotator operator*(FLOAT Scale) const
    {
        return FRotator(static_cast<INT>(Pitch * Scale), static_cast<INT>(Yaw * Scale), static_cast<INT>(Roll * Scale));
    }
    // 0x000b6caf
    FRotator operator*=(FLOAT Scale)
    {
        Pitch = static_cast<INT>(Pitch * Scale);
        Yaw = static_cast<INT>(Yaw * Scale);
        Roll = static_cast<INT>(Roll * Scale);
        return *this;
    }
    UBOOL operator==(const FRotator& R) const { return Pitch == R.Pitch && Yaw == R.Yaw && Roll == R.Roll; }
    UBOOL operator!=(const FRotator& R) const { return Pitch != R.Pitch || Yaw != R.Yaw || Roll != R.Roll; }

    // 0x0003f990. only the low 16 bits of each angle count.
    UBOOL IsZero() const { return !(Pitch & 65535) && !(Yaw & 65535) && !(Roll & 65535); }
    // 0x001339d0. the unit vector this rotation points along.
    FVector Vector() const;
};

struct FPlane : public FVector
{
    FLOAT W;

    FPlane() = default;
    constexpr FPlane(const FVector& V, FLOAT InW) : FVector(V), W(InW) {}
    constexpr FPlane(FLOAT InX, FLOAT InY, FLOAT InZ, FLOAT InW) : FVector(InX, InY, InZ), W(InW) {}
};

// a centre and a radius
struct FSphere : public FPlane
{
    FSphere() = default;
};

// a bounding box; IsValid is 0 for an empty box
struct FBox
{
    FVector Min;
    FVector Max;
    BYTE    IsValid;

    // 0x00061b3b
    FBox ExpandBy(FLOAT W) const
    {
        FBox Result;
        Result.Min = FVector(Min.X - W, Min.Y - W, Min.Z - W);
        Result.Max = FVector(Max.X + W, Max.Y + W, Max.Z + W);
        Result.IsValid = 1;
        return Result;
    }
    // 0x00076548
    FVector GetCenter() const { return FVector((Min.X + Max.X) * 0.5f, (Min.Y + Max.Y) * 0.5f, (Min.Z + Max.Z) * 0.5f); }
    // 0x000643ab. the box around the eight corners moved into Coords.
    FBox TransformBy(const struct FCoords& Coords) const;
    // half the size on each axis
    FVector GetExtent() const { return FVector((Max.X - Min.X) * 0.5f, (Max.Y - Min.Y) * 0.5f, (Max.Z - Min.Z) * 0.5f); }
    // 0x000435b6. grows the box to hold Other.
    FBox& operator+=(const FVector& Other)
    {
        if (IsValid)
        {
            Min = FVector(Min.X < Other.X ? Min.X : Other.X, Min.Y < Other.Y ? Min.Y : Other.Y, Min.Z < Other.Z ? Min.Z : Other.Z);
            Max = FVector(Max.X > Other.X ? Max.X : Other.X, Max.Y > Other.Y ? Max.Y : Other.Y, Max.Z > Other.Z ? Max.Z : Other.Z);
        }
        else
        {
            Min = Max = Other;
            IsValid = 1;
        }
        return *this;
    }
    // 0x00038536. when either box is empty this becomes Other, even an empty Other.
    FBox& operator+=(const FBox& Other)
    {
        if (IsValid && Other.IsValid)
        {
            Min = FVector(Min.X <= Other.Min.X ? Min.X : Other.Min.X, Min.Y <= Other.Min.Y ? Min.Y : Other.Min.Y, Min.Z <= Other.Min.Z ? Min.Z : Other.Min.Z);
            Max = FVector(Max.X < Other.Max.X ? Other.Max.X : Max.X, Max.Y < Other.Max.Y ? Other.Max.Y : Max.Y, Max.Z < Other.Max.Z ? Other.Max.Z : Max.Z);
        }
        else
        {
            *this = Other;
        }
        return *this;
    }
};

// the xbox copies vectors, planes and boxes as raw bytes
inline FArchive& operator<<(FArchive& Ar, FVector& V)
{
    return Ar << V.X << V.Y << V.Z;
}

inline FArchive& operator<<(FArchive& Ar, FRotator& R)
{
    return Ar << R.Pitch << R.Yaw << R.Roll;
}

inline FArchive& operator<<(FArchive& Ar, FPlane& P)
{
    return Ar << static_cast<FVector&>(P) << P.W;
}

// 0x000386e7
inline FArchive& operator<<(FArchive& Ar, FBox& B)
{
    return Ar << B.Min << B.Max << B.IsValid;
}

struct FRange
{
    FLOAT Min;
    FLOAT Max;
};

struct FRangeVector
{
    FRange X;
    FRange Y;
    FRange Z;
};

// a colour in the order the hardware stores it
struct FColor
{
    BYTE B, G, R, A;

    // 0x0003f9e8
    friend FArchive& operator<<(FArchive& Ar, FColor& Color)
    {
        return Ar << Color.R << Color.G << Color.B << Color.A;
    }
};

// screen rectangles
struct FIntBox
{
    INT X1, Y1, X2, Y2;
};

struct FFloatBox
{
    FLOAT X1, Y1, X2, Y2;
};

struct FCompressedPosition
{
    FVector  Location;
    FRotator Rotation;
    FVector  Velocity;
};

struct FMatrix;

// a coordinate system: an origin and three axes
struct FCoords
{
    FVector Origin;
    FVector XAxis;
    FVector YAxis;
    FVector ZAxis;

    FCoords() = default;
    // 0x00061abc
    FCoords(const FVector& InOrigin, const FVector& InX, const FVector& InY, const FVector& InZ)
        : Origin(InOrigin), XAxis(InX), YAxis(InY), ZAxis(InZ) {}

    // 0x00134538. the matrix that moves a point into these coordinates.
    FMatrix Matrix() const;
};

inline FBox FBox::TransformBy(const FCoords& Coords) const
{
    FBox Result{};
    for (INT X = 0; X < 2; X++)
        for (INT Y = 0; Y < 2; Y++)
            for (INT Z = 0; Z < 2; Z++)
            {
                const FVector P = FVector(X ? Max.X : Min.X, Y ? Max.Y : Min.Y, Z ? Max.Z : Min.Z) - Coords.Origin;
                Result += FVector(P | Coords.XAxis, P | Coords.YAxis, P | Coords.ZAxis);
            }
    return Result;
}

// a scale with an optional shear
struct FScale
{
    FVector Scale;
    FLOAT   SheerRate;
    BYTE    SheerAxis;
};

// a 4x4 matrix of row vectors
struct FMatrix
{
    FLOAT M[4][4];

    // at 0x0049f0b0 on the xbox
    static const FMatrix Identity;

    FMatrix() = default;
    FMatrix(const FPlane& InX, const FPlane& InY, const FPlane& InZ, const FPlane& InW);

    // 0x00133d79. the identity when the matrix is singular.
    FMatrix Inverse() const;
    // 0x00135e47
    FRotator Rotator() const;
    // 0x00134396. the cofactors of the 3x3 part, transposed; transforms normals.
    FMatrix TransposeAdjoint() const;
    FLOAT Determinant() const
    {
        return M[0][0] * (M[1][1] * (M[2][2] * M[3][3] - M[2][3] * M[3][2]) - M[2][1] * (M[1][2] * M[3][3] - M[1][3] * M[3][2])
                   + M[3][1] * (M[1][2] * M[2][3] - M[1][3] * M[2][2]))
            - M[1][0] * (M[0][1] * (M[2][2] * M[3][3] - M[2][3] * M[3][2]) - M[2][1] * (M[0][2] * M[3][3] - M[0][3] * M[3][2])
                   + M[3][1] * (M[0][2] * M[2][3] - M[0][3] * M[2][2]))
            + M[2][0] * (M[0][1] * (M[1][2] * M[3][3] - M[1][3] * M[3][2]) - M[1][1] * (M[0][2] * M[3][3] - M[0][3] * M[3][2])
                   + M[3][1] * (M[0][2] * M[1][3] - M[0][3] * M[1][2]))
            - M[3][0] * (M[0][1] * (M[1][2] * M[2][3] - M[1][3] * M[2][2]) - M[1][1] * (M[0][2] * M[2][3] - M[0][3] * M[2][2])
                   + M[2][1] * (M[0][2] * M[1][3] - M[0][3] * M[1][2]));
    }

    // this transform followed by Other
    FMatrix operator*(const FMatrix& Other) const;
    // a point: the vector as a row with w 1
    FVector TransformFVector(const FVector& V) const
    {
        return FVector(V.X * M[0][0] + V.Y * M[1][0] + V.Z * M[2][0] + M[3][0],
                       V.X * M[0][1] + V.Y * M[1][1] + V.Z * M[2][1] + M[3][1],
                       V.X * M[0][2] + V.Y * M[1][2] + V.Z * M[2][2] + M[3][2]);
    }
    // a direction: the vector as a row with w 0
    FVector TransformNormal(const FVector& V) const
    {
        return FVector(V.X * M[0][0] + V.Y * M[1][0] + V.Z * M[2][0],
                       V.X * M[0][1] + V.Y * M[1][1] + V.Z * M[2][1],
                       V.X * M[0][2] + V.Y * M[1][2] + V.Z * M[2][2]);
    }
};

// 0x0002fe4a
FMatrix FTranslationMatrix(const FVector& Delta);
// 0x0002fee4
FMatrix FScaleMatrix(const FVector& Scale);
// 0x0002feaa. the inverse of the rotation by Rot.
FMatrix FInverseRotationMatrix(const FRotator& Rot);

// copied as 64 raw bytes
inline FArchive& operator<<(FArchive& Ar, FMatrix& M)
{
    for (INT i = 0; i < 4; i++)
        for (INT j = 0; j < 4; j++)
            Ar << M.M[i][j];
    return Ar;
}

// 0x0013464b. a rotation by Rot followed by a move to Origin.
FMatrix FRotationTranslationMatrix(const FRotator& Rot, const FVector& Origin);

struct FQuat
{
    FLOAT X, Y, Z, W;

    FQuat() = default;
    constexpr FQuat(FLOAT InX, FLOAT InY, FLOAT InZ, FLOAT InW) : X(InX), Y(InY), Z(InZ), W(InW) {}
    // 0x00134c9d
    explicit FQuat(const FMatrix& M);

    // 0x001348e5
    FVector RotateVector(const FVector& V) const;

    // 0x00042153. a quaternion too short to normalize becomes the identity.
    UBOOL Normalize()
    {
        const FLOAT SquareSum = X * X + Y * Y + Z * Z + W * W;
        if (SquareSum >= 1e-5f)
        {
            const FLOAT Scale = appInvSqrt(SquareSum);
            X *= Scale;
            Y *= Scale;
            Z *= Scale;
            W *= Scale;
            return 1;
        }
        X = Y = Z = 0.f;
        W = 1.f;
        return 0;
    }

    // 0x00135f2d. negates this when the negated rotation is closer to Q.
    void AlignWith(const FQuat& Q)
    {
        const FLOAT Plus = (X + Q.X) * (X + Q.X) + (Y + Q.Y) * (Y + Q.Y) + (Z + Q.Z) * (Z + Q.Z) + (W + Q.W) * (W + Q.W);
        const FLOAT Minus = (X - Q.X) * (X - Q.X) + (Y - Q.Y) * (Y - Q.Y) + (Z - Q.Z) * (Z - Q.Z) + (W - Q.W) * (W - Q.W);
        if (Plus < Minus)
        {
            X = 0.f - X;
            Y = 0.f - Y;
            Z = 0.f - Z;
            W = 0.f - W;
        }
    }

    // 0x00134a1c
    static FQuat Slerp(const FQuat& A, const FQuat& B, FLOAT Alpha);
};

// copied as 16 raw bytes
inline FArchive& operator<<(FArchive& Ar, FQuat& Q)
{
    return Ar << Q.X << Q.Y << Q.Z << Q.W;
}

// 0x001347ac. a rotation by Q followed by a move to Origin.
FMatrix FQuatRotationTranslationMatrix(const FQuat& Q, const FVector& Origin);

// a piecewise linear curve of script values
struct FInterpCurvePoint
{
    FLOAT InVal;
    FLOAT OutVal;
};

struct FInterpCurve
{
    TArray<FInterpCurvePoint> Points;

    // 0x0013612c. 0 outside the curve or with fewer than two points.
    FLOAT Eval(FLOAT Input) const;
};

// the global math tables. at 0x0047f020 on the xbox, built by its constructor (0x001337ef) at startup.
class FGlobalMath
{
public:
    const FVector WorldMin;
    const FVector WorldMax;
    const FCoords UnitCoords;
    const FScale  UnitScale;
    const FCoords ViewCoords;

    FGlobalMath();

    FLOAT SinTab(INT i) const { return TrigFLOAT[(i >> ANGLE_SHIFT) & ANGLE_MASK]; }
    FLOAT CosTab(INT i) const { return TrigFLOAT[((i + 16384) >> ANGLE_SHIFT) & ANGLE_MASK]; }
    FLOAT SqrtTab(INT i) const { return SqrtFLOAT[i]; }

private:
    FLOAT TrigFLOAT[NUM_ANGLES];
    FLOAT SqrtFLOAT[NUM_SQRTS];
};

extern FGlobalMath GMath;
