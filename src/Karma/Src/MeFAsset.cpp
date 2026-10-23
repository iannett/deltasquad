#include <cmath>
#include <cstdlib>
#include <cstring>

#include "Core.h"
#include "Karma.h"

// ---------------------------------------------------------------------------------------------- strings and ids

// 0x0018e340. frees the old copy and keeps a copy of Str, or nothing.
void MeStrSet(char** Field, const char* Str)
{
    if (*Field)
    {
        MeMemory.Destroy(*Field);
        *Field = nullptr;
    }
    if (Str)
    {
        *Field = static_cast<char*>(MeMemory.Create(static_cast<INT>(strlen(Str)) + 1));
        strcpy(*Field, Str);
    }
}

// 0x001ca330
MeIDPool* MeIDPoolCreate()
{
    MeIDPool* Pool = static_cast<MeIDPool*>(MeMemory.Create(sizeof(MeIDPool)));
    Pool->Capacity = 32;
    Pool->Words = 1;
    Pool->Bits = static_cast<DWORD*>(MeMemory.CreateZeroed(sizeof(DWORD)));
    Pool->Word = 0;
    Pool->Used = 0;
    return Pool;
}

// 0x001ca360. the first free id from the word last searched, high bit first. a full pool grows by 32 ids, though
// its word count comes from an odd formula and only the first new word is cleared.
INT MeIDPoolRequestID(MeIDPool* Pool)
{
    UBOOL Found = 0;
    if (Pool->Used == Pool->Capacity)
    {
        const INT OldWords = Pool->Words;
        Pool->Capacity += 32;
        const INT Words = Pool->Capacity < 32 ? 1 : Pool->Capacity / 32 + Pool->Capacity % 32;
        Pool->Words = Words;
        if (static_cast<DWORD>(Words) > static_cast<DWORD>(OldWords))
        {
            Pool->Bits = static_cast<DWORD*>(MeMemory.Resize(Pool->Bits, Words * static_cast<INT>(sizeof(DWORD))));
            Pool->Bits[OldWords] = 0;
            Pool->Word++;
        }
    }
    for (;;)
    {
        const INT Word = Pool->Word;
        DWORD Bit = 0x80000000;
        INT Id = Word * 32;
        INT i = 0;
        while (static_cast<DWORD>(Id) < static_cast<DWORD>(Pool->Capacity))
        {
            if (!(Pool->Bits[Word] & Bit))
            {
                Found = 1;
                break;
            }
            i++;
            Id++;
            Bit >>= 1;
            if (i >= 32)
                break;
        }
        if (!Found || i == 31)
            Pool->Word = Word + 1;
        if (static_cast<DWORD>(Pool->Word) >= static_cast<DWORD>(Pool->Words))
            Pool->Word = 0;
        if (Found)
        {
            Pool->Bits[Word] |= Bit;
            Pool->Used++;
            return Id;
        }
    }
}

// 0x001ca480
void MeIDPoolReset(MeIDPool* Pool)
{
    memset(Pool->Bits, 0, (Pool->Words & 0x3fffffff) * sizeof(DWORD));
    Pool->Word = 0;
    Pool->Used = 0;
}

// ---------------------------------------------------------------------------------------------- hash tables

struct MeHashEntry
{
    const void*  Key;
    void*        Data;
    MeHashEntry* Next;
};

// 0x001893e0. chains stay sorted; a key already present keeps its data.
void* MeHashInsert(const void* Key, void* Data, MeHash* Hash)
{
    const DWORD Index = Hash->Hash(Key, Hash->Size);
    MeHashEntry* Entry = Hash->Table[Index];
    MeHashEntry* New;
    if (!Entry)
    {
        New = static_cast<MeHashEntry*>(MeMemory.Create(sizeof(MeHashEntry)));
        if (!New)
            return nullptr;
        New->Next = nullptr;
    }
    else
    {
        for (;;)
        {
            const INT Order = Hash->Compare(Key, Entry->Key);
            if (Order == 0)
                return Entry->Data;
            Hash->Unknown1C++;
            if (Order < 0)
            {
                New = static_cast<MeHashEntry*>(MeMemory.Create(sizeof(MeHashEntry)));
                if (!New)
                    return nullptr;
                New->Next = Entry;
                break;
            }
            if (!Entry->Next || Hash->Compare(Key, Entry->Next->Key) < 0)
            {
                New = static_cast<MeHashEntry*>(MeMemory.Create(sizeof(MeHashEntry)));
                if (!New)
                    return nullptr;
                New->Key = Key;
                New->Data = Data;
                New->Next = Entry->Next;
                Entry->Next = New;
                Hash->Count++;
                return Data;
            }
            Entry = Entry->Next;
        }
    }
    New->Data = Data;
    New->Key = Key;
    Hash->Table[Index] = New;
    Hash->Count++;
    return Data;
}

