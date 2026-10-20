#include "Core.h"
#include "UnMath.h"

FGlobalMath GMath;

const FMatrix FMatrix::Identity(FPlane(1.f, 0.f, 0.f, 0.f), FPlane(0.f, 1.f, 0.f, 0.f), FPlane(0.f, 0.f, 1.f, 0.f), FPlane(0.f, 0.f, 0.f, 1.f));

// 0x001337ef
FGlobalMath::FGlobalMath()
    : WorldMin(-32700.f, -32700.f, -32700.f)
    , WorldMax(32700.f, 32700.f, 32700.f)
    , UnitCoords{ FVector(0.f, 0.f, 0.f), FVector(1.f, 0.f, 0.f), FVector(0.f, 1.f, 0.f), FVector(0.f, 0.f, 1.f) }
    , UnitScale{ FVector(1.f, 1.f, 1.f), 0.f, 5 }
    , ViewCoords{ FVector(0.f, 0.f, 0.f), FVector(0.f, 1.f, 0.f), FVector(0.f, 0.f, -1.f), FVector(1.f, 0.f, 0.f) }
{
    // the step is rounded to a float before the multiply, as on the xbox
    const FLOAT AngleStep = 2.f * 3.1415926535897932f / NUM_ANGLES;
    for (INT i = 0; i < NUM_ANGLES; i++)
        TrigFLOAT[i] = static_cast<FLOAT>(sin(static_cast<double>(i) * AngleStep));
    for (INT i = 0; i < NUM_SQRTS; i++)
        SqrtFLOAT[i] = appSqrt(i * (1.f / 16384.f));
}

// 0x00133a41. truncates; roll is always 0.
FRotator FVector::Rotation() const
{
    const double Scale = 65535.0 / (2.0 * 3.141592653589793);
    FRotator R;
    R.Yaw = static_cast<INT>(atan2(static_cast<double>(Y), static_cast<double>(X)) * Scale);
    R.Pitch = static_cast<INT>(atan2(static_cast<double>(Z), static_cast<double>(Size2D())) * Scale);
    R.Roll = 0;
    return R;
}

// 0x001339d0
FVector FRotator::Vector() const
{
    const FLOAT CP = GMath.CosTab(Pitch);
    return FVector(GMath.CosTab(Yaw) * CP, GMath.SinTab(Yaw) * CP, GMath.SinTab(Pitch));
}

FMatrix::FMatrix(const FPlane& InX, const FPlane& InY, const FPlane& InZ, const FPlane& InW)
{
    const FPlane* Rows[4] = { &InX, &InY, &InZ, &InW };
    for (INT i = 0; i < 4; i++)
    {
        M[i][0] = Rows[i]->X;
        M[i][1] = Rows[i]->Y;
        M[i][2] = Rows[i]->Z;
        M[i][3] = Rows[i]->W;
    }
}

FMatrix FCoords::Matrix() const
{
    const FVector Neg(0.f - Origin.X, 0.f - Origin.Y, 0.f - Origin.Z);
    FMatrix R;
    R.M[0][0] = XAxis.X; R.M[0][1] = YAxis.X; R.M[0][2] = ZAxis.X; R.M[0][3] = 0.f;
    R.M[1][0] = XAxis.Y; R.M[1][1] = YAxis.Y; R.M[1][2] = ZAxis.Y; R.M[1][3] = 0.f;
    R.M[2][0] = XAxis.Z; R.M[2][1] = YAxis.Z; R.M[2][2] = ZAxis.Z; R.M[2][3] = 0.f;
    R.M[3][0] = XAxis.Z * Neg.Z + XAxis.Y * Neg.Y + XAxis.X * Neg.X;
    R.M[3][1] = YAxis.Z * Neg.Z + YAxis.Y * Neg.Y + YAxis.X * Neg.X;
    R.M[3][2] = ZAxis.Z * Neg.Z + ZAxis.Y * Neg.Y + Neg.X * ZAxis.X;
    R.M[3][3] = 1.f;
    return R;
}

