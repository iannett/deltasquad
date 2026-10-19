#include <alloca.h>

#include "Core.h"
#include "UnMath.h"

Native     GNatives[MAX_NATIVES];
Native     GCasts[MAX_CASTS];
INT        GNativeDuplicate;
INT        GCastDuplicate;
UProperty* GProperty;
BYTE*      GPropAddr;
UObject*   GPropObject;
DWORD      GRuntimeUCFlags;
UBOOL      GIsScriptable;

namespace
{
    UBOOL GNativesInitialized;
    UBOOL GCastsInitialized;

    // UTrace indents nested calls by two spaces
    FString GUTraceIndent;

    // where Let writes when its target is None. 8 bytes on the xbox; larger here so 64-bit values fit.
    BYTE GLetScratch[1024];

    // 0x00035ad3. script unicode strings are 16-bit; characters past Latin-1 become 0x7f.
    TCHAR* FromScriptUnicode(const BYTE* Str, TCHAR* Result, INT ResultSize)
    {
        INT i = 0;
        for (; i < ResultSize - 1; i++)
        {
            _WORD Ch;
            memcpy(&Ch, Str + i * 2, sizeof(Ch));
            if (!Ch)
                break;
            Result[i] = Ch < 0x100 ? static_cast<TCHAR>(Ch) : 0x7f;
        }
        Result[i] = 0;
        return Result;
    }

    // starts a UTrace entry for a call
    void UTraceBegin(UObject* Object, UFunction* Function)
    {
        GLog->Logf(NAME_UTrace, TEXT("%sBEGIN %s %s"), *GUTraceIndent, Object->GetName(), Function->GetPathName());
        GUTraceIndent += TEXT("  ");
    }

    void UTraceEnd(UObject* Object, UFunction* Function)
    {
        GUTraceIndent = GUTraceIndent.Left(GUTraceIndent.Len() - 2);
        GLog->Logf(NAME_UTrace, TEXT("%sEND   %s %s"), *GUTraceIndent, Object->GetName(), Function->GetPathName());
    }

    // destroys the constructed values among a function's locals from Offset on
    void DestroyLocals(UFunction* Function, BYTE* Locals, INT FromOffset)
    {
        for (TFieldIterator<UProperty> It(Function); It; ++It)
        {
            if ((It->PropertyFlags & CPF_NeedCtorLink) && It->Offset >= FromOffset)
                It->DestroyValue(Locals + It->Offset);
        }
    }

    // 0x00099819. net fields sort by where they are in their package.
    INT CompareNetFields(UField* A, UField* B)
    {
        return A->GetLinker() && B->GetLinker() ? A->GetLinkerIndex() - B->GetLinkerIndex() : 0;
    }
}

// ---------------------------------------------------------------------------------------------- registration

// 0x000bd67f
BYTE GRegisterNative(INT iNative, const Native& Func)
{
    if (!GNativesInitialized)
    {
        for (INT i = 0; i < MAX_NATIVES; i++)
            GNatives[i] = &UObject::execUndefined;
        GNativesInitialized = 1;
    }
    if (iNative != INDEX_NONE)
    {
        if (iNative < 0 || iNative >= MAX_NATIVES || GNatives[iNative] != &UObject::execUndefined)
            GNativeDuplicate = iNative;
        GNatives[iNative] = Func;
    }
    return 0;
}

// 0x000bd6d5
BYTE GRegisterCast(INT CastCode, const Native& Func)
{
    if (!GCastsInitialized)
    {
        for (INT i = 0; i < MAX_CASTS; i++)
            GCasts[i] = &UObject::execUndefined;
        GCastsInitialized = 1;
    }
    if (CastCode != INDEX_NONE)
    {
        if (CastCode < 0 || CastCode >= MAX_CASTS || GCasts[CastCode] != &UObject::execUndefined)
            GCastDuplicate = CastCode;
        GCasts[CastCode] = Func;
    }
    return 0;
}

// 0x000997d8
void UClass::RegisterNatives()
{
    if (!NativeFunctionTable)
        return;
    for (FNativeEntry* Entry = NativeFunctionTable; Entry->Name; Entry++)
    {
        if (Entry->Index)
            GRegisterNative(Entry->Index, Entry->Func);
    }
}

// 0x0009a732
Native* UClass::FindNative(const TCHAR* NativeName, UBOOL Checked)
{
    Native* Result = nullptr;
    if (NativeFunctionTable)
    {
        for (FNativeEntry* Entry = NativeFunctionTable; Entry->Name; Entry++)
        {
            if (!appStrcmp(Entry->Name, NativeName))
            {
                Result = &Entry->Func;
                break;
            }
        }
    }
    if (Result)
        GLogDetail->Logf(NAME_DevBind, TEXT("Found %s in %s%s"), NativeName, GetName(), TEXT(".dll"));
    else if (Checked)
        GError->Logf(LocalizeError(TEXT("NotInDll"), TEXT("Core")), NativeName, GetName());
    return Result;
}

// 0x0009afbb
void UFunction::Bind()
{
    if (!(FunctionFlags & FUNC_Native))
    {
        Func = &UObject::ProcessInternal;
    }
    else if (iNative)
    {
        Func = GNatives[iNative];
    }
    else if (Native* Found = GetOwnerClass()->FindNative(GetName(), 1))
    {
        Func = *Found;
    }
}

// 0x0009b1f7
void UClass::Bind()
{
    if (!ClassConstructor && (GetFlags() & RF_Native))
    {
        // a native class registered only by name keeps its class in an "autoclass" table entry
        TCHAR ProcName[256];
        appSprintf(ProcName, TEXT("autoclass%s"), GetNameCPP());
        if (Native* Found = FindNative(ProcName, 0))
            ClassConstructor = reinterpret_cast<FNativeEntry*>(reinterpret_cast<BYTE*>(Found) - offsetof(FNativeEntry, Func))->AutoClass->ClassConstructor;
        else
            GError->Logf(TEXT("Can't bind to native class %s"), GetPathName());
    }
    if (!ClassConstructor && SuperField)
    {
        SuperField->Bind();
        ClassConstructor = GetSuperClass()->ClassConstructor;
    }
}

// ---------------------------------------------------------------------------------------------- function lookup

// 0x0009a6f8
UStruct* UState::FindHashedStruct(FName Name)
{
    for (UState* State = this; State; State = static_cast<UState*>(State->SuperField))
    {
        if (!State->FuncHash)
            continue;
        for (UStruct* Struct = State->FuncHash[Name.GetIndex() & (FUNC_HASH_BUCKETS - 1)]; Struct; Struct = Struct->HashNext)
        {
            if (Struct->GetFName() == Name)
                return Struct;
        }
        return nullptr;
    }
    return nullptr;
}

// 0x00022d33. states come first unless Global is set.
UFunction* UObject::FindFunction(FName InName, UBOOL Global) const
{
    if (StateFrame && StateFrame->StateNode && !Global)
    {
        if (UStruct* Found = StateFrame->StateNode->FindHashedStruct(InName))
            return static_cast<UFunction*>(Found);
    }
    return static_cast<UFunction*>(Class->FindHashedStruct(InName));
}

// 0x000244c2
UFunction* UObject::FindFunctionChecked(FName InName, UBOOL Global) const
{
    if (!GIsScriptable)
        return nullptr;

    UFunction* Result = nullptr;
    UFunction* Found = FindFunction(InName, Global);
    if (Found && Found->IsA(UFunction::StaticClass()))
        Result = Found;
    if (!Result)
        GError->Logf(TEXT("Failed to find function %s in %s"), *InName, GetFullName());
    return Result;
}

// ---------------------------------------------------------------------------------------------- linking

// 0x0009c50c. hashes the functions, states and structs declared here on top of a copy of the parent's hash.
void UState::Link(FArchive& Ar, UBOOL Props)
{
    UStruct::Link(Ar, Props);

    for (TFieldIterator<UStruct> It(this); It && It->GetOuter() == this; ++It)
    {
        if (!FuncHash)
        {
            FuncHash = static_cast<UStruct**>(appMalloc(FUNC_HASH_BUCKETS * sizeof(UStruct*)));
            memset(FuncHash, 0, FUNC_HASH_BUCKETS * sizeof(UStruct*));
            for (UState* Super = static_cast<UState*>(SuperField); Super; Super = static_cast<UState*>(Super->SuperField))
            {
                if (Super->FuncHash)
                {
                    memcpy(FuncHash, Super->FuncHash, FUNC_HASH_BUCKETS * sizeof(UStruct*));
                    break;
                }
            }
        }
        const INT Bucket = It->GetFName().GetIndex() & (FUNC_HASH_BUCKETS - 1);
        It->HashNext = FuncHash[Bucket];
        FuncHash[Bucket] = *It;
    }
}

// 0x0009a660
void UState::Destroy()
{
    appFree(FuncHash);
    FuncHash = nullptr;
    UStruct::Destroy();
}

// 0x0009c60e
void UClass::Link(FArchive& Ar, UBOOL Props)
{
    UState::Link(Ar, Props);

    if (NativePropertiesSize && PropertiesSize != NativePropertiesSize)
    {
        GWarn->Logf(TEXT("VERIFY: Class %s size problem: Script=%i C++=%i"), GetName(), PropertiesSize, NativePropertiesSize);
        appExit();
    }

    // replication tables: the parent's records, then one per element of each replicated property declared here
    NetFields.Empty();
    if (SuperField)
        ClassReps = GetSuperClass()->ClassReps;
    else
        ClassReps = TArray<FRepRecord>();

    for (TFieldIterator<UField> It(this); It && It->GetOwnerClass() == this; ++It)
    {
        if (It->IsA(UProperty::StaticClass()))
        {
            UProperty* Property = static_cast<UProperty*>(*It);
            if (Property->PropertyFlags & CPF_Net)
            {
                NetFields.AddItem(*It);
                if (Property->GetOuter() == this)
                {
                    Property->RepIndex = static_cast<_WORD>(ClassReps.Num());
                    for (INT i = 0; i < Property->ArrayDim; i++)
                        ClassReps.AddItem(FRepRecord{ Property, i });
                }
            }
        }
        else if (It->IsA(UFunction::StaticClass()))
        {
            UFunction* Function = static_cast<UFunction*>(*It);
            if ((Function->FunctionFlags & FUNC_Net) && !Function->SuperField)
                NetFields.AddItem(*It);
        }
    }
    NetFields.Shrink();
    Sort(NetFields.GetData(), NetFields.Num(), CompareNetFields);
}

// ---------------------------------------------------------------------------------------------- calling

// 0x000b6cf5. script warnings name the object, function and code offset they come from.
void FFrame::Serialize(const TCHAR* Text, EName Event)
{
    if (Event == NAME_Critical || GIsStrict)
        GError->Logf(TEXT("%s (%s:%04X) %s"), Object->GetFullName(), Node->GetFullName(), static_cast<INT>(Code - Node->Script.GetData()), Text);
    else
        GLog->Logf(NAME_ScriptWarning, TEXT("%s (%s:%04X) %s"), Object->GetFullName(), Node->GetFullName(), static_cast<INT>(Code - Node->Script.GetData()), Text);
}

// 0x000c15f0. calls a function from c++ with its parameters laid out in Parms; Result is unused.
void UObject::ProcessEvent(UFunction* Function, void* Parms, void* Result)
{
    if (!GIsScriptable)
        return;

    // probe functions only run when the current state enables them
    const INT NameIndex = Function->GetFName().GetIndex();
    if (NameIndex >= NAME_PROBEMIN && NameIndex < NAME_PROBEMAX && StateFrame && !(StateFrame->ProbeMask & (1ull << (NameIndex - NAME_PROBEMIN))))
        return;
    if (IsPendingKill() || Function->iNative)
        return;
    if ((Function->FunctionFlags & FUNC_Native) && ProcessRemoteFunction(Function, Parms, nullptr))
        return;

    const UBOOL Trace = GIsUTracing && !(Function->FunctionFlags & FUNC_Operator);
    if (Trace)
        UTraceBegin(this, Function);

    BYTE* Locals = Function->PropertiesSize ? static_cast<BYTE*>(alloca(Function->PropertiesSize)) : nullptr;
    FFrame NewStack(this, Function, 0, Locals);
    memcpy(Locals, Parms, Function->ParmsSize);
    memset(Locals + Function->ParmsSize, 0, Function->PropertiesSize - Function->ParmsSize);
    (this->*Function->Func)(NewStack, Locals + Function->ReturnValueOffset);
    memcpy(Parms, Locals, Function->ParmsSize);

    // the caller owns the parameters; only the other locals are destroyed here
    DestroyLocals(Function, Locals, Function->ParmsSize);

    if (Trace)
        UTraceEnd(this, Function);
}

// 0x000c0215. runs a script function's statements, then its return expression.
void UObject::ProcessInternal(FFrame& Stack, RESULT_DECL)
{
    UFunction* Function = static_cast<UFunction*>(Stack.Node);
    // singular functions borrow an object flag bit with the same value as FUNC_Singular
    const DWORD SingularFlag = Function->FunctionFlags & FUNC_Singular;
    if (ProcessRemoteFunction(Function, Stack.Locals, nullptr))
        return;

    const INT NameIndex = Function->GetFName().GetIndex();
    if (NameIndex >= NAME_PROBEMIN && NameIndex < NAME_PROBEMAX && StateFrame && !(StateFrame->ProbeMask & (1ull << (NameIndex - NAME_PROBEMIN))))
        return;
    if (ObjectFlags & SingularFlag)
        return;
    ObjectFlags |= SingularFlag;

    // statement results go here. the xbox clears 8 bytes, the size of a string there.
    BYTE Buffer[1024];
    memset(Buffer, 0, sizeof(FString));
    while (*Stack.Code != EX_Return)
        Stack.Step(Stack.Object, Buffer);
    Stack.Code++;
    Stack.Step(Stack.Object, Result);

    if (Stack.Node && Stack.Code <= &Stack.Node->Script.Last() && *Stack.Code == EX_DebugInfo)
    {
        Stack.Code++;
        (Stack.Object->*GNatives[EX_DebugInfo])(Stack, Result);
    }
    ObjectFlags &= ~SingularFlag;
}

// 0x000c12e7. calls a function from script, evaluating its parameters from the caller's code.
void UObject::CallFunction(FFrame& Stack, RESULT_DECL, UFunction* Function)
{
    const UBOOL Trace = GIsUTracing && !(Function->FunctionFlags & FUNC_Operator);
    if (Trace)
        UTraceBegin(this, Function);

    if (Function->iNative)
    {
        (this->*Function->Func)(Stack, Result);
    }
    else
    {
        // a native without a fixed index runs directly unless it is sent over the network; then only its
        // parameters are evaluated here
        UBOOL SkipCall = 0;
        if (Function->FunctionFlags & FUNC_Native)
        {
            BYTE RemoteBuffer[1024];
            if (!ProcessRemoteFunction(Function, RemoteBuffer, &Stack))
            {
                (this->*Function->Func)(Stack, Result);
                if (Trace)
                    UTraceEnd(this, Function);
                return;
            }
            SkipCall = 1;
        }

        BYTE* Locals = Function->PropertiesSize ? static_cast<BYTE*>(alloca(Function->PropertiesSize)) : nullptr;
        memset(Locals, 0, Function->PropertiesSize);
        FFrame NewStack(this, Function, 0, Locals);

        // out parameters are copied back to their source once the call returns. the xbox keeps room for 16.
        struct FOutParm { UProperty* Property; BYTE* PropAddr; };
        FOutParm OutParms[16];
        FOutParm* LastOut = OutParms;

        for (UProperty* Property = static_cast<UProperty*>(Function->Children); *Stack.Code != EX_EndFunctionParms;
            Property = static_cast<UProperty*>(Property->Next))
        {
            GPropAddr = nullptr;
            GPropObject = nullptr;
            Stack.Step(Stack.Object, NewStack.Locals + Property->Offset);
            if ((Property->PropertyFlags & CPF_OutParm) && GPropAddr)
            {
                LastOut->Property = Property;
                LastOut->PropAddr = GPropAddr;
                LastOut++;
                if (GPropObject && GProperty && (GProperty->PropertyFlags & CPF_Net))
                    GPropObject->NetDirty(GProperty);
            }
        }
        Stack.Code++;
        if (*Stack.Code == EX_DebugInfo)
        {
            Stack.Code++;
            (Stack.Object->*GNatives[EX_DebugInfo])(Stack, nullptr);
        }

        if (!SkipCall)
            ProcessInternal(NewStack, Result);

        while (--LastOut >= OutParms)
            LastOut->Property->CopyCompleteValue(LastOut->PropAddr, NewStack.Locals + LastOut->Property->Offset, nullptr);

        DestroyLocals(Function, NewStack.Locals, 0);
    }

    if (Trace)
        UTraceEnd(this, Function);
}

// ---------------------------------------------------------------------------------------------- bytecode

// 0x000b6d7d
void UObject::execUndefined(FFrame& Stack, RESULT_DECL)
{
    Stack.Logf(NAME_Critical, TEXT("Unknown code token %02X"), Stack.Code[-1]);
}

// 0x000b6d9f
void UObject::execLocalVariable(FFrame& Stack, RESULT_DECL)
{
    GProperty = static_cast<UProperty*>(Stack.ReadObject());
    GPropAddr = Stack.Locals + GProperty->Offset;
    GPropObject = nullptr;
    if (Result)
        GProperty->CopyCompleteValue(Result, GPropAddr, nullptr);
}
IMPLEMENT_FUNCTION(UObject, EX_LocalVariable, execLocalVariable)

// 0x000b6de1
void UObject::execInstanceVariable(FFrame& Stack, RESULT_DECL)
{
    GProperty = static_cast<UProperty*>(Stack.ReadObject());
    GPropAddr = reinterpret_cast<BYTE*>(this) + GProperty->Offset;
    GPropObject = this;
    if (Result)
        GProperty->CopyCompleteValue(Result, GPropAddr, nullptr);
}
IMPLEMENT_FUNCTION(UObject, EX_InstanceVariable, execInstanceVariable)

