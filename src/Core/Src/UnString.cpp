#include <initializer_list>
#include <cstdlib>
#include <climits>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <strings.h>

#include "Core.h"

DWORD GCRCTable[256];

// appInit, 0x00037265 (crc table setup only)
void appInitCRCTable()
{
    for (DWORD Index = 0; Index < 256; Index++)
    {
        DWORD Crc = Index << 24;
        for (INT Bit = 0; Bit < 8; Bit++)
            Crc = (Crc & 0x80000000) ? (Crc << 1) ^ 0x04C11DB7 : Crc << 1;
        GCRCTable[Index] = Crc;
    }
}

TCHAR appToUpper(TCHAR Ch)
{
    const BYTE C = static_cast<BYTE>(Ch);
    const UBOOL Lower = (C >= 'a' && C <= 'z')
        || (C >= 0xE0 && C <= 0xEF)
        || (C >= 0xF1 && C <= 0xF6)
        || (C >= 0xF8 && C <= 0xFD);
    if (Lower)
        return static_cast<TCHAR>(C - 0x20);

    // š and ž sit 0x10 above their capitals in latin-1 / cp1252
    if (C == 0x9A || C == 0x9E)
        return static_cast<TCHAR>(C - 0x10);

    // the original maps capital thorn to lowercase here, kept so hashes stay the same
    if (C == 0xDE)
        return static_cast<TCHAR>(0xFE);

    return Ch;
}

DWORD appStrihash(const TCHAR* Str)
{
    DWORD Hash = 0;
    for (; *Str; Str++)
        Hash = (Hash >> 8) ^ GCRCTable[(static_cast<BYTE>(appToUpper(*Str)) ^ Hash) & 0xFF];
    return Hash;
}

INT appStricmp(const TCHAR* A, const TCHAR* B)
{
    return strcasecmp(A, B);
}

INT appStrcmp(const TCHAR* A, const TCHAR* B)
{
    return strcmp(A, B);
}

INT appStrlen(const TCHAR* Str)
{
    return static_cast<INT>(strlen(Str));
}

TCHAR* appStrcpy(TCHAR* Dest, const TCHAR* Src)
{
    return strcpy(Dest, Src);
}

// 0x0003aced
TCHAR* appStrcat(TCHAR* Dest, const TCHAR* Src)
{
    return strcat(Dest, Src);
}

// 0x0003ace3
const TCHAR* appStrchr(const TCHAR* String, INT Ch)
{
    return strchr(String, Ch);
}

// 0x0003ad74
INT appAtoi(const TCHAR* Str)
{
    return atoi(Str);
}

// 0x0003ad79
FLOAT appAtof(const TCHAR* Str)
{
    return static_cast<FLOAT>(atof(Str));
}

// 0x0003adaa
INT appStrnicmp(const TCHAR* A, const TCHAR* B, INT Count)
{
    return strncasecmp(A, B, Count);
}

// 0x0003acd9
const TCHAR* appStrstr(const TCHAR* String, const TCHAR* Find)
{
    return strstr(String, Find);
}

TCHAR* appStrstr(TCHAR* String, const TCHAR* Find)
{
    return strstr(String, Find);
}

// 0x0003ac92
TCHAR* appStrncpy(TCHAR* Dest, const TCHAR* Src, INT MaxLen)
{
    strncpy(Dest, Src, MaxLen);
    Dest[MaxLen - 1] = 0;
    return Dest;
}

// 0x0003aed4
INT appSprintf(TCHAR* Dest, const TCHAR* Fmt, ...)
{
    va_list Args;
    va_start(Args, Fmt);
    const INT Result = vsnprintf(Dest, INT_MAX, Fmt, Args);
    va_end(Args);
    return Result;
}

FString& FString::operator=(const TCHAR* In)
{
    const INT Length = In && *In ? appStrlen(In) + 1 : 0;
    Data.Empty(Length);
    if (Length)
    {
        Data.Add(Length);
        memcpy(Data.GetData(), In, Length);
    }
    return *this;
}

// 0x00011449
FString::FString(INT Count, const TCHAR* In)
{
    if (Count)
    {
        Data.Add(Count + 1);
        appStrncpy(Data.GetData(), In, Count + 1);
    }
}

// 0x00036a5a
FString FString::Printf(const TCHAR* Fmt, ...)
{
    TCHAR Buffer[4096];
    va_list Args;
    va_start(Args, Fmt);
    vsnprintf(Buffer, sizeof(Buffer), Fmt, Args);
    va_end(Args);
    return FString(Buffer);
}

