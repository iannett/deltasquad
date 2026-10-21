#include "Core.h"
#include "Karma.h"

namespace
{
    // 0x001856f0
    MeDictNode* MeDictAllocNode(void* Context)
    {
        return static_cast<MeDictNode*>(MeMemory.Create(sizeof(MeDictNode)));
    }

    // 0x00185700
    void MeDictFreeNode(MeDictNode* Node, void* Context)
    {
        MeMemory.Destroy(Node);
    }
}

// 0x00185770
void MeDictInit(MeDict* Dict, DWORD MaxCount, INT (*Compare)(const void* A, const void* B))
{
    Dict->Compare = Compare;
    Dict->AllocNode = MeDictAllocNode;
    Dict->FreeNode = MeDictFreeNode;
    Dict->Context = nullptr;
    Dict->NodeCount = 0;
    Dict->MaxCount = MaxCount;
    Dict->Nil.Left = &Dict->Nil;
    Dict->Nil.Right = &Dict->Nil;
    Dict->Nil.Parent = &Dict->Nil;
    // black
    Dict->Nil.Color = 1;
    Dict->AllowDupes = 0;
}

// 0x001856e0
void MeDictAllowDupes(MeDict* Dict)
{
    Dict->AllowDupes = 1;
}

// 0x00184e90
INT MeDictCompareKeys(const void* A, const void* B)
{
    const PTRINT KeyA = reinterpret_cast<PTRINT>(A);
    const PTRINT KeyB = reinterpret_cast<PTRINT>(B);
    if (KeyA < KeyB)
        return -1;
    return KeyA > KeyB;
}

// 0x0018f4a0
void MeChunkInit(MeChunk* Chunk, INT Alignment)
{
    if (static_cast<DWORD>(Alignment) < 4)
        Alignment = 4;
    Chunk->Alignment = Alignment;
    Chunk->Unknown14 = 0;
    Chunk->Unknown8 = 0;
    Chunk->Unknown0 = nullptr;
    Chunk->Unknown4 = 0;
    Chunk->Unknown10 = 1;
}
