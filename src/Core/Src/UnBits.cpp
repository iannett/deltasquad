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
