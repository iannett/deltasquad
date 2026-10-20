#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "Core.h"

TCHAR GTrue[64]  = TEXT("True");
TCHAR GFalse[64] = TEXT("False");
TCHAR GNone[64]  = TEXT("None");
TCHAR GYes[64]   = TEXT("Yes");
TCHAR GNo[64]    = TEXT("No");

namespace
{
    // latin-1 letters and digits, the characters ReadToken groups into words
    UBOOL IsTokenChar(TCHAR Ch)
    {
        const BYTE C = static_cast<BYTE>(Ch);
        return (C >= 'A' && C <= 'Z') || (C >= 'a' && C <= 'z') || (C >= '0' && C <= '9')
            || (C >= 0xC0 && C <= 0xCF) || (C >= 0xD1 && C <= 0xD6) || (C >= 0xD8 && C <= 0xDD)
            || (C >= 0xE0 && C <= 0xEF) || (C >= 0xF1 && C <= 0xF6) || (C >= 0xF8 && C <= 0xFD)
            || C == 0x8A || C == 0x8E || C == 0x9A || C == 0x9E || C == 0xDE || C == 0xFE;
    }

    // 0x000b31b8
    INT HexDigit(TCHAR Ch)
    {
        if (Ch >= '0' && Ch <= '9') return Ch - '0';
        if (Ch >= 'a' && Ch <= 'f') return Ch - 'a' + 10;
        if (Ch >= 'A' && Ch <= 'F') return Ch - 'A' + 10;
        return 0;
    }

    INT Align(INT Value, INT Alignment)
    {
        return (Value + Alignment - 1) & ~(Alignment - 1);
    }

    // where Link places a property. the xbox aligns to 4, which GetLinkAlign also gives on 32-bit targets.
    INT LinkOffset(UProperty* Property)
    {
        return Align(static_cast<UField*>(Property->GetOuter())->GetPropertiesSize(), Property->GetLinkAlign());
    }
}

// 0x000b31e8
const TCHAR* ReadToken(const TCHAR* Buffer, TCHAR* Result, INT MaxLen, UBOOL Dotted)
{
    INT Count = 0;
    if (*Buffer == '"')
    {
        Buffer++;
        while (*Buffer && *Buffer != '"' && *Buffer != '\r' && *Buffer != '\n' && Count < MaxLen - 1)
        {
            if (*Buffer == '\\')
            {
                Buffer++;
                if (*Buffer == '\\')
                {
                    Result[Count++] = '\\';
                    Buffer++;
                }
                else
                {
                    Result[Count++] = static_cast<TCHAR>(HexDigit(Buffer[0]) * 16 + HexDigit(Buffer[1]));
                    Buffer += 2;
                }
            }
            else
            {
                Result[Count++] = *Buffer++;
            }
        }
        if (Count == MaxLen - 1)
        {
            GLog->Logf(NAME_Warning, TEXT("ReadToken: Quoted string too long"));
            return nullptr;
        }
        if (*Buffer++ != '"')
        {
            GWarn->Logf(NAME_Warning, TEXT("ReadToken: Bad quoted string"));
            return nullptr;
        }
    }
    else if (IsTokenChar(*Buffer))
    {
        while ((IsTokenChar(*Buffer) || *Buffer == '_' || *Buffer == '-' || (Dotted && *Buffer == '.')) && Count < MaxLen - 1)
            Result[Count++] = *Buffer++;
        if (Count == MaxLen - 1)
        {
            GLog->Logf(NAME_Warning, TEXT("ReadToken: Alphanumeric overflow"));
            return nullptr;
        }
    }
    else
    {
        // a single symbol is its own token, but isn't consumed
        Result[Count++] = *Buffer;
    }
    Result[Count] = 0;
    return Buffer;
}

// ---------------------------------------------------------------------------------------------- UProperty

// 0x000b33a7
UProperty::UProperty()
    : ArrayDim(1)
{
}

// 0x000b33bf
UProperty::UProperty(INT InOffset, const TCHAR* InCategory, DWORD InFlags)
    : PropertyFlags(InFlags)
    , Category(InCategory)
    , ArrayDim(1)
    , Offset(static_cast<_WORD>(InOffset))
{
    static_cast<UField*>(GetOuter())->AddCppProperty(this);
}

// 0x000b341a
void UProperty::Serialize(FArchive& Ar)
{
    UField::Serialize(Ar);

    // older packages stored a SuperField here
    if (Ar.Ver() < 137)
    {
        UObject* Dummy = nullptr;
        Ar << Dummy;
    }

    if (Ar.Ver() < 136)
    {
        INT OldArrayDim = ArrayDim;
        Ar << OldArrayDim;
        ArrayDim = static_cast<_WORD>(OldArrayDim);
    }
    else
    {
        Ar << ArrayDim;
    }

    Ar << PropertyFlags;
    Ar << Category;
    if (PropertyFlags & CPF_Net)
        Ar << RepOffset;

    if (Ar.IsLoading())
        Offset = 0;
}

// 0x000b34aa
UBOOL UProperty::ExportText(INT Index, TCHAR* ValueStr, BYTE* Data, BYTE* Delta, INT PortFlags) const
{
    *ValueStr = 0;
    if (Data != Delta && Matches(Data, Delta, Index))
        return 0;

    BYTE* DeltaValue = Delta ? Delta + Offset + Index * GetElementSize() : nullptr;
    ExportTextItem(ValueStr, Data + Offset + Index * GetElementSize(), DeltaValue, PortFlags);
    return 1;
}

// 0x000b3523
void UProperty::CopySingleValue(void* Dest, void* Src, UObject* SubobjectRoot) const
{
    memcpy(Dest, Src, GetElementSize());
}

// 0x000b3548
UBOOL UProperty::NetSerializeItem(FArchive& Ar, UPackageMap* Map, void* Data) const
{
    SerializeItem(Ar, Data, 0, 0);
    return 1;
}

// 0x000b3562
void UProperty::CopyCompleteValue(void* Dest, void* Src, UObject* SubobjectRoot) const
{
    for (INT i = 0; i < ArrayDim; i++)
    {
        CopySingleValue(static_cast<BYTE*>(Dest) + i * GetElementSize(),
            static_cast<BYTE*>(Src) + i * GetElementSize(), SubobjectRoot);
    }
}

// 0x000b35ae
BYTE* UProperty::GetAddress(const void* Data, INT Index) const
{
    if (Index >= ArrayDim)
        return nullptr;
    if (Index < 0)
        Index = 0;
    return const_cast<BYTE*>(static_cast<const BYTE*>(Data)) + Offset + Index * GetElementSize();
}

// 0x000b35e6
INT UProperty::GetAlign() const
{
    const INT Size = GetElementSize();
    if (Size % 4 == 0)
        return 4;
    return Size % 2 == 0 ? 2 : 1;
}

// 0x000b4157, shared by int, float and name properties
void UProperty::Link(FArchive& Ar, UProperty* Prev)
{
    Offset = static_cast<_WORD>(LinkOffset(this));
}

// 0x000b4003
UBOOL UProperty::Port() const
{
    return GetSize()
        && (Category != NAME_None || !(PropertyFlags & (CPF_Native | CPF_Transient)))
        && GetFName() != NAME_Class;
}

// 0x000b4fd3
BYTE UProperty::GetID() const
{
    return static_cast<BYTE>(GetClass()->GetFName().GetIndex());
}

// ---------------------------------------------------------------------------------------------- UByteProperty

// 0x000b3673
void UByteProperty::Serialize(FArchive& Ar)
{
    UProperty::Serialize(Ar);
    Ar << reinterpret_cast<UObject*&>(Enum);
}

// 0x000b3611
void UByteProperty::CopySingleValue(void* Dest, void* Src, UObject* SubobjectRoot) const
{
    *static_cast<BYTE*>(Dest) = *static_cast<BYTE*>(Src);
}

