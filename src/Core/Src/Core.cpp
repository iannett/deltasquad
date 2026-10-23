#include <chrono>
#include <ctime>
#include <cstring>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#if defined(__APPLE__)
#include <malloc/malloc.h>
#else
#include <malloc.h>
#endif

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

// the size the allocator actually gave a block, as the xbox allocator reports for its memory stats
size_t appMallocSize(void* Ptr)
{
#if defined(__APPLE__)
    return malloc_size(Ptr);
#else
    return malloc_usable_size(Ptr);
#endif
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

void appThrowf(const TCHAR* Fmt, ...)
{
    // 0x00451318
    static TCHAR TempStr[4096];
    va_list Args;
    va_start(Args, Fmt);
    vsnprintf(TempStr, sizeof(TempStr), Fmt, Args);
    va_end(Args);
    throw static_cast<const TCHAR*>(TempStr);
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

// 0x0003add0. the crt's date and time formats, "MM/DD/YY HH:MM:SS".
const TCHAR* appTimestamp(UBOOL Date, UBOOL Time)
{
    static TCHAR Result[64];
    Result[0] = 0;
    const time_t Now = time(nullptr);
    const tm* Local = localtime(&Now);
    if (Date)
        strftime(Result, sizeof(Result), "%m/%d/%y", Local);
    if (Time)
    {
        if (Date)
            strcat(Result, " ");
        strftime(Result + strlen(Result), sizeof(Result) - strlen(Result), "%H:%M:%S", Local);
    }
    return Result;
}

// 0x0003b552. the local date and time, as the xbox's GetLocalTime gives it.
void appSystemTime(INT& Year, INT& Month, INT& DayOfWeek, INT& Day, INT& Hour, INT& Min, INT& Sec, INT& MSec)
{
    const auto Now = std::chrono::system_clock::now();
    const time_t Seconds = std::chrono::system_clock::to_time_t(Now);
    const tm* Local = localtime(&Seconds);
    Year = Local->tm_year + 1900;
    Month = Local->tm_mon + 1;
    DayOfWeek = Local->tm_wday;
    Day = Local->tm_mday;
    Hour = Local->tm_hour;
    Min = Local->tm_min;
    Sec = Local->tm_sec;
    MSec = static_cast<INT>(std::chrono::duration_cast<std::chrono::milliseconds>(Now.time_since_epoch()).count() % 1000);
}

// 0x0003b4da. counts from the first call, as the xbox's millisecond tick sum does.
DOUBLE appSeconds()
{
    static const std::chrono::steady_clock::time_point Start = std::chrono::steady_clock::now();
    return std::chrono::duration<DOUBLE>(std::chrono::steady_clock::now() - Start).count();
}

// the port counts nanoseconds of the same clock as appSeconds
DOUBLE GSecondsPerCycle = 1e-9;

DWORD appCycles()
{
    return static_cast<DWORD>(std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
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
