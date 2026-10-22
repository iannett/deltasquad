#include <cfloat>
#include <cmath>

#include "Core.h"
#include "Karma.h"

// the xbox's body and constraint sizes, used until those structs are decompiled
static constexpr INT MdtBodySize = 0x240;
static constexpr INT MdtConstraintSize = 0x1ec;

namespace
{
    // 0x0018f200
    void MdtWorldUnknown228(void* Unknown1, void* Bodies, INT Count)
    {
        NOT_DECOMPILED(0x0018f200);
    }

    // 0x00192290. the defaults of every world setting, scaled by the world's units (name invented).
    void MdtWorldResetParameters(MdtWorld* World)
    {
        const FLOAT Epsilon = 0.01f / World->MassScale;
        World->Unknown1E4 = 0.2f;
        World->Gamma = 0.2f;
        World->Unknown18C = 0.001f;
        World->AutoDisableAngularVelocityThreshold = 0.001f;
        World->AutoDisableAliveWindow = 0.2f;
        World->AutoDisableAngularAccelerationThreshold = 0.05f;
        World->Unknown1AC = 0;
        World->Unknown1B0 = 150.f;
        World->Unknown1B4 = 100.f;
        World->Unknown1B8 = 40.f;
        World->Unknown1C4 = 40.f;
        World->Unknown1A8 = 0x7ffffffc;
        World->AutoDisable = 1;
        World->Gravity[0] = 0.f;
        World->Gravity[1] = 0.f;
        World->Gravity[2] = 0.f;
        World->Unknown1C8 = 0;
        World->Unknown174 = 0;
        World->Unknown178 = 0;
        World->Unknown1E8 = 10;
        World->Unknown204 = 0;
        World->Unknown1FC = 0;
        World->Unknown20C = 0;
        World->Unknown214 = 0;
        World->Unknown22C = 0;
        World->Unknown228 = MdtWorldUnknown228;
        World->Unknown230 = 0;
        World->Epsilon = Epsilon;
        World->Unknown1EC = World->LengthScale * 0.03f;
        World->AutoDisableVelocityThreshold = World->LengthScale * 0.02f;
        World->AutoDisableAccelerationThreshold = World->LengthScale * 0.05f;
        World->Unknown1BC = World->LengthScale * 15.f;
        World->Unknown1C0 = World->LengthScale * 6.5f;
    }
}

// 0x001925a0
MdtWorld* MdtWorldCreate(INT MaxBodies, INT MaxConstraints, FLOAT LengthScale, FLOAT MassScale)
{
    if (!MaxBodies)
        return nullptr;
    MdtWorld* World = static_cast<MdtWorld*>(MeMemory.Create(sizeof(MdtWorld)));
    if (!World)
        return nullptr;
    World->LengthScale = LengthScale;
    World->MassScale = MassScale;
    World->DensityScale = MassScale / (LengthScale * LengthScale * LengthScale);
    // 0x001d4190 returns 0
    World->Unknown1F8 = 0;
    MeChunkInit(&World->Unknown9C, 0x10);
    World->UnknownB8 = 0;
    World->UnknownBC = 0;
    World->MaxBodies = MaxBodies;
    MeChunkInit(&World->UnknownDC, 0x10);
    MePools.Init(&World->BodyPool, World->MaxBodies, MdtBodySize, 0x10);
    World->MaxConstraints = MaxConstraints;
    World->UnknownF8 = 0;
    MePools.Init(&World->ConstraintPool, MaxConstraints, MdtConstraintSize, 0);
    MeDictInit(&World->Unknown0, 1000000, MeDictCompareKeys);
    MeDictAllowDupes(&World->Unknown0);
    MeDictInit(&World->EnabledBodies, 1000000, MeDictCompareKeys);
    MeDictAllowDupes(&World->EnabledBodies);
    MeDictInit(&World->Unknown34, 1000000, MeDictCompareKeys);
    MeDictAllowDupes(&World->Unknown34);
    MeChunkInit(&World->Unknown14C, 0);
    MeChunkInit(&World->Unknown134, 0);
    World->BodyDisableCB = nullptr;
    World->BodyEnableCB = nullptr;
    World->Unknown1D4 = nullptr;
    MdtWorldResetParameters(World);
    World->Unknown1D8 = McdGetVersion();
    return World;
}

// 0x00192440
void MdtWorldSetEpsilon(MdtWorld* World, FLOAT Epsilon)
{
    World->Epsilon = Epsilon;
}

// 0x00192480
void MdtWorldSetAutoDisable(MdtWorld* World, UBOOL AutoDisable)
{
    World->AutoDisable = AutoDisable;
}

// 0x001924a0
void MdtWorldSetAutoDisableVelocityThreshold(MdtWorld* World, FLOAT Threshold)
{
    World->AutoDisableVelocityThreshold = Threshold;
}

// 0x001924c0
void MdtWorldSetAutoDisableAngularVelocityThreshold(MdtWorld* World, FLOAT Threshold)
{
    World->AutoDisableAngularVelocityThreshold = Threshold;
}

// 0x001924e0
void MdtWorldSetAutoDisableAccelerationThreshold(MdtWorld* World, FLOAT Threshold)
{
    World->AutoDisableAccelerationThreshold = Threshold;
}

// 0x00192500
void MdtWorldSetAutoDisableAngularAccelerationThreshold(MdtWorld* World, FLOAT Threshold)
{
    World->AutoDisableAngularAccelerationThreshold = Threshold;
}

