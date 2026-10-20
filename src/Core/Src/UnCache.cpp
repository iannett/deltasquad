#include "Core.h"

// 0x0018375c. SegSize splits the memory into segments that free space never crosses; 0 keeps it whole.
void FMemCache::Init(INT BytesToAllocate, INT InMaxItems, void* Start, INT SegSize)
{
    MaxItems = InMaxItems;
    MemSize = BytesToAllocate;
    Unknown8 = 0;
    UnknownC = 0;
    Unknown10 = 0;
    if (!Start)
        Start = appMalloc(BytesToAllocate);
    CacheMemory = static_cast<BYTE*>(Start);

    // every item starts on the unused list
    ItemMemory = static_cast<FCacheItem*>(appMalloc(MaxItems * sizeof(FCacheItem)));
    FCacheItem** Link = &UnusedItems;
    for (INT i = 0; i < MaxItems; i++)
    {
        *Link = &ItemMemory[i];
        Link = &ItemMemory[i].LinearNext;
    }
    *Link = nullptr;

    FCacheItem* Prev = nullptr;
    INT Segment;
    if (SegSize == 0)
    {
        Prev = UnusedItems;
        CreateNewFreeSpace(CacheMemory, CacheMemory + BytesToAllocate, nullptr, nullptr, 0);
        Segment = 1;
    }
    else
    {
        Segment = 0;
        FCacheItem* SegmentPrev = nullptr;
        for (INT Offset = 0, Remaining = BytesToAllocate; Offset < BytesToAllocate; Offset += SegSize, Remaining -= SegSize, Segment++)
        {
            Prev = UnusedItems;
            CreateNewFreeSpace(CacheMemory + Offset, CacheMemory + Offset + (Remaining < SegSize ? Remaining : SegSize), SegmentPrev, nullptr, Segment);
            SegmentPrev = Prev;
        }
    }
    LastItem = UnusedItems;
    CreateNewFreeSpace(CacheMemory + BytesToAllocate, CacheMemory + BytesToAllocate, Prev, nullptr, Segment);

    for (INT i = 0; i < HASH_COUNT; i++)
        HashItems[i] = nullptr;
    Initialized = 1;
    CheckState();
}

// 0x001834d1. space next to a free item of the same segment joins it instead of taking a new item.
void FMemCache::CreateNewFreeSpace(BYTE* Start, BYTE* End, FCacheItem* Prev, FCacheItem* Next, INT Segment)
{
    if (Prev && Prev->Id == 0 && Prev->Segment == Segment)
        return;
    if (Next && Next->Id == 0 && Next->Segment == Segment)
    {
        Next->Data = Start;
        return;
    }
    FCacheItem* Item = UnusedItems;
    UnusedItems = Item->LinearNext;
    Item->Data = Start;
    Item->Segment = static_cast<BYTE>(Segment);
    Item->Time = 0;
    Item->Id = 0;
    Item->Cost = 0;
    Item->LinearNext = Next;
    Item->LinearPrev = Prev;
    Item->HashNext = nullptr;
    if (Prev)
        Prev->LinearNext = Item;
    else
        FirstItem = Item;
    if (Next)
        Next->LinearPrev = Item;
}

// 0x00183463. the xbox build compiles the checks out and only the walks over the lists remain.
void FMemCache::CheckState()
{
}

void FMemCache::Tick()
{
    Unknown8 = 0;
    UnknownC = 0;
    Unknown10 = 0;
    Unknown100FC[2] = 0;
    Unknown100FC[1] = 0;
    Unknown100FC[0] = 0;
    Unknown10028[51] = 0;
    Unknown10028[50] = 0;
    for (FCacheItem* Item = FirstItem; Item != LastItem; Item = Item->LinearNext)
    {
        if (Item->Id && Item->Time < Time)
            Item->Cost -= Item->Cost >> 5;
    }
    Time++;
}
