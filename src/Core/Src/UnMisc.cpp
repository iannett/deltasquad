#include <ctime>

#include "Core.h"
#include "FConfigCacheIni.h"

TCHAR   GErrorHist[4096];
FString GBuildLabel;
FString GOutDir(TEXT("Z:\\Out\\"));
UBOOL   GIsStarted;
UBOOL   GIsStrict;
UBOOL   GIsUTracing;
UBOOL   GIsRunning;
UBOOL   GIsRequestingExit;
QWORD   GFrameCounter;
UBOOL   GIsBenchmarking;
INT     GProfilingStackDepth;
FString GPlatformName;
FString GMachineCPU;

// a mod's directory, searched after the game's own when set
TCHAR GModDir[256];

// 0x00036f4b
UBOOL appLoadFileToString(FString& Result, const TCHAR* Filename, FFileManager* FileManager)
{
    FArchive* Reader = FileManager->CreateFileReader(Filename, 0, GLogDetail);
    if (!Reader)
        return 0;

    const INT Size = Reader->TotalSize();
    TArray<BYTE> Bytes;
    Bytes.Add(Size + 2);
    Reader->Serialize(Bytes.GetData(), Size);
    const UBOOL Success = Reader->Close();
    delete Reader;
    Bytes[Size] = 0;
    Bytes[Size + 1] = 0;

    TArray<TCHAR>& Chars = Result.GetCharArray();
    Chars.Empty();
    const BYTE* Src = Bytes.GetData();
    if (Size >= 2 && !(Size & 1) && ((Src[0] == 0xFF && Src[1] == 0xFE) || (Src[0] == 0xFE && Src[1] == 0xFF)))
    {
        // utf-16 in either byte order, after the byte order mark
        const UBOOL LittleEndian = Src[0] == 0xFF;
        Chars.Add(Size / 2);
        for (INT i = 0; i < Chars.Num() - 1; i++)
        {
            const BYTE* Unit = Src + 2 + i * 2;
            const _WORD Ch = LittleEndian ? (Unit[0] | (Unit[1] << 8)) : ((Unit[0] << 8) | Unit[1]);
            Chars[i] = static_cast<TCHAR>(Ch < 0x100 ? Ch : 0x7F);
        }
    }
    else
    {
        Chars.Add(Size + 1);
        for (INT i = 0; i < Chars.Num() - 1; i++)
            Chars[i] = static_cast<TCHAR>(Src[i]);
    }
    Chars[Chars.Num() - 1] = 0;
    return Success;
}

// 0x0003715f
UBOOL appSaveStringToFile(const FString& String, const TCHAR* Filename, FFileManager* FileManager)
{
    if (!String.Len())
        return 0;

    FArchive* Writer = FileManager->CreateFileWriter(Filename, 0, GLogDetail);
    if (!Writer)
        return 0;

    Writer->Serialize(const_cast<TCHAR*>(*String), String.Len());
    delete Writer;
    return 1;
}

// 0x00035816. opening the file for append updates its time stamp.
UBOOL appUpdateFileModTime(const TCHAR* Filename)
{
    FArchive* Ar = GFileManager->CreateFileWriter(Filename, FILEWRITE_Append, GLogDetail);
    if (!Ar)
        return 0;
    delete Ar;
    return 1;
}

