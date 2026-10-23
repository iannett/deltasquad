#pragma once

#include "UnClass.h"
#include "UnOutputDevice.h"

// script bytecode tokens, named as UELib names them. tokens from EX_ExtendedNative up call native functions.
enum EExprToken : BYTE
{
    EX_LocalVariable      = 0x00,
    EX_InstanceVariable   = 0x01,
    EX_DefaultVariable    = 0x02,
    EX_Return             = 0x04,
    EX_Switch             = 0x05,
    EX_Jump               = 0x06,
    EX_JumpIfNot          = 0x07,
    EX_Stop               = 0x08,
    EX_Assert             = 0x09,
    EX_Case               = 0x0A,
    EX_Nothing            = 0x0B,
    EX_LabelTable         = 0x0C,
    EX_GotoLabel          = 0x0D,
    EX_EatString          = 0x0E,
    EX_Let                = 0x0F,
    EX_DynArrayElement    = 0x10,
    EX_New                = 0x11,
    EX_ClassContext       = 0x12,
    EX_MetaCast           = 0x13,
    EX_LetBool            = 0x14,
    EX_EndFunctionParms   = 0x16,
    EX_Self               = 0x17,
    EX_Skip               = 0x18,
    EX_Context            = 0x19,
    EX_ArrayElement       = 0x1A,
    EX_VirtualFunction    = 0x1B,
    EX_FinalFunction      = 0x1C,
    EX_IntConst           = 0x1D,
    EX_FloatConst         = 0x1E,
    EX_StringConst        = 0x1F,
    EX_ObjectConst        = 0x20,
    EX_NameConst          = 0x21,
    EX_RotationConst      = 0x22,
    EX_VectorConst        = 0x23,
    EX_ByteConst          = 0x24,
    EX_IntZero            = 0x25,
    EX_IntOne             = 0x26,
    EX_True               = 0x27,
    EX_False              = 0x28,
    EX_NativeParm         = 0x29,
    EX_NoObject           = 0x2A,
    EX_IntConstByte       = 0x2C,
    EX_BoolVariable       = 0x2D, // takes no operand on the xbox
    EX_DynamicCast        = 0x2E,
    EX_Iterator           = 0x2F,
    EX_IteratorPop        = 0x30,
    EX_IteratorNext       = 0x31,
    EX_StructCmpEq        = 0x32,
    EX_StructCmpNe        = 0x33,
    EX_UnicodeStringConst = 0x34,
    EX_StructMember       = 0x36,
    EX_DynArrayLength     = 0x37,
    EX_GlobalFunction     = 0x38,
    EX_PrimitiveCast      = 0x39,
    EX_DynArrayInsert     = 0x40,
    EX_DynArrayRemove     = 0x41,
    EX_DebugInfo          = 0x42,
    EX_DelegateFunction   = 0x43,
    EX_DelegateProperty   = 0x44,
    EX_LetDelegate        = 0x45,
    EX_ExtendedNative     = 0x60,
    EX_FirstNative        = 0x70,
};

// cast tokens that follow EX_PrimitiveCast
enum ECastToken : BYTE
{
    CST_RotatorToVector = 0x39,
    CST_ByteToInt       = 0x3A,
    CST_ByteToBool      = 0x3B,
    CST_ByteToFloat     = 0x3C,
    CST_IntToByte       = 0x3D,
    CST_IntToBool       = 0x3E,
    CST_IntToFloat      = 0x3F,
    CST_BoolToByte      = 0x40,
    CST_BoolToInt       = 0x41,
    CST_BoolToFloat     = 0x42,
    CST_FloatToByte     = 0x43,
    CST_FloatToInt      = 0x44,
    CST_FloatToBool     = 0x45,
    CST_ObjectToBool    = 0x47,
    CST_NameToBool      = 0x48,
    CST_StringToByte    = 0x49,
    CST_StringToInt     = 0x4A,
    CST_StringToBool    = 0x4B,
    CST_StringToFloat   = 0x4C,
    CST_StringToVector  = 0x4D,
    CST_StringToRotator = 0x4E,
    CST_VectorToBool    = 0x4F,
    CST_VectorToRotator = 0x50,
    CST_RotatorToBool   = 0x51,
    CST_ByteToString    = 0x52,
    CST_IntToString     = 0x53,
    CST_BoolToString    = 0x54,
    CST_FloatToString   = 0x55,
    CST_ObjectToString  = 0x56,
    CST_NameToString    = 0x57,
    CST_VectorToString  = 0x58,
    CST_RotatorToString = 0x59,
};

