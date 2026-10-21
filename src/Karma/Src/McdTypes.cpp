#include <cmath>

#include "Core.h"
#include "Karma.h"

// collision routines not decompiled yet. each is fatal if reached.
namespace
{
    // 0x00186ea0
    void McdCylinderGetAABB(void*, void*, void*)
    {
        NOT_DECOMPILED(0x00186ea0);
    }

    // 0x001871b0
    void McdCylinderGetBSphere(void*, void*, void*)
    {
        NOT_DECOMPILED(0x001871b0);
    }

    // 0x001871d0
    SWORD McdCylinderGetMassProperties(void*, void*, void*, void*)
    {
        NOT_DECOMPILED(0x001871d0);
        return 0;
    }

    // 0x00187270
    void McdCylinderDebugDraw(void*, void*, void*)
    {
        NOT_DECOMPILED(0x00187270);
    }

    // 0x001873e0
    void McdCylinderMaximumPoint(void*, void*, void*)
    {
        NOT_DECOMPILED(0x001873e0);
    }

    // 0x00187630
    void McdBoxGetAABB(void*, void*, void*)
    {
        NOT_DECOMPILED(0x00187630);
    }

    // 0x00187840
    void McdBoxGetBSphere(void*, void*, void*)
    {
        NOT_DECOMPILED(0x00187840);
    }

    // 0x00187860
    void McdBoxMaximumPoint(void*, void*, void*)
    {
        NOT_DECOMPILED(0x00187860);
    }

    // 0x001879d0
    SWORD McdBoxGetMassProperties(void*, void*, void*, void*)
    {
        NOT_DECOMPILED(0x001879d0);
        return 0;
    }

    // 0x00187a80
    void McdBoxDebugDraw(void*, void*, void*)
    {
        NOT_DECOMPILED(0x00187a80);
    }

    // 0x00187c90
    void McdSphereGetAABB(void*, void*, void*)
    {
        NOT_DECOMPILED(0x00187c90);
    }

    // 0x00187db0
    void McdSphereGetBSphere(void*, void*, void*)
    {
        NOT_DECOMPILED(0x00187db0);
    }

    // 0x00187dd0
    void McdSphereMaximumPoint(void*, void*, void*)
    {
        NOT_DECOMPILED(0x00187dd0);
    }

    // 0x00187e30
    SWORD McdSphereGetMassProperties(void*, void*, void*, void*)
    {
        NOT_DECOMPILED(0x00187e30);
        return 0;
    }

    // 0x00187ee0
    void McdSphereDebugDraw(void*, void*, void*)
    {
        NOT_DECOMPILED(0x00187ee0);
    }

    // 0x00189900
    void McdAggregateDestroy(void*)
    {
        NOT_DECOMPILED(0x00189900);
    }

    // 0x00189950
    void McdAggregateGetAABB(void*, void*, void*)
    {
        NOT_DECOMPILED(0x00189950);
    }

    // 0x0018a040
    SWORD McdAggregateGetMassProperties(void*, void*, void*, void*)
    {
        NOT_DECOMPILED(0x0018a040);
        return 0;
    }

    // 0x0018a810
    void McdAggregateDebugDraw(void*, void*, void*)
    {
        NOT_DECOMPILED(0x0018a810);
    }

    // 0x0018adb0
    void McdSphylGetAABB(void*, void*, void*)
    {
        NOT_DECOMPILED(0x0018adb0);
    }

    // 0x0018af30
    void McdSphylGetBSphere(void*, void*, void*)
    {
        NOT_DECOMPILED(0x0018af30);
    }

    // 0x0018af60
    void McdSphylMaximumPoint(void*, void*, void*)
    {
        NOT_DECOMPILED(0x0018af60);
    }

    // 0x0018b010
    SWORD McdSphylGetMassProperties(void*, void*, void*, void*)
    {
        NOT_DECOMPILED(0x0018b010);
        return 0;
    }

