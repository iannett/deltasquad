#include "Core.h"
#include "Karma.h"

namespace
{
    // 0x001b3400. reports an internal collision error from the table at 0x004327e0.
    void McdError(INT Code, const char* Detail, const char* Function, const char* File, INT Line)
    {
        NOT_DECOMPILED(0x001b3400);
    }

    // 0x001b33f0
    const char* McdGetVersion()
    {
        return "1.2.1 (build 631)";
    }
}

// 0x0018ca50
McdFramework* McdInit(INT GeometryTypeMaxCount, INT ModelCount, INT Unknown3, FLOAT Scale)
{
    McdFramework* Framework = static_cast<McdFramework*>(MeMemory.Create(sizeof(McdFramework)));
    Framework->Hooks = nullptr;
    Framework->Version = McdGetVersion();
    // nine built-in types come first
    Framework->GeometryTypeCount = GeometryTypeMaxCount + 9;
    Framework->RegisteredTypes = 0;
    Framework->Types = static_cast<McdGeometryType*>(MeMemory.Create(Framework->GeometryTypeCount * sizeof(McdGeometryType)));
    memset(Framework->Types, 0, Framework->GeometryTypeCount * sizeof(McdGeometryType));
    MePools.Init(&Framework->ModelPool, ModelCount, 0x78, 0);
    MePools.Init(&Framework->Unknown60, Unknown3, 0x34, 0);
    const INT InteractionCount = Framework->GeometryTypeCount * Framework->GeometryTypeCount;
    Framework->Interactions = static_cast<McdInteraction*>(MeMemory.Create(InteractionCount * sizeof(McdInteraction)));
    memset(Framework->Interactions, 0, InteractionCount * sizeof(McdInteraction));
    Framework->Unknown40 = 0;
    Framework->Unknown24 = 0;
    Framework->Unknown84 = static_cast<INT*>(MeMemory.Create(2 * sizeof(INT)));
    Framework->Unknown84[0] = 4;
    Framework->Unknown84[1] = 0;
    Framework->Unknown7C = Scale * 0.005f;
    Framework->Unknown88 = Framework->Unknown84;
    Framework->Geometries = nullptr;
    Framework->Unknown8 = 0;
    Framework->Unknown10 = 0;
    Framework->GeometryCount = 0;
    Framework->Scale = Scale;
    return Framework;
}

// 0x0018cc50
void McdGeometryRegisterType(McdFramework* Framework, SWORD TypeId, const char* Name, void (*Destroy)(void*),
    void (*GetAABB)(void*, void*, void*), void (*GetBSphere)(void*, void*, void*), void (*MaximumPoint)(void*, void*, void*),
    SWORD (*GetMassProperties)(void*, void*, void*, void*), void (*DebugDraw)(void*, void*, void*))
{
    if (Framework->GeometryTypeCount <= TypeId)
        McdError(0x15, " in McdGeometryRegisterType.", "McdGeometryRegisterType", "McdFrame.cpp", 399);
    McdGeometryType& Type = Framework->Types[TypeId];
    Type.Registered = 1;
    Type.Destroy = Destroy;
    Type.GetAABB = GetAABB;
    Type.GetBSphere = GetBSphere;
    Type.MaximumPoint = MaximumPoint;
    Type.GetMassProperties = GetMassProperties;
    Type.DebugDraw = DebugDraw;
    Type.Name = Name;
    Framework->RegisteredTypes++;
}

// 0x0018cdc0
void McdFrameworkSetTypeFunction(McdFramework* Framework, INT TypeId, void (*Fn)())
{
    Framework->Types[TypeId].Unknown1C = Fn;
}

// 0x0018ccf0. the reversed pair gets the same entry marked as swapped.
void McdFrameworkSetInteraction(McdFramework* Framework, INT TypeA, INT TypeB, const McdInteraction* Interaction)
{
    McdInteraction& Entry = Framework->Interactions[Framework->GeometryTypeCount * TypeB + TypeA];
    Entry = *Interaction;
    Entry.Swapped = 0;
    if (TypeA != TypeB)
    {
        McdInteraction& Reversed = Framework->Interactions[Framework->GeometryTypeCount * TypeA + TypeB];
        Reversed = *Interaction;
        Reversed.Swapped = 1;
    }
}

// 0x0018cbf0
void McdFrameworkAddHook(McdFramework* Framework, void (*Fn)())
{
    McdFrameworkHook* Hook = static_cast<McdFrameworkHook*>(MeMemory.Create(sizeof(McdFrameworkHook)));
    Hook->Fn = Fn;
    Hook->Next = Framework->Hooks;
    Framework->Hooks = Hook;
}