// 0x000b3620
void UByteProperty::CopyCompleteValue(void* Dest, void* Src, UObject* SubobjectRoot) const
{
    memcpy(Dest, Src, ArrayDim);
}

// 0x000b3641
UBOOL UByteProperty::Identical(const void* A, const void* B) const
{
    return *static_cast<const BYTE*>(A) == (B ? *static_cast<const BYTE*>(B) : 0);
}

// 0x000b3661
void UByteProperty::SerializeItem(FArchive& Ar, void* Value, INT MaxReadBytes, UBOOL Unknown) const
{
    Ar.Serialize(Value, 1);
}

// 0x000b3691
void UByteProperty::ExportCppItem(FOutputDevice& Out, UBOOL IsLocal) const
{
    Out.Log(TEXT("BYTE"));
}

// ---------------------------------------------------------------------------------------------- UIntProperty

// 0x000b36a2, shared with UFloatProperty
void UIntProperty::SerializeItem(FArchive& Ar, void* Value, INT MaxReadBytes, UBOOL Unknown) const
{
    Ar.Serialize(Value, 4);
}

// 0x000b36b4, shared with UFloatProperty
UBOOL UIntProperty::NetSerializeItem(FArchive& Ar, UPackageMap* Map, void* Data) const
{
    Ar.Serialize(Data, 4);
    return 1;
}

// 0x000b36c9
void UIntProperty::ExportCppItem(FOutputDevice& Out, UBOOL IsLocal) const
{
    Out.Log(TEXT("INT"));
}

// 0x000b36da
void UIntProperty::ExportTextItem(TCHAR* ValueStr, BYTE* PropertyValue, BYTE* DefaultValue, INT PortFlags) const
{
    appSprintf(ValueStr, TEXT("%i"), *reinterpret_cast<INT*>(PropertyValue));
}

// 0x000b36f4
const TCHAR* UIntProperty::ImportText(const TCHAR* Buffer, BYTE* Data, INT PortFlags) const
{
    if (*Buffer == '-' || (*Buffer >= '0' && *Buffer <= '9'))
        *reinterpret_cast<INT*>(Data) = static_cast<INT>(atol(Buffer));
    while (*Buffer == '-' || (*Buffer >= '0' && *Buffer <= '9'))
        Buffer++;
    return Buffer;
}

// 0x000b3ab9, shared with UObjectProperty
UBOOL UIntProperty::Identical(const void* A, const void* B) const
{
    return *static_cast<const INT*>(A) == (B ? *static_cast<const INT*>(B) : 0);
}

// 0x000b3b67, shared with UNameProperty and UObjectProperty
void UIntProperty::CopySingleValue(void* Dest, void* Src, UObject* SubobjectRoot) const
{
    *static_cast<INT*>(Dest) = *static_cast<INT*>(Src);
}

// 0x000b3b76, shared with UFloatProperty and UNameProperty
void UIntProperty::CopyCompleteValue(void* Dest, void* Src, UObject* SubobjectRoot) const
{
    memcpy(Dest, Src, ArrayDim * GetElementSize());
}

// ---------------------------------------------------------------------------------------------- UBoolProperty

// 0x000b379f
void UBoolProperty::Serialize(FArchive& Ar)
{
    UProperty::Serialize(Ar);
    if (!Ar.IsLoading() && !Ar.IsSaving())
        Ar << BitIndex;
}

// 0x000b37cb
void UBoolProperty::ExportCppItem(FOutputDevice& Out, UBOOL IsLocal) const
{
    Out.Log(IsLocal ? TEXT("UBOOL") : TEXT("BITFIELD"));
}

// 0x000b37e9
void UBoolProperty::ExportTextItem(TCHAR* ValueStr, BYTE* PropertyValue, BYTE* DefaultValue, INT PortFlags) const
{
    const UBOOL Value = (*reinterpret_cast<DWORD*>(PropertyValue) & BitMask()) != 0;
    const TCHAR* Text = (PortFlags & PPF_Localized) ? (Value ? GTrue : GFalse) : (Value ? TEXT("True") : TEXT("False"));
    appSprintf(ValueStr, TEXT("%s"), Text);
}

// 0x000b3831
const TCHAR* UBoolProperty::ImportText(const TCHAR* Buffer, BYTE* Data, INT PortFlags) const
{
    TCHAR Temp[1024];
    Buffer = ReadToken(Buffer, Temp, ARRAY_COUNT(Temp), 0);
    if (!Buffer)
        return nullptr;

    DWORD& Value = *reinterpret_cast<DWORD*>(Data);
    if (!appStricmp(Temp, TEXT("1")) || !appStricmp(Temp, TEXT("True")) || !appStricmp(Temp, GTrue))
    {
        Value |= BitMask();
        return Buffer;
    }
    if (!appStricmp(Temp, TEXT("0")) || !appStricmp(Temp, TEXT("False")) || !appStricmp(Temp, GFalse))
    {
        Value &= ~BitMask();
        return Buffer;
    }
    return nullptr;
}

// 0x000b38fb
UBOOL UBoolProperty::Identical(const void* A, const void* B) const
{
    const DWORD Other = B ? *static_cast<const DWORD*>(B) : 0;
    return ((*static_cast<const DWORD*>(A) ^ Other) & BitMask()) == 0;
}

// 0x000b3923
void UBoolProperty::SerializeItem(FArchive& Ar, void* Value, INT MaxReadBytes, UBOOL Unknown) const
{
    DWORD& Bits = *static_cast<DWORD*>(Value);
    BYTE B = (Bits & BitMask()) != 0;
    Ar.Serialize(&B, 1);
    Bits = B ? Bits | BitMask() : Bits & ~BitMask();
}

// 0x000b3967
UBOOL UBoolProperty::NetSerializeItem(FArchive& Ar, UPackageMap* Map, void* Data) const
{
    DWORD& Bits = *static_cast<DWORD*>(Data);
    BYTE B = (Bits & BitMask()) != 0;
    Ar.SerializeBits(&B, 1);
    Bits = B ? Bits | BitMask() : Bits & ~BitMask();
    return 1;
}

// 0x000b39ae
void UBoolProperty::CopySingleValue(void* Dest, void* Src, UObject* SubobjectRoot) const
{
    DWORD& D = *static_cast<DWORD*>(Dest);
    D = (D & ~BitMask()) | (*static_cast<DWORD*>(Src) & BitMask());
}

// ---------------------------------------------------------------------------------------------- UFloatProperty

// 0x000b39d1
void UFloatProperty::CopySingleValue(void* Dest, void* Src, UObject* SubobjectRoot) const
{
    *static_cast<FLOAT*>(Dest) = *static_cast<FLOAT*>(Src);
}

// 0x000b39e0
UBOOL UFloatProperty::Identical(const void* A, const void* B) const
{
    return *static_cast<const FLOAT*>(A) == (B ? *static_cast<const FLOAT*>(B) : 0.f);
}

// 0x000b3a0c
void UFloatProperty::ExportCppItem(FOutputDevice& Out, UBOOL IsLocal) const
{
    Out.Log(TEXT("FLOAT"));
}

// 0x000b3a1d
void UFloatProperty::ExportTextItem(TCHAR* ValueStr, BYTE* PropertyValue, BYTE* DefaultValue, INT PortFlags) const
{
    appSprintf(ValueStr, TEXT("%g"), *reinterpret_cast<FLOAT*>(PropertyValue));
}

// 0x000b3a3c
const TCHAR* UFloatProperty::ImportText(const TCHAR* Buffer, BYTE* Data, INT PortFlags) const
{
    *reinterpret_cast<FLOAT*>(Data) = static_cast<FLOAT>(atof(Buffer));
    while (*Buffer && *Buffer != ',' && *Buffer != ')' && *Buffer != ' ' && *Buffer != '\r' && *Buffer != '\n')
        Buffer++;
    return Buffer;
}

