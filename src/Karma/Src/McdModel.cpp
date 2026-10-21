#include <cfloat>

#include "Core.h"
#include "Karma.h"

// ---------------------------------------------------------------------------------------------- math

// 0x0018c8f0
void MeMatrix4TMCompose(FLOAT* Out, const FLOAT* A, const FLOAT* B)
{
    for (INT Row = 0; Row < 4; Row++)
    {
        for (INT Col = 0; Col < 3; Col++)
        {
            Out[Row * 4 + Col] = A[Row * 4] * B[Col] + A[Row * 4 + 1] * B[4 + Col] + A[Row * 4 + 2] * B[8 + Col];
            if (Row == 3)
                Out[Row * 4 + Col] += B[12 + Col];
        }
        Out[Row * 4 + 3] = Row == 3 ? 1.f : 0.f;
    }
}

// ---------------------------------------------------------------------------------------------- geometry

// 0x00189130. the reference count sits above the type in the low byte.
void McdGeometryIncrementReferenceCount(McdGeometry* Geometry)
{
    Geometry->TypeId += 0x100;
}

// 0x00189140
void McdGeometryDecrementReferenceCount(McdGeometry* Geometry)
{
    Geometry->TypeId -= 0x100;
}

// 0x001898b0
INT McdAggregateGetElementCount(McdAggregate* Aggregate)
{
    return Aggregate->ElementCount;
}

// 0x001898c0
McdGeometry* McdAggregateGetElementGeometry(McdAggregate* Aggregate, INT Index)
{
    if (Index >= Aggregate->ElementCount)
        return nullptr;
    return Aggregate->Elements[Index].Geometry;
}

// ---------------------------------------------------------------------------------------------- instances

namespace
{
    // 0x001b14c0 (name invented)
    McdGeometryInstance* McdGeometryInstanceCreate(McdFramework* Framework)
    {
        McdGeometryInstance* Instance = static_cast<McdGeometryInstance*>(MePools.GetStruct(&Framework->InstancePool));
        McdGeometryInstanceReset(Instance);
        return Instance;
    }

    // 0x001b1340. frees an aggregate part's instance and those below it.
    void McdGeometryInstanceDestroyChild(McdFramework* Framework, McdGeometryInstance* Instance)
    {
        for (McdGeometryInstance* Child = Instance->Child; Child; )
        {
            McdGeometryInstance* Next = Child->Next;
            McdGeometryInstanceDestroyChild(Framework, Child);
            Child = Next;
        }
        if (Instance->Geometry)
            McdGeometryDecrementReferenceCount(Instance->Geometry);
        MePools.PutStruct(&Framework->InstancePool, Instance);
    }
}

// 0x001b1300
void McdGeometryInstanceReset(McdGeometryInstance* Instance)
{
    Instance->Unknown20 = 0;
    Instance->TM = nullptr;
    Instance->Geometry = nullptr;
    Instance->Min[0] = Instance->Min[1] = Instance->Min[2] = FLT_MAX;
    Instance->Max[0] = Instance->Max[1] = Instance->Max[2] = -FLT_MAX;
    Instance->Next = nullptr;
    Instance->Prev = nullptr;
    Instance->Parent = nullptr;
    Instance->Child = nullptr;
}

// 0x001b1500. an aggregate gets a child instance for each of its parts.
void McdGeometryInstanceSetGeometry(McdGeometryInstance* Instance, McdGeometry* Geometry)
{
    if (Instance->Child)
    {
        McdFramework* Framework = Instance->Geometry->Framework;
        for (McdGeometryInstance* Child = Instance->Child; Child; )
        {
            McdGeometryInstance* Next = Child->Next;
            for (McdGeometryInstance* Below = Child->Child; Below; )
            {
                McdGeometryInstance* BelowNext = Below->Next;
                McdGeometryInstanceDestroyChild(Framework, Below);
                Below = BelowNext;
            }
            if (Child->Geometry)
                McdGeometryDecrementReferenceCount(Child->Geometry);
            MePools.PutStruct(&Framework->InstancePool, Child);
            Child = Next;
        }
        Instance->Child = nullptr;
    }
    if (Instance->Geometry)
        McdGeometryDecrementReferenceCount(Instance->Geometry);
    Instance->Geometry = Geometry;
    if (!Geometry)
        return;
    McdGeometryIncrementReferenceCount(Geometry);
    if ((Geometry->TypeId & 0xff) != 8)
        return;
    McdAggregate* Aggregate = reinterpret_cast<McdAggregate*>(Geometry);
    const INT Count = McdAggregateGetElementCount(Aggregate);
    if (!Count)
        return;
    McdFramework* Framework = Geometry->Framework;
    McdGeometryInstance* Last = McdGeometryInstanceCreate(Framework);
    Last->Parent = Instance;
    Instance->Child = Last;
    for (INT i = 1; i < Count; i++)
    {
        McdGeometryInstance* Child = McdGeometryInstanceCreate(Framework);
        Child->Prev = Last;
        Child->Parent = Instance;
        Last->Next = Child;
        Last = Child;
    }
    McdGeometryInstance* Child = Instance->Child;
    for (INT i = 0; i < Count; i++)
    {
        McdGeometryInstanceSetGeometry(Child, McdAggregateGetElementGeometry(Aggregate, i));
        Child = Child->Next;
    }
}