    // 0x0018b530
    void McdConvexMeshDestroy(void*)
    {
        NOT_DECOMPILED(0x0018b530);
    }

    // 0x0018b660
    void McdConvexMeshGetAABB(void*, void*, void*)
    {
        NOT_DECOMPILED(0x0018b660);
    }

    // 0x0018b760
    void McdConvexMeshGetBSphere(void*, void*, void*)
    {
        NOT_DECOMPILED(0x0018b760);
    }

    // 0x0018b790
    void McdConvexMeshMaximumPoint(void*, void*, void*)
    {
        NOT_DECOMPILED(0x0018b790);
    }

    // 0x00196a20
    void McdTriangleListDestroy(void*)
    {
        NOT_DECOMPILED(0x00196a20);
    }

    // the list's box moved by TM, as a box along the axes
    void McdTriangleListTransformBox(const McdTriangleList* List, const FLOAT* TM, FLOAT* Min, FLOAT* Max)
    {
        for (INT i = 0; i < 3; i++)
        {
            const FLOAT Extent = fabsf(TM[i]) * List->Extent[0] + fabsf(TM[4 + i]) * List->Extent[1] + fabsf(TM[8 + i]) * List->Extent[2];
            const FLOAT Center = List->Center[0] * TM[i] + List->Center[1] * TM[4 + i] + List->Center[2] * TM[8 + i] + TM[12 + i];
            Min[i] = Center - Extent;
            Max[i] = Center + Extent;
        }
    }

    // 0x00196a40. with FinalTM the box also covers where the list ends up.
    void McdTriangleListGetAABB(void* InstancePtr, void* FinalTM, void*)
    {
        McdGeometryInstance* Instance = static_cast<McdGeometryInstance*>(InstancePtr);
        const McdTriangleList* List = reinterpret_cast<McdTriangleList*>(Instance->Geometry);
        McdTriangleListTransformBox(List, Instance->TM, Instance->Min, Instance->Max);
        if (!FinalTM)
            return;
        FLOAT Min[3], Max[3];
        McdTriangleListTransformBox(List, static_cast<FLOAT*>(FinalTM), Min, Max);
        for (INT i = 0; i < 3; i++)
        {
            Instance->Min[i] = Instance->Min[i] < Min[i] ? Instance->Min[i] : Min[i];
            Instance->Max[i] = Max[i] < Instance->Max[i] ? Instance->Max[i] : Max[i];
        }
    }

    // 0x00196d00
    void McdTriangleListGetBSphere(void*, void*, void*)
    {
        NOT_DECOMPILED(0x00196d00);
    }

    // 0x00196d50
    void McdTriangleListMaximumPoint(void*, void*, void*)
    {
        NOT_DECOMPILED(0x00196d50);
    }

    // 0x00196d80
    SWORD McdTriangleListGetMassProperties(void*, void*, void*, void*)
    {
        NOT_DECOMPILED(0x00196d80);
        return 0;
    }

    // 0x00196dd0
    void McdTriangleListDebugDraw(void*, void*, void*)
    {
        NOT_DECOMPILED(0x00196dd0);
    }

    // 0x001b3110
    SWORD McdConvexMeshGetMassProperties(void*, void*, void*, void*)
    {
        NOT_DECOMPILED(0x001b3110);
        return 0;
    }

    // 0x001b3840
    void McdPlaneGetAABB(void*, void*, void*)
    {
        NOT_DECOMPILED(0x001b3840);
    }

    // 0x001b3a70
    void McdPlaneGetBSphere(void*, void*, void*)
    {
        NOT_DECOMPILED(0x001b3a70);
    }

    // 0x001b3a90
    void McdPlaneMaximumPoint(void*, void*, void*)
    {
        NOT_DECOMPILED(0x001b3a90);
    }

    // 0x001b3b10
    SWORD McdPlaneGetMassProperties(void*, void*, void*, void*)
    {
        NOT_DECOMPILED(0x001b3b10);
        return 0;
    }