// 0x00133d79. the inverse by cofactors.
FMatrix FMatrix::Inverse() const
{
    FLOAT A0 = M[2][2] * M[3][3] - M[3][2] * M[2][3];
    FLOAT A1 = M[3][3] * M[1][2] - M[3][2] * M[1][3];
    FLOAT A2 = M[2][3] * M[1][2] - M[2][2] * M[1][3];
    FLOAT A3 = M[3][3] * M[0][2] - M[3][2] * M[0][3];
    FLOAT A4 = M[2][3] * M[0][2] - M[2][2] * M[0][3];
    FLOAT A5 = M[1][3] * M[0][2] - M[1][2] * M[0][3];

    const FLOAT Det = ((M[1][1] * A0 - M[2][1] * A1 + M[3][1] * A2) * M[0][0]
        - (M[0][1] * A0 - M[2][1] * A3 + M[3][1] * A4) * M[1][0]
        + (M[0][1] * A1 - M[1][1] * A3 + M[3][1] * A5) * M[2][0])
        - (M[0][1] * A2 - M[1][1] * A4 + M[2][1] * A5) * M[3][0];
    if (Det == 0.f)
        return Identity;

    const FLOAT RDet = 1.f / Det;
    FMatrix Result;
    Result.M[0][0] = (A0 * M[1][1] - M[2][1] * A1 + M[3][1] * A2) * RDet;
    Result.M[0][1] = 0.f - (A0 * M[0][1] - M[2][1] * A3 + M[3][1] * A4) * RDet;
    Result.M[0][2] = (A1 * M[0][1] - A3 * M[1][1] + M[3][1] * A5) * RDet;
    Result.M[0][3] = 0.f - (A2 * M[0][1] - A4 * M[1][1] + M[2][1] * A5) * RDet;
    Result.M[1][0] = 0.f - (M[1][0] * A0 - M[2][0] * A1 + M[3][0] * A2) * RDet;
    Result.M[1][1] = (M[0][0] * A0 - M[2][0] * A3 + M[3][0] * A4) * RDet;
    Result.M[1][2] = 0.f - (M[0][0] * A1 - M[1][0] * A3 + M[3][0] * A5) * RDet;
    Result.M[1][3] = (M[0][0] * A2 - M[1][0] * A4 + M[2][0] * A5) * RDet;

    A0 = M[2][1] * M[3][3] - M[3][1] * M[2][3];
    A1 = M[3][3] * M[1][1] - M[3][1] * M[1][3];
    A2 = M[2][3] * M[1][1] - M[2][1] * M[1][3];
    A3 = M[3][3] * M[0][1] - M[3][1] * M[0][3];
    A4 = M[2][3] * M[0][1] - M[2][1] * M[0][3];
    A5 = M[1][3] * M[0][1] - M[1][1] * M[0][3];
    Result.M[2][0] = (M[1][0] * A0 - M[2][0] * A1 + M[3][0] * A2) * RDet;
    Result.M[2][1] = 0.f - (M[0][0] * A0 - M[2][0] * A3 + M[3][0] * A4) * RDet;
    Result.M[2][2] = (M[0][0] * A1 - M[1][0] * A3 + M[3][0] * A5) * RDet;
    Result.M[2][3] = 0.f - (M[0][0] * A2 - M[1][0] * A4 + M[2][0] * A5) * RDet;

    A0 = M[3][2] * M[2][1] - M[3][1] * M[2][2];
    A1 = M[3][2] * M[1][1] - M[3][1] * M[1][2];
    A2 = M[2][2] * M[1][1] - M[2][1] * M[1][2];
    A3 = M[3][2] * M[0][1] - M[3][1] * M[0][2];
    A4 = M[2][2] * M[0][1] - M[2][1] * M[0][2];
    A5 = M[1][2] * M[0][1] - M[1][1] * M[0][2];
    Result.M[3][0] = 0.f - (M[1][0] * A0 - M[2][0] * A1 + M[3][0] * A2) * RDet;
    Result.M[3][1] = (A0 * M[0][0] - M[2][0] * A3 + M[3][0] * A4) * RDet;
    Result.M[3][2] = 0.f - (A1 * M[0][0] - M[1][0] * A3 + M[3][0] * A5) * RDet;
    Result.M[3][3] = (A2 * M[0][0] - M[1][0] * A4 + M[2][0] * A5) * RDet;
    return Result;
}

// 0x00135e47. each angle is rounded to the nearest unit and wrapped to 16 bits.
FRotator FMatrix::Rotator() const
{
    const FLOAT Scale = 32768.f / 3.1415926535897932f;
    FRotator R;
    R.Pitch = appRound(static_cast<FLOAT>(atan2(M[0][2], appSqrt(M[0][0] * M[0][0] + M[0][1] * M[0][1])) * Scale)) & 0xffff;
    R.Yaw = appRound(static_cast<FLOAT>(atan2(M[0][1], M[0][0]) * Scale)) & 0xffff;
    R.Roll = appRound(static_cast<FLOAT>(atan2(-M[1][2], M[2][2]) * Scale)) & 0xffff;
    return R;
}

