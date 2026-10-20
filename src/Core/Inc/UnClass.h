#pragma once

#include "UnObjBas.h"
#include "UnOutputDevice.h"

// property flags
enum EPropertyFlags : DWORD
{
    CPF_Edit            = 0x00000001, // shown in the editor
    CPF_Const           = 0x00000002, // SetPropertyText will not change it
    CPF_ExportObject    = 0x00000008,
    CPF_OptionalParm    = 0x00000010,
    CPF_Net             = 0x00000020, // replicated, has a RepOffset
    CPF_EditConstArray  = 0x00000040, // array size fixed in the editor
    CPF_Parm            = 0x00000080, // function parameter
    CPF_ReturnParm      = 0x00000400,
    CPF_Native          = 0x00001000,
    CPF_Transient       = 0x00002000,
    CPF_Config          = 0x00004000, // loaded from the class's ini section
    CPF_Localized       = 0x00008000, // loaded from the package's .int file
    CPF_EditConst       = 0x00020000, // shown but not editable in the editor
    CPF_GlobalConfig    = 0x00040000, // config read from the declaring class's section, not the object's
    CPF_HasObjectRef    = 0x00080000, // holds object references that CleanupDestroyed must clear
    CPF_NeedCtorLink    = 0x00400000, // needs construction and destruction (strings, arrays)
    CPF_OutParm         = 0x00000100,
    CPF_LoadObjectRef   = 0x00100000, // a string naming an object to load along with it
    CPF_Static          = 0x00200000, // tagged values are only checked against the default ("Static property" in the log)
    CPF_Unknown20000000 = 0x20000000, // not saved and not recorded for undo
    CPF_SkipNameTables  = 0x01000000, // object reference saved with archive flag 0x17 set, meaning unknown
    CPF_EditInline      = 0x04000000,
    CPF_EditInlineNotify = 0x80000000,
};

// function flags
enum EFunctionFlags : DWORD
{
    FUNC_Defined  = 0x00000002, // a probe function only enables its probe when set
    FUNC_Singular = 0x00000020, // may not run again on an object while it is running on it
    FUNC_Net      = 0x00000040, // replicated, has a RepOffset
    FUNC_Exec     = 0x00000200, // can be typed at the console
    FUNC_Native   = 0x00000400, // implemented in c++
    FUNC_Operator = 0x00001000, // not traced by UTrace
};

// struct flags
enum EStructFlags : DWORD
{
    STRUCT_Unknown4 = 0x00000004, // struct members are read in place instead of from a copy of the struct
};

// class flags. the IsA flags let casts test a single bit instead of walking the class chain.
enum EClassFlags : DWORD
{
    CLASS_Abstract      = 0x00000001,
    CLASS_Compiled      = 0x00000002,
    CLASS_Parsed        = 0x00000010,
    CLASS_Config        = 0x00000004, // has config properties
    CLASS_Transient     = 0x00000008, // instances are never saved
    CLASS_Localized     = 0x00000020, // has localized properties
    CLASS_RuntimeStatic = 0x00000080,
    CLASS_PerObjectConfig = 0x00000400, // config is kept per object instead of per class
    CLASS_NativeReplication = 0x00000800, // replication is done in C++
    CLASS_IsAUProperty  = 0x00008000,
    CLASS_IsAUObjectProperty = 0x00010000,
    CLASS_IsAUBoolProperty = 0x00020000,
    CLASS_IsAUState     = 0x00040000,
    CLASS_IsAUFunction  = 0x00080000,
    CLASS_EditInlineNew = 0x00200000, // instances can be created inline in the editor
    CLASS_NoAutoLoad    = 0x01000000, // its script is not loaded with the native classes at engine start
    // classes whose unnamed instances get unique names instead of NAME_InGameTempName
    CLASS_UniqueNames   = CLASS_IsAUProperty | CLASS_IsAUState | CLASS_IsAUFunction | 0x00800000,
};

// anything that can live in a struct: properties, functions, consts, enums
class UField : public UObject
{
    DECLARE_CLASS(UField, UObject, 0x0001, UObject)
public:
    UField() = default;
    UField(ENativeConstructor, UClass* InClass, const TCHAR* InName, const TCHAR* InPackageName, DWORD InFlags);

    void PostLoad() override;
    void Register() override;