    // 0x001b3c20
    void McdPlaneDebugDraw(void*, void*, void*)
    {
        NOT_DECOMPILED(0x001b3c20);
    }

    // 0x0018a350
    void McdAggregateUnknown8()
    {
        NOT_DECOMPILED(0x0018a350);
    }

    // 0x0018a640
    void McdAggregateTypeFunction()
    {
        NOT_DECOMPILED(0x0018a640);
    }

    // 0x001b3d80
    void McdConvexUnknown0()
    {
        NOT_DECOMPILED(0x001b3d80);
    }

    // 0x001b3dd0
    void McdConvexUnknown4()
    {
        NOT_DECOMPILED(0x001b3dd0);
    }

    // 0x001b5440
    void McdConvexUnknown8()
    {
        NOT_DECOMPILED(0x001b5440);
    }

    // 0x001b5460
    void McdSphylConvexMeshUnknown8()
    {
        NOT_DECOMPILED(0x001b5460);
    }

    // 0x001b5810
    void McdConvexMeshTypeFunction()
    {
        NOT_DECOMPILED(0x001b5810);
    }

    // 0x001b66e0
    void McdConvexMeshTriangleListUnknown8()
    {
        NOT_DECOMPILED(0x001b66e0);
    }

    // 0x001b7e00
    void McdSphylPlaneUnknown8()
    {
        NOT_DECOMPILED(0x001b7e00);
    }

    // 0x001b8180
    void McdSphylSphereUnknown8()
    {
        NOT_DECOMPILED(0x001b8180);
    }

    // 0x001b8420
    void McdSphylSphylUnknown8()
    {
        NOT_DECOMPILED(0x001b8420);
    }

    // 0x001b8750
    void McdSphylBoxUnknown8()
    {
        NOT_DECOMPILED(0x001b8750);
    }

    // 0x001b92e0
    void McdSphylCylinderUnknown8()
    {
        NOT_DECOMPILED(0x001b92e0);
    }

    // 0x001ba310
    void McdSphylTriangleListUnknown8()
    {
        NOT_DECOMPILED(0x001ba310);
    }

    // 0x001ba6b0
    void McdSphylTypeFunction()
    {
        NOT_DECOMPILED(0x001ba6b0);
    }

    // 0x001bc130
    void McdCylinderTriangleListUnknown8()
    {
        NOT_DECOMPILED(0x001bc130);
    }

    // 0x001bd930
    void McdBoxTriangleListUnknown8()
    {
        NOT_DECOMPILED(0x001bd930);
    }

    // 0x001be320
    void McdSphereTriangleListUnknown8()
    {
        NOT_DECOMPILED(0x001be320);
    }

    // 0x001be800
    void McdCylinderTypeFunction()
    {
        NOT_DECOMPILED(0x001be800);
    }

    // 0x001bf2f0
    void McdCylinderCylinderUnknownC()
    {
        NOT_DECOMPILED(0x001bf2f0);
    }

    // 0x001c1960
    void McdCylinderCylinderUnknown8()
    {
        NOT_DECOMPILED(0x001c1960);
    }

    // 0x001c1e80
    void McdCylinderSphereUnknownC()
    {
        NOT_DECOMPILED(0x001c1e80);
    }

    // 0x001c1fa0
    void McdCylinderSphereUnknown8()
    {
        NOT_DECOMPILED(0x001c1fa0);
    }

    // 0x001c2810
    void McdCylinderPlaneUnknown8()
    {
        NOT_DECOMPILED(0x001c2810);
    }

    // 0x001c3090
    void McdBoxCylinderUnknownC()
    {
        NOT_DECOMPILED(0x001c3090);
    }

    // 0x001c4410
    void McdBoxCylinderUnknown8()
    {
        NOT_DECOMPILED(0x001c4410);
    }

    // 0x001c4990
    void McdSphereTypeFunction()
    {
        NOT_DECOMPILED(0x001c4990);
    }

    // 0x001c4bd0
    void McdPlaneTypeFunction()
    {
        NOT_DECOMPILED(0x001c4bd0);
    }

