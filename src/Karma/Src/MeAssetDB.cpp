#include <cstring>

#include "Core.h"
#include "Karma.h"

// ---------------------------------------------------------------------------------------------- geometry

// 0x00189090. adds the geometry to the end of its framework's list.
void McdGeometryInit(McdGeometry* Geometry, McdFramework* Framework, SWORD TypeId)
{
    Geometry->Framework = Framework;
    Geometry->TypeId = TypeId;
    if (!Framework->Geometries)
    {
        Framework->Geometries = Geometry;
        Geometry->Next = Geometry;
        Geometry->Prev = Geometry;
    }
    else
    {
        Geometry->Next = Framework->Geometries->Next;
        Geometry->Prev = Framework->Geometries;
        Geometry->Next->Prev = Geometry;
        Geometry->Prev->Next = Geometry;
    }
    Framework->GeometryCount++;
}

// 0x0018ddf0
McdGeometry* McdNullCreate(McdFramework* Framework)
{
    McdGeometry* Geometry = static_cast<McdGeometry*>(MeMemory.CreateAligned(0x20, 0x10));
    if (!Geometry)
        return nullptr;
    McdGeometryInit(Geometry, Framework, 0);
    return Geometry;
}

// ---------------------------------------------------------------------------------------------- hash tables

namespace
{
    // 0x00189230
    DWORD HashString(const void* Key, DWORD Size)
    {
        DWORD Hash = 0;
        for (const char* Ch = static_cast<const char*>(Key); *Ch; Ch++)
            Hash = Hash * 33 + *Ch;
        return Hash % Size;
    }

    // 0x00189270
    INT CompareStrings(const void* A, const void* B)
    {
        return strcmp(static_cast<const char*>(A), static_cast<const char*>(B));
    }

    // 0x00189260
    DWORD HashInt(const void* Key, DWORD Size)
    {
        return static_cast<DWORD>(static_cast<INT>(reinterpret_cast<UPTRINT>(Key)) % static_cast<INT>(Size));
    }

    // 0x001892c0
    INT ComparePointers(const void* A, const void* B)
    {
        return A != B;
    }
}

// 0x001892e0. string keys by default.
MeHash* MeHashCreate(INT Size)
{
    MeHash* Hash = static_cast<MeHash*>(MeMemory.Create(sizeof(MeHash)));
    Hash->Unknown4 = 0;
    Hash->Hash = HashString;
    Hash->Compare = CompareStrings;
    Hash->Unknown14 = 0;
    Hash->FreeData = nullptr;
    Hash->Unknown1C = 0;
    if (Size < 0x40000000 && Size != 0)
    {
        Hash->Table = static_cast<MeHashEntry**>(MeMemory.Create(Size * static_cast<INT>(sizeof(MeHashEntry*))));
        if (Hash->Table)
        {
            Hash->Size = Size;
            for (INT i = Size - 1; i >= 0; i--)
                Hash->Table[i] = nullptr;
            return Hash;
        }
    }
    // the table itself is not freed
    Hash->Size = 0;
    return nullptr;
}

// 0x001893c0
void MeHashSetHashFunction(MeHash* Hash, DWORD (*Fn)(const void*, DWORD))
{
    Hash->Hash = Fn;
}

// 0x001893d0
void MeHashSetCompareFunction(MeHash* Hash, INT (*Fn)(const void*, const void*))
{
    Hash->Compare = Fn;
}

// 0x00188150
void MeHashSetDataFreeFunction(MeHash* Hash, void (*Fn)(void*))
{
    Hash->FreeData = Fn;
}

// ---------------------------------------------------------------------------------------------- asset database

// 0x0018e250
MeAssetDB* MeAssetDBCreate()
{
    MeAssetDB* DB = static_cast<MeAssetDB*>(MeMemory.CreateZeroed(sizeof(MeAssetDB)));
    DB->Assets = static_cast<MeAssetDBNode*>(MeMemory.Create(sizeof(MeAssetDBNode)));
    DB->Assets->Next = DB->Assets;
    DB->Assets->Prev = DB->Assets->Next;
    DB->Assets->Asset = nullptr;
    DB->Count = 0;
    return DB;
}

