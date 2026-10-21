#pragma once

#include <new>
#include <type_traits>

#include "CoreTypes.h"
#include "UnName.h"

class FArchive;
class FOutputDevice;
class UClass;
class UField;
class UFunction;
class ULinkerLoad;
class UPackage;
class UProperty;
class UState;
class UStruct;
struct FFrame;
struct FPropertyInstance;
struct FScriptDelegate;
struct FStateFrame;

// object flags. names follow unreal 2; meanings were checked against the code that uses them.
enum EObjectFlags : DWORD
{
    RF_Transactional    = 0x00000001, // supports undo/redo
    RF_Unreachable      = 0x00000002, // not reachable by the garbage collector
    RF_Public           = 0x00000004, // visible outside its package
    RF_TagImp           = 0x00000008,
    RF_TagExp           = 0x00000010,
    RF_SourceModified   = 0x00000020,
    RF_TagGarbage       = 0x00000040,
    RF_PerObjectLocalized = 0x00000100, // localized by object name instead of class name
    RF_NeedLoad         = 0x00000200, // still needs to be loaded
    RF_HighlightedName  = 0x00000400, // on names: a script keyword the editor highlights
    RF_WasLoaded        = 0x00000800, // Preload has serialized it
    RF_StateChanged     = 0x00001000, // GotoState switched state; tells it EndState or BeginState switched again
    RF_InEndState       = 0x00002000, // GotoState is running EndState
    RF_Transient        = 0x00004000, // don't save
    RF_Preloading       = 0x00008000, // being serialized by Preload
    RF_LoadForClient    = 0x00010000, // on names and exports: needed by clients
    RF_LoadForServer    = 0x00020000, // on names and exports: needed by servers
    RF_Standalone       = 0x00080000, // keep even when unreferenced
    RF_Destroyed        = 0x00800000, // Destroy has been called
    RF_NeedPostLoad     = 0x01000000,
    RF_HasStack         = 0x02000000, // has an FStateFrame for script execution
    RF_Native           = 0x04000000,
    RF_Marked           = 0x08000000,
    RF_ErrorShutdown    = 0x10000000, // ShutdownAfterError has been called
    RF_DebugPostLoad    = 0x20000000,
    RF_DebugSerialize   = 0x40000000,
    RF_DebugDestroy     = 0x80000000,

    RF_Load             = 0x067F01A5, // flags an export keeps when the linker creates it
};

// selects the constructors that build static native classes and objects
enum ENativeConstructor { EC_NativeConstructor };

// a 128-bit identifier
struct FGuid
{
    DWORD A, B, C, D;

    UBOOL operator==(const FGuid& X) const { return A == X.A && B == X.B && C == X.C && D == X.D; }
    // 0x00022ba9
    UBOOL operator!=(const FGuid& X) const { return !(*this == X); }
    // as 32 hex digits, in a shared static buffer
    const TCHAR* String() const;
};

enum EGotoState
{
    GOTOSTATE_NotFound,
    GOTOSTATE_Success,
    GOTOSTATE_Preempted,
};

// placement-constructs a T for its class's ClassConstructor; abstract classes are never constructed
template<typename T>
void InternalConstruct(void* X)
{
    if constexpr (!std::is_abstract_v<T>)
        ::new (X) T;
}

// script natives and bytecode handlers all have this signature; Result receives the value, or is null
#define RESULT_DECL void* const Result
#define DECLARE_FUNCTION(func) void func(FFrame& Stack, RESULT_DECL);

