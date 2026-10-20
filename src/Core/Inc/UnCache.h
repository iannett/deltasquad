#pragma once

#include "CoreTypes.h"

// blocks of memory found by a 64-bit id, in one or more segments. only Init is decompiled so far.
class FMemCache
{
public:
    class FCacheItem
    {
    public:
        QWORD       Id;
        BYTE*       Data;
        _WORD       Time;
        BYTE        Segment;
        BYTE        Extra;
        INT         Cost;
        FCacheItem* LinearNext;
        FCacheItem* LinearPrev;
        FCacheItem* HashNext;
    };

    enum { HASH_COUNT = 16384 };

    void Init(INT BytesToAllocate, INT InMaxItems, void* Start = nullptr, INT SegSize = 0);
    void CheckState();

private:
    void CreateNewFreeSpace(BYTE* Start, BYTE* End, FCacheItem* Prev, FCacheItem* Next, INT Segment);

public:
    UBOOL       Initialized;
    INT         Unknown4;
    INT         Unknown8;
    INT         UnknownC;
    INT         Unknown10;
    FCacheItem* ItemMemory;
    FCacheItem* FirstItem;
    // the free item past the end of the memory
    FCacheItem* LastItem;
    FCacheItem* UnusedItems;
    FCacheItem* HashItems[HASH_COUNT];
    BYTE*       CacheMemory;
    INT         Unknown10028[52];
    INT         MaxItems;
    INT         Unknown100FC[3];
    INT         MemSize;
};
