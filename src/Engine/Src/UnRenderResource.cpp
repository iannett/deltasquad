#include "EngineClasses.h"

QWORD GRenderResourceCount = 0;

// 0x0003eef4. counts the resource's memory through the render device, which only some archives ask for.
void FRenderResource::Serialize(FArchive& Ar)
{
    if (Ar.GetResourceObject())
        NOT_DECOMPILED(0x0003eef4);
}
