#pragma once

#include "CoreTypes.h"

class FOutputDevice;
class FOutputDeviceError;
class FFeedbackContext;
class FConfigCache;
class FString;

// the build number and engine version the xbox build reports
enum { ENGINE_BUILD = 139, ENGINE_VERSION = 2226, ENGINE_MIN_NET_VERSION = 2000 };

extern TCHAR   GErrorHist[4096];
extern FString GBuildLabel;
extern FString GOutDir;
extern TCHAR   GModDir[256];
// set first thing in main; Localize only looks keys up once it is set
extern UBOOL   GIsStarted;
extern UBOOL   GIsStrict;
extern UBOOL   GIsUTracing;
extern INT     GProfilingStackDepth;
// "XBox" on the original
extern FString GPlatformName;
// the processor description appPlatformInit logs
extern FString GMachineCPU;

// starts Core: logging, files, config, names, objects and GSys
void appInit(const TCHAR* InCmdLine, FOutputDevice* InLog, FOutputDeviceError* InError, FFeedbackContext* InWarn,
    FConfigCache* (*ConfigFactory)(), UBOOL RequireConfig);
// logs "Exiting." and shuts the engine down
void appExit();
void appPlatformPreInit();
void appPlatformInit();

const TCHAR* appComputerName();
const TCHAR* appUserName();

// md5 message digest (RFC 1321)
struct FMD5Context
{
    DWORD state[4];
    DWORD count[2];
    BYTE  buffer[64];
};

void appMD5Init(FMD5Context* Context);
void appMD5Update(FMD5Context* Context, const BYTE* Input, INT InputLen);
void appMD5Final(BYTE* Digest, FMD5Context* Context);
void appMD5Transform(DWORD* State, const BYTE* Block);
void appMD5Encode(BYTE* Output, const DWORD* Input, INT Len);
void appMD5Decode(DWORD* Output, const BYTE* Input, INT Len);