// 0x0018e400. kept sorted by id.
void MeAssetDBInsertAsset(MeAssetDB* DB, MeFAsset* Asset)
{
    if (Asset->Owner)
    {
        if (Asset->Owner == DB)
            return;
        MeAssetDBRemoveAsset(Asset);
    }
    Asset->Owner = DB;
    MeAssetDBNode* Node = static_cast<MeAssetDBNode*>(MeMemory.Create(sizeof(MeAssetDBNode)));
    Node->Asset = Asset;
    MeAssetDBNode* After = DB->Assets;
    while (After->Prev->Asset && strcmp(After->Prev->Asset->Id, Asset->Id) > 0)
        After = After->Prev;
    Node->Prev = After->Prev;
    After->Prev->Next = Node;
    After->Prev = Node;
    Node->Next = After;
    DB->Count++;
}

// 0x0018e2f0. an asset missing from the list unlinks the list's own head instead.
void MeAssetDBRemoveAsset(MeFAsset* Asset)
{
    if (!Asset->Owner)
        return;
    MeAssetDBNode* Head = Asset->Owner->Assets;
    MeAssetDBNode* Node = Head->Next;
    if (Node != Head)
    {
        do
        {
            if (Node->Asset == Asset)
                break;
            Node = Node->Next;
        } while (Node != Asset->Owner->Assets);
    }
    Node->Next->Prev = Node->Prev;
    Node->Prev->Next = Node->Next;
    MeMemory.Destroy(Node);
    Asset->Owner->Count--;
    Asset->Owner = nullptr;
}

// 0x00184cf0
INT MeAssetDBGetAssetCount(MeAssetDB* DB)
{
    return DB->Count;
}

// 0x0018d8a0
MeAssetFactory* MeAssetFactoryCreate(McdFramework* Framework)
{
    MeAssetFactory* Factory = static_cast<MeAssetFactory*>(MeMemory.Create(sizeof(MeAssetFactory)));
    Factory->Framework = Framework;
    Factory->Unknown4 = MeHashCreate(97);
    Factory->Unknown8 = MeHashCreate(97);
    MeHashSetCompareFunction(Factory->Unknown8, ComparePointers);
    MeHashSetHashFunction(Factory->Unknown8, HashInt);
    MeHashSetDataFreeFunction(Factory->Unknown4, MeMemory.Destroy);
    Factory->NullGeometry = McdNullCreate(Framework);
    return Factory;
}

// ---------------------------------------------------------------------------------------------- xml input

// 0x00197d50
MeAssetDBXMLInput* MeAssetDBXMLInputCreate(MeAssetDB* DB, void* Unknown)
{
    MeAssetDBXMLInput* Input = static_cast<MeAssetDBXMLInput*>(MeMemory.Create(sizeof(MeAssetDBXMLInput)));
    Input->DB = DB;
    Input->Unknown4 = Unknown;
    return Input;
}

// 0x001cf2e0
void MeAssetDBXMLInputDestroy(MeAssetDBXMLInput* Input)
{
    MeMemory.Destroy(Input);
}

// 0x00197d70. reads the file into a tree, then makes an asset of each child of the root.
UBOOL MeAssetDBXMLInputRead(MeAssetDBXMLInput* Input, MeFile* File)
{
    MeXMLHandler Handlers[2] = {};
    Handlers[0].Tag = "KARMA";
    Handlers[0].Handler = reinterpret_cast<void*>(MeAssetDBXMLReadKarma);
    Handlers[1].Type = 2;
    MeXMLNode* Root = MeXMLNodeCreate(MEXML_Root, "ROOT", nullptr, nullptr, nullptr);
    if (!Input || !File)
        return 0;
    MeXMLInput* XML = MeXMLInputCreate(File);
    MeXMLInputSetUserData(XML, Input);
    if (MeXMLInputProcess(XML, Handlers, Root) == 0)
    {
        MeMemory.Destroy(XML);
        for (MeXMLList* Entry = Root->Children; Entry; Entry = Entry->Next)
            MeAssetDBInsertAsset(Input->DB, MeAssetDBXML.CreateAsset(Input->DB, Input->Unknown4, static_cast<MeXMLNode*>(Entry->Item)));
        MeXMLNodeWalk(Root, MeXMLNodeDestroyChildren, 0, nullptr);
        MeXMLNodeDestroyChildren(Root, nullptr, nullptr);
        MeXMLNodeDestroy(Root);
        return 1;
    }
    MeWarning(3, "Parse Error in file %s.\n%s", File->Name, MeXMLInputGetErrorString(XML));
    MeMemory.Destroy(XML);
    return 0;
}
