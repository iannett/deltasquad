#include "Core.h"

IMPLEMENT_CLASS(UCommandlet, Core)
IMPLEMENT_CLASS(UHelloWorldCommandlet, Core)

// 0x00024d14
void UCommandlet::execMain(FFrame& Stack, RESULT_DECL)
{
    FString Parms;
    Stack.Step(Stack.Object, &Parms);
    P_FINISH;
    *static_cast<INT*>(Result) = Main(*Parms);
}

FNativeEntry GCommandletNatives[] =
{
    { TEXT("Main"), { static_cast<Native>(&UCommandlet::execMain) }, 0 },
    { nullptr, { nullptr }, 0 },
};
IMPLEMENT_NATIVE_TABLE(UCommandlet, GCommandletNatives)

// 0x00022fd0. the xbox fills this slot with a thunk that calls the same slot again.
INT UCommandlet::Main(const TCHAR* Parms)
{
    NOT_DECOMPILED(0x00022fd0);
    return 0;
}
