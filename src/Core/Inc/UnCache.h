#pragma once

#include "CoreTypes.h"

// blocks of memory found by a 64-bit id, in one or more segments
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
    // 0x00183650. clears the per-frame counters, ages the cost of items unused since the last tick and
    // advances the clock.
    void Tick();

private:
    void CreateNewFreeSpace(BYTE* Start, BYTE* End, FCacheItem* Prev, FCacheItem* Next, INT Segment);

public:
    UBOOL       Initialized;
    // advanced by each tick; an item's Time is the tick it was last used
    INT         Time;
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
