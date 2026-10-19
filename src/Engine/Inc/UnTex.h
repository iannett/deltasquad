#pragma once

#include "Core.h"

// one mip level of a texture
struct FMipmapBase
{
    FMipmapBase() {}

    BYTE* DataPtr;
    INT   USize;
    INT   VSize;
    BYTE  UBits;
    BYTE  VBits;
};

// a mip level with its pixels, which stay in the package file until loaded
struct FMipmap : public FMipmapBase
{
    TLazyArray<BYTE> DataArray;
};

FArchive& operator<<(FArchive& Ar, FMipmap& M);
