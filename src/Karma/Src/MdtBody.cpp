#include <cfloat>

#include "Core.h"
#include "Karma.h"

namespace
{
    const void* DictKey(INT Key)
    {
        return reinterpret_cast<const void*>(static_cast<PTRINT>(Key));
    }

    FLOAT Clamp(FLOAT Value, FLOAT Min, FLOAT Max)
    {
        const FLOAT Capped = Value < Max ? Value : Max;
        if (Capped < Min)
            return Min;
        return Max <= Value ? Max : Value;
    }

    // 0x00185230. the center of mass follows the body's transform.
    void MdtBodyUpdateComTM(MdtBody* Body)
    {
        for (INT i = 0; i < 16; i++)
            Body->ComTM[i] = Body->TM[i];
        if (Body->HasComOffset)
        {
            const FLOAT* TM = Body->TM;
            const FLOAT* Offset = Body->ComOffset;
            Body->ComTM[12] = TM[0] * Offset[0] + TM[4] * Offset[1] + TM[8] * Offset[2] + TM[12];
            Body->ComTM[13] = TM[5] * Offset[1] + TM[1] * Offset[0] + TM[9] * Offset[2] + TM[13];
            Body->ComTM[14] = TM[6] * Offset[1] + TM[2] * Offset[0] + TM[10] * Offset[2] + TM[14];
        }
        MeQuaternionFromTM(Body->Quaternion, Body->TM);
    }

    // 0x00185e70. the inverse of the symmetric inertia tensor.
    void MdtBodyUpdateInvInertia(MdtBody* Body)
    {
        const FLOAT (*I)[4] = Body->Inertia;
        const FLOAT A = I[2][0] * I[2][1] - I[2][2] * I[1][0];
        const FLOAT B = I[1][0] * I[2][1] - I[2][0] * I[1][1];
        const FLOAT InvDet = 1.f / (A * I[1][0] + B * I[2][0] + (I[2][2] * I[1][1] - I[2][1] * I[2][1]) * I[0][0]);
        FLOAT (*Inv)[4] = Body->InvInertia;
        Inv[0][0] = (I[2][2] * I[1][1] - I[1][2] * I[2][1]) * InvDet;
        Inv[0][1] = A * InvDet;
        Inv[0][2] = B * InvDet;
        Inv[1][1] = (I[2][2] * I[0][0] - I[2][0] * I[2][0]) * InvDet;
        Inv[1][2] = (I[2][0] * I[1][0] - I[0][0] * I[2][1]) * InvDet;
        Inv[2][2] = (I[1][1] * I[0][0] - I[1][0] * I[1][0]) * InvDet;
        Inv[1][0] = Inv[0][1];
        Inv[2][0] = Inv[0][2];
        Inv[2][1] = Inv[1][2];
    }
}

// 0x001868b0
MdtBody* MdtBodyCreate(MdtWorld* World)
{
    MdtBody* Body = static_cast<MdtBody*>(MePools.GetStruct(&World->BodyPool));
    if (!Body)
        return nullptr;
    Body->World = World;
    Body->Tag = 100;
    Body->Unknown4 = 0xf0;
    Body->Key = 0;
    Body->Model = nullptr;
    Body->EnabledFlags = 0;
    MdtBodyReset(Body);
    MeDictNodeInit(&Body->WorldNode, nullptr);
    Body->WorldNode.Data = Body;
    MeDictNodeInit(&Body->EnabledNode, nullptr);
    Body->EnabledNode.Data = Body;
    MeDictInsert(&World->Bodies, &Body->WorldNode, DictKey(Body->Key));
    MeDictInit(&Body->Constraints, 1000000, MeDictCompareKeys);
    MeDictAllowDupes(&Body->Constraints);
    World->BodyCount++;
    return Body;
}

