#pragma once

#include "UnObjBas.h"

// an array of bits
class UBitArray : public UObject
{
    DECLARE_CLASS(UBitArray, UObject, 0, UObject)
public:
    void Serialize(FArchive& Ar) override;

    // 0x000a6086
    UBOOL Get(DWORD Index) const { return (Unknown28[Index >> 5] & (1 << (Index & 31))) != 0; }

    TArray<DWORD> Unknown28;
    INT           Unknown30;
};

class UBitArray2D : public UBitArray
{
    DECLARE_CLASS(UBitArray2D, UBitArray, 0, UObject)
public:
    void Serialize(FArchive& Ar) override;

    // 0x000a60ac. logs each row as a line of 0s and 1s.
    void DebugPrint();

    INT Unknown34;
};
