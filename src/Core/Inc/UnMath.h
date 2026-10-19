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

// a bounding box; IsValid is 0 for an empty box
struct FBox
{
    FVector Min;
    FVector Max;
    BYTE    IsValid;
};

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

// a coordinate system: an origin and three axes
struct FCoords
{
    FVector Origin;
    FVector XAxis;
    FVector YAxis;
    FVector ZAxis;
};

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
};

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
};

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
