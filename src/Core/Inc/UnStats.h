#pragma once

#include "CoreTypes.h"
#include "UnTemplate.h"

// a named cycle counter in the stats tree. the tree hangs off Main, the "Frame" record; each frame a parent
// adds its children's cycles to its own.
struct FStatRecord
{
    // cycles this frame
    DWORD        DWORD0;
    DWORD        Count;
    // DWORD0 and Count as of the last frame shown
    DWORD        Pad1;
    DWORD        Pad2;
    const TCHAR* Tag;
    // set while the record is shown
    DWORD        DWORD20;
    FStatRecord* Parent;
    FStatRecord* Children;
    FStatRecord* NextChild;
    bool         ChildrenShown;

    // 0x0005fca1. a record with no parent stays out of the tree.
    FStatRecord(const TCHAR* InTag, FStatRecord* InParent);
    // 0x0005fdbb. the parent is found by tag anywhere under Main.
    FStatRecord(const TCHAR* InTag, const TCHAR* ParentTag);

    // 0x0005fb36. a child with the same tag as an existing one goes under that one, with an empty tag.
    void AddChild(FStatRecord* Child);
    // 0x0005fb7a. this record, or the child with the tag, searching the whole subtree when Recursive.
    FStatRecord* FindStat(const TCHAR* InTag, bool Recursive);
    // 0x0005fd80. adds the children's cycles into this record, except those of Ignore.
    void Tick();
    // 0x0005fbe1. moves each counter Fraction of the way towards its last shown value.
    void UpdateAverage(FLOAT Fraction);

    // 0x0005fce4
    static FStatRecord& Main();
    // 0x0005fd30. cycles counted here are left out of their parent's sum.
    static FStatRecord& Ignore();
};

// walks a stats subtree depth first
struct FStatIterator
{
    FStatRecord* Record;
    bool         bDone;

    explicit FStatIterator(FStatRecord* Start) : Record(Start), bDone(false) {}
    explicit operator bool() const { return Record != nullptr; }
    FStatRecord* operator->() const { return Record; }
    // 0x0005fc6e
    void operator++();
};

// the per-frame stats: the frame rate history and averaging of the stats tree
class FStats
{
public:
    // 0x0005fe0c
    FStats();
    // 0x0005fe5d
    void Clear();
    // 0x0005fe72. ends a frame of the stats tree.
    void Tick();

    // only ticks while set
    BYTE         Unknown0;
    // when set, the shown values only change when a frame takes longer
    BYTE         Unknown1;
    // keeps a frame rate per frame in UnknownC
    BYTE         Unknown2;
    // seconds to average over; 0 shows each frame as is
    FLOAT        Unknown4;
    // seconds of frames counted
    FLOAT        Unknown8;
    TArray<BYTE> UnknownC;
};

// 0x00461464
extern FStats GStats;