// 0x00037888
UBOOL appFindPackageFile(const TCHAR* In, const FGuid* Guid, TCHAR* Out)
{
    // native code packages have no file
    const INT InLen = appStrlen(In);
    if (InLen > appStrlen(TEXT(".dll")) && !appStricmp(In + InLen - appStrlen(TEXT(".dll")), TEXT(".dll")))
        return 0;

    const INT LanguageCount = appStricmp(UObject::GetLanguage(), TEXT("int")) ? 1 : 0;
    const UBOOL HasExtension = appStrchr(In, '.') != nullptr;

    // a name with an extension may already be a path
    FString Temp;
    if (HasExtension)
    {
        if (GFileManager->FileSize(In) >= 0)
        {
            appStrcpy(Out, In);
            return 1;
        }
        Temp = FString(appBaseDir()) + In;
        if (GFileManager->FileSize(*Temp) >= 0)
        {
            appStrcpy(Out, *Temp);
            return 1;
        }
    }

    // each path in the game directory, then the mod directory, then the cache when a guid is given.
    // localized files (Name.<lang>_ext) are tried before plain ones.
    const INT PassCount = GModDir[0] ? 2 : 1;
    for (INT Pass = 0; Pass < PassCount; Pass++)
    {
        for (INT i = 0; i < GSys->Paths.Num() + (Guid ? 1 : 0); i++)
        {
            for (INT j = 0, Localized = LanguageCount; j < LanguageCount + 1; j++, Localized--)
            {
                FString Ext;
                Temp = Pass ? FString(GModDir) * TEXT("System") : FString(appBaseDir());
                if (i < GSys->Paths.Num())
                {
                    if (!GSys->bLinearLoad && appStrstr(*GSys->Paths[i], TEXT("Linear")))
                        continue;
                    Temp *= GSys->Paths[i];
                    const INT Wildcard = Temp.InStr(TEXT("*."));
                    if (Wildcard >= 0)
                    {
                        Ext = Temp.Mid(Wildcard + 2);
                        Temp = Temp.Left(Wildcard) + In;
                    }
                    else
                    {
                        Temp *= In;
                    }
                }
                else
                {
                    Ext = GSys->CacheExt;
                    Temp = GSys->CachePath * Guid->String();
                }

                if (HasExtension && GFileManager->FileSize(*Temp) >= 0)
                {
                    appStrcpy(Out, *Temp);
                    if (i == GSys->Paths.Num())
                        appUpdateFileModTime(Out);
                    return 1;
                }

                Temp += TEXT(".");
                if (Localized)
                    Temp += FString(UObject::GetLanguage()) + TEXT("_");
                Temp += Ext;
                if (GFileManager->FileSize(*Temp) >= 0)
                {
                    appStrcpy(Out, *Temp);
                    if (i == GSys->Paths.Num())
                        appUpdateFileModTime(Out);
                    return 1;
                }
            }
        }
    }
    return 0;
}

// 0x00037265
void appInit(const TCHAR* InCmdLine, FOutputDevice* InLog, FOutputDeviceError* InError, FFeedbackContext* InWarn,
    FConfigCache* (*ConfigFactory)(), UBOOL RequireConfig)
{
    appInitCRCTable();

    // GBitLengths[i] is the number of bits needed to hold i
    for (INT i = 0, Bits = 0, Count = 0; i <= 256; i++)
    {
        GBitLengths[i] = static_cast<BYTE>(Bits);
        if (i == 0 || ++Count >= (1 << (Bits - 1)))
        {
            Count = 0;
            Bits++;
        }
    }

    appStrcpy(GCmdLine, InCmdLine);
    appStrcpy(GErrorHist, TEXT("General protection fault!\r\n\r\nHistory: "));
    GLog = InLog;
    GError = InError;
    GWarn = InWarn;

    FName::StaticInit();
    appPlatformPreInit();
    GFileManager->Init(1);
    GFileManager->SetDefaultDirectory(appBaseDir());
    GFileManager->MakeDirectory(*GOutDir, 0);

    GBuildLabel = FString::Printf(TEXT("Build: %d"), ENGINE_BUILD);
    GLog->Logf(NAME_Init, TEXT("%s"), *GBuildLabel);
    GLog->Logf(NAME_Init, TEXT("Engine version: %i"), ENGINE_VERSION);
    GLog->Logf(NAME_Init, TEXT("Compiled: %s"), TEXT("Jan 25 2005"));
    GLog->Logf(NAME_Init, TEXT("Command line: %s"), GCmdLine);
    GLog->Logf(NAME_Init, TEXT("Base directory: %s"), appBaseDir());
    GLog->Logf(NAME_Init, TEXT("Character set: %s"), TEXT("ANSI"));

    GIsStrict = ParseParam(GCmdLine, TEXT("STRICT"));

    FString SystemIni(TEXT("System.ini"));
    FString UserIni(TEXT("User.ini"));
    Parse(GCmdLine, TEXT("INI="), SystemIni);
    Parse(GCmdLine, TEXT("USERINI="), UserIni);
    GIsUTracing = ParseParam(GCmdLine, TEXT("UTRACE"));

    GConfig = ConfigFactory();
    GConfig->Init(*SystemIni, *UserIni, RequireConfig);

    TCHAR Language[256];
    if (GConfig->GetString(TEXT("Engine.Engine"), TEXT("Language"), Language, ARRAY_COUNT(Language)) && appStricmp(Language, TEXT("multi")))
        UObject::SetLanguage(Language);

    if (!Parse(GCmdLine, TEXT("PROFILINGSTACKDEPTH="), GProfilingStackDepth))
        GProfilingStackDepth = 1000000;

    UObject::StaticInit();
    GMem.Init(65536);
    appPlatformInit();

    if (ParseParam(GCmdLine, TEXT("nolinear")))
    {
        GSys->bLinearSave = 0;
        GSys->bLinearLoad = 0;
    }
}

