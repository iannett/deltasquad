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
    TCHAR CmdLine[4096] = TEXT("");
    for (int i = 1; i < argc; i++)
    {
        strncat(CmdLine, " ", sizeof(CmdLine) - strlen(CmdLine) - 1);
        strncat(CmdLine, argv[i], sizeof(CmdLine) - strlen(CmdLine) - 1);
    }

    GIsStarted = 1;
    GIsScriptable = 1;
    GFileManager = CreatePosixFileManager();
    TCHAR BaseDir[1024];
    if (Parse(CmdLine, TEXT("BASEDIR="), BaseDir, ARRAY_COUNT(BaseDir)))
        appSetBaseDir(BaseDir);

    appInit(CmdLine, GLog, GError, GWarn, CreateConfigCacheIni, 0);
    // the game's main (0x00015281) from here. SERVER runs without a client: no viewport, rendering or sound.
    GIsServer = 1;
    GIsClient = !ParseParam(appCmdLine(), TEXT("SERVER"));

    UClass* EngineClass = UObject::StaticLoadClass(UGameEngine::StaticClass(), nullptr, TEXT("ini:Engine.Engine.GameEngine"), nullptr, LOAD_NoFail, nullptr);
    GEngine = static_cast<UEngine*>(UObject::StaticConstructObject(EngineClass));
    GLog->Logf(TEXT("Initializing Engine"));
    GEngine->Init();

    UObject::StaticExit();
    return 0;
}
