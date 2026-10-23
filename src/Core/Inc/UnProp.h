#pragma once

#include "UnClass.h"

class UPackageMap;

// port flags for text import and export
enum EPropertyPortFlags
{
    PPF_Localized   = 0x00000001, // use the localized True/False strings
    PPF_Delimited   = 0x00000002, // quote strings and names, used inside structs and arrays
    PPF_Description = 0x00000008, // print objects by their description instead of their path
};

// a reference to a function on an object, the value of a delegate property
struct FScriptDelegate
{
    UObject* Object;
    FName    FunctionName;
};

// a property of a struct. holds where in an object its value lives and how to copy, compare, save and print it.
class UProperty : public UField
{
    DECLARE_CLASS(UProperty, UField, CLASS_Abstract | CLASS_IsAUProperty, UField)
public:
    UProperty();
    UProperty(INT InOffset, const TCHAR* InCategory, DWORD InFlags);

    void Serialize(FArchive& Ar) override;

    virtual INT GetElementSize() const = 0;
    virtual INT GetAlign() const;
    virtual void CleanupDestroyed(BYTE* Data) const {}
    virtual void Link(FArchive& Ar, UProperty* Prev);
    virtual UBOOL Identical(const void* A, const void* B) const = 0;
    virtual BYTE* GetAddress(const void* Data, INT Index) const;
    virtual void ExportCpp(FOutputDevice& Out, UBOOL IsLocal, UBOOL IsParm, UBOOL IsEvent, UBOOL IsStruct) const;
    virtual void ExportCppItem(FOutputDevice& Out, UBOOL IsLocal) const = 0;
    virtual void SerializeItem(FArchive& Ar, void* Value, INT MaxReadBytes, UBOOL Unknown) const = 0;
    virtual UBOOL NetSerializeItem(FArchive& Ar, UPackageMap* Map, void* Data) const;
    virtual void ExportTextItem(TCHAR* ValueStr, BYTE* PropertyValue, BYTE* DefaultValue, INT PortFlags) const = 0;
    virtual const TCHAR* ImportText(const TCHAR* Buffer, BYTE* Data, INT PortFlags) const = 0;
    virtual UBOOL ExportText(INT Index, TCHAR* ValueStr, BYTE* Data, BYTE* Delta, INT PortFlags) const;
    virtual void CopySingleValue(void* Dest, void* Src, UObject* SubobjectRoot) const;
    virtual void CopyCompleteValue(void* Dest, void* Src, UObject* SubobjectRoot) const;
    virtual void DestroyValue(void* Dest) const {}
    virtual UBOOL Port() const;
    virtual BYTE GetID() const;

    // alignment used to place this property. matches GetAlign on 32-bit targets; types holding pointers
    // override it so 64-bit hosts lay out script properties the way the c++ compiler lays out native ones.
    virtual INT GetLinkAlign() const { return GetAlign(); }

    INT GetSize() const { return ArrayDim * GetElementSize(); }
    UBOOL Matches(const void* A, const void* B, INT ArrayIndex) const;
    UBOOL ShouldSerializeValue(FArchive& Ar) const;

    DWORD PropertyFlags;
    FName Category;
    _WORD RepOffset;
    _WORD RepIndex;
    _WORD ArrayDim;
    _WORD Offset;
    // the replicated property whose replication condition this one shares
    UProperty* RepOwner;
};