// the version EX_DebugInfo carries when it follows a function call
enum { DEBUG_INFO_VERSION = 100 };

// script code keeps object and name references in 4 bytes, as on the xbox, so code offsets match the packages.
// an object is stored as its index in GObjObjects plus one (0 is null), a name as its name index.
inline UObject* ReadScriptObject(const BYTE* Code)
{
    INT Index;
    memcpy(&Index, Code, sizeof(Index));
    return Index ? GObjObjects[Index - 1] : nullptr;
}

inline void WriteScriptObject(BYTE* Code, UObject* Object)
{
    const INT Index = Object ? Object->GetIndex() + 1 : 0;
    memcpy(Code, &Index, sizeof(Index));
}

inline FName ReadScriptName(const BYTE* Code)
{
    INT Index;
    memcpy(&Index, Code, sizeof(Index));
    return FName(static_cast<EName>(Index));
}

inline void WriteScriptName(BYTE* Code, FName Name)
{
    const INT Index = Name.GetIndex();
    memcpy(Code, &Index, sizeof(Index));
}

enum { MAX_NATIVES = 0x1000, MAX_CASTS = 0x100 };

// flags bytecode handlers pass to each other
enum ERuntimeUCFlags : DWORD
{
    RUC_ArrayLengthSet = 0x01, // a dynamic array's length was used as an lvalue; the next Let resizes the array
};

// handlers by bytecode token (and by fixed native index from EX_FirstNative up), and casts by cast token
extern Native GNatives[MAX_NATIVES];
extern Native GCasts[MAX_CASTS];
// the last index registered twice or out of range
extern INT    GNativeDuplicate;
extern INT    GCastDuplicate;
// the variable the last handler evaluated, for assignments and error messages
extern UProperty* GProperty;
extern BYTE*      GPropAddr;
extern UObject*   GPropObject;
extern DWORD      GRuntimeUCFlags;

BYTE GRegisterNative(INT iNative, const Native& Func);
BYTE GRegisterCast(INT CastCode, const Native& Func);

#define IMPLEMENT_FUNCTION(cls, num, func) \
    static BYTE cls##func##Temp = GRegisterNative(num, static_cast<Native>(&cls::func));
#define IMPLEMENT_CAST_FUNCTION(cls, num, func) \
    static BYTE cls##func##CastTemp = GRegisterCast(num, &cls::func);
// hooks a native table up to its class. it has to follow the class's IMPLEMENT_CLASS in the same file,
// since the class constructor clears the table pointer.
#define IMPLEMENT_NATIVE_TABLE(cls, Table) \
    static BYTE cls##NativeTableTemp = (cls::StaticClass()->NativeFunctionTable = Table, cls::StaticClass()->RegisterNatives(), 0);

// a script stack frame. it is also an output device so script warnings can name where they happened.
struct FFrame : public FOutputDevice
{
    UStruct* Node;
    UObject* Object;
    BYTE*    Code;
    BYTE*    Locals;

    FFrame(UObject* InObject)
        : Node(InObject ? static_cast<UStruct*>(InObject->GetClass()) : nullptr)
        , Object(InObject)
        , Code(nullptr)
        , Locals(nullptr)
    {
    }

    // inlined at each call on the xbox
    FFrame(UObject* InObject, UStruct* InNode, INT CodeOffset, void* InLocals)
        : Node(InNode)
        , Object(InObject)
        , Code(InNode->Script.GetData() + CodeOffset)
        , Locals(static_cast<BYTE*>(InLocals))
    {
    }

    void Serialize(const TCHAR* Text, EName Event) override;

    // runs the next expression with Context as self
    void Step(UObject* Context, RESULT_DECL)
    {
        const INT B = *Code++;
        (Context->*GNatives[B])(*this, Result);
    }

    // port: operands may be unaligned, and object and name operands are 4 byte indices (see ReadScriptObject)
    INT ReadInt() { INT V; memcpy(&V, Code, sizeof(V)); Code += sizeof(V); return V; }
    FLOAT ReadFloat() { FLOAT V; memcpy(&V, Code, sizeof(V)); Code += sizeof(V); return V; }
    _WORD ReadWord() { _WORD V; memcpy(&V, Code, sizeof(V)); Code += sizeof(V); return V; }
    UObject* ReadObject() { UObject* V = ReadScriptObject(Code); Code += 4; return V; }
    FName ReadName() { FName V = ReadScriptName(Code); Code += 4; return V; }
};

