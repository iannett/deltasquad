#pragma once

#include <cstring>
#include <new>
#include <type_traits>

#include "CoreTypes.h"
#include "UnMem.h"

// for members of native classes that the object system fills in before the c++ constructor runs
enum ENoInit { E_NoInit };

// a dynamic array of elements whose size is only known at runtime, as script arrays are
class FArray
{
public:
    FArray() : Data(nullptr), ArrayNum(0), ArrayMax(0) {}
    explicit FArray(ENoInit) {}
    FArray(const FArray&) = delete;
    FArray& operator=(const FArray&) = delete;
    // 0x0002d8fc for strings. leaves an empty array, so destroying twice is harmless, as the engine relies on.
    ~FArray()
    {
        appFree(Data);
        Data = nullptr;
        ArrayNum = 0;
        ArrayMax = 0;
    }

    INT Num() const { return ArrayNum; }
    INT GetMax() const { return ArrayMax; }
    void* GetData() { return Data; }
    const void* GetData() const { return Data; }

    // adds Count uninitialized elements and returns the index of the first
    INT Add(INT Count, INT ElementSize)
    {
        const INT Index = ArrayNum;
        ArrayNum += Count;
        if (ArrayNum > ArrayMax)
        {
            ArrayMax = ArrayNum + 3 * ArrayNum / 8 + 32;
            Data = appRealloc(Data, static_cast<size_t>(ArrayMax) * ElementSize);
        }
        return Index;
    }

    INT AddZeroed(INT Count, INT ElementSize)
    {
        const INT Index = Add(Count, ElementSize);
        memset(static_cast<BYTE*>(Data) + static_cast<size_t>(Index) * ElementSize, 0, static_cast<size_t>(Count) * ElementSize);
        return Index;
    }

    // empties the array and reserves room for Slack elements
    void Empty(INT ElementSize, INT Slack = 0)
    {
        ArrayNum = 0;
        ArrayMax = Slack;
        Data = appRealloc(Data, static_cast<size_t>(ArrayMax) * ElementSize);
    }

    void Remove(INT Index, INT Count, INT ElementSize)
    {
        memmove(static_cast<BYTE*>(Data) + static_cast<size_t>(Index) * ElementSize,
            static_cast<BYTE*>(Data) + static_cast<size_t>(Index + Count) * ElementSize,
            static_cast<size_t>(ArrayNum - Index - Count) * ElementSize);
        ArrayNum -= Count;
    }

    // 0x0002304f. opens a gap of Count uninitialized elements at Index.
    void Insert(INT Index, INT Count, INT ElementSize)
    {
        const INT OldNum = ArrayNum;
        Add(Count, ElementSize);
        memmove(static_cast<BYTE*>(Data) + static_cast<size_t>(Index + Count) * ElementSize,
            static_cast<BYTE*>(Data) + static_cast<size_t>(Index) * ElementSize,
            static_cast<size_t>(OldNum - Index) * ElementSize);
    }

    // 0x0002309e
    void InsertZeroed(INT Index, INT Count, INT ElementSize)
    {
        Insert(Index, Count, ElementSize);
        memset(static_cast<BYTE*>(Data) + static_cast<size_t>(Index) * ElementSize, 0, static_cast<size_t>(Count) * ElementSize);
    }

    // frees the unused slack
    void Shrink(INT ElementSize)
    {
        if (ArrayMax != ArrayNum)
        {
            ArrayMax = ArrayNum;
            Data = appRealloc(Data, static_cast<size_t>(ArrayMax) * ElementSize);
        }
    }

protected:
    void* Data;
    INT   ArrayNum;
    INT   ArrayMax;
};

// dynamic array. elements are moved around as raw bytes, which every engine type allows,
// but constructed and destroyed properly when added and removed.
template<typename T>
class TArray : public FArray
{
public:
    TArray() = default;
    explicit TArray(ENoInit) : FArray(E_NoInit) {}
    ~TArray() { DestroyRange(0, ArrayNum); }

    T* GetData() { return static_cast<T*>(Data); }
    const T* GetData() const { return static_cast<const T*>(Data); }

    T& operator[](INT Index) { return GetData()[Index]; }
    const T& operator[](INT Index) const { return GetData()[Index]; }
    T& Last() { return GetData()[ArrayNum - 1]; }

    // element by element, as the per-type copies on the xbox (0x00089408 for class replication records)
    TArray& operator=(const TArray& Other)
    {
        if (this != &Other)
        {
            Empty(Other.ArrayNum);
            for (INT i = 0; i < Other.ArrayNum; i++)
                AddItem(Other[i]);
        }
        return *this;
    }

    void Empty(INT Slack = 0)
    {
        DestroyRange(0, ArrayNum);
        FArray::Empty(sizeof(T), Slack);
    }
    INT Add(INT Count = 1) { return FArray::Add(Count, sizeof(T)); }
    INT AddZeroed(INT Count = 1) { return FArray::AddZeroed(Count, sizeof(T)); }
    void InsertZeroed(INT Index, INT Count = 1) { FArray::InsertZeroed(Index, Count, sizeof(T)); }
    void Shrink() { FArray::Shrink(sizeof(T)); }
    void Remove(INT Index, INT Count = 1)
    {
        DestroyRange(Index, Count);
        FArray::Remove(Index, Count, sizeof(T));
    }