void UFloatProperty::SerializeItem(FArchive& Ar, void* Value, INT MaxReadBytes, UBOOL Unknown) const
{
    Ar.Serialize(Value, 4);
}

UBOOL UFloatProperty::NetSerializeItem(FArchive& Ar, UPackageMap* Map, void* Data) const
{
    Ar.Serialize(Data, 4);
    return 1;
}

void UFloatProperty::CopyCompleteValue(void* Dest, void* Src, UObject* SubobjectRoot) const
{
    memcpy(Dest, Src, ArrayDim * GetElementSize());
}

// ---------------------------------------------------------------------------------------------- UObjectProperty

// 0x000b3a71
void UObjectProperty::CleanupDestroyed(BYTE* Data) const
{
    for (INT i = 0; i < ArrayDim; i++)
    {
        UObject*& Obj = *reinterpret_cast<UObject**>(Data + i * GetElementSize());
        if (Obj && Obj->IsPendingKill())
        {
            Obj->Modify();
            Obj = nullptr;
        }
    }
}

UBOOL UObjectProperty::Identical(const void* A, const void* B) const
{
    return *static_cast<UObject* const*>(A) == (B ? *static_cast<UObject* const*>(B) : nullptr);
}

// 0x000b3af0
void UObjectProperty::Serialize(FArchive& Ar)
{
    if (Ar.IsSaving() && (PropertyClass->ClassFlags & CLASS_EditInlineNew))
        PropertyFlags |= CPF_EditInline | CPF_ExportObject;

    UProperty::Serialize(Ar);
    Ar << reinterpret_cast<UObject*&>(PropertyClass);
}

// 0x000b3b29
void UObjectProperty::ExportCppItem(FOutputDevice& Out, UBOOL IsLocal) const
{
    Out.Logf(TEXT("class %s*"), PropertyClass->GetNameCPP());
}

// 0x000b4366
void UObjectProperty::Link(FArchive& Ar, UProperty* Prev)
{
    Offset = static_cast<_WORD>(LinkOffset(this));
    if (((PropertyFlags & CPF_EditInline) && (PropertyFlags & CPF_ExportObject)) || (PropertyFlags & CPF_EditInlineNotify))
        PropertyFlags |= CPF_NeedCtorLink;
    PropertyFlags |= CPF_HasObjectRef;
}

void UObjectProperty::CopySingleValue(void* Dest, void* Src, UObject* SubobjectRoot) const
{
    *static_cast<UObject**>(Dest) = *static_cast<UObject**>(Src);
}

// 0x000b43a1
void UObjectProperty::CopyCompleteValue(void* Dest, void* Src, UObject* SubobjectRoot) const
{
    if (!(PropertyFlags & CPF_NeedCtorLink) || !SubobjectRoot)
    {
        memcpy(Dest, Src, ArrayDim * GetElementSize());
        return;
    }

    // inline objects are duplicated for the new owner instead of shared
    for (INT i = 0; i < ArrayDim; i++)
    {
        UObject* Template = static_cast<UObject**>(Src)[i];
        static_cast<UObject**>(Dest)[i] = Template
            ? StaticConstructObject(Template->GetClass(), SubobjectRoot, NAME_None, 0, Template, GError, SubobjectRoot)
            : nullptr;
    }
}

// 0x000b3ad7
UBOOL UObjectProperty::NetSerializeItem(FArchive& Ar, UPackageMap* Map, void* Data) const
{
    return Map->SerializeObject(Ar, PropertyClass, *static_cast<UObject**>(Data));
}

// 0x000b3b49
void UClassProperty::Serialize(FArchive& Ar)
{
    UObjectProperty::Serialize(Ar);
    Ar << reinterpret_cast<UObject*&>(MetaClass);
}

// ---------------------------------------------------------------------------------------------- UNameProperty

// 0x000b3ba4
void UNameProperty::SerializeItem(FArchive& Ar, void* Value, INT MaxReadBytes, UBOOL Unknown) const
{
    Ar << *static_cast<FName*>(Value);
}

// 0x000b3bb4
void UNameProperty::ExportCppItem(FOutputDevice& Out, UBOOL IsLocal) const
{
    Out.Log(TEXT("FName"));
}

// 0x000b3bc5
const TCHAR* UNameProperty::ImportText(const TCHAR* Buffer, BYTE* Data, INT PortFlags) const
{
    TCHAR Temp[1024];
    Buffer = ReadToken(Buffer, Temp, ARRAY_COUNT(Temp), 0);
    if (Buffer)
        *reinterpret_cast<FName*>(Data) = FName(Temp, FNAME_Add);
    return Buffer;
}

// 0x000b4641
UBOOL UNameProperty::Identical(const void* A, const void* B) const
{
    return *static_cast<const FName*>(A) == (B ? *static_cast<const FName*>(B) : FName(NAME_None));
}

void UNameProperty::CopySingleValue(void* Dest, void* Src, UObject* SubobjectRoot) const
{
    *static_cast<FName*>(Dest) = *static_cast<FName*>(Src);
}

void UNameProperty::CopyCompleteValue(void* Dest, void* Src, UObject* SubobjectRoot) const
{
    for (INT i = 0; i < ArrayDim; i++)
        static_cast<FName*>(Dest)[i] = static_cast<FName*>(Src)[i];
}

// ---------------------------------------------------------------------------------------------- UStrProperty

// 0x000b3c09
void UStrProperty::Serialize(FArchive& Ar)
{
    UProperty::Serialize(Ar);
}

// 0x000b3c0e
void UStrProperty::ExportCppItem(FOutputDevice& Out, UBOOL IsLocal) const
{
    Out.Log(TEXT("FString"));
}

// 0x000b4662
void UStrProperty::Link(FArchive& Ar, UProperty* Prev)
{
    Offset = static_cast<_WORD>(LinkOffset(this));
    if (!(PropertyFlags & CPF_Native))
        PropertyFlags |= CPF_NeedCtorLink;
}

// 0x000b468f
UBOOL UStrProperty::Identical(const void* A, const void* B) const
{
    return appStricmp(**static_cast<const FString*>(A), B ? **static_cast<const FString*>(B) : TEXT("")) == 0;
}

// ---------------------------------------------------------------------------------------------- UArrayProperty

// 0x000b3c1f
void UArrayProperty::Serialize(FArchive& Ar)
{
    UProperty::Serialize(Ar);
    Ar << reinterpret_cast<UObject*&>(Inner);
    Ar.Preload(Inner);
}

// 0x000b3c46
void UArrayProperty::ExportCppItem(FOutputDevice& Out, UBOOL IsLocal) const
{
    Out.Log(IsLocal ? TEXT("TArray<") : TEXT("TArrayNoInit<"));
    Inner->ExportCppItem(Out, 0);
    Out.Log(TEXT(">"));
}

// 0x000b4789
void UArrayProperty::CleanupDestroyed(BYTE* Data) const
{
    for (INT i = 0; i < ArrayDim; i++)
    {
        FArray& Array = *reinterpret_cast<FArray*>(Data + i * GetElementSize());
        for (INT j = 0; j < Array.Num(); j++)
            Inner->CleanupDestroyed(static_cast<BYTE*>(Array.GetData()) + j * Inner->GetSize());
    }
}

// 0x000b4806
void UArrayProperty::Link(FArchive& Ar, UProperty* Prev)
{
    Ar.Preload(Inner);
    Inner->Link(Ar, nullptr);
    Offset = static_cast<_WORD>(LinkOffset(this));
    if (!(PropertyFlags & CPF_Native))
        PropertyFlags |= CPF_NeedCtorLink;
    PropertyFlags |= Inner->PropertyFlags & CPF_HasObjectRef;
}