// 0x0013464b
FMatrix FRotationTranslationMatrix(const FRotator& Rot, const FVector& Origin)
{
    const FLOAT SR = GMath.SinTab(Rot.Roll);
    const FLOAT SP = GMath.SinTab(Rot.Pitch);
    const FLOAT SY = GMath.SinTab(Rot.Yaw);
    const FLOAT CR = GMath.CosTab(Rot.Roll);
    const FLOAT CP = GMath.CosTab(Rot.Pitch);
    const FLOAT CY = GMath.CosTab(Rot.Yaw);

    FMatrix Result;
    Result.M[0][0] = CY * CP;
    Result.M[0][1] = CP * SY;
    Result.M[0][2] = SP;
    Result.M[0][3] = 0.f;
    Result.M[1][0] = CY * SP * SR - CR * SY;
    Result.M[1][1] = SY * SP * SR + CY * CR;
    Result.M[1][2] = 0.f - CP * SR;
    Result.M[1][3] = 0.f;
    Result.M[2][0] = 0.f - (CY * CR * SP + SY * SR);
    Result.M[2][1] = CY * SR - CR * SY * SP;
    Result.M[2][2] = CP * CR;
    Result.M[2][3] = 0.f;
    Result.M[3][0] = Origin.X;
    Result.M[3][1] = Origin.Y;
    Result.M[3][2] = Origin.Z;
    Result.M[3][3] = 1.f;
    return Result;
}

FMatrix FMatrix::operator*(const FMatrix& Other) const
{
    FMatrix Result;
    for (INT i = 0; i < 4; i++)
        for (INT j = 0; j < 4; j++)
            Result.M[i][j] = M[i][0] * Other.M[0][j] + M[i][1] * Other.M[1][j] + M[i][2] * Other.M[2][j] + M[i][3] * Other.M[3][j];
    return Result;
}

FMatrix FTranslationMatrix(const FVector& Delta)
{
    return FMatrix(FPlane(1.f, 0.f, 0.f, 0.f), FPlane(0.f, 1.f, 0.f, 0.f), FPlane(0.f, 0.f, 1.f, 0.f), FPlane(Delta.X, Delta.Y, Delta.Z, 1.f));
}

FMatrix FScaleMatrix(const FVector& Scale)
{
    return FMatrix(FPlane(Scale.X, 0.f, 0.f, 0.f), FPlane(0.f, Scale.Y, 0.f, 0.f), FPlane(0.f, 0.f, Scale.Z, 0.f), FPlane(0.f, 0.f, 0.f, 1.f));
}

FMatrix FInverseRotationMatrix(const FRotator& Rot)
{
    return FRotationTranslationMatrix(Rot, FVector(0.f, 0.f, 0.f)).Inverse();
}

// 0x00134c9d. outside the positive trace case the w terms are sums, and the largest axis gets 0.25 / root
// instead of 0.5 * root; both differ from the usual conversion and are kept.
FQuat::FQuat(const FMatrix& M)
{
    const FLOAT Trace = M.M[1][1] + M.M[0][0] + M.M[2][2] + 1.f;
    if (Trace > 0.f)
    {
        const FLOAT S = 0.5f / appSqrt(Trace);
        W = 0.25f / S;
        X = (M.M[1][2] - M.M[2][1]) * S;
        Y = (M.M[2][0] - M.M[0][2]) * S;
        Z = (M.M[0][1] - M.M[1][0]) * S;
    }
    else if (M.M[0][0] > M.M[1][1] && M.M[0][0] > M.M[2][2])
    {
        const FLOAT S = 0.5f / appSqrt(((M.M[0][0] + 1.f) - M.M[1][1]) - M.M[2][2]);
        X = S * 0.5f;
        Y = (M.M[1][0] + M.M[0][1]) * S;
        Z = (M.M[2][0] + M.M[0][2]) * S;
        W = (M.M[2][1] + M.M[1][2]) * S;
    }
    else if (M.M[1][1] > M.M[2][2])
    {
        const FLOAT S = 0.5f / appSqrt(((M.M[1][1] + 1.f) - M.M[0][0]) - M.M[2][2]);
        X = (M.M[1][0] + M.M[0][1]) * S;
        Y = S * 0.5f;
        Z = (M.M[2][1] + M.M[1][2]) * S;
        W = (M.M[2][0] + M.M[0][2]) * S;
    }
    else
    {
        const FLOAT S = 0.5f / appSqrt(((M.M[2][2] + 1.f) - M.M[0][0]) - M.M[1][1]);
        X = (M.M[2][0] + M.M[0][2]) * S;
        Y = (M.M[2][1] + M.M[1][2]) * S;
        Z = S * 0.5f;
        W = (M.M[1][0] + M.M[0][1]) * S;
    }
}