class UByteProperty : public UProperty
{
    DECLARE_CLASS(UByteProperty, UProperty, CLASS_IsAUProperty, UField)
public:
    UByteProperty() = default;
    UByteProperty(INT InOffset, const TCHAR* InCategory, DWORD InFlags, UEnum* InEnum = nullptr)
        : UProperty(InOffset, InCategory, InFlags), Enum(InEnum) {}
    void Serialize(FArchive& Ar) override;
    void Link(FArchive& Ar, UProperty* Prev) override;
    INT GetElementSize() const override { return sizeof(BYTE); }
    UBOOL Identical(const void* A, const void* B) const override;
    void ExportCppItem(FOutputDevice& Out, UBOOL IsLocal) const override;
    void SerializeItem(FArchive& Ar, void* Value, INT MaxReadBytes, UBOOL Unknown) const override;
    UBOOL NetSerializeItem(FArchive& Ar, UPackageMap* Map, void* Data) const override;
    void ExportTextItem(TCHAR* ValueStr, BYTE* PropertyValue, BYTE* DefaultValue, INT PortFlags) const override;
    const TCHAR* ImportText(const TCHAR* Buffer, BYTE* Data, INT PortFlags) const override;
    void CopySingleValue(void* Dest, void* Src, UObject* SubobjectRoot) const override;
    void CopyCompleteValue(void* Dest, void* Src, UObject* SubobjectRoot) const override;

    UEnum* Enum;
};

class UIntProperty : public UProperty
{
    DECLARE_CLASS(UIntProperty, UProperty, CLASS_IsAUProperty, UField)
public:
    using UProperty::UProperty;
    INT GetElementSize() const override { return sizeof(INT); }
    UBOOL Identical(const void* A, const void* B) const override;
    void ExportCppItem(FOutputDevice& Out, UBOOL IsLocal) const override;
    void SerializeItem(FArchive& Ar, void* Value, INT MaxReadBytes, UBOOL Unknown) const override;
    UBOOL NetSerializeItem(FArchive& Ar, UPackageMap* Map, void* Data) const override;
    void ExportTextItem(TCHAR* ValueStr, BYTE* PropertyValue, BYTE* DefaultValue, INT PortFlags) const override;
    const TCHAR* ImportText(const TCHAR* Buffer, BYTE* Data, INT PortFlags) const override;
    void CopySingleValue(void* Dest, void* Src, UObject* SubobjectRoot) const override;
    void CopyCompleteValue(void* Dest, void* Src, UObject* SubobjectRoot) const override;
};

// a single bit of a DWORD. republic commando stores the bit index, not a mask.
class UBoolProperty : public UProperty
{
    DECLARE_CLASS(UBoolProperty, UProperty, CLASS_IsAUProperty | CLASS_IsAUBoolProperty, UField)
public:
    UBoolProperty() = default;
    UBoolProperty(INT InOffset, const TCHAR* InCategory, DWORD InFlags)
        : UProperty(InOffset, InCategory, InFlags), BitIndex(0) {}
    void Serialize(FArchive& Ar) override;
    void Link(FArchive& Ar, UProperty* Prev) override;
    INT GetElementSize() const override { return sizeof(DWORD); }
    UBOOL Identical(const void* A, const void* B) const override;
    void ExportCppItem(FOutputDevice& Out, UBOOL IsLocal) const override;
    void SerializeItem(FArchive& Ar, void* Value, INT MaxReadBytes, UBOOL Unknown) const override;
    UBOOL NetSerializeItem(FArchive& Ar, UPackageMap* Map, void* Data) const override;
    void ExportTextItem(TCHAR* ValueStr, BYTE* PropertyValue, BYTE* DefaultValue, INT PortFlags) const override;
    const TCHAR* ImportText(const TCHAR* Buffer, BYTE* Data, INT PortFlags) const override;
    void CopySingleValue(void* Dest, void* Src, UObject* SubobjectRoot) const override;

    DWORD BitMask() const { return 1u << (BitIndex & 31); }

    BYTE BitIndex;
};

class UFloatProperty : public UProperty
{
    DECLARE_CLASS(UFloatProperty, UProperty, CLASS_IsAUProperty, UField)
public:
    using UProperty::UProperty;
    INT GetElementSize() const override { return sizeof(FLOAT); }
    UBOOL Identical(const void* A, const void* B) const override;
    void ExportCppItem(FOutputDevice& Out, UBOOL IsLocal) const override;
    void SerializeItem(FArchive& Ar, void* Value, INT MaxReadBytes, UBOOL Unknown) const override;
    UBOOL NetSerializeItem(FArchive& Ar, UPackageMap* Map, void* Data) const override;
    void ExportTextItem(TCHAR* ValueStr, BYTE* PropertyValue, BYTE* DefaultValue, INT PortFlags) const override;
    const TCHAR* ImportText(const TCHAR* Buffer, BYTE* Data, INT PortFlags) const override;
    void CopySingleValue(void* Dest, void* Src, UObject* SubobjectRoot) const override;
    void CopyCompleteValue(void* Dest, void* Src, UObject* SubobjectRoot) const override;
};