// 0x000b4904
BYTE* UArrayProperty::GetAddress(const void* Data, INT Index) const
{
    BYTE* Address = const_cast<BYTE*>(static_cast<const BYTE*>(Data)) + Offset;
    if (Index < 0)
        return Address;
    FArray& Array = *reinterpret_cast<FArray*>(Address);
    return Index < Array.Num() ? static_cast<BYTE*>(Array.GetData()) + Index * Inner->GetElementSize() : nullptr;
}

// ---------------------------------------------------------------------------------------------- UMapProperty

// 0x000b3c9e
void UMapProperty::Serialize(FArchive& Ar)
{
    UProperty::Serialize(Ar);
    Ar << reinterpret_cast<UObject*&>(Key) << reinterpret_cast<UObject*&>(Value);
}

// 0x000b3cc7
void UMapProperty::ExportCppItem(FOutputDevice& Out, UBOOL IsLocal) const
{
    Out.Log(TEXT("TMap<"));
    Key->ExportCppItem(Out, 0);
    Out.Log(TEXT(","));
    Value->ExportCppItem(Out, 0);
    Out.Log(TEXT(">"));
}

// 0x000b4c13
void UMapProperty::Link(FArchive& Ar, UProperty* Prev)
{
    Ar.Preload(Key);
    Key->Link(Ar, nullptr);
    Ar.Preload(Value);
    Value->Link(Ar, nullptr);
    Offset = static_cast<_WORD>(Align(static_cast<UField*>(GetOuter())->GetPropertiesSize(), 4));
    if (!(PropertyFlags & CPF_Native))
        PropertyFlags |= CPF_NeedCtorLink;
}

// ---------------------------------------------------------------------------------------------- UStructProperty

// 0x000b3d1b
void UStructProperty::CleanupDestroyed(BYTE* Data) const
{
    for (INT i = 0; i < ArrayDim; i++)
        Struct->CleanupDestroyed(Data + i * GetElementSize());
}

// 0x000b3d57
void UStructProperty::ExportCppItem(FOutputDevice& Out, UBOOL IsLocal) const
{
    Out.Logf(TEXT("%s"), Struct->GetNameCPP());
}

// 0x000b3d77
void UStructProperty::DestroyValue(void* Dest) const
{
    for (UStruct* S = Struct; S; S = S->GetInheritanceSuper())
    {
        for (UProperty* P = S->PropertyLink; P; P = static_cast<UProperty*>(P->Next))
        {
            if (!(P->PropertyFlags & CPF_NeedCtorLink))
                continue;
            if (!ArrayDim)
                P->DestroyValue(static_cast<BYTE*>(Dest) + P->Offset);
            for (INT i = 0; i < ArrayDim; i++)
                P->DestroyValue(static_cast<BYTE*>(Dest) + i * GetElementSize() + P->Offset);
        }
    }
}

// 0x000b4c76
void UStructProperty::Link(FArchive& Ar, UProperty* Prev)
{
    Ar.Preload(Struct);
    Offset = static_cast<_WORD>(Align(static_cast<UField*>(GetOuter())->GetPropertiesSize(), GetAlign()));
    if (!(PropertyFlags & CPF_Native))
        PropertyFlags |= CPF_NeedCtorLink;
    PropertyFlags |= CPF_HasObjectRef;
}

// 0x000b4cc5
void UStructProperty::SerializeItem(FArchive& Ar, void* Value, INT MaxReadBytes, UBOOL Unknown) const
{
    // vectors, rotators and colors stay binary, everything else is tagged
    const FName StructName = Struct->GetFName();
    if ((Ar.IsLoading() || Ar.IsSaving()) && Ar.Ver() >= 118
        && StructName != NAME_Vector && StructName != NAME_Rotator && StructName != NAME_Color)
    {
        Struct->SerializeTaggedProperties(Ar, static_cast<BYTE*>(Value), nullptr, Unknown);
        return;
    }
    Ar.Preload(Struct);
    Struct->SerializeBin(Ar, static_cast<BYTE*>(Value), MaxReadBytes);
}

// ---------------------------------------------------------------------------------------------- UDelegateProperty

// 0x000b372b
void UDelegateProperty::CopySingleValue(void* Dest, void* Src, UObject* SubobjectRoot) const
{
    *static_cast<FScriptDelegate*>(Dest) = *static_cast<FScriptDelegate*>(Src);
}

// 0x000b3740
UBOOL UDelegateProperty::Identical(const void* A, const void* B) const
{
    const FScriptDelegate& DA = *static_cast<const FScriptDelegate*>(A);
    if (!B)
        return DA.Object == nullptr;
    const FScriptDelegate& DB = *static_cast<const FScriptDelegate*>(B);
    return DA.Object == DB.Object && DA.FunctionName == DB.FunctionName;
}

// 0x000b376b (through 0x000b316a)
void UDelegateProperty::SerializeItem(FArchive& Ar, void* Value, INT MaxReadBytes, UBOOL Unknown) const
{
    FScriptDelegate& D = *static_cast<FScriptDelegate*>(Value);
    Ar << D.Object << D.FunctionName;
}

// 0x000b377b
UBOOL UDelegateProperty::NetSerializeItem(FArchive& Ar, UPackageMap* Map, void* Data) const
{
    FScriptDelegate& D = *static_cast<FScriptDelegate*>(Data);
    Ar << D.Object << D.FunctionName;
    return 1;
}

// 0x000b378e
void UDelegateProperty::ExportCppItem(FOutputDevice& Out, UBOOL IsLocal) const
{
    Out.Log(TEXT("FScriptDelegate"));
}

// 0x000b3673, shared with UByteProperty
void UDelegateProperty::Serialize(FArchive& Ar)
{
    UProperty::Serialize(Ar);
    Ar << Function;
}

// 0x000b4174
void UDelegateProperty::Link(FArchive& Ar, UProperty* Prev)
{
    PropertyFlags |= CPF_NeedCtorLink | CPF_HasObjectRef;
    Offset = static_cast<_WORD>(LinkOffset(this));
}

// 0x000b4195
void UDelegateProperty::CopyCompleteValue(void* Dest, void* Src, UObject* SubobjectRoot) const
{
    FScriptDelegate* D = static_cast<FScriptDelegate*>(Dest);
    FScriptDelegate* S = static_cast<FScriptDelegate*>(Src);
    for (INT i = 0; i < ArrayDim; i++)
    {
        D[i].FunctionName = S[i].FunctionName;
        // a delegate bound to a class is rebound to the new owner if it is of that class
        UClass* BoundClass = S[i].Object && S[i].Object->IsA(UClass::StaticClass()) ? static_cast<UClass*>(S[i].Object) : nullptr;
        D[i].Object = SubobjectRoot && BoundClass && SubobjectRoot->IsA(BoundClass) ? SubobjectRoot : S[i].Object;
    }
}

// 0x000b42ac
void UDelegateProperty::CleanupDestroyed(BYTE* Data) const
{
    for (INT i = 0; i < ArrayDim; i++)
    {
        FScriptDelegate& D = *reinterpret_cast<FScriptDelegate*>(Data + i * GetElementSize());
        if (D.Object && D.Object->IsPendingKill())
        {
            D.Object->Modify();
            D.Object = nullptr;
            D.FunctionName = FName(NAME_None);
        }
    }
}

// ---------------------------------------------------------------------------------------------- UByte/UBool link

// 0x000b4110
void UByteProperty::Link(FArchive& Ar, UProperty* Prev)
{
    Offset = static_cast<_WORD>(static_cast<UField*>(GetOuter())->GetPropertiesSize());
}

// 0x000b42f8
void UBoolProperty::Link(FArchive& Ar, UProperty* Prev)
{
    // consecutive bools share a DWORD, up to 32 of them
    UBoolProperty* PrevBool = Prev && Prev->IsA(UBoolProperty::StaticClass()) ? static_cast<UBoolProperty*>(Prev) : nullptr;
    if (static_cast<UField*>(GetOuter())->MergeBools() && PrevBool && PrevBool->BitIndex < 31)
    {
        Offset = PrevBool->Offset;
        BitIndex = PrevBool->BitIndex + 1;
    }
    else
    {
        Offset = static_cast<_WORD>(LinkOffset(this));
        BitIndex = 0;
    }
}