// 0x001894c0
void* MeHashLookup(const void* Key, MeHash* Hash)
{
    for (MeHashEntry* Entry = Hash->Table[Hash->Hash(Key, Hash->Size)]; Entry; Entry = Entry->Next)
    {
        const INT Order = Hash->Compare(Key, Entry->Key);
        if (Order == 0)
            return Entry->Data;
        if (Order < 0)
            return nullptr;
    }
    return nullptr;
}

// 0x00189500. takes the entry out and returns its data, after the free functions have run.
void* MeHashRemove(const void* Key, MeHash* Hash)
{
    const DWORD Index = Hash->Hash(Key, Hash->Size);
    MeHashEntry* Prev = nullptr;
    MeHashEntry* Entry = Hash->Table[Index];
    for (;; Prev = Entry, Entry = Entry->Next)
    {
        if (!Entry)
            return nullptr;
        const INT Order = Hash->Compare(Key, Entry->Key);
        if (Order == 0)
            break;
        if (Order < 0)
            return nullptr;
    }
    if (Prev)
        Prev->Next = Entry->Next;
    else
        Hash->Table[Index] = Entry->Next;
    void* Data = Entry->Data;
    if (Hash->FreeData)
        Hash->FreeData(Data);
    if (Hash->FreeKey)
        Hash->FreeKey(const_cast<void*>(Entry->Key));
    MeMemory.Destroy(Entry);
    Hash->Count--;
    return Data;
}

// 0x00189350
void MeHashEmpty(MeHash* Hash)
{
    for (INT i = Hash->Size - 1; i >= 0; i--)
    {
        while (MeHashEntry* Entry = Hash->Table[i])
        {
            Hash->Table[i] = Entry->Next;
            if (Hash->FreeData)
                Hash->FreeData(Entry->Data);
            if (Hash->FreeKey)
                Hash->FreeKey(const_cast<void*>(Entry->Key));
            MeMemory.Destroy(Entry);
        }
    }
    Hash->Count = 0;
}

// 0x00189650
void MeHashDestroy(MeHash* Hash)
{
    MeHashEmpty(Hash);
    MeMemory.Destroy(Hash->Table);
    MeMemory.Destroy(Hash);
}

// 0x00189590
void MeHashIteratorInit(MeHashIterator* It, MeHash* Hash)
{
    if (!It)
        return;
    It->Entry = nullptr;
    It->Bucket = 0;
    It->Hash = Hash;
}

// 0x001895b0. the data of the next entry, or null once every bucket is done.
void* MeHashIteratorGetNext(MeHashIterator* It)
{
    MeHash* Hash = It->Hash;
    if (It->Entry && It->Entry->Next)
    {
        It->Entry = It->Entry->Next;
        return It->Entry->Data;
    }
    for (It->Bucket = It->Entry ? It->Bucket + 1 : 0; It->Bucket < Hash->Size; It->Bucket++)
    {
        if (Hash->Table[It->Bucket])
        {
            It->Entry = Hash->Table[It->Bucket];
            return It->Entry->Data;
        }
    }
    It->Bucket = 0;
    It->Entry = nullptr;
    return nullptr;
}

// ---------------------------------------------------------------------------------------------- assets

namespace
{
    MeFAssetNode* CreateList()
    {
        return static_cast<MeFAssetNode*>(MeMemory.Create(sizeof(MeFAssetNode)));
    }

    void ClearList(MeFAssetNode* List)
    {
        List->Next = List;
        List->Prev = List->Next;
        List->Item = nullptr;
    }

    void Append(MeFAssetNode* List, void* Item)
    {
        MeFAssetNode* Node = static_cast<MeFAssetNode*>(MeMemory.Create(sizeof(MeFAssetNode)));
        Node->Item = Item;
        Node->Prev = List->Prev;
        List->Prev->Next = Node;
        List->Prev = Node;
        Node->Next = List;
    }

    // 0x00190b20
    void ResetAsset(MeFAsset* Asset)
    {
        Asset->AssetId = 0;
        Asset->Id = nullptr;
        Asset->Owner = nullptr;
        Asset->Graphic = nullptr;
        Asset->RefPart = nullptr;
        Asset->GeometryCount = 0;
        Asset->ModelCount = 0;
        Asset->PartCount = 0;
        Asset->JointCount = 0;
        Asset->Scale = 1.f;
        Asset->MassScale = 1.f;
        Asset->LengthScale = 1.f;
        ClearList(Asset->Geometries);
        ClearList(Asset->Models);
        ClearList(Asset->Parts);
        ClearList(Asset->Joints);
        MeIDPoolReset(Asset->PartIDs);
        memset(Asset->NoCollision, 0, Asset->MaxParts * Asset->MaxParts / 8);
    }

    // 0x00190f00
    void RemoveGeometry(MeFGeometry* Geometry)
    {
        NOT_DECOMPILED(0x00190f00);
    }

    // 0x00190f60
    void RemoveModel(MeFModel* Model)
    {
        NOT_DECOMPILED(0x00190f60);
    }

    // 0x00191020
    void RemovePart(MeFPart* Part)
    {
        NOT_DECOMPILED(0x00191020);
    }

