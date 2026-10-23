#include "Core.h"

IMPLEMENT_CLASS(UBitArray, Core)
IMPLEMENT_CLASS(UBitArray2D, Core)

// 0x00025a88
void UBitArray::Serialize(FArchive& Ar)
{
    Super::Serialize(Ar);
    Ar << AR_INDEX(Unknown30) << Unknown28;
}

// 0x00026ede
void UBitArray2D::Serialize(FArchive& Ar)
{
    Super::Serialize(Ar);
    Ar << AR_INDEX(Unknown34);
}

void UBitArray2D::DebugPrint()
{
    TCHAR Line[256];
    for (DWORD Row = 0; Row < static_cast<DWORD>(Unknown34); Row++)
    {
        DWORD Col;
        for (Col = 0; Col < static_cast<DWORD>(Unknown34); Col++)
            Line[Col] = Get(Unknown34 * Row + Col) ? '1' : '0';
        Line[Col] = 0;
        GLog->Logf(TEXT("%s"), Line);
    }
}