// 0x001348e5
FVector FQuat::RotateVector(const FVector& V) const
{
    const FLOAT W2 = W * 2.f;
    const FLOAT T = W * W - (Y * Y + Z * Z + X * X);
    const FLOAT D2 = (V.X * X + V.Z * Z + Y * V.Y) * 2.f;
    return FVector(
        X * D2 + V.X * T + (V.Z * Y - Z * V.Y) * W2,
        D2 * Y + T * V.Y + (V.X * Z - V.Z * X) * W2,
        D2 * Z + V.Z * T + (X * V.Y - V.X * Y) * W2);
}

// 0x00134a1c. close rotations are blended linearly and normalized; the cutoff is a cosine of 0.55.
FQuat FQuat::Slerp(const FQuat& A, const FQuat& B, FLOAT Alpha)
{
    const FLOAT Cosine = A.X * B.X + A.Z * B.Z + A.Y * B.Y + B.W * A.W;
    const FLOAT AbsCosine = Cosine < 0.f ? 0.f - Cosine : Cosine;
    FLOAT ScaleA, ScaleB;
    if (AbsCosine < 0.55f)
    {
        const DOUBLE Omega = acos(AbsCosine);
        const DOUBLE InvSin = 1.0 / sin(Omega);
        ScaleA = static_cast<FLOAT>(sin((1.0 - Alpha) * Omega) * InvSin);
        ScaleB = static_cast<FLOAT>(sin(Omega * Alpha) * InvSin);
        if (Cosine < 0.f)
            ScaleB = 0.f - ScaleB;
        return FQuat(A.X * ScaleA + B.X * ScaleB, A.Y * ScaleA + B.Y * ScaleB, A.Z * ScaleA + B.Z * ScaleB, A.W * ScaleA + B.W * ScaleB);
    }
    ScaleA = 1.f - Alpha;
    ScaleB = Cosine < 0.f ? 0.f - Alpha : Alpha;
    FQuat Result(A.X * ScaleA + B.X * ScaleB, A.Y * ScaleA + B.Y * ScaleB, A.Z * ScaleA + B.Z * ScaleB, A.W * ScaleA + B.W * ScaleB);
    const FLOAT Scale = appInvSqrt(Result.W * Result.W + Result.Z * Result.Z + Result.Y * Result.Y + Result.X * Result.X);
    Result.X *= Scale;
    Result.Y *= Scale;
    Result.Z *= Scale;
    Result.W *= Scale;
    return Result;
}

// 0x001347ac
FMatrix FQuatRotationTranslationMatrix(const FQuat& Q, const FVector& Origin)
{
    const FLOAT Z2 = Q.Z * 2.f;
    const FLOAT Y2 = Q.Y * 2.f;
    const FLOAT XX2 = Q.X * Q.X * 2.f;
    const FLOAT WX2 = Q.W * Q.X * 2.f;

    FMatrix Result;
    Result.M[0][0] = 1.f - (Z2 * Q.Z + Y2 * Q.Y);
    Result.M[0][1] = Q.W * Z2 + Q.X * Y2;
    Result.M[0][2] = Q.X * Z2 - Q.W * Y2;
    Result.M[0][3] = 0.f;
    Result.M[1][0] = Q.X * Y2 - Q.W * Z2;
    Result.M[1][1] = 1.f - (Z2 * Q.Z + XX2);
    Result.M[1][2] = WX2 + Z2 * Q.Y;
    Result.M[1][3] = 0.f;
    Result.M[2][0] = Q.W * Y2 + Q.X * Z2;
    Result.M[2][1] = Z2 * Q.Y - WX2;
    Result.M[2][2] = 1.f - (Y2 * Q.Y + XX2);
    Result.M[2][3] = 0.f;
    Result.M[3][0] = Origin.X;
    Result.M[3][1] = Origin.Y;
    Result.M[3][2] = Origin.Z;
    Result.M[3][3] = 1.f;
    return Result;
}

// 0x0013612c
FLOAT FInterpCurve::Eval(FLOAT Input) const
{
    const INT Num = Points.Num();
    if (Num > 1 && Points[0].InVal <= Input && Input <= Points[Num - 1].InVal)
    {
        for (INT i = 1; i < Num; i++)
        {
            if (Input <= Points[i].InVal)
            {
                const FInterpCurvePoint& Prev = Points[i - 1];
                return (Points[i].OutVal - Prev.OutVal) * ((Input - Prev.InVal) / (Points[i].InVal - Prev.InVal)) + Prev.OutVal;
            }
        }
    }
    return 0.f;
}
