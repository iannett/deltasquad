#include <cstdio>
#include <cstring>

#include "Core.h"
#include "FConfigCacheIni.h"

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

    GLog->Logf(TEXT("%i objects after startup"), GObjObjects.Num());

    UObject::StaticExit();
    return 0;
}