// 0x001866b0. a disabled unit body: a sphere's inertia, at the origin, not moving.
void MdtBodyReset(MdtBody* Body)
{
    MdtWorld* World = Body->World;
    if (Body->EnabledFlags & 1)
        MdtBodyDisable(Body);
    Body->Flags = 0;
    Body->InvMass = 1.f / World->MassScale;
    for (INT i = 0; i < 4; i++)
    {
        Body->Unknown10[i] = 0.f;
        Body->Unknown20[i] = 0.f;
        for (INT Row = 0; Row < 3; Row++)
        {
            Body->InvInertia[Row][i] = 0.f;
            Body->Inertia[Row][i] = 0.f;
        }
        Body->LinearVelocity[i] = 0.f;
        Body->AngularVelocity[i] = 0.f;
        Body->Quaternion[i] = 0.f;
        Body->UnknownC0[i] = 0.f;
        Body->UnknownD0[i] = 0.f;
        Body->FastSpinAxis[i] = 0.f;
        Body->Unknown170[i] = 0.f;
        Body->Unknown180[i] = 0.f;
    }
    Body->Unknown1A0 = 0;
    Body->Quaternion[0] = 1.f;
    const FLOAT Inertia = World->MassScale * 0.4f;
    Body->Inertia[2][2] = Inertia;
    Body->Inertia[1][1] = Inertia;
    Body->Inertia[0][0] = Inertia;
    Body->FastSpinAxis[1] = 1.f;
    Body->FastSpinAxis[0] = 0.f;
    Body->FastSpinAxis[2] = 0.f;
    const FLOAT InvInertia = 1.f / (World->MassScale * 0.4f);
    Body->InvInertia[2][2] = InvInertia;
    Body->InvInertia[1][1] = InvInertia;
    Body->InvInertia[0][0] = InvInertia;
    for (INT i = 0; i < 16; i++)
    {
        Body->ComTM[i] = i % 5 == 0 ? 1.f : 0.f;
        Body->TM[i] = i % 5 == 0 ? 1.f : 0.f;
    }
    Body->ComOffset[0] = 0.f;
    Body->ComOffset[1] = 0.f;
    Body->ComOffset[2] = 0.f;
    Body->HasComOffset = 0;
    Body->Unknown1E4 = -1;
    Body->Unknown1E8 = -1;
    Body->Unknown1E0 = -1;
    Body->Mass = World->MassScale;
    Body->EnabledFlags = 0;
    Body->LinearDamping = 0.f;
    Body->AngularDamping = 0.f;
    Body->Unknown230 = 0;
    Body->UserData = nullptr;
    Body->Unknown238 = FLT_MAX;
}

// 0x00186610. destroys the body's constraints too.
void MdtBodyDestroy(MdtBody* Body)
{
    MdtWorld* World = Body->World;
    MeDictDelete(&World->Bodies, &Body->WorldNode);
    for (MeDictNode* Node = MeDictFirst(&Body->Constraints); Node;)
    {
        MdtConstraint* Constraint = static_cast<MdtConstraint*>(Node->Data);
        Node = MeDictNext(&Body->Constraints, Node);
        MdtConstraintDisable(Constraint);
        MdtConstraintDestroy(Constraint);
    }
    if (Body->EnabledFlags & 1)
        MdtBodyDisable(Body);
    Body->Tag = -1;
    World->BodyCount--;
    MePools.PutStruct(&World->BodyPool, Body);
}

// 0x00186200
void MdtBodyEnable(MdtBody* Body)
{
    if (!(Body->EnabledFlags & 1))
    {
        MeDictInsert(&Body->World->EnabledBodies, &Body->EnabledNode, DictKey(Body->Key));
        Body->EnabledFlags |= 1;
        Body->World->EnabledBodyCount++;
        if (Body->World->BodyEnableCB)
            Body->World->BodyEnableCB(Body);
    }
    Body->Unknown230 = 0;
    Body->EnabledFlags &= ~4;
}

// 0x00186190
void MdtBodyDisable(MdtBody* Body)
{
    if (!(Body->EnabledFlags & 1))
        return;
    MeDictDelete(&Body->World->EnabledBodies, &Body->EnabledNode);
    Body->EnabledFlags &= ~5;
    Body->Unknown230 = 0;
    Body->Unknown1E0 = -1;
    Body->World->EnabledBodyCount--;
    if (Body->World->BodyDisableCB)
        Body->World->BodyDisableCB(Body);
}

// 0x00186280
UBOOL MdtBodyIsEnabled(MdtBody* Body)
{
    return Body->EnabledFlags & 1;
}

// 0x00185c60
void MdtBodySetTransform(MdtBody* Body, const FLOAT* TM)
{
    for (INT i = 0; i < 16; i++)
        Body->TM[i] = TM[i];
    MdtBodyUpdateComTM(Body);
}

// 0x001859d0
FLOAT* MdtBodyGetTransformPtr(MdtBody* Body)
{
    return Body->TM;
}

// 0x00185ac0
FLOAT* MdtBodyGetLinearVelocityPtr(MdtBody* Body)
{
    return Body->LinearVelocity;
}

// 0x00185b00
FLOAT* MdtBodyGetAngularVelocityPtr(MdtBody* Body)
{
    return Body->AngularVelocity;
}

// 0x0007f0c6, an LTCG copy of the library's. kept within a factor of 100 of the world's mass unit.
void MdtBodySetMass(MdtBody* Body, FLOAT Mass)
{
    Body->Mass = Mass;
    const FLOAT Scale = Body->World->MassScale;
    const FLOAT Clamped = Clamp(Mass, Scale * 0.01f, Scale * 100.f);
    Body->Mass = Clamped;
    Body->InvMass = 1.f / Clamped;
}

// 0x00185f60, with an LTCG copy at 0x0007eeb5. the diagonal is kept within a factor of 100 of the world's units.
void MdtBodySetInertiaTensor(MdtBody* Body, const FLOAT* Inertia)
{
    MdtWorld* World = Body->World;
    const FLOAT Scale = World->MassScale * World->LengthScale * World->LengthScale;
    const FLOAT Max = Scale * 100.f;
    const FLOAT Min = Scale * 0.01f;
    Body->Inertia[0][0] = Clamp(Inertia[0], Min, Max);
    Body->Inertia[2][0] = Inertia[6];
    Body->Inertia[1][0] = Inertia[3];
    Body->Inertia[1][1] = Clamp(Inertia[4], Min, Max);
    Body->Inertia[0][1] = Inertia[1];
    Body->Inertia[0][2] = Inertia[2];
    Body->Inertia[2][1] = Inertia[7];
    Body->Inertia[2][2] = Clamp(Inertia[8], Min, Max);
    Body->Inertia[1][2] = Inertia[5];
    MdtBodyUpdateInvInertia(Body);
}

