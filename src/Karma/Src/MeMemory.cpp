#include <cstdlib>

#include "Karma.h"

namespace
{
    // 0x00184470
    void* MallocCreate(INT Size)
    {
        return malloc(Size);
    }

    // 0x00184480
    void* MallocCreateZeroed(INT Size)
    {
        return calloc(1, Size);
    }

    // 0x001844d0. the offset back to the real block is kept just before the returned pointer.
    void* MallocCreateAligned(INT Size, INT Alignment)
    {
        BYTE* Block = static_cast<BYTE*>(malloc(Size + 4 + Alignment));
        if (!Block)
            return nullptr;
        UPTRINT Offset = Alignment - reinterpret_cast<UPTRINT>(Block) % Alignment;
        if (Offset == static_cast<UPTRINT>(Alignment))
            Offset = 0;
        if (Alignment & 3)
            MeFatalError(0, "MallocCreateAligned: Alignment must be a multiple of sizeof(unsigned int).");
        while (Offset < 4)
            Offset += Alignment;
        *reinterpret_cast<DWORD*>(Block + Offset - 4) = static_cast<DWORD>(Offset);
        return Block + Offset;
    }

    // 0x001844a0
    void MallocDestroy(void* Ptr)
    {
        free(Ptr);
    }

    // 0x00184540
    void MallocDestroyAligned(void* Ptr)
    {
        BYTE* Block = static_cast<BYTE*>(Ptr);
        free(Block - reinterpret_cast<DWORD*>(Block)[-1]);
    }

    // 0x001844b0
    void* MallocResize(void* Ptr, INT Size)
    {
        return realloc(Ptr, Size);
    }

    // 0x001980b0. the structs are carved out of one block and handed out from a stack of free pointers.
    void FixedPoolInit(MePool* Pool, INT MaxStructs, INT StructSize, INT Alignment)
    {
        INT Size = 0;
        if (MaxStructs > 0)
        {
            if (Alignment == 0)
            {
                Pool->Fixed.Memory = MeMemory.Create(MaxStructs * StructSize);
                Pool->Fixed.Aligned = 0;
            }
            else
            {
                if (StructSize % Alignment)
                    StructSize = StructSize - StructSize % Alignment + Alignment;
                Pool->Fixed.Memory = MeMemory.CreateAligned(StructSize * MaxStructs, Alignment);
                Pool->Fixed.Aligned = 1;
            }
            Pool->Fixed.FreeList = static_cast<void**>(MeMemory.Create(MaxStructs * static_cast<INT>(sizeof(void*))));
            Size = StructSize;
        }
        Pool->Fixed.StructSize = Size;
        Pool->Fixed.MaxStructs = MaxStructs;
        Pool->PoolType = 1;
        BYTE* Struct = static_cast<BYTE*>(Pool->Fixed.Memory);
        for (INT i = 0; i < Pool->Fixed.MaxStructs; i++, Struct += Pool->Fixed.StructSize)
            Pool->Fixed.FreeList[i] = Struct;
        Pool->Fixed.Used = 0;
    }

    // 0x00198150
    void FixedPoolDestroy(MePool* Pool)
    {
        if (Pool->Fixed.MaxStructs <= 0)
            return;
        if (Pool->Fixed.Aligned)
            MeMemory.DestroyAligned(Pool->Fixed.Memory);
        else
            MeMemory.Destroy(Pool->Fixed.Memory);
        MeMemory.Destroy(Pool->Fixed.FreeList);
    }

    // 0x00198080
    void FixedPoolReset(MePool* Pool)
    {
        BYTE* Struct = static_cast<BYTE*>(Pool->Fixed.Memory);
        for (INT i = 0; i < Pool->Fixed.MaxStructs; i++, Struct += Pool->Fixed.StructSize)
            Pool->Fixed.FreeList[i] = Struct;
    }

    // 0x00198190
    void* FixedPoolGetStruct(MePool* Pool)
    {
        if (Pool->Fixed.Used >= Pool->Fixed.MaxStructs)
            return nullptr;
        return Pool->Fixed.FreeList[Pool->Fixed.Used++];
    }

    // 0x001981b0
    void FixedPoolPutStruct(MePool* Pool, void* Struct)
    {
        if (Pool->Fixed.Used > 0)
            Pool->Fixed.FreeList[--Pool->Fixed.Used] = Struct;
    }

    // 0x001b3730
    INT FixedPoolGetUsed(MePool* Pool)
    {
        return Pool->Fixed.Used;
    }

    // 0x001981d0
    INT FixedPoolGetUnused(MePool* Pool)
    {
        return Pool->Fixed.MaxStructs - Pool->Fixed.Used;
    }
}

MeMemoryAPI MeMemory = { MallocCreate, MallocCreateZeroed, MallocCreateAligned, MallocDestroy, MallocDestroyAligned, MallocResize };
MePoolAPI MePools = { FixedPoolInit, FixedPoolDestroy, FixedPoolReset, FixedPoolGetStruct, FixedPoolPutStruct, FixedPoolGetUsed, FixedPoolGetUnused };

// 0x0018aa00
MeUnknown1C* MeUnknown1CCreate(INT Max)
{
    MeUnknown1C* Array = static_cast<MeUnknown1C*>(MeMemory.Create(sizeof(MeUnknown1C)));
    Array->Data = static_cast<void**>(MeMemory.Create(Max * static_cast<INT>(sizeof(void*))));
    Array->Max = Max;
    Array->Unknown10 = Max;
    Array->Unknown14 = Max;
    Array->Unknown18 = Max;
    Array->Unknown8 = 0;
    Array->UnknownC = 0;
    return Array;
}

// 0x0018a910
void MeUnknown1CDestroy(MeUnknown1C* Array)
{
    MeMemory.Destroy(Array->Data);
    MeMemory.Destroy(Array);
}
