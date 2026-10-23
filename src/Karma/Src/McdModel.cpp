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

// 0x00189120
INT McdGeometryGetReferenceCount(McdGeometry* Geometry)
{
    return static_cast<DWORD>(Geometry->TypeId) >> 8;
}

// 0x00189150. the geometry's type frees it.
void McdGeometryDestroy(McdGeometry* Geometry)
{
    if (static_cast<DWORD>(Geometry->TypeId) & 0xffffff00)
        MeWarning(0, "Destroying geometry with non-zero reference count!");
    Geometry->Framework->Types[Geometry->TypeId & 0xff].Destroy(Geometry);
}

// 0x00189750
McdAggregate* McdAggregateCreate(McdFramework* Framework, INT MaxElements)
{
    McdAggregate* Aggregate = static_cast<McdAggregate*>(MeMemory.CreateAligned(sizeof(McdAggregate), 0x10));
    if (!Aggregate)
        return nullptr;
    McdGeometryInit(&Aggregate->Base, Framework, 8);
    Aggregate->Elements = static_cast<McdAggregateElement*>(MeMemory.Create(MaxElements * static_cast<INT>(sizeof(McdAggregateElement))));
    Aggregate->ElementCount = MaxElements;
    Aggregate->UsedCount = 0;
    for (INT i = 0; i < MaxElements; i++)
        Aggregate->Elements[i].Geometry = nullptr;
    return Aggregate;
}

// 0x001897c0. the first free slot, or -1 when full.
INT McdAggregateAddElement(McdAggregate* Aggregate, McdGeometry* Geometry, const FLOAT* TM)
{
    INT Index = 0;
    while (Index < Aggregate->ElementCount && Aggregate->Elements[Index].Geometry)
        Index++;
    if (Index >= Aggregate->ElementCount)
        return -1;
    McdAggregateElement& Element = Aggregate->Elements[Index];
    for (INT i = 0; i < 16; i++)
        Element.TM[i] = TM[i];
    Element.Geometry = Geometry;
    Aggregate->UsedCount++;
    McdGeometryIncrementReferenceCount(Geometry);
    return Index;
}

// 0x00189880
void McdAggregateRemoveElement(McdAggregate* Aggregate, INT Index)
{
    McdGeometryDecrementReferenceCount(Aggregate->Elements[Index].Geometry);
    Aggregate->Elements[Index].Geometry = nullptr;
    Aggregate->UsedCount--;
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
    Model->Body = nullptr;
    Model->Unknown28 = 0;
    Model->Space = nullptr;
    Model->SpaceIndex = -1;
    Model->UserData = nullptr;
    Model->LinearVelocity = nullptr;
    Model->AngularVelocity = nullptr;
    Model->Unknown2C = nullptr;
    Model->Unknown20 = 0;
    Model->Unknown38 = 0;
    Model->Flags = 0;
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

// 0x00188130, through the shared getter at 0x00184cf0
FLOAT* McdModelGetTransformPtr(McdModel* Model)
{
    return Model->Instance.TM;
}

// 0x001881e0, through the shared getter at 0x00165900
McdGeometry* McdModelGetGeometry(McdModel* Model)
{
    return Model->Instance.Geometry;
}

// 0x00188700. the dynamics body the model follows, if any.
MdtBody* McdModelGetBody(McdModel* Model)
{
    return Model->Body;
}

namespace
{
    // 0x00188710. moves the old body's constraints that hold this model over to the new body.
    void McdModelMoveConstraint(MdtConstraint* Constraint, void* Context)
    {
        NOT_DECOMPILED(0x00188710);
    }
}

// 0x00188820. the model follows the body: its transform and velocities point into it.
void McdModelSetBody(McdModel* Model, MdtBody* Body)
{
    MdtBody* Old = Model->Body;
    if (Body == Old && (!Body || Body->Model == Model))
        return;
    if (Old)
    {
        struct
        {
            MdtBody*  Old;
            MdtBody*  New;
            McdModel* Model;
            void*     Space;
        } Context = { Old, Body, Model, McdModelGetSpace(Model) };
        MdtBodyForAllConstraints(Old, McdModelMoveConstraint, &Context);
        if (Old->Model == Model)
            Old->Model = nullptr;
    }
    if (!Body)
    {
        if (Old)
        {
            if (McdModelGetTransformPtr(Model) == MdtBodyGetTransformPtr(Old))
                McdModelSetTransformPtr(Model, nullptr);
            if (McdModelGetLinearVelocityPtr(Model) == MdtBodyGetLinearVelocityPtr(Old))
                McdModelSetLinearVelocityPtr(Model, nullptr);
            if (McdModelGetAngularVelocityPtr(Model) == MdtBodyGetAngularVelocityPtr(Old))
                McdModelSetAngularVelocityPtr(Model, nullptr);
        }
        if (McdModelGetSpace(Model))
            NOT_DECOMPILED(0x001b1680);
    }
    else
    {
        Body->Model = Model;
        McdModelSetTransformPtr(Model, MdtBodyGetTransformPtr(Body));
        McdModelSetLinearVelocityPtr(Model, MdtBodyGetLinearVelocityPtr(Body));
        McdModelSetAngularVelocityPtr(Model, MdtBodyGetAngularVelocityPtr(Body));
        // a model in a space is enabled there with its body
        if (McdModelGetSpace(Model))
            NOT_DECOMPILED(MdtBodyIsEnabled(Body) ? 0x001b16a0 : 0x001b1680);
    }
    Model->Body = Body;
}

// 0x00188080
void McdModelDestroy(McdModel* Model)
{
    if (Model->Flags & 1)
    {
        MeMemory.DestroyAligned(Model->RelTM);
        MeMemory.DestroyAligned(Model->Instance.TM);
        Model->Flags &= ~1;
    }
    McdGeometryInstanceSetGeometry(&Model->Instance, nullptr);
    McdFramework* Framework = Model->Framework;
    if (Framework->Models == Model)
        Framework->Models = Model->Next == Model ? nullptr : Model->Next;
    Model->Next->Prev = Model->Prev;
    Model->Prev->Next = Model->Next;
    MePools.PutStruct(&Framework->ModelPool, Model);
    Framework->ModelCount--;
}

// 0x001b3730, through the shared getter
void* McdModelGetSpace(McdModel* Model)
{
    return Model->Space;
}

// 0x00188160
FLOAT* McdModelGetLinearVelocityPtr(McdModel* Model)
{
    return Model->LinearVelocity;
}

// 0x00188150, which the xbox shares with MeHashSetKeyFreeFunction
void McdModelSetLinearVelocityPtr(McdModel* Model, FLOAT* V)
{
    Model->LinearVelocity = V;
}

// 0x001b3690, through the shared getter
FLOAT* McdModelGetAngularVelocityPtr(McdModel* Model)
{
    return Model->AngularVelocity;
}

// 0x00188170
void McdModelSetAngularVelocityPtr(McdModel* Model, FLOAT* V)
{
    Model->AngularVelocity = V;
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
