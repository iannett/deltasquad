#pragma once

#include <cstdlib>

#include "CoreTypes.h"

// the xbox build routes these through GMalloc; plain malloc is enough until the allocator is decompiled
void* appMalloc(size_t Size);
void* appRealloc(void* Ptr, size_t Size);
void  appFree(void* Ptr);
size_t appMallocSize(void* Ptr);

// a stack of temporary allocations freed in bulk
class FMemStack
{
public:
    void Init(INT InDefaultChunkSize);
    // 0x0015d76d. frees the chunks kept for reuse; the stack's own chunks stay.
    void Exit();

    // 0x0004198e
    BYTE* PushBytes(INT AllocSize, INT Align)
    {
        BYTE* Result = AlignUp(Top, Align);
        Top = Result + AllocSize;
        if (Top > End)
        {
            AllocateNewChunk(AllocSize + Align);
            Result = AlignUp(Top, Align);
            Top = Result + AllocSize;
        }
        return Result;
    }

private:
    friend class FMemMark;

    // a chunk of the stack, with its data right after the header
    struct FTaggedMemory
    {
        FTaggedMemory* Next;
        INT            DataSize;
        BYTE           Data[1];
    };

    static BYTE* AlignUp(BYTE* Ptr, INT Align)
    {
        return reinterpret_cast<BYTE*>((reinterpret_cast<PTRINT>(Ptr) + Align - 1) & ~static_cast<PTRINT>(Align - 1));
    }

    // 0x0015d7ba. reuses a freed chunk that is big enough, else allocates one.
    BYTE* AllocateNewChunk(INT MinSize);
    // 0x0015d777. gives the chunks above NewTopChunk back for reuse.
    void FreeChunks(FTaggedMemory* NewTopChunk);

    BYTE*          Top;
    BYTE*          End;
    INT            DefaultChunkSize;
    FTaggedMemory* TopChunk;

    // 0x0049f92c. chunks freed by every stack, kept for reuse.
    static FTaggedMemory* UnusedChunks;
};

extern FMemStack GMem;

// a point on a memory stack to free back to
class FMemMark
{
public:
    explicit FMemMark(FMemStack& InMem) : Mem(&InMem), Top(InMem.Top), SavedChunk(InMem.TopChunk) {}

    // 0x000419d6
    void Pop()
    {
        if (SavedChunk != Mem->TopChunk)
            Mem->FreeChunks(SavedChunk);
        Mem->Top = Top;
    }

private:
    FMemStack*                Mem;
    BYTE*                     Top;
    FMemStack::FTaggedMemory* SavedChunk;
};

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