// 0x00192520
void MdtWorldSetAutoDisableAliveWindow(MdtWorld* World, FLOAT Time)
{
    World->AutoDisableAliveWindow = Time;
}

// 0x00192460
void MdtWorldSetGamma(MdtWorld* World, FLOAT Gamma)
{
    World->Unknown1E4 = Gamma;
    World->Gamma = Gamma;
}

// 0x00186970. room for every body in its own partition. port: the pointer arrays come first so they stay aligned.
MdtPartitionOutput* MdtPartitionOutputCreateInChunk(MeChunk* Chunk, INT MaxBodies, INT MaxConstraints)
{
    const INT Size = static_cast<INT>(sizeof(MdtPartitionOutput)
        + MaxBodies * (sizeof(void*) + 4 * sizeof(INT) + sizeof(MdtPartitionInfo)) + MaxConstraints * sizeof(void*));
    MdtPartitionOutput* Output = static_cast<MdtPartitionOutput*>(MeChunkGetMem(Chunk, Size));
    BYTE* Next = reinterpret_cast<BYTE*>(Output + 1);
    Output->Bodies = reinterpret_cast<void**>(Next);
    Next += MaxBodies * sizeof(void*);
    Output->Constraints = reinterpret_cast<void**>(Next);
    Next += MaxConstraints * sizeof(void*);
    Output->BodiesStart = reinterpret_cast<INT*>(Next);
    Next += MaxBodies * sizeof(INT);
    Output->BodiesSize = reinterpret_cast<INT*>(Next);
    Next += MaxBodies * sizeof(INT);
    Output->ConstraintsStart = reinterpret_cast<INT*>(Next);
    Next += MaxBodies * sizeof(INT);
    Output->ConstraintsSize = reinterpret_cast<INT*>(Next);
    Next += MaxBodies * sizeof(INT);
    Output->Info = reinterpret_cast<MdtPartitionInfo*>(Next);
    Output->MaxPartitions = MaxBodies;
    Output->MaxBodies = MaxBodies;
    Output->MaxConstraints = MaxConstraints;
    Output->NumPartitions = 0;
    Output->TotalBodies = 0;
    Output->TotalConstraints = 0;
    return Output;
}

// 0x00192540
void MdtWorldSetUnknown22C(MdtWorld* World, UBOOL Value)
{
    World->Unknown22C = Value;
}

// ---------------------------------------------------------------------------------------------- contact params

// 0x0018aa40
void MdtContactParamsReset(MdtContactParams* Params)
{
    Params->Type = 0;
    Params->PrimaryFrictionModel = 0;
    Params->SecondaryFrictionModel = 0;
    Params->Options = 0;
    Params->Restitution = 0.f;
    Params->VelocityThreshold = 0.001f;
    Params->Softness = 0.f;
    Params->Unknown1C = 0.f;
    Params->PrimaryFriction = FLT_MAX;
    Params->PrimarySlip = 0.f;
    Params->Unknown28 = 0.f;
    Params->SecondaryFriction = FLT_MAX;
    Params->SecondarySlip = 0.f;
    Params->Unknown34 = 0.f;
    Params->Unknown38 = 0.f;
    Params->Unknown3C = 0.f;
}

// 0x0018aa90
void MdtContactParamsSetType(MdtContactParams* Params, INT Type)
{
    if (Type == 1 || Type == 2)
    {
        Params->Type = Type;
        return;
    }
    if (Type != 0)
        MeWarning(12, "Unknown Contact Type, defaulting to zero friction.");
    Params->Type = 0;
}

// 0x0018aae0
void MdtContactParamsSetFrictionModel(MdtContactParams* Params, INT Model)
{
    if (Model != 0 && Model != 1)
        MeWarning(12, "Unknown Friction Model, defaulting to box friction.");
    Params->PrimaryFrictionModel = Model == 1 ? 1 : 0;
    if (Model != 0 && Model != 1)
        MeWarning(12, "Unknown Friction Model, defaulting to box friction.");
    Params->SecondaryFrictionModel = Model == 1 ? 1 : 0;
}

// 0x0018abb0
void MdtContactParamsSetSoftness(MdtContactParams* Params, FLOAT Softness)
{
    if (Softness < 0.f)
    {
        MeWarning(12, "MdtContactParamsSetSoftness: Negative softness not valid - setting to zero.");
        Softness = 0.f;
    }
    Params->Softness = Softness;
    if (fabsf(Softness) >= 1e-6f)
        Params->Options |= 4;
    else
        Params->Options &= ~4;
}

// 0x0018ad30
void MdtContactParamsSetFriction(MdtContactParams* Params, FLOAT Friction)
{
    if (Friction < 0.f)
    {
        MeWarning(12, "MdtContactParamsSetFriction: Negative friction not valid - setting to zero.");
        Friction = 0.f;
    }
    Params->PrimaryFriction = Friction;
    Params->SecondaryFriction = Friction;
}

// 0x0018ad70. sets both slips despite the name in its warning.
void MdtContactParamsSetPrimarySlip(MdtContactParams* Params, FLOAT Slip)
{
    if (Slip < 0.f)
    {
        MeWarning(12, "MdtContactParamsSetPrimarySlip: Negative friction not valid - setting to zero.");
        Slip = 0.f;
    }
    Params->PrimarySlip = Slip;
    Params->SecondarySlip = Slip;
}