    // 0x001c4e30
    void McdBoxTypeFunction()
    {
        NOT_DECOMPILED(0x001c4e30);
    }

    // 0x001e91b0
    void McdSpherePlaneUnknownC()
    {
        NOT_DECOMPILED(0x001e91b0);
    }

    // 0x001e9330
    void McdSpherePlaneUnknown8()
    {
        NOT_DECOMPILED(0x001e9330);
    }

    // 0x001e97a0
    void McdBoxSphereUnknownC()
    {
        NOT_DECOMPILED(0x001e97a0);
    }

    // 0x001e9890
    void McdBoxSphereUnknown8()
    {
        NOT_DECOMPILED(0x001e9890);
    }

    // 0x001ea0c0
    void McdBoxPlaneUnknownC()
    {
        NOT_DECOMPILED(0x001ea0c0);
    }

    // 0x001ea200
    void McdBoxPlaneUnknown8()
    {
        NOT_DECOMPILED(0x001ea200);
    }

    // 0x001eada0
    void McdBoxBoxUnknownC()
    {
        NOT_DECOMPILED(0x001eada0);
    }

    // 0x001eb990
    void McdBoxBoxUnknown8()
    {
        NOT_DECOMPILED(0x001eb990);
    }

    // 0x001ebca0
    void McdSphereSphereUnknown8()
    {
        NOT_DECOMPILED(0x001ebca0);
    }

    // 0x001ebee0
    void McdSphereSphereUnknownC()
    {
        NOT_DECOMPILED(0x001ebee0);
    }

    // 0x001f01a0
    void McdConvexMeshTriangleListHook()
    {
        NOT_DECOMPILED(0x001f01a0);
    }

    // 0x001890e0. unlinks a geometry from its owner and frees it.
    void McdGeometryDestroyDefault(void* Geometry)
    {
        NOT_DECOMPILED(0x001890e0);
    }

    // 0x00189ad0
    void McdNoOp(void*, void*, void*)
    {
    }

    // 0x0018de30
    void McdNullGetAABB(void*, void*, void*)
    {
        MeFatalError(0, "Attempt to calculate the bounding box of a null geometry (perhaps you inserted it into an McdSpace?)");
    }

    // 0x0018de50
    void McdNullGetBSphere(void*, void*, void*)
    {
        MeFatalError(0, "Attempt to calculate the bounding sphere of a null geometry");
    }

    // 0x0018de70
    void McdNullMaximumPoint(void*, void*, void*)
    {
        MeFatalError(0, "Attempt to calculate a maximum point on a null geometry");
    }

    // 0x0018de90
    SWORD McdNullGetMassProperties(void*, void*, void*, void*)
    {
        MeFatalError(0, "Attempt to calculate mass properties of a null geometry");
        return 0;
    }

    // 0x004322bc. warns about drawing a null geometry only once.
    UBOOL GWarnNullDebugDraw = 1;

    // 0x0018deb0
    void McdNullDebugDraw(void*, void*, void*)
    {
        if (GWarnNullDebugDraw)
            MeWarning(0, "McdNullDebugDraw: Don't know how to draw a Null!");
        GWarnNullDebugDraw = 0;
    }
}

// 0x00188010
static void McdSphereRegisterType(McdFramework* Framework)
{
    McdGeometryRegisterType(Framework, 1, "McdSphere", McdGeometryDestroyDefault, McdSphereGetAABB, McdSphereGetBSphere, McdSphereMaximumPoint, McdSphereGetMassProperties, McdSphereDebugDraw);
}

// 0x00187be0
static void McdBoxRegisterType(McdFramework* Framework)
{
    McdGeometryRegisterType(Framework, 2, "McdBox", McdGeometryDestroyDefault, McdBoxGetAABB, McdBoxGetBSphere, McdBoxMaximumPoint, McdBoxGetMassProperties, McdBoxDebugDraw);
}

