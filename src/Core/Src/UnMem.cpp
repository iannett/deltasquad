#include <cstdlib>

#include "Core.h"

FMemStack GMem;
BYTE GBitLengths[257];

// the seed of appSRand
static INT GSRandSeed;

// 0x0015d743
void FMemStack::Init(INT InDefaultChunkSize)
{
    DefaultChunkSize = InDefaultChunkSize;
    TopChunk = nullptr;
    End = nullptr;
    Top = nullptr;
}

// 0x00035121
BYTE appCeilLogTwo(DWORD Arg)
{
    if (--Arg == 0xFFFFFFFF)
        return 0;
    const BYTE Shift = Arg <= 0x10000 ? (Arg > 0x100 ? 8 : 0) : (Arg > 0x1000000 ? 24 : 16);
    return GBitLengths[Arg >> Shift] + Shift;
}

// 0x0003ac5a
void appRandInit(INT Seed)
{
    srand(static_cast<unsigned>(Seed));
}

// 0x0003ac62
void appSRandInit(INT Seed)
{
    GSRandSeed = Seed;
}
