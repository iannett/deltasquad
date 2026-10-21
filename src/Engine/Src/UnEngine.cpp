#include "EngineClasses.h"
#include "UnEngineGlobals.h"

FMemStack           GEngineMem;
FMemCache           GCache;
UEngine*            GEngine;
UCubemapManager*    GCubemapManager;
FStatGraph*         GStatGraph;
FRenderUnknown      GRenderUnknown;
FTempLineBatcher*   GTempLineBatcher;
UGlobalTempObjects* GGlobalTempObjects;
UBOOL               GIsLoadingLevel;
UBOOL               GUseFrontEnd;

// 0x000a3fe2
void UEngine::Init()
{
    FURL::StaticInit();
    GEngineMem.Init(65536);
    GCache.Init(CacheSizeMegs * 1024 * 1024, 4096);
    if (!GStatGraph)
        GStatGraph = new FStatGraph;
    if (!GTempLineBatcher)
        GTempLineBatcher = new FTempLineBatcher;
    Cylinder = new(GetTransientPackage()) UPrimitive;
    Cylinder->AddToRoot();
    GGlobalTempObjects = new(GetTransientPackage()) UGlobalTempObjects;
    GLog->Logf(NAME_Init, TEXT("Unreal engine initialized"));
}

UBOOL UEngine::Exec(const TCHAR* Cmd, FOutputDevice& Ar)
{
    if (GSys && GSys->Exec(Cmd, Ar))
        return 1;
    if (UObject::StaticExec(Cmd, Ar))
        return 1;
    if (GCache.Exec(Cmd, Ar))
        return 1;
    if (GExec && GExec->Exec(Cmd, Ar))
        return 1;
    // the client's commands
    if (Client)
        NOT_DECOMPILED(0x000a42b5);
    if (Audio && Audio->Exec(Cmd, Ar))
        return 1;
    if (GStatGraph && GStatGraph->Exec(Cmd, Ar))
        return 1;

    if (ParseCommand(&Cmd, TEXT("FLUSH")))
    {
        Flush(1);
        Ar.Log(TEXT("Flushed engine caches"));
    }
    else if (ParseCommand(&Cmd, TEXT("STAT")))
    {
        if (ParseCommand(&Cmd, TEXT("DEFAULT")) || ParseCommand(&Cmd, TEXT("RESET")))
        {
            GEngineStats.Unknown0 = 0;
            GEngineStats.Unknown4 = 0;
            GEngineStats.Unknown1 = 0;
            GEngineStats.Unknown2 = 0;
            GEngineStats.Unknown5 = 0;
            GEngineStats.Unknown3 = 0;
            GStats = FStats();
            GStats.Clear();
        }
        else if (ParseCommand(&Cmd, TEXT("AVG")))
            GStats.Unknown4 = appAtof(Cmd);
        else if (ParseCommand(&Cmd, TEXT("PEAK")))
            GStats.Unknown1 = !GStats.Unknown1;
        else if (ParseCommand(&Cmd, TEXT("HIST")))
            GStats.Unknown2 = !GStats.Unknown2;
        else if (ParseCommand(&Cmd, TEXT("FPS")))
            GEngineStats.Unknown2 = !GEngineStats.Unknown2;
        else if (ParseCommand(&Cmd, TEXT("PAWNS")))
            GEngineStats.Unknown3 = !GEngineStats.Unknown3;
        else if (ParseCommand(&Cmd, TEXT("MAIN")))
            GEngineStats.Unknown1 = !GEngineStats.Unknown1;
        else if (ParseCommand(&Cmd, TEXT("HIGH")))
            GEngineStats.Unknown4 = !GEngineStats.Unknown4;
        else if (ParseCommand(&Cmd, TEXT("MEM")))
            GEngineStats.Unknown0 = !GEngineStats.Unknown0;
        else if (ParseCommand(&Cmd, TEXT("NET")))
            GEngineStats.Unknown5 = !GEngineStats.Unknown5;
        else if (FStatRecord* Record = FStatRecord::Main().FindStat(Cmd, true))
            Record->DWORD20 = Record->DWORD20 ? 0 : ~0u;
    }
    else if (ParseCommand(&Cmd, TEXT("CRACKURL")))
    {
        FURL URL(nullptr, Cmd, TRAVEL_Absolute);
        if (URL.Valid)
        {
            Ar.Logf(TEXT("     Protocol: %s"), *URL.Protocol);
            Ar.Logf(TEXT("         Host: %s"), *URL.Host);
            Ar.Logf(TEXT("         Port: %i"), URL.Port);
            Ar.Logf(TEXT("          Map: %s"), *URL.Map);
            Ar.Logf(TEXT("   NumOptions: %i"), URL.Op.Num());
            for (INT i = 0; i < URL.Op.Num(); i++)
                Ar.Logf(TEXT("     Option %i: %s"), i, *URL.Op[i]);
            Ar.Logf(TEXT("       Portal: %s"), *URL.Portal);
            Ar.Logf(TEXT("       String: '%s'"), *URL.String());
        }
        else
        {
            Ar.Logf(TEXT("BAD URL"));
        }
    }
    else if (ParseCommand(&Cmd, TEXT("TVSAFEZONE")))
    {
        GEngineStats.Unknown6 = !GEngineStats.Unknown6;
    }
    else
    {
        return 0;
    }
    return 1;
}

void UEngine::Flush(UBOOL AllowPrecache)
{
    GCache.Flush();
    if (Client)
        NOT_DECOMPILED(0x000a3b14);
}

UBOOL FStatGraph::Exec(const TCHAR* Cmd, FOutputDevice& Ar)
{
    if (ParseCommand(&Cmd, TEXT("GRAPH")))
    {
        if (ParseCommand(&Cmd, TEXT("SHOW")))
            Unknown0 = !Unknown0;
        else if (ParseCommand(&Cmd, TEXT("KEY")))
            Unknown3C = !Unknown3C;
        else if (ParseCommand(&Cmd, TEXT("LOCKSCALE")))
            Unknown4 = !Unknown4;
        else if (ParseCommand(&Cmd, TEXT("RESCALE")))
            NOT_DECOMPILED(0x001982d0);
        else if (ParseCommand(&Cmd, TEXT("HIDE")))
            Unknown0 = 1;
        else if (ParseCommand(&Cmd, TEXT("UNHIDE")))
            Unknown0 = 0;
        else
            // sets the graph's options and adds or removes the named stat's line
            NOT_DECOMPILED(0x00199215);
        return 1;
    }
    if (ParseCommand(&Cmd, TEXT("BARGRAPH")))
    {
        if (ParseCommand(&Cmd, TEXT("SHOW")))
        {
            Unknown50 = !Unknown50;
            return 1;
        }
        if (ParseCommand(&Cmd, TEXT("HIDE")))
        {
            Unknown50 = 0;
            return 1;
        }
        if (ParseCommand(&Cmd, TEXT("UNHIDE")))
        {
            Unknown50 = 1;
            return 1;
        }
        if (ParseCommand(&Cmd, TEXT("ADDSTAT")))
        {
            NOT_DECOMPILED(0x0019946a);
            return 1;
        }
        if (ParseCommand(&Cmd, TEXT("REMOVESTAT")))
        {
            NOT_DECOMPILED(0x00199578);
            return 1;
        }
    }
    return 0;
}
