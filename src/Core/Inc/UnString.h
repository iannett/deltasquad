#pragma once

#include "CoreTypes.h"
#include "UnTemplate.h"

extern DWORD GCRCTable[256];

void appInitCRCTable();

// uppercases one latin-1 character the way the engine does, including its quirks
TCHAR appToUpper(TCHAR Ch);

// case-insensitive crc of a string, used for name hashing
DWORD appStrihash(const TCHAR* Str);

INT appStricmp(const TCHAR* A, const TCHAR* B);
INT appStrcmp(const TCHAR* A, const TCHAR* B);
INT appStrlen(const TCHAR* Str);
TCHAR* appStrcpy(TCHAR* Dest, const TCHAR* Src);
const TCHAR* appStrchr(const TCHAR* String, INT Ch);
INT appAtoi(const TCHAR* Str);
FLOAT appAtof(const TCHAR* Str);
INT appStrnicmp(const TCHAR* A, const TCHAR* B, INT Count);
const TCHAR* appStrstr(const TCHAR* String, const TCHAR* Find);
TCHAR* appStrstr(TCHAR* String, const TCHAR* Find);
// case-insensitive search for Find starting at a word boundary
const TCHAR* appStrfind(const TCHAR* Str, const TCHAR* Find);

// copies at most MaxLen - 1 characters and always terminates
TCHAR* appStrncpy(TCHAR* Dest, const TCHAR* Src, INT MaxLen);

// sprintf into a buffer the caller guarantees is large enough, like the engine's appSprintf
INT appSprintf(TCHAR* Dest, const TCHAR* Fmt, ...);

// a dynamic string. stored with its terminator, so an empty string has no data at all.
class FString
{
public:
    FString() = default;
    explicit FString(ENoInit) : Data(E_NoInit) {}
    FString(const TCHAR* In) { *this = In; }
    FString(const FString& Other) { *this = *Other; }
    // the first Count characters of In
    FString(INT Count, const TCHAR* In);

    static FString Printf(const TCHAR* Fmt, ...);

    FString& operator=(const TCHAR* In);
    FString& operator=(const FString& Other) { return this == &Other ? *this : (*this = *Other); }

    INT Len() const { return Data.Num() ? Data.Num() - 1 : 0; }
    const TCHAR* operator*() const { return Data.Num() ? Data.GetData() : TEXT(""); }

    TArray<TCHAR>& GetCharArray() { return Data; }

    FString& operator+=(const TCHAR* Str);
    FString& operator+=(const FString& Str) { return *this += *Str; }
    FString operator+(const TCHAR* Str) const { return FString(*this) += Str; }
    FString operator+(const FString& Str) const { return *this + *Str; }
    // appends a path, turning slashes into backslashes and resolving leading ..\ against this one
    FString& operator*=(const TCHAR* Str);
    FString& operator*=(const FString& Str) { return *this *= *Str; }
    FString operator*(const TCHAR* Str) const { return FString(*this) *= Str; }

    // comparisons ignore case. 0x00011314, 0x00011337, 0x0001136e
    UBOOL operator==(const TCHAR* Other) const { return !appStricmp(**this, Other); }
    UBOOL operator==(const FString& Other) const { return !appStricmp(**this, *Other); }
    UBOOL operator!=(const TCHAR* Other) const { return appStricmp(**this, Other) != 0; }
    UBOOL operator!=(const FString& Other) const { return appStricmp(**this, *Other) != 0; }

    // index of SubStr, searching from the end when Right is set; -1 if missing
    INT InStr(const TCHAR* SubStr, UBOOL Right = 0) const;
    FString Left(INT Count) const;
    FString Mid(INT Start, INT Count = 0x7FFFFFFF) const;
    FString Right(INT Count) const;
    // uppercased with appToUpper
    FString Caps() const;

    friend class FArchive& operator<<(class FArchive& Ar, FString& S);

private:
    TArray<TCHAR> Data;
};

// an FString member left as the object system initialized it
class FStringNoInit : public FString
{
public:
    FStringNoInit() : FString(E_NoInit) {}
    FStringNoInit& operator=(const TCHAR* In) { FString::operator=(In); return *this; }
    FStringNoInit& operator=(const FString& In) { FString::operator=(In); return *this; }
};

inline DWORD GetTypeHash(const FString& S) { return appStrihash(*S); }

// the command line the game was started with
extern TCHAR GCmdLine[4096];
const TCHAR* appCmdLine();

// true if -Param or /Param appears in Stream
UBOOL ParseParam(const TCHAR* Stream, const TCHAR* Param);

// the value after Match in Stream, quoted or up to the next space, tab, comma or line break
UBOOL Parse(const TCHAR* Stream, const TCHAR* Match, TCHAR* Value, INT MaxLen);
UBOOL Parse(const TCHAR* Stream, const TCHAR* Match, FString& Value);
UBOOL Parse(const TCHAR* Stream, const TCHAR* Match, INT& Value);
// the next word, or quoted string, of Str; moves Str past it. false if there was none.
UBOOL ParseToken(const TCHAR*& Str, TCHAR* Result, INT MaxLen, UBOOL UseEscape);
// skips whitespace and ; comments
void ParseNext(const TCHAR** Stream);

// where the game's files live
const TCHAR* appBaseDir();
// port only: the directory appBaseDir returns, normally the game's System directory with a trailing separator
void appSetBaseDir(const TCHAR* Dir);

// seconds since some fixed point, for timing
DOUBLE appSeconds();

// with everything statically linked this just returns a non-null handle, as on the xbox
void* appGetDllHandle(const TCHAR* Filename);
