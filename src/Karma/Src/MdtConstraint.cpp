#include <cfloat>
#include <cmath>

#include "Core.h"
#include "Karma.h"

namespace
{
    const void* DictKey(INT Key)
    {
        return reinterpret_cast<const void*>(static_cast<PTRINT>(Key));
    }

    // 0x001a8050
    void MdtBSJointAddRows(void* Unknown1, MdtConstraint* Constraint, void* Unknown2)
    {
        NOT_DECOMPILED(0x001a8050);
    }

    // 0x001a8c30
    void MdtHingeAddRows(void* Unknown1, MdtConstraint* Constraint, void* Unknown2)
    {
        NOT_DECOMPILED(0x001a8c30);
    }

    // 0x001ac5d0
    void MdtSkeletalAddRows(void* Unknown1, MdtConstraint* Constraint, void* Unknown2)
    {
        NOT_DECOMPILED(0x001ac5d0);
    }

    // 0x00165fe0. bodies only change while the constraint is disabled.
    void MdtConstraintSetBodiesDefault(MdtConstraint* Constraint, MdtBody* Body1, MdtBody* Body2)
    {
        if (Constraint->Flags & 1)
            return;
        Constraint->Body[0] = Body1;
        Constraint->Body[1] = Body2;
    }

    // 0x001657e0. a world axis becomes both frames' primary axis, with any two axes at right angles to it.
    void MdtConstraintSetAxisDefault(MdtConstraint* Constraint, FLOAT X, FLOAT Y, FLOAT Z)
    {
        FLOAT Axis[3] = { X, Y, Z };
        if (fabsf(Z * Z + Y * Y + X * X - 1.f) >= 0.001f)
            MeVector3Normalize(Axis);
        FLOAT Orthogonal[3];
        FLOAT Third[3];
        MeVector3PlaneSpace(Axis, Orthogonal, Third);
        for (INT i = 0; i < 2; i++)
        {
            MdtConvertVector(nullptr, Axis, Constraint->Body[i], Constraint->Frame[i].Primary);
            MdtConvertVector(nullptr, Orthogonal, Constraint->Body[i], Constraint->Frame[i].Orthogonal);
            MdtConvertVector(nullptr, Third, Constraint->Body[i], Constraint->Frame[i].Third);
        }
    }

    // 0x00165640. a constraint from the world's pool, not yet reset.
    MdtConstraint* MdtConstraintCreate(MdtWorld* World)
    {
        MdtConstraint* Constraint = static_cast<MdtConstraint*>(MePools.GetStruct(&World->ConstraintPool));
        if (!Constraint)
            return nullptr;
        Constraint->World = World;
        Constraint->Body[1] = nullptr;
        Constraint->Body[0] = nullptr;
        Constraint->AddRows = nullptr;
        Constraint->MaxRows = 0;
        Constraint->Flags = 0;
        Constraint->Key = 0;
        return Constraint;
    }

    // normalizes P and O when they are off unit length and fills in the third axis
    void MakeAxes(FLOAT* P, FLOAT* O, FLOAT* Third)
    {
        if (fabsf(P[2] * P[2] + P[1] * P[1] + P[0] * P[0] - 1.f) >= 0.001f)
            MeVector3Normalize(P);
        if (fabsf(O[2] * O[2] + O[1] * O[1] + O[0] * O[0] - 1.f) >= 0.001f)
            MeVector3Normalize(O);
        Third[0] = P[1] * O[2] - P[2] * O[1];
        Third[1] = P[2] * O[0] - P[0] * O[2];
        Third[2] = P[0] * O[1] - P[1] * O[0];
    }
}

// 0x00165680. both frames become the identity at the body's center.
void MdtConstraintReset(MdtConstraint* Constraint)
{
    Constraint->UnknownB8 = -1;
    Constraint->UnknownB4 = -1;
    Constraint->Flags = 0;
    Constraint->Unknown50 = 0;
    for (INT i = 0; i < 4; i++)
    {
        Constraint->Unknown68[i] = 0.f;
        Constraint->Unknown78[i] = 0.f;
        Constraint->Unknown88[i] = 0.f;
        Constraint->Unknown98[i] = 0.f;
    }
    for (INT i = 0; i < 3; i++)
    {
        Constraint->Unknown13C[i] = 0.f;
        Constraint->Unknown148[i] = 0.f;
    }
    for (INT i = 0; i < 2; i++)
    {
        for (INT j = 0; j < 4; j++)
        {
            Constraint->Frame[i].Primary[j] = j == 0 ? 1.f : 0.f;
            Constraint->Frame[i].Orthogonal[j] = j == 1 ? 1.f : 0.f;
            Constraint->Frame[i].Third[j] = j == 2 ? 1.f : 0.f;
            Constraint->Frame[i].Pos[j] = j == 3 ? 1.f : 0.f;
        }
    }
    MeDictNodeInit(&Constraint->WorldNode, const_cast<void*>(DictKey(Constraint->Key)));
    Constraint->WorldNode.Data = Constraint;
    for (INT i = 0; i < 2; i++)
    {
        MeDictNodeInit(&Constraint->BodyNode[i], const_cast<void*>(DictKey(Constraint->Key)));
        Constraint->BodyNode[i].Data = Constraint;
    }
}

