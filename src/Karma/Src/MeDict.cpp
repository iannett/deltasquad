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

    // the colors of the red-black tree
    constexpr INT Red = 0;
    constexpr INT Black = 1;

    // 0x001853f0. Upper's right child takes its place.
    void RotateLeft(MeDictNode* Upper)
    {
        MeDictNode* Lower = Upper->Right;
        Upper->Right = Lower->Left;
        Lower->Left->Parent = Upper;
        MeDictNode* Parent = Upper->Parent;
        Lower->Parent = Parent;
        if (Upper == Parent->Left)
            Parent->Left = Lower;
        else
            Parent->Right = Lower;
        Lower->Left = Upper;
        Upper->Parent = Lower;
    }

    // 0x00185420. Upper's left child takes its place.
    void RotateRight(MeDictNode* Upper)
    {
        MeDictNode* Lower = Upper->Left;
        Upper->Left = Lower->Right;
        Lower->Right->Parent = Upper;
        MeDictNode* Parent = Upper->Parent;
        Lower->Parent = Parent;
        if (Upper == Parent->Right)
            Parent->Right = Lower;
        else
            Parent->Left = Lower;
        Lower->Right = Upper;
        Upper->Parent = Lower;
    }
}

// 0x00185710
void MeDictNodeInit(MeDictNode* Node, void* Data)
{
    Node->Data = Data;
    Node->Parent = nullptr;
    Node->Left = nullptr;
    Node->Right = nullptr;
}

// 0x001854d0. equal keys go to the right.
void MeDictInsert(MeDict* Dict, MeDictNode* Node, const void* Key)
{
    MeDictNode* Nil = &Dict->Nil;
    MeDictNode* Parent = Nil;
    INT Order = -1;
    Node->Key = Key;
    for (MeDictNode* Where = Nil->Left; Where != Nil; Where = Order < 0 ? Where->Left : Where->Right)
    {
        Parent = Where;
        Order = Dict->Compare(Key, Where->Key);
    }
    if (Order < 0)
        Parent->Left = Node;
    else
        Parent->Right = Node;
    Node->Parent = Parent;
    Node->Left = Nil;
    Node->Right = Nil;
    Dict->NodeCount++;
    Node->Color = Red;

    while (Parent->Color == Red)
    {
        MeDictNode* Grandpa = Parent->Parent;
        if (Parent == Grandpa->Left)
        {
            MeDictNode* Uncle = Grandpa->Right;
            if (Uncle->Color != Red)
            {
                if (Node == Parent->Right)
                {
                    RotateLeft(Parent);
                    Parent = Node;
                }
                Parent->Color = Black;
                Grandpa->Color = Red;
                RotateRight(Grandpa);
                break;
            }
            Parent->Color = Black;
            Uncle->Color = Black;
        }
        else
        {
            MeDictNode* Uncle = Grandpa->Left;
            if (Uncle->Color != Red)
            {
                if (Node == Parent->Left)
                {
                    RotateRight(Parent);
                    Parent = Node;
                }
                Parent->Color = Black;
                Grandpa->Color = Red;
                RotateLeft(Grandpa);
                break;
            }
            Parent->Color = Black;
            Uncle->Color = Black;
        }
        Grandpa->Color = Red;
        Node = Grandpa;
        Parent = Grandpa->Parent;
    }
    Nil->Left->Color = Black;
}

// 0x001857b0. unlinks Node and rebalances; the node itself is not freed.
MeDictNode* MeDictDelete(MeDict* Dict, MeDictNode* Node)
{
    MeDictNode* Nil = &Dict->Nil;
    MeDictNode* Parent = Node->Parent;
    MeDictNode* Child;
    if (Node->Left == Nil || Node->Right == Nil)
    {
        Child = Node->Left == Nil ? Node->Right : Node->Left;
        Child->Parent = Parent;
        if (Node == Parent->Left)
            Parent->Left = Child;
        else
            Parent->Right = Child;
    }
    else
    {
        // the next node takes Node's place and color
        MeDictNode* Next = MeDictNext(Dict, Node);
        MeDictNode* NextParent = Next->Parent;
        const INT NextColor = Next->Color;
        Child = Next->Right;
        Child->Parent = NextParent;
        if (NextParent->Left == Next)
            NextParent->Left = Child;
        else
            NextParent->Right = Child;
        Next->Parent = Parent;
        Next->Left = Node->Left;
        Next->Right = Node->Right;
        Next->Left->Parent = Next;
        Next->Right->Parent = Next;
        Next->Color = Node->Color;
        Node->Color = NextColor;
        if (Parent->Left == Node)
            Parent->Left = Next;
        else
            Parent->Right = Next;
    }
    Node->Parent = nullptr;
    Node->Right = nullptr;
    Node->Left = nullptr;
    Dict->NodeCount--;

    if (Node->Color == Black)
    {
        Nil->Left->Color = Red;
        while (Child->Color == Black)
        {
            Parent = Child->Parent;
            if (Child == Parent->Left)
            {
                MeDictNode* Sister = Parent->Right;
                if (Sister->Color == Red)
                {
                    Sister->Color = Black;
                    Parent->Color = Red;
                    RotateLeft(Parent);
                    Sister = Parent->Right;
                }
                if (Sister->Left->Color != Black || Sister->Right->Color != Black)
                {
                    if (Sister->Right->Color == Black)
                    {
                        Sister->Left->Color = Black;
                        Sister->Color = Red;
                        RotateRight(Sister);
                        Sister = Parent->Right;
                    }
                    Sister->Color = Parent->Color;
                    Sister->Right->Color = Black;
                    Parent->Color = Black;
                    RotateLeft(Parent);
                    break;
                }
                Sister->Color = Red;
            }
            else
            {
                MeDictNode* Sister = Parent->Left;
                if (Sister->Color == Red)
                {
                    Sister->Color = Black;
                    Parent->Color = Red;
                    RotateRight(Parent);
                    Sister = Parent->Left;
                }
                if (Sister->Right->Color != Black || Sister->Left->Color != Black)
                {
                    if (Sister->Left->Color == Black)
                    {
                        Sister->Right->Color = Black;
                        Sister->Color = Red;
                        RotateLeft(Sister);
                        Sister = Parent->Left;
                    }
                    Sister->Color = Parent->Color;
                    Sister->Left->Color = Black;
                    Parent->Color = Black;
                    RotateRight(Parent);
                    break;
                }
                Sister->Color = Red;
            }
            Child = Parent;
        }
        Child->Color = Black;
        Nil->Left->Color = Black;
    }
    return Node;
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
