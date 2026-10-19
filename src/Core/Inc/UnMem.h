#pragma once

#include <cstdlib>

#include "CoreTypes.h"

// the xbox build routes these through GMalloc; plain malloc is enough until the allocator is decompiled
void* appMalloc(size_t Size);
void* appRealloc(void* Ptr, size_t Size);
void  appFree(void* Ptr);

// a stack of temporary allocations freed in bulk. only Init is decompiled so far.
class FMemStack
{
public:
    void Init(INT InDefaultChunkSize);

private:
    BYTE* Top;
    BYTE* End;
    INT   DefaultChunkSize;
    void* TopChunk;
};

extern FMemStack GMem;

// bits needed to hold each value 0 to 256; appInit fills it
extern BYTE GBitLengths[257];

// the smallest power of two exponent with 1 << Result >= Arg; 0 for 0 and 1
BYTE appCeilLogTwo(DWORD Arg);

// seeds rand() and appSRand
void appRandInit(INT Seed);
void appSRandInit(INT Seed);
// the crt rand(), scaled to the xbox's range of 0 to 32767 so script sees the same values
inline INT appRand() { return static_cast<INT>(static_cast<unsigned long long>(rand()) * 32768 / (static_cast<unsigned long long>(RAND_MAX) + 1)); }
// 0 to 1
inline FLOAT appFrand() { return appRand() * (1.f / 32767.f); }
