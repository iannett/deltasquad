#include <cstdio>
#include <cstdlib>

#include "Karma.h"

INT MeInfoLevel;
INT MeWarningLevel;
INT MeDebugLevel;

namespace
{
    // 0x00186a50. the xbox prints through the debugger; the port writes to stderr.
    void DefaultOutputLine(INT Level, const char* Text)
    {
        fputs(Text, stderr);
        fputs("\n", stderr);
    }

    // 0x00186a70
    void DefaultOutput(INT Level, const char* Text)
    {
        fputs(Text, stderr);
    }

    // formats "<Kind>{level}: <message>" and passes it to Output
    void FormatMessage(MeOutputFn Output, const char* Kind, INT Level, const char* Fmt, va_list Args)
    {
        char Format[5000];
        char Text[5000];
        snprintf(Format, sizeof(Format), Kind, Level, Fmt);
        vsnprintf(Text, sizeof(Text), Format, Args);
        Output(Level, Text);
    }

    // 0x00186a80
    void DefaultInfo(INT Level, const char* Fmt, va_list Args)
    {
        if (Level <= MeInfoLevel)
            FormatMessage(MeMessages.InfoOutput, "MeInfo{%d}: %s", Level, Fmt, Args);
    }

    // 0x00186b00
    void DefaultLog(INT Level, const char* Fmt, va_list Args)
    {
        if (Level <= MeInfoLevel)
            FormatMessage(MeMessages.LogOutput, "MeLog{%d}: %s", Level, Fmt, Args);
    }

    // 0x00186b80
    void DefaultWarning(INT Level, const char* Fmt, va_list Args)
    {
        if (Level <= MeWarningLevel)
            FormatMessage(MeMessages.WarningOutput, "MeWarning{%d}: %s", Level, Fmt, Args);
    }

    // 0x00186c00
    void DefaultFatal(INT Level, const char* Fmt, va_list Args)
    {
        FormatMessage(MeMessages.FatalOutput, "MeFatalError{%d}: %s", Level, Fmt, Args);
        exit(1);
    }

    // 0x00186c70
    void DefaultDebug(INT Level, const char* Fmt, va_list Args)
    {
        if (Level <= MeDebugLevel)
            FormatMessage(MeMessages.DebugOutput, "MeDebug{%d}: %s", Level, Fmt, Args);
    }
}

MeMessageHandlers MeMessages = { DefaultOutputLine, DefaultOutputLine, DefaultOutputLine, DefaultOutput, DefaultOutput,
    DefaultInfo, DefaultLog, DefaultWarning, DefaultFatal, DefaultDebug, 1, 1 };

// 0x00186ce0
MeOutputFn MeSetInfoOutput(MeOutputFn Fn)
{
    MeOutputFn Old = MeMessages.InfoOutput;
    MeMessages.InfoOutput = Fn;
    return Old;
}

// 0x00186d00
MeOutputFn MeSetWarningOutput(MeOutputFn Fn)
{
    MeOutputFn Old = MeMessages.WarningOutput;
    MeMessages.WarningOutput = Fn;
    return Old;
}

// 0x00186d20
MeOutputFn MeSetFatalOutput(MeOutputFn Fn)
{
    MeOutputFn Old = MeMessages.FatalOutput;
    MeMessages.FatalOutput = Fn;
    return Old;
}

// 0x00186d40
MeOutputFn MeSetDebugOutput(MeOutputFn Fn)
{
    MeOutputFn Old = MeMessages.DebugOutput;
    MeMessages.DebugOutput = Fn;
    return Old;
}

// 0x00186d60
MeMessageFn MeSetInfoHandler(MeMessageFn Fn)
{
    MeMessageFn Old = MeMessages.Info;
    MeMessages.Info = Fn;
    return Old;
}

// 0x00186d80
MeMessageFn MeSetWarningHandler(MeMessageFn Fn)
{
    MeMessageFn Old = MeMessages.Warning;
    MeMessages.Warning = Fn;
    return Old;
}

// 0x00186da0
MeMessageFn MeSetFatalHandler(MeMessageFn Fn)
{
    MeMessageFn Old = MeMessages.Fatal;
    MeMessages.Fatal = Fn;
    return Old;
}

// 0x00186dc0
MeMessageFn MeSetDebugHandler(MeMessageFn Fn)
{
    MeMessageFn Old = MeMessages.Debug;
    MeMessages.Debug = Fn;
    return Old;
}

// 0x00186e00
void MeWarning(INT Level, const char* Fmt, ...)
{
    va_list Args;
    va_start(Args, Fmt);
    MeMessages.Warning(Level, Fmt, Args);
    va_end(Args);
}

// 0x00186e20
void MeFatalError(INT Level, const char* Fmt, ...)
{
    va_list Args;
    va_start(Args, Fmt);
    MeMessages.Fatal(Level, Fmt, Args);
    va_end(Args);
}