// 0x00011282
FString& FString::operator+=(const TCHAR* Str)
{
    if (*Str)
    {
        if (Data.Num())
        {
            const INT Index = Data.Num() - 1;
            Data.Add(appStrlen(Str));
            appStrcpy(&Data[Index], Str);
        }
        else
        {
            *this = Str;
        }
    }
    return *this;
}

// 0x00036cc9
FString& FString::operator*=(const TCHAR* Str)
{
    if (*Str == '/' || *Str == '\\' || appStrchr(Str, ':'))
        return *this = Str;

    for (INT i = 0; i < Len(); i++)
    {
        if (Data[i] == '/' || Data[i] == '\\')
            Data[i] = '\\';
    }
    if (Data.Num() > 1 && Data[Data.Num() - 2] != '\\')
        *this += TEXT("\\");

    // each leading ..\ drops the last directory of this path
    while (!appStrnicmp(Str, TEXT("../"), 3) || !appStrnicmp(Str, TEXT("..\\"), 3))
    {
        Str += 3;
        for (INT i = Len() - 2; i >= 0; i--)
        {
            if (Data[i] == '\\')
            {
                Data[i + 1] = 0;
                Data.Remove(i + 2, Data.Num() - (i + 2));
                break;
            }
        }
    }

    const INT Start = Len();
    *this += Str;
    for (INT i = Start; i < Len(); i++)
    {
        if (Data[i] == '/')
            Data[i] = '\\';
    }
    return *this;
}

// 0x00035cb2
INT FString::InStr(const TCHAR* SubStr, UBOOL Right) const
{
    if (!Right)
    {
        const TCHAR* Found = appStrstr(**this, SubStr);
        return Found ? static_cast<INT>(Found - **this) : -1;
    }
    for (INT i = Len() - 1; i >= 0; i--)
    {
        INT j = 0;
        while (SubStr[j] && Data[i + j] == SubStr[j])
            j++;
        if (!SubStr[j])
            return i;
    }
    return -1;
}

// 0x00011549
FString FString::Left(INT Count) const
{
    return FString(Count < 0 ? 0 : Count < Len() ? Count : Len(), **this);
}

// 0x0001158c
FString FString::Right(INT Count) const
{
    return FString(**this + Len() - (Count < 0 ? 0 : Count < Len() ? Count : Len()));
}

// 0x0003768d
FString FString::Caps() const
{
    FString New(**this);
    for (INT i = 0; i < Data.Num(); i++)
        New.Data[i] = appToUpper(New.Data[i]);
    return New;
}

// 0x000371fe
FString FString::Mid(INT Start, INT Count) const
{
    // unsigned clamps, as in the engine
    DWORD First = static_cast<DWORD>(Start);
    DWORD End = static_cast<DWORD>(Start) + static_cast<DWORD>(Count);
    const DWORD Length = static_cast<DWORD>(Len());
    if (First > Length)
        First = Length;
    if (End < First)
        End = First;
    else if (End > Length)
        End = Length;
    return FString(static_cast<INT>(End - First), **this + First);
}

// 0x00036e09
FArchive& operator<<(FArchive& Ar, FString& S)
{
    S.Data.CountBytes(Ar);
    if (!Ar.IsLoading())
    {
        INT SaveNum = S.Data.Num();
        Ar << AR_INDEX(SaveNum);
        Ar.Serialize(S.Data.GetData(), S.Data.Num());
        return Ar;
    }

    INT SaveNum;
    Ar << AR_INDEX(SaveNum);
    const INT MaxSize = Ar.GetMaxSerializeSize();
    if (MaxSize && MaxSize < (SaveNum < 0 ? -SaveNum : SaveNum))
    {
        Ar.ArIsError = true;
        Ar.ArIsCriticalError = true;
    }

    S.Data.Empty(SaveNum);
    S.Data.Add(SaveNum);
    Ar.Serialize(S.Data.GetData(), SaveNum);

    // a string holding only its terminator is stored as empty
    if (S.Data.Num() == 1)
        S.Data.Empty();
    return Ar;
}

TCHAR GCmdLine[4096];

// 0x00035a9d
const TCHAR* appCmdLine()
{
    return GCmdLine;
}

// 0x00034fab
const TCHAR* appStrfind(const TCHAR* Str, const TCHAR* Find)
{
    UBOOL Alnum = 0;
    TCHAR First = (*Find >= 'a' && *Find <= 'z') ? *Find - ('a' - 'A') : *Find;
    const INT Length = appStrlen(Find++) - 1;
    for (TCHAR Ch = *Str; Ch; Ch = *++Str)
    {
        if (Ch >= 'a' && Ch <= 'z')
            Ch -= 'a' - 'A';
        if (!Alnum && Ch == First && !appStrnicmp(Str + 1, Find, Length))
            return Str;
        Alnum = (Ch >= 'A' && Ch <= 'Z') || (Ch >= '0' && Ch <= '9');
    }
    return nullptr;
}