class UObjectProperty : public UProperty
{
    DECLARE_CLASS(UObjectProperty, UProperty, CLASS_IsAUProperty | CLASS_IsAUObjectProperty, UField)
public:
    UObjectProperty() = default;
    UObjectProperty(INT InOffset, const TCHAR* InCategory, DWORD InFlags, UClass* InClass)
        : UProperty(InOffset, InCategory, InFlags), PropertyClass(InClass) {}
    INT GetLinkAlign() const override { return alignof(UObject*) > 4 ? alignof(UObject*) : 4; }
    void Serialize(FArchive& Ar) override;
    INT GetElementSize() const override { return sizeof(UObject*); }
    void CleanupDestroyed(BYTE* Data) const override;
    void Link(FArchive& Ar, UProperty* Prev) override;
    UBOOL Identical(const void* A, const void* B) const override;
    void ExportCppItem(FOutputDevice& Out, UBOOL IsLocal) const override;
    void SerializeItem(FArchive& Ar, void* Value, INT MaxReadBytes, UBOOL Unknown) const override;
    UBOOL NetSerializeItem(FArchive& Ar, UPackageMap* Map, void* Data) const override;
    void ExportTextItem(TCHAR* ValueStr, BYTE* PropertyValue, BYTE* DefaultValue, INT PortFlags) const override;
    const TCHAR* ImportText(const TCHAR* Buffer, BYTE* Data, INT PortFlags) const override;
    void CopySingleValue(void* Dest, void* Src, UObject* SubobjectRoot) const override;
    void CopyCompleteValue(void* Dest, void* Src, UObject* SubobjectRoot) const override;

    UClass* PropertyClass;
};

class UClassProperty : public UObjectProperty
{
    DECLARE_CLASS(UClassProperty, UObjectProperty, 0x0000, UField)
public:
    void Serialize(FArchive& Ar) override;
    const TCHAR* ImportText(const TCHAR* Buffer, BYTE* Data, INT PortFlags) const override;
    // class properties are saved as object properties
    BYTE GetID() const override { return NAME_ObjectProperty; }

    UClass* MetaClass;
};

class UNameProperty : public UProperty
{
    DECLARE_CLASS(UNameProperty, UProperty, CLASS_IsAUProperty, UField)
public:
    using UProperty::UProperty;
    INT GetLinkAlign() const override { return alignof(FName) > 4 ? alignof(FName) : 4; }
    INT GetElementSize() const override { return sizeof(FName); }
    UBOOL Identical(const void* A, const void* B) const override;
    void ExportCppItem(FOutputDevice& Out, UBOOL IsLocal) const override;
    void SerializeItem(FArchive& Ar, void* Value, INT MaxReadBytes, UBOOL Unknown) const override;
    void ExportTextItem(TCHAR* ValueStr, BYTE* PropertyValue, BYTE* DefaultValue, INT PortFlags) const override;
    const TCHAR* ImportText(const TCHAR* Buffer, BYTE* Data, INT PortFlags) const override;
    void CopySingleValue(void* Dest, void* Src, UObject* SubobjectRoot) const override;
    void CopyCompleteValue(void* Dest, void* Src, UObject* SubobjectRoot) const override;
};