// 0x0009963e
UBOOL UProperty::Matches(const void* A, const void* B, INT ArrayIndex) const
{
    const INT ItemOffset = Offset + ArrayIndex * GetElementSize();
    return Identical(static_cast<const BYTE*>(A) + ItemOffset, B ? static_cast<const BYTE*>(B) + ItemOffset : nullptr);
}

// 0x000b4e2a
void UProperty::ExportCpp(FOutputDevice& Out, UBOOL IsLocal, UBOOL IsParm, UBOOL IsEvent, UBOOL IsStruct) const
{
    TCHAR ArrayStr[80] = {};
    ExportCppItem(Out, IsEvent || IsStruct);
    if (ArrayDim != 1)
        appSprintf(ArrayStr, TEXT("[%i]"), ArrayDim);

    const UBOOL IsBool = IsA(UBoolProperty::StaticClass());
    const UBOOL IsStr = IsA(UStrProperty::StaticClass());
    if (!IsParm)
    {
        if (IsBool && ArrayDim == 1 && !IsLocal)
            Out.Logf(TEXT(" %s%s:1"), GetName(), ArrayStr);
        else if (IsStr && !IsLocal && !IsStruct)
            Out.Logf(TEXT("NoInit %s%s"), GetName(), ArrayStr);
        else
            Out.Logf(TEXT(" %s%s"), GetName(), ArrayStr);
    }
    else if (ArrayDim > 1)
    {
        Out.Logf(TEXT("* %s"), GetName());
    }
    else if (PropertyFlags & CPF_OutParm)
    {
        Out.Logf(TEXT("& %s%s"), GetName(), ArrayStr);
    }
    else if (IsStr || IsA(UStructProperty::StaticClass()))
    {
        Out.Logf(TEXT(" const& %s%s"), GetName(), ArrayStr);
    }
    else
    {
        Out.Logf(TEXT(" %s%s"), GetName(), ArrayStr);
    }
}

// 0x000b4fe7
void UByteProperty::ExportTextItem(TCHAR* ValueStr, BYTE* PropertyValue, BYTE* DefaultValue, INT PortFlags) const
{
    if (Enum)
        appSprintf(ValueStr, TEXT("%s"), *PropertyValue < Enum->Names.Num() ? *Enum->Names[*PropertyValue] : TEXT("(INVALID)"));
    else
        appSprintf(ValueStr, TEXT("%i"), *PropertyValue);
}

// 0x000b5043
const TCHAR* UByteProperty::ImportText(const TCHAR* Buffer, BYTE* Data, INT PortFlags) const
{
    if (Enum)
    {
        // note the number check below then runs on what follows the token, so enums only accept names
        TCHAR Temp[1024];
        Buffer = ReadToken(Buffer, Temp, ARRAY_COUNT(Temp), 0);
        if (!Buffer)
            return nullptr;
        const FName EnumName(Temp, FNAME_Find);
        INT EnumIndex;
        if (EnumName != NAME_None && Enum->Names.FindItem(EnumName, EnumIndex))
        {
            *Data = static_cast<BYTE>(EnumIndex);
            return Buffer;
        }
    }
    if (*Buffer >= '0' && *Buffer <= '9')
    {
        *Data = static_cast<BYTE>(atol(Buffer));
        while (*Buffer >= '0' && *Buffer <= '9')
            Buffer++;
        return Buffer;
    }
    return nullptr;
}

// 0x000b511a
void UObjectProperty::SerializeItem(FArchive& Ar, void* Value, INT MaxReadBytes, UBOOL Unknown) const
{
    if (PropertyFlags & CPF_SkipNameTables)
    {
        Ar.ArSkipObjectRefs = true;
        Ar << *static_cast<UObject**>(Value);
        Ar.ArSkipObjectRefs = false;
    }
    else
    {
        Ar << *static_cast<UObject**>(Value);
    }
}

// 0x000b514b
void UObjectProperty::ExportTextItem(TCHAR* ValueStr, BYTE* PropertyValue, BYTE* DefaultValue, INT PortFlags) const
{
    UObject* Obj = *reinterpret_cast<UObject**>(PropertyValue);
    if (!Obj)
        appStrcpy(ValueStr, TEXT("None"));
    else if (!(PortFlags & PPF_Description))
        appSprintf(ValueStr, TEXT("%s'%s'"), Obj->GetClass()->GetName(), Obj->GetPathName());
    else
        appStrcpy(ValueStr, *Obj->GetDescription());
}

// 0x000b51f1
void UNameProperty::ExportTextItem(TCHAR* ValueStr, BYTE* PropertyValue, BYTE* DefaultValue, INT PortFlags) const
{
    const FName& Value = *reinterpret_cast<FName*>(PropertyValue);
    if (PortFlags & PPF_Delimited)
        appSprintf(ValueStr, TEXT("\"%s\""), *Value);
    else
        appStrcpy(ValueStr, *Value);
}

// 0x000b46cc
void UStrProperty::SerializeItem(FArchive& Ar, void* Value, INT MaxReadBytes, UBOOL Unknown) const
{
    FString& Str = *static_cast<FString*>(Value);
    Ar << Str;
    if (Ar.ArIsPersistent && (PropertyFlags & CPF_LoadObjectRef) && Str.Len())
    {
        if (UObject* Obj = StaticLoadObject(UObject::StaticClass(), nullptr, *Str, nullptr, LOAD_Unknown2002, nullptr))
            Ar.Preload(Obj);
    }
}

// 0x000b4738
void UStrProperty::ExportTextItem(TCHAR* ValueStr, BYTE* PropertyValue, BYTE* DefaultValue, INT PortFlags) const
{
    const FString& Value = *reinterpret_cast<FString*>(PropertyValue);
    if (PortFlags & PPF_Delimited)
        appSprintf(ValueStr, TEXT("\"%s\""), *Value);
    else
        appStrcpy(ValueStr, *Value);
}

// 0x000b6037
const TCHAR* UStrProperty::ImportText(const TCHAR* Buffer, BYTE* Data, INT PortFlags) const
{
    if (!(PortFlags & PPF_Delimited))
    {
        // the whole rest of the buffer is the string. the original returns Buffer unadvanced.
        *reinterpret_cast<FString*>(Data) = Buffer;
        if ((PropertyFlags & CPF_LoadObjectRef) && *Buffer)
            StaticLoadObject(UObject::StaticClass(), nullptr, Buffer, nullptr, LOAD_Unknown2002, nullptr);
        return Buffer;
    }

    TCHAR Temp[4096];
    Buffer = ReadToken(Buffer, Temp, ARRAY_COUNT(Temp), 0);
    if (Buffer)
        *reinterpret_cast<FString*>(Data) = Temp;
    return Buffer;
}

// 0x000b60af
void UStrProperty::CopySingleValue(void* Dest, void* Src, UObject* SubobjectRoot) const
{
    *static_cast<FString*>(Dest) = *static_cast<FString*>(Src);
}

// 0x000b5f84
void UStrProperty::DestroyValue(void* Dest) const
{
    for (INT i = 0; i < ArrayDim; i++)
        static_cast<FString*>(Dest)[i].~FString();
}

// 0x000b485f
UBOOL UArrayProperty::Identical(const void* A, const void* B) const
{
    const FArray& ArrayA = *static_cast<const FArray*>(A);
    const FArray* ArrayB = static_cast<const FArray*>(B);
    const INT Num = ArrayA.Num();
    if (Num != (ArrayB ? ArrayB->Num() : 0))
        return 0;

    const INT Size = Inner->GetElementSize();
    const BYTE* DataA = static_cast<const BYTE*>(ArrayA.GetData());
    for (INT i = 0; i < Num; i++)
    {
        if (!Inner->Identical(DataA + i * Size, ArrayB ? static_cast<const BYTE*>(ArrayB->GetData()) + i * Size : nullptr))
            return 0;
    }
    return 1;
}