// 0x0003df75. the processor detection and logging that followed is xbox platform code.
void appPlatformInit()
{
    GSys = new USystem;
    GSys->AddToRoot();
    for (INT i = 0; i < GSys->Suppress.Num(); i++)
    {
        if (FNameEntry* Entry = FName::GetEntry(GSys->Suppress[i].GetIndex()))
            Entry->Flags |= NF_Suppress;
    }

    // recordings and benchmarks need the same random numbers every run
    if (ParseParam(appCmdLine(), TEXT("BENCHMARK")) || ParseParam(appCmdLine(), TEXT("RECORD")) || ParseParam(appCmdLine(), TEXT("REPLAY")))
    {
        appRandInit(1);
        appSRandInit(1);
    }
    else
    {
        appRandInit(static_cast<INT>(time(nullptr)));
    }

    GLog->Logf(NAME_Init, TEXT("Computer: %s"), appComputerName());
    GLog->Logf(NAME_Init, TEXT("User: %s"), appUserName());
}

// 0x0003515f
void appMD5Init(FMD5Context* Context)
{
    Context->count[0] = Context->count[1] = 0;
    Context->state[0] = 0x67452301;
    Context->state[1] = 0xefcdab89;
    Context->state[2] = 0x98badcfe;
    Context->state[3] = 0x10325476;
    memset(Context->buffer, 0, sizeof(Context->buffer));
}

// 0x00036ab1
void appMD5Update(FMD5Context* Context, const BYTE* Input, INT InputLen)
{
    INT Index = (Context->count[0] >> 3) & 0x3F;
    if ((Context->count[0] += static_cast<DWORD>(InputLen) << 3) < (static_cast<DWORD>(InputLen) << 3))
        Context->count[1]++;
    Context->count[1] += static_cast<DWORD>(InputLen) >> 29;

    const INT PartLen = 64 - Index;
    INT i = 0;
    if (InputLen >= PartLen)
    {
        memcpy(&Context->buffer[Index], Input, PartLen);
        appMD5Transform(Context->state, Context->buffer);
        for (i = PartLen; i + 63 < InputLen; i += 64)
            appMD5Transform(Context->state, &Input[i]);
        Index = 0;
    }
    memcpy(&Context->buffer[Index], &Input[i], InputLen - i);
}

// 0x00036b61
void appMD5Final(BYTE* Digest, FMD5Context* Context)
{
    static const BYTE Padding[64] = { 0x80 };

    BYTE Bits[8];
    appMD5Encode(Bits, Context->count, 8);

    const INT Index = (Context->count[0] >> 3) & 0x3f;
    const INT PadLen = Index < 56 ? 56 - Index : 120 - Index;
    appMD5Update(Context, Padding, PadLen);
    appMD5Update(Context, Bits, 8);
    appMD5Encode(Digest, Context->state, 16);
    memset(Context, 0, sizeof(*Context));
}

#define MD5_F(x, y, z) (((x) & (y)) | ((~x) & (z)))
#define MD5_G(x, y, z) (((x) & (z)) | ((y) & (~z)))
#define MD5_H(x, y, z) ((x) ^ (y) ^ (z))
#define MD5_I(x, y, z) ((y) ^ ((x) | (~z)))
#define MD5_ROTL(x, n) (((x) << (n)) | ((x) >> (32 - (n))))
#define MD5_STEP(f, a, b, c, d, x, s, ac) { (a) += f((b), (c), (d)) + (x) + static_cast<DWORD>(ac); (a) = MD5_ROTL((a), (s)) + (b); }