// 0x00185a10. as nine floats, row by row.
void MdtBodyGetInertiaTensor(MdtBody* Body, FLOAT* Inertia)
{
    for (INT Row = 0; Row < 3; Row++)
        for (INT Col = 0; Col < 3; Col++)
            Inertia[Row * 3 + Col] = Body->Inertia[Row][Col];
}

// 0x00186110. ignored once the body has constraints.
void MdtBodySetCenterOfMassRelativePosition(MdtBody* Body, const FLOAT* Pos)
{
    if (Body->Constraints.NodeCount)
        return;
    Body->ComOffset[0] = Pos[0];
    Body->ComOffset[1] = Pos[1];
    Body->ComOffset[2] = Pos[2];
    Body->HasComOffset = Pos[0] != 0.f || Pos[1] != 0.f || Pos[2] != 0.f;
    MdtBodyUpdateComTM(Body);
}

// 0x00185df0
void MdtBodySetLinearVelocityDamping(MdtBody* Body, FLOAT Damping)
{
    Body->LinearDamping = Damping;
}

// 0x00185e10
void MdtBodySetAngularVelocityDamping(MdtBody* Body, FLOAT Damping)
{
    Body->AngularDamping = Damping;
}

// 0x00185e30
void MdtBodySetFastSpinAxis(MdtBody* Body, FLOAT X, FLOAT Y, FLOAT Z)
{
    Body->Flags |= 1;
    Body->FastSpinAxis[0] = X;
    Body->FastSpinAxis[1] = Y;
    Body->FastSpinAxis[2] = Z;
    MeVector3Normalize(Body->FastSpinAxis);
}

// 0x001865d0
void MdtBodySetFlag4(MdtBody* Body)
{
    Body->Flags |= 4;
}

// 0x001865f0
void MdtBodySetFlag8(MdtBody* Body)
{
    Body->Flags |= 8;
}

// 0x00186600
void MdtBodyClearFlag8(MdtBody* Body)
{
    Body->Flags &= ~8;
}

// 0x00186300. Fn may remove the constraint it is given.
void MdtBodyForAllConstraints(MdtBody* Body, void (*Fn)(MdtConstraint* Constraint, void* Context), void* Context)
{
    for (MeDictNode* Node = MeDictFirst(&Body->Constraints); Node;)
    {
        MdtConstraint* Constraint = static_cast<MdtConstraint*>(Node->Data);
        Node = MeDictNext(&Body->Constraints, Node);
        Fn(Constraint, Context);
    }
}

// 0x00184eb0
void MdtConvertVector(MdtBody* From, const FLOAT* V, MdtBody* To, FLOAT* Out)
{
    FLOAT W[3];
    if (From)
        MeMatrix4TMRotate(W, From->TM, V);
    else
    {
        W[0] = V[0];
        W[1] = V[1];
        W[2] = V[2];
    }
    if (!To)
    {
        Out[0] = W[0];
        Out[2] = W[2];
        Out[1] = W[1];
        return;
    }
    const FLOAT* TM = To->TM;
    Out[0] = W[0] * TM[0] + W[1] * TM[1] + W[2] * TM[2];
    Out[1] = W[0] * TM[4] + W[1] * TM[5] + W[2] * TM[6];
    Out[2] = W[0] * TM[8] + W[1] * TM[9] + W[2] * TM[10];
}

namespace
{
    // 0x00184fd0. moves a point between frames, ignoring the centers of mass.
    void MdtConvertPoint(MdtBody* From, const FLOAT* V, MdtBody* To, FLOAT* Out)
    {
        FLOAT W[3];
        if (From)
            MeMatrix4TMTransform(W, From->TM, V);
        else
        {
            W[0] = V[0];
            W[1] = V[1];
            W[2] = V[2];
        }
        if (To)
            MeMatrix4TMInverseTransform(Out, To->TM, W);
        else
        {
            Out[0] = W[0];
            Out[1] = W[1];
            Out[2] = W[2];
        }
    }
}

// 0x00185070. positions in a body's frame are relative to its center of mass.
void MdtConvertPositionVector(MdtBody* From, const FLOAT* V, MdtBody* To, FLOAT* Out)
{
    FLOAT Local[3];
    if (From)
    {
        Local[0] = From->ComOffset[0] + V[0];
        Local[1] = From->ComOffset[1] + V[1];
        Local[2] = From->ComOffset[2] + V[2];
        V = Local;
    }
    MdtConvertPoint(From, V, To, Out);
    if (To)
    {
        Out[0] -= To->ComOffset[0];
        Out[1] -= To->ComOffset[1];
        Out[2] -= To->ComOffset[2];
    }
}
