#include <cstdio>
#include <cstring>

#include "Core.h"
#include "FConfigCacheIni.h"
#include "EngineClasses.h"
#include "UnEngineGlobals.h"

// brings up what has been decompiled so far. grows into the real launcher.
// BASEDIR= names the game's System directory, with a trailing slash; the game starts there.
int main(int argc, char** argv)
{
    // BASEDIR= is for the port only and stays off the game's command line
    TCHAR CmdLine[4096] = TEXT("");
    const TCHAR* BaseDir = nullptr;
    for (int i = 1; i < argc; i++)
    {
        if (!strncmp(argv[i], "BASEDIR=", 8))
        {
            BaseDir = argv[i] + 8;
            continue;
        }
        strncat(CmdLine, " ", sizeof(CmdLine) - strlen(CmdLine) - 1);
        strncat(CmdLine, argv[i], sizeof(CmdLine) - strlen(CmdLine) - 1);
    }

    GIsStarted = 1;
    GIsScriptable = 1;
    GFileManager = CreatePosixFileManager();
    if (BaseDir)
        appSetBaseDir(BaseDir);

    appInit(CmdLine, GLog, GError, GWarn, CreateConfigCacheIni, 0);
    // the game's main (0x00015281) from here. SERVER runs without a client: no viewport, rendering or sound.
    GIsServer = 1;
    GIsClient = !ParseParam(appCmdLine(), TEXT("SERVER"));

    UClass* EngineClass = UObject::StaticLoadClass(UGameEngine::StaticClass(), nullptr, TEXT("ini:Engine.Engine.GameEngine"), nullptr, LOAD_NoFail, nullptr);
    GEngine = static_cast<UEngine*>(UObject::StaticConstructObject(EngineClass));
    GLog->Logf(TEXT("Initializing Engine"));
    GEngine->Init();
    GLog->Logf(TEXT("Starting Engine"));

    // 0x00014b7f, the main loop
    GLog->Logf(TEXT("Entering Loop"));
    GIsRunning = 1;
    DOUBLE OldTime = appSeconds();
    while (GIsRunning && !GIsRequestingExit)
    {
        const DOUBLE NewTime = appSeconds();
        GEngine->Tick(static_cast<FLOAT>(NewTime - OldTime));
        OldTime = NewTime;
    }
    GIsRunning = 0;

    GLog->Logf(TEXT("Exiting Loop"));
    appPreExit();
    appExit();
    GIsStarted = 0;
    return 0;
}