// 0x00166000. needs at least one body.
void MdtConstraintEnable(MdtConstraint* Constraint)
{
    if ((Constraint->Flags & 1) || (!Constraint->Body[0] && !Constraint->Body[1]))
        return;
    MdtWorld* World = Constraint->World;
    Constraint->Flags |= 1;
    MeDictInsert(&World->EnabledConstraints, &Constraint->WorldNode, DictKey(Constraint->Key));
    World->EnabledConstraintCount++;
    for (INT i = 0; i < 2; i++)
    {
        if (Constraint->Body[i])
            MeDictInsert(&Constraint->Body[i]->Constraints, &Constraint->BodyNode[i], DictKey(Constraint->Key));
    }
}

// 0x00166080
void MdtConstraintDisable(MdtConstraint* Constraint)
{
    if (!(Constraint->Flags & 1))
        return;
    MdtWorld* World = Constraint->World;
    Constraint->Flags &= ~1;
    MeDictDelete(&World->EnabledConstraints, &Constraint->WorldNode);
    World->EnabledConstraintCount--;
    for (INT i = 0; i < 2; i++)
    {
        if (Constraint->Body[i])
            MeDictDelete(&Constraint->Body[i]->Constraints, &Constraint->BodyNode[i]);
    }
}

// 0x001658f0
UBOOL MdtConstraintIsEnabled(MdtConstraint* Constraint)
{
    return Constraint->Flags & 1;
}

// 0x001660f0
void MdtConstraintDestroy(MdtConstraint* Constraint)
{
    if (Constraint->Flags & 1)
        MdtConstraintDisable(Constraint);
    MdtWorld* World = Constraint->World;
    if (Constraint->Type == 5 && World->Unknown1D4)
        World->Unknown1D4(Constraint);
    Constraint->Type = -1;
    MePools.PutStruct(&World->ConstraintPool, Constraint);
}

// 0x00165b60
void MdtConstraintSetBodies(MdtConstraint* Constraint, MdtBody* Body1, MdtBody* Body2)
{
    Constraint->SetBodies(Constraint, Body1, Body2);
}

// 0x00165970
MdtBody* MdtConstraintGetBody(MdtConstraint* Constraint, INT Index)
{
    return Constraint->Body[Index];
}

// 0x00165bf0. a position in the body's frame, made relative to its center of mass.
void MdtConstraintBodySetPositionRel(MdtConstraint* Constraint, INT Index, FLOAT X, FLOAT Y, FLOAT Z)
{
    MdtBody* Body = Constraint->Body[Index];
    FLOAT* Pos = Constraint->Frame[Index].Pos;
    Pos[0] = X;
    Pos[1] = Y;
    Pos[2] = Z;
    if (Body && Body->HasComOffset)
    {
        Pos[0] = X - Body->ComOffset[0];
        Pos[1] = Y - Body->ComOffset[1];
        Pos[2] = Z - Body->ComOffset[2];
    }
}

// 0x00165b80. a world position.
void MdtConstraintBodySetPosition(MdtConstraint* Constraint, INT Index, FLOAT X, FLOAT Y, FLOAT Z)
{
    const FLOAT Pos[3] = { X, Y, Z };
    MdtConvertPositionVector(nullptr, Pos, Constraint->Body[Index], Constraint->Frame[Index].Pos);
}

// 0x00165e00. axes in the body's frame.
void MdtConstraintBodySetAxesRel(MdtConstraint* Constraint, INT Index, FLOAT PX, FLOAT PY, FLOAT PZ, FLOAT OX, FLOAT OY, FLOAT OZ)
{
    FLOAT P[3] = { PX, PY, PZ };
    FLOAT O[3] = { OX, OY, OZ };
    FLOAT Third[3];
    MakeAxes(P, O, Third);
    for (INT i = 0; i < 3; i++)
    {
        Constraint->Frame[Index].Primary[i] = P[i];
        Constraint->Frame[Index].Orthogonal[i] = O[i];
        Constraint->Frame[Index].Third[i] = Third[i];
    }
}