// declares the static class of a native c++ class. UObject uses this directly, it has its own destructor.
#define DECLARE_BASE_CLASS(TClass, TSuperClass, TStaticFlags, TWithinClass) \
public: \
    typedef TSuperClass Super; \
    typedef TWithinClass WithinClass; \
    enum { StaticClassFlags = TStaticFlags }; \
    static UClass PrivateStaticClass; \
    static UClass* StaticClass() { return &PrivateStaticClass; } \
    static void InternalConstructor(void* X) { InternalConstruct<TClass>(X); } \
    /* 0x00028baf for ULinkerLoad; one copy per class */ \
    static void* operator new(size_t Size, UObject* Outer = UObject::GetTransientPackage(), FName Name = NAME_None, DWORD Flags = 0) \
        { return StaticAllocateObject(StaticClass(), Outer, Name, Flags, nullptr, GError); } \
    static constexpr const TCHAR* StaticName() { return &#TClass[1]; } \
private:

// every class below UObject destroys itself while its vtable is still its own, as on the xbox
#define DECLARE_CLASS(TClass, TSuperClass, TStaticFlags, TWithinClass) \
    DECLARE_BASE_CLASS(TClass, TSuperClass, TStaticFlags, TWithinClass) \
public: \
    virtual ~TClass() { ConditionalDestroy(); } \
private:

// defines the static class declared by DECLARE_CLASS, in the given package
#define IMPLEMENT_CLASS(TClass, TPackage) \
    UClass TClass::PrivateStaticClass(EC_NativeConstructor, sizeof(TClass), TClass::StaticClassFlags, \
        TClass::Super::StaticClass(), TClass::WithinClass::StaticClass(), FGuid{}, TClass::StaticName(), TEXT(#TPackage), TEXT("System"), \
        RF_Public | RF_Standalone | RF_Transient | RF_Native, TClass::InternalConstructor, \
        static_cast<UClass::FStaticConstructor>(&TClass::StaticConstructor));

// logs an error and unwinds, like appErrorf but recoverable in the original
#define appThrowf(...) GError->Logf(__VA_ARGS__)

// looks up Section.Key in Package.int
const TCHAR* Localize(const TCHAR* Section, const TCHAR* Key, const TCHAR* Package, const TCHAR* LangExt = nullptr, UBOOL Optional = 0);

// looks up an error message in the [Errors] section of Package.int
const TCHAR* LocalizeError(const TCHAR* Key, const TCHAR* Package, const TCHAR* LangExt = nullptr);
// 0x000367e9
inline const TCHAR* LocalizeProgress(const TCHAR* Key, const TCHAR* Package, const TCHAR* LangExt = nullptr)
{
    return Localize(TEXT("Progress"), Key, Package, LangExt);
}
// 0x00036819
inline const TCHAR* LocalizeGeneral(const TCHAR* Key, const TCHAR* Package, const TCHAR* LangExt = nullptr)
{
    return Localize(TEXT("General"), Key, Package, LangExt, 0);
}

// flags for StaticLoadObject
enum ELoadFlags : DWORD
{
    LOAD_Unknown2002 = 0x00002002, // used when string properties load the object they name
};

// pass as the outer to find an object in any package
#define ANY_PACKAGE (reinterpret_cast<UObject*>(-1))

enum { OBJECT_HASH_BINS = 4096 };

// objects hash by name only
inline INT GetObjectHash(FName ObjName)
{
    return ObjName.GetIndex() & (OBJECT_HASH_BINS - 1);
}

// the base of every script-visible object. 40 bytes on the xbox.
class UObject
{
    DECLARE_BASE_CLASS(UObject, UObject, 0x0001, UObject)
public:
    UObject();
    void StaticConstructor() {}
    UObject(ENativeConstructor, UClass* InClass, const TCHAR* InName, const TCHAR* InPackageName, DWORD InFlags);
    virtual ~UObject();

    virtual void ProcessEvent(UFunction* Function, void* Parms, void* Result = nullptr);
    virtual void ProcessDelegate(FName DelegateName, FScriptDelegate* Delegate, void* Parms, void* Result = nullptr);
    virtual void ProcessState(FLOAT DeltaSeconds) {}
    virtual UBOOL ProcessRemoteFunction(UFunction* Function, void* Parms, FFrame* Stack) { return 0; }
    virtual void Modify();
    virtual void PostLoad();
    virtual void Destroy();
    virtual void Serialize(FArchive& Ar);
    virtual UBOOL IsPendingKill() { return 0; }
    virtual UBOOL IsInState(FName StateName);
    virtual EGotoState GotoState(FName State);
    virtual UBOOL GotoLabel(FName Label);
    virtual void InitExecution();
    virtual void ShutdownAfterError() {}
    virtual void PostEditChange();
    virtual void PreEditUndo() {}
    virtual void PostEditUndo() {}
    virtual void CallFunction(FFrame& Stack, void* Result, UFunction* Function);
    virtual UBOOL ScriptConsoleExec(const TCHAR* Cmd, FOutputDevice& Ar, UObject* Executor);
    virtual void Register();
    virtual void LanguageChange();
    virtual FString GetDescription() const;
    virtual void DebugOutput(FOutputDevice& Ar) { DebugOutputSelf(Ar); }
    virtual void DebugOutputSelf(FOutputDevice& Ar) {}
    virtual void DebugWindowSizeGet(INT& Width, INT& Height) const;
    virtual void Rename(const TCHAR* NewName = nullptr, UObject* NewOuter = nullptr);
    virtual void NetDirty(UProperty* Property) {}
    virtual UBOOL IsRuntimeStatic();
    virtual UBOOL IsDefaultValue(const FPropertyInstance& Property);

    // non-virtual helpers
    UBOOL ConditionalDestroy();
    void ConditionalPostLoad();
    void ConditionalRegister();
    void ConditionalShutdownAfterError();
    void SetLinker(ULinkerLoad* Linker, INT LinkerIndex);

    const TCHAR* GetName() const;
    const TCHAR* GetPathName(UObject* StopOuter = nullptr, TCHAR* Str = nullptr) const;
    const TCHAR* GetFullName(TCHAR* Str = nullptr) const;
    UPackage* GetOutermostPackage() const;

    INT GetIndex() const { return Index; }
    UObject* GetOuter() const { return Outer; }
    // whether SomeOuter is among the outers; a null SomeOuter matches everything
    UBOOL IsIn(UObject* SomeOuter) const
    {
        for (UObject* It = Outer; It; It = It->Outer)
            if (It == SomeOuter)
                return 1;
        return SomeOuter == nullptr;
    }
    UClass* GetClass() const { return Class; }
    FName GetFName() const { return Name; }
    DWORD GetFlags() const { return ObjectFlags; }
    void SetFlags(DWORD Flags) { ObjectFlags |= Flags; }
    void ClearFlags(DWORD Flags) { ObjectFlags &= ~Flags; }
    FStateFrame* GetStateFrame() const { return StateFrame; }
    ULinkerLoad* GetLinker() const { return _Linker; }
    INT GetLinkerIndex() const { return _LinkerIndex; }

    UBOOL IsA(UClass* SomeBase) const;
    static UBOOL IsValid(const UObject* Object);

    static ULinkerLoad* GetLoader(INT Index);
    static UObject* StaticFindObjectFast(UClass* ObjectClass, UObject* InOuter, FName ObjectName, UBOOL ExactClass = 0);
    static FName MakeUniqueObjectName(UClass* Class);
    static UObject* StaticFindObject(UClass* ObjectClass, UObject* InOuter, const TCHAR* Name, UBOOL ExactClass = 0);
    // 0x00024698. a missing object is a critical error.
    static UObject* StaticFindObjectChecked(UClass* ObjectClass, UObject* InOuter, const TCHAR* Name, UBOOL ExactClass = 0);
    static UBOOL ResolveName(UObject*& InPackage, const TCHAR*& InName, UBOOL Create, UBOOL Throw);
    static UPackage* CreatePackage(UObject* InOuter, const TCHAR* PackageName);
    static UPackage* LoadPackage(UObject* InOuter, const TCHAR* Filename, DWORD LoadFlags);
    static void ProcessRegistrants();
    static void StaticInit();
    static void StaticExit();
    // 0x00022dc5. warns about natives and casts registered twice at startup.
    static void StaticTick();
    static void BeginLoad(const FString& Context = FString());
    static void EndLoad();
    static const TCHAR* GetLanguage();
    static void SetLanguage(const TCHAR* LanguageExt);
    static void ResetLoaders(UObject* Pkg, UBOOL DynamicOnly, UBOOL ForceLazyLoad);
    static void PurgeGarbage(UBOOL OnlyFlagged = 0);
    // 0x00029bea. allocator, object and name debugging commands, GET and SET.
    static UBOOL StaticExec(const TCHAR* Cmd, FOutputDevice& Ar);
    // 0x00028fea. sets the property in the class's defaults and saves its config; Immediate also sets it in
    // every object of the class.
    static void GlobalSetProperty(const TCHAR* Value, UClass* Class, UProperty* Property, INT Offset, UBOOL Immediate);
    void RemoveFromRoot();

    // objects come from appMalloc in StaticAllocateObject
    static void operator delete(void* Ptr) { appFree(Ptr); }
    static UObject* GetTransientPackage();
    static UObject* StaticLoadObject(UClass* Class, UObject* InOuter, const TCHAR* Name, const TCHAR* Filename, DWORD LoadFlags, class UPackageMap* Sandbox);
    static UClass* StaticLoadClass(UClass* BaseClass, UObject* InOuter, const TCHAR* Name, const TCHAR* Filename, DWORD LoadFlags, class UPackageMap* Sandbox);
    static UObject* StaticAllocateObject(UClass* Class, UObject* InOuter = reinterpret_cast<UObject*>(-1), FName InName = NAME_None,
        DWORD SetFlags = 0, UObject* Template = nullptr, FOutputDevice* Error = nullptr, UObject* Ptr = nullptr, UObject* SubobjectRoot = nullptr);
    static UObject* StaticConstructObject(UClass* Class, UObject* InOuter = reinterpret_cast<UObject*>(-1), FName InName = NAME_None,
        DWORD SetFlags = 0, UObject* Template = nullptr, FOutputDevice* Error = nullptr, UObject* SubobjectRoot = nullptr);
    static void InitProperties(BYTE* Data, INT DataCount, UClass* DefaultsClass, BYTE* Defaults, INT DefaultsCount,
        UObject* DestObject, UObject* SubobjectRoot);

    // script execution
    void ProcessInternal(FFrame& Stack, RESULT_DECL);
    UFunction* FindFunction(FName InName, UBOOL Global = 0) const;
    UState* FindState(FName StateName);
    // FindFunction, but only when the name is a function
    UFunction* FindFunctionGlobal(FName InName, UBOOL Global = 0);
    UBOOL IsProbing(FName ProbeName);
    UFunction* FindFunctionChecked(FName InName, UBOOL Global = 0) const;

    // bytecode handlers, registered in GNatives under their token
    DECLARE_FUNCTION(execUndefined)
    DECLARE_FUNCTION(execLocalVariable)
    DECLARE_FUNCTION(execInstanceVariable)
    DECLARE_FUNCTION(execDefaultVariable)
    DECLARE_FUNCTION(execSwitch)
    DECLARE_FUNCTION(execJump)
    DECLARE_FUNCTION(execJumpIfNot)
    DECLARE_FUNCTION(execStop)
    DECLARE_FUNCTION(execAssert)
    DECLARE_FUNCTION(execCase)
    DECLARE_FUNCTION(execNothing)
    DECLARE_FUNCTION(execGotoLabel)
    DECLARE_FUNCTION(execEatString)
    DECLARE_FUNCTION(execLet)
    DECLARE_FUNCTION(execDynArrayElement)
    DECLARE_FUNCTION(execNew)
    DECLARE_FUNCTION(execClassContext)
    DECLARE_FUNCTION(execMetaCast)
    DECLARE_FUNCTION(execLetBool)
    DECLARE_FUNCTION(execEndFunctionParms)
    DECLARE_FUNCTION(execSelf)
    DECLARE_FUNCTION(execContext)
    DECLARE_FUNCTION(execArrayElement)
    DECLARE_FUNCTION(execVirtualFunction)
    DECLARE_FUNCTION(execFinalFunction)
    DECLARE_FUNCTION(execIntConst)
    DECLARE_FUNCTION(execFloatConst)
    DECLARE_FUNCTION(execStringConst)
    DECLARE_FUNCTION(execObjectConst)
    DECLARE_FUNCTION(execNameConst)
    DECLARE_FUNCTION(execRotationConst)
    DECLARE_FUNCTION(execVectorConst)
    DECLARE_FUNCTION(execByteConst)
    DECLARE_FUNCTION(execIntZero)
    DECLARE_FUNCTION(execIntOne)
    DECLARE_FUNCTION(execTrue)
    DECLARE_FUNCTION(execFalse)
    DECLARE_FUNCTION(execNativeParm)
    DECLARE_FUNCTION(execNoObject)
    DECLARE_FUNCTION(execIntConstByte)
    DECLARE_FUNCTION(execBoolVariable)
    DECLARE_FUNCTION(execDynamicCast)
    DECLARE_FUNCTION(execIterator)
    DECLARE_FUNCTION(execStructCmpEq)
    DECLARE_FUNCTION(execStructCmpNe)
    DECLARE_FUNCTION(execUnicodeStringConst)
    DECLARE_FUNCTION(execStructMember)
    DECLARE_FUNCTION(execDynArrayLength)
    DECLARE_FUNCTION(execGlobalFunction)
    DECLARE_FUNCTION(execPrimitiveCast)
    DECLARE_FUNCTION(execDynArrayInsert)
    DECLARE_FUNCTION(execDynArrayRemove)
    DECLARE_FUNCTION(execDebugInfo)
    DECLARE_FUNCTION(execDelegateFunction)
    DECLARE_FUNCTION(execDelegateProperty)
    DECLARE_FUNCTION(execLetDelegate)
    DECLARE_FUNCTION(execHighNative0)
    DECLARE_FUNCTION(execHighNative1)
    DECLARE_FUNCTION(execHighNative2)
    DECLARE_FUNCTION(execHighNative3)
    DECLARE_FUNCTION(execHighNative4)
    DECLARE_FUNCTION(execHighNative5)
    DECLARE_FUNCTION(execHighNative6)
    DECLARE_FUNCTION(execHighNative7)
    DECLARE_FUNCTION(execHighNative8)
    DECLARE_FUNCTION(execHighNative9)
    DECLARE_FUNCTION(execHighNative10)
    DECLARE_FUNCTION(execHighNative11)
    DECLARE_FUNCTION(execHighNative12)
    DECLARE_FUNCTION(execHighNative13)
    DECLARE_FUNCTION(execHighNative14)
    DECLARE_FUNCTION(execHighNative15)

    // primitive casts, registered in GCasts under their cast token
    DECLARE_FUNCTION(execRotatorToVector)
    DECLARE_FUNCTION(execByteToInt)
    DECLARE_FUNCTION(execByteToBool)
    DECLARE_FUNCTION(execByteToFloat)
    DECLARE_FUNCTION(execIntToByte)
    DECLARE_FUNCTION(execIntToBool)
    DECLARE_FUNCTION(execIntToFloat)
    DECLARE_FUNCTION(execBoolToByte)
    DECLARE_FUNCTION(execBoolToInt)
    DECLARE_FUNCTION(execBoolToFloat)
    DECLARE_FUNCTION(execFloatToByte)
    DECLARE_FUNCTION(execFloatToInt)
    DECLARE_FUNCTION(execFloatToBool)
    DECLARE_FUNCTION(execObjectToBool)
    DECLARE_FUNCTION(execNameToBool)
    DECLARE_FUNCTION(execStringToByte)
    DECLARE_FUNCTION(execStringToInt)
    DECLARE_FUNCTION(execStringToBool)
    DECLARE_FUNCTION(execStringToFloat)
    DECLARE_FUNCTION(execStringToVector)
    DECLARE_FUNCTION(execStringToRotator)
    DECLARE_FUNCTION(execVectorToBool)
    DECLARE_FUNCTION(execVectorToRotator)
    DECLARE_FUNCTION(execRotatorToBool)
    DECLARE_FUNCTION(execByteToString)
    DECLARE_FUNCTION(execIntToString)
    DECLARE_FUNCTION(execBoolToString)
    DECLARE_FUNCTION(execFloatToString)
    DECLARE_FUNCTION(execObjectToString)
    DECLARE_FUNCTION(execNameToString)
    DECLARE_FUNCTION(execVectorToString)
    DECLARE_FUNCTION(execRotatorToString)

    // the natives Object.uc declares, in the order of the xbox native table
    DECLARE_FUNCTION(execNot_PreBool)
    DECLARE_FUNCTION(execEqualEqual_BoolBool)
    DECLARE_FUNCTION(execNotEqual_BoolBool)
    DECLARE_FUNCTION(execAndAnd_BoolBool)
    DECLARE_FUNCTION(execXorXor_BoolBool)
    DECLARE_FUNCTION(execOrOr_BoolBool)
    DECLARE_FUNCTION(execMultiplyEqual_ByteByte)
    DECLARE_FUNCTION(execDivideEqual_ByteByte)
    DECLARE_FUNCTION(execAddEqual_ByteByte)
    DECLARE_FUNCTION(execSubtractEqual_ByteByte)
    DECLARE_FUNCTION(execAddAdd_PreByte)
    DECLARE_FUNCTION(execSubtractSubtract_PreByte)
    DECLARE_FUNCTION(execAddAdd_Byte)
    DECLARE_FUNCTION(execSubtractSubtract_Byte)
    DECLARE_FUNCTION(execComplement_PreInt)
    DECLARE_FUNCTION(execSubtract_PreInt)
    DECLARE_FUNCTION(execMultiply_IntInt)
    DECLARE_FUNCTION(execDivide_IntInt)
    DECLARE_FUNCTION(execAdd_IntInt)
    DECLARE_FUNCTION(execSubtract_IntInt)
    DECLARE_FUNCTION(execLessLess_IntInt)
    DECLARE_FUNCTION(execGreaterGreater_IntInt)
    DECLARE_FUNCTION(execGreaterGreaterGreater_IntInt)
    DECLARE_FUNCTION(execLess_IntInt)
    DECLARE_FUNCTION(execGreater_IntInt)
    DECLARE_FUNCTION(execLessEqual_IntInt)
    DECLARE_FUNCTION(execGreaterEqual_IntInt)
    DECLARE_FUNCTION(execEqualEqual_IntInt)
    DECLARE_FUNCTION(execNotEqual_IntInt)
    DECLARE_FUNCTION(execAnd_IntInt)
    DECLARE_FUNCTION(execXor_IntInt)
    DECLARE_FUNCTION(execOr_IntInt)
    DECLARE_FUNCTION(execMultiplyEqual_IntFloat)
    DECLARE_FUNCTION(execDivideEqual_IntFloat)
    DECLARE_FUNCTION(execAddEqual_IntInt)
    DECLARE_FUNCTION(execSubtractEqual_IntInt)
    DECLARE_FUNCTION(execAddAdd_PreInt)
    DECLARE_FUNCTION(execSubtractSubtract_PreInt)
    DECLARE_FUNCTION(execAddAdd_Int)
    DECLARE_FUNCTION(execSubtractSubtract_Int)
    DECLARE_FUNCTION(execRand)
    DECLARE_FUNCTION(execMin)
    DECLARE_FUNCTION(execMax)
    DECLARE_FUNCTION(execClamp)
    DECLARE_FUNCTION(execSubtract_PreFloat)
    DECLARE_FUNCTION(execMultiplyMultiply_FloatFloat)
    DECLARE_FUNCTION(execMultiply_FloatFloat)
    DECLARE_FUNCTION(execDivide_FloatFloat)
    DECLARE_FUNCTION(execPercent_FloatFloat)
    DECLARE_FUNCTION(execAdd_FloatFloat)
    DECLARE_FUNCTION(execSubtract_FloatFloat)
    DECLARE_FUNCTION(execLess_FloatFloat)
    DECLARE_FUNCTION(execGreater_FloatFloat)
    DECLARE_FUNCTION(execLessEqual_FloatFloat)
    DECLARE_FUNCTION(execGreaterEqual_FloatFloat)
    DECLARE_FUNCTION(execEqualEqual_FloatFloat)
    DECLARE_FUNCTION(execComplementEqual_FloatFloat)
    DECLARE_FUNCTION(execNotEqual_FloatFloat)
    DECLARE_FUNCTION(execMultiplyEqual_FloatFloat)
    DECLARE_FUNCTION(execDivideEqual_FloatFloat)
    DECLARE_FUNCTION(execAddEqual_FloatFloat)
    DECLARE_FUNCTION(execSubtractEqual_FloatFloat)
    DECLARE_FUNCTION(execAbs)
    DECLARE_FUNCTION(execSin)
    DECLARE_FUNCTION(execAsin)
    DECLARE_FUNCTION(execCos)
    DECLARE_FUNCTION(execAcos)
    DECLARE_FUNCTION(execTan)
    DECLARE_FUNCTION(execAtan)
    DECLARE_FUNCTION(execExp)
    DECLARE_FUNCTION(execLoge)
    DECLARE_FUNCTION(execSqrt)
    DECLARE_FUNCTION(execSquare)
    DECLARE_FUNCTION(execFRand)
    DECLARE_FUNCTION(execFMin)
    DECLARE_FUNCTION(execFMax)
    DECLARE_FUNCTION(execFClamp)
    DECLARE_FUNCTION(execLerp)
    DECLARE_FUNCTION(execSmerp)
    DECLARE_FUNCTION(execCeil)
    DECLARE_FUNCTION(execRound)
    DECLARE_FUNCTION(execFrac)
    DECLARE_FUNCTION(execSubtract_PreVector)
    DECLARE_FUNCTION(execMultiply_VectorFloat)
    DECLARE_FUNCTION(execMultiply_FloatVector)
    DECLARE_FUNCTION(execMultiply_VectorVector)
    DECLARE_FUNCTION(execDivide_VectorFloat)
    DECLARE_FUNCTION(execAdd_VectorVector)
    DECLARE_FUNCTION(execSubtract_VectorVector)
    DECLARE_FUNCTION(execLessLess_VectorRotator)
    DECLARE_FUNCTION(execGreaterGreater_VectorRotator)
    DECLARE_FUNCTION(execEqualEqual_VectorVector)
    DECLARE_FUNCTION(execNotEqual_VectorVector)
    DECLARE_FUNCTION(execDot_VectorVector)
    DECLARE_FUNCTION(execCross_VectorVector)
    DECLARE_FUNCTION(execMultiplyEqual_VectorFloat)
    DECLARE_FUNCTION(execMultiplyEqual_VectorVector)
    DECLARE_FUNCTION(execDivideEqual_VectorFloat)
    DECLARE_FUNCTION(execAddEqual_VectorVector)
    DECLARE_FUNCTION(execSubtractEqual_VectorVector)
    DECLARE_FUNCTION(execVSize)
    DECLARE_FUNCTION(execNormal)
    DECLARE_FUNCTION(execInvert)
    DECLARE_FUNCTION(execVIsZero)
    DECLARE_FUNCTION(execVRand)
    DECLARE_FUNCTION(execMirrorVectorByNormal)
    DECLARE_FUNCTION(execVSizeSq)
    DECLARE_FUNCTION(execVSize2D)
    DECLARE_FUNCTION(execVDistSq)
    DECLARE_FUNCTION(execVDist2DSq)
    DECLARE_FUNCTION(execCalculateAimPitch)
    DECLARE_FUNCTION(execEqualEqual_RotatorRotator)
    DECLARE_FUNCTION(execNotEqual_RotatorRotator)
    DECLARE_FUNCTION(execMultiply_RotatorFloat)
    DECLARE_FUNCTION(execMultiply_FloatRotator)
    DECLARE_FUNCTION(execDivide_RotatorFloat)
    DECLARE_FUNCTION(execMultiplyEqual_RotatorFloat)
    DECLARE_FUNCTION(execDivideEqual_RotatorFloat)
    DECLARE_FUNCTION(execAdd_RotatorRotator)
    DECLARE_FUNCTION(execSubtract_RotatorRotator)
    DECLARE_FUNCTION(execAddEqual_RotatorRotator)
    DECLARE_FUNCTION(execSubtractEqual_RotatorRotator)
    DECLARE_FUNCTION(execGetAxes)
    DECLARE_FUNCTION(execGetUnAxes)
    DECLARE_FUNCTION(execRotRand)
    DECLARE_FUNCTION(execOrthoRotation)
    DECLARE_FUNCTION(execNormalize)
    DECLARE_FUNCTION(execClockwiseFrom_IntInt)
    DECLARE_FUNCTION(execConcat_StringString)
    DECLARE_FUNCTION(execAt_StringString)
    DECLARE_FUNCTION(execLess_StringString)
    DECLARE_FUNCTION(execGreater_StringString)
    DECLARE_FUNCTION(execLessEqual_StringString)
    DECLARE_FUNCTION(execGreaterEqual_StringString)
    DECLARE_FUNCTION(execEqualEqual_StringString)
    DECLARE_FUNCTION(execNotEqual_StringString)
    DECLARE_FUNCTION(execComplementEqual_StringString)
    DECLARE_FUNCTION(execLen)
    DECLARE_FUNCTION(execInStr)
    DECLARE_FUNCTION(execMid)
    DECLARE_FUNCTION(execLeft)
    DECLARE_FUNCTION(execRight)
    DECLARE_FUNCTION(execCaps)
    DECLARE_FUNCTION(execChr)
    DECLARE_FUNCTION(execAsc)
    DECLARE_FUNCTION(execEqualEqual_ObjectObject)
    DECLARE_FUNCTION(execNotEqual_ObjectObject)
    DECLARE_FUNCTION(execEqualEqual_NameName)
    DECLARE_FUNCTION(execNotEqual_NameName)
    DECLARE_FUNCTION(execInterpCurveEval)
    DECLARE_FUNCTION(execInterpCurveGetOutputRange)
    DECLARE_FUNCTION(execInterpCurveGetInputDomain)
    DECLARE_FUNCTION(execQuatProduct)
    DECLARE_FUNCTION(execQuatInvert)
    DECLARE_FUNCTION(execQuatRotateVector)
    DECLARE_FUNCTION(execQuatFindBetween)
    DECLARE_FUNCTION(execQuatFromAxisAndAngle)
    DECLARE_FUNCTION(execQuatFromRotator)
    DECLARE_FUNCTION(execQuatToRotator)
    DECLARE_FUNCTION(execLog)
    DECLARE_FUNCTION(execWarn)
    DECLARE_FUNCTION(execLocalize)
    DECLARE_FUNCTION(execGotoState)
    DECLARE_FUNCTION(execIsInState)
    DECLARE_FUNCTION(execGetStateName)
    DECLARE_FUNCTION(execClassIsChildOf)
    DECLARE_FUNCTION(execIsA)
    DECLARE_FUNCTION(execEnable)
    DECLARE_FUNCTION(execDisable)
    DECLARE_FUNCTION(execGetPropertyText)
    DECLARE_FUNCTION(execSetPropertyText)
    DECLARE_FUNCTION(execGetEnum)
    DECLARE_FUNCTION(execDynamicLoadObject)
    DECLARE_FUNCTION(execFindObject)
    DECLARE_FUNCTION(execSaveConfig)
    DECLARE_FUNCTION(execStaticSaveConfig)
    DECLARE_FUNCTION(execResetConfig)
    DECLARE_FUNCTION(execUnloadInts)
    DECLARE_FUNCTION(execSetUTracing)
    DECLARE_FUNCTION(execIsUTracing)
    DECLARE_FUNCTION(execIsOnConsole)

    void AddObject(INT InIndex);
    void InitClassDefaultObject(UClass* InClass, UBOOL SetOuter);
    void AddToRoot();
    void HashObject();
    void UnhashObject();
    void LoadConfig(UBOOL Propagate = 0, UClass* Class = nullptr, const TCHAR* Filename = nullptr);
    void SaveConfig(DWORD Flags = 0x00004000 /* CPF_Config */, const TCHAR* Filename = nullptr, const TCHAR* ForcedSection = nullptr);
    void LoadLocalized();
    // destroys the strings and arrays in Data, an object or defaults block of Class
    static void ExitProperties(BYTE* Data, UClass* Class);
    static void LoadLocalizedStruct(UStruct* Struct, const TCHAR* IntName, const TCHAR* SectionName, const TCHAR* KeyPrefix, BYTE* Data);

protected:
    // the core native layouts take offsets of the members below
    friend struct FCoreNativeLayouts;

    INT          Index;
    UObject*     HashNext;
    FStateFrame* StateFrame;
    ULinkerLoad* _Linker;
    union
    {
        INT      _LinkerIndex;
        // before registration this links the native object into GAutoRegister. a union so it holds a
        // whole pointer on 64-bit hosts; the xbox simply stored the pointer in _LinkerIndex.
        UObject* _AutoRegisterNext;
    };
    UObject*     Outer;
    DWORD        ObjectFlags;
    // a registered object's name. native classes keep their raw TCHAR* name here until Register runs.
    FName        Name;
    UClass*      Class;
};

// a pointer to a native or bytecode handler
typedef void (UObject::*Native)(FFrame& Stack, RESULT_DECL);

// a reference to an object of any class, through the archive's object serializer
template<typename T, typename = std::enable_if_t<std::is_base_of_v<UObject, T>>>
FArchive& operator<<(FArchive& Ar, T*& Object)
{
    return Ar << reinterpret_cast<UObject*&>(Object);
}

// globals of the object system
// 0x0003531e. Match=Name in Stream names an object of Class, or NONE for null.
UBOOL ParseObject(const TCHAR* Stream, const TCHAR* Match, UClass* Class, UObject*& DestRes, UObject* InParent);

extern TArray<UObject*>     GObjObjects;
extern TArray<ULinkerLoad*> GObjLoaders;

// maps keyed by objects hash them by object index
inline DWORD GetTypeHash(const UObject* A) { return A ? static_cast<DWORD>(A->GetIndex()) : 0; }
extern TArray<INT>          GObjAvailable;
extern TArray<UObject*>     GObjRoot;
extern UObject*             GObjTransientPkg;
extern INT                  GObjMaxIndex;
extern INT                  GObjRegisterCount;
extern UBOOL                GCheckConflicts;
extern UBOOL                GObjInitialized;
extern UBOOL                GNoGC;
// 0x004595f4. a command handler UEngine::Exec tries; nothing on the xbox sets it.
extern FExec*               GExec;
// 0x0045a708. printed by GTIME; nothing on the xbox sets it.
extern DOUBLE               GTime;
extern INT                  GGarbageRefCount;
extern TArray<UObject*>     GObjLoaded;
extern UBOOL                GObjStaticInitStarted;
extern TMap<FName, FName>*  GObjPackageRemap;
extern UObject*             GAutoRegister;
extern TArray<UObject*>     GObjRegistrants;
extern INT                  GObjBeginLoadCount;
// imports resolved since the outermost BeginLoad
extern INT                  GImportCount;
// set while a level loads; IsLevelLoading returns it
extern UBOOL                GIsLevelLoading;

// functions run at most once per Interval seconds by Preload. nothing in Core registers one.
struct FPeriodicCallbacks
{
    FPeriodicCallbacks();
    void Tick();

    TArray<void (*)()> Callbacks;
    DOUBLE             LastTime;
    DOUBLE             Interval;
};
extern FPeriodicCallbacks   GPeriodicCallbacks;
// set at startup: this machine renders (client) or runs the game (server); picks which names a linker keeps
extern UBOOL                GIsClient;
extern UBOOL                GIsServer;
// set by main; scripts only run while it is set
extern UBOOL                GIsScriptable;
extern UBOOL                GIsGarbageCollecting;
extern UBOOL                GExitPurge;
extern UObject*             GObjHash[OBJECT_HASH_BINS];

// receives objects about to change, for undo
class FTransactionBase
{
public:
    virtual void SaveObject(UObject* Object) = 0;
};
extern FTransactionBase* GUndo;