    // 0x00190fc0
    void RemoveJoint(MeFJoint* Joint)
    {
        NOT_DECOMPILED(0x00190fc0);
    }

    // 0x00191eb0
    void RemovePrimitive(MeFPrimitive* Primitive)
    {
        NOT_DECOMPILED(0x00191eb0);
    }
}

// 0x00190bd0
MeFAsset* MeFAssetCreate(const char* Id, INT AssetId)
{
    MeFAsset* Asset = static_cast<MeFAsset*>(MeMemory.CreateZeroed(sizeof(MeFAsset)));
    Asset->Geometries = CreateList();
    Asset->Models = CreateList();
    Asset->Parts = CreateList();
    Asset->Joints = CreateList();
    Asset->GeometryHash = MeHashCreate(17);
    Asset->ModelHash = MeHashCreate(17);
    Asset->PartHash = MeHashCreate(17);
    Asset->JointHash = MeHashCreate(17);
    Asset->MaxParts = 256;
    Asset->NoCollision = static_cast<DWORD*>(MeMemory.CreateZeroed(0x2000));
    Asset->PartIDs = MeIDPoolCreate();
    ResetAsset(Asset);
    MeStrSet(&Asset->Id, Id);
    Asset->AssetId = AssetId;
    return Asset;
}

// 0x001911e0
void MeFAssetInsertGeometry(MeFAsset* Asset, MeFGeometry* Geometry)
{
    if (Geometry->Owner)
    {
        if (Geometry->Owner == Asset)
            return;
        RemoveGeometry(Geometry);
    }
    Append(Asset->Geometries, Geometry);
    MeHashInsert(Geometry->Name, Geometry, Asset->GeometryHash);
    Geometry->Owner = Asset;
    Asset->GeometryCount++;
}

// 0x00191240
void MeFAssetInsertModel(MeFAsset* Asset, MeFModel* Model)
{
    if (Model->Owner)
    {
        if (Model->Owner == Asset)
            return;
        RemoveModel(Model);
    }
    Append(Asset->Models, Model);
    MeHashInsert(Model->Name, Model, Asset->ModelHash);
    Model->Owner = Asset;
    Asset->ModelCount++;
}

// 0x00191300. a part leaves another asset only while that asset is under its part limit.
void MeFAssetInsertPart(MeFAsset* Asset, MeFPart* Part)
{
    if (Part->Owner)
    {
        if (Part->Owner == Asset)
            return;
        if (Part->Owner->MaxParts <= Part->Owner->PartCount)
            return;
        RemovePart(Part);
    }
    Append(Asset->Parts, Part);
    MeHashInsert(Part->Name, Part, Asset->PartHash);
    Part->Owner = Asset;
    Part->Id = MeIDPoolRequestID(Asset->PartIDs);
    Asset->PartCount++;
}

// 0x001912a0
void MeFAssetInsertJoint(MeFAsset* Asset, MeFJoint* Joint)
{
    if (Joint->Owner)
    {
        if (Joint->Owner == Asset)
            return;
        RemoveJoint(Joint);
    }
    Append(Asset->Joints, Joint);
    MeHashInsert(Joint->Name, Joint, Asset->JointHash);
    Joint->Owner = Asset;
    Asset->JointCount++;
}

// 0x001921c0, which the xbox shares with MeAssetInstanceGetModel
MeFGeometry* MeFAssetLookupGeometry(MeFAsset* Asset, const char* Name)
{
    return static_cast<MeFGeometry*>(MeHashLookup(Name, Asset->GeometryHash));
}

// 0x001921e0, which the xbox shares with MeAssetInstanceGetJoint
MeFModel* MeFAssetLookupModel(MeFAsset* Asset, const char* Name)
{
    return static_cast<MeFModel*>(MeHashLookup(Name, Asset->ModelHash));
}

// 0x00190d20
MeFPart* MeFAssetLookupPart(MeFAsset* Asset, const char* Name)
{
    return static_cast<MeFPart*>(MeHashLookup(Name, Asset->PartHash));
}

// 0x00190ef0
void MeFAssetSetGraphic(MeFAsset* Asset, const char* Graphic)
{
    MeStrSet(&Asset->Graphic, Graphic);
}

// 0x00190ee0
void MeFAssetSetRefPart(MeFAsset* Asset, const char* RefPart)
{
    MeStrSet(&Asset->RefPart, RefPart);
}

// 0x0018cfe0
void MeFAssetSetScale(MeFAsset* Asset, FLOAT Scale)
{
    Asset->Scale = Scale;
}

// 0x0018d2b0
void MeFAssetSetMassScale(MeFAsset* Asset, FLOAT Scale)
{
    Asset->MassScale = Scale;
}

// 0x001910a0
void MeFAssetSetLengthScale(MeFAsset* Asset, FLOAT Scale)
{
    Asset->LengthScale = Scale;
}

// ---------------------------------------------------------------------------------------------- geometry

