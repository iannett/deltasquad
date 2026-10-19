#include "EngineClasses.h"

UClient* UTexture::__Client = nullptr;

// ---------------------------------------------------------------------------------------------- materials

// 0x0009dcc0
void UMaterial::Serialize(FArchive& Ar)
{
    if (!(GUglyHackFlags & 8))
        Super::Serialize(Ar);
    // the cached passes are pointers kept in script ints
    if (Ar.IsCountingMem())
    {
        for (INT i = 0; i < static_cast<INT>(ARRAY_COUNT(CachedMaterialPasses)); i++)
        {
            if (CachedMaterialPasses[i])
                Ar.CountBytes(reinterpret_cast<void*>(static_cast<PTRINT>(CachedMaterialPasses[i])), 0);
        }
    }
}

// 0x0005c8ad
void UBitmapMaterial::Serialize(FArchive& Ar)
{
    Super::Serialize(Ar);
    // counts the render resource's memory, which only some archives ask for
    if (Ar.GetResourceObject())
        NOT_DECOMPILED(0x0005c8ad);
}

// ---------------------------------------------------------------------------------------------- palettes

// 0x0005f317
void UPalette::Serialize(FArchive& Ar)
{
    Super::Serialize(Ar);
    Ar << Colors;
    // palettes before version 66 have no alpha
    if (Ar.Ver() < 66)
    {
        for (INT i = 0; i < Colors.Num(); i++)
            Colors[i].A = 255;
    }
}

// ---------------------------------------------------------------------------------------------- textures

// 0x0005f439
FArchive& operator<<(FArchive& Ar, FMipmap& M)
{
    M.DataArray.Serialize(Ar);
    Ar << M.USize << M.VSize << M.UBits << M.VBits;
    return Ar;
}

// 0x0005f7d1. mips below the texture's first used mip stay in the file. with no client, none load yet.
static void SerializeMips(UTexture* Texture, FArchive& Ar, TArray<FMipmap>& Mips)
{
    Mips.CountBytes(Ar);
    if (!Ar.IsLoading())
    {
        INT Num = Mips.Num();
        Ar << AR_INDEX(Num);
        for (INT i = 0; i < Mips.Num(); i++)
            Ar << Mips[i];
        return;
    }

    INT NewNum;
    Ar << AR_INDEX(NewNum);
    // entries already there are constructed over without being destroyed, as on the xbox
    if (NewNum < Mips.Num())
        Mips.Remove(NewNum, Mips.Num() - NewNum);
    else
        Mips.AddZeroed(NewNum - Mips.Num());

    const INT FirstMip = Texture->DefaultLOD();
    for (INT i = 0; i < NewNum; i++)
    {
        const UBOOL SavedLazyLoad = GLazyLoad;
        if (GUglyHackFlags & 8)
            GLazyLoad = 0;
        else
            GLazyLoad = !UTexture::__Client || i < FirstMip;
        new (&Mips[i]) FMipmap;
        Ar << Mips[i];
        GLazyLoad = SavedLazyLoad;
    }
}

// 0x0005f8d1
void UTexture::Serialize(FArchive& Ar)
{
    Super::Serialize(Ar);

    // parametric textures are generated, so their pixels are never saved or loaded
    if ((Ar.IsSaving() || Ar.IsLoading()) && bParametric)
    {
        for (INT i = 0; i < Mips.Num(); i++)
            Mips[i].DataArray.Empty();
    }

    // the xbox reads and drops a second mip array first in packages before version 84 when bit 10 of the bool
    // word is set. no script bool is bit 10 and nothing else sets it, so that never happens.
    SerializeMips(this, Ar, Mips);

    if ((Ar.IsSaving() || Ar.IsLoading()) && bParametric)
    {
        for (INT i = 0; i < Mips.Num(); i++)
        {
            const INT Size = Mips[i].USize * Mips[i].VSize;
            Mips[i].DataArray.Empty(Size);
            Mips[i].DataArray.AddZeroed(Size);
        }
    }
}

// 0x0005c92b. the client's detail setting raises the first mip, but never below 64 texels on the longer side.
INT UTexture::DefaultLOD()
{
    INT LOD = NormalLOD;
    if (__Client)
        LOD += __Client->GetTextureLODBias(LODSet);
    if (LOD > Mips.Num() - 1)
        LOD = Mips.Num() - 1;
    const INT LongSide = USize > VSize ? USize : VSize;
    while (LOD > 0 && (LongSide >> LOD) < 64)
        LOD--;
    if (LOD < MinLOD)
        LOD = MinLOD;
    return LOD;
}

// ---------------------------------------------------------------------------------------------- client

// 0x0007c29b
INT UClient::GetTextureLODBias(BYTE LODSet)
{
    NOT_DECOMPILED(0x0007c29b);
    return 0;
}
