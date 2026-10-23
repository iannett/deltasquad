#include "Core.h"
#include "Karma.h"

// 0x00192060. with OwnsParts the joints, models and bodies go too, then any geometry nothing else uses.
void MeAssetInstanceDestroy(MeAssetInstance* Instance)
{
    if (Instance->OwnsParts)
    {
        MeHashIterator It;
        MeHashIteratorInit(&It, Instance->Joints);
        while (MdtConstraint* Constraint = static_cast<MdtConstraint*>(MeHashIteratorGetNext(&It)))
        {
            MdtConstraintDisable(Constraint);
            MdtConstraintDestroy(Constraint);
        }
        MeHashIteratorInit(&It, Instance->Models);
        while (McdModel* Model = static_cast<McdModel*>(MeHashIteratorGetNext(&It)))
        {
            FLOAT* TM = nullptr;
            if (McdModelGetSpace(Model))
                NOT_DECOMPILED(0x001b16f0);
            if (MdtBody* Body = McdModelGetBody(Model))
            {
                MdtBodyDisable(Body);
                MdtBodyDestroy(Body);
            }
            else
            {
                TM = McdModelGetTransformPtr(Model);
            }
            McdModelDestroy(Model);
            if (TM)
                MeMemory.DestroyAligned(TM);
        }
        MeHashIteratorInit(&It, Instance->Geometries);
        while (McdGeometry* Geometry = static_cast<McdGeometry*>(MeHashIteratorGetNext(&It)))
        {
            McdGeometryDecrementReferenceCount(Geometry);
            McdGMDestroyGeometry(Instance->Factory->Factory, Geometry);
        }
    }
    MeHashDestroy(Instance->Geometries);
    MeHashDestroy(Instance->Models);
    MeHashDestroy(Instance->Joints);
    MeMemory.Destroy(Instance);
}

// 0x001921c0, shared with MeFAssetLookupGeometry
McdModel* MeAssetInstanceGetModel(MeAssetInstance* Instance, const char* Name)
{
    return static_cast<McdModel*>(MeHashLookup(Name, Instance->Models));
}

// 0x001921e0, shared with MeFAssetLookupModel
MdtConstraint* MeAssetInstanceGetJoint(MeAssetInstance* Instance, const char* Name)
{
    return static_cast<MdtConstraint*>(MeHashLookup(Name, Instance->Joints));
}

// 0x00192200
void MeAssetInstanceGetGeometryIterator(MeAssetInstance* Instance, MeHashIterator* It)
{
    MeHashIteratorInit(It, Instance->Geometries);
}