// 0x00191df0
MeFGeometry* MeFGeometryCreate(const char* Name)
{
    MeFGeometry* Geometry = static_cast<MeFGeometry*>(MeMemory.Create(sizeof(MeFGeometry)));
    Geometry->Primitives = CreateList();
    Geometry->Name = nullptr;
    Geometry->Owner = nullptr;
    ClearList(Geometry->Primitives);
    Geometry->Graphic = nullptr;
    Geometry->Scale = 1.f;
    Geometry->GraphicOffset[0] = 0.f;
    Geometry->GraphicOffset[1] = 0.f;
    Geometry->GraphicOffset[2] = 0.f;
    Geometry->PrimitiveCount = 0;
    MeStrSet(&Geometry->Name, Name);
    return Geometry;
}

// 0x00191fb0. kept sorted by name.
void MeFGeometryInsertPrimitive(MeFGeometry* Geometry, MeFPrimitive* Primitive)
{
    if (Primitive->Owner)
    {
        if (Primitive->Owner == Geometry)
            return;
        RemovePrimitive(Primitive);
    }
    MeFAssetNode* Node = static_cast<MeFAssetNode*>(MeMemory.Create(sizeof(MeFAssetNode)));
    Node->Item = Primitive;
    MeFAssetNode* After = Geometry->Primitives;
    while (After->Prev->Item && strcmp(static_cast<MeFPrimitive*>(After->Prev->Item)->Name, Primitive->Name) > 0)
        After = After->Prev;
    Node->Prev = After->Prev;
    After->Prev->Next = Node;
    After->Prev = Node;
    Node->Next = After;
    Primitive->Owner = Geometry;
    Geometry->PrimitiveCount++;
}

// 0x0018d2a0, the same code as MeFPartSetGraphic
void MeFGeometrySetGraphic(MeFGeometry* Geometry, const char* Graphic)
{
    MeStrSet(&Geometry->Graphic, Graphic);
}

// 0x00191e80
void MeFGeometrySetScale(MeFGeometry* Geometry, FLOAT Scale)
{
    Geometry->Scale = Scale;
}

// 0x00191e90
void MeFGeometrySetGraphicOffset(MeFGeometry* Geometry, FLOAT X, FLOAT Y, FLOAT Z)
{
    Geometry->GraphicOffset[0] = X;
    Geometry->GraphicOffset[1] = Y;
    Geometry->GraphicOffset[2] = Z;
}

// 0x001b3630
MeFPrimitive* MeFPrimitiveCreate(const char* Name, INT Type)
{
    MeFPrimitive* Primitive = static_cast<MeFPrimitive*>(MeMemory.CreateZeroed(sizeof(MeFPrimitive)));
    // 0x001b35d0
    Primitive->Name = nullptr;
    Primitive->Owner = nullptr;
    Primitive->VertexCount = 0;
    Primitive->VertexCapacity = 1;
    Primitive->Vertices = static_cast<FLOAT*>(MeMemory.Create(3 * sizeof(FLOAT)));
    Primitive->Type = 0;
    Primitive->Dims[0] = 1.f;
    Primitive->Dims[1] = 1.f;
    Primitive->Dims[2] = 1.f;
    memset(Primitive->TM, 0, sizeof(Primitive->TM));
    Primitive->TM[0] = 1.f;
    Primitive->TM[5] = 1.f;
    Primitive->TM[10] = 1.f;
    Primitive->TM[15] = 1.f;
    MeStrSet(&Primitive->Name, Name);
    Primitive->Type = Type;
    return Primitive;
}

// 0x001b3760
void MeFPrimitiveSetRadius(MeFPrimitive* Primitive, FLOAT Radius)
{
    Primitive->Dims[0] = Radius;
}

// 0x001b3790, the same code as MeFModelSetMass
void MeFPrimitiveSetHeight(MeFPrimitive* Primitive, FLOAT Height)
{
    Primitive->Dims[1] = Height;
}

// 0x001b3770
void MeFPrimitiveSetDims(MeFPrimitive* Primitive, FLOAT X, FLOAT Y, FLOAT Z)
{
    Primitive->Dims[0] = X;
    Primitive->Dims[1] = Y;
    Primitive->Dims[2] = Z;
}

// 0x001b37a0
void MeFPrimitiveAddVertex(MeFPrimitive* Primitive, const FLOAT* Vertex)
{
    FLOAT* Out = Primitive->Vertices + Primitive->VertexCount++ * 3;
    Out[0] = Vertex[0];
    Out[1] = Vertex[1];
    Out[2] = Vertex[2];
    const INT Capacity = Primitive->VertexCapacity;
    if (Capacity <= Primitive->VertexCount)
    {
        Primitive->VertexCapacity = Capacity * 2;
        Primitive->Vertices = static_cast<FLOAT*>(MeMemory.Resize(Primitive->Vertices, Capacity * 2 * 3 * static_cast<INT>(sizeof(FLOAT))));
    }
}

// 0x001b37f0. the copy is 0x0018d050.
void MeFPrimitiveSetTM(MeFPrimitive* Primitive, const FLOAT* TM)
{
    memcpy(Primitive->TM, TM, sizeof(Primitive->TM));
}