    // 0x00047d34 and its copies for each element type: resizes to Num, zeroing added elements
    void Set(INT Num)
    {
        if (Num < ArrayNum)
            Remove(Num, ArrayNum - Num);
        else
            AddZeroed(Num - ArrayNum);
    }

    INT AddItem(const T& Item)
    {
        const INT Index = Add();
        new (&GetData()[Index]) T(Item);
        return Index;
    }

    // 0x000fa0c0 and its copies for each element type: removes every match and returns how many
    INT RemoveItem(const T& Item)
    {
        const INT OriginalNum = ArrayNum;
        for (INT Index = 0; Index < ArrayNum; Index++)
        {
            if (GetData()[Index] == Item)
                Remove(Index--);
        }
        return OriginalNum - ArrayNum;
    }

    // 0x00066711 and its copies for each element type
    UBOOL FindItem(const T& Item, INT& Index) const
    {
        for (Index = 0; Index < ArrayNum; Index++)
        {
            if (GetData()[Index] == Item)
                return 1;
        }
        return 0;
    }

    void CountBytes(class FArchive& Ar);

    T Pop()
    {
        T Item = Last();
        Remove(ArrayNum - 1);
        return Item;
    }

private:
    void DestroyRange(INT Index, INT Count)
    {
        if constexpr (!std::is_trivially_destructible_v<T>)
        {
            for (INT i = Index; i < Index + Count; i++)
                GetData()[i].~T();
        }
    }
};

class UObject;

// an array an object owns, which the editor records for undo. the owner is set by the owning object.
template<typename T>
class TTransArray : public TArray<T>
{
public:
    TTransArray() : Owner(nullptr) {}

    UObject* Owner;
};

// a TArray member left as the object system initialized it
template<typename T>
class TArrayNoInit : public TArray<T>
{
public:
    TArrayNoInit() : TArray<T>(E_NoInit) {}
};

inline DWORD GetTypeHash(INT A) { return static_cast<DWORD>(A); }
inline DWORD GetTypeHash(DWORD A) { return A; }

// hashed key to value storage shared by TMap and TMultiMap. pairs stay in insertion order; each hash
// chain starts at its newest pair, so lookups find the most recently added match first.
template<typename TK, typename TV>
class TMapBase
{
public:
    TMapBase() { Rehash(); }
    TMapBase(const TMapBase& Other) : Pairs(), HashCount(Other.HashCount)
    {
        for (INT i = 0; i < Other.Pairs.Num(); i++)
            Pairs.AddItem(Other.Pairs[i]);
        Rehash();
    }
    ~TMapBase() { delete[] Hash; }
    TMapBase& operator=(const TMapBase& Other)
    {
        if (this != &Other)
        {
            Pairs.Empty();
            for (INT i = 0; i < Other.Pairs.Num(); i++)
                Pairs.AddItem(Other.Pairs[i]);
            HashCount = Other.HashCount;
            Rehash();
        }
        return *this;
    }

    void Empty()
    {
        Pairs.Empty();
        HashCount = 8;
        Rehash();
    }

    TV* Find(const TK& Key)
    {
        for (INT i = Hash[GetTypeHash(Key) & (HashCount - 1)]; i != INDEX_NONE; i = Pairs[i].HashNext)
        {
            if (Pairs[i].Key == Key)
                return &Pairs[i].Value;
        }
        return nullptr;
    }

    // removes every pair with this key and returns how many went
    INT Remove(const TK& Key)
    {
        INT Count = 0;
        for (INT i = Pairs.Num() - 1; i >= 0; i--)
        {
            if (Pairs[i].Key == Key)
            {
                Pairs.Remove(i);
                Count++;
            }
        }
        if (Count)
            Relax();
        return Count;
    }

    // 0x00015fae and its copies for each map type
    template<typename TOutputDevice>
    void Dump(TOutputDevice& Ar)
    {
        Ar.Logf(TEXT("TMap: %i items, %i hash slots"), Pairs.Num(), HashCount);
        for (INT i = 0; i < HashCount; i++)
        {
            INT Count = 0;
            for (INT j = Hash[i]; j != INDEX_NONE; j = Pairs[j].HashNext)
                Count++;
            Ar.Logf(TEXT("   Hash[%i] = %i"), i, Count);
        }
    }

    // the pairs as an array; the hash is rebuilt on load
    template<typename TArchive>
    friend TArchive& operator<<(TArchive& Ar, TMapBase& Map)
    {
        Ar << Map.Pairs;
        if (Ar.IsLoading())
            Map.Rehash();
        Ar.CountBytes(Map.Hash, Map.HashCount * sizeof(INT));
        return Ar;
    }

