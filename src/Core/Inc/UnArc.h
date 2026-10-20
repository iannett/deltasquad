#pragma once

#include "CoreTypes.h"
#include "UnString.h"

class FName;
class FLazyLoader;
class UObject;


extern INT GPackageFileVersion;
extern INT GPackageFileLicenseeVersion;
extern INT GEngineNegotiationVersion;

// memory usage totals reported by FArchive::CountBytes
struct FMemCount
{
    DWORD Num;
    DWORD Max;
    DWORD Slack;
};

// base class for anything that reads or writes serialized data. does nothing by itself.
class FArchive
{
public:
    FArchive();
    virtual ~FArchive() = default;

    virtual void Serialize(void* V, INT Length) {}
    virtual void SerializeBits(void* V, INT LengthBits);
    virtual void SerializeInt(DWORD& Value, DWORD Max);
    virtual void SerializeText(const TCHAR* Text);
    virtual void SerializeText(const FString& Text);
    virtual void Preload(UObject* Object) {}
    virtual void CountBytes(char Category, const FMemCount& Count) {}
    virtual void CountBytes(void* Data, DWORD Max) {}
    virtual FArchive& operator<<(FName& Name) { return *this; }
    virtual FArchive& operator<<(UObject*& Object) { return *this; }
    virtual INT MapName(FName* Name) { return 0; }
    virtual INT MapObject(UObject* Object) { return 0; }
    virtual INT Tell() { return INDEX_NONE; }
    virtual INT TotalSize() { return INDEX_NONE; }
    virtual UBOOL AtEnd();
    virtual void Seek(INT Pos) {}
    virtual void AttachLazyLoader(FLazyLoader* Loader) {}
    virtual void DetachLazyLoader(FLazyLoader* Loader) {}
    virtual void Precache(INT Size) {}
    virtual void Flush() {}
    virtual UBOOL Close() { return !ArIsError; }
    virtual UBOOL GetError() { return ArIsError; }
    virtual INT GetMaxSerializeSize() { return 0; }
    virtual UObject* GetResourceObject() { return nullptr; }

    INT Ver() const { return ArVer; }
    INT NetVer() const { return ArNetVer; }
    INT LicenseeVer() const { return ArLicenseeVer; }
    UBOOL IsLoading() const { return ArIsLoading; }
    UBOOL IsSaving() const { return ArIsSaving; }
    UBOOL IsTrans() const { return ArIsTrans; }
    UBOOL IsPersistent() const { return ArIsPersistent; }
    UBOOL IsCountingMem() const { return ArIsCountingMem; }
    UBOOL ForEdit() const { return ArForEdit; }
    UBOOL ForClient() const { return ArForClient; }
    UBOOL ForServer() const { return ArForServer; }
    UBOOL IsError() const { return ArIsError; }
    UBOOL IsCriticalError() const { return ArIsCriticalError; }
    UBOOL ContainsCode() const { return ArContainsCode; }
    void ThisContainsCode() { ArContainsCode = 1; }

    // plain byte copies; the xbox and every supported target are little endian
    FArchive& ByteOrderSerialize(void* V, INT Length)
    {
        Serialize(V, Length);
        return *this;
    }

    friend FArchive& operator<<(FArchive& Ar, BYTE& B)   { Ar.Serialize(&B, 1); return Ar; }
    friend FArchive& operator<<(FArchive& Ar, SBYTE& B)  { Ar.Serialize(&B, 1); return Ar; }
    friend FArchive& operator<<(FArchive& Ar, _WORD& W)  { return Ar.ByteOrderSerialize(&W, sizeof(W)); }
    friend FArchive& operator<<(FArchive& Ar, SWORD& W)  { return Ar.ByteOrderSerialize(&W, sizeof(W)); }
    friend FArchive& operator<<(FArchive& Ar, DWORD& D)  { return Ar.ByteOrderSerialize(&D, sizeof(D)); }
    friend FArchive& operator<<(FArchive& Ar, INT& I)    { return Ar.ByteOrderSerialize(&I, sizeof(I)); }
    friend FArchive& operator<<(FArchive& Ar, FLOAT& F)  { return Ar.ByteOrderSerialize(&F, sizeof(F)); }
    friend FArchive& operator<<(FArchive& Ar, QWORD& Q)  { return Ar.ByteOrderSerialize(&Q, sizeof(Q)); }
    friend FArchive& operator<<(FArchive& Ar, SQWORD& S) { return Ar.ByteOrderSerialize(&S, sizeof(S)); }

    friend FArchive& operator<<(FArchive& Ar, FString& S);

protected:
    INT  ArVer;
    INT  ArNetVer;
    INT  ArLicenseeVer;
    bool ArIsLoading;
    bool ArIsSaving;
    bool ArUnknown12;
    bool ArIsCountingMem;
    bool ArIsTrans;
public:
    // set by both linkers; string properties preload the object they name when it is set
    bool ArIsPersistent;
    // package flag 0x20 of the file being loaded or saved; UClass::Serialize checks it. meaning unknown.
    bool ArUnknown16;
    // object references read as null and name arrays skip their contents
    bool ArSkipObjectRefs;
protected:
    bool ArForEdit;
    bool ArForClient;
    bool ArForServer;
    bool ArIsError;
    bool ArIsCriticalError;
    bool ArContainsCode;
};

// an INT stored in 1 to 5 bytes: sign and 6 bits in the first byte, then 7 bits per byte
class FCompactIndex
{
public:
    INT Value;
    friend FArchive& operator<<(FArchive& Ar, FCompactIndex& I);
};

#define AR_INDEX(intref) (*reinterpret_cast<FCompactIndex*>(&(intref)))

// 0x0003f9aa and its copies for each element type
template<typename T>
void TArray<T>::CountBytes(FArchive& Ar)
{
    Ar.CountBytes(Data, ArrayNum * sizeof(T));
}

// 0x000250b0 and its copies for each element type: a compact count, then the elements
template<typename T>
FArchive& operator<<(FArchive& Ar, TArray<T>& A)
{
    A.CountBytes(Ar);
    INT Count = A.Num();
    Ar << AR_INDEX(Count);
    if (Ar.IsLoading() && Ar.ArSkipObjectRefs)
    {
        for (INT i = 0; i < Count; i++)
        {
            T Skipped;
            Ar << Skipped;
        }
        return Ar;
    }
    if (Ar.IsLoading())
    {
        A.Empty(Count);
        A.AddZeroed(Count);
    }
    for (INT i = 0; i < Count; i++)
    {
        // loaded elements are constructed in place first, as each xbox copy does
        if (Ar.IsLoading())
            new(&A[i]) T;
        Ar << A[i];
    }
    return Ar;
}