// ---------------------------------------------------------------------------------------------- models and parts

// 0x0018cde0
MeFModel* MeFModelCreate(const char* Name, INT Type)
{
    MeFModel* Model = static_cast<MeFModel*>(MeMemory.CreateZeroed(sizeof(MeFModel)));
    Model->Name = nullptr;
    Model->Owner = nullptr;
    Model->Geometry = nullptr;
    Model->MassOffset[0] = Model->MassOffset[1] = Model->MassOffset[2] = 0.f;
    Model->AngularDamping = 0.f;
    Model->LinearDamping = 0.f;
    Model->FastSpin[0] = 0.f;
    Model->FastSpin[2] = 0.f;
    Model->UseFastSpin = 0;
    Model->Inertia01 = 0.f;
    Model->Inertia02 = 0.f;
    Model->Inertia12 = 0.f;
    Model->FastSpin[1] = 1.f;
    Model->Inertia00 = 0.4f;
    Model->Inertia11 = 0.4f;
    Model->Inertia22 = 0.4f;
    Model->Mass = 1.f;
    Model->Density = 1.f;
    MeStrSet(&Model->Name, Name);
    Model->Type = Type;
    return Model;
}

// 0x0018cf80
void MeFModelSetMassOffset(MeFModel* Model, const FLOAT* Offset)
{
    Model->MassOffset[0] = Offset[0];
    Model->MassOffset[1] = Offset[1];
    Model->MassOffset[2] = Offset[2];
}

// 0x001b3790
void MeFModelSetMass(MeFModel* Model, FLOAT Mass)
{
    Model->Mass = Mass;
}

// 0x0018cfa0
void MeFModelSetDensity(MeFModel* Model, FLOAT Density)
{
    Model->Density = Density;
}

// 0x0018cfb0. takes a full 3x3 matrix and keeps its upper triangle.
void MeFModelSetInertiaTensor(MeFModel* Model, const FLOAT* Inertia)
{
    Model->Inertia00 = Inertia[0];
    Model->Inertia11 = Inertia[4];
    Model->Inertia22 = Inertia[8];
    Model->Inertia01 = Inertia[3];
    Model->Inertia02 = Inertia[6];
    Model->Inertia12 = Inertia[7];
}

// 0x0018cfe0, the same code as MeFAssetSetScale
void MeFModelSetLinearDamping(MeFModel* Model, FLOAT Damping)
{
    Model->LinearDamping = Damping;
}

// 0x0018cff0
void MeFModelSetAngularDamping(MeFModel* Model, FLOAT Damping)
{
    Model->AngularDamping = Damping;
}

// 0x0018d000
void MeFModelSetFastSpinAxis(MeFModel* Model, FLOAT X, FLOAT Y, FLOAT Z)
{
    Model->FastSpin[0] = X;
    Model->FastSpin[1] = Y;
    Model->FastSpin[2] = Z;
}

// 0x00165b70
void MeFModelSetUseFastSpinAxis(MeFModel* Model, INT Use)
{
    Model->UseFastSpin = Use;
}

// 0x0018d020
void MeFModelSetGeometry(MeFModel* Model, MeFGeometry* Geometry)
{
    if (Geometry)
        MeStrSet(&Model->Geometry, Geometry->Name);
}

namespace
{
    // 0x00189670. Result = A * B for 3x3 matrices.
    void MultiplyMatrix3(const FLOAT* A, const FLOAT* B, FLOAT* Result)
    {
        for (INT i = 0; i < 3; i++)
            for (INT j = 0; j < 3; j++)
                Result[i * 3 + j] = A[i * 3 + 0] * B[0 * 3 + j] + A[i * 3 + 1] * B[1 * 3 + j] + A[i * 3 + 2] * B[2 * 3 + j];
    }

    // 0x0018c630. each element within Epsilon of the identity, relative to the element when it is not tiny.
    UBOOL IsIdentityMatrix3(const FLOAT* M, FLOAT Epsilon)
    {
        FLOAT Identity[3] = { 0.f, 0.f, 0.f };
        for (INT i = 0; i < 3; i++)
        {
            Identity[i] = 1.f;
            for (INT j = 0; j < 3; j++)
            {
                const FLOAT Value = M[i * 3 + j];
                const FLOAT Error = fabsf(Value) >= Epsilon ? (Identity[j] - Value) / Value : Identity[j];
                if (fabsf(Error) >= Epsilon)
                    return 0;
            }
            Identity[i] = 0.f;
        }
        return 1;
    }

    // 0x0018c710. a rotation: orthonormal with a determinant near 1.
    UBOOL IsValidOrientationMatrix3(const FLOAT* M, FLOAT Epsilon)
    {
        const FLOAT Transpose[9] = { M[0], M[3], M[6], M[1], M[4], M[7], M[2], M[5], M[8] };
        FLOAT Product[9];
        MultiplyMatrix3(M, Transpose, Product);
        if (!IsIdentityMatrix3(Product, Epsilon))
            return 0;
        const FLOAT Det = (M[5] * M[1] - M[4] * M[2]) * M[6] + (M[8] * M[4] - M[5] * M[7]) * M[0] + (M[2] * M[7] - M[8] * M[1]) * M[3];
        return 1.f - Epsilon < Det;
    }