    virtual void AddCppProperty(UProperty* Property);
    virtual UBOOL MergeBools() { return 1; }
    virtual void Bind() {}
    virtual UClass* GetOwnerClass();
    virtual INT GetPropertiesSize();

    UField* Next;
};

// a struct, function, state or class: owns a list of child fields and optional script code
class UStruct : public UField
{
    DECLARE_CLASS(UStruct, UField, 0x0000, UObject)
public:
    UStruct() = default;
    explicit UStruct(UStruct* InSuperStruct);
    UStruct(ENativeConstructor, INT InSize, const TCHAR* InName, const TCHAR* InPackageName, DWORD InFlags, UStruct* InSuperStruct);

    void Destroy() override;
    void PostLoad() override;
    void Serialize(FArchive& Ar) override;
    void Register() override;
    void AddCppProperty(UProperty* Property) override;
    INT GetPropertiesSize() override { return PropertiesSize; }

    virtual UStruct* GetInheritanceSuper() { return SuperField; }
    virtual void Link(FArchive& Ar, UBOOL Props);
    virtual void SerializeBin(FArchive& Ar, BYTE* Data, INT MaxReadBytes);
    virtual void SerializeTaggedProperties(FArchive& Ar, BYTE* Data, UClass* DefaultsClass, UBOOL Unknown);
    virtual void CleanupDestroyed(BYTE* Data);
    virtual BYTE SerializeExpr(INT& iCode, FArchive& Ar);
    virtual const TCHAR* GetNameCPP();

    UBOOL IsChildOf(const UStruct* SomeBase) const;
    // whether two values of this struct match property by property
    UBOOL StructCompare(const void* A, const void* B);

    // next struct in the owning state's FuncHash bucket. cleared when loading.
    UStruct*  HashNext;
    UStruct*  SuperField;
    UObject*  ScriptText;
    UObject*  CppText;
    UField*   Children;
    INT       PropertiesSize;
    // PropertiesSize before alignment to MinAlignment, rounded to 4. subclasses start their properties here.
    INT       AlignedSize;
    INT       MinAlignment;
    FName     FriendlyName;
    TArray<BYTE> Script;
    INT       TextPos;
    INT       Line;
    DWORD     StructFlags;
    // first property. properties come last in the Next chain, so walking Next from here visits only them.
    UProperty* PropertyLink;
    // md5 of the script bytecode, computed when serialized
    BYTE      ScriptMD5[16];
};

// a function: its parameters are the children of the struct
class UFunction : public UStruct
{
    DECLARE_CLASS(UFunction, UStruct, CLASS_IsAUFunction, UState)
public:
    void Serialize(FArchive& Ar) override;
    void Bind() override;

    DWORD  FunctionFlags;
    _WORD  iNative;
    _WORD  RepOffset;
    BYTE   OperPrecedence;
    BYTE   NumParms;
    _WORD  ParmsSize;
    _WORD  ReturnValueOffset;
    // the native, or ProcessInternal for script functions
    Native Func;
};

// an enumeration: a list of names its values index
// a script constant, kept as the text of its value
class UConst : public UField
{
    DECLARE_CLASS(UConst, UField, 0x0000, UStruct)
public:
    void Serialize(FArchive& Ar) override;

    FString Value;
};

class UEnum : public UField
{
    DECLARE_CLASS(UEnum, UField, 0x0000, UStruct)
public:
    void Serialize(FArchive& Ar) override;

    TArray<FName> Names;
};

// probe functions are named by the hardcoded names 300 to 363, one bit of ProbeMask each
enum { NAME_PROBEMIN = 300, NAME_PROBEMAX = 364 };

// a state: a struct with code, labels and probe masks
class UState : public UStruct
{
    DECLARE_CLASS(UState, UStruct, CLASS_IsAUState, UObject)
public:
    explicit UState(UState* InSuperState = nullptr);
    UState(ENativeConstructor, INT InSize, const TCHAR* InName, const TCHAR* InPackageName, DWORD InFlags, UState* InSuperState);

    void Serialize(FArchive& Ar) override;
    void Destroy() override;
    void Link(FArchive& Ar, UBOOL Props) override;

    // the struct named Name in the nearest state up the chain that has a FuncHash
    UStruct* FindHashedStruct(FName Name);

