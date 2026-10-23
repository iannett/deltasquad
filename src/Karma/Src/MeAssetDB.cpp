#include <cmath>
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

// 0x00188050
McdSphere* McdSphereCreate(McdFramework* Framework, FLOAT Radius)
{
    McdSphere* Sphere = static_cast<McdSphere*>(MeMemory.CreateAligned(sizeof(McdSphere), 0x10));
    if (!Sphere)
        return nullptr;
    McdGeometryInit(&Sphere->Base, Framework, 1);
    Sphere->Radius = Radius;
    return Sphere;
}

// 0x00187c20. takes the full size along each axis.
McdBox* McdBoxCreate(McdFramework* Framework, FLOAT X, FLOAT Y, FLOAT Z)
{
    McdBox* Box = static_cast<McdBox*>(MeMemory.CreateAligned(sizeof(McdBox), 0x10));
    if (!Box)
        return nullptr;
    McdGeometryInit(&Box->Base, Framework, 2);
    Box->Radii[0] = X * 0.5f;
    Box->Radii[1] = Y * 0.5f;
    Box->Radii[2] = Z * 0.5f;
    Box->BoundRadius = sqrtf(Box->Radii[0] * Box->Radii[0] + Box->Radii[1] * Box->Radii[1] + Box->Radii[2] * Box->Radii[2]);
    return Box;
}

// 0x00187390
McdCylinder* McdCylinderCreate(McdFramework* Framework, FLOAT Radius, FLOAT Height)
{
    McdCylinder* Cylinder = static_cast<McdCylinder*>(MeMemory.CreateAligned(sizeof(McdCylinder), 0x10));
    if (!Cylinder)
        return nullptr;
    McdGeometryInit(&Cylinder->Base, Framework, 4);
    const FLOAT HalfHeight = Height * 0.5f;
    Cylinder->Radius = Radius;
    Cylinder->HalfHeight = HalfHeight;
    Cylinder->BoundRadius = sqrtf(Radius * Radius + HalfHeight * HalfHeight);
    return Cylinder;
}

// 0x0018b100. negative sizes become zero.
McdSphyl* McdSphylCreate(McdFramework* Framework, FLOAT Radius, FLOAT Height)
{
    McdSphyl* Sphyl = static_cast<McdSphyl*>(MeMemory.CreateAligned(sizeof(McdSphyl), 0x10));
    if (!Sphyl)
        return nullptr;
    McdGeometryInit(&Sphyl->Base, Framework, 5);
    Sphyl->Radius = Radius < 0.f ? 0.f : Radius;
    Sphyl->HalfHeight = (Height < 0.f ? 0.f : Height) * 0.5f;
    return Sphyl;
}