// 0x00035699. a quoted token may hold spaces and, with UseEscape, backslash escapes.
UBOOL ParseToken(const TCHAR*& Str, TCHAR* Result, INT MaxLen, UBOOL UseEscape)
{
    INT Len = 0;
    while (*Str == ' ' || *Str == '\t')
        Str++;

    if (*Str == '"')
    {
        Str++;
        while (*Str && *Str != '"' && Len < MaxLen - 1)
        {
            TCHAR Ch = *Str++;
            if (Ch == '\\' && UseEscape)
            {
                Ch = *Str++;
                if (!Ch)
                    break;
            }
            Result[Len++] = Ch;
        }
        if (*Str == '"')
            Str++;
    }
    else
    {
        for (; *Str && *Str != ' ' && *Str != '\t'; Str++)
        {
            if (Len < MaxLen - 1)
                Result[Len++] = *Str;
        }
    }
    Result[Len] = 0;
    return Len != 0;
}

// 0x00035660
void ParseNext(const TCHAR** Stream)
{
    for (;;)
    {
        while (**Stream == ' ' || **Stream == '\t' || **Stream == '\r' || **Stream == '\n')
            ++*Stream;
        if (**Stream != ';')
            return;
        // a comment runs to the end of the line
        while (**Stream != '\n' && **Stream != '\r' && *++*Stream)
        {
        }
    }
}

UBOOL ParseCommand(const TCHAR** Stream, const TCHAR* Match)
{
    while (**Stream == ' ' || **Stream == '\t')
        (*Stream)++;
    if (appStrnicmp(*Stream, Match, appStrlen(Match)) == 0)
    {
        *Stream += appStrlen(Match);
        // the match has to end the word
        if (!appIsAlnum(**Stream))
        {
            while (**Stream == ' ' || **Stream == '\t')
                (*Stream)++;
            return 1;
        }
        *Stream -= appStrlen(Match);
    }
    return 0;
}

// 0x000352e5
UBOOL ParseParam(const TCHAR* Stream, const TCHAR* Param)
{
    const TCHAR* Start = Stream;
    if (*Stream)
    {
        while ((Start = appStrfind(Start + 1, Param)) != nullptr)
        {
            if (Start > Stream && (Start[-1] == '-' || Start[-1] == '/'))
                return 1;
        }
    }
    return 0;
}

// 0x00035231
UBOOL Parse(const TCHAR* Stream, const TCHAR* Match, TCHAR* Value, INT MaxLen)
{
    const TCHAR* Found = appStrfind(Stream, Match);
    if (!Found)
        return 0;

    const TCHAR* Start = Found + appStrlen(Match);
    if (*Start == '"')
    {
        appStrncpy(Value, Start + 1, MaxLen);
        Value[MaxLen - 1] = 0;
        if (TCHAR* Temp = appStrstr(Value, TEXT("\"")))
            *Temp = 0;
    }
    else
    {
        appStrncpy(Value, Start, MaxLen);
        Value[MaxLen - 1] = 0;
        for (const TCHAR* Delimiter : { TEXT(" "), TEXT("\r"), TEXT("\n"), TEXT("\t"), TEXT(",") })
        {
            if (TCHAR* Temp = appStrstr(Value, Delimiter))
                *Temp = 0;
        }
    }
    return 1;
}

// 0x00036eaf
UBOOL Parse(const TCHAR* Stream, const TCHAR* Match, FString& Value)
{
    TCHAR Temp[4096] = TEXT("");
    if (!Parse(Stream, Match, Temp, ARRAY_COUNT(Temp)))
        return 0;
    Value = Temp;
    return 1;
}

// 0x00035450
UBOOL Parse(const TCHAR* Stream, const TCHAR* Match, INT& Value)
{
    const TCHAR* Found = appStrfind(Stream, Match);
    if (!Found)
        return 0;
    Value = appAtoi(Found + appStrlen(Match));
    return 1;
}

const TCHAR* appSpc(INT Num)
{
    static TCHAR Spaces[256];
    static INT OldNum = 0;
    if (Num != OldNum)
    {
        for (OldNum = 0; OldNum < Num; OldNum++)
            Spaces[OldNum] = ' ';
        Spaces[Num] = 0;
    }
    return Spaces;
}