// 0x001b3c50
static void McdPlaneRegisterType(McdFramework* Framework)
{
    McdGeometryRegisterType(Framework, 3, "McdPlane", McdGeometryDestroyDefault, McdPlaneGetAABB, McdPlaneGetBSphere, McdPlaneMaximumPoint, McdPlaneGetMassProperties, McdPlaneDebugDraw);
}

// 0x00187550
static void McdCylinderRegisterType(McdFramework* Framework)
{
    McdGeometryRegisterType(Framework, 4, "McdCylinder", McdGeometryDestroyDefault, McdCylinderGetAABB, McdCylinderGetBSphere, McdCylinderMaximumPoint, McdCylinderGetMassProperties, McdCylinderDebugDraw);
}

// 0x0018b0c0
static void McdSphylRegisterType(McdFramework* Framework)
{
    McdGeometryRegisterType(Framework, 5, "McdSphyl", McdGeometryDestroyDefault, McdSphylGetAABB, McdSphylGetBSphere, McdSphylMaximumPoint, McdSphylGetMassProperties, McdNoOp);
}

// 0x00196e00
static void McdTriangleListRegisterType(McdFramework* Framework)
{
    McdGeometryRegisterType(Framework, 6, "McdTriangleList", McdTriangleListDestroy, McdTriangleListGetAABB, McdTriangleListGetBSphere, McdTriangleListMaximumPoint, McdTriangleListGetMassProperties, McdTriangleListDebugDraw);
}

// 0x0018b7f0
void McdConvexMeshRegisterType(McdFramework* Framework)
{
    McdGeometryRegisterType(Framework, 7, "McdConvexMesh", McdConvexMeshDestroy, McdConvexMeshGetAABB, McdConvexMeshGetBSphere, McdConvexMeshMaximumPoint, McdConvexMeshGetMassProperties, McdNoOp);
}

// 0x0018a8b0
void McdAggregateRegisterType(McdFramework* Framework)
{
    McdGeometryRegisterType(Framework, 8, "McdAggregate", McdAggregateDestroy, McdAggregateGetAABB, McdNoOp, McdNoOp, McdAggregateGetMassProperties, McdAggregateDebugDraw);
}

// 0x001ec080
static UBOOL McdSphereSphereRegisterInteraction(McdFramework* Framework)
{
    const McdInteraction Interaction = { nullptr, nullptr, McdSphereSphereUnknown8, McdSphereSphereUnknownC, 0, 0, 0 };
    McdFrameworkSetInteraction(Framework, 1, 1, &Interaction);
    return 1;
}

// 0x001ebc50
static UBOOL McdBoxBoxRegisterInteraction(McdFramework* Framework)
{
    const McdInteraction Interaction = { nullptr, nullptr, McdBoxBoxUnknown8, McdBoxBoxUnknownC, 0, 1, 0 };
    McdFrameworkSetInteraction(Framework, 2, 2, &Interaction);
    return 1;
}

// 0x001ea370
static UBOOL McdBoxPlaneRegisterInteraction(McdFramework* Framework)
{
    const McdInteraction Interaction = { nullptr, nullptr, McdBoxPlaneUnknown8, McdBoxPlaneUnknownC, 0, 1, 0 };
    McdFrameworkSetInteraction(Framework, 2, 3, &Interaction);
    return 1;
}

// 0x001e99c0
static UBOOL McdBoxSphereRegisterInteraction(McdFramework* Framework)
{
    const McdInteraction Interaction = { nullptr, nullptr, McdBoxSphereUnknown8, McdBoxSphereUnknownC, 0, 0, 0 };
    McdFrameworkSetInteraction(Framework, 2, 1, &Interaction);
    return 1;
}

// 0x001e94b0
static UBOOL McdSpherePlaneRegisterInteraction(McdFramework* Framework)
{
    const McdInteraction Interaction = { nullptr, nullptr, McdSpherePlaneUnknown8, McdSpherePlaneUnknownC, 0, 0, 0 };
    McdFrameworkSetInteraction(Framework, 1, 3, &Interaction);
    return 1;
}

