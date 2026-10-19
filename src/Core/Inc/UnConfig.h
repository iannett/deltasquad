#pragma once

#include "UnName.h"
#include "UnString.h"

class FArchive;
class FOutputDevice;

// one value of an ini key. Dirty marks values changed since the file was read; only those are saved.
struct FConfigValue
{
    FString Text;
    BYTE    Dirty = 0;

    FConfigValue() = default;
    // 0x000115df
    FConfigValue(const TCHAR* InText) : Text(InText) {}
    const TCHAR* operator*() const { return *Text; }
};

// one [section] of an ini file. keys may repeat, as array entries do.
class FConfigSection : public TMultiMap<FName, FConfigValue>
{
public:
    BYTE Dirty = 0;
};

// the ini file cache. Filename is a file name with or without ".ini"; null means System.ini.
class FConfigCache
{
public:
    virtual UBOOL GetBool(const TCHAR* Section, const TCHAR* Key, UBOOL& Value, const TCHAR* Filename = nullptr) = 0;
    virtual UBOOL GetInt(const TCHAR* Section, const TCHAR* Key, INT& Value, const TCHAR* Filename = nullptr) = 0;
    virtual UBOOL GetFloat(const TCHAR* Section, const TCHAR* Key, FLOAT& Value, const TCHAR* Filename = nullptr) = 0;
    virtual UBOOL GetString(const TCHAR* Section, const TCHAR* Key, FString& Value, const TCHAR* Filename = nullptr) = 0;
    virtual UBOOL GetString(const TCHAR* Section, const TCHAR* Key, TCHAR* Value, INT Size, const TCHAR* Filename = nullptr) = 0;
    virtual const TCHAR* GetStr(const TCHAR* Section, const TCHAR* Key, const TCHAR* Filename = nullptr) = 0;
    // the section as Key=Value strings, each null terminated, with an empty string at the end
    virtual UBOOL GetSection(const TCHAR* Section, TCHAR* Result, INT Size, const TCHAR* Filename = nullptr) = 0;
    // Force creates the section if missing; without Const the section counts as changed
    virtual FConfigSection* GetSectionPrivate(const TCHAR* Section, UBOOL Force, UBOOL Const, const TCHAR* Filename = nullptr) = 0;
    virtual void EmptySection(const TCHAR* Section, const TCHAR* Filename = nullptr) = 0;
    virtual void SetBool(const TCHAR* Section, const TCHAR* Key, UBOOL Value, const TCHAR* Filename = nullptr) = 0;
    virtual void SetInt(const TCHAR* Section, const TCHAR* Key, INT Value, const TCHAR* Filename = nullptr) = 0;
    virtual void SetFloat(const TCHAR* Section, const TCHAR* Key, FLOAT Value, const TCHAR* Filename = nullptr) = 0;
    virtual void SetString(const TCHAR* Section, const TCHAR* Key, const TCHAR* Value, const TCHAR* Filename = nullptr) = 0;
    // saves changed files (one, or all when Filename is null); Read also drops them from the cache
    virtual void Flush(UBOOL Read, const TCHAR* Filename = nullptr, const TCHAR* Section = nullptr) = 0;
    virtual void UnloadFile(const TCHAR* Filename) = 0;
    // drops Filename.<language> and Filename.int from the cache
    virtual void UnloadLocalizedFiles(const TCHAR* Filename) = 0;
    // the file is never saved
    virtual void Detach(const TCHAR* Filename) = 0;
    virtual void Init(const TCHAR* InSystem, const TCHAR* InUser, UBOOL RequireConfig) = 0;
    virtual void Exit() = 0;
    virtual void Dump(FOutputDevice& Ar) = 0;
    virtual void Serialize(FArchive& Ar) = 0;
    virtual ~FConfigCache() = default;
};

extern FConfigCache* GConfig;