class UStrProperty : public UProperty
{
    DECLARE_CLASS(UStrProperty, UProperty, CLASS_IsAUProperty, UField)
public:
    using UProperty::UProperty;
    INT GetLinkAlign() const override { return alignof(FString) > 4 ? alignof(FString) : 4; }
    void Serialize(FArchive& Ar) override;
    INT GetElementSize() const override { return sizeof(FString); }
    void Link(FArchive& Ar, UProperty* Prev) override;
    UBOOL Identical(const void* A, const void* B) const override;
    void ExportCppItem(FOutputDevice& Out, UBOOL IsLocal) const override;
    void SerializeItem(FArchive& Ar, void* Value, INT MaxReadBytes, UBOOL Unknown) const override;
    void ExportTextItem(TCHAR* ValueStr, BYTE* PropertyValue, BYTE* DefaultValue, INT PortFlags) const override;
    const TCHAR* ImportText(const TCHAR* Buffer, BYTE* Data, INT PortFlags) const override;
    void CopySingleValue(void* Dest, void* Src, UObject* SubobjectRoot) const override;
    void DestroyValue(void* Dest) const override;
};

class UArrayProperty : public UProperty
{
    DECLARE_CLASS(UArrayProperty, UProperty, CLASS_IsAUProperty, UField)
public:
    using UProperty::UProperty;
    INT GetLinkAlign() const override { return alignof(FArray) > 4 ? alignof(FArray) : 4; }
    void Serialize(FArchive& Ar) override;
    void AddCppProperty(UProperty* Property) override { Inner = Property; }
    INT GetElementSize() const override { return sizeof(FArray); }
    void CleanupDestroyed(BYTE* Data) const override;
    void Link(FArchive& Ar, UProperty* Prev) override;
    UBOOL Identical(const void* A, const void* B) const override;
    BYTE* GetAddress(const void* Data, INT Index) const override;
    void ExportCppItem(FOutputDevice& Out, UBOOL IsLocal) const override;
    void SerializeItem(FArchive& Ar, void* Value, INT MaxReadBytes, UBOOL Unknown) const override;
    UBOOL NetSerializeItem(FArchive& Ar, UPackageMap* Map, void* Data) const override { return 1; }
    void ExportTextItem(TCHAR* ValueStr, BYTE* PropertyValue, BYTE* DefaultValue, INT PortFlags) const override;
    const TCHAR* ImportText(const TCHAR* Buffer, BYTE* Data, INT PortFlags) const override;
    void CopyCompleteValue(void* Dest, void* Src, UObject* SubobjectRoot) const override;
    void DestroyValue(void* Dest) const override;

    UProperty* Inner;
};


class UMapProperty : public UProperty
{
    DECLARE_CLASS(UMapProperty, UProperty, CLASS_IsAUProperty, UField)
public:
    void Serialize(FArchive& Ar) override;
    INT GetElementSize() const override { return 16; }
    void Link(FArchive& Ar, UProperty* Prev) override;
    UBOOL Identical(const void* A, const void* B) const override { return 1; }
    void ExportCppItem(FOutputDevice& Out, UBOOL IsLocal) const override;
    // maps were never finished in the engine: they don't save, print or copy
    void SerializeItem(FArchive& Ar, void* Value, INT MaxReadBytes, UBOOL Unknown) const override {}
    UBOOL NetSerializeItem(FArchive& Ar, UPackageMap* Map, void* Data) const override { return 1; }
    void ExportTextItem(TCHAR* ValueStr, BYTE* PropertyValue, BYTE* DefaultValue, INT PortFlags) const override {}
    const TCHAR* ImportText(const TCHAR* Buffer, BYTE* Data, INT PortFlags) const override { return Buffer; }
    void CopySingleValue(void* Dest, void* Src, UObject* SubobjectRoot) const override {}

    UProperty* Key;
    UProperty* Value;
};