    // 0x0018c820. a transform: a rotation with a bottom row of 0, 0, 0, 1.
    UBOOL IsValidTM(const FLOAT* TM, FLOAT Epsilon)
    {
        FLOAT Corner = TM[15];
        if (static_cast<DOUBLE>(Epsilon) <= 1.0)
            Corner -= 1.f;
        if (fabsf(Corner) >= Epsilon)
            return 0;
        for (INT i = 0; i < 3; i++)
        {
            if (Epsilon <= fabsf(TM[3 + i * 4]))
                return 0;
        }
        const FLOAT Rotation[9] = { TM[0], TM[1], TM[2], TM[4], TM[5], TM[6], TM[8], TM[9], TM[10] };
        return IsValidOrientationMatrix3(Rotation, Epsilon);
    }
}

// 0x0018d110. the transform is checked, but copied whatever the result.
MeFPart* MeFPartCreate(const char* Name, MeFModel* Model, const FLOAT* TM)
{
    MeFPart* Part = static_cast<MeFPart*>(MeMemory.CreateZeroed(sizeof(MeFPart)));
    // 0x0018d0b0
    Part->Name = nullptr;
    Part->Owner = nullptr;
    Part->Graphic = nullptr;
    Part->Model = nullptr;
    Part->Parent = nullptr;
    memset(Part->TM, 0, sizeof(Part->TM));
    Part->TM[0] = 1.f;
    Part->TM[5] = 1.f;
    Part->TM[10] = 1.f;
    Part->TM[15] = 1.f;
    Part->Scale = 1.f;
    Part->GraphicOffset[0] = Part->GraphicOffset[1] = Part->GraphicOffset[2] = 0.f;
    Part->Id = -1;
    MeStrSet(&Part->Name, Name);
    MeStrSet(&Part->Model, Model->Name);
    if (TM)
    {
        IsValidTM(TM, 0.001f);
        memcpy(Part->TM, TM, sizeof(Part->TM));
    }
    return Part;
}

// 0x0018d2a0
void MeFPartSetGraphic(MeFPart* Part, const char* Graphic)
{
    MeStrSet(&Part->Graphic, Graphic);
}

// 0x0018d2b0, the same code as MeFAssetSetMassScale
void MeFPartSetScale(MeFPart* Part, FLOAT Scale)
{
    Part->Scale = Scale;
}

// 0x0018d2e0
void MeFPartSetParent(MeFPart* Part, const char* Parent)
{
    MeStrSet(&Part->Parent, Parent);
}

// 0x0018d2c0
void MeFPartSetGraphicOffset(MeFPart* Part, FLOAT X, FLOAT Y, FLOAT Z)
{
    Part->GraphicOffset[0] = X;
    Part->GraphicOffset[1] = Y;
    Part->GraphicOffset[2] = Z;
}

namespace
{
    // 0x0018d200. the word and bit of the pair in the asset's table.
    DWORD* GetCollisionBit(MeFPart* Part1, MeFPart* Part2, DWORD& Bit)
    {
        MeFAsset* Asset = Part1->Owner;
        INT Low = Part1->Id;
        INT High = Part2->Id;
        if (Low > High)
        {
            const INT Swap = Low;
            Low = High;
            High = Swap;
        }
        const INT Index = Asset->MaxParts * Low + High;
        Bit = 0x80000000u >> (Index % 32);
        return Asset->NoCollision + Index / 32;
    }
}

// 0x0018d2f0. a set bit turns collision off between the two parts.
void MeFPartSetCollision(MeFPart* Part1, MeFPart* Part2, UBOOL Enable)
{
    if (Part1 == Part2)
        return;
    MeFAsset* Asset = Part1->Owner;
    if (Asset && Asset->PartCount > Asset->MaxParts)
        return;
    DWORD Bit;
    DWORD* Word = GetCollisionBit(Part1, Part2, Bit);
    if (Enable)
        *Word &= ~Bit;
    else
        *Word |= Bit;
}

// 0x0018d260. a part always collides with itself.
UBOOL MeFPartIsCollisionEnabled(MeFPart* Part1, MeFPart* Part2)
{
    if (Part1 == Part2)
        return 1;
    DWORD Bit;
    return !(*GetCollisionBit(Part1, Part2, Bit) & Bit);
}

// 0x0018d1a0
MeFModel* MeFPartGetModel(MeFPart* Part)
{
    return MeFAssetLookupModel(Part->Owner, Part->Model);
}

// 0x0018d1c0
FLOAT* MeFPartGetTransformPtr(MeFPart* Part)
{
    return Part->TM;
}

