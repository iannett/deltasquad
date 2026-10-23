#include "Core.h"
#include "Karma.h"

namespace
{
    // 0x0007d4db (name invented)
    MstBridgeBuffers* MstBridgeBuffersCreate(McdFramework* Framework)
    {
        MstBridgeBuffers* Buffers = static_cast<MstBridgeBuffers*>(MeMemory.Create(sizeof(MstBridgeBuffers)));
        Buffers->Framework = Framework;
        Buffers->TypeCount = Framework->GeometryTypeCount;
        Buffers->Unknown10 = MeMemory.CreateAligned(64000, 0x10);
        Buffers->Unknown14 = MeMemory.Create(4000);
        Buffers->Unknown18 = 1000;
        Buffers->PairTable = static_cast<INT*>(MeMemory.Create(Buffers->TypeCount * Buffers->TypeCount * static_cast<INT>(sizeof(INT))));
        Buffers->Unknown20 = MeMemory.Create(0x6000);
        Buffers->Unknown24 = 0x100;
        Buffers->Unknown2C = MeMemory.Create(20000);
        Buffers->Unknown30 = 1000;
        Buffers->Unknown38 = static_cast<MstBridgeBuffer*>(MeMemory.Create(17 * sizeof(MstBridgeBuffer)));
        Buffers->Unknown40 = 17;
        for (INT i = 0; i < 17; i++)
        {
            Buffers->Unknown38[i].Data = MeMemory.Create(0x2800);
            Buffers->Unknown38[i].Max = 0x100;
        }
        return Buffers;
    }

    // a pair of materials in either order picks one entry of the triangle
    MstMaterialPair* MstBridgeGetPair(MstBridge* Bridge, DWORD Material1, DWORD Material2)
    {
        const DWORD Max = static_cast<DWORD>(Bridge->MaxMaterials);
        if (Material1 > Max || Material2 > Max)
            return nullptr;
        const DWORD Low = Material1 < Material2 ? Material1 : Material2;
        const DWORD High = Material1 < Material2 ? Material2 : Material1;
        return &Bridge->Pairs[High - static_cast<INT>((Low + 1) * Low) / 2 + Max * Low];
    }
}

// 0x0007f262
MstBridge* MstBridgeCreate(McdFramework* Framework, INT MaxMaterials)
{
    MstBridge* Bridge = static_cast<MstBridge*>(MeMemory.Create(sizeof(MstBridge)));
    if (!Bridge)
    {
        MeWarning(0, "MstBridgeCreate: Could not allocate memory for bridge.");
        return nullptr;
    }
    INT PairCount = (MaxMaterials + 1) * MaxMaterials / 2;
    Bridge->MaxMaterials = MaxMaterials;
    Bridge->Pairs = static_cast<MstMaterialPair*>(MeMemory.Create(PairCount * static_cast<INT>(sizeof(MstMaterialPair))));
    for (MstMaterialPair* Pair = Bridge->Pairs; PairCount; PairCount--, Pair++)
    {
        MdtContactParamsReset(&Pair->Params);
        Pair->IntersectCB = nullptr;
        Pair->PerPairCB = nullptr;
        Pair->PerContactCB = nullptr;
    }
    Bridge->MaterialCount = 1;
    Bridge->Unknown0C = MeUnknown1CCreate(50);
    Bridge->Buffers = MstBridgeBuffersCreate(Framework);
    Bridge->Unknown18 = 50;
    Bridge->Unknown14 = MeMemory.Create(2000);
    return Bridge;
}

// 0x0018d700
void MstBridgeSetUnknown0C(MstBridge* Bridge, INT Max)
{
    MeUnknown1CDestroy(Bridge->Unknown0C);
    Bridge->Unknown0C = MeUnknown1CCreate(Max);
}

// 0x0018d490
MdtContactParams* MstBridgeGetContactParams(MstBridge* Bridge, DWORD Material1, DWORD Material2)
{
    MstMaterialPair* Pair = MstBridgeGetPair(Bridge, Material1, Material2);
    return Pair ? &Pair->Params : nullptr;
}

// 0x0018d6a0
void MstBridgeSetIntersectCB(MstBridge* Bridge, DWORD Material1, DWORD Material2, INT (*Fn)(void* Unknown1, void* Unknown2, void* Unknown3))
{
    if (MstMaterialPair* Pair = MstBridgeGetPair(Bridge, Material1, Material2))
        Pair->IntersectCB = Fn;
}

// 0x0018d640
void MstBridgeSetPerContactCB(MstBridge* Bridge, DWORD Material1, DWORD Material2, INT (*Fn)(void* Unknown1, void* Unknown2))
{
    if (MstMaterialPair* Pair = MstBridgeGetPair(Bridge, Material1, Material2))
        Pair->PerContactCB = Fn;
}