class UStructProperty : public UProperty
{
    DECLARE_CLASS(UStructProperty, UProperty, CLASS_IsAUProperty, UField)
public:
    UStructProperty() = default;
    UStructProperty(INT InOffset, const TCHAR* InCategory, DWORD InFlags, UStruct* InStruct)
        : UProperty(InOffset, InCategory, InFlags), Struct(InStruct) {}
    INT GetLinkAlign() const override { return Struct->MinAlignment; }
    void Serialize(FArchive& Ar) override;
    INT GetElementSize() const override { return Struct->PropertiesSize; }
    INT GetAlign() const override { return Struct->MinAlignment; }
    void CleanupDestroyed(BYTE* Data) const override;
    void Link(FArchive& Ar, UProperty* Prev) override;
    UBOOL Identical(const void* A, const void* B) const override;
    void ExportCppItem(FOutputDevice& Out, UBOOL IsLocal) const override;
    void SerializeItem(FArchive& Ar, void* Value, INT MaxReadBytes, UBOOL Unknown) const override;
    UBOOL NetSerializeItem(FArchive& Ar, UPackageMap* Map, void* Data) const override;
    void ExportTextItem(TCHAR* ValueStr, BYTE* PropertyValue, BYTE* DefaultValue, INT PortFlags) const override;
    const TCHAR* ImportText(const TCHAR* Buffer, BYTE* Data, INT PortFlags) const override;
    void CopySingleValue(void* Dest, void* Src, UObject* SubobjectRoot) const override;
    void DestroyValue(void* Dest) const override;

    UStruct* Struct;
};

class UDelegateProperty : public UProperty
{
    DECLARE_CLASS(UDelegateProperty, UProperty, CLASS_IsAUProperty, UField)
public:
    INT GetLinkAlign() const override { return alignof(FScriptDelegate) > 4 ? alignof(FScriptDelegate) : 4; }
    INT GetElementSize() const override { return sizeof(FScriptDelegate); }
    void CleanupDestroyed(BYTE* Data) const override;
    void Link(FArchive& Ar, UProperty* Prev) override;
    UBOOL Identical(const void* A, const void* B) const override;
    void ExportCppItem(FOutputDevice& Out, UBOOL IsLocal) const override;
    void SerializeItem(FArchive& Ar, void* Value, INT MaxReadBytes, UBOOL Unknown) const override;
    UBOOL NetSerializeItem(FArchive& Ar, UPackageMap* Map, void* Data) const override;
    void ExportTextItem(TCHAR* ValueStr, BYTE* PropertyValue, BYTE* DefaultValue, INT PortFlags) const override;
    const TCHAR* ImportText(const TCHAR* Buffer, BYTE* Data, INT PortFlags) const override;
    void CopySingleValue(void* Dest, void* Src, UObject* SubobjectRoot) const override;
    void CopyCompleteValue(void* Dest, void* Src, UObject* SubobjectRoot) const override;
    void Serialize(FArchive& Ar) override;

    // the function declaring the delegate's parameters
    UFunction* Function = nullptr;
};

// localized True and False, used by bool properties when exporting with PPF_Localized
// localized words for text import and export; SetLanguage reloads them from Core.int
extern TCHAR GTrue[64];
extern TCHAR GFalse[64];
extern TCHAR GNone[64];
extern TCHAR GYes[64];
extern TCHAR GNo[64];

// reads one token from Buffer: a quoted string, a run of letters, digits, _ and - (and . when Dotted), or a single character
const TCHAR* ReadToken(const TCHAR* Buffer, TCHAR* Result, INT MaxLen, UBOOL Dotted);

// the header written before each property by SerializeTaggedProperties
struct FPropertyTag
{
    BYTE  Type;      // the property's GetID
    BYTE  Info;      // Type in the low 4 bits, size code in the next 3, array index flag in the top bit
    FName Name;
    FName ItemName;  // the struct's name, for struct properties
    INT   Size;
    INT   ArrayIndex;

    FPropertyTag() : Type(0), Info(0), Name(NAME_None), ItemName(NAME_None), Size(0), ArrayIndex(0) {}
    FPropertyTag(FArchive& InSaveAr, UProperty* Property, INT InIndex, BYTE* Value);

    void SerializeTaggedProperty(FArchive& Ar, UProperty* Property, BYTE* Value, INT MaxReadBytes, UBOOL Unknown);
    friend FArchive& operator<<(FArchive& Ar, FPropertyTag& Tag);
};