// 0x001c4720
static UBOOL McdBoxCylinderRegisterInteraction(McdFramework* Framework)
{
    const McdInteraction Interaction = { nullptr, nullptr, McdBoxCylinderUnknown8, McdBoxCylinderUnknownC, 0, 1, 0 };
    McdFrameworkSetInteraction(Framework, 2, 4, &Interaction);
    return 1;
}

// 0x001c2a10
static UBOOL McdCylinderPlaneRegisterInteraction(McdFramework* Framework)
{
    const McdInteraction Interaction = { nullptr, nullptr, McdCylinderPlaneUnknown8, nullptr, 0, 1, 0 };
    McdFrameworkSetInteraction(Framework, 4, 3, &Interaction);
    return 1;
}

// 0x001c20f0
static UBOOL McdCylinderSphereRegisterInteraction(McdFramework* Framework)
{
    const McdInteraction Interaction = { nullptr, nullptr, McdCylinderSphereUnknown8, McdCylinderSphereUnknownC, 0, 0, 0 };
    McdFrameworkSetInteraction(Framework, 4, 1, &Interaction);
    return 1;
}

// 0x001c1bd0
static UBOOL McdCylinderCylinderRegisterInteraction(McdFramework* Framework)
{
    const McdInteraction Interaction = { nullptr, nullptr, McdCylinderCylinderUnknown8, McdCylinderCylinderUnknownC, 0, 1, 0 };
    McdFrameworkSetInteraction(Framework, 4, 4, &Interaction);
    return 1;
}

// 0x001be7b0
static UBOOL McdSphereTriangleListRegisterInteraction(McdFramework* Framework)
{
    const McdInteraction Interaction = { nullptr, nullptr, McdSphereTriangleListUnknown8, nullptr, 0, 1, 0 };
    McdFrameworkSetInteraction(Framework, 1, 6, &Interaction);
    return 1;
}

// 0x001bde10
static UBOOL McdBoxTriangleListRegisterInteraction(McdFramework* Framework)
{
    const McdInteraction Interaction = { nullptr, nullptr, McdBoxTriangleListUnknown8, nullptr, 0, 1, 0 };
    McdFrameworkSetInteraction(Framework, 2, 6, &Interaction);
    return 1;
}

// 0x001bc6c0
static UBOOL McdCylinderTriangleListRegisterInteraction(McdFramework* Framework)
{
    const McdInteraction Interaction = { nullptr, nullptr, McdCylinderTriangleListUnknown8, nullptr, 0, 1, 0 };
    McdFrameworkSetInteraction(Framework, 4, 6, &Interaction);
    return 1;
}

// 0x001b8130
static UBOOL McdSphylPlaneRegisterInteraction(McdFramework* Framework)
{
    const McdInteraction Interaction = { nullptr, nullptr, McdSphylPlaneUnknown8, nullptr, 0, 1, 0 };
    McdFrameworkSetInteraction(Framework, 5, 3, &Interaction);
    return 1;
}

// 0x001b83d0
static UBOOL McdSphylSphereRegisterInteraction(McdFramework* Framework)
{
    const McdInteraction Interaction = { nullptr, nullptr, McdSphylSphereUnknown8, nullptr, 0, 1, 0 };
    McdFrameworkSetInteraction(Framework, 5, 1, &Interaction);
    return 1;
}

// 0x001b8700
static UBOOL McdSphylSphylRegisterInteraction(McdFramework* Framework)
{
    const McdInteraction Interaction = { nullptr, nullptr, McdSphylSphylUnknown8, nullptr, 0, 1, 0 };
    McdFrameworkSetInteraction(Framework, 5, 5, &Interaction);
    return 1;
}

// 0x001b9290
static UBOOL McdSphylBoxRegisterInteraction(McdFramework* Framework)
{
    const McdInteraction Interaction = { nullptr, nullptr, McdSphylBoxUnknown8, nullptr, 0, 1, 0 };
    McdFrameworkSetInteraction(Framework, 5, 2, &Interaction);
    return 1;
}

