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

// 0x00185670. the node with the smallest key, or null.
MeDictNode* MeDictFirst(MeDict* Dict)
{
    MeDictNode* Nil = &Dict->Nil;
    MeDictNode* Node = Nil->Left;
    if (Node != Nil)
    {
        for (MeDictNode* Left = Node->Left; Left != Nil; Left = Left->Left)
            Node = Left;
    }
    return Node == Nil ? nullptr : Node;
}

// 0x001856a0. the node after Node, or null.
MeDictNode* MeDictNext(MeDict* Dict, MeDictNode* Node)
{
    MeDictNode* Nil = &Dict->Nil;
    if (Node->Right != Nil)
    {
        Node = Node->Right;
        for (MeDictNode* Left = Node->Left; Left != Nil; Left = Left->Left)
            Node = Left;
        return Node;
    }
    MeDictNode* Parent = Node->Parent;
    while (Parent != Nil && Node == Parent->Right)
    {
        Node = Parent;
        Parent = Node->Parent;
    }
    return Parent == Nil ? nullptr : Parent;
}

// 0x0018f4a0
void MeChunkInit(MeChunk* Chunk, INT Alignment)
{
    if (static_cast<DWORD>(Alignment) < 4)
        Alignment = 4;
    Chunk->Alignment = Alignment;
    Chunk->InUse = 0;
    Chunk->MaxSize = 0;
    Chunk->Data = nullptr;
    Chunk->Size = 0;
    Chunk->Unknown10 = 1;
}

// 0x0018f500. the chunk's memory, reallocated when it is smaller than Size.
void* MeChunkGetMem(MeChunk* Chunk, INT Size)
{
    if (Chunk->Size < Size)
    {
        if (Chunk->Data)
        {
            MeMemory.DestroyAligned(Chunk->Data);
            Chunk->Data = nullptr;
            Chunk->Size = 0;
        }
        Chunk->Data = MeMemory.CreateAligned(Size, Chunk->Alignment);
        Chunk->Size = Size;
        Chunk->InUse = 1;
        if (Chunk->MaxSize < Size)
            Chunk->MaxSize = Size;
        return Chunk->Data;
    }
    Chunk->InUse = 1;
    return Chunk->Data;
}

// 0x0018f560
void MeChunkPutMem(MeChunk* Chunk, void* Mem)
{
    if (!Chunk->InUse)
        return;
    if (Chunk->Unknown10 == 2)
    {
        MeMemory.DestroyAligned(Mem);
        Chunk->Data = nullptr;
        Chunk->Size = 0;
    }
    Chunk->InUse = 0;
}
