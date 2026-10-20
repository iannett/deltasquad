#include "EngineClasses.h"
#include "UnEngineGlobals.h"

FMemStack           GEngineMem;
FMemCache           GCache;
UEngine*            GEngine;
UCubemapManager*    GCubemapManager;
FStatGraph*         GStatGraph;
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