    QWORD ProbeMask;
    QWORD IgnoreMask;
    DWORD StateFlags;
    _WORD LabelTableOffset;
    // FUNC_HASH_BUCKETS chains of the functions, states and structs declared here and in the parents,
    // linked through HashNext. built by Link.
    UStruct** FuncHash;
};

enum { FUNC_HASH_BUCKETS = 128 };

// UState::StateFlags
enum { STATE_Auto = 0x00000002 }; // the state GotoState('Auto') picks

// one entry of a class's native function table. tables end with a null Name.
struct FNativeEntry
{
    const TCHAR* Name;
    union
    {
        Native   Func;
        UClass*  AutoClass; // for "autoclass<Name>" entries, which name a native class instead
    };
    INT          Index; // fixed native index, or 0 for natives bound by name
};

// one replicated property element of a class
struct FRepRecord
{
    UProperty* Property;
    INT        Index;
};

// a class: a state that objects are instances of
class UClass : public UState
{
    DECLARE_CLASS(UClass, UState, 0x0000, UPackage)
public:
    typedef void (*FClassConstructor)(void*);
    typedef void (UObject::*FStaticConstructor)();

    UClass();
    UClass(ENativeConstructor, DWORD InSize, DWORD InClassFlags, UClass* InSuperClass, UClass* InWithinClass, FGuid InGuid,
        const TCHAR* InName, const TCHAR* InPackageName, const TCHAR* InConfigName, DWORD InFlags,
        FClassConstructor InClassConstructor, FStaticConstructor InClassStaticConstructor);

    void Serialize(FArchive& Ar) override;
    void Register() override;
    void Bind() override;
    void Destroy() override;
    void Link(FArchive& Ar, UBOOL Props) override;

    // whether this is the "<Super>Defaults" class that holds its parent's default properties
    UBOOL IsDefaultsClass();
    // restores this class's config section from the shipped defaults
    void ResetConfig();
    // registers the table's fixed-index natives in GNatives
    void RegisterNatives();
    // the Func slot of the named entry in this class's own table
    Native* FindNative(const TCHAR* NativeName, UBOOL Unknown);

    UClass* GetSuperClass() const { return static_cast<UClass*>(SuperField); }
    UObject* GetDefaultObject() const { return reinterpret_cast<UObject*>(DefaultsClass->Defaults.GetData()); }

    DWORD              ClassFlags;
    INT                ClassUnique; // counter for MakeUniqueObjectName
    FGuid              ClassGuid;
    INT                UnknownAC;
    UClass*            ClassWithin;
    // a raw TCHAR* until Register turns it into a name
    FName              ClassConfigName;
    INT                NativePropertiesSize; // size of the native c++ class, restored into PropertiesSize on reallocation
    // each replicated property element, the parent's first
    TArray<FRepRecord> ClassReps;
    // the replicated properties and functions declared here, in linker order
    TArray<UField*>    NetFields;
    TArray<BYTE>       Defaults;
    TArray<UObject*>   UnknownD4;
    FNativeEntry*      NativeFunctionTable;
    FClassConstructor  ClassConstructor;
    FStaticConstructor ClassStaticConstructor;
    // set to the class itself by the native constructor. tagged properties are compared against its defaults.
    UClass*            DefaultsClass;
};

// a script property of a native class and the offset of the C++ member it stands for
struct FNativePropertyOffset
{
    const TCHAR* Name;
    INT          Offset;
};

// the C++ layout behind a native class's or struct's script properties. port only: on the xbox Link already put them
// at the C++ offsets, but on 64-bit hosts it does not, so Link takes the offsets and the size from here.
// script structs have no static class, so they are found by their outer's name and their own.
struct FNativeLayout
{
    FNativeLayout(UClass* (*InStaticClass)(), INT InSize, INT InAlignment, const FNativePropertyOffset* InProperties, INT InCount);
    FNativeLayout(const TCHAR* InOuterName, const TCHAR* InStructName, INT InSize, INT InAlignment,
        const FNativePropertyOffset* InProperties, INT InCount);

    static const FNativeLayout* Find(const UStruct* Struct);
    // the registered layouts, newest first, chained through Next
    static const FNativeLayout* First();
    // INDEX_NONE when the property has no C++ member
    INT FindOffset(const TCHAR* PropertyName) const;

    UClass* (*StaticClass)();
    const TCHAR*                 OuterName;
    const TCHAR*                 StructName;
    INT                          Size;
    INT                          Alignment;
    const FNativePropertyOffset* Properties;
    INT                          Count;
    const FNativeLayout*         Next;
};