// 0x000b493f
void UArrayProperty::SerializeItem(FArchive& Ar, void* Value, INT MaxReadBytes, UBOOL Unknown) const
{
    FArray& Array = *static_cast<FArray*>(Value);
    const INT Size = Inner->GetElementSize();
    INT Num = Array.Num();
    Ar.CountBytes(Array.GetData(), Num * Size);
    Ar << AR_INDEX(Num);
    if (Ar.IsLoading())
    {
        DestroyValue(&Array);
        Array.AddZeroed(Num, Size);
    }

    BYTE* Data = static_cast<BYTE*>(Array.GetData());
    for (INT i = 0; i < Num; i++)
        Inner->SerializeItem(Ar, Data + i * Size, MaxReadBytes > 0 ? MaxReadBytes / Num : 0, Unknown);
}

// 0x000b49df
void UArrayProperty::ExportTextItem(TCHAR* ValueStr, BYTE* PropertyValue, BYTE* DefaultValue, INT PortFlags) const
{
    const FArray& Array = *reinterpret_cast<FArray*>(PropertyValue);
    const FArray* Default = reinterpret_cast<FArray*>(DefaultValue);
    const INT Size = Inner->GetElementSize();

    *ValueStr++ = '(';
    for (INT i = 0; i < Array.Num(); i++)
    {
        if (i > 0)
            *ValueStr++ = ',';
        // the default is used up to and including index Num, one past its end, as in the original
        BYTE* DefaultItem = Default && i <= Default->Num() ? static_cast<BYTE*>(const_cast<void*>(Default->GetData())) + i * Size : nullptr;
        Inner->ExportTextItem(ValueStr, static_cast<BYTE*>(const_cast<void*>(Array.GetData())) + i * Size, DefaultItem, PortFlags | PPF_Delimited);
        ValueStr += appStrlen(ValueStr);
    }
    *ValueStr++ = ')';
    *ValueStr = 0;
}

// 0x000b4a92
const TCHAR* UArrayProperty::ImportText(const TCHAR* Buffer, BYTE* Data, INT PortFlags) const
{
    if (*Buffer++ != '(')
        return nullptr;

    FArray& Array = *reinterpret_cast<FArray*>(Data);
    const INT Size = Inner->GetElementSize();
    DestroyValue(&Array);
    if (*Buffer != ')')
    {
        do
        {
            const INT Index = Array.Add(1, Size);
            BYTE* Item = static_cast<BYTE*>(Array.GetData()) + Index * Size;
            memset(Item, 0, Size);
            Buffer = Inner->ImportText(Buffer, Item, PortFlags | PPF_Delimited);
            if (!Buffer)
                return nullptr;
        } while (*Buffer == ',' && *++Buffer != ')');

        if (*Buffer != ')')
            return nullptr;
    }
    return Buffer + 1;
}

// 0x000b4b40
void UArrayProperty::CopyCompleteValue(void* Dest, void* Src, UObject* SubobjectRoot) const
{
    FArray& DestArray = *static_cast<FArray*>(Dest);
    FArray& SrcArray = *static_cast<FArray*>(Src);
    const INT Size = Inner->GetElementSize();

    DestroyValue(&DestArray);
    DestArray.Empty(Size, SrcArray.Num());
    if (!(Inner->PropertyFlags & CPF_NeedCtorLink))
    {
        DestArray.Add(SrcArray.Num(), Size);
        memcpy(DestArray.GetData(), SrcArray.GetData(), SrcArray.Num() * Size);
        return;
    }

    DestArray.AddZeroed(SrcArray.Num(), Size);
    for (INT i = 0; i < DestArray.Num(); i++)
    {
        Inner->CopyCompleteValue(static_cast<BYTE*>(DestArray.GetData()) + i * Size,
            static_cast<BYTE*>(SrcArray.GetData()) + i * Size, SubobjectRoot);
    }
}

// 0x000b5fb8
void UArrayProperty::DestroyValue(void* Dest) const
{
    // arrays at offset 0 of a class are its own members, not script data
    if (Offset == 0 && GetOuter()->IsA(UClass::StaticClass()))
        return;

    FArray& Array = *static_cast<FArray*>(Dest);
    if (Inner->PropertyFlags & CPF_NeedCtorLink)
    {
        const INT Size = Inner->GetElementSize();
        for (INT i = 0; i < Array.Num(); i++)
            Inner->DestroyValue(static_cast<BYTE*>(Array.GetData()) + i * Size);
    }
    Array.~FArray();
}

// 0x000b3c1f, shared with UArrayProperty
void UStructProperty::Serialize(FArchive& Ar)
{
    UProperty::Serialize(Ar);
    Ar << reinterpret_cast<UObject*&>(Struct);
    Ar.Preload(Struct);
}

// 0x000b525b
UBOOL UStructProperty::Identical(const void* A, const void* B) const
{
    for (UStruct* S = Struct; S; S = S->GetInheritanceSuper())
    {
        for (UProperty* P = S->PropertyLink; P; P = static_cast<UProperty*>(P->Next))
        {
            for (INT i = 0; i < P->ArrayDim; i++)
            {
                if (!P->Matches(A, B, i))
                    return 0;
            }
        }
    }
    return 1;
}

// 0x000b5cd1
void UStructProperty::CopySingleValue(void* Dest, void* Src, UObject* SubobjectRoot) const
{
    for (UStruct* S = Struct; S; S = S->GetInheritanceSuper())
    {
        for (UProperty* P = S->PropertyLink; P; P = static_cast<UProperty*>(P->Next))
            P->CopyCompleteValue(static_cast<BYTE*>(Dest) + P->Offset, static_cast<BYTE*>(Src) + P->Offset, SubobjectRoot);
    }
}

// 0x000b58f6
void UStructProperty::ExportTextItem(TCHAR* ValueStr, BYTE* PropertyValue, BYTE* DefaultValue, INT PortFlags) const
{
    INT Count = 0;
    for (UStruct* S = Struct; S; S = S->GetInheritanceSuper())
    {
        for (UProperty* P = S->PropertyLink; P; P = static_cast<UProperty*>(P->Next))
        {
            if (!P->Port())
                continue;
            for (INT i = 0; i < P->ArrayDim; i++)
            {
                TCHAR Value[65536];
                if (!P->ExportText(i, Value, PropertyValue, DefaultValue, PPF_Delimited))
                    continue;

                *ValueStr++ = ++Count == 1 ? '(' : ',';
                ValueStr += P->ArrayDim == 1
                    ? appSprintf(ValueStr, TEXT("%s="), P->GetName())
                    : appSprintf(ValueStr, TEXT("%s[%i]="), P->GetName(), i);
                ValueStr += appSprintf(ValueStr, TEXT("%s"), Value);
            }
        }
    }
    if (Count > 0)
    {
        *ValueStr++ = ')';
        *ValueStr = 0;
    }
}

