#include "Core.h"

INT GPackageFileVersion         = 159;
INT GPackageFileLicenseeVersion = 1;
INT GEngineNegotiationVersion   = 2000;

// 0x00011127
FArchive::FArchive()
    : ArVer(GPackageFileVersion)
    , ArNetVer(GEngineNegotiationVersion)
    , ArLicenseeVer(GPackageFileLicenseeVersion)
    , ArIsLoading(false)
    , ArIsSaving(false)
    , ArUnknown12(false)
    , ArIsCountingMem(false)
    , ArIsTrans(false)
    , ArIsPersistent(false)
    , ArUnknown16(false)
    , ArSkipObjectRefs(false)
    , ArForEdit(true)
    , ArForClient(true)
    , ArForServer(true)
    , ArIsError(false)
    , ArIsCriticalError(false)
    , ArContainsCode(false)
{
}

// 0x00011093
void FArchive::SerializeBits(void* V, INT LengthBits)
{
    Serialize(V, (LengthBits + 7) / 8);

    // clear the unused bits of the last byte
    if (IsLoading())
        static_cast<BYTE*>(V)[LengthBits / 8] &= (1 << (LengthBits & 7)) - 1;
}

// 0x000110dd
void FArchive::SerializeInt(DWORD& Value, DWORD Max)
{
    Serialize(&Value, sizeof(Value));
}

// 0x00034edf
void FArchive::SerializeText(const TCHAR* Text)
{
    Serialize(const_cast<TCHAR*>(Text), appStrlen(Text));
}

// 0x00036a08
void FArchive::SerializeText(const FString& Text)
{
    Serialize(const_cast<TCHAR*>(*Text), Text.Len());
}

// 0x000110f4
UBOOL FArchive::AtEnd()
{
    const INT Pos = Tell();
    return Pos != INDEX_NONE && Pos >= TotalSize();
}

// 0x00023454
FArchive& operator<<(FArchive& Ar, FCompactIndex& I)
{
    // archives that neither load nor save (counters, finders) just see the raw value
    if (!Ar.IsLoading() && !Ar.IsSaving())
    {
        Ar.Serialize(&I.Value, sizeof(I.Value));
        return Ar;
    }

    const INT Original = I.Value;
    DWORD V = Original < 0 ? -Original : Original;

    BYTE B0 = (Original < 0 ? 0x80 : 0) + (V < 0x40 ? V : (V & 0x3F) + 0x40);
    I.Value = 0;
    Ar << B0;
    if (B0 & 0x40)
    {
        V >>= 6;
        BYTE B1 = V < 0x80 ? V : (V & 0x7F) + 0x80;
        Ar << B1;
        if (B1 & 0x80)
        {
            V >>= 7;
            BYTE B2 = V < 0x80 ? V : (V & 0x7F) + 0x80;
            Ar << B2;
            if (B2 & 0x80)
            {
                V >>= 7;
                BYTE B3 = V < 0x80 ? V : (V & 0x7F) + 0x80;
                Ar << B3;
                if (B3 & 0x80)
                {
                    V >>= 7;
                    BYTE B4 = V;
                    Ar << B4;
                    I.Value = B4;
                }
                I.Value = (I.Value << 7) + (B3 & 0x7F);
            }
            I.Value = (I.Value << 7) + (B2 & 0x7F);
        }
        I.Value = (I.Value << 7) + (B1 & 0x7F);
    }
    I.Value = (I.Value << 6) + (B0 & 0x3F);
    if (B0 & 0x80)
        I.Value = -I.Value;

    // saving decodes what it wrote as a self check
    if (Ar.IsSaving() && I.Value != Original)
        appErrorf(TEXT("Mismatch: %08X %08X"), I.Value, Original);
    return Ar;
}
