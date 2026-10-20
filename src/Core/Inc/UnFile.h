#pragma once

#include "CoreTypes.h"
#include "UnString.h"

class FArchive;
class FOutputDevice;

// file reader and writer flags
enum EFileFlags
{
    FILEREAD_NoFail = 0x01,
    FILEWRITE_NoFail = 0x01,
    FILEWRITE_Append = 0x10,
};

// file activity counts the xbox file manager keeps, logged around level loads
struct FFileStats
{
    INT Opens;
    INT Seeks;
    INT Skips;
    INT Reads;
    INT ReadBytes;
    INT Serializes;
    INT SerializeBytes;

    // 0x00037ffb
    FString Describe() const;
};

// opens files as archives. each platform provides one; the xbox version is platform code the port replaces.
class FFileManager
{
public:
    virtual ~FFileManager() = default;
    virtual void Init(UBOOL Startup) {}
    virtual FArchive* CreateFileReader(const TCHAR* Filename, DWORD ReadFlags = 0, FOutputDevice* Error = nullptr) = 0;
    virtual FArchive* CreateFileWriter(const TCHAR* Filename, DWORD WriteFlags = 0, FOutputDevice* Error = nullptr) = 0;
    // size in bytes, or -1 if the file doesn't exist
    virtual INT FileSize(const TCHAR* Filename) = 0;
    // bytes free on the drive holding Path; the xbox asks before saving ini files
    virtual SQWORD GetDiskFreeSpace(const TCHAR* Path) = 0;
    virtual UBOOL MakeDirectory(const TCHAR* Path, UBOOL Tree = 0) = 0;
    virtual UBOOL SetDefaultDirectory(const TCHAR* Path) = 0;
    // xbox slot 0x2c
    virtual TArray<FString> FindFiles(const TCHAR* Wildcard, UBOOL Files, UBOOL Directories) = 0;
    // xbox slot 0x40. the port's file managers keep no counts, so these stay zero.
    virtual FFileStats GetFileStats(UBOOL Reset) { return FFileStats{}; }
};

extern FFileManager* GFileManager;

class FString;

// reads a whole text file. utf-16 files with a byte order mark are narrowed, with characters past 255 as 0x7f.
UBOOL appLoadFileToString(FString& Result, const TCHAR* Filename, FFileManager* FileManager = GFileManager);
// touches a file's modification time
UBOOL appUpdateFileModTime(const TCHAR* Filename);
UBOOL appSaveStringToFile(const FString& String, const TCHAR* Filename, FFileManager* FileManager = GFileManager);

// the file manager the port uses on desktop hosts: plain stdio, with the game's backslash paths converted
FFileManager* CreatePosixFileManager();