// 0x000b5a2d
const TCHAR* UStructProperty::ImportText(const TCHAR* InBuffer, BYTE* Data, INT PortFlags) const
{
    const TCHAR* Buffer = InBuffer;
    if (*Buffer++ != '(')
    {
        GLog->Logf(NAME_Warning, TEXT("ImportText: Struct missing '(' in: %s"), InBuffer);
        return nullptr;
    }

    while (*Buffer != ')')
    {
        // member name and optional [index]
        TCHAR Name[NAME_SIZE];
        INT Len = 0;
        while (*Buffer && *Buffer != '=' && *Buffer != '[' && Len < NAME_SIZE - 1)
            Name[Len++] = *Buffer++;
        Name[Len] = 0;

        INT Index = 0;
        if (*Buffer == '[')
        {
            const TCHAR* Start = ++Buffer;
            while (*Buffer >= '0' && *Buffer <= '9')
                Buffer++;
            if (*Buffer++ != ']')
            {
                GWarn->Logf(NAME_Error, TEXT("ImportText: Illegal array element in: %s"), InBuffer);
                return nullptr;
            }
            Index = atol(Start);
        }
        if (*Buffer++ != '=')
        {
            GWarn->Logf(NAME_Error, TEXT("ImportText: Missing key name in: %s"), InBuffer);
            return nullptr;
        }
        if (Index < 0)
        {
            GWarn->Logf(NAME_Error, TEXT("ImportText: Negative array index in: %s"), InBuffer);
            return nullptr;
        }

        UProperty* Found = nullptr;
        const FName MemberName(Name, FNAME_Find);
        if (MemberName != NAME_None)
        {
            for (UStruct* S = Struct; S && !Found; S = S->GetInheritanceSuper())
            {
                for (UProperty* P = S->PropertyLink; P; P = static_cast<UProperty*>(P->Next))
                {
                    if (P->GetFName() == MemberName && P->GetSize() && P->Port())
                    {
                        Found = P;
                        break;
                    }
                }
            }
        }

        if (Found)
        {
            if (Index >= Found->ArrayDim)
            {
                GWarn->Logf(NAME_Error, TEXT("ImportText: Array index out of bounds in: %s"), InBuffer);
                return nullptr;
            }
            Buffer = Found->ImportText(Buffer, Data + Found->Offset + Index * Found->GetElementSize(), PortFlags | PPF_Delimited);
            if (!Buffer)
            {
                GWarn->Logf(NAME_Error, TEXT("ImportText failed in: %s"), InBuffer);
                return nullptr;
            }
        }
        else
        {
            // skip the value of an unknown member, minding quotes and nested parentheses
            GWarn->Logf(NAME_Error, TEXT("Unknown member %s in %s"), Name, GetName());
            INT Depth = 0;
            for (; *Buffer && *Buffer != '\n' && *Buffer != '\r' && !(Depth < 1 && (*Buffer == ')' || *Buffer == ',')); Buffer++)
            {
                if (*Buffer == '"')
                {
                    do
                        Buffer++;
                    while (*Buffer && *Buffer != '"' && *Buffer != '\n' && *Buffer != '\r');
                    if (*Buffer != '"')
                    {
                        GWarn->Logf(NAME_Error, TEXT("ImportText: Bad quoted string in: %s"), InBuffer);
                        return nullptr;
                    }
                }
                else if (*Buffer == '(')
                {
                    Depth++;
                }
                else if (*Buffer == ')' && --Depth < 0)
                {
                    GWarn->Logf(NAME_Error, TEXT("ImportText: Bad parenthesised struct in: "), InBuffer);
                    return nullptr;
                }
            }
            if (Depth > 0)
            {
                GWarn->Logf(NAME_Error, TEXT("ImportText: Incomplete parenthesised struct in: "), InBuffer);
                return nullptr;
            }
        }

        if (*Buffer == ',')
        {
            Buffer++;
            continue;
        }
        if (*Buffer != ')')
        {
            GWarn->Logf(NAME_Error, TEXT("ImportText: Bad termination in: %s"), InBuffer);
            return nullptr;
        }
    }
    return Buffer + 1;
}

// 0x000b50de
void UDelegateProperty::ExportTextItem(TCHAR* ValueStr, BYTE* PropertyValue, BYTE* DefaultValue, INT PortFlags) const
{
    const FScriptDelegate& D = *reinterpret_cast<FScriptDelegate*>(PropertyValue);
    appSprintf(ValueStr, TEXT("%s.%s"), D.Object->GetName(), *D.FunctionName);
}

// 0x000b4431. reads None, Name or Class'Name'. with PortFlags 4 failures are warned about, except for sounds.
// after a plain name one more character is skipped, whatever it is.
const TCHAR* UObjectProperty::ImportText(const TCHAR* InBuffer, BYTE* Data, INT PortFlags) const
{
    UObject*& Result = *reinterpret_cast<UObject**>(Data);
    TCHAR Temp[1024];
    const TCHAR* Buffer = ReadToken(InBuffer, Temp, ARRAY_COUNT(Temp), 1);
    if (!Buffer)
        return nullptr;
    if (!appStricmp(Temp, TEXT("None")))
    {
        Result = nullptr;
        return Buffer;
    }

    while (*Buffer == ' ')
        Buffer++;
    if (*Buffer++ != '\'')
    {
        Result = StaticFindObject(PropertyClass, ANY_PACKAGE, Temp);
        if (Result)
            return Buffer;
    }
    else
    {
        TCHAR Other[1024];
        Buffer = ReadToken(Buffer, Other, ARRAY_COUNT(Other), 1);
        if (!Buffer || *Buffer++ != '\'')
            return nullptr;

        UClass* ObjectClass = static_cast<UClass*>(StaticFindObject(UClass::StaticClass(), ANY_PACKAGE, Temp));
        if (!ObjectClass)
        {
            if (PortFlags & 4)
                GWarn->Logf(NAME_Error, TEXT("%s: unresolved cast in '%s'"), GetFullName(), InBuffer);
            return nullptr;
        }

        Result = StaticFindObject(ObjectClass, ANY_PACKAGE, Other);
        if (!Result)
            Result = StaticLoadObject(ObjectClass, nullptr, Other, nullptr, LOAD_Unknown2, nullptr);
        if (Result)
        {
            if ((PortFlags & 4) && !Result->GetClass()->IsChildOf(PropertyClass))
                GWarn->Logf(NAME_Error, TEXT("%s: bad cast in '%s'"), GetFullName(), InBuffer);
            if (Result)
                return Buffer;
        }
    }

    if ((PortFlags & 4) && PropertyClass->GetFName() != NAME_Sound)
        GWarn->Logf(NAME_Error, TEXT("%s: unresolved reference to '%s'"), GetFullName(), InBuffer);
    return nullptr;
}

// 0x000b45fe. anything but a subclass of MetaClass reads as None.
const TCHAR* UClassProperty::ImportText(const TCHAR* Buffer, BYTE* Data, INT PortFlags) const
{
    const TCHAR* Result = UObjectProperty::ImportText(Buffer, Data, PortFlags);
    if (Result)
    {
        UObject*& Object = *reinterpret_cast<UObject**>(Data);
        if (!Object || Object->GetClass() != UClass::StaticClass() || !static_cast<UClass*>(Object)->IsChildOf(MetaClass))
            Object = nullptr;
    }
    return Result;
}

// 0x000b5d43. reads Object.Function. the function has to take the same parameters as the delegate's own
// declaration, which is named like the property without its "__" and "__Delegate". the xbox does not bound
// the name buffers; here they stop at NAME_SIZE.
const TCHAR* UDelegateProperty::ImportText(const TCHAR* Buffer, BYTE* Data, INT PortFlags) const
{
    TCHAR ObjectName[NAME_SIZE];
    TCHAR FunctionName[NAME_SIZE];
    INT i = 0;
    for (; *Buffer && *Buffer != '.'; Buffer++)
    {
        if (i < NAME_SIZE - 1)
            ObjectName[i++] = *Buffer;
    }
    ObjectName[i] = 0;
    i = 0;
    if (*Buffer)
    {
        for (Buffer++; *Buffer; Buffer++)
        {
            if (i < NAME_SIZE - 1)
                FunctionName[i++] = *Buffer;
        }
    }
    FunctionName[i] = 0;

    UObject* Object = StaticFindObject(UObject::StaticClass(), ANY_PACKAGE, ObjectName);
    UFunction* Function = nullptr;
    if (Object)
    {
        UClass* Cls = Cast<UClass>(Object) ? static_cast<UClass*>(Object) : Object->GetClass();
        Function = FindField<UFunction>(Cls, FunctionName);
        if (Function)
        {
            // reuses the function name buffer for the declaration's name
            appStrcpy(FunctionName, GetName() + 2);
            FunctionName[appStrlen(FunctionName) - 10] = 0;
            UFunction* Signature = FindField<UFunction>(CastChecked<UClass>(GetOuter()), FunctionName);
            if (Function->NumParms == Signature->NumParms)
            {
                TFieldIterator<UProperty> A(Function);
                TFieldIterator<UProperty> B(Signature);
                for (INT j = 0; j < Signature->NumParms; j++, ++A, ++B)
                {
                    if (A->GetClass() != B->GetClass() || ((A->PropertyFlags ^ B->PropertyFlags) & CPF_OutParm))
                    {
                        Function = nullptr;
                        break;
                    }
                }
            }
            else
            {
                Function = nullptr;
            }
        }
    }

    FScriptDelegate& Delegate = *reinterpret_cast<FScriptDelegate*>(Data);
    Delegate.Object = Function ? Object : nullptr;
    Delegate.FunctionName = Function ? Function->GetFName() : FName(NAME_None);
    return Buffer;
}

