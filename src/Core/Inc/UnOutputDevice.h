#pragma once

#include "CoreTypes.h"
#include "UnString.h"

enum EName : INT;

// anything that text can be written to: the log, the error handler, a console
class FOutputDevice
{
public:
    virtual ~FOutputDevice() = default;
    virtual void Serialize(const TCHAR* Text, EName Event) = 0;

    void Log(const TCHAR* Text);
    void Logf(const TCHAR* Fmt, ...);
    void Logf(EName Event, const TCHAR* Fmt, ...);
};

// anything that handles console commands
class FExec
{
public:
    virtual UBOOL Exec(const TCHAR* Cmd, FOutputDevice& Ar) = 0;
};

// the error device never returns from Serialize
class FOutputDeviceError : public FOutputDevice
{
};

// the warning device, which also reports progress of long tasks such as loading
class FFeedbackContext : public FOutputDevice
{
public:
    virtual void BeginSlowTask(const TCHAR* Task, UBOOL StatusWindow) {}
    virtual void EndSlowTask() {}
    virtual UBOOL StatusUpdatef(INT Numerator, INT Denominator, const TCHAR* Fmt, ...) { return 1; }
    // xbox slot 0x1c. narrows the part of the loading bar that the following updates fill; a negative Start
    // continues from the current end. its real name is not known.
    virtual void SetProgressRange(FLOAT Start, FLOAT End) {}
};

// collects everything written to it in a string. ctor 0x000af74e, Serialize 0x000af96e
class FStringOutputDevice : public FString, public FOutputDevice
{
public:
    FStringOutputDevice(const TCHAR* InStr = TEXT("")) : FString(InStr) {}
    void Serialize(const TCHAR* Text, EName Event) override { *this += Text; }
};

extern FOutputDevice*      GLog;
extern FOutputDeviceError* GError;
extern FFeedbackContext*   GWarn;
// a second log pointer used for detail messages (destroying, renaming). starts as the log; its other uses are unknown.
extern FOutputDevice*      GLogDetail;
extern UBOOL               GIsCriticalError;

#define appErrorf(...) GError->Logf(__VA_ARGS__)