// 0x00165c70. world axes.
void MdtConstraintBodySetAxes(MdtConstraint* Constraint, INT Index, FLOAT PX, FLOAT PY, FLOAT PZ, FLOAT OX, FLOAT OY, FLOAT OZ)
{
    FLOAT P[3] = { PX, PY, PZ };
    FLOAT O[3] = { OX, OY, OZ };
    FLOAT Third[3];
    MakeAxes(P, O, Third);
    MdtBody* Body = Constraint->Body[Index];
    MdtConvertVector(nullptr, P, Body, Constraint->Frame[Index].Primary);
    MdtConvertVector(nullptr, O, Body, Constraint->Frame[Index].Orthogonal);
    MdtConvertVector(nullptr, Third, Body, Constraint->Frame[Index].Third);
}

// ---------------------------------------------------------------------------------------------- limits

// 0x00184c90. both ends rigid and at zero.
void MdtLimitReset(MdtLimit* Limit)
{
    Limit->Unknown10 = 0;
    Limit->DesiredVelocity = 0.f;
    Limit->MaxForce = 0.f;
    Limit->Unknown24 = 0;
    Limit->Unknown14 = 0;
    Limit->Unknown20 = 0;
    Limit->Unknown28 = 0;
    Limit->Motorized = 0;
    Limit->Unknown1C = 0;
    Limit->Limited = 0;
    Limit->Active = 0;
    Limit->Unknown8 = 0;
    Limit->UnknownC = 0;
    Limit->Unknown2C = FLT_MAX;
    Limit->Lower.Stop = 0.f;
    Limit->Lower.Restitution = 1.f;
    Limit->Lower.Stiffness = FLT_MAX;
    Limit->Lower.Damping = 0.f;
    Limit->Upper.Stop = 0.f;
    Limit->Upper.Restitution = 1.f;
    Limit->Upper.Stiffness = FLT_MAX;
    Limit->Upper.Damping = 0.f;
}

// 0x00184d00
MdtSingleLimit* MdtLimitGetLowerLimit(MdtLimit* Limit)
{
    return &Limit->Lower;
}

// 0x00184d10
MdtSingleLimit* MdtLimitGetUpperLimit(MdtLimit* Limit)
{
    return &Limit->Upper;
}

// 0x00184d30
void MdtLimitSetLimited(MdtLimit* Limit, UBOOL Limited)
{
    Limit->Limited = Limited;
    Limit->Active = Limited || Limit->Motorized || Limit->Unknown1C;
}

// 0x00184de0
void MdtLimitSetLimitedForceMotor(MdtLimit* Limit, FLOAT DesiredVelocity, FLOAT MaxForce)
{
    Limit->DesiredVelocity = DesiredVelocity;
    Limit->Motorized = MaxForce != 0.f;
    Limit->Unknown1C = 0;
    Limit->MaxForce = MaxForce < 0.f ? -MaxForce : MaxForce;
}

// 0x00184da0
void MdtLimitActivateMotor(MdtLimit* Limit, UBOOL Motorized)
{
    Limit->Motorized = Motorized;
    if (Motorized)
        Limit->Unknown1C = 0;
    Limit->Active = Motorized || Limit->Limited || Limit->Unknown1C;
}

// 0x00184c20
void MdtSingleLimitSetStop(MdtSingleLimit* Limit, FLOAT Stop)
{
    Limit->Stop = Stop;
}

// 0x00184c30
void MdtSingleLimitSetStiffness(MdtSingleLimit* Limit, FLOAT Stiffness)
{
    Limit->Stiffness = Stiffness >= 0.f ? Stiffness : -Stiffness;
}

// ---------------------------------------------------------------------------------------------- joint types

// 0x00184590
MdtBSJoint* MdtBSJointCreate(MdtWorld* World)
{
    MdtConstraint* Constraint = MdtConstraintCreate(World);
    if (!Constraint)
        return nullptr;
    Constraint->Type = 0;
    Constraint->AddRows = MdtBSJointAddRows;
    Constraint->MaxRows = 3;
    Constraint->SetBodies = MdtConstraintSetBodiesDefault;
    Constraint->SetAxis = MdtConstraintSetAxisDefault;
    if (MdtConstraintIsEnabled(Constraint))
        MdtConstraintDisable(Constraint);
    MdtConstraintReset(Constraint);
    return reinterpret_cast<MdtBSJoint*>(Constraint);
}