// 0x00035d31
void appMD5Transform(DWORD* State, const BYTE* Block)
{
    DWORD a = State[0], b = State[1], c = State[2], d = State[3], x[16];
    appMD5Decode(x, Block, 64);

    MD5_STEP(MD5_F, a, b, c, d, x[ 0],  7, 0xd76aa478); MD5_STEP(MD5_F, d, a, b, c, x[ 1], 12, 0xe8c7b756);
    MD5_STEP(MD5_F, c, d, a, b, x[ 2], 17, 0x242070db); MD5_STEP(MD5_F, b, c, d, a, x[ 3], 22, 0xc1bdceee);
    MD5_STEP(MD5_F, a, b, c, d, x[ 4],  7, 0xf57c0faf); MD5_STEP(MD5_F, d, a, b, c, x[ 5], 12, 0x4787c62a);
    MD5_STEP(MD5_F, c, d, a, b, x[ 6], 17, 0xa8304613); MD5_STEP(MD5_F, b, c, d, a, x[ 7], 22, 0xfd469501);
    MD5_STEP(MD5_F, a, b, c, d, x[ 8],  7, 0x698098d8); MD5_STEP(MD5_F, d, a, b, c, x[ 9], 12, 0x8b44f7af);
    MD5_STEP(MD5_F, c, d, a, b, x[10], 17, 0xffff5bb1); MD5_STEP(MD5_F, b, c, d, a, x[11], 22, 0x895cd7be);
    MD5_STEP(MD5_F, a, b, c, d, x[12],  7, 0x6b901122); MD5_STEP(MD5_F, d, a, b, c, x[13], 12, 0xfd987193);
    MD5_STEP(MD5_F, c, d, a, b, x[14], 17, 0xa679438e); MD5_STEP(MD5_F, b, c, d, a, x[15], 22, 0x49b40821);

    MD5_STEP(MD5_G, a, b, c, d, x[ 1],  5, 0xf61e2562); MD5_STEP(MD5_G, d, a, b, c, x[ 6],  9, 0xc040b340);
    MD5_STEP(MD5_G, c, d, a, b, x[11], 14, 0x265e5a51); MD5_STEP(MD5_G, b, c, d, a, x[ 0], 20, 0xe9b6c7aa);
    MD5_STEP(MD5_G, a, b, c, d, x[ 5],  5, 0xd62f105d); MD5_STEP(MD5_G, d, a, b, c, x[10],  9, 0x02441453);
    MD5_STEP(MD5_G, c, d, a, b, x[15], 14, 0xd8a1e681); MD5_STEP(MD5_G, b, c, d, a, x[ 4], 20, 0xe7d3fbc8);
    MD5_STEP(MD5_G, a, b, c, d, x[ 9],  5, 0x21e1cde6); MD5_STEP(MD5_G, d, a, b, c, x[14],  9, 0xc33707d6);
    MD5_STEP(MD5_G, c, d, a, b, x[ 3], 14, 0xf4d50d87); MD5_STEP(MD5_G, b, c, d, a, x[ 8], 20, 0x455a14ed);
    MD5_STEP(MD5_G, a, b, c, d, x[13],  5, 0xa9e3e905); MD5_STEP(MD5_G, d, a, b, c, x[ 2],  9, 0xfcefa3f8);
    MD5_STEP(MD5_G, c, d, a, b, x[ 7], 14, 0x676f02d9); MD5_STEP(MD5_G, b, c, d, a, x[12], 20, 0x8d2a4c8a);

    MD5_STEP(MD5_H, a, b, c, d, x[ 5],  4, 0xfffa3942); MD5_STEP(MD5_H, d, a, b, c, x[ 8], 11, 0x8771f681);
    MD5_STEP(MD5_H, c, d, a, b, x[11], 16, 0x6d9d6122); MD5_STEP(MD5_H, b, c, d, a, x[14], 23, 0xfde5380c);
    MD5_STEP(MD5_H, a, b, c, d, x[ 1],  4, 0xa4beea44); MD5_STEP(MD5_H, d, a, b, c, x[ 4], 11, 0x4bdecfa9);
    MD5_STEP(MD5_H, c, d, a, b, x[ 7], 16, 0xf6bb4b60); MD5_STEP(MD5_H, b, c, d, a, x[10], 23, 0xbebfbc70);
    MD5_STEP(MD5_H, a, b, c, d, x[13],  4, 0x289b7ec6); MD5_STEP(MD5_H, d, a, b, c, x[ 0], 11, 0xeaa127fa);
    MD5_STEP(MD5_H, c, d, a, b, x[ 3], 16, 0xd4ef3085); MD5_STEP(MD5_H, b, c, d, a, x[ 6], 23, 0x04881d05);
    MD5_STEP(MD5_H, a, b, c, d, x[ 9],  4, 0xd9d4d039); MD5_STEP(MD5_H, d, a, b, c, x[12], 11, 0xe6db99e5);
    MD5_STEP(MD5_H, c, d, a, b, x[15], 16, 0x1fa27cf8); MD5_STEP(MD5_H, b, c, d, a, x[ 2], 23, 0xc4ac5665);

    MD5_STEP(MD5_I, a, b, c, d, x[ 0],  6, 0xf4292244); MD5_STEP(MD5_I, d, a, b, c, x[ 7], 10, 0x432aff97);
    MD5_STEP(MD5_I, c, d, a, b, x[14], 15, 0xab9423a7); MD5_STEP(MD5_I, b, c, d, a, x[ 5], 21, 0xfc93a039);
    MD5_STEP(MD5_I, a, b, c, d, x[12],  6, 0x655b59c3); MD5_STEP(MD5_I, d, a, b, c, x[ 3], 10, 0x8f0ccc92);
    MD5_STEP(MD5_I, c, d, a, b, x[10], 15, 0xffeff47d); MD5_STEP(MD5_I, b, c, d, a, x[ 1], 21, 0x85845dd1);
    MD5_STEP(MD5_I, a, b, c, d, x[ 8],  6, 0x6fa87e4f); MD5_STEP(MD5_I, d, a, b, c, x[15], 10, 0xfe2ce6e0);
    MD5_STEP(MD5_I, c, d, a, b, x[ 6], 15, 0xa3014314); MD5_STEP(MD5_I, b, c, d, a, x[13], 21, 0x4e0811a1);
    MD5_STEP(MD5_I, a, b, c, d, x[ 4],  6, 0xf7537e82); MD5_STEP(MD5_I, d, a, b, c, x[11], 10, 0xbd3af235);
    MD5_STEP(MD5_I, c, d, a, b, x[ 2], 15, 0x2ad7d2bb); MD5_STEP(MD5_I, b, c, d, a, x[ 9], 21, 0xeb86d391);

    State[0] += a;
    State[1] += b;
    State[2] += c;
    State[3] += d;
    memset(x, 0, sizeof(x));
}