// 0x000bd8d4
void UObject::execDefaultVariable(FFrame& Stack, RESULT_DECL)
{
    GProperty = static_cast<UProperty*>(Stack.ReadObject());
    GPropAddr = reinterpret_cast<BYTE*>(Class->GetDefaultObject()) + GProperty->Offset;
    GPropObject = nullptr;
    if (Result)
        GProperty->CopyCompleteValue(Result, GPropAddr, nullptr);
}
IMPLEMENT_FUNCTION(UObject, EX_DefaultVariable, execDefaultVariable)

// 0x000c04ed. each case is an EX_Case token, the offset of the next case and the value; 0xffff marks default.
void UObject::execSwitch(FFrame& Stack, RESULT_DECL)
{
    // a size of 0 means the values are strings
    const BYTE bSize = *Stack.Code++;
    BYTE SwitchBuffer[1024];
    BYTE CaseBuffer[1024];
    memset(SwitchBuffer, 0, sizeof(FString));
    memset(CaseBuffer, 0, sizeof(FString));
    Stack.Step(Stack.Object, SwitchBuffer);

    Stack.Code++;
    for (;;)
    {
        const _WORD wNext = Stack.ReadWord();
        if (wNext == MAXWORD)
            break;
        Stack.Step(Stack.Object, CaseBuffer);
        const UBOOL Matched = bSize ? !memcmp(SwitchBuffer, CaseBuffer, bSize)
            : *reinterpret_cast<FString*>(SwitchBuffer) == *reinterpret_cast<FString*>(CaseBuffer);
        if (Matched)
            break;
        Stack.Code = Stack.Node->Script.GetData() + wNext;
        Stack.Code++;
    }

    if (!bSize)
    {
        reinterpret_cast<FString*>(CaseBuffer)->~FString();
        reinterpret_cast<FString*>(SwitchBuffer)->~FString();
    }
}
IMPLEMENT_FUNCTION(UObject, EX_Switch, execSwitch)

// 0x000b704b
void UObject::execJump(FFrame& Stack, RESULT_DECL)
{
    const _WORD Offset = Stack.ReadWord();
    Stack.Code = Stack.Node->Script.GetData() + Offset;
}
IMPLEMENT_FUNCTION(UObject, EX_Jump, execJump)

// 0x000b7069
void UObject::execJumpIfNot(FFrame& Stack, RESULT_DECL)
{
    const _WORD Offset = Stack.ReadWord();
    UBOOL Value = 0;
    Stack.Step(Stack.Object, &Value);
    if (!Value)
        Stack.Code = Stack.Node->Script.GetData() + Offset;
}
IMPLEMENT_FUNCTION(UObject, EX_JumpIfNot, execJumpIfNot)

// 0x000b6ff5
void UObject::execStop(FFrame& Stack, RESULT_DECL)
{
    Stack.Code = nullptr;
}
IMPLEMENT_FUNCTION(UObject, EX_Stop, execStop)

// 0x000b70ae
void UObject::execAssert(FFrame& Stack, RESULT_DECL)
{
    const _WORD Line = Stack.ReadWord();
    UBOOL Value = 0;
    Stack.Step(Stack.Object, &Value);
    if (!Value)
        Stack.Logf(NAME_Critical, TEXT("Assertion failed, line %i"), Line);
}
IMPLEMENT_FUNCTION(UObject, EX_Assert, execAssert)

// 0x000b7000. reached when a case falls through into the next one.
void UObject::execCase(FFrame& Stack, RESULT_DECL)
{
    const _WORD wNext = Stack.ReadWord();
    if (wNext != MAXWORD)
    {
        BYTE Buffer[1024];
        memset(Buffer, 0, sizeof(FString));
        Stack.Step(Stack.Object, Buffer);
    }
}
IMPLEMENT_FUNCTION(UObject, EX_Case, execCase)

// 0x0003ed42, shared with execIterator
void UObject::execNothing(FFrame& Stack, RESULT_DECL)
{
}
IMPLEMENT_FUNCTION(UObject, EX_Nothing, execNothing)

// 0x000bede6
void UObject::execGotoLabel(FFrame& Stack, RESULT_DECL)
{
    FName Label = NAME_None;
    Stack.Step(Stack.Object, &Label);
    if (!GotoLabel(Label))
        Stack.Logf(TEXT("GotoLabel (%s): Label not found"), *Label);
}
IMPLEMENT_FUNCTION(UObject, EX_GotoLabel, execGotoLabel)

// 0x000bf2c7
void UObject::execEatString(FFrame& Stack, RESULT_DECL)
{
    FString Temp;
    Stack.Step(Stack.Object, &Temp);
}
IMPLEMENT_FUNCTION(UObject, EX_EatString, execEatString)

// 0x000bdc87
void UObject::execLet(FFrame& Stack, RESULT_DECL)
{
    GPropAddr = nullptr;
    Stack.Step(Stack.Object, nullptr);
    if (!GPropAddr)
    {
        Stack.Logf(NAME_ScriptWarning, TEXT("Attempt to assign variable through None"));
        GPropAddr = GLetScratch;
        memset(GLetScratch, 0, sizeof(FString));
    }
    else if (GPropObject && GProperty && (GProperty->PropertyFlags & CPF_Net))
    {
        GPropObject->NetDirty(GProperty);
    }

    if (GRuntimeUCFlags & RUC_ArrayLengthSet)
    {
        // assigning to a dynamic array's length resizes it
        GRuntimeUCFlags &= ~RUC_ArrayLengthSet;
        FArray* Array = reinterpret_cast<FArray*>(GPropAddr);
        UArrayProperty* ArrayProperty = static_cast<UArrayProperty*>(GProperty);
        INT NewSize = 0;
        Stack.Step(Stack.Object, &NewSize);
        if (NewSize > Array->Num())
        {
            Array->AddZeroed(NewSize - Array->Num(), ArrayProperty->Inner->GetElementSize());
        }
        else if (NewSize < Array->Num())
        {
            for (INT i = Array->Num() - 1; i >= NewSize; i--)
                ArrayProperty->Inner->DestroyValue(static_cast<BYTE*>(Array->GetData()) + ArrayProperty->Inner->GetElementSize() * i);
            Array->Remove(NewSize, Array->Num() - NewSize, ArrayProperty->Inner->GetElementSize());
        }
    }
    else
    {
        Stack.Step(Stack.Object, GPropAddr);
    }
}
IMPLEMENT_FUNCTION(UObject, EX_Let, execLet)

// 0x000becb8. as an lvalue (no Result) an index past the end grows the array.
void UObject::execDynArrayElement(FFrame& Stack, RESULT_DECL)
{
    INT Index = 0;
    Stack.Step(Stack.Object, &Index);

    GProperty = nullptr;
    Stack.Step(this, nullptr);
    GPropObject = this;
    if (!GProperty || !GPropAddr)
        return;

    UArrayProperty* ArrayProperty = static_cast<UArrayProperty*>(GProperty);
    FArray* Array = reinterpret_cast<FArray*>(GPropAddr);
    if (Index >= Array->Num() || Index < 0)
    {
        if (!Result && Index >= 0)
        {
            Array->AddZeroed(Index - Array->Num() + 1, ArrayProperty->Inner->GetElementSize());
        }
        else
        {
            Stack.Logf(TEXT("Accessed array '%s' out of bounds (%i/%i)"), ArrayProperty->GetName(), Index, Array->Num());
            GPropAddr = nullptr;
            GPropObject = nullptr;
            if (Result)
                memset(Result, 0, ArrayProperty->Inner->GetElementSize());
            return;
        }
    }
    GPropAddr = static_cast<BYTE*>(Array->GetData()) + ArrayProperty->Inner->GetElementSize() * Index;
    if (Result)
        ArrayProperty->Inner->CopySingleValue(Result, GPropAddr, nullptr);
}
IMPLEMENT_FUNCTION(UObject, EX_DynArrayElement, execDynArrayElement)

// 0x000bfb80
void UObject::execNew(FFrame& Stack, RESULT_DECL)
{
    UObject* Outer = nullptr;
    Stack.Step(Stack.Object, &Outer);
    FString Name(TEXT(""));
    Stack.Step(Stack.Object, &Name);
    DWORD Flags = 0;
    Stack.Step(Stack.Object, &Flags);
    UClass* NewClass = nullptr;
    Stack.Step(Stack.Object, &NewClass);

    if (Flags & ~0x00784085)
        Stack.Logf(TEXT("new: Flags %08X not allowed"), Flags & ~0x00784085);
    if (!Outer)
        Outer = GetTransientPackage();
    const FName NewName = Name.Len() ? FName(*Name, FNAME_Add) : FName(NAME_None);
    *static_cast<UObject**>(Result) = StaticConstructObject(NewClass, Outer, NewName, Flags & 0x00784085, nullptr, &Stack, nullptr);
}
IMPLEMENT_FUNCTION(UObject, EX_New, execNew)

// 0x000bd924. the operand is the class; skip size and result size follow, for when it is None.
void UObject::execClassContext(FFrame& Stack, RESULT_DECL)
{
    UClass* ClassContext = nullptr;
    Stack.Step(Stack.Object, &ClassContext);
    if (ClassContext)
    {
        Stack.Code += 3;
        Stack.Step(ClassContext->GetDefaultObject(), Result);
    }
    else
    {
        Stack.Logf(TEXT("Accessed null class context"));
        const _WORD wSkip = Stack.ReadWord();
        // the xbox size of the result, which can be smaller than the port's for pointers
        const BYTE bSize = *Stack.Code++;
        Stack.Code += wSkip;
        GPropAddr = nullptr;
        GPropObject = nullptr;
        GProperty = nullptr;
        if (Result)
            memset(Result, 0, bSize);
    }
}
IMPLEMENT_FUNCTION(UObject, EX_ClassContext, execClassContext)

// 0x000bdee9
void UObject::execMetaCast(FFrame& Stack, RESULT_DECL)
{
    UClass* MetaClass = static_cast<UClass*>(Stack.ReadObject());
    UObject* Castee = nullptr;
    Stack.Step(Stack.Object, &Castee);
    *static_cast<UObject**>(Result) = Castee && Castee->IsA(UClass::StaticClass()) && static_cast<UClass*>(Castee)->IsChildOf(MetaClass) ? Castee : nullptr;
}
IMPLEMENT_FUNCTION(UObject, EX_MetaCast, execMetaCast)

// 0x000b70fc
void UObject::execLetBool(FFrame& Stack, RESULT_DECL)
{
    GPropAddr = nullptr;
    GProperty = nullptr;
    GPropObject = nullptr;
    Stack.Step(Stack.Object, nullptr);
    UBoolProperty* Property = static_cast<UBoolProperty*>(GProperty);
    BITFIELD* BoolAddr = reinterpret_cast<BITFIELD*>(GPropAddr);
    if (GPropObject && Property && (Property->PropertyFlags & CPF_Net))
        GPropObject->NetDirty(Property);

    UBOOL NewValue = 0;
    Stack.Step(Stack.Object, &NewValue);
    if (BoolAddr)
    {
        if (NewValue)
            *BoolAddr |= 1u << Property->BitIndex;
        else
            *BoolAddr &= ~(1u << Property->BitIndex);
    }
}
IMPLEMENT_FUNCTION(UObject, EX_LetBool, execLetBool)

// 0x000b6fe4. leaves the token for CallFunction's parameter loop to see.
void UObject::execEndFunctionParms(FFrame& Stack, RESULT_DECL)
{
    GPropObject = nullptr;
    Stack.Code--;
}
IMPLEMENT_FUNCTION(UObject, EX_EndFunctionParms, execEndFunctionParms)

// 0x000b7197
void UObject::execSelf(FFrame& Stack, RESULT_DECL)
{
    *static_cast<UObject**>(Result) = this;
}
IMPLEMENT_FUNCTION(UObject, EX_Self, execSelf)

// 0x000bee42. evaluates an expression on another object; skip size and result size follow, for when it is None.
void UObject::execContext(FFrame& Stack, RESULT_DECL)
{
    UObject* NewContext = nullptr;
    Stack.Step(Stack.Object, &NewContext);
    if (NewContext)
    {
        Stack.Code += 3;
        Stack.Step(NewContext, Result);
    }
    else
    {
        // the xbox would also tell a script debugger here, but nothing in the game installs one
        Stack.Logf(TEXT("Accessed None: %s.%s"), GPropObject ? GPropObject->GetName() : TEXT(""), GProperty ? GProperty->GetName() : TEXT(""));
        const _WORD wSkip = Stack.ReadWord();
        // the xbox size of the result, which can be smaller than the port's for pointers
        const BYTE bSize = *Stack.Code++;
        Stack.Code += wSkip;
        GPropAddr = nullptr;
        GProperty = nullptr;
        GPropObject = nullptr;
        if (Result)
            memset(Result, 0, bSize);
    }
}
IMPLEMENT_FUNCTION(UObject, EX_Context, execContext)

// 0x000b6e24
void UObject::execArrayElement(FFrame& Stack, RESULT_DECL)
{
    INT Index = 0;
    Stack.Step(Stack.Object, &Index);

    GProperty = nullptr;
    Stack.Step(this, nullptr);
    GPropObject = this;
    if (!GProperty || !GPropAddr)
        return;

    if (Index >= GProperty->ArrayDim || Index < 0)
    {
        Stack.Logf(TEXT("Accessed array out of bounds (%i/%i)"), Index, GProperty->ArrayDim);
        Index = Index < 0 ? 0 : GProperty->ArrayDim - 1;
    }
    GPropAddr += Index * GProperty->GetElementSize();
    if (Result)
        GProperty->CopySingleValue(Result, GPropAddr, nullptr);
}
IMPLEMENT_FUNCTION(UObject, EX_ArrayElement, execArrayElement)

// 0x000b71a0
void UObject::execVirtualFunction(FFrame& Stack, RESULT_DECL)
{
    const FName FunctionName = Stack.ReadName();
    CallFunction(Stack, Result, FindFunctionChecked(FunctionName, 0));
}
IMPLEMENT_FUNCTION(UObject, EX_VirtualFunction, execVirtualFunction)

// 0x000b71d1
void UObject::execFinalFunction(FFrame& Stack, RESULT_DECL)
{
    UFunction* Function = static_cast<UFunction*>(Stack.ReadObject());
    CallFunction(Stack, Result, Function);
}
IMPLEMENT_FUNCTION(UObject, EX_FinalFunction, execFinalFunction)

// 0x000b73eb. the xbox shares this body with execObjectConst and execNameConst, which are 4 bytes there too.
void UObject::execIntConst(FFrame& Stack, RESULT_DECL)
{
    *static_cast<INT*>(Result) = Stack.ReadInt();
}
IMPLEMENT_FUNCTION(UObject, EX_IntConst, execIntConst)

// 0x000b73cf
void UObject::execFloatConst(FFrame& Stack, RESULT_DECL)
{
    *static_cast<FLOAT*>(Result) = Stack.ReadFloat();
}
IMPLEMENT_FUNCTION(UObject, EX_FloatConst, execFloatConst)

// 0x000c06de
void UObject::execStringConst(FFrame& Stack, RESULT_DECL)
{
    *static_cast<FString*>(Result) = reinterpret_cast<const TCHAR*>(Stack.Code);
    while (*Stack.Code)
        Stack.Code++;
    Stack.Code++;
}
IMPLEMENT_FUNCTION(UObject, EX_StringConst, execStringConst)

// 0x000b73eb on the xbox
void UObject::execObjectConst(FFrame& Stack, RESULT_DECL)
{
    *static_cast<UObject**>(Result) = Stack.ReadObject();
}
IMPLEMENT_FUNCTION(UObject, EX_ObjectConst, execObjectConst)

// 0x000b73eb on the xbox
void UObject::execNameConst(FFrame& Stack, RESULT_DECL)
{
    *static_cast<FName*>(Result) = Stack.ReadName();
}
IMPLEMENT_FUNCTION(UObject, EX_NameConst, execNameConst)

// 0x000b9b47
void UObject::execRotationConst(FFrame& Stack, RESULT_DECL)
{
    FRotator* Rotator = static_cast<FRotator*>(Result);
    Rotator->Pitch = Stack.ReadInt();
    Rotator->Yaw = Stack.ReadInt();
    Rotator->Roll = Stack.ReadInt();
}
IMPLEMENT_FUNCTION(UObject, EX_RotationConst, execRotationConst)

// 0x000b9b7d
void UObject::execVectorConst(FFrame& Stack, RESULT_DECL)
{
    memcpy(Result, Stack.Code, sizeof(FVector));
    Stack.Code += sizeof(FVector);
}
IMPLEMENT_FUNCTION(UObject, EX_VectorConst, execVectorConst)

// 0x000b7403
void UObject::execByteConst(FFrame& Stack, RESULT_DECL)
{
    *static_cast<BYTE*>(Result) = *Stack.Code++;
}
IMPLEMENT_FUNCTION(UObject, EX_ByteConst, execByteConst)

// 0x000b7425, shared with execFalse and execNoObject
void UObject::execIntZero(FFrame& Stack, RESULT_DECL)
{
    *static_cast<INT*>(Result) = 0;
}
IMPLEMENT_FUNCTION(UObject, EX_IntZero, execIntZero)

// 0x000b7418, shared with execTrue
void UObject::execIntOne(FFrame& Stack, RESULT_DECL)
{
    *static_cast<INT*>(Result) = 1;
}
IMPLEMENT_FUNCTION(UObject, EX_IntOne, execIntOne)

// 0x000b7418 on the xbox
void UObject::execTrue(FFrame& Stack, RESULT_DECL)
{
    *static_cast<UBOOL*>(Result) = 1;
}
IMPLEMENT_FUNCTION(UObject, EX_True, execTrue)

// 0x000b7425 on the xbox
void UObject::execFalse(FFrame& Stack, RESULT_DECL)
{
    *static_cast<UBOOL*>(Result) = 0;
}
IMPLEMENT_FUNCTION(UObject, EX_False, execFalse)