// 0x00184ba0
MdtHinge* MdtHingeCreate(MdtWorld* World)
{
    MdtConstraint* Constraint = MdtConstraintCreate(World);
    if (!Constraint)
        return nullptr;
    Constraint->Type = 1;
    Constraint->AddRows = MdtHingeAddRows;
    Constraint->MaxRows = 6;
    Constraint->SetBodies = MdtConstraintSetBodiesDefault;
    Constraint->SetAxis = MdtConstraintSetAxisDefault;
    if (MdtConstraintIsEnabled(Constraint))
        MdtConstraintDisable(Constraint);
    MdtHinge* Hinge = reinterpret_cast<MdtHinge*>(Constraint);
    MdtLimitReset(&Hinge->Limit);
    MdtConstraintReset(Constraint);
    return Hinge;
}

namespace
{
    // 0x00184aa0. an eighth of a turn each way with no give.
    void MdtSkeletalReset(MdtSkeletal* Joint)
    {
        if (MdtConstraintIsEnabled(&Joint->Base))
            MdtConstraintDisable(&Joint->Base);
        const FLOAT CosHalfAngle = static_cast<FLOAT>(cos(0.39269909262657166));
        Joint->TwistType = 2;
        Joint->ConeType = 2;
        Joint->Unknown160 = 0;
        Joint->TwistStiffness = FLT_MAX;
        Joint->ConeStiffness = FLT_MAX;
        Joint->TwistDamping = FLT_MAX;
        Joint->ConeDamping = FLT_MAX;
        Joint->CosTwistHalfAngle = CosHalfAngle;
        Joint->CosConeHalfAngleX = CosHalfAngle;
        Joint->CosConeHalfAngleY = CosHalfAngle;
        MdtConstraintReset(&Joint->Base);
    }
}

// 0x00184b20
MdtSkeletal* MdtSkeletalCreate(MdtWorld* World)
{
    MdtConstraint* Constraint = MdtConstraintCreate(World);
    if (!Constraint)
        return nullptr;
    Constraint->Type = 9;
    Constraint->AddRows = MdtSkeletalAddRows;
    Constraint->MaxRows = 5;
    Constraint->SetBodies = MdtConstraintSetBodiesDefault;
    Constraint->SetAxis = MdtConstraintSetAxisDefault;
    MdtSkeletal* Joint = reinterpret_cast<MdtSkeletal*>(Constraint);
    MdtSkeletalReset(Joint);
    return Joint;
}

// 0x00184570
MdtBSJoint* MdtConstraintDCastBSJoint(MdtConstraint* Constraint)
{
    return Constraint->Type == 0 ? reinterpret_cast<MdtBSJoint*>(Constraint) : nullptr;
}

// 0x00184b70
MdtHinge* MdtConstraintDCastHinge(MdtConstraint* Constraint)
{
    return Constraint->Type == 1 ? reinterpret_cast<MdtHinge*>(Constraint) : nullptr;
}

// 0x00184940
MdtSkeletal* MdtConstraintDCastSkeletal(MdtConstraint* Constraint)
{
    return Constraint->Type == 9 ? reinterpret_cast<MdtSkeletal*>(Constraint) : nullptr;
}

// 0x00184b90
MdtLimit* MdtHingeGetLimit(MdtHinge* Hinge)
{
    return &Hinge->Limit;
}

// 0x001849a0
void MdtSkeletalSetConeType(MdtSkeletal* Joint, INT Type)
{
    Joint->ConeType = Type;
}

// 0x00184a40
void MdtSkeletalSetTwistType(MdtSkeletal* Joint, INT Type)
{
    Joint->TwistType = Type;
}

// 0x001849c0. angles are full angles; the joint keeps the cosine of half.
void MdtSkeletalSetConeHalfAngleX(MdtSkeletal* Joint, FLOAT Angle)
{
    Joint->CosConeHalfAngleX = cosf(Angle * 0.5f);
}

// 0x001849e0
void MdtSkeletalSetConeHalfAngleY(MdtSkeletal* Joint, FLOAT Angle)
{
    Joint->CosConeHalfAngleY = cosf(Angle * 0.5f);
}

// 0x00184a60
void MdtSkeletalSetTwistHalfAngle(MdtSkeletal* Joint, FLOAT Angle)
{
    Joint->CosTwistHalfAngle = cosf(Angle * 0.5f);
}

// 0x00184a00
void MdtSkeletalSetConeStiffness(MdtSkeletal* Joint, FLOAT Stiffness)
{
    Joint->ConeStiffness = Stiffness;
}

// 0x00184a20
void MdtSkeletalSetConeDamping(MdtSkeletal* Joint, FLOAT Damping)
{
    Joint->ConeDamping = Damping;
}

// 0x00188560
void MdtSkeletalSetTwistStiffness(MdtSkeletal* Joint, FLOAT Stiffness)
{
    Joint->TwistStiffness = Stiffness;
}

// 0x00184a80
void MdtSkeletalSetTwistDamping(MdtSkeletal* Joint, FLOAT Damping)
{
    Joint->TwistDamping = Damping;
}
