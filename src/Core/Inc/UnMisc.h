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
// 0x0045a740. set while the main loop runs.
extern UBOOL   GIsRunning;
// 0x0045a750. ends the main loop.
extern UBOOL   GIsRequestingExit;
// 0x0045a76c. time runs undilated while set; nothing on the xbox sets it.
extern UBOOL   GIsBenchmarking;
// 0x00430fc8. frames ticked by the engine.
extern QWORD   GFrameCounter;
// "XBox" on the original
extern FString GPlatformName;
// the processor description appPlatformInit logs
extern FString GMachineCPU;

// starts Core: logging, files, config, names, objects and GSys
void appInit(const TCHAR* InCmdLine, FOutputDevice* InLog, FOutputDeviceError* InError, FFeedbackContext* InWarn,
    FConfigCache* (*ConfigFactory)(), UBOOL RequireConfig);
// 0x00036897. saves and closes the config and shuts down the object system.
void appPreExit();
// logs "Exiting." and shuts the engine down
void appExit();
// 0x0003b4a0. asks the main loop to end. a forced exit hangs on the xbox.
void appRequestExit(UBOOL Force);
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