// 0x0018cee0. the symmetric tensor as nine floats.
void MeFModelGetInertiaTensor(MeFModel* Model, FLOAT* Inertia)
{
    Inertia[0] = Model->Inertia00;
    Inertia[4] = Model->Inertia11;
    Inertia[8] = Model->Inertia22;
    Inertia[1] = Inertia[3] = Model->Inertia01;
    Inertia[2] = Inertia[6] = Model->Inertia02;
    Inertia[5] = Inertia[7] = Model->Inertia12;
}

// 0x0018ce80
MeFGeometry* MeFModelGetGeometry(MeFModel* Model)
{
    if (!Model->Owner || !Model->Geometry)
        return nullptr;
    return MeFAssetLookupGeometry(Model->Owner, Model->Geometry);
}

// 0x00191e70. the next item of the list, or null at its end.
void* MeFAssetIteratorGetNext(MeFAssetIterator* It)
{
    void* Item = (*It)->Item;
    *It = (*It)->Next;
    return Item;
}

// 0x00190cc0
void MeFAssetGetGeometryIterator(MeFAsset* Asset, MeFAssetIterator* It)
{
    *It = Asset->Geometries->Next;
}

// 0x00190ce0
void MeFAssetGetJointIterator(MeFAsset* Asset, MeFAssetIterator* It)
{
    *It = Asset->Joints->Next;
}

// 0x00190cb0
INT MeFAssetGetPartCount(MeFAsset* Asset)
{
    return Asset->PartCount;
}

// 0x00190ca0
INT MeFAssetGetJointCount(MeFAsset* Asset)
{
    return Asset->JointCount;
}

// 0x00165900, through the shared getter
const char* MeFAssetGetId(MeFAsset* Asset)
{
    return Asset->Id;
}

namespace
{
    // 0x00190d40
    int ComparePartNames(const void* A, const void* B)
    {
        return strcmp((*static_cast<MeFPart* const*>(A))->Name, (*static_cast<MeFPart* const*>(B))->Name);
    }
}

// 0x00190e30. Parts must have room for every part.
void MeFAssetGetPartsSortedByName(MeFAsset* Asset, MeFPart** Parts)
{
    MeFPart** Out = Parts;
    for (MeFAssetNode* Node = Asset->Parts->Next; Node->Item; Node = Node->Next)
        *Out++ = static_cast<MeFPart*>(Node->Item);
    qsort(Parts, Asset->PartCount, sizeof(MeFPart*), ComparePartNames);
}

// ---------------------------------------------------------------------------------------------- joints

namespace
{
    // 0x00191b30. the defaults of each joint type.
    void ResetJoint(MeFJoint* Joint)
    {
        for (INT i = 0; i < 29; i++)
            Joint->Values[i].Int = 0;
        MeFJointValue* V = Joint->Values;
        switch (Joint->Type)
        {
        case 1:
            V[23].Real = 1.f;
            V[24].Real = 1.f;
            V[9].Real = 1.f;
            V[17].Real = 1000.f;
            V[18].Real = 1000.f;
            V[28].Int = 0;
            V[0].Real = 0.5f;
            V[1].Real = -0.5f;
            V[25].Real = 0.f;
            V[3].Real = 1000.f;
            V[4].Real = 1000.f;
            return;
        case 2:
            V[0].Real = 1.5707964f;
            V[1].Real = -1.5707964f;
            break;
        case 4:
            V[0].Real = 1.5707964f;
            V[3].Real = 1000.f;
            return;
        case 6:
            V[17].Real = 1000.f;
            V[18].Real = 1000.f;
            V[19].Real = 1000.f;
            V[20].Real = 0.f;
            V[21].Real = 0.f;
            V[22].Real = 0.f;
            return;
        case 7:
            V[0].Real = 0.5f;
            V[1].Real = -0.5f;
            break;
        case 8:
            V[26].Int = 2;
            V[0].Real = 0.7853982f;
            V[1].Real = 0.7853982f;
            V[27].Int = 2;
            V[2].Real = 1.5707964f;
            V[10].Real = 1.f;
            V[9].Real = 1.f;
            V[3].Real = 1000.f;
            V[4].Real = 1000.f;
            return;
        case 9:
            V[28].Int = 0;
            V[3].Real = 1000.f;
            V[9].Real = 1.f;
            return;
        case 10:
            V[5].Real = 1000.f;
            V[6].Real = 1000.f;
            V[7].Real = 1000.f;
            V[8].Real = 1000.f;
            V[11].Real = 1.f;
            V[12].Real = 1.f;
            V[13].Real = 1.f;
            V[14].Real = 1.f;
            V[10].Real = 1.f;
            V[9].Real = 1.f;
            V[3].Real = 1000.f;
            V[4].Real = 1000.f;
            return;
        default:
            return;
        }
        // hinges and prismatic joints
        V[15].Int = 0;
        V[16].Int = 0;
        V[23].Real = 1.f;
        V[17].Real = 1000.f;
        V[3].Real = 1000.f;
        V[4].Real = 1000.f;
    }
}