// 0x001b3810
McdGeometry* McdPlaneCreate(McdFramework* Framework)
{
    McdGeometry* Plane = static_cast<McdGeometry*>(MeMemory.CreateAligned(sizeof(McdGeometry), 0x10));
    if (Plane)
        McdGeometryInit(Plane, Framework, 3);
    return Plane;
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
    Hash->Count = 0;
    Hash->Hash = HashString;
    Hash->Compare = CompareStrings;
    Hash->FreeData = nullptr;
    Hash->FreeKey = nullptr;
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
void MeHashSetKeyFreeFunction(MeHash* Hash, void (*Fn)(void*))
{
    Hash->FreeKey = Fn;
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

// 0x0018e280
MeFAsset* MeAssetDBLookupAsset(MeAssetDB* DB, const char* Id)
{
    for (MeAssetDBNode* Node = DB->Assets->Next; Node->Asset; Node = Node->Next)
    {
        if (!strcmp(Node->Asset->Id, Id))
            return Node->Asset;
    }
    return nullptr;
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
    Factory->GeometryByName = MeHashCreate(97);
    Factory->NameByGeometry = MeHashCreate(97);
    MeHashSetCompareFunction(Factory->NameByGeometry, ComparePointers);
    MeHashSetHashFunction(Factory->NameByGeometry, HashInt);
    MeHashSetKeyFreeFunction(Factory->GeometryByName, MeMemory.Destroy);
    Factory->NullGeometry = McdNullCreate(Framework);
    return Factory;
}

namespace
{
    // a new string of the parts joined by "::"
    char* JoinNames(const char* A, const char* B, const char* C = nullptr)
    {
        const size_t Size = strlen(A) + strlen(B) + (C ? strlen(C) + 2 : 0) + 3;
        char* Name = static_cast<char*>(MeMemory.Create(static_cast<INT>(Size)));
        Name[0] = 0;
        strcat(Name, A);
        strcat(Name, "::");
        strcat(Name, B);
        if (C)
        {
            strcat(Name, "::");
            strcat(Name, C);
        }
        return Name;
    }
}

// 0x0018d940. geometry and primitives already made under the same names are shared.
McdGeometry* McdGMCreateGeometry(MeAssetFactory* Factory, MeFGeometry* Geometry, const char* AssetId)
{
    const INT PrimitiveCount = Geometry->PrimitiveCount;
    char* Name = JoinNames(AssetId, Geometry->Name);
    if (McdGeometry* Existing = static_cast<McdGeometry*>(MeHashLookup(Name, Factory->GeometryByName)))
    {
        MeMemory.Destroy(Name);
        return Existing;
    }
    McdAggregate* Aggregate = McdAggregateCreate(Factory->Framework, PrimitiveCount);
    MeFAssetIterator It = Geometry->Primitives->Next;
    while (MeFPrimitive* Primitive = static_cast<MeFPrimitive*>(MeFAssetIteratorGetNext(&It)))
    {
        char* PrimitiveName = JoinNames(AssetId, Geometry->Name, Primitive->Name);
        McdGeometry* Made = static_cast<McdGeometry*>(MeHashLookup(PrimitiveName, Factory->GeometryByName));
        if (Made)
        {
            MeMemory.Destroy(PrimitiveName);
        }
        else
        {
            McdFramework* Framework = Factory->Framework;
            switch (Primitive->Type)
            {
            case 2:
                Made = &McdBoxCreate(Framework, Primitive->Dims[0], Primitive->Dims[1], Primitive->Dims[2])->Base;
                break;
            case 1:
                Made = &McdSphereCreate(Framework, Primitive->Dims[0])->Base;
                break;
            case 3:
                Made = &McdCylinderCreate(Framework, Primitive->Dims[0], Primitive->Dims[1])->Base;
                break;
            case 4:
                Made = &McdSphylCreate(Framework, Primitive->Dims[0], Primitive->Dims[1])->Base;
                break;
            case 5:
                Made = McdPlaneCreate(Framework);
                break;
            case 6:
                // a convex hull of the vertices
                if (Primitive->VertexCount > 0)
                    NOT_DECOMPILED(0x0018b4f0);
                break;
            default:
                MeWarning(0, "McdGMCreateGeometry: Unknown Geometry type.");
                return nullptr;
            }
            if (!Made)
            {
                MeWarning(0, "McdGMCreateGeometry: Failed to create primitive.");
                return nullptr;
            }
            MeHashInsert(PrimitiveName, Made, Factory->GeometryByName);
            MeHashInsert(Made, PrimitiveName, Factory->NameByGeometry);
        }
        McdAggregateAddElement(Aggregate, Made, Primitive->TM);
    }
    if (!Aggregate)
    {
        MeWarning(0, "McdGMCreateGeometry: Failed to create aggregate.");
        return nullptr;
    }
    MeHashInsert(Name, &Aggregate->Base, Factory->GeometryByName);
    MeHashInsert(&Aggregate->Base, Name, Factory->NameByGeometry);
    return &Aggregate->Base;
}

// 0x0018dd60. frees geometry nothing references any more, with an aggregate's parts.
void McdGMDestroyGeometry(MeAssetFactory* Factory, McdGeometry* Geometry)
{
    if (McdGeometryGetReferenceCount(Geometry) > 0)
        return;
    if ((Geometry->TypeId & 0xff) == 8)
    {
        McdAggregate* Aggregate = reinterpret_cast<McdAggregate*>(Geometry);
        const INT Count = Aggregate->UsedCount;
        for (INT i = 0; i < Count; i++)
        {
            if (McdGeometry* Element = McdAggregateGetElementGeometry(Aggregate, i))
            {
                McdAggregateRemoveElement(Aggregate, i);
                McdGMDestroyGeometry(Factory, Element);
            }
        }
    }
    // the null geometry is never freed
    if (!(Geometry->TypeId & 0xff))
        return;
    if (void* Name = MeHashLookup(Geometry, Factory->NameByGeometry))
    {
        MeHashRemove(Name, Factory->GeometryByName);
        MeHashRemove(Geometry, Factory->NameByGeometry);
        McdGeometryDestroy(Geometry);
    }
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