// the end of a native's parameters, and the debug info that may follow it
#define P_FINISH \
    { \
        Stack.Code++; \
        if (*Stack.Code == EX_DebugInfo) \
        { \
            Stack.Code++; \
            (Stack.Object->*GNatives[EX_DebugInfo])(Stack, nullptr); \
        } \
    }

// native parameters, read in order by running each one's expression
#define P_GET_UBOOL(Var)         DWORD Var = 0; Stack.Step(Stack.Object, &Var);
#define P_GET_UBOOL_OPTX(Var, D) DWORD Var = D; Stack.Step(Stack.Object, &Var);
#define P_GET_BYTE(Var)          BYTE Var = 0; Stack.Step(Stack.Object, &Var);
#define P_GET_INT(Var)           INT Var = 0; Stack.Step(Stack.Object, &Var);
#define P_GET_INT_OPTX(Var, D)   INT Var = D; Stack.Step(Stack.Object, &Var);
#define P_GET_FLOAT(Var)         FLOAT Var = 0.f; Stack.Step(Stack.Object, &Var);
#define P_GET_FLOAT_OPTX(Var, D) FLOAT Var = D; Stack.Step(Stack.Object, &Var);
#define P_GET_NAME(Var)          FName Var(NAME_None); Stack.Step(Stack.Object, &Var);
#define P_GET_NAME_OPTX(Var, D)  FName Var(D); Stack.Step(Stack.Object, &Var);
#define P_GET_STR(Var)           FString Var; Stack.Step(Stack.Object, &Var);
#define P_GET_OBJECT(Cls, Var)   Cls* Var = nullptr; Stack.Step(Stack.Object, &Var);
#define P_GET_STRUCT(Type, Var)  Type Var; Stack.Step(Stack.Object, &Var);
// the EX_Skip before a short-circuited operand and the number of bytes to skip it
#define P_GET_SKIP_OFFSET(Var)   Stack.Code++; _WORD Var = Stack.ReadWord();

// an out parameter: Var points at the variable the expression named, or at a local copy when it named none
#define P_GET_REF(Type, Var, Zero) \
    Type Var##Temp = Zero; \
    GPropAddr = nullptr; \
    Stack.Step(Stack.Object, &Var##Temp); \
    if (GPropObject) \
        GPropObject->NetDirty(GProperty); \
    Type* Var = GPropAddr ? reinterpret_cast<Type*>(GPropAddr) : &Var##Temp;
#define P_GET_BYTE_REF(Var)  P_GET_REF(BYTE, Var, 0)
#define P_GET_INT_REF(Var)   P_GET_REF(INT, Var, 0)
#define P_GET_FLOAT_REF(Var) P_GET_REF(FLOAT, Var, 0.f)
#define P_GET_OBJECT_REF(Cls, Var) P_GET_REF(Cls*, Var, nullptr)
#define P_GET_STRUCT_REF(Type, Var) \
    Type Var##Temp; \
    GPropAddr = nullptr; \
    Stack.Step(Stack.Object, &Var##Temp); \
    if (GPropObject) \
        GPropObject->NetDirty(GProperty); \
    Type* Var = GPropAddr ? reinterpret_cast<Type*>(GPropAddr) : &Var##Temp;

// an iterator native runs the foreach body itself: the body follows the parameters, then the offset of the
// code after the loop. each pass steps the body until EX_IteratorNext starts the next pass or EX_IteratorPop
// leaves the loop (a break or return inside it).
#define PRE_ITERATOR \
    const INT wEndOffset = Stack.ReadWord(); \
    BYTE* const StartCode = Stack.Code; \
    BYTE IterBuffer[1024];
#define POST_ITERATOR \
    BYTE IterToken; \
    while ((IterToken = *Stack.Code) != EX_IteratorPop && IterToken != EX_IteratorNext) \
        Stack.Step(Stack.Object, IterBuffer); \
    if (*Stack.Code++ == EX_IteratorNext) \
        Stack.Code = StartCode; \
    if (IterToken == EX_IteratorPop) \
        return;
// the loop ran out: continue after it
#define END_ITERATOR \
    Stack.Code = &Stack.Node->Script[wEndOffset + 1];

// the frame an object runs its state code in
struct FStateFrame : public FFrame
{
    FFrame* CurrentFrame;
    UState* StateNode;
    QWORD   ProbeMask;
    _WORD   LatentAction;

    FStateFrame(UObject* InObject)
        : FFrame(InObject)
        , CurrentFrame(nullptr)
        , StateNode(InObject->GetClass())
        , ProbeMask(~0ull)
        , LatentAction(0)
    {
    }
};