// 0x001b1420
void McdGeometryInstanceSetTransformPtr(McdGeometryInstance* Instance, FLOAT* TM)
{
    Instance->TM = TM;
}

// 0x001b1380. asks the geometry's type for the box.
void McdGeometryInstanceUpdateAABB(McdGeometryInstance* Instance, void* Unknown1, void* Unknown2)
{
    McdGeometry* Geometry = Instance->Geometry;
    Geometry->Framework->Types[Geometry->TypeId & 0xff].GetAABB(Instance, Unknown1, Unknown2);
}

// 0x001b13e0
void McdGeometryInstanceGetAABB(McdGeometryInstance* Instance, FLOAT* Min, FLOAT* Max)
{
    for (INT i = 0; i < 3; i++)
        Min[i] = Instance->Min[i];
    for (INT i = 0; i < 3; i++)
        Max[i] = Instance->Max[i];
}

// ---------------------------------------------------------------------------------------------- models

// 0x00188210
McdModel* McdModelCreate(McdGeometry* Geometry)
{
    McdFramework* Framework = Geometry->Framework;
    McdModel* Model = static_cast<McdModel*>(MePools.GetStruct(&Framework->ModelPool));
    if (!Model)
        return nullptr;
    Framework->ModelCount++;
    Model->RelTM = nullptr;
    Model->BodyTM = nullptr;
    Model->Unknown24 = 0;
    Model->Unknown28 = 0;
    Model->UnknownC = 0;
    Model->Unknown1C = -1;
    Model->UserData = nullptr;
    Model->Unknown10 = 0;
    Model->Unknown14 = 0;
    Model->Unknown2C = nullptr;
    Model->Unknown20 = 0;
    Model->Unknown38 = 0;
    Model->Unknown3C = 0;
    McdGeometryInstanceReset(&Model->Instance);
    Model->Padding = Framework->DefaultPadding;
    Model->Framework = Framework;
    McdGeometryInstanceSetGeometry(&Model->Instance, Geometry);
    // new models go in front of the list
    if (!Framework->Models)
    {
        Model->Prev = Model;
        Model->Next = Model;
    }
    else
    {
        McdModel* Last = Framework->Models->Prev;
        Model->Prev = Last;
        Model->Next = Framework->Models;
        Last->Next = Model;
        Model->Next->Prev = Model;
    }
    Framework->Models = Model;
    return Model;
}

// 0x00188140
void McdModelSetTransformPtr(McdModel* Model, FLOAT* TM)
{
    McdGeometryInstanceSetTransformPtr(&Model->Instance, TM);
}

// 0x00188180
void McdModelSetUserData(McdModel* Model, void* UserData)
{
    Model->UserData = UserData;
}

// 0x00188120
void McdModelGetAABB(McdModel* Model, FLOAT* Min, FLOAT* Max)
{
    McdGeometryInstanceGetAABB(&Model->Instance, Min, Max);
}

// 0x001882c0. moves the model to its body and recomputes its padded box.
void McdModelUpdate(McdModel* Model)
{
    if (Model->Unknown2C)
        Model->Unknown2C(Model);
    if (Model->RelTM)
        MeMatrix4TMCompose(Model->Instance.TM, Model->RelTM, Model->BodyTM);
    McdGeometryInstanceUpdateAABB(&Model->Instance, nullptr, nullptr);
    for (INT i = 0; i < 3; i++)
    {
        Model->Instance.Min[i] -= Model->Padding;
        Model->Instance.Max[i] += Model->Padding;
    }
}

// ---------------------------------------------------------------------------------------------- triangle lists

namespace
{
    // 0x00196990 (name invented)
    void McdTriangleListSetBox(McdTriangleList* List, const FLOAT* Min, const FLOAT* Max)
    {
        for (INT i = 0; i < 3; i++)
            List->Center[i] = (Min[i] + Max[i]) * 0.5f;
        for (INT i = 0; i < 3; i++)
            List->Extent[i] = (Max[i] - Min[i]) * 0.5f;
    }
}

// 0x00196e40
McdTriangleList* McdTriangleListCreate(McdFramework* Framework, const FLOAT* Min, const FLOAT* Max, INT MaxTriangles,
    McdTriangleListGenerator Generator)
{
    if (!Generator)
        return nullptr;
    McdTriangleList* List = static_cast<McdTriangleList*>(MeMemory.CreateAligned(sizeof(McdTriangleList), 0x10));
    McdGeometryInit(&List->Base, Framework, 6);
    McdTriangleListSetBox(List, Min, Max);
    List->MaxTriangles = MaxTriangles;
    List->Generator = Generator;
    return List;
}

// 0x00196dc0
void McdTriangleListSetUserData(McdTriangleList* List, void* UserData)
{
    List->UserData = UserData;
}