    INT Num() const { return Pairs.Num(); }
    const TK& KeyAt(INT i) const { return Pairs[i].Key; }
    TV& ValueAt(INT i) { return Pairs[i].Value; }

protected:
    struct TPair
    {
        INT HashNext;
        TK  Key;
        TV  Value;

        template<typename TArchive>
        friend TArchive& operator<<(TArchive& Ar, TPair& Pair)
        {
            return Ar << Pair.Key << Pair.Value;
        }
    };

    TV& Add(const TK& Key, const TV& Value)
    {
        const INT Index = Pairs.AddItem({ INDEX_NONE, Key, Value });
        const INT iHash = GetTypeHash(Key) & (HashCount - 1);
        Pairs[Index].HashNext = Hash[iHash];
        Hash[iHash] = Index;
        if (HashCount * 2 + 8 < Pairs.Num())
        {
            HashCount *= 2;
            Rehash();
        }
        return Pairs[Index].Value;
    }

    void Rehash()
    {
        delete[] Hash;
        Hash = new INT[HashCount];
        for (INT i = 0; i < HashCount; i++)
            Hash[i] = INDEX_NONE;
        for (INT i = 0; i < Pairs.Num(); i++)
        {
            const INT iHash = GetTypeHash(Pairs[i].Key) & (HashCount - 1);
            Pairs[i].HashNext = Hash[iHash];
            Hash[iHash] = i;
        }
    }

    void Relax()
    {
        while (HashCount > Pairs.Num() * 2 + 8)
            HashCount /= 2;
        Rehash();
    }

    TArray<TPair> Pairs;
    INT*          Hash = nullptr;
    INT           HashCount = 8;
};

// one value per key
template<typename TK, typename TV>
class TMap : public TMapBase<TK, TV>
{
public:
    // 0x000260e2 and its copies for each key type
    TV& Set(const TK& Key, const TV& Value)
    {
        if (TV* Existing = this->Find(Key))
            return *Existing = Value;
        return this->Add(Key, Value);
    }
};

// any number of values per key
template<typename TK, typename TV>
class TMultiMap : public TMapBase<TK, TV>
{
public:
    using TMapBase<TK, TV>::Add;

    // every value of Key, newest first
    void MultiFind(const TK& Key, TArray<TV>& Values)
    {
        for (INT i = this->Hash[GetTypeHash(Key) & (this->HashCount - 1)]; i != INDEX_NONE; i = this->Pairs[i].HashNext)
        {
            if (this->Pairs[i].Key == Key)
                Values.AddItem(this->Pairs[i].Value);
        }
    }
};

// the engine's sort, inlined per element type (0x0009a057 for class net fields). Compare(A, B) is negative,
// zero or positive. a quicksort with an explicit stack that finishes short runs with a selection sort.
template<typename T, typename TCompare>
void Sort(T* First, INT Num, TCompare Compare)
{
    if (Num < 2)
        return;

    struct FRange { T* Min; T* Max; };
    FRange Stack[32] = {};
    FRange* Top = Stack;
    Top->Min = First;
    Top->Max = First + Num - 1;

    for (; Top >= Stack; --Top)
    {
        T* Min = Top->Min;
        T* Max = Top->Max;
        for (;;)
        {
            const INT Count = static_cast<INT>(Max - Min) + 1;
            if (Count <= 8)
            {
                // move the largest remaining item to the end each pass
                while (Max > Min)
                {
                    T* ItemMax = Min;
                    for (T* Item = Min + 1; Item <= Max; Item++)
                    {
                        if (Compare(*Item, *ItemMax) > 0)
                            ItemMax = Item;
                    }
                    T Temp = *ItemMax; *ItemMax = *Max; *Max = Temp;
                    Max--;
                }
                break;
            }

            { T Temp = Min[Count / 2]; Min[Count / 2] = Min[0]; Min[0] = Temp; }
            T* Inner1 = Min;
            T* Inner2 = Max + 1;
            for (;;)
            {
                while (++Inner1 <= Max && Compare(*Inner1, *Min) <= 0);
                while (--Inner2 > Min && Compare(*Inner2, *Min) >= 0);
                if (Inner1 > Inner2)
                    break;
                T Temp = *Inner1; *Inner1 = *Inner2; *Inner2 = Temp;
            }
            { T Temp = *Min; *Min = *Inner2; *Inner2 = Temp; }

            // push the larger half and keep working on the smaller one
            if (Inner2 - 1 - Min >= Max - Inner1)
            {
                if (Min + 1 < Inner2)
                {
                    Top->Min = Min;
                    Top->Max = Inner2 - 1;
                    Top++;
                }
                if (Max <= Inner1)
                    break;
                Min = Inner1;
            }
            else
            {
                if (Max > Inner1)
                {
                    Top->Min = Inner1;
                    Top->Max = Max;
                    Top++;
                }
                if (Min + 1 >= Inner2)
                    break;
                Max = Inner2 - 1;
            }
        }
    }
}
