#include <cmath>

#include "Core.h"
#include "Karma.h"

// matrices are 16 floats, row by row, with the translation in the last row

// 0x001a70f0
void MeMatrix4MultiplyMatrix(FLOAT* Out, const FLOAT* A, const FLOAT* B)
{
    for (INT i = 0; i < 4; i++)
    {
        const FLOAT* Row = A + i * 4;
        for (INT j = 0; j < 4; j++)
            Out[i * 4 + j] = Row[0] * B[j] + Row[1] * B[4 + j] + Row[3] * B[12 + j] + Row[2] * B[8 + j];
    }
}

// 0x00196e90
void MeMatrix4TMTransform(FLOAT* Out, const FLOAT* TM, const FLOAT* V)
{
    Out[0] = TM[0] * V[0] + TM[4] * V[1] + TM[8] * V[2] + TM[12];
    Out[1] = TM[5] * V[1] + TM[1] * V[0] + TM[9] * V[2] + TM[13];
    Out[2] = TM[6] * V[1] + TM[2] * V[0] + TM[10] * V[2] + TM[14];
}

// 0x00184e40
void MeMatrix4TMInverseTransform(FLOAT* Out, const FLOAT* TM, const FLOAT* V)
{
    const FLOAT X = V[0] - TM[12];
    const FLOAT Y = V[1] - TM[13];
    const FLOAT Z = V[2] - TM[14];
    Out[0] = X * TM[0] + Y * TM[1] + Z * TM[2];
    Out[1] = X * TM[4] + Y * TM[5] + Z * TM[6];
    Out[2] = X * TM[8] + Y * TM[9] + Z * TM[10];
}

// 0x00196ee0
void MeMatrix4TMRotate(FLOAT* Out, const FLOAT* TM, const FLOAT* V)
{
    Out[0] = TM[0] * V[0] + TM[4] * V[1] + TM[8] * V[2];
    Out[1] = TM[5] * V[1] + TM[1] * V[0] + TM[9] * V[2];
    Out[2] = TM[6] * V[1] + TM[2] * V[0] + TM[10] * V[2];
}

// 0x00165520
FLOAT MeVector3Normalize(FLOAT* V)
{
    const FLOAT LengthSq = V[2] * V[2] + V[1] * V[1] + V[0] * V[0];
    if (LengthSq > 0.f)
    {
        const FLOAT Length = sqrtf(LengthSq);
        const FLOAT Scale = 1.f / Length;
        V[0] *= Scale;
        V[1] *= Scale;
        V[2] *= Scale;
        return Length;
    }
    V[0] = 1.f;
    V[1] = 0.f;
    V[2] = 0.f;
    return 0.f;
}

// 0x00165580. P drops the axis N is longest along least.
void MeVector3PlaneSpace(const FLOAT* N, FLOAT* P, FLOAT* Q)
{
    if (fabsf(N[1]) < fabsf(N[0]))
    {
        const FLOAT Scale = 1.f / sqrtf(N[0] * N[0] + N[2] * N[2]);
        P[1] = 0.f;
        P[0] = Scale * N[2];
        P[2] = -(Scale * N[0]);
        Q[0] = P[2] * N[1];
        Q[1] = N[2] * P[0] - N[0] * P[2];
        Q[2] = -(N[1] * P[0]);
        return;
    }
    const FLOAT Scale = 1.f / sqrtf(N[1] * N[1] + N[2] * N[2]);
    P[0] = 0.f;
    P[1] = -(Scale * N[2]);
    P[2] = Scale * N[1];
    Q[0] = P[2] * N[1] - N[2] * P[1];
    Q[1] = -(N[0] * P[2]);
    Q[2] = N[0] * P[1];
}

// 0x0018c3c0. with a negative trace the largest diagonal entry picks which part is solved first.
void MeQuaternionFromTM(FLOAT* Q, const FLOAT* TM)
{
    const FLOAT Trace = TM[10] + TM[5] + TM[0];
    if (Trace > 0.f)
    {
        FLOAT S = sqrtf(Trace + 1.f);
        Q[0] = 0.5f * S;
        S = 0.5f / S;
        Q[1] = (TM[6] - TM[9]) * S;
        Q[2] = (TM[8] - TM[2]) * S;
        Q[3] = (TM[1] - TM[4]) * S;
        return;
    }
    // 0x003df088
    static const INT Next[3] = { 1, 2, 0 };
    INT i = TM[0] < TM[5];
    if (TM[i * 5] < TM[10])
        i = 2;
    const INT j = Next[i];
    const INT k = Next[j];
    FLOAT S = sqrtf(TM[i * 5] - TM[j * 5] - TM[k * 5] + 1.f);
    FLOAT V[4];
    V[i] = 0.5f * S;
    if (S != 0.f)
        S = 0.5f / S;
    V[3] = (TM[k + j * 4] - TM[j + k * 4]) * S;
    V[j] = (TM[i + j * 4] + TM[j + i * 4]) * S;
    V[k] = (TM[i + k * 4] + TM[k + i * 4]) * S;
    Q[2] = V[1];
    Q[1] = V[0];
    Q[3] = V[2];
    Q[0] = V[3];
}