// 0x001ba2c0
static UBOOL McdSphylCylinderRegisterInteraction(McdFramework* Framework)
{
    const McdInteraction Interaction = { nullptr, nullptr, McdSphylCylinderUnknown8, nullptr, 0, 1, 0 };
    McdFrameworkSetInteraction(Framework, 5, 4, &Interaction);
    return 1;
}

// 0x001ba500
static UBOOL McdSphylTriangleListRegisterInteraction(McdFramework* Framework)
{
    const McdInteraction Interaction = { nullptr, nullptr, McdSphylTriangleListUnknown8, nullptr, 0, 1, 0 };
    McdFrameworkSetInteraction(Framework, 5, 6, &Interaction);
    return 1;
}

// 0x0018df20
static UBOOL McdConvexMeshConvexMeshRegisterInteraction(McdFramework* Framework)
{
    const McdInteraction Interaction = { McdConvexUnknown0, McdConvexUnknown4, McdConvexUnknown8, nullptr, 0, 1, 0 };
    McdFrameworkSetInteraction(Framework, 7, 7, &Interaction);
    return 1;
}

// 0x0018e0b0
static UBOOL McdBoxConvexMeshRegisterInteraction(McdFramework* Framework)
{
    const McdInteraction Interaction = { McdConvexUnknown0, McdConvexUnknown4, McdConvexUnknown8, nullptr, 0, 1, 0 };
    McdFrameworkSetInteraction(Framework, 2, 7, &Interaction);
    return 1;
}

// 0x0018e010
static UBOOL McdSphereConvexMeshRegisterInteraction(McdFramework* Framework)
{
    const McdInteraction Interaction = { McdConvexUnknown0, McdConvexUnknown4, McdConvexUnknown8, nullptr, 0, 1, 0 };
    McdFrameworkSetInteraction(Framework, 1, 7, &Interaction);
    return 1;
}

// 0x0018e060
static UBOOL McdCylinderConvexMeshRegisterInteraction(McdFramework* Framework)
{
    const McdInteraction Interaction = { McdConvexUnknown0, McdConvexUnknown4, McdConvexUnknown8, nullptr, 0, 1, 0 };
    McdFrameworkSetInteraction(Framework, 4, 7, &Interaction);
    return 1;
}

// 0x001b6c70
static UBOOL McdConvexMeshTriangleListRegisterInteraction(McdFramework* Framework)
{
    const McdInteraction Interaction = { nullptr, nullptr, McdConvexMeshTriangleListUnknown8, nullptr, 0, 1, 0 };
    McdFrameworkSetInteraction(Framework, 7, 6, &Interaction);
    McdFrameworkAddHook(Framework, McdConvexMeshTriangleListHook);
    return 1;
}

// 0x0018df70
static UBOOL McdConvexMeshPlaneRegisterInteraction(McdFramework* Framework)
{
    const McdInteraction Interaction = { McdConvexUnknown0, McdConvexUnknown4, McdConvexUnknown8, nullptr, 0, 1, 0 };
    McdFrameworkSetInteraction(Framework, 7, 3, &Interaction);
    return 1;
}

// 0x0018dfc0
static UBOOL McdSphylConvexMeshRegisterInteraction(McdFramework* Framework)
{
    const McdInteraction Interaction = { nullptr, nullptr, McdSphylConvexMeshUnknown8, nullptr, 0, 1, 0 };
    McdFrameworkSetInteraction(Framework, 5, 7, &Interaction);
    return 1;
}

// 0x0018dee0
void McdNullRegisterType(McdFramework* Framework)
{
    McdGeometryRegisterType(Framework, 0, "Null", McdGeometryDestroyDefault, McdNullGetAABB, McdNullGetBSphere, McdNullMaximumPoint,
        McdNullGetMassProperties, McdNullDebugDraw);
}