// 0x000b6fa0
void UObject::execNativeParm(FFrame& Stack, RESULT_DECL)
{
    UProperty* Property = static_cast<UProperty*>(Stack.ReadObject());
    if (Result)
    {
        GPropAddr = Stack.Locals + Property->Offset;
        GPropObject = nullptr;
        Property->CopyCompleteValue(Result, Stack.Locals + Property->Offset, nullptr);
    }
}
IMPLEMENT_FUNCTION(UObject, EX_NativeParm, execNativeParm)

// 0x000b7425 on the xbox
void UObject::execNoObject(FFrame& Stack, RESULT_DECL)
{
    *static_cast<UObject**>(Result) = nullptr;
}
IMPLEMENT_FUNCTION(UObject, EX_NoObject, execNoObject)

// 0x000b742f
void UObject::execIntConstByte(FFrame& Stack, RESULT_DECL)
{
    *static_cast<INT*>(Result) = *Stack.Code++;
}
IMPLEMENT_FUNCTION(UObject, EX_IntConstByte, execIntConstByte)

// 0x000b6f47. reads the variable token that follows, peeking at its property operand for the bit.
void UObject::execBoolVariable(FFrame& Stack, RESULT_DECL)
{
    const INT B = *Stack.Code++;
    UBoolProperty* Property = static_cast<UBoolProperty*>(ReadScriptObject(Stack.Code));
    (this->*GNatives[B])(Stack, nullptr);
    GProperty = Property;
    GPropObject = this;
    if (Result)
        *static_cast<BITFIELD*>(Result) = GPropAddr && (*reinterpret_cast<BITFIELD*>(GPropAddr) & (1u << Property->BitIndex)) ? 1 : 0;
}
IMPLEMENT_FUNCTION(UObject, EX_BoolVariable, execBoolVariable)

// 0x000b7445. a null class lets everything through.
void UObject::execDynamicCast(FFrame& Stack, RESULT_DECL)
{
    UClass* CastClass = static_cast<UClass*>(Stack.ReadObject());
    UObject* Castee = nullptr;
    Stack.Step(Stack.Object, &Castee);
    *static_cast<UObject**>(Result) = Castee && (!CastClass || Castee->IsA(CastClass)) ? Castee : nullptr;
}
IMPLEMENT_FUNCTION(UObject, EX_DynamicCast, execDynamicCast)

// 0x0003ed42 on the xbox. iterator natives run the loop themselves.
void UObject::execIterator(FFrame& Stack, RESULT_DECL)
{
}
IMPLEMENT_FUNCTION(UObject, EX_Iterator, execIterator)

// 0x000b7221
void UObject::execStructCmpEq(FFrame& Stack, RESULT_DECL)
{
    UStruct* Struct = static_cast<UStruct*>(Stack.ReadObject());
    BYTE* Buffer1 = Struct->PropertiesSize ? static_cast<BYTE*>(alloca(Struct->PropertiesSize)) : nullptr;
    BYTE* Buffer2 = Struct->PropertiesSize ? static_cast<BYTE*>(alloca(Struct->PropertiesSize)) : nullptr;
    memset(Buffer1, 0, Struct->PropertiesSize);
    memset(Buffer2, 0, Struct->PropertiesSize);
    Stack.Step(this, Buffer1);
    Stack.Step(this, Buffer2);
    *static_cast<UBOOL*>(Result) = Struct->StructCompare(Buffer1, Buffer2);
}
IMPLEMENT_FUNCTION(UObject, EX_StructCmpEq, execStructCmpEq)

// 0x000b72f7
void UObject::execStructCmpNe(FFrame& Stack, RESULT_DECL)
{
    UStruct* Struct = static_cast<UStruct*>(Stack.ReadObject());
    BYTE* Buffer1 = Struct->PropertiesSize ? static_cast<BYTE*>(alloca(Struct->PropertiesSize)) : nullptr;
    BYTE* Buffer2 = Struct->PropertiesSize ? static_cast<BYTE*>(alloca(Struct->PropertiesSize)) : nullptr;
    memset(Buffer1, 0, Struct->PropertiesSize);
    memset(Buffer2, 0, Struct->PropertiesSize);
    Stack.Step(this, Buffer1);
    Stack.Step(this, Buffer2);
    *static_cast<UBOOL*>(Result) = !Struct->StructCompare(Buffer1, Buffer2);
}
IMPLEMENT_FUNCTION(UObject, EX_StructCmpNe, execStructCmpNe)

// 0x000c070b
void UObject::execUnicodeStringConst(FFrame& Stack, RESULT_DECL)
{
    TCHAR Converted[1024];
    *static_cast<FString*>(Result) = FromScriptUnicode(Stack.Code, Converted, ARRAY_COUNT(Converted));
    while (Stack.Code[0] || Stack.Code[1])
        Stack.Code += 2;
    Stack.Code += 2;
}
IMPLEMENT_FUNCTION(UObject, EX_UnicodeStringConst, execUnicodeStringConst)

// 0x000c033c
void UObject::execStructMember(FFrame& Stack, RESULT_DECL)
{
    UProperty* Property = static_cast<UProperty*>(Stack.ReadObject());
    UStruct* Struct = CastChecked<UStruct>(Property->GetOuter());

    // the struct value is evaluated into a copy unless its members are read in place
    BYTE* Buffer = nullptr;
    if (!(Struct->StructFlags & STRUCT_Unknown4))
    {
        Buffer = Struct->PropertiesSize ? static_cast<BYTE*>(alloca(Struct->PropertiesSize)) : nullptr;
        memset(Buffer, 0, Struct->PropertiesSize);
    }
    GPropAddr = nullptr;
    Stack.Step(this, Buffer);
    GProperty = Property;
    GPropObject = this;

    if (Struct->StructFlags & STRUCT_Unknown4)
    {
        if (Result && GPropAddr)
            Property->CopyCompleteValue(Result, GPropAddr + Property->Offset, nullptr);
        if (GPropAddr)
            GPropAddr += Property->Offset;
    }
    else
    {
        if (GPropAddr)
            GPropAddr += Property->Offset;
        if (Result)
            Property->CopyCompleteValue(Result, Buffer + Property->Offset, nullptr);
        for (TFieldIterator<UProperty> It(Struct); It; ++It)
        {
            if (It->PropertyFlags & CPF_NeedCtorLink)
                It->DestroyValue(Buffer + It->Offset);
        }
    }
}
IMPLEMENT_FUNCTION(UObject, EX_StructMember, execStructMember)

// 0x000b6ef4. as an lvalue it flags the following Let to resize the array.
void UObject::execDynArrayLength(FFrame& Stack, RESULT_DECL)
{
    GProperty = nullptr;
    Stack.Step(this, nullptr);
    GPropObject = this;
    if (GPropAddr)
    {
        FArray* Array = reinterpret_cast<FArray*>(GPropAddr);
        if (!Result)
            GRuntimeUCFlags |= RUC_ArrayLengthSet;
        else
            *static_cast<INT*>(Result) = Array->Num();
    }
}
IMPLEMENT_FUNCTION(UObject, EX_DynArrayLength, execDynArrayLength)

// 0x000b71f0
void UObject::execGlobalFunction(FFrame& Stack, RESULT_DECL)
{
    const FName FunctionName = Stack.ReadName();
    CallFunction(Stack, Result, FindFunctionChecked(FunctionName, 1));
}
IMPLEMENT_FUNCTION(UObject, EX_GlobalFunction, execGlobalFunction)

// 0x000b749d
void UObject::execPrimitiveCast(FFrame& Stack, RESULT_DECL)
{
    const INT B = *Stack.Code++;
    (Stack.Object->*GCasts[B])(Stack, Result);
}
IMPLEMENT_FUNCTION(UObject, EX_PrimitiveCast, execPrimitiveCast)

// 0x000bd9ce
void UObject::execDynArrayInsert(FFrame& Stack, RESULT_DECL)
{
    GPropObject = this;
    GProperty = nullptr;
    Stack.Step(this, nullptr);
    UArrayProperty* ArrayProperty = GProperty && GProperty->IsA(UArrayProperty::StaticClass()) ? static_cast<UArrayProperty*>(GProperty) : nullptr;
    FArray* Array = reinterpret_cast<FArray*>(GPropAddr);

    INT Index = 0;
    Stack.Step(Stack.Object, &Index);
    INT Count = 0;
    Stack.Step(Stack.Object, &Count);
    if (!Array || !Count)
        return;

    if (Count < 0)
    {
        Stack.Logf(TEXT("Attempt to insert a negative number of elements"));
        return;
    }
    if (Index < 0 || Index > Array->Num())
    {
        Stack.Logf(TEXT("Attempt to insert %i elements at %i an %i-element array"), Count, Index, Array->Num());
        Index = Index < 0 ? 0 : Array->Num();
    }
    Array->InsertZeroed(Index, Count, ArrayProperty->Inner->GetElementSize());
}
IMPLEMENT_FUNCTION(UObject, EX_DynArrayInsert, execDynArrayInsert)

// 0x000bdaea
void UObject::execDynArrayRemove(FFrame& Stack, RESULT_DECL)
{
    GProperty = nullptr;
    GPropObject = this;
    Stack.Step(this, nullptr);
    UArrayProperty* ArrayProperty = GProperty && GProperty->IsA(UArrayProperty::StaticClass()) ? static_cast<UArrayProperty*>(GProperty) : nullptr;
    FArray* Array = reinterpret_cast<FArray*>(GPropAddr);

    INT Index = 0;
    Stack.Step(Stack.Object, &Index);
    INT Count = 0;
    Stack.Step(Stack.Object, &Count);
    if (!Array || !Count)
        return;

    if (Count < 0)
    {
        Stack.Logf(TEXT("Attempt to remove a negative number of elements"));
        return;
    }
    if (Index < 0 || Index >= Array->Num() || Index + Count > Array->Num())
    {
        if (Count == 1)
            Stack.Logf(TEXT("Attempt to remove element %i in an %i-element array"), Index, Array->Num());
        else
            Stack.Logf(TEXT("Attempt to remove elements %i through %i in an %i-element array"), Index, Index + Count - 1, Array->Num());
        Index = Index < 0 ? 0 : (Index >= Array->Num() ? Array->Num() : Index);
        if (Index + Count > Array->Num())
            Count = Array->Num() - Index;
    }

    for (INT i = Index + Count - 1; i >= Index; i--)
        ArrayProperty->Inner->DestroyValue(static_cast<BYTE*>(Array->GetData()) + ArrayProperty->Inner->GetElementSize() * i);
    Array->Remove(Index, Count, ArrayProperty->Inner->GetElementSize());
}
IMPLEMENT_FUNCTION(UObject, EX_DynArrayRemove, execDynArrayRemove)

// 0x000c18cb. the version is checked first; the xbox steps back over it when it does not match.
void UObject::execDebugInfo(FFrame& Stack, RESULT_DECL)
{
    const INT Version = Stack.ReadInt();
    if (Version != DEBUG_INFO_VERSION)
    {
        Stack.Code -= 5;
        return;
    }
    // line, position and the opcode name are only used by a script debugger, which the game never installs
    Stack.ReadInt();
    Stack.ReadInt();
    while (*Stack.Code)
        Stack.Code++;
    Stack.Code++;
}
IMPLEMENT_FUNCTION(UObject, EX_DebugInfo, execDebugInfo)

// 0x000bdde2. calls the delegate's target if it has one, else the default function.
void UObject::execDelegateFunction(FFrame& Stack, RESULT_DECL)
{
    UProperty* DelegateProperty = static_cast<UProperty*>(Stack.ReadObject());
    const FName DelegateName = Stack.ReadName();
    FScriptDelegate* Delegate = reinterpret_cast<FScriptDelegate*>(reinterpret_cast<BYTE*>(this) + DelegateProperty->Offset);
    if (Delegate->Object && Delegate->Object->IsPendingKill())
    {
        Delegate->Object = nullptr;
        Delegate->FunctionName = NAME_None;
    }
    if (Delegate->Object)
        Delegate->Object->CallFunction(Stack, Result, Delegate->Object->FindFunctionChecked(Delegate->FunctionName, 0));
    else
        CallFunction(Stack, Result, FindFunctionChecked(DelegateName, 0));
}
IMPLEMENT_FUNCTION(UObject, EX_DelegateFunction, execDelegateFunction)

// 0x000bde5c
void UObject::execDelegateProperty(FFrame& Stack, RESULT_DECL)
{
    const FName FunctionName = Stack.ReadName();
    FScriptDelegate* Delegate = static_cast<FScriptDelegate*>(Result);
    Delegate->FunctionName = FunctionName;
    Delegate->Object = FunctionName != NAME_None ? this : nullptr;
}
IMPLEMENT_FUNCTION(UObject, EX_DelegateProperty, execDelegateProperty)

// 0x000bde7f
void UObject::execLetDelegate(FFrame& Stack, RESULT_DECL)
{
    GPropAddr = nullptr;
    GProperty = nullptr;
    GPropObject = nullptr;
    Stack.Step(Stack.Object, nullptr);
    FScriptDelegate* Dest = reinterpret_cast<FScriptDelegate*>(GPropAddr);
    FScriptDelegate Delegate = {};
    Stack.Step(Stack.Object, &Delegate);
    if (Dest)
        *Dest = Delegate;
}
IMPLEMENT_FUNCTION(UObject, EX_LetDelegate, execLetDelegate)