// port only: the offset of the DWORD holding a bool's bitfield, found by setting the bit in a zeroed T-sized buffer.
// Set only writes the bit; no T is constructed.
template<class T, class TSet>
INT NativeBitfieldOffset(TSet Set)
{
    alignas(T) BYTE Buffer[sizeof(T)] = {};
    Set(*reinterpret_cast<T*>(Buffer));
    for (INT i = 0; i < static_cast<INT>(sizeof(T)); i++)
    {
        if (Buffer[i])
            return i & ~3;
    }
    return INDEX_NONE;
}

// the object as a T, or null when it is not one. inlined everywhere on the xbox.
template<class T>
T* Cast(UObject* Src)
{
    return Src && Src->IsA(T::StaticClass()) ? static_cast<T*>(Src) : nullptr;
}

// a cast that is fatal when the object is not a T. inlined everywhere on the xbox.
template<class T>
T* CastChecked(UObject* Src)
{
    if (!Src || !Src->IsA(T::StaticClass()))
        appErrorf(TEXT("Cast of %s to %s failed"), Src ? Src->GetFullName() : TEXT("NULL"), T::StaticClass()->GetName());
    return static_cast<T*>(Src);
}

// class flags every class inherits from its parent at registration
enum { CLASS_Inherit = 0x018F86EC };

// a class another class's script depends on. read from packages but not kept on the xbox.
struct FDependency
{
    UClass* Class;
    INT     Deep;
    DWORD   ScriptTextCRC;

    // 0x00099834
    friend FArchive& operator<<(FArchive& Ar, FDependency& Dep)
    {
        UObject* Obj = Dep.Class;
        Ar << Obj;
        Dep.Class = static_cast<UClass*>(Obj);
        return Ar << Dep.Deep << Dep.ScriptTextCRC;
    }
};

// walks the fields of type T in a struct, then in each struct it inherits from
// 0x00023768 for properties, 0x00099d16 for functions
template<class T>
class TFieldIterator
{
public:
    explicit TFieldIterator(UStruct* InStruct)
        : Struct(InStruct)
        , Field(InStruct ? InStruct->Children : nullptr)
    {
        IterateToNext();
    }

    explicit operator bool() const { return Field != nullptr; }
    void operator++()
    {
        Field = Field->Next;
        IterateToNext();
    }
    T* operator*() const { return static_cast<T*>(Field); }
    T* operator->() const { return static_cast<T*>(Field); }
    UStruct* GetStruct() const { return Struct; }

private:
    void IterateToNext()
    {
        while (Struct)
        {
            for (; Field; Field = Field->Next)
            {
                if (Field->IsA(T::StaticClass()))
                    return;
            }
            Struct = Struct->GetInheritanceSuper();
            if (Struct)
                Field = Struct->Children;
        }
    }

    UStruct* Struct;
    UField*  Field;
};

// 0x00024ffd. the T named FieldName in Owner or its parents; null when no such name exists.
template<class T>
T* FindField(UStruct* Owner, const TCHAR* FieldName)
{
    const FName Name(FieldName, FNAME_Find);
    if (Name == NAME_None)
        return nullptr;
    for (TFieldIterator<T> It(Owner); It; ++It)
    {
        if (It->GetFName() == Name)
            return *It;
    }
    return nullptr;
}




// an outermost object: what a .u or map file becomes when loaded
class UPackage : public UObject
{
    DECLARE_CLASS(UPackage, UObject, 0x0000, UObject)
public:
    UPackage();

    // on the xbox this looked up the package's native code; the port links all native code statically
    void BindPackage();

    void* DllHandle;
    UBOOL AttemptedBind;
    DWORD PackageFlags; // copied from the package file by the linker
    UBOOL bDirty;
};

// one entry of a state's label table, stored in its script
struct FLabelEntry
{
    FName Name;
    INT   iCode;
};

// a block of text owned by an object, such as a class's script source
class UTextBuffer : public UObject, public FOutputDevice
{
    DECLARE_CLASS(UTextBuffer, UObject, 0x0000, UObject)
public:
    void Serialize(FArchive& Ar) override;
    void Serialize(const TCHAR* Data, EName Event) override;

    INT     Pos;
    INT     Top;
    FString Text;
};