// 0x00196f30, partly: TM places a joint end whose body is the world. only the joint types the game's assets use are
// decompiled: hinge, ball and socket, and skeletal.
MdtConstraint* MdtConstraintCreateFromMeFJoint(MeFJoint* Joint, MdtWorld* World, McdModel* Model1, McdModel* Model2, const FLOAT* TM)
{
    const INT Type = MeFJointGetType(Joint);
    MdtBody* Body1 = Model1 ? McdModelGetBody(Model1) : nullptr;
    MdtBody* Body2 = Model2 ? McdModelGetBody(Model2) : nullptr;
    // which end of the joint each body takes
    INT End1 = 0;
    INT End2 = 1;
    if (!Body1)
    {
        if (!Body2)
        {
            MeWarning(0, "MdtConstraintCreateFromMeFJoint: Both bodies cannot be NULL.");
            return nullptr;
        }
        Body1 = Body2;
        Body2 = nullptr;
        End1 = 1;
        End2 = 0;
    }
    else if (Body1 == Body2)
    {
        MeWarning(0, "MdtConstraintCreateFromMeFJoint: Both bodies the same.");
        return nullptr;
    }

    MdtConstraint* Constraint;
    switch (Type)
    {
    case 3:
        Constraint = &MdtBSJointCreate(World)->Base;
        break;
    case 5:
        NOT_DECOMPILED(0x001caa10);
        return nullptr;
    case 6:
        NOT_DECOMPILED(0x00197d00);
        return nullptr;
    case 2:
        Constraint = &MdtHingeCreate(World)->Base;
        break;
    case 1:
        // a car wheel takes its ends the other way round when it has two bodies
        NOT_DECOMPILED(0x001ca9a0);
        return nullptr;
    case 4:
        NOT_DECOMPILED(0x001853a0);
        return nullptr;
    case 7:
        NOT_DECOMPILED(0x001ca7f0);
        return nullptr;
    case 8:
        Constraint = &MdtSkeletalCreate(World)->Base;
        break;
    case 9:
        NOT_DECOMPILED(0x0018d850);
        return nullptr;
    case 10:
        NOT_DECOMPILED(0x001ca600);
        return nullptr;
    default:
        MeWarning(0, "MdtConstraintCreateFromMeFJoint: Unknown joint type.");
        return nullptr;
    }
    MdtConstraintSetBodies(Constraint, Body1, Body2);

    FLOAT Pos[3];
    FLOAT Primary[3];
    FLOAT Orthogonal[3];
    MeFJointGetPosition(Joint, End1, Pos);
    MeFJointGetPrimaryAxis(Joint, End1, Primary);
    MeFJointGetOrthogonalAxis(Joint, End1, Orthogonal);
    MdtConstraintBodySetPositionRel(Constraint, 0, Pos[0], Pos[1], Pos[2]);
    MdtConstraintBodySetAxesRel(Constraint, 0, Primary[0], Primary[1], Primary[2], Orthogonal[0], Orthogonal[1], Orthogonal[2]);
    if (!Body2)
    {
        // the world end is placed by the other end's part (or this end's when it has none), then TM
        INT End = End2;
        MeFPart* Part = MeFJointGetPart(Joint, End2);
        if (!Part)
        {
            End = End1;
            Part = MeFJointGetPart(Joint, End1);
        }
        FLOAT PartPos[3];
        FLOAT PartPrimary[3];
        FLOAT PartOrthogonal[3];
        MeFJointGetPosition(Joint, End, PartPos);
        const FLOAT* PartTM = MeFPartGetTransformPtr(Part);
        MeFJointGetPrimaryAxis(Joint, End, PartPrimary);
        MeFJointGetOrthogonalAxis(Joint, End, PartOrthogonal);
        FLOAT Combined[16];
        if (TM)
        {
            MeMatrix4MultiplyMatrix(Combined, PartTM, TM);
            PartTM = Combined;
        }
        FLOAT WorldPos[3];
        MeMatrix4TMTransform(WorldPos, PartTM, PartPos);
        MdtConstraintBodySetPosition(Constraint, 1, WorldPos[0], WorldPos[1], WorldPos[2]);
        MeMatrix4TMRotate(Primary, PartTM, PartPrimary);
        MeMatrix4TMRotate(Orthogonal, PartTM, PartOrthogonal);
        MdtConstraintBodySetAxes(Constraint, 1, Primary[0], Primary[1], Primary[2], Orthogonal[0], Orthogonal[1], Orthogonal[2]);
    }
    else
    {
        MeFJointGetPosition(Joint, End2, Pos);
        MeFJointGetPrimaryAxis(Joint, End2, Primary);
        MeFJointGetOrthogonalAxis(Joint, End2, Orthogonal);
        MdtConstraintBodySetPositionRel(Constraint, 1, Pos[0], Pos[1], Pos[2]);
        MdtConstraintBodySetAxesRel(Constraint, 1, Primary[0], Primary[1], Primary[2], Orthogonal[0], Orthogonal[1], Orthogonal[2]);
    }

    if (Type == 3)
    {
        MdtConstraintDCastBSJoint(Constraint);
    }
    else if (Type == 2)
    {
        MdtLimit* Limit = MdtHingeGetLimit(MdtConstraintDCastHinge(Constraint));
        MdtSingleLimit* Upper = MdtLimitGetUpperLimit(Limit);
        MdtSingleLimit* Lower = MdtLimitGetLowerLimit(Limit);
        FLOAT Real = 0.f;
        FLOAT MaxForce = 0.f;
        INT Int = 0;
        MeFJointGetReal(Joint, 0, &Real);
        MdtSingleLimitSetStop(Upper, Real);
        MeFJointGetReal(Joint, 1, &Real);
        MdtSingleLimitSetStop(Lower, Real);
        MeFJointGetReal(Joint, 3, &Real);
        MdtSingleLimitSetStiffness(Upper, Real);
        MeFJointGetReal(Joint, 4, &Real);
        MdtSingleLimitSetStiffness(Lower, Real);
        MeFJointGetInt(Joint, 15, &Int);
        MdtLimitSetLimited(Limit, Int);
        MeFJointGetReal(Joint, 23, &Real);
        MeFJointGetReal(Joint, 17, &MaxForce);
        MdtLimitSetLimitedForceMotor(Limit, Real, MaxForce);
        MeFJointGetInt(Joint, 16, &Int);
        MdtLimitActivateMotor(Limit, Int);
    }
    else if (Type == 8)
    {
        MdtSkeletal* Skeletal = MdtConstraintDCastSkeletal(Constraint);
        // the xbox passes types 0 to 3 through and anything else as whatever is on the stack
        INT Enum = 0;
        FLOAT Real = 0.f;
        MeFJointGetEnum(Joint, 26, &Enum);
        MdtSkeletalSetConeType(Skeletal, Enum);
        MeFJointGetReal(Joint, 0, &Real);
        MdtSkeletalSetConeHalfAngleX(Skeletal, Real);
        MeFJointGetReal(Joint, 1, &Real);
        MdtSkeletalSetConeHalfAngleY(Skeletal, Real);
        MeFJointGetReal(Joint, 3, &Real);
        MdtSkeletalSetConeStiffness(Skeletal, Real);
        MeFJointGetReal(Joint, 9, &Real);
        MdtSkeletalSetConeDamping(Skeletal, Real);
        MeFJointGetEnum(Joint, 27, &Enum);
        MdtSkeletalSetTwistType(Skeletal, Enum);
        MeFJointGetReal(Joint, 2, &Real);
        MdtSkeletalSetTwistHalfAngle(Skeletal, Real);
        MeFJointGetReal(Joint, 4, &Real);
        MdtSkeletalSetTwistStiffness(Skeletal, Real);
        MeFJointGetReal(Joint, 10, &Real);
        MdtSkeletalSetTwistDamping(Skeletal, Real);
        if (!Body2)
        {
            // a body jointed to the world gets its smallest inertia raised past the others
            MdtBody* Body = MdtConstraintGetBody(Constraint, 0);
            FLOAT Inertia[9];
            MdtBodyGetInertiaTensor(Body, Inertia);
            if (Inertia[0] < Inertia[4] && Inertia[0] < Inertia[8])
            {
                Inertia[0] = (Inertia[4] < Inertia[8] ? Inertia[4] : Inertia[8]) * 1.1f;
                MdtBodySetInertiaTensor(Body, Inertia);
                MeWarning(0, "AF: Modified Inertia for Skeletal to World");
            }
        }
    }
    return Constraint;
}
