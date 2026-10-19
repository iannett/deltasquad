#pragma once

#include "Core.h"

// where one character sits on a font texture
struct FFontCharacter
{
    INT  StartU;
    INT  StartV;
    INT  USize;
    INT  VSize;
    BYTE TextureIndex;

    // 0x000b28b1
    friend FArchive& operator<<(FArchive& Ar, FFontCharacter& Ch)
    {
        return Ar << Ch.StartU << Ch.StartV << Ch.USize << Ch.VSize << Ch.TextureIndex;
    }
};