// 0x000bd41b to 0x000bd60a. natives from 0x100 up are a token from EX_ExtendedNative and a byte.
#define IMPLEMENT_HIGH_NATIVE(n) \
    void UObject::execHighNative##n(FFrame& Stack, RESULT_DECL) \
    { \
        const INT B = *Stack.Code++; \
        (this->*GNatives[n * 0x100 + B])(Stack, Result); \
    } \
    IMPLEMENT_FUNCTION(UObject, EX_ExtendedNative + n, execHighNative##n)
IMPLEMENT_HIGH_NATIVE(0)
IMPLEMENT_HIGH_NATIVE(1)
IMPLEMENT_HIGH_NATIVE(2)
IMPLEMENT_HIGH_NATIVE(3)
IMPLEMENT_HIGH_NATIVE(4)
IMPLEMENT_HIGH_NATIVE(5)
IMPLEMENT_HIGH_NATIVE(6)
IMPLEMENT_HIGH_NATIVE(7)
IMPLEMENT_HIGH_NATIVE(8)
IMPLEMENT_HIGH_NATIVE(9)
IMPLEMENT_HIGH_NATIVE(10)
IMPLEMENT_HIGH_NATIVE(11)
IMPLEMENT_HIGH_NATIVE(12)
IMPLEMENT_HIGH_NATIVE(13)
IMPLEMENT_HIGH_NATIVE(14)
IMPLEMENT_HIGH_NATIVE(15)

// ---------------------------------------------------------------------------------------------- casts

// 0x000b9ca1
void UObject::execRotatorToVector(FFrame& Stack, RESULT_DECL)
{
    FRotator R = { 0, 0, 0 };
    Stack.Step(Stack.Object, &R);
    *static_cast<FVector*>(Result) = R.Vector();
}
IMPLEMENT_CAST_FUNCTION(UObject, CST_RotatorToVector, execRotatorToVector)

// 0x000b74bd
void UObject::execByteToInt(FFrame& Stack, RESULT_DECL)
{
    BYTE B = 0;
    Stack.Step(Stack.Object, &B);
    *static_cast<INT*>(Result) = B;
}
IMPLEMENT_CAST_FUNCTION(UObject, CST_ByteToInt, execByteToInt)

// 0x000b74ee
void UObject::execByteToBool(FFrame& Stack, RESULT_DECL)
{
    BYTE B = 0;
    Stack.Step(Stack.Object, &B);
    *static_cast<UBOOL*>(Result) = B ? 1 : 0;
}
IMPLEMENT_CAST_FUNCTION(UObject, CST_ByteToBool, execByteToBool)

// 0x000b7523
void UObject::execByteToFloat(FFrame& Stack, RESULT_DECL)
{
    BYTE B = 0;
    Stack.Step(Stack.Object, &B);
    *static_cast<FLOAT*>(Result) = B;
}
IMPLEMENT_CAST_FUNCTION(UObject, CST_ByteToFloat, execByteToFloat)

// 0x000b755a
void UObject::execIntToByte(FFrame& Stack, RESULT_DECL)
{
    INT I = 0;
    Stack.Step(Stack.Object, &I);
    *static_cast<BYTE*>(Result) = static_cast<BYTE>(I);
}
IMPLEMENT_CAST_FUNCTION(UObject, CST_IntToByte, execIntToByte)

// 0x000b7712, shared with execObjectToBool and execNameToBool
void UObject::execIntToBool(FFrame& Stack, RESULT_DECL)
{
    INT I = 0;
    Stack.Step(Stack.Object, &I);
    *static_cast<UBOOL*>(Result) = I ? 1 : 0;
}
IMPLEMENT_CAST_FUNCTION(UObject, CST_IntToBool, execIntToBool)

// 0x000b758a
void UObject::execIntToFloat(FFrame& Stack, RESULT_DECL)
{
    INT I = 0;
    Stack.Step(Stack.Object, &I);
    *static_cast<FLOAT*>(Result) = static_cast<FLOAT>(I);
}
IMPLEMENT_CAST_FUNCTION(UObject, CST_IntToFloat, execIntToFloat)

// 0x000b75be
void UObject::execBoolToByte(FFrame& Stack, RESULT_DECL)
{
    UBOOL B = 0;
    Stack.Step(Stack.Object, &B);
    *static_cast<BYTE*>(Result) = B & 1;
}
IMPLEMENT_CAST_FUNCTION(UObject, CST_BoolToByte, execBoolToByte)

// 0x000b75f0
void UObject::execBoolToInt(FFrame& Stack, RESULT_DECL)
{
    UBOOL B = 0;
    Stack.Step(Stack.Object, &B);
    *static_cast<INT*>(Result) = B & 1;
}
IMPLEMENT_CAST_FUNCTION(UObject, CST_BoolToInt, execBoolToInt)

// 0x000b7623
void UObject::execBoolToFloat(FFrame& Stack, RESULT_DECL)
{
    UBOOL B = 0;
    Stack.Step(Stack.Object, &B);
    *static_cast<FLOAT*>(Result) = static_cast<FLOAT>(B & 1);
}
IMPLEMENT_CAST_FUNCTION(UObject, CST_BoolToFloat, execBoolToFloat)

// 0x000b765c
void UObject::execFloatToByte(FFrame& Stack, RESULT_DECL)
{
    FLOAT F = 0.f;
    Stack.Step(Stack.Object, &F);
    *static_cast<BYTE*>(Result) = static_cast<BYTE>(static_cast<INT>(F));
}
IMPLEMENT_CAST_FUNCTION(UObject, CST_FloatToByte, execFloatToByte)

// 0x000b7692
void UObject::execFloatToInt(FFrame& Stack, RESULT_DECL)
{
    FLOAT F = 0.f;
    Stack.Step(Stack.Object, &F);
    *static_cast<INT*>(Result) = static_cast<INT>(F);
}
IMPLEMENT_CAST_FUNCTION(UObject, CST_FloatToInt, execFloatToInt)

// 0x000b76c8
void UObject::execFloatToBool(FFrame& Stack, RESULT_DECL)
{
    FLOAT F = 0.f;
    Stack.Step(Stack.Object, &F);
    *static_cast<UBOOL*>(Result) = F != 0.f ? 1 : 0;
}
IMPLEMENT_CAST_FUNCTION(UObject, CST_FloatToBool, execFloatToBool)

// 0x000b7712 on the xbox
void UObject::execObjectToBool(FFrame& Stack, RESULT_DECL)
{
    UObject* Obj = nullptr;
    Stack.Step(Stack.Object, &Obj);
    *static_cast<UBOOL*>(Result) = Obj ? 1 : 0;
}
IMPLEMENT_CAST_FUNCTION(UObject, CST_ObjectToBool, execObjectToBool)

// 0x000b7712 on the xbox
void UObject::execNameToBool(FFrame& Stack, RESULT_DECL)
{
    FName N = NAME_None;
    Stack.Step(Stack.Object, &N);
    *static_cast<UBOOL*>(Result) = N != NAME_None ? 1 : 0;
}
IMPLEMENT_CAST_FUNCTION(UObject, CST_NameToBool, execNameToBool)

// 0x000bef31
void UObject::execStringToByte(FFrame& Stack, RESULT_DECL)
{
    FString Str;
    Stack.Step(Stack.Object, &Str);
    *static_cast<BYTE*>(Result) = static_cast<BYTE>(appAtoi(*Str));
}
IMPLEMENT_CAST_FUNCTION(UObject, CST_StringToByte, execStringToByte)

// 0x000bef9d
void UObject::execStringToInt(FFrame& Stack, RESULT_DECL)
{
    FString Str;
    Stack.Step(Stack.Object, &Str);
    *static_cast<INT*>(Result) = appAtoi(*Str);
}
IMPLEMENT_CAST_FUNCTION(UObject, CST_StringToInt, execStringToInt)

// 0x000bf009. True and False in English or the current language, else a number.
void UObject::execStringToBool(FFrame& Stack, RESULT_DECL)
{
    FString Str;
    Stack.Step(Stack.Object, &Str);
    if (!appStricmp(*Str, TEXT("True")) || !appStricmp(*Str, GTrue))
        *static_cast<UBOOL*>(Result) = 1;
    else if (!appStricmp(*Str, TEXT("False")) || !appStricmp(*Str, GFalse))
        *static_cast<UBOOL*>(Result) = 0;
    else
        *static_cast<UBOOL*>(Result) = appAtoi(*Str) ? 1 : 0;
}
IMPLEMENT_CAST_FUNCTION(UObject, CST_StringToBool, execStringToBool)

// 0x000bf0ed
void UObject::execStringToFloat(FFrame& Stack, RESULT_DECL)
{
    FString Str;
    Stack.Step(Stack.Object, &Str);
    *static_cast<FLOAT*>(Result) = appAtof(*Str);
}
IMPLEMENT_CAST_FUNCTION(UObject, CST_StringToFloat, execStringToFloat)

// 0x000bf157. "X,Y,Z"; missing parts stay 0.
void UObject::execStringToVector(FFrame& Stack, RESULT_DECL)
{
    FString Str;
    Stack.Step(Stack.Object, &Str);
    const TCHAR* Text = *Str;
    FVector Value;
    Value.X = appAtof(Text);
    Value.Y = 0.f;
    Value.Z = 0.f;
    if (const TCHAR* Next = appStrstr(Text, TEXT(",")))
    {
        Value.Y = appAtof(++Next);
        if ((Next = appStrstr(Next, TEXT(","))) != nullptr)
            Value.Z = appAtof(++Next);
    }
    *static_cast<FVector*>(Result) = Value;
}
IMPLEMENT_CAST_FUNCTION(UObject, CST_StringToVector, execStringToVector)

// 0x000bf212. "Pitch,Yaw,Roll"; missing parts stay 0.
void UObject::execStringToRotator(FFrame& Stack, RESULT_DECL)
{
    FString Str;
    Stack.Step(Stack.Object, &Str);
    const TCHAR* Text = *Str;
    FRotator Value;
    Value.Pitch = appAtoi(Text);
    Value.Yaw = 0;
    Value.Roll = 0;
    if (const TCHAR* Next = appStrstr(Text, TEXT(",")))
    {
        Value.Yaw = appAtoi(++Next);
        if ((Next = appStrstr(Next, TEXT(","))) != nullptr)
            Value.Roll = appAtoi(++Next);
    }
    *static_cast<FRotator*>(Result) = Value;
}
IMPLEMENT_CAST_FUNCTION(UObject, CST_StringToRotator, execStringToRotator)

// 0x000b9b96
void UObject::execVectorToBool(FFrame& Stack, RESULT_DECL)
{
    FVector V = { 0.f, 0.f, 0.f };
    Stack.Step(Stack.Object, &V);
    *static_cast<UBOOL*>(Result) = !V.IsZero();
}
IMPLEMENT_CAST_FUNCTION(UObject, CST_VectorToBool, execVectorToBool)

// 0x000b9c0e
void UObject::execVectorToRotator(FFrame& Stack, RESULT_DECL)
{
    FVector V = { 0.f, 0.f, 0.f };
    Stack.Step(Stack.Object, &V);
    *static_cast<FRotator*>(Result) = V.Rotation();
}
IMPLEMENT_CAST_FUNCTION(UObject, CST_VectorToRotator, execVectorToRotator)

// 0x000b9c5e
void UObject::execRotatorToBool(FFrame& Stack, RESULT_DECL)
{
    FRotator R = { 0, 0, 0 };
    Stack.Step(Stack.Object, &R);
    *static_cast<UBOOL*>(Result) = !R.IsZero();
}
IMPLEMENT_CAST_FUNCTION(UObject, CST_RotatorToBool, execRotatorToBool)

// 0x000c0d4c
void UObject::execByteToString(FFrame& Stack, RESULT_DECL)
{
    BYTE B = 0;
    Stack.Step(Stack.Object, &B);
    *static_cast<FString*>(Result) = FString::Printf(TEXT("%i"), B);
}
IMPLEMENT_CAST_FUNCTION(UObject, CST_ByteToString, execByteToString)

// 0x000c0db8
void UObject::execIntToString(FFrame& Stack, RESULT_DECL)
{
    INT I = 0;
    Stack.Step(Stack.Object, &I);
    *static_cast<FString*>(Result) = FString::Printf(TEXT("%i"), I);
}
IMPLEMENT_CAST_FUNCTION(UObject, CST_IntToString, execIntToString)

// 0x000c0739
void UObject::execBoolToString(FFrame& Stack, RESULT_DECL)
{
    UBOOL B = 0;
    Stack.Step(Stack.Object, &B);
    *static_cast<FString*>(Result) = B ? GTrue : GFalse;
}
IMPLEMENT_CAST_FUNCTION(UObject, CST_BoolToString, execBoolToString)

// 0x000c0e22
void UObject::execFloatToString(FFrame& Stack, RESULT_DECL)
{
    FLOAT F = 0.f;
    Stack.Step(Stack.Object, &F);
    *static_cast<FString*>(Result) = FString::Printf(TEXT("%.2f"), F);
}
IMPLEMENT_CAST_FUNCTION(UObject, CST_FloatToString, execFloatToString)

// 0x000c077a
void UObject::execObjectToString(FFrame& Stack, RESULT_DECL)
{
    UObject* Obj = nullptr;
    Stack.Step(Stack.Object, &Obj);
    *static_cast<FString*>(Result) = Obj ? Obj->GetPathName() : TEXT("None");
}
IMPLEMENT_CAST_FUNCTION(UObject, CST_ObjectToString, execObjectToString)

// 0x000c07c2
void UObject::execNameToString(FFrame& Stack, RESULT_DECL)
{
    FName N = NAME_None;
    Stack.Step(Stack.Object, &N);
    *static_cast<FString*>(Result) = *N;
}
IMPLEMENT_CAST_FUNCTION(UObject, CST_NameToString, execNameToString)

// 0x000c0e92
void UObject::execVectorToString(FFrame& Stack, RESULT_DECL)
{
    FVector V;
    Stack.Step(Stack.Object, &V);
    *static_cast<FString*>(Result) = FString::Printf(TEXT("%.2f,%.2f,%.2f"), V.X, V.Y, V.Z);
}
IMPLEMENT_CAST_FUNCTION(UObject, CST_VectorToString, execVectorToString)

// 0x000c0f0c. angles are printed modulo 65536.
void UObject::execRotatorToString(FFrame& Stack, RESULT_DECL)
{
    FRotator R;
    Stack.Step(Stack.Object, &R);
    *static_cast<FString*>(Result) = FString::Printf(TEXT("%i,%i,%i"), R.Pitch & 65535, R.Yaw & 65535, R.Roll & 65535);
}
IMPLEMENT_CAST_FUNCTION(UObject, CST_RotatorToString, execRotatorToString)

// ---------------------------------------------------------------------------------------------- struct compare

// 0x0009ae1e
UBOOL UStruct::StructCompare(const void* A, const void* B)
{
    for (TFieldIterator<UProperty> It(this); It; ++It)
    {
        for (INT i = 0; i < It->ArrayDim; i++)
        {
            if (!It->Matches(A, B, i))
                return 0;
        }
    }
    return 1;
}

// ---------------------------------------------------------------------------------------------- object natives

// the natives Object.uc declares, as the xbox native table lists them
FNativeEntry GObjectNatives[] =
{
    { TEXT("Not_PreBool"), { &UObject::execNot_PreBool }, 129 },
    { TEXT("EqualEqual_BoolBool"), { &UObject::execEqualEqual_BoolBool }, 242 },
    { TEXT("NotEqual_BoolBool"), { &UObject::execNotEqual_BoolBool }, 243 },
    { TEXT("AndAnd_BoolBool"), { &UObject::execAndAnd_BoolBool }, 130 },
    { TEXT("XorXor_BoolBool"), { &UObject::execXorXor_BoolBool }, 131 },
    { TEXT("OrOr_BoolBool"), { &UObject::execOrOr_BoolBool }, 132 },
    { TEXT("MultiplyEqual_ByteByte"), { &UObject::execMultiplyEqual_ByteByte }, 133 },
    { TEXT("DivideEqual_ByteByte"), { &UObject::execDivideEqual_ByteByte }, 134 },
    { TEXT("AddEqual_ByteByte"), { &UObject::execAddEqual_ByteByte }, 135 },
    { TEXT("SubtractEqual_ByteByte"), { &UObject::execSubtractEqual_ByteByte }, 136 },
    { TEXT("AddAdd_PreByte"), { &UObject::execAddAdd_PreByte }, 137 },
    { TEXT("SubtractSubtract_PreByte"), { &UObject::execSubtractSubtract_PreByte }, 138 },
    { TEXT("AddAdd_Byte"), { &UObject::execAddAdd_Byte }, 139 },
    { TEXT("SubtractSubtract_Byte"), { &UObject::execSubtractSubtract_Byte }, 140 },
    { TEXT("Complement_PreInt"), { &UObject::execComplement_PreInt }, 141 },
    { TEXT("Subtract_PreInt"), { &UObject::execSubtract_PreInt }, 143 },
    { TEXT("Multiply_IntInt"), { &UObject::execMultiply_IntInt }, 144 },
    { TEXT("Divide_IntInt"), { &UObject::execDivide_IntInt }, 145 },
    { TEXT("Add_IntInt"), { &UObject::execAdd_IntInt }, 146 },
    { TEXT("Subtract_IntInt"), { &UObject::execSubtract_IntInt }, 147 },
    { TEXT("LessLess_IntInt"), { &UObject::execLessLess_IntInt }, 148 },
    { TEXT("GreaterGreater_IntInt"), { &UObject::execGreaterGreater_IntInt }, 149 },
    { TEXT("GreaterGreaterGreater_IntInt"), { &UObject::execGreaterGreaterGreater_IntInt }, 196 },
    { TEXT("Less_IntInt"), { &UObject::execLess_IntInt }, 150 },
    { TEXT("Greater_IntInt"), { &UObject::execGreater_IntInt }, 151 },
    { TEXT("LessEqual_IntInt"), { &UObject::execLessEqual_IntInt }, 152 },
    { TEXT("GreaterEqual_IntInt"), { &UObject::execGreaterEqual_IntInt }, 153 },
    { TEXT("EqualEqual_IntInt"), { &UObject::execEqualEqual_IntInt }, 154 },
    { TEXT("NotEqual_IntInt"), { &UObject::execNotEqual_IntInt }, 155 },
    { TEXT("And_IntInt"), { &UObject::execAnd_IntInt }, 156 },
    { TEXT("Xor_IntInt"), { &UObject::execXor_IntInt }, 157 },
    { TEXT("Or_IntInt"), { &UObject::execOr_IntInt }, 158 },
    { TEXT("MultiplyEqual_IntFloat"), { &UObject::execMultiplyEqual_IntFloat }, 159 },
    { TEXT("DivideEqual_IntFloat"), { &UObject::execDivideEqual_IntFloat }, 160 },
    { TEXT("AddEqual_IntInt"), { &UObject::execAddEqual_IntInt }, 161 },
    { TEXT("SubtractEqual_IntInt"), { &UObject::execSubtractEqual_IntInt }, 162 },
    { TEXT("AddAdd_PreInt"), { &UObject::execAddAdd_PreInt }, 163 },
    { TEXT("SubtractSubtract_PreInt"), { &UObject::execSubtractSubtract_PreInt }, 164 },
    { TEXT("AddAdd_Int"), { &UObject::execAddAdd_Int }, 165 },
    { TEXT("SubtractSubtract_Int"), { &UObject::execSubtractSubtract_Int }, 166 },
    { TEXT("Rand"), { &UObject::execRand }, 167 },
    { TEXT("Min"), { &UObject::execMin }, 249 },
    { TEXT("Max"), { &UObject::execMax }, 250 },
    { TEXT("Clamp"), { &UObject::execClamp }, 251 },
    { TEXT("Subtract_PreFloat"), { &UObject::execSubtract_PreFloat }, 169 },
    { TEXT("MultiplyMultiply_FloatFloat"), { &UObject::execMultiplyMultiply_FloatFloat }, 170 },
    { TEXT("Multiply_FloatFloat"), { &UObject::execMultiply_FloatFloat }, 171 },
    { TEXT("Divide_FloatFloat"), { &UObject::execDivide_FloatFloat }, 172 },
    { TEXT("Percent_FloatFloat"), { &UObject::execPercent_FloatFloat }, 173 },
    { TEXT("Add_FloatFloat"), { &UObject::execAdd_FloatFloat }, 174 },
    { TEXT("Subtract_FloatFloat"), { &UObject::execSubtract_FloatFloat }, 175 },
    { TEXT("Less_FloatFloat"), { &UObject::execLess_FloatFloat }, 176 },
    { TEXT("Greater_FloatFloat"), { &UObject::execGreater_FloatFloat }, 177 },
    { TEXT("LessEqual_FloatFloat"), { &UObject::execLessEqual_FloatFloat }, 178 },
    { TEXT("GreaterEqual_FloatFloat"), { &UObject::execGreaterEqual_FloatFloat }, 179 },
    { TEXT("EqualEqual_FloatFloat"), { &UObject::execEqualEqual_FloatFloat }, 180 },
    { TEXT("ComplementEqual_FloatFloat"), { &UObject::execComplementEqual_FloatFloat }, 210 },
    { TEXT("NotEqual_FloatFloat"), { &UObject::execNotEqual_FloatFloat }, 181 },
    { TEXT("MultiplyEqual_FloatFloat"), { &UObject::execMultiplyEqual_FloatFloat }, 182 },
    { TEXT("DivideEqual_FloatFloat"), { &UObject::execDivideEqual_FloatFloat }, 183 },
    { TEXT("AddEqual_FloatFloat"), { &UObject::execAddEqual_FloatFloat }, 184 },
    { TEXT("SubtractEqual_FloatFloat"), { &UObject::execSubtractEqual_FloatFloat }, 185 },
    { TEXT("Abs"), { &UObject::execAbs }, 186 },
    { TEXT("Sin"), { &UObject::execSin }, 187 },
    { TEXT("Asin"), { &UObject::execAsin }, 0 },
    { TEXT("Cos"), { &UObject::execCos }, 188 },
    { TEXT("Acos"), { &UObject::execAcos }, 0 },
    { TEXT("Tan"), { &UObject::execTan }, 189 },
    { TEXT("Atan"), { &UObject::execAtan }, 190 },
    { TEXT("Exp"), { &UObject::execExp }, 191 },
    { TEXT("Loge"), { &UObject::execLoge }, 192 },
    { TEXT("Sqrt"), { &UObject::execSqrt }, 193 },
    { TEXT("Square"), { &UObject::execSquare }, 194 },
    { TEXT("FRand"), { &UObject::execFRand }, 195 },
    { TEXT("FMin"), { &UObject::execFMin }, 244 },
    { TEXT("FMax"), { &UObject::execFMax }, 245 },
    { TEXT("FClamp"), { &UObject::execFClamp }, 246 },
    { TEXT("Lerp"), { &UObject::execLerp }, 247 },
    { TEXT("Smerp"), { &UObject::execSmerp }, 248 },
    { TEXT("Ceil"), { &UObject::execCeil }, 253 },
    { TEXT("Round"), { &UObject::execRound }, 257 },
    { TEXT("Frac"), { &UObject::execFrac }, 0 },
    { TEXT("Subtract_PreVector"), { &UObject::execSubtract_PreVector }, 211 },
    { TEXT("Multiply_VectorFloat"), { &UObject::execMultiply_VectorFloat }, 212 },
    { TEXT("Multiply_FloatVector"), { &UObject::execMultiply_FloatVector }, 213 },
    { TEXT("Multiply_VectorVector"), { &UObject::execMultiply_VectorVector }, 296 },
    { TEXT("Divide_VectorFloat"), { &UObject::execDivide_VectorFloat }, 214 },
    { TEXT("Add_VectorVector"), { &UObject::execAdd_VectorVector }, 215 },
    { TEXT("Subtract_VectorVector"), { &UObject::execSubtract_VectorVector }, 216 },
    { TEXT("LessLess_VectorRotator"), { &UObject::execLessLess_VectorRotator }, 275 },
    { TEXT("GreaterGreater_VectorRotator"), { &UObject::execGreaterGreater_VectorRotator }, 276 },
    { TEXT("EqualEqual_VectorVector"), { &UObject::execEqualEqual_VectorVector }, 217 },
    { TEXT("NotEqual_VectorVector"), { &UObject::execNotEqual_VectorVector }, 218 },
    { TEXT("Dot_VectorVector"), { &UObject::execDot_VectorVector }, 219 },
    { TEXT("Cross_VectorVector"), { &UObject::execCross_VectorVector }, 220 },
    { TEXT("MultiplyEqual_VectorFloat"), { &UObject::execMultiplyEqual_VectorFloat }, 221 },
    { TEXT("MultiplyEqual_VectorVector"), { &UObject::execMultiplyEqual_VectorVector }, 297 },
    { TEXT("DivideEqual_VectorFloat"), { &UObject::execDivideEqual_VectorFloat }, 222 },
    { TEXT("AddEqual_VectorVector"), { &UObject::execAddEqual_VectorVector }, 223 },
    { TEXT("SubtractEqual_VectorVector"), { &UObject::execSubtractEqual_VectorVector }, 224 },
    { TEXT("VSize"), { &UObject::execVSize }, 225 },
    { TEXT("Normal"), { &UObject::execNormal }, 226 },
    { TEXT("Invert"), { &UObject::execInvert }, 227 },
    { TEXT("VIsZero"), { &UObject::execVIsZero }, 228 },
    { TEXT("VRand"), { &UObject::execVRand }, 252 },
    { TEXT("MirrorVectorByNormal"), { &UObject::execMirrorVectorByNormal }, 300 },
    { TEXT("VSizeSq"), { &UObject::execVSizeSq }, 0 },
    { TEXT("VSize2D"), { &UObject::execVSize2D }, 0 },
    { TEXT("VDistSq"), { &UObject::execVDistSq }, 0 },
    { TEXT("VDist2DSq"), { &UObject::execVDist2DSq }, 0 },
    { TEXT("CalculateAimPitch"), { &UObject::execCalculateAimPitch }, 0 },
    { TEXT("EqualEqual_RotatorRotator"), { &UObject::execEqualEqual_RotatorRotator }, 142 },
    { TEXT("NotEqual_RotatorRotator"), { &UObject::execNotEqual_RotatorRotator }, 203 },
    { TEXT("Multiply_RotatorFloat"), { &UObject::execMultiply_RotatorFloat }, 287 },
    { TEXT("Multiply_FloatRotator"), { &UObject::execMultiply_FloatRotator }, 288 },
    { TEXT("Divide_RotatorFloat"), { &UObject::execDivide_RotatorFloat }, 289 },
    { TEXT("MultiplyEqual_RotatorFloat"), { &UObject::execMultiplyEqual_RotatorFloat }, 290 },
    { TEXT("DivideEqual_RotatorFloat"), { &UObject::execDivideEqual_RotatorFloat }, 291 },
    { TEXT("Add_RotatorRotator"), { &UObject::execAdd_RotatorRotator }, 316 },
    { TEXT("Subtract_RotatorRotator"), { &UObject::execSubtract_RotatorRotator }, 317 },
    { TEXT("AddEqual_RotatorRotator"), { &UObject::execAddEqual_RotatorRotator }, 318 },
    { TEXT("SubtractEqual_RotatorRotator"), { &UObject::execSubtractEqual_RotatorRotator }, 319 },
    { TEXT("GetAxes"), { &UObject::execGetAxes }, 229 },
    { TEXT("GetUnAxes"), { &UObject::execGetUnAxes }, 230 },
    { TEXT("RotRand"), { &UObject::execRotRand }, 320 },
    { TEXT("OrthoRotation"), { &UObject::execOrthoRotation }, 0 },
    { TEXT("Normalize"), { &UObject::execNormalize }, 0 },
    { TEXT("ClockwiseFrom_IntInt"), { &UObject::execClockwiseFrom_IntInt }, 0 },
    { TEXT("Concat_StringString"), { &UObject::execConcat_StringString }, 112 },
    { TEXT("At_StringString"), { &UObject::execAt_StringString }, 168 },
    { TEXT("Less_StringString"), { &UObject::execLess_StringString }, 115 },
    { TEXT("Greater_StringString"), { &UObject::execGreater_StringString }, 116 },
    { TEXT("LessEqual_StringString"), { &UObject::execLessEqual_StringString }, 120 },
    { TEXT("GreaterEqual_StringString"), { &UObject::execGreaterEqual_StringString }, 121 },
    { TEXT("EqualEqual_StringString"), { &UObject::execEqualEqual_StringString }, 122 },
    { TEXT("NotEqual_StringString"), { &UObject::execNotEqual_StringString }, 123 },
    { TEXT("ComplementEqual_StringString"), { &UObject::execComplementEqual_StringString }, 124 },
    { TEXT("Len"), { &UObject::execLen }, 125 },
    { TEXT("InStr"), { &UObject::execInStr }, 126 },
    { TEXT("Mid"), { &UObject::execMid }, 127 },
    { TEXT("Left"), { &UObject::execLeft }, 128 },
    { TEXT("Right"), { &UObject::execRight }, 234 },
    { TEXT("Caps"), { &UObject::execCaps }, 235 },
    { TEXT("Chr"), { &UObject::execChr }, 236 },
    { TEXT("Asc"), { &UObject::execAsc }, 237 },
    { TEXT("EqualEqual_ObjectObject"), { &UObject::execEqualEqual_ObjectObject }, 114 },
    { TEXT("NotEqual_ObjectObject"), { &UObject::execNotEqual_ObjectObject }, 119 },
    { TEXT("EqualEqual_NameName"), { &UObject::execEqualEqual_NameName }, 254 },
    { TEXT("NotEqual_NameName"), { &UObject::execNotEqual_NameName }, 255 },
    { TEXT("InterpCurveEval"), { &UObject::execInterpCurveEval }, 0 },
    { TEXT("InterpCurveGetOutputRange"), { &UObject::execInterpCurveGetOutputRange }, 0 },
    { TEXT("InterpCurveGetInputDomain"), { &UObject::execInterpCurveGetInputDomain }, 0 },
    { TEXT("QuatProduct"), { &UObject::execQuatProduct }, 0 },
    { TEXT("QuatInvert"), { &UObject::execQuatInvert }, 0 },
    { TEXT("QuatRotateVector"), { &UObject::execQuatRotateVector }, 0 },
    { TEXT("QuatFindBetween"), { &UObject::execQuatFindBetween }, 0 },
    { TEXT("QuatFromAxisAndAngle"), { &UObject::execQuatFromAxisAndAngle }, 0 },
    { TEXT("QuatFromRotator"), { &UObject::execQuatFromRotator }, 0 },
    { TEXT("QuatToRotator"), { &UObject::execQuatToRotator }, 0 },
    { TEXT("Log"), { &UObject::execLog }, 231 },
    { TEXT("Warn"), { &UObject::execWarn }, 232 },
    { TEXT("Localize"), { &UObject::execLocalize }, 0 },
    { TEXT("GotoState"), { &UObject::execGotoState }, 113 },
    { TEXT("IsInState"), { &UObject::execIsInState }, 281 },
    { TEXT("GetStateName"), { &UObject::execGetStateName }, 284 },
    { TEXT("ClassIsChildOf"), { &UObject::execClassIsChildOf }, 258 },
    { TEXT("IsA"), { &UObject::execIsA }, 303 },
    { TEXT("Enable"), { &UObject::execEnable }, 117 },
    { TEXT("Disable"), { &UObject::execDisable }, 118 },
    { TEXT("GetPropertyText"), { &UObject::execGetPropertyText }, 0 },
    { TEXT("SetPropertyText"), { &UObject::execSetPropertyText }, 0 },
    { TEXT("GetEnum"), { &UObject::execGetEnum }, 0 },
    { TEXT("DynamicLoadObject"), { &UObject::execDynamicLoadObject }, 0 },
    { TEXT("FindObject"), { &UObject::execFindObject }, 0 },
    { TEXT("SaveConfig"), { &UObject::execSaveConfig }, 536 },
    { TEXT("StaticSaveConfig"), { &UObject::execStaticSaveConfig }, 0 },
    { TEXT("ResetConfig"), { &UObject::execResetConfig }, 0 },
    { TEXT("UnloadInts"), { &UObject::execUnloadInts }, 0 },
    { TEXT("SetUTracing"), { &UObject::execSetUTracing }, 0 },
    { TEXT("IsUTracing"), { &UObject::execIsUTracing }, 0 },
    { TEXT("IsOnConsole"), { &UObject::execIsOnConsole }, 0 },
    { nullptr, { nullptr }, 0 },
};

// bool operators

// 0x000b7747
void UObject::execNot_PreBool(FFrame& Stack, RESULT_DECL)
{
    P_GET_UBOOL(A);
    P_FINISH;
    *static_cast<DWORD*>(Result) = !A;
}

// 0x000b7799
void UObject::execEqualEqual_BoolBool(FFrame& Stack, RESULT_DECL)
{
    P_GET_UBOOL(A);
    P_GET_UBOOL(B);
    P_FINISH;
    *static_cast<DWORD*>(Result) = !A == !B;
}

// 0x000b7818
void UObject::execNotEqual_BoolBool(FFrame& Stack, RESULT_DECL)
{
    P_GET_UBOOL(A);
    P_GET_UBOOL(B);
    P_FINISH;
    *static_cast<DWORD*>(Result) = !A != !B;
}

// 0x000b7897. the compiler puts a skip offset before B so it is only run when needed.
void UObject::execAndAnd_BoolBool(FFrame& Stack, RESULT_DECL)
{
    P_GET_UBOOL(A);
    P_GET_SKIP_OFFSET(W);
    if (A)
    {
        P_GET_UBOOL(B);
        *static_cast<DWORD*>(Result) = A && B;
        Stack.Code++;
    }
    else
    {
        *static_cast<DWORD*>(Result) = 0;
        Stack.Code += W;
    }
}

// 0x000b7911
void UObject::execXorXor_BoolBool(FFrame& Stack, RESULT_DECL)
{
    P_GET_UBOOL(A);
    P_GET_UBOOL(B);
    P_FINISH;
    *static_cast<DWORD*>(Result) = !A != !B;
}

// 0x000b798b
void UObject::execOrOr_BoolBool(FFrame& Stack, RESULT_DECL)
{
    P_GET_UBOOL(A);
    P_GET_SKIP_OFFSET(W);
    if (!A)
    {
        P_GET_UBOOL(B);
        *static_cast<DWORD*>(Result) = A || B;
        Stack.Code++;
    }
    else
    {
        *static_cast<DWORD*>(Result) = 1;
        Stack.Code += W;
    }
}

// byte operators

// 0x000b7a0b
void UObject::execMultiplyEqual_ByteByte(FFrame& Stack, RESULT_DECL)
{
    P_GET_BYTE_REF(A);
    P_GET_BYTE(B);
    P_FINISH;
    *static_cast<BYTE*>(Result) = (*A *= B);
}

// 0x000b7aa5. dividing by zero leaves A alone and gives 0.
void UObject::execDivideEqual_ByteByte(FFrame& Stack, RESULT_DECL)
{
    P_GET_BYTE_REF(A);
    P_GET_BYTE(B);
    P_FINISH;
    *static_cast<BYTE*>(Result) = B ? (*A /= B) : 0;
}

// 0x000b7b4f
void UObject::execAddEqual_ByteByte(FFrame& Stack, RESULT_DECL)
{
    P_GET_BYTE_REF(A);
    P_GET_BYTE(B);
    P_FINISH;
    *static_cast<BYTE*>(Result) = (*A += B);
}

// 0x000b7be9
void UObject::execSubtractEqual_ByteByte(FFrame& Stack, RESULT_DECL)
{
    P_GET_BYTE_REF(A);
    P_GET_BYTE(B);
    P_FINISH;
    *static_cast<BYTE*>(Result) = (*A -= B);
}

// 0x000b7c83
void UObject::execAddAdd_PreByte(FFrame& Stack, RESULT_DECL)
{
    P_GET_BYTE_REF(A);
    P_FINISH;
    *static_cast<BYTE*>(Result) = ++(*A);
}

// 0x000b7cfc
void UObject::execSubtractSubtract_PreByte(FFrame& Stack, RESULT_DECL)
{
    P_GET_BYTE_REF(A);
    P_FINISH;
    *static_cast<BYTE*>(Result) = --(*A);
}

// 0x000b7d75
void UObject::execAddAdd_Byte(FFrame& Stack, RESULT_DECL)
{
    P_GET_BYTE_REF(A);
    P_FINISH;
    *static_cast<BYTE*>(Result) = (*A)++;
}

// 0x000b7def
void UObject::execSubtractSubtract_Byte(FFrame& Stack, RESULT_DECL)
{
    P_GET_BYTE_REF(A);
    P_FINISH;
    *static_cast<BYTE*>(Result) = (*A)--;
}

// int operators

// 0x000b7e69
void UObject::execComplement_PreInt(FFrame& Stack, RESULT_DECL)
{
    P_GET_INT(A);
    P_FINISH;
    *static_cast<INT*>(Result) = ~A;
}

// 0x000b7f27
void UObject::execSubtract_PreInt(FFrame& Stack, RESULT_DECL)
{
    P_GET_INT(A);
    P_FINISH;
    *static_cast<INT*>(Result) = -A;
}

// 0x000b7f76
void UObject::execMultiply_IntInt(FFrame& Stack, RESULT_DECL)
{
    P_GET_INT(A);
    P_GET_INT(B);
    P_FINISH;
    *static_cast<INT*>(Result) = A * B;
}

// 0x000b7fe4. dividing by zero gives 0.
void UObject::execDivide_IntInt(FFrame& Stack, RESULT_DECL)
{
    P_GET_INT(A);
    P_GET_INT(B);
    P_FINISH;
    *static_cast<INT*>(Result) = B ? A / B : 0;
}

// 0x000b805c
void UObject::execAdd_IntInt(FFrame& Stack, RESULT_DECL)
{
    P_GET_INT(A);
    P_GET_INT(B);
    P_FINISH;
    *static_cast<INT*>(Result) = A + B;
}

// 0x000b80cb
void UObject::execSubtract_IntInt(FFrame& Stack, RESULT_DECL)
{
    P_GET_INT(A);
    P_GET_INT(B);
    P_FINISH;
    *static_cast<INT*>(Result) = A - B;
}

// 0x000b8138. shift counts wrap at 32 like the x86 shift instructions.
void UObject::execLessLess_IntInt(FFrame& Stack, RESULT_DECL)
{
    P_GET_INT(A);
    P_GET_INT(B);
    P_FINISH;
    *static_cast<INT*>(Result) = static_cast<INT>(static_cast<DWORD>(A) << (B & 31));
}

// 0x000b81a7
void UObject::execGreaterGreater_IntInt(FFrame& Stack, RESULT_DECL)
{
    P_GET_INT(A);
    P_GET_INT(B);
    P_FINISH;
    *static_cast<INT*>(Result) = A >> (B & 31);
}

// 0x000b7eb8
void UObject::execGreaterGreaterGreater_IntInt(FFrame& Stack, RESULT_DECL)
{
    P_GET_INT(A);
    P_GET_INT(B);
    P_FINISH;
    *static_cast<DWORD*>(Result) = static_cast<DWORD>(A) >> (B & 31);
}

// 0x000b8216
void UObject::execLess_IntInt(FFrame& Stack, RESULT_DECL)
{
    P_GET_INT(A);
    P_GET_INT(B);
    P_FINISH;
    *static_cast<DWORD*>(Result) = A < B;
}

// 0x000b8288
void UObject::execGreater_IntInt(FFrame& Stack, RESULT_DECL)
{
    P_GET_INT(A);
    P_GET_INT(B);
    P_FINISH;
    *static_cast<DWORD*>(Result) = A > B;
}

// 0x000b82fa
void UObject::execLessEqual_IntInt(FFrame& Stack, RESULT_DECL)
{
    P_GET_INT(A);
    P_GET_INT(B);
    P_FINISH;
    *static_cast<DWORD*>(Result) = A <= B;
}

// 0x000b836c
void UObject::execGreaterEqual_IntInt(FFrame& Stack, RESULT_DECL)
{
    P_GET_INT(A);
    P_GET_INT(B);
    P_FINISH;
    *static_cast<DWORD*>(Result) = A >= B;
}

// 0x000b83de, shared with EqualEqual_ObjectObject on the xbox
void UObject::execEqualEqual_IntInt(FFrame& Stack, RESULT_DECL)
{
    P_GET_INT(A);
    P_GET_INT(B);
    P_FINISH;
    *static_cast<DWORD*>(Result) = A == B;
}

// 0x000bd37d, shared with NotEqual_ObjectObject on the xbox
void UObject::execNotEqual_IntInt(FFrame& Stack, RESULT_DECL)
{
    P_GET_INT(A);
    P_GET_INT(B);
    P_FINISH;
    *static_cast<DWORD*>(Result) = A != B;
}

// 0x000b8450
void UObject::execAnd_IntInt(FFrame& Stack, RESULT_DECL)
{
    P_GET_INT(A);
    P_GET_INT(B);
    P_FINISH;
    *static_cast<INT*>(Result) = A & B;
}

// 0x000b84bd
void UObject::execXor_IntInt(FFrame& Stack, RESULT_DECL)
{
    P_GET_INT(A);
    P_GET_INT(B);
    P_FINISH;
    *static_cast<INT*>(Result) = A ^ B;
}

// 0x000b852a
void UObject::execOr_IntInt(FFrame& Stack, RESULT_DECL)
{
    P_GET_INT(A);
    P_GET_INT(B);
    P_FINISH;
    *static_cast<INT*>(Result) = A | B;
}

// 0x000b8597
void UObject::execMultiplyEqual_IntFloat(FFrame& Stack, RESULT_DECL)
{
    P_GET_INT_REF(A);
    P_GET_FLOAT(B);
    P_FINISH;
    *static_cast<INT*>(Result) = (*A = static_cast<INT>(*A * B));
}

// 0x000b863d. dividing by zero sets A to 0.
void UObject::execDivideEqual_IntFloat(FFrame& Stack, RESULT_DECL)
{
    P_GET_INT_REF(A);
    P_GET_FLOAT(B);
    P_FINISH;
    *static_cast<INT*>(Result) = (*A = static_cast<INT>(B != 0.f ? *A / B : 0.f));
}

// 0x000b86f3
void UObject::execAddEqual_IntInt(FFrame& Stack, RESULT_DECL)
{
    P_GET_INT_REF(A);
    P_GET_INT(B);
    P_FINISH;
    *static_cast<INT*>(Result) = (*A += B);
}

// 0x000b878d
void UObject::execSubtractEqual_IntInt(FFrame& Stack, RESULT_DECL)
{
    P_GET_INT_REF(A);
    P_GET_INT(B);
    P_FINISH;
    *static_cast<INT*>(Result) = (*A -= B);
}

// 0x000b8827
void UObject::execAddAdd_PreInt(FFrame& Stack, RESULT_DECL)
{
    P_GET_INT_REF(A);
    P_FINISH;
    *static_cast<INT*>(Result) = ++(*A);
}

// 0x000b88a0
void UObject::execSubtractSubtract_PreInt(FFrame& Stack, RESULT_DECL)
{
    P_GET_INT_REF(A);
    P_FINISH;
    *static_cast<INT*>(Result) = --(*A);
}

// 0x000b8919
void UObject::execAddAdd_Int(FFrame& Stack, RESULT_DECL)
{
    P_GET_INT_REF(A);
    P_FINISH;
    *static_cast<INT*>(Result) = (*A)++;
}

// 0x000b8993
void UObject::execSubtractSubtract_Int(FFrame& Stack, RESULT_DECL)
{
    P_GET_INT_REF(A);
    P_FINISH;
    *static_cast<INT*>(Result) = (*A)--;
}

// 0x000b8a0d
void UObject::execRand(FFrame& Stack, RESULT_DECL)
{
    P_GET_INT(A);
    P_FINISH;
    *static_cast<INT*>(Result) = A > 0 ? appRand() % A : 0;
}

// 0x000b8a6a
void UObject::execMin(FFrame& Stack, RESULT_DECL)
{
    P_GET_INT(A);
    P_GET_INT(B);
    P_FINISH;
    *static_cast<INT*>(Result) = B < A ? B : A;
}

// 0x000b8adb
void UObject::execMax(FFrame& Stack, RESULT_DECL)
{
    P_GET_INT(A);
    P_GET_INT(B);
    P_FINISH;
    *static_cast<INT*>(Result) = A < B ? B : A;
}

// 0x000b8b4c
void UObject::execClamp(FFrame& Stack, RESULT_DECL)
{
    P_GET_INT(V);
    P_GET_INT(A);
    P_GET_INT(B);
    P_FINISH;
    *static_cast<INT*>(Result) = V < A ? A : V < B ? V : B;
}

// float operators

// 0x000b8be6
void UObject::execSubtract_PreFloat(FFrame& Stack, RESULT_DECL)
{
    P_GET_FLOAT(A);
    P_FINISH;
    *static_cast<FLOAT*>(Result) = 0.f - A;
}

// 0x000b8c3e
void UObject::execMultiplyMultiply_FloatFloat(FFrame& Stack, RESULT_DECL)
{
    P_GET_FLOAT(A);
    P_GET_FLOAT(B);
    P_FINISH;
    *static_cast<FLOAT*>(Result) = static_cast<FLOAT>(pow(A, B));
}

// 0x000b8cb8
void UObject::execMultiply_FloatFloat(FFrame& Stack, RESULT_DECL)
{
    P_GET_FLOAT(A);
    P_GET_FLOAT(B);
    P_FINISH;
    *static_cast<FLOAT*>(Result) = A * B;
}

// 0x000b8d33
void UObject::execDivide_FloatFloat(FFrame& Stack, RESULT_DECL)
{
    P_GET_FLOAT(A);
    P_GET_FLOAT(B);
    P_FINISH;
    *static_cast<FLOAT*>(Result) = A / B;
}

// 0x000b8dae
void UObject::execPercent_FloatFloat(FFrame& Stack, RESULT_DECL)
{
    P_GET_FLOAT(A);
    P_GET_FLOAT(B);
    P_FINISH;
    *static_cast<FLOAT*>(Result) = static_cast<FLOAT>(fmod(A, B));
}

// 0x000b8e28
void UObject::execAdd_FloatFloat(FFrame& Stack, RESULT_DECL)
{
    P_GET_FLOAT(A);
    P_GET_FLOAT(B);
    P_FINISH;
    *static_cast<FLOAT*>(Result) = A + B;
}

// 0x000b8ea3
void UObject::execSubtract_FloatFloat(FFrame& Stack, RESULT_DECL)
{
    P_GET_FLOAT(A);
    P_GET_FLOAT(B);
    P_FINISH;
    *static_cast<FLOAT*>(Result) = A - B;
}

// 0x000b8f1e
void UObject::execLess_FloatFloat(FFrame& Stack, RESULT_DECL)
{
    P_GET_FLOAT(A);
    P_GET_FLOAT(B);
    P_FINISH;
    *static_cast<DWORD*>(Result) = A < B;
}

// 0x000b8f9b
void UObject::execGreater_FloatFloat(FFrame& Stack, RESULT_DECL)
{
    P_GET_FLOAT(A);
    P_GET_FLOAT(B);
    P_FINISH;
    *static_cast<DWORD*>(Result) = A > B;
}

// 0x000b9018
void UObject::execLessEqual_FloatFloat(FFrame& Stack, RESULT_DECL)
{
    P_GET_FLOAT(A);
    P_GET_FLOAT(B);
    P_FINISH;
    *static_cast<DWORD*>(Result) = A <= B;
}

// 0x000b9095
void UObject::execGreaterEqual_FloatFloat(FFrame& Stack, RESULT_DECL)
{
    P_GET_FLOAT(A);
    P_GET_FLOAT(B);
    P_FINISH;
    *static_cast<DWORD*>(Result) = A >= B;
}

// 0x000b9112
void UObject::execEqualEqual_FloatFloat(FFrame& Stack, RESULT_DECL)
{
    P_GET_FLOAT(A);
    P_GET_FLOAT(B);
    P_FINISH;
    *static_cast<DWORD*>(Result) = A == B;
}

// 0x000bdf4b. "nearly equal": within 0.0001.
void UObject::execComplementEqual_FloatFloat(FFrame& Stack, RESULT_DECL)
{
    P_GET_FLOAT(A);
    P_GET_FLOAT(B);
    P_FINISH;
    FLOAT Diff = A - B;
    if (Diff < 0.f)
        Diff = 0.f - Diff;
    *static_cast<DWORD*>(Result) = Diff < 0.0001;
}

// 0x000b9197
void UObject::execNotEqual_FloatFloat(FFrame& Stack, RESULT_DECL)
{
    P_GET_FLOAT(A);
    P_GET_FLOAT(B);
    P_FINISH;
    *static_cast<DWORD*>(Result) = A != B;
}

// 0x000b921c
void UObject::execMultiplyEqual_FloatFloat(FFrame& Stack, RESULT_DECL)
{
    P_GET_FLOAT_REF(A);
    P_GET_FLOAT(B);
    P_FINISH;
    *static_cast<FLOAT*>(Result) = (*A *= B);
}

// 0x000b92c6
void UObject::execDivideEqual_FloatFloat(FFrame& Stack, RESULT_DECL)
{
    P_GET_FLOAT_REF(A);
    P_GET_FLOAT(B);
    P_FINISH;
    *static_cast<FLOAT*>(Result) = (*A /= B);
}

// 0x000b9370
void UObject::execAddEqual_FloatFloat(FFrame& Stack, RESULT_DECL)
{
    P_GET_FLOAT_REF(A);
    P_GET_FLOAT(B);
    P_FINISH;
    *static_cast<FLOAT*>(Result) = (*A += B);
}

// 0x000b941a
void UObject::execSubtractEqual_FloatFloat(FFrame& Stack, RESULT_DECL)
{
    P_GET_FLOAT_REF(A);
    P_GET_FLOAT(B);
    P_FINISH;
    *static_cast<FLOAT*>(Result) = (*A -= B);
}

// 0x000bdff2
void UObject::execAbs(FFrame& Stack, RESULT_DECL)
{
    P_GET_FLOAT(A);
    P_FINISH;
    *static_cast<FLOAT*>(Result) = A < 0.f ? 0.f - A : A;
}

// 0x000b94c4
void UObject::execSin(FFrame& Stack, RESULT_DECL)
{
    P_GET_FLOAT(A);
    P_FINISH;
    *static_cast<FLOAT*>(Result) = static_cast<FLOAT>(sin(A));
}

// 0x000b9517
void UObject::execAsin(FFrame& Stack, RESULT_DECL)
{
    P_GET_FLOAT(A);
    P_FINISH;
    *static_cast<FLOAT*>(Result) = static_cast<FLOAT>(asin(A));
}

// 0x000b956d
void UObject::execCos(FFrame& Stack, RESULT_DECL)
{
    P_GET_FLOAT(A);
    P_FINISH;
    *static_cast<FLOAT*>(Result) = static_cast<FLOAT>(cos(A));
}

// 0x000b95c0
void UObject::execAcos(FFrame& Stack, RESULT_DECL)
{
    P_GET_FLOAT(A);
    P_FINISH;
    *static_cast<FLOAT*>(Result) = static_cast<FLOAT>(acos(A));
}

// 0x000b9616
void UObject::execTan(FFrame& Stack, RESULT_DECL)
{
    P_GET_FLOAT(A);
    P_FINISH;
    *static_cast<FLOAT*>(Result) = static_cast<FLOAT>(tan(A));
}

// 0x000b966b. the two argument arctangent of A / B.
void UObject::execAtan(FFrame& Stack, RESULT_DECL)
{
    P_GET_FLOAT(A);
    P_GET_FLOAT(B);
    P_FINISH;
    *static_cast<FLOAT*>(Result) = static_cast<FLOAT>(atan2(A, B));
}

// 0x000b96e2
void UObject::execExp(FFrame& Stack, RESULT_DECL)
{
    P_GET_FLOAT(A);
    P_FINISH;
    *static_cast<FLOAT*>(Result) = static_cast<FLOAT>(exp(A));
}

// 0x000b9738
void UObject::execLoge(FFrame& Stack, RESULT_DECL)
{
    P_GET_FLOAT(A);
    P_FINISH;
    *static_cast<FLOAT*>(Result) = static_cast<FLOAT>(log(A));
}

// 0x000b9793
void UObject::execSqrt(FFrame& Stack, RESULT_DECL)
{
    P_GET_FLOAT(A);
    P_FINISH;
    *static_cast<FLOAT*>(Result) = appSqrt(A);
}

// 0x000be058
void UObject::execSquare(FFrame& Stack, RESULT_DECL)
{
    P_GET_FLOAT(A);
    P_FINISH;
    *static_cast<FLOAT*>(Result) = A * A;
}

// 0x000b9853
void UObject::execFRand(FFrame& Stack, RESULT_DECL)
{
    P_FINISH;
    *static_cast<FLOAT*>(Result) = appFrand();
}

// 0x000be0b1
void UObject::execFMin(FFrame& Stack, RESULT_DECL)
{
    P_GET_FLOAT(A);
    P_GET_FLOAT(B);
    P_FINISH;
    *static_cast<FLOAT*>(Result) = A <= B ? A : B;
}

// 0x000be132
void UObject::execFMax(FFrame& Stack, RESULT_DECL)
{
    P_GET_FLOAT(A);
    P_GET_FLOAT(B);
    P_FINISH;
    *static_cast<FLOAT*>(Result) = A < B ? B : A;
}

// 0x000be1b3
void UObject::execFClamp(FFrame& Stack, RESULT_DECL)
{
    P_GET_FLOAT(V);
    P_GET_FLOAT(A);
    P_GET_FLOAT(B);
    P_FINISH;
    *static_cast<FLOAT*>(Result) = V < A ? A : V < B ? V : B;
}

// 0x000b988e
void UObject::execLerp(FFrame& Stack, RESULT_DECL)
{
    P_GET_FLOAT(Alpha);
    P_GET_FLOAT(A);
    P_GET_FLOAT(B);
    P_FINISH;
    *static_cast<FLOAT*>(Result) = (B - A) * Alpha + A;
}

// 0x000b9935. like Lerp, eased with 3a^2 - 2a^3.
void UObject::execSmerp(FFrame& Stack, RESULT_DECL)
{
    P_GET_FLOAT(Alpha);
    P_GET_FLOAT(A);
    P_GET_FLOAT(B);
    P_FINISH;
    *static_cast<FLOAT*>(Result) = (Alpha * Alpha * 3.f - Alpha * Alpha * Alpha * 2.f) * (B - A) + A;
}

// 0x000b9a00
void UObject::execCeil(FFrame& Stack, RESULT_DECL)
{
    P_GET_FLOAT(A);
    P_FINISH;
    *static_cast<FLOAT*>(Result) = static_cast<FLOAT>(appCeil(A));
}

// 0x000b9ae4. halves round to even.
void UObject::execRound(FFrame& Stack, RESULT_DECL)
{
    P_GET_FLOAT(A);
    P_FINISH;
    *static_cast<FLOAT*>(Result) = static_cast<FLOAT>(appRound(A));
}

// 0x000b9a60
void UObject::execFrac(FFrame& Stack, RESULT_DECL)
{
    P_GET_FLOAT(A);
    P_FINISH;
    *static_cast<FLOAT*>(Result) = A - static_cast<FLOAT>(appFloor(A));
}

// string operators. ==, != and the orderings are case sensitive; ~= is not.

// 0x000c1979
void UObject::execConcat_StringString(FFrame& Stack, RESULT_DECL)
{
    P_GET_STR(A);
    P_GET_STR(B);
    P_FINISH;
    *static_cast<FString*>(Result) = A + B;
}

// 0x000c1a3c. joins with a space.
void UObject::execAt_StringString(FFrame& Stack, RESULT_DECL)
{
    P_GET_STR(A);
    P_GET_STR(B);
    P_FINISH;
    *static_cast<FString*>(Result) = A + TEXT(" ") + B;
}

// 0x000bf313
void UObject::execLess_StringString(FFrame& Stack, RESULT_DECL)
{
    P_GET_STR(A);
    P_GET_STR(B);
    P_FINISH;
    *static_cast<DWORD*>(Result) = appStrcmp(*A, *B) < 0;
}

// 0x000bf3da
void UObject::execGreater_StringString(FFrame& Stack, RESULT_DECL)
{
    P_GET_STR(A);
    P_GET_STR(B);
    P_FINISH;
    *static_cast<DWORD*>(Result) = appStrcmp(*A, *B) > 0;
}

// 0x000bf4a1
void UObject::execLessEqual_StringString(FFrame& Stack, RESULT_DECL)
{
    P_GET_STR(A);
    P_GET_STR(B);
    P_FINISH;
    *static_cast<DWORD*>(Result) = appStrcmp(*A, *B) <= 0;
}

// 0x000bf568
void UObject::execGreaterEqual_StringString(FFrame& Stack, RESULT_DECL)
{
    P_GET_STR(A);
    P_GET_STR(B);
    P_FINISH;
    *static_cast<DWORD*>(Result) = appStrcmp(*A, *B) >= 0;
}

// 0x000bf62f
void UObject::execEqualEqual_StringString(FFrame& Stack, RESULT_DECL)
{
    P_GET_STR(A);
    P_GET_STR(B);
    P_FINISH;
    *static_cast<DWORD*>(Result) = appStrcmp(*A, *B) == 0;
}

// 0x000bf6f4
void UObject::execNotEqual_StringString(FFrame& Stack, RESULT_DECL)
{
    P_GET_STR(A);
    P_GET_STR(B);
    P_FINISH;
    *static_cast<DWORD*>(Result) = appStrcmp(*A, *B) != 0;
}

// 0x000bf7ba
void UObject::execComplementEqual_StringString(FFrame& Stack, RESULT_DECL)
{
    P_GET_STR(A);
    P_GET_STR(B);
    P_FINISH;
    *static_cast<DWORD*>(Result) = appStricmp(*A, *B) == 0;
}

// 0x000bf87f
void UObject::execLen(FFrame& Stack, RESULT_DECL)
{
    P_GET_STR(S);
    P_FINISH;
    *static_cast<INT*>(Result) = S.Len();
}

// 0x000bf8fd
void UObject::execInStr(FFrame& Stack, RESULT_DECL)
{
    P_GET_STR(S);
    P_GET_STR(T);
    P_FINISH;
    *static_cast<INT*>(Result) = S.InStr(*T);
}

// 0x000c1027
void UObject::execMid(FFrame& Stack, RESULT_DECL)
{
    P_GET_STR(S);
    P_GET_INT(I);
    P_GET_INT_OPTX(J, 65535);
    P_FINISH;
    *static_cast<FString*>(Result) = S.Mid(I, J);
}

// 0x000c10f9
void UObject::execLeft(FFrame& Stack, RESULT_DECL)
{
    P_GET_STR(S);
    P_GET_INT(I);
    P_FINISH;
    *static_cast<FString*>(Result) = S.Left(I);
}

// 0x000c11a8
void UObject::execRight(FFrame& Stack, RESULT_DECL)
{
    P_GET_STR(S);
    P_GET_INT(I);
    P_FINISH;
    *static_cast<FString*>(Result) = S.Right(I);
}

// 0x000c1257
void UObject::execCaps(FFrame& Stack, RESULT_DECL)
{
    P_GET_STR(S);
    P_FINISH;
    *static_cast<FString*>(Result) = S.Caps();
}

// 0x000c0af4
void UObject::execChr(FFrame& Stack, RESULT_DECL)
{
    P_GET_INT(I);
    P_FINISH;
    TCHAR Temp[2];
    Temp[0] = static_cast<TCHAR>(I);
    Temp[1] = 0;
    *static_cast<FString*>(Result) = Temp;
}

// 0x000bf9a9. xbox characters are signed, so Latin-1 letters give negative codes.
void UObject::execAsc(FFrame& Stack, RESULT_DECL)
{
    P_GET_STR(S);
    P_FINISH;
    *static_cast<INT*>(Result) = static_cast<signed char>(**S);
}

// object and name operators

// 0x000b83de, the same code as EqualEqual_IntInt on the xbox
void UObject::execEqualEqual_ObjectObject(FFrame& Stack, RESULT_DECL)
{
    P_GET_OBJECT(UObject, A);
    P_GET_OBJECT(UObject, B);
    P_FINISH;
    *static_cast<DWORD*>(Result) = A == B;
}

// 0x000bd37d, the same code as NotEqual_IntInt on the xbox
void UObject::execNotEqual_ObjectObject(FFrame& Stack, RESULT_DECL)
{
    P_GET_OBJECT(UObject, A);
    P_GET_OBJECT(UObject, B);
    P_FINISH;
    *static_cast<DWORD*>(Result) = A != B;
}

// 0x000be619
void UObject::execEqualEqual_NameName(FFrame& Stack, RESULT_DECL)
{
    P_GET_NAME(A);
    P_GET_NAME(B);
    P_FINISH;
    *static_cast<DWORD*>(Result) = A == B;
}

// 0x000be68c
void UObject::execNotEqual_NameName(FFrame& Stack, RESULT_DECL)
{
    P_GET_NAME(A);
    P_GET_NAME(B);
    P_FINISH;
    *static_cast<DWORD*>(Result) = A != B;
}

// logging and localization

// 0x000bfa2d
void UObject::execLog(FFrame& Stack, RESULT_DECL)
{
    P_GET_STR(S);
    P_GET_NAME_OPTX(Tag, NAME_ScriptLog);
    P_FINISH;
    GLog->Logf(static_cast<EName>(Tag.GetIndex()), TEXT("%s"), *S);
}

// 0x000bfaf5. logged through the frame, so the warning names the script location.
void UObject::execWarn(FFrame& Stack, RESULT_DECL)
{
    P_GET_STR(S);
    P_FINISH;
    Stack.Logf(TEXT("%s"), *S);
}

// 0x000c0b4f
void UObject::execLocalize(FFrame& Stack, RESULT_DECL)
{
    P_GET_STR(SectionName);
    P_GET_STR(KeyName);
    P_GET_STR(PackageName);
    P_GET_UBOOL(bOptional);
    P_FINISH;
    *static_cast<FString*>(Result) = Localize(*SectionName, *KeyName, *PackageName, nullptr, bOptional);
}

// states

// 0x000bfca2. the state defaults to the current one, so GotoState('', 'Label') jumps within it.
void UObject::execGotoState(FFrame& Stack, RESULT_DECL)
{
    FName CurrentStateName = StateFrame && StateFrame->StateNode != Class ? StateFrame->StateNode->GetFName() : FName(NAME_None);
    P_GET_NAME_OPTX(S, CurrentStateName);
    P_GET_NAME_OPTX(L, NAME_None);
    P_FINISH;

    EGotoState GotoResult = GOTOSTATE_Success;
    if (S != CurrentStateName)
        GotoResult = GotoState(S);
    if (GotoResult == GOTOSTATE_Success)
    {
        if (!GotoLabel(L == NAME_None ? FName(NAME_Begin) : L) && L != NAME_None)
            Stack.Logf(TEXT("GotoState (%s %s): Label not found"), *S, *L);
    }
    else if (GotoResult == GOTOSTATE_NotFound && S != NAME_None && S != NAME_Auto)
    {
        Stack.Logf(TEXT("GotoState (%s %s): State not found"), *S, *L);
    }
}

// 0x000be89d
void UObject::execIsInState(FFrame& Stack, RESULT_DECL)
{
    P_GET_NAME(StateName);
    P_FINISH;
    *static_cast<DWORD*>(Result) = IsInState(StateName);
}

// 0x000be8f5
void UObject::execGetStateName(FFrame& Stack, RESULT_DECL)
{
    P_FINISH;
    *static_cast<FName*>(Result) = StateFrame && StateFrame->StateNode ? StateFrame->StateNode->GetFName() : FName(NAME_None);
}

// 0x000bfdba. turns a probe back on, as far as the class and state allow.
void UObject::execEnable(FFrame& Stack, RESULT_DECL)
{
    P_GET_NAME(N);
    if (N.GetIndex() >= NAME_PROBEMIN && N.GetIndex() < NAME_PROBEMAX && StateFrame)
    {
        const QWORD BaseProbeMask = (GetClass()->ProbeMask | StateFrame->StateNode->ProbeMask) & StateFrame->StateNode->IgnoreMask;
        StateFrame->ProbeMask |= BaseProbeMask & (static_cast<QWORD>(1) << (N.GetIndex() - NAME_PROBEMIN));
    }
    else
    {
        Stack.Logf(TEXT("Enable: '%s' is not a probe function"), *N);
    }
    P_FINISH;
}

// 0x000bfe9c. its warning says Enable too.
void UObject::execDisable(FFrame& Stack, RESULT_DECL)
{
    P_GET_NAME(N);
    P_FINISH;
    if (N.GetIndex() >= NAME_PROBEMIN && N.GetIndex() < NAME_PROBEMAX && StateFrame)
        StateFrame->ProbeMask &= ~(static_cast<QWORD>(1) << (N.GetIndex() - NAME_PROBEMIN));
    else
        Stack.Logf(TEXT("Enable: '%s' is not a probe function"), *N);
}

// classes and objects

// 0x000be6ff
void UObject::execClassIsChildOf(FFrame& Stack, RESULT_DECL)
{
    P_GET_OBJECT(UClass, K);
    P_GET_OBJECT(UClass, C);
    P_FINISH;
    *static_cast<DWORD*>(Result) = C && K ? K->IsChildOf(C) : 0;
}

// 0x000be942. on a class object this checks the class itself and its parents, not UClass.
void UObject::execIsA(FFrame& Stack, RESULT_DECL)
{
    P_GET_NAME(ClassName);
    P_FINISH;
    UClass* TempClass = IsA(UClass::StaticClass()) ? static_cast<UClass*>(this) : GetClass();
    while (TempClass && TempClass->GetFName() != ClassName)
        TempClass = TempClass->GetSuperClass();
    *static_cast<DWORD*>(Result) = TempClass != nullptr;
}

// 0x000be7c1
void UObject::execGetEnum(FFrame& Stack, RESULT_DECL)
{
    P_GET_OBJECT(UObject, E);
    P_GET_INT(I);
    P_FINISH;
    *static_cast<FName*>(Result) = NAME_None;
    UEnum* Enum = Cast<UEnum>(E);
    if (Enum && I >= 0 && I < Enum->Names.Num())
        *static_cast<FName*>(Result) = Enum->Names[I];
}

// 0x000c0083. an empty name leaves the result alone.
void UObject::execDynamicLoadObject(FFrame& Stack, RESULT_DECL)
{
    P_GET_STR(Name);
    P_GET_OBJECT(UClass, ObjectClass);
    P_GET_UBOOL(bMayFail);
    P_FINISH;
    if (Name.Len())
        *static_cast<UObject**>(Result) = StaticLoadObject(ObjectClass, nullptr, *Name, nullptr, LOAD_Unknown2 | (bMayFail ? LOAD_NoThrow : 0), nullptr);
}

// 0x000c016c
void UObject::execFindObject(FFrame& Stack, RESULT_DECL)
{
    P_GET_STR(Name);
    P_GET_OBJECT(UClass, ObjectClass);
    P_FINISH;
    *static_cast<UObject**>(Result) = StaticFindObject(ObjectClass, nullptr, *Name);
}

// misc

// 0x000bd3ef
void UObject::execIsOnConsole(FFrame& Stack, RESULT_DECL)
{
    P_FINISH;
    *static_cast<DWORD*>(Result) = 1;
}

// 0x000bd72b. writes a zero result although the function returns nothing.
void UObject::execSetUTracing(FFrame& Stack, RESULT_DECL)
{
    P_GET_UBOOL(bNewUTracing);
    P_FINISH;
    GIsUTracing = bNewUTracing;
    *static_cast<DWORD*>(Result) = 0;
}

// 0x000bd77e
void UObject::execIsUTracing(FFrame& Stack, RESULT_DECL)
{
    P_FINISH;
    *static_cast<DWORD*>(Result) = GIsUTracing;
}

// config

// 0x000bff57. an empty section name means none.
void UObject::execSaveConfig(FFrame& Stack, RESULT_DECL)
{
    P_GET_STR(ForcedDirtySection);
    P_FINISH;
    SaveConfig(CPF_Config, nullptr, ForcedDirtySection == TEXT("") ? nullptr : *ForcedDirtySection);
}

// 0x000be77e
void UObject::execStaticSaveConfig(FFrame& Stack, RESULT_DECL)
{
    P_FINISH;
    GetClass()->GetDefaultObject()->SaveConfig(CPF_Config, nullptr, nullptr);
}

// 0x000bd62b
void UObject::execResetConfig(FFrame& Stack, RESULT_DECL)
{
    P_FINISH;
    GetClass()->ResetConfig();
}

// 0x000bfffb
void UObject::execUnloadInts(FFrame& Stack, RESULT_DECL)
{
    P_GET_STR(IntBase);
    P_FINISH;
    GConfig->UnloadLocalizedFiles(*IntBase);
}

// 0x000c0c6c. only public properties can be read by name.
void UObject::execGetPropertyText(FFrame& Stack, RESULT_DECL)
{
    P_GET_STR(PropName);
    P_FINISH;
    UProperty* Property = FindField<UProperty>(GetClass(), *PropName);
    if (Property && (Property->GetFlags() & RF_Public))
    {
        TCHAR Temp[1024] = {};
        Property->ExportText(0, Temp, reinterpret_cast<BYTE*>(this), reinterpret_cast<BYTE*>(this), 1);
        *static_cast<FString*>(Result) = Temp;
    }
    else
    {
        *static_cast<FString*>(Result) = TEXT("");
    }
}

// 0x000c05f2. only public, non-const properties can be set by name.
void UObject::execSetPropertyText(FFrame& Stack, RESULT_DECL)
{
    P_GET_STR(PropName);
    P_GET_STR(PropValue);
    P_FINISH;
    UProperty* Property = FindField<UProperty>(GetClass(), *PropName);
    if (Property && (Property->GetFlags() & RF_Public) && !(Property->PropertyFlags & (CPF_Const | CPF_Static)))
        Property->ImportText(*PropValue, reinterpret_cast<BYTE*>(this) + Property->Offset, 1);
}

// vector operators

// 0x000b9cea
void UObject::execSubtract_PreVector(FFrame& Stack, RESULT_DECL)
{
    P_GET_STRUCT(FVector, A);
    P_FINISH;
    *static_cast<FVector*>(Result) = -A;
}

// 0x000b9d5f
void UObject::execMultiply_VectorFloat(FFrame& Stack, RESULT_DECL)
{
    P_GET_STRUCT(FVector, A);
    P_GET_FLOAT(B);
    P_FINISH;
    *static_cast<FVector*>(Result) = A * B;
}

// 0x000b9dfb
void UObject::execMultiply_FloatVector(FFrame& Stack, RESULT_DECL)
{
    P_GET_FLOAT(A);
    P_GET_STRUCT(FVector, B);
    P_FINISH;
    *static_cast<FVector*>(Result) = B * A;
}

// 0x000b9e97
void UObject::execMultiply_VectorVector(FFrame& Stack, RESULT_DECL)
{
    P_GET_STRUCT(FVector, A);
    P_GET_STRUCT(FVector, B);
    P_FINISH;
    *static_cast<FVector*>(Result) = A * B;
}

// 0x000b9f2b
void UObject::execDivide_VectorFloat(FFrame& Stack, RESULT_DECL)
{
    P_GET_STRUCT(FVector, A);
    P_GET_FLOAT(B);
    P_FINISH;
    *static_cast<FVector*>(Result) = A / B;
}

// 0x000b9fcd
void UObject::execAdd_VectorVector(FFrame& Stack, RESULT_DECL)
{
    P_GET_STRUCT(FVector, A);
    P_GET_STRUCT(FVector, B);
    P_FINISH;
    *static_cast<FVector*>(Result) = A + B;
}

// 0x000ba061
void UObject::execSubtract_VectorVector(FFrame& Stack, RESULT_DECL)
{
    P_GET_STRUCT(FVector, A);
    P_GET_STRUCT(FVector, B);
    P_FINISH;
    *static_cast<FVector*>(Result) = A - B;
}

// 0x000ba0f5. world to local: A in the frame of rotation B. the xbox goes through GMath.UnitCoords / B;
// dotting with the rows of the rotation matrix gives the same result.
void UObject::execLessLess_VectorRotator(FFrame& Stack, RESULT_DECL)
{
    P_GET_STRUCT(FVector, A);
    P_GET_STRUCT(FRotator, B);
    P_FINISH;
    const FMatrix M = FRotationTranslationMatrix(B, FVector(0.f, 0.f, 0.f));
    *static_cast<FVector*>(Result) = FVector(
        A.X * M.M[0][0] + A.Y * M.M[0][1] + A.Z * M.M[0][2],
        A.X * M.M[1][0] + A.Y * M.M[1][1] + A.Z * M.M[1][2],
        A.X * M.M[2][0] + A.Y * M.M[2][1] + A.Z * M.M[2][2]);
}

// 0x000ba9ed. local to world, the inverse of <<; vect(1,0,0) >> R is R's direction.
void UObject::execGreaterGreater_VectorRotator(FFrame& Stack, RESULT_DECL)
{
    P_GET_STRUCT(FVector, A);
    P_GET_STRUCT(FRotator, B);
    P_FINISH;
    const FMatrix M = FRotationTranslationMatrix(B, FVector(0.f, 0.f, 0.f));
    *static_cast<FVector*>(Result) = FVector(
        A.X * M.M[0][0] + A.Y * M.M[1][0] + A.Z * M.M[2][0],
        A.X * M.M[0][1] + A.Y * M.M[1][1] + A.Z * M.M[2][1],
        A.X * M.M[0][2] + A.Y * M.M[1][2] + A.Z * M.M[2][2]);
}

// 0x000bb2e5
void UObject::execEqualEqual_VectorVector(FFrame& Stack, RESULT_DECL)
{
    P_GET_STRUCT(FVector, A);
    P_GET_STRUCT(FVector, B);
    P_FINISH;
    *static_cast<DWORD*>(Result) = A == B;
}

// 0x000bb37a
void UObject::execNotEqual_VectorVector(FFrame& Stack, RESULT_DECL)
{
    P_GET_STRUCT(FVector, A);
    P_GET_STRUCT(FVector, B);
    P_FINISH;
    *static_cast<DWORD*>(Result) = A != B;
}

// 0x000bb40f
void UObject::execDot_VectorVector(FFrame& Stack, RESULT_DECL)
{
    P_GET_STRUCT(FVector, A);
    P_GET_STRUCT(FVector, B);
    P_FINISH;
    *static_cast<FLOAT*>(Result) = A | B;
}

// 0x000bb498
void UObject::execCross_VectorVector(FFrame& Stack, RESULT_DECL)
{
    P_GET_STRUCT(FVector, A);
    P_GET_STRUCT(FVector, B);
    P_FINISH;
    *static_cast<FVector*>(Result) = A ^ B;
}

// 0x000bb550
void UObject::execMultiplyEqual_VectorFloat(FFrame& Stack, RESULT_DECL)
{
    P_GET_STRUCT_REF(FVector, A);
    P_GET_FLOAT(B);
    P_FINISH;
    *static_cast<FVector*>(Result) = (*A = *A * B);
}

// 0x000bb612
void UObject::execMultiplyEqual_VectorVector(FFrame& Stack, RESULT_DECL)
{
    P_GET_STRUCT_REF(FVector, A);
    P_GET_STRUCT(FVector, B);
    P_FINISH;
    *static_cast<FVector*>(Result) = (*A = *A * B);
}

// 0x000bb6ca
void UObject::execDivideEqual_VectorFloat(FFrame& Stack, RESULT_DECL)
{
    P_GET_STRUCT_REF(FVector, A);
    P_GET_FLOAT(B);
    P_FINISH;
    *static_cast<FVector*>(Result) = (*A = *A / B);
}

// 0x000bb794
void UObject::execAddEqual_VectorVector(FFrame& Stack, RESULT_DECL)
{
    P_GET_STRUCT_REF(FVector, A);
    P_GET_STRUCT(FVector, B);
    P_FINISH;
    *static_cast<FVector*>(Result) = (*A = *A + B);
}

// 0x000bb84c
void UObject::execSubtractEqual_VectorVector(FFrame& Stack, RESULT_DECL)
{
    P_GET_STRUCT_REF(FVector, A);
    P_GET_STRUCT(FVector, B);
    P_FINISH;
    *static_cast<FVector*>(Result) = (*A = *A - B);
}

// 0x000bb904
void UObject::execVSize(FFrame& Stack, RESULT_DECL)
{
    P_GET_STRUCT(FVector, A);
    P_FINISH;
    *static_cast<FLOAT*>(Result) = A.Size();
}

// 0x000bb9da
void UObject::execVSizeSq(FFrame& Stack, RESULT_DECL)
{
    P_GET_STRUCT(FVector, A);
    P_FINISH;
    *static_cast<FLOAT*>(Result) = A.SizeSquared();
}

// 0x000bba47
void UObject::execVSize2D(FFrame& Stack, RESULT_DECL)
{
    P_GET_STRUCT(FVector, A);
    P_FINISH;
    *static_cast<FLOAT*>(Result) = A.Size2D();
}

// 0x000be261
void UObject::execVDistSq(FFrame& Stack, RESULT_DECL)
{
    P_GET_STRUCT(FVector, A);
    P_GET_STRUCT(FVector, B);
    P_FINISH;
    *static_cast<FLOAT*>(Result) = (A - B).SizeSquared();
}

// 0x000be2ff
void UObject::execVDist2DSq(FFrame& Stack, RESULT_DECL)
{
    P_GET_STRUCT(FVector, A);
    P_GET_STRUCT(FVector, B);
    P_FINISH;
    *static_cast<FLOAT*>(Result) = (A.X - B.X) * (A.X - B.X) + (A.Y - B.Y) * (A.Y - B.Y);
}

// 0x000bbb10
void UObject::execNormal(FFrame& Stack, RESULT_DECL)
{
    P_GET_STRUCT(FVector, A);
    P_FINISH;
    *static_cast<FVector*>(Result) = A.SafeNormal();
}

// 0x000bbc0b. the matrix gets a zero fourth row, so it is always singular and Inverse gives the identity:
// the three vectors always come back as the unit axes.
void UObject::execInvert(FFrame& Stack, RESULT_DECL)
{
    P_GET_STRUCT_REF(FVector, X);
    P_GET_STRUCT_REF(FVector, Y);
    P_GET_STRUCT_REF(FVector, Z);
    P_FINISH;
    const FMatrix M = FMatrix(FPlane(*X, 0.f), FPlane(*Y, 0.f), FPlane(*Z, 0.f), FPlane(0.f, 0.f, 0.f, 0.f)).Inverse();
    *X = FVector(M.M[0][0], M.M[0][1], M.M[0][2]);
    *Y = FVector(M.M[1][0], M.M[1][1], M.M[1][2]);
    *Z = FVector(M.M[2][0], M.M[2][1], M.M[2][2]);
}

// 0x000bbda1
void UObject::execVIsZero(FFrame& Stack, RESULT_DECL)
{
    P_GET_STRUCT(FVector, A);
    P_FINISH;
    *static_cast<DWORD*>(Result) = A.IsZero();
}

// 0x000bbe20. a random point in the unit ball, scaled to length 1.
void UObject::execVRand(FFrame& Stack, RESULT_DECL)
{
    P_FINISH;
    FVector V;
    do
    {
        V.X = appRand() * (2.f / 32767.f) - 1.f;
        V.Y = appRand() * (2.f / 32767.f) - 1.f;
        V.Z = appRand() * (2.f / 32767.f) - 1.f;
    } while (V.SizeSquared() > 1.f);
    *static_cast<FVector*>(Result) = V * appInvSqrt(V.SizeSquared());
}

// 0x000bbff4
void UObject::execMirrorVectorByNormal(FFrame& Stack, RESULT_DECL)
{
    P_GET_STRUCT(FVector, A);
    P_GET_STRUCT(FVector, B);
    P_FINISH;
    const FVector N = B.SafeNormal();
    *static_cast<FVector*>(Result) = A - N * 2.f * (A | N);
}

// 0x000be388. the pitch that lands a projectile DeltaZ up and HorizDist away; ShouldFire is cleared when
// it cannot reach.
void UObject::execCalculateAimPitch(FFrame& Stack, RESULT_DECL)
{
    P_GET_FLOAT(ProjSpeed);
    P_GET_FLOAT(DeltaZ);
    P_GET_FLOAT(HorizDist);
    P_GET_FLOAT(Gravity);
    P_GET_INT_REF(ShouldFire);
    P_FINISH;
    const FLOAT RSpeedSq = 1.f / (ProjSpeed * ProjSpeed);
    const FLOAT G = HorizDist * HorizDist * Gravity;
    const FLOAT GOverSpeedSq = RSpeedSq * G;
    const FLOAT Disc = (HorizDist * HorizDist - GOverSpeedSq * GOverSpeedSq) + RSpeedSq * G * DeltaZ * 2.f;
    FLOAT Angle = 0.f;
    if (Disc <= 0.f)
    {
        *ShouldFire = 0;
    }
    else
    {
        *ShouldFire = 1;
        Angle = static_cast<FLOAT>(atan((appSqrt(Disc) - HorizDist) / G * (ProjSpeed * ProjSpeed)));
    }
    *static_cast<INT*>(Result) = static_cast<INT>(Angle * (32768.f / 3.1415926535897932f));
}

// rotator operators

// 0x000bc18e
void UObject::execEqualEqual_RotatorRotator(FFrame& Stack, RESULT_DECL)
{
    P_GET_STRUCT(FRotator, A);
    P_GET_STRUCT(FRotator, B);
    P_FINISH;
    *static_cast<DWORD*>(Result) = A == B;
}

// 0x000bc20e
void UObject::execNotEqual_RotatorRotator(FFrame& Stack, RESULT_DECL)
{
    P_GET_STRUCT(FRotator, A);
    P_GET_STRUCT(FRotator, B);
    P_FINISH;
    *static_cast<DWORD*>(Result) = A != B;
}

// 0x000bc28e
void UObject::execMultiply_RotatorFloat(FFrame& Stack, RESULT_DECL)
{
    P_GET_STRUCT(FRotator, A);
    P_GET_FLOAT(B);
    P_FINISH;
    *static_cast<FRotator*>(Result) = A * B;
}

// 0x000bc330
void UObject::execMultiply_FloatRotator(FFrame& Stack, RESULT_DECL)
{
    P_GET_FLOAT(A);
    P_GET_STRUCT(FRotator, B);
    P_FINISH;
    *static_cast<FRotator*>(Result) = B * A;
}

// 0x000bc3d2. divides by multiplying with the reciprocal.
void UObject::execDivide_RotatorFloat(FFrame& Stack, RESULT_DECL)
{
    P_GET_STRUCT(FRotator, A);
    P_GET_FLOAT(B);
    P_FINISH;
    *static_cast<FRotator*>(Result) = A * (1.f / B);
}

// 0x000bc47e
void UObject::execMultiplyEqual_RotatorFloat(FFrame& Stack, RESULT_DECL)
{
    P_GET_STRUCT_REF(FRotator, A);
    P_GET_FLOAT(B);
    P_FINISH;
    *static_cast<FRotator*>(Result) = (*A *= B);
}

// 0x000bc528
void UObject::execDivideEqual_RotatorFloat(FFrame& Stack, RESULT_DECL)
{
    P_GET_STRUCT_REF(FRotator, A);
    P_GET_FLOAT(B);
    P_FINISH;
    *static_cast<FRotator*>(Result) = (*A *= 1.f / B);
}

// 0x000bc5de
void UObject::execAdd_RotatorRotator(FFrame& Stack, RESULT_DECL)
{
    P_GET_STRUCT(FRotator, A);
    P_GET_STRUCT(FRotator, B);
    P_FINISH;
    *static_cast<FRotator*>(Result) = A + B;
}

// 0x000bc666
void UObject::execSubtract_RotatorRotator(FFrame& Stack, RESULT_DECL)
{
    P_GET_STRUCT(FRotator, A);
    P_GET_STRUCT(FRotator, B);
    P_FINISH;
    *static_cast<FRotator*>(Result) = A - B;
}

// 0x000bc6e8
void UObject::execAddEqual_RotatorRotator(FFrame& Stack, RESULT_DECL)
{
    P_GET_STRUCT_REF(FRotator, A);
    P_GET_STRUCT(FRotator, B);
    P_FINISH;
    *static_cast<FRotator*>(Result) = (*A = *A + B);
}

// 0x000bc786
void UObject::execSubtractEqual_RotatorRotator(FFrame& Stack, RESULT_DECL)
{
    P_GET_STRUCT_REF(FRotator, A);
    P_GET_STRUCT(FRotator, B);
    P_FINISH;
    *static_cast<FRotator*>(Result) = (*A = *A - B);
}

// 0x000bc824. the rows of the rotation matrix.
void UObject::execGetAxes(FFrame& Stack, RESULT_DECL)
{
    P_GET_STRUCT(FRotator, A);
    P_GET_STRUCT_REF(FVector, X);
    P_GET_STRUCT_REF(FVector, Y);
    P_GET_STRUCT_REF(FVector, Z);
    P_FINISH;
    const FMatrix M = FRotationTranslationMatrix(A, FVector(0.f, 0.f, 0.f));
    *X = FVector(M.M[0][0], M.M[0][1], M.M[0][2]);
    *Y = FVector(M.M[1][0], M.M[1][1], M.M[1][2]);
    *Z = FVector(M.M[2][0], M.M[2][1], M.M[2][2]);
}

// 0x000bc973. the columns of the rotation matrix.
void UObject::execGetUnAxes(FFrame& Stack, RESULT_DECL)
{
    P_GET_STRUCT(FRotator, A);
    P_GET_STRUCT_REF(FVector, X);
    P_GET_STRUCT_REF(FVector, Y);
    P_GET_STRUCT_REF(FVector, Z);
    P_FINISH;
    const FMatrix M = FRotationTranslationMatrix(A, FVector(0.f, 0.f, 0.f));
    *X = FVector(M.M[0][0], M.M[1][0], M.M[2][0]);
    *Y = FVector(M.M[0][1], M.M[1][1], M.M[2][1]);
    *Z = FVector(M.M[0][2], M.M[1][2], M.M[2][2]);
}

// 0x000bbf68. each angle is an even number below 65535; roll stays 0 unless asked for.
void UObject::execRotRand(FFrame& Stack, RESULT_DECL)
{
    P_GET_UBOOL(bRoll);
    P_FINISH;
    FRotator R;
    R.Yaw = (appRand() << 1) % 0xffff;
    R.Pitch = (appRand() << 1) % 0xffff;
    R.Roll = bRoll ? (appRand() << 1) % 0xffff : 0;
    *static_cast<FRotator*>(Result) = R;
}

// 0x000bcb72
void UObject::execOrthoRotation(FFrame& Stack, RESULT_DECL)
{
    P_GET_STRUCT(FVector, X);
    P_GET_STRUCT(FVector, Y);
    P_GET_STRUCT(FVector, Z);
    P_FINISH;
    const FMatrix M(FPlane(X, 0.f), FPlane(Y, 0.f), FPlane(Z, 0.f), FPlane(0.f, 0.f, 0.f, 0.f));
    *static_cast<FRotator*>(Result) = M.Rotator();
}

// 0x000bcc77. each angle to the signed range -32768 to 32767.
void UObject::execNormalize(FFrame& Stack, RESULT_DECL)
{
    P_GET_STRUCT(FRotator, R);
    P_FINISH;
    *static_cast<FRotator*>(Result) = FRotator(static_cast<SWORD>(R.Pitch), static_cast<SWORD>(R.Yaw), static_cast<SWORD>(R.Roll));
}

// 0x000be584. whether turning from B to A is clockwise, taking the short way round.
void UObject::execClockwiseFrom_IntInt(FFrame& Stack, RESULT_DECL)
{
    P_GET_INT(IntA);
    P_GET_INT(IntB);
    P_FINISH;
    IntA &= 0xffff;
    IntB &= 0xffff;
    INT Diff = IntA - IntB;
    if (Diff < 0)
        Diff = -Diff;
    *static_cast<DWORD*>(Result) = Diff <= 32768 ? IntB < IntA : IntA < IntB;
}

// interpolation curves

// 0x000c0f86
void UObject::execInterpCurveEval(FFrame& Stack, RESULT_DECL)
{
    P_GET_STRUCT(FInterpCurve, Curve);
    P_GET_FLOAT(Input);
    P_FINISH;
    *static_cast<FLOAT*>(Result) = Curve.Eval(Input);
}

// 0x000c097a. both 0 with fewer than two points.
void UObject::execInterpCurveGetOutputRange(FFrame& Stack, RESULT_DECL)
{
    P_GET_STRUCT(FInterpCurve, Curve);
    P_GET_FLOAT_REF(Min);
    P_GET_FLOAT_REF(Max);
    P_FINISH;
    if (Curve.Points.Num() < 2)
    {
        *Min = 0.f;
        *Max = 0.f;
        return;
    }
    *Min = Curve.Points[0].OutVal;
    *Max = Curve.Points[0].OutVal;
    for (INT i = 1; i < Curve.Points.Num(); i++)
    {
        *Min = *Min <= Curve.Points[i].OutVal ? *Min : Curve.Points[i].OutVal;
        *Max = *Max < Curve.Points[i].OutVal ? Curve.Points[i].OutVal : *Max;
    }
}

// 0x000c0804. both 0 with fewer than two points.
void UObject::execInterpCurveGetInputDomain(FFrame& Stack, RESULT_DECL)
{
    P_GET_STRUCT(FInterpCurve, Curve);
    P_GET_FLOAT_REF(Min);
    P_GET_FLOAT_REF(Max);
    P_FINISH;
    if (Curve.Points.Num() < 2)
    {
        *Min = 0.f;
        *Max = 0.f;
        return;
    }
    *Min = Curve.Points[0].InVal;
    *Max = Curve.Points[0].InVal;
    for (INT i = 1; i < Curve.Points.Num(); i++)
    {
        *Min = *Min <= Curve.Points[i].InVal ? *Min : Curve.Points[i].InVal;
        *Max = *Max < Curve.Points[i].InVal ? Curve.Points[i].InVal : *Max;
    }
}

// quaternions

// 0x000bccda
void UObject::execQuatProduct(FFrame& Stack, RESULT_DECL)
{
    P_GET_STRUCT(FQuat, A);
    P_GET_STRUCT(FQuat, B);
    P_FINISH;
    *static_cast<FQuat*>(Result) = FQuat(
        (A.Z * B.Y + A.W * B.X + A.X * B.W) - A.Y * B.Z,
        (A.Y * B.W - A.Z * B.X) + A.X * B.Z + A.W * B.Y,
        ((A.Z * B.W + A.Y * B.X) - A.X * B.Y) + A.W * B.Z,
        ((A.W * B.W - A.X * B.X) - A.Y * B.Y) - A.Z * B.Z);
}

// 0x000bce14
void UObject::execQuatInvert(FFrame& Stack, RESULT_DECL)
{
    P_GET_STRUCT(FQuat, A);
    P_FINISH;
    *static_cast<FQuat*>(Result) = FQuat(0.f - A.X, 0.f - A.Y, 0.f - A.Z, A.W);
}

// 0x000bce94
void UObject::execQuatRotateVector(FFrame& Stack, RESULT_DECL)
{
    P_GET_STRUCT(FQuat, A);
    P_GET_STRUCT(FVector, B);
    P_FINISH;
    *static_cast<FVector*>(Result) = A.RotateVector(B);
}

// 0x000bcf09. the rotation taking A onto B; the identity when they are nearly parallel.
void UObject::execQuatFindBetween(FFrame& Stack, RESULT_DECL)
{
    P_GET_STRUCT(FVector, A);
    P_GET_STRUCT(FVector, B);
    P_FINISH;
    const FVector Cross = A ^ B;
    const FLOAT CrossMag = Cross.Size();
    FQuat Q(0.f, 0.f, 0.f, 1.f);
    if (CrossMag >= 0.0001f)
    {
        FLOAT Angle = static_cast<FLOAT>(asin(CrossMag));
        if ((A | B) < 0.f)
            Angle = 3.1415927f - Angle;
        const FLOAT RCrossMag = 1.f / CrossMag;
        const FLOAT SinHalf = static_cast<FLOAT>(sin(Angle * 0.5f));
        Q = FQuat(RCrossMag * Cross.X * SinHalf, Cross.Y * RCrossMag * SinHalf, Cross.Z * RCrossMag * SinHalf, static_cast<FLOAT>(cos(Angle * 0.5f)));
    }
    *static_cast<FQuat*>(Result) = Q;
}

// 0x000bd11a
void UObject::execQuatFromAxisAndAngle(FFrame& Stack, RESULT_DECL)
{
    P_GET_STRUCT(FVector, Axis);
    P_GET_FLOAT(Angle);
    P_FINISH;
    const FLOAT S = static_cast<FLOAT>(sin(Angle * 0.5f));
    const FLOAT C = static_cast<FLOAT>(cos(Angle * 0.5f));
    const FVector N = Axis.SafeNormal();
    *static_cast<FQuat*>(Result) = FQuat(N.X * S, N.Y * S, N.Z * S, C);
}

// 0x000bd28c
void UObject::execQuatFromRotator(FFrame& Stack, RESULT_DECL)
{
    P_GET_STRUCT(FRotator, A);
    P_FINISH;
    *static_cast<FQuat*>(Result) = FQuat(FRotationTranslationMatrix(A, FVector(0.f, 0.f, 0.f)));
}

// 0x000bd305
void UObject::execQuatToRotator(FFrame& Stack, RESULT_DECL)
{
    P_GET_STRUCT(FQuat, A);
    P_FINISH;
    *static_cast<FRotator*>(Result) = FQuatRotationTranslationMatrix(A, FVector(0.f, 0.f, 0.f)).Rotator();
}