// 0x00035191
void appMD5Encode(BYTE* Output, const DWORD* Input, INT Len)
{
    for (INT i = 0, j = 0; j < Len; i++, j += 4)
    {
        Output[j]     = static_cast<BYTE>(Input[i] & 0xff);
        Output[j + 1] = static_cast<BYTE>((Input[i] >> 8) & 0xff);
        Output[j + 2] = static_cast<BYTE>((Input[i] >> 16) & 0xff);
        Output[j + 3] = static_cast<BYTE>((Input[i] >> 24) & 0xff);
    }
}

// 0x000351c6
void appMD5Decode(DWORD* Output, const BYTE* Input, INT Len)
{
    for (INT i = 0, j = 0; j < Len; i++, j += 4)
        Output[i] = static_cast<DWORD>(Input[j]) | (static_cast<DWORD>(Input[j + 1]) << 8) | (static_cast<DWORD>(Input[j + 2]) << 16) | (static_cast<DWORD>(Input[j + 3]) << 24);
}

// 0x000368ea. the xbox also frees a lock pool and a network lock that appInit makes there; the port's
// appInit does not make them yet.
void appPreExit()
{
    GLog->Logf(NAME_Exit, TEXT("Preparing to exit."));
    if (GConfig)
    {
        GConfig->Exit();
        delete GConfig;
        GConfig = nullptr;
    }
    UObject::StaticExit();
    GMem.Exit();
}

void appRequestExit(UBOOL Force)
{
    GLog->Logf(TEXT("appRequestExit(%i)"), Force);
    if (Force)
    {
        for (;;)
        {
        }
    }
    GIsRequestingExit = 1;
}

void appExit()
{
    GLog->Logf(NAME_Exit, TEXT("Exiting."));
    FName::StaticExit();
}

// 0x00037ffb
FString FFileStats::Describe() const
{
    return FString::Printf(TEXT("Opens %i, Seeks %i, Skips %i, Reads %i KB / %i, Serialize %i KB / %i"), Opens, Seeks, Skips,
        ReadBytes / 1024, Reads, SerializeBytes / 1024, Serializes);
}