// 0x00191d70
MeFJoint* MeFJointCreate(const char* Name, INT Type)
{
    MeFJoint* Joint = static_cast<MeFJoint*>(MeMemory.CreateZeroed(sizeof(MeFJoint)));
    MeStrSet(&Joint->Name, Name);
    Joint->Type = Type;
    Joint->Parts[1] = nullptr;
    Joint->Parts[0] = nullptr;
    Joint->Owner = nullptr;
    memset(Joint->Pos, 0, sizeof(Joint->Pos));
    memset(Joint->PrimaryAxis, 0, sizeof(Joint->PrimaryAxis));
    memset(Joint->OrthogonalAxis, 0, sizeof(Joint->OrthogonalAxis));
    Joint->PrimaryAxis[0][0] = 1.f;
    Joint->PrimaryAxis[1][0] = 1.f;
    Joint->OrthogonalAxis[0][2] = 1.f;
    Joint->OrthogonalAxis[1][2] = 1.f;
    ResetJoint(Joint);
    return Joint;
}

// 0x00191930. the real properties: 0 to 14 and 17 to 25.
void MeFJointSetReal(MeFJoint* Joint, INT Property, FLOAT Value)
{
    if ((Property >= 0 && Property <= 14) || (Property >= 17 && Property <= 25))
        Joint->Values[Property].Real = Value;
}

// 0x001918e0. the flags 15, 16 and 28.
void MeFJointSetInt(MeFJoint* Joint, INT Property, INT Value)
{
    if (Property == 15 || Property == 16 || Property == 28)
        Joint->Values[Property].Int = Value;
}

// 0x001918b0. the types 26 and 27.
void MeFJointSetEnum(MeFJoint* Joint, INT Property, INT Value)
{
    if (Property == 26 || Property == 27)
        Joint->Values[Property].Int = Value;
}

// 0x00191820
void MeFJointSetPosition(MeFJoint* Joint, INT Body, FLOAT X, FLOAT Y, FLOAT Z)
{
    Joint->Pos[Body][0] = X;
    Joint->Pos[Body][1] = Y;
    Joint->Pos[Body][2] = Z;
}

// 0x00191850
void MeFJointSetPrimaryAxis(MeFJoint* Joint, INT Body, FLOAT X, FLOAT Y, FLOAT Z)
{
    Joint->PrimaryAxis[Body][0] = X;
    Joint->PrimaryAxis[Body][1] = Y;
    Joint->PrimaryAxis[Body][2] = Z;
}

// 0x00191880
void MeFJointSetOrthogonalAxis(MeFJoint* Joint, INT Body, FLOAT X, FLOAT Y, FLOAT Z)
{
    Joint->OrthogonalAxis[Body][0] = X;
    Joint->OrthogonalAxis[Body][1] = Y;
    Joint->OrthogonalAxis[Body][2] = Z;
}

// 0x001917f0
void MeFJointSetPart(MeFJoint* Joint, const char* Part, INT Body)
{
    if (Body || Part)
        MeStrSet(&Joint->Parts[Body], Part);
}

// 0x001913d0
MeFPart* MeFJointGetPart(MeFJoint* Joint, INT Body)
{
    if (!Joint->Owner || !Joint->Parts[Body])
        return nullptr;
    return MeFAssetLookupPart(Joint->Owner, Joint->Parts[Body]);
}

// 0x00188160, through the shared getter
INT MeFJointGetType(MeFJoint* Joint)
{
    return Joint->Type;
}

// 0x00191400
void MeFJointGetPosition(MeFJoint* Joint, INT Body, FLOAT* Pos)
{
    for (INT i = 0; i < 3; i++)
        Pos[i] = Joint->Pos[Body][i];
}

// 0x00191430
void MeFJointGetPrimaryAxis(MeFJoint* Joint, INT Body, FLOAT* Axis)
{
    for (INT i = 0; i < 3; i++)
        Axis[i] = Joint->PrimaryAxis[Body][i];
}

// 0x00191460
void MeFJointGetOrthogonalAxis(MeFJoint* Joint, INT Body, FLOAT* Axis)
{
    for (INT i = 0; i < 3; i++)
        Axis[i] = Joint->OrthogonalAxis[Body][i];
}

// 0x00191540. properties 0 to 14 and 17 to 25 are reals.
UBOOL MeFJointGetReal(MeFJoint* Joint, INT Property, FLOAT* Value)
{
    if ((Property < 0 || Property > 14) && (Property < 17 || Property > 25))
        return 0;
    *Value = Joint->Values[Property].Real;
    return 1;
}

// 0x001914e0. properties 15, 16 and 28 are ints.
UBOOL MeFJointGetInt(MeFJoint* Joint, INT Property, INT* Value)
{
    if (Property != 15 && Property != 16 && Property != 28)
        return 0;
    *Value = Joint->Values[Property].Int;
    return 1;
}

// 0x00191490. properties 26 and 27 are enums.
UBOOL MeFJointGetEnum(MeFJoint* Joint, INT Property, INT* Value)
{
    if (Property != 26 && Property != 27)
        return 0;
    *Value = Joint->Values[Property].Int;
    return 1;
}