// 0x001c50e0
static void McdSphereBoxPlaneRegisterTypes(McdFramework* Framework)
{
    McdSphereRegisterType(Framework);
    McdBoxRegisterType(Framework);
    McdPlaneRegisterType(Framework);
}

// 0x0018e1c0
void McdPrimitivesRegisterTypes(McdFramework* Framework)
{
    McdSphereBoxPlaneRegisterTypes(Framework);
    McdCylinderRegisterType(Framework);
    McdTriangleListRegisterType(Framework);
    McdSphylRegisterType(Framework);
}

// 0x001c50b0
static void McdSphereBoxPlaneRegisterInteractions(McdFramework* Framework)
{
    McdSphereSphereRegisterInteraction(Framework);
    McdBoxBoxRegisterInteraction(Framework);
    McdBoxPlaneRegisterInteraction(Framework);
    McdBoxSphereRegisterInteraction(Framework);
    McdSpherePlaneRegisterInteraction(Framework);
}

// 0x0018e140
UBOOL McdPrimitivesRegisterInteractions(McdFramework* Framework)
{
    McdSphereBoxPlaneRegisterInteractions(Framework);
    // 0x001c4df0, 0x001c5090 and 0x001c4e10
    McdFrameworkSetTypeFunction(Framework, 1, McdSphereTypeFunction);
    McdFrameworkSetTypeFunction(Framework, 2, McdBoxTypeFunction);
    McdFrameworkSetTypeFunction(Framework, 3, McdPlaneTypeFunction);
    McdBoxCylinderRegisterInteraction(Framework);
    McdCylinderPlaneRegisterInteraction(Framework);
    McdCylinderSphereRegisterInteraction(Framework);
    McdCylinderCylinderRegisterInteraction(Framework);
    // 0x001bec30
    McdFrameworkSetTypeFunction(Framework, 4, McdCylinderTypeFunction);
    McdSphereTriangleListRegisterInteraction(Framework);
    McdBoxTriangleListRegisterInteraction(Framework);
    McdCylinderTriangleListRegisterInteraction(Framework);
    McdSphylPlaneRegisterInteraction(Framework);
    McdSphylSphereRegisterInteraction(Framework);
    McdSphylSphylRegisterInteraction(Framework);
    McdSphylBoxRegisterInteraction(Framework);
    McdSphylCylinderRegisterInteraction(Framework);
    McdSphylTriangleListRegisterInteraction(Framework);
    // 0x001bab80
    McdFrameworkSetTypeFunction(Framework, 5, McdSphylTypeFunction);
    return 1;
}

// 0x0018e100
UBOOL McdConvexMeshRegisterInteractions(McdFramework* Framework)
{
    McdConvexMeshConvexMeshRegisterInteraction(Framework);
    McdBoxConvexMeshRegisterInteraction(Framework);
    McdSphereConvexMeshRegisterInteraction(Framework);
    McdCylinderConvexMeshRegisterInteraction(Framework);
    McdConvexMeshTriangleListRegisterInteraction(Framework);
    McdConvexMeshPlaneRegisterInteraction(Framework);
    // 0x001b5ba0
    McdFrameworkSetTypeFunction(Framework, 7, McdConvexMeshTypeFunction);
    McdSphylConvexMeshRegisterInteraction(Framework);
    return 1;
}

// 0x0018a7b0. an aggregate meets every type the same way.
static UBOOL McdAggregateRegisterAllInteractions(McdFramework* Framework)
{
    const McdInteraction Interaction = { nullptr, nullptr, McdAggregateUnknown8, nullptr, 0, 1, 0 };
    for (INT i = 0; i < Framework->GeometryTypeCount; i++)
        McdFrameworkSetInteraction(Framework, 8, i, &Interaction);
    return 1;
}

// 0x0018a890
void McdAggregateRegisterInteractions(McdFramework* Framework)
{
    McdAggregateRegisterAllInteractions(Framework);
    McdFrameworkSetTypeFunction(Framework, 8, McdAggregateTypeFunction);
}
