#pragma once

#include "UnConfig.h"

// where the profile's User.ini is saved; empty until a profile is chosen
extern FString GUserIniDir;
// where System.ini is saved; while it equals GInvalidIniDir, System.ini is never saved
extern FString GSystemIniDir;
extern FString GInvalidIniDir;

// one ini file: sections by case-insensitive name
class FConfigFile : public TMap<FString, FConfigSection>
{
public:
    explicit FConfigFile(UBOOL InUnknown13 = 0);

    void Read(const TCHAR* Filename);
    // marks the file, its sections and their values unchanged
    void ClearDirty();
    // saves to the ini directory. with CommentUnchanged, unchanged values in Section (or every section) are written commented out.
    UBOOL Write(const TCHAR* Filename, UBOOL CommentUnchanged, const TCHAR* Section);
    // Add keeps existing values of Key, for array entries written as +Key=Value
    void SetString(FConfigSection* Section, const TCHAR* Key, const TCHAR* Value, UBOOL Add);

    BYTE Dirty;
    BYTE NoSave;
    BYTE QuoteValues;
    // set when the file was missing and not created on request; nothing reads it in Core
    BYTE Unknown13;
};

// the ini cache the xbox uses
class FConfigCacheIni : public FConfigCache, public TMap<FString, FConfigFile>
{
public:
    UBOOL GetBool(const TCHAR* Section, const TCHAR* Key, UBOOL& Value, const TCHAR* Filename) override;
    UBOOL GetInt(const TCHAR* Section, const TCHAR* Key, INT& Value, const TCHAR* Filename) override;
    UBOOL GetFloat(const TCHAR* Section, const TCHAR* Key, FLOAT& Value, const TCHAR* Filename) override;
    UBOOL GetString(const TCHAR* Section, const TCHAR* Key, FString& Value, const TCHAR* Filename) override;
    UBOOL GetString(const TCHAR* Section, const TCHAR* Key, TCHAR* Value, INT Size, const TCHAR* Filename) override;
    const TCHAR* GetStr(const TCHAR* Section, const TCHAR* Key, const TCHAR* Filename) override;
    UBOOL GetSection(const TCHAR* Section, TCHAR* Result, INT Size, const TCHAR* Filename) override;
    FConfigSection* GetSectionPrivate(const TCHAR* Section, UBOOL Force, UBOOL Const, const TCHAR* Filename) override;
    void EmptySection(const TCHAR* Section, const TCHAR* Filename) override;
    void SetBool(const TCHAR* Section, const TCHAR* Key, UBOOL Value, const TCHAR* Filename) override;
    void SetInt(const TCHAR* Section, const TCHAR* Key, INT Value, const TCHAR* Filename) override;
    void SetFloat(const TCHAR* Section, const TCHAR* Key, FLOAT Value, const TCHAR* Filename) override;
    void SetString(const TCHAR* Section, const TCHAR* Key, const TCHAR* Value, const TCHAR* Filename) override;
    void Flush(UBOOL Read, const TCHAR* Filename, const TCHAR* Section) override;
    void UnloadFile(const TCHAR* Filename) override;
    void UnloadLocalizedFiles(const TCHAR* Filename) override;
    void Detach(const TCHAR* Filename) override;
    void Init(const TCHAR* InSystem, const TCHAR* InUser, UBOOL RequireConfig) override;
    void Exit() override;
    void Dump(FOutputDevice& Ar) override;
    void Serialize(FArchive& Ar) override;

    // the cached file, read on first use. System.ini and User.ini stand for the files Init named.
    FConfigFile* Find(const TCHAR* InFilename, UBOOL CreateIfNotFound);

    FString SystemIni;
    FString UserIni;

private:
    FConfigValue* FindValue(const TCHAR* Section, const TCHAR* Key, const TCHAR* Filename);
};

// 0x00014acd. what appInit is given to create GConfig.
FConfigCache* CreateConfigCacheIni();
