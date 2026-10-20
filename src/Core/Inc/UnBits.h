#pragma once

#include "UnObjBas.h"

// an array of bits
class UBitArray : public UObject
{
    DECLARE_CLASS(UBitArray, UObject, 0, UObject)
public:
    void Serialize(FArchive& Ar) override;

    TArray<DWORD> Unknown28;
    INT           Unknown30;
};

class UBitArray2D : public UBitArray
{
    DECLARE_CLASS(UBitArray2D, UBitArray, 0, UObject)
public:
    void Serialize(FArchive& Ar) override;

    INT Unknown34;
};
