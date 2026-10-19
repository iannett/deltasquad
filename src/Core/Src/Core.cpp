#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>

#include "Core.h"

void* appMalloc(size_t Size)
{
    return malloc(Size);
}

void* appRealloc(void* Ptr, size_t Size)
{
    if (!Size)
    {
        free(Ptr);
        return nullptr;
    }
    return realloc(Ptr, Size);
}

void appFree(void* Ptr)
{
    free(Ptr);
}

// stand-in log devices until the platform layer is written
namespace
{
    // event names not in the name table yet print as their number
    const TCHAR* EventName(EName Event)
    {
        if (Event < FName::GetMaxNames() && FName::GetEntry(Event))
            return FName::GetEntry(Event)->Name;
        static TCHAR Buffer[16];
        snprintf(Buffer, sizeof(Buffer), "%d", Event);
        return Buffer;
    }

    class FOutputDeviceStdout : public FOutputDevice
    {
    public:
        void Serialize(const TCHAR* Text, EName Event) override
        {
            printf("%s: %s\n", EventName(Event), Text);
        }
    };

    class FOutputDeviceStdoutError : public FOutputDeviceError
    {
    public:
        void Serialize(const TCHAR* Text, EName Event) override
        {
            fprintf(stderr, "Critical: %s\n", Text);
            abort();
        }
    };

    class FFeedbackContextStdout : public FFeedbackContext
    {
    public:
        void Serialize(const TCHAR* Text, EName Event) override
        {
            printf("%s: %s\n", EventName(Event), Text);
        }
    };

    FOutputDeviceStdout      LogStdout;
    FOutputDeviceStdoutError ErrorStdout;
    FFeedbackContextStdout   WarnStdout;
}

FOutputDevice*      GLog   = &LogStdout;
FOutputDeviceError* GError = &ErrorStdout;
FFeedbackContext*   GWarn  = &WarnStdout;
FOutputDevice*      GLogDetail = &LogStdout;
UBOOL               GIsCriticalError;

void FOutputDevice::Log(const TCHAR* Text)
{
    Serialize(Text, NAME_Log);
}

// 0x00035bcd
void FOutputDevice::Logf(const TCHAR* Fmt, ...)
{
    if (FName::SafeSuppressed(NAME_Log))
        return;
    TCHAR Buffer[4096];
    va_list Args;
    va_start(Args, Fmt);
    vsnprintf(Buffer, sizeof(Buffer), Fmt, Args);
    va_end(Args);
    Serialize(Buffer, NAME_Log);
}

// 0x00035b8d
void FOutputDevice::Logf(EName Event, const TCHAR* Fmt, ...)
{
    if (FName::SafeSuppressed(Event))
        return;
    TCHAR Buffer[4096];
    va_list Args;
    va_start(Args, Fmt);
    vsnprintf(Buffer, sizeof(Buffer), Fmt, Args);
    va_end(Args);
    Serialize(Buffer, Event);
}

static TCHAR GBaseDir[1024];

// 0x0003b6a5. the xbox returned the System directory on the disc; the port's launcher sets it.
const TCHAR* appBaseDir()
{
    return GBaseDir;
}

void appSetBaseDir(const TCHAR* Dir)
{
    appStrncpy(GBaseDir, Dir, ARRAY_COUNT(GBaseDir));
}

DOUBLE appSeconds()
{
    return std::chrono::duration<DOUBLE>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

// 0x0003b4cf
void* appGetDllHandle(const TCHAR* Filename)
{
    static BYTE StaticDllHandle;
    return &StaticDllHandle;
}

// 0x0003de82 is the xbox version. the platform name stays "XBox" since game code may compare it.
void appPlatformPreInit()
{
    GPlatformName = TEXT("XBox");
}

const TCHAR* appComputerName()
{
    static TCHAR Name[256];
    if (!Name[0] && gethostname(Name, sizeof(Name)) != 0)
        appStrcpy(Name, TEXT("Unknown"));
    return Name;
}

const TCHAR* appUserName()
{
    const TCHAR* Name = getenv("USER");
    return Name ? Name : TEXT("Unknown");
}