// everything below is waiting to be decompiled

UBOOL UByteProperty::NetSerializeItem(FArchive& Ar, UPackageMap* Map, void* Data) const { NOT_DECOMPILED(0x000b4126); return 0; }
UBOOL UStructProperty::NetSerializeItem(FArchive& Ar, UPackageMap* Map, void* Data) const { NOT_DECOMPILED(0x000b52dc); return 0; }

// 0x00099676
UBOOL UProperty::ShouldSerializeValue(FArchive& Ar) const
{
    const UBOOL Skip = (PropertyFlags & CPF_Native)
        || ((PropertyFlags & CPF_Transient) && Ar.IsPersistent())
        || ((PropertyFlags & CPF_Unknown20000000) && (Ar.IsSaving() || Ar.IsTrans()));
    return !Skip;
}

// ---------------------------------------------------------------------------------------------- FPropertyTag

namespace
{
    // measures a property value in bytes as the saver would write it. 0x000996b2.
    class FArchiveTagSize : public FArchive
    {
    public:
        explicit FArchiveTagSize(FArchive& InSaver)
            : Size(0)
            , Saver(InSaver)
        {
            ArIsSaving = InSaver.IsSaving();
            ArIsPersistent = InSaver.IsPersistent();
        }

        // 0x000996dd
        void Serialize(void* V, INT Length) override { Size += Length; }
        // 0x000996e7
        void Preload(UObject* Object) override { Saver.Preload(Object); }
        // 0x00099881
        FArchive& operator<<(UObject*& Object) override
        {
            INT Index = Saver.MapObject(Object);
            return *this << AR_INDEX(Index);
        }
        // 0x000998a7
        FArchive& operator<<(FName& Name) override
        {
            INT Index = Saver.MapName(&Name);
            return *this << AR_INDEX(Index);
        }

        INT Size;

    private:
        FArchive& Saver;
    };
}

// 0x000998cb
FPropertyTag::FPropertyTag(FArchive& InSaveAr, UProperty* Property, INT InIndex, BYTE* Value)
    : Type(Property->GetID())
    , Info(Property->GetID())
    , Name(Property->GetFName())
    , ItemName(NAME_None)
    , Size(0)
    , ArrayIndex(InIndex)
{
    if (UStructProperty* StructProperty = Property->IsA(UStructProperty::StaticClass()) ? static_cast<UStructProperty*>(Property) : nullptr)
        ItemName = StructProperty->Struct->GetFName();

    FArchiveTagSize SizeAr(InSaveAr);
    SerializeTaggedProperty(SizeAr, Property, Value, 0, 0);
    Size = SizeAr.Size;

    // common sizes have their own code, others are stored after the tag in 1, 2 or 4 bytes
    BYTE SizeCode;
    if (Size == 1)
        SizeCode = 0x00;
    else if (Size == 2)
        SizeCode = 0x10;
    else if (Size == 4)
        SizeCode = 0x20;
    else if (Size == 12)
        SizeCode = 0x30;
    else if (Size == 16)
        SizeCode = 0x40;
    else if (Size <= 0xFF)
        SizeCode = 0x50;
    else if (Size <= 0x10000)
        SizeCode = 0x60;
    else
        SizeCode = 0x70;
    Info |= SizeCode;

    // a bool keeps its value in the array flag
    UBoolProperty* Bool = Property->IsA(UBoolProperty::StaticClass()) ? static_cast<UBoolProperty*>(Property) : nullptr;
    if (ArrayIndex || (Bool && (*reinterpret_cast<DWORD*>(Value) & Bool->BitMask())))
        Info |= 0x80;
}

// 0x000996ef
void FPropertyTag::SerializeTaggedProperty(FArchive& Ar, UProperty* Property, BYTE* Value, INT MaxReadBytes, UBOOL Unknown)
{
    if (Property->GetClass() == UBoolProperty::StaticClass())
    {
        if (Ar.IsLoading())
        {
            UBoolProperty* Bool = static_cast<UBoolProperty*>(Property);
            if (Info & 0x80)
                *reinterpret_cast<DWORD*>(Value) |= Bool->BitMask();
            else
                *reinterpret_cast<DWORD*>(Value) &= ~Bool->BitMask();
        }
    }
    else
    {
        Property->SerializeItem(Ar, Value, MaxReadBytes, Unknown);
    }
}

// 0x000999f0
FArchive& operator<<(FArchive& Ar, FPropertyTag& Tag)
{
    Ar << Tag.Name;
    if (Tag.Name == NAME_None)
        return Ar;

    Ar << Tag.Info;
    Tag.Type = Tag.Info & 0x0F;
    if (Tag.Type == NAME_StructProperty)
        Ar << Tag.ItemName;

    switch (Tag.Info & 0x70)
    {
        case 0x00: Tag.Size = 1; break;
        case 0x10: Tag.Size = 2; break;
        case 0x20: Tag.Size = 4; break;
        case 0x30: Tag.Size = 12; break;
        case 0x40: Tag.Size = 16; break;
        case 0x50: { BYTE B = static_cast<BYTE>(Tag.Size); Ar << B; Tag.Size = B; break; }
        case 0x60: { _WORD W = static_cast<_WORD>(Tag.Size); Ar << W; Tag.Size = W; break; }
        case 0x70: { INT I = Tag.Size; Ar << I; Tag.Size = I; break; }
    }

    // the array index takes 1, 2 or 4 bytes; the top bits of the first byte say which
    if ((Tag.Info & 0x80) && Tag.Type != NAME_BoolProperty)
    {
        BYTE B;
        if (Tag.ArrayIndex <= 0x7F)
            B = static_cast<BYTE>(Tag.ArrayIndex);
        else if (Tag.ArrayIndex <= 0x3FFF)
            B = static_cast<BYTE>((Tag.ArrayIndex >> 8) + 0x80);
        else
            B = static_cast<BYTE>((Tag.ArrayIndex >> 24) + 0xC0);
        Ar << B;

        if (!(B & 0x80))
        {
            Tag.ArrayIndex = B;
        }
        else if ((B & 0xC0) == 0x80)
        {
            BYTE C = static_cast<BYTE>(Tag.ArrayIndex);
            Ar << C;
            Tag.ArrayIndex = ((B & 0x7F) << 8) + C;
        }
        else
        {
            BYTE C = static_cast<BYTE>(Tag.ArrayIndex >> 16);
            BYTE D = static_cast<BYTE>(Tag.ArrayIndex >> 8);
            BYTE E = static_cast<BYTE>(Tag.ArrayIndex);
            Ar << C << D << E;
            Tag.ArrayIndex = ((((B & 0x3F) << 8) + C) << 8 | D) << 8 | E;
        }
    }
    else
    {
        Tag.ArrayIndex = 0;
    }
    return Ar;
}
