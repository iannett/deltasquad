#include <new>

#include "EngineClasses.h"
#include "UnEngineGlobals.h"

void KInitGameKarma();

FEngineStats GEngineStats;

// ---------------------------------------------------------------------------------------------- engine stats

// 0x00168b27
void FEngineMemStats::InitMemBudgets()
{
    static const INT Budgets[13] = { 0xc00000, 0x200000, 0x300000, 0x500000, 0x200000, 0x200000, 0x200000, 0x100000, 0x200000, 0x100000, 0x200000, 0, 0x200000 };
    static const INT Budgets2[8] = { 0x800000, 0x400000, 0x200000, 0, 0, 0x600000, 0x400000, 0 };
    TotalMemory = 0x4000000;
    for (INT i = 0; i < 13; i++)
        Unknown38[i] = { Budgets[i], 0, 0, 0 };
    for (INT i = 0; i < 8; i++)
        Unknown118[i] = { Budgets2[i], 0, 0, 0 };
    Unknown19C = { 0x100000, 0, 0, 0 };
    Unknown1AC = 0x100000;
    UpdateMemTotals();
}

// 0x00168a57
void FEngineMemStats::UpdateMemTotals()
{
    Unknown28 = { 0, 0, 0, 0 };
    for (INT i = 12; i >= 0; i--)
        Unknown28.Budget += Unknown38[i].Budget;
    Unknown108 = { 0, 0, 0, 0 };
    for (INT i = 7; i >= 0; i--)
        Unknown108.Budget += Unknown118[i].Budget;
    Unknown198 = TotalMemory - Unknown28.Budget - Unknown108.Budget;
    Unknown1B0 = Unknown198 - Unknown1AC - Unknown19C.Budget;
}

// ---------------------------------------------------------------------------------------------- feedback

namespace
{
    // the engine's warning device while it runs: passes everything on to the previous GWarn and moves the
    // loading bar during slow tasks. its real name is not known.
    class FEngineFeedbackContext : public FFeedbackContext
    {
    public:
        // 0x0011e6bd
        explicit FEngineFeedbackContext(UGameEngine* InEngine)
            : Engine(InEngine), Previous(GWarn), SlowTaskCount(0), LastUpdateTime(0.f), ProgressStart(0.f), ProgressEnd(1.f), ShowStatus(0)
        {
            GWarn = this;
        }

        // 0x0011e70b
        void Serialize(const TCHAR* Text, EName Event) override
        {
            Previous->Serialize(Text, Event);
        }

        // 0x0011f4eb
        void BeginSlowTask(const TCHAR* InTask, UBOOL StatusWindow) override
        {
            Previous->BeginSlowTask(InTask, StatusWindow);
            if (SlowTaskCount++ == 0)
            {
                ShowStatus = StatusWindow != 0;
                Task = InTask;
                Engine->LoadProgressFraction = 0.f;
                ProgressStart = 0.f;
                ProgressEnd = 1.f;
                if (StatusWindow)
                    Engine->UpdateProgress();
                LastUpdateTime = static_cast<FLOAT>(appSeconds());
            }
        }

        // 0x0011e712
        void EndSlowTask() override
        {
            Previous->EndSlowTask();
            if (--SlowTaskCount == 0)
            {
                Engine->LoadProgressFraction = 1.f;
                ProgressStart = 0.f;
                ProgressEnd = 1.f;
                if (ShowStatus)
                    Engine->UpdateProgress();
            }
        }

        // 0x0011e75e. updates at most ten times a second and passes on the task name, not the message.
        UBOOL StatusUpdatef(INT Numerator, INT Denominator, const TCHAR* Fmt, ...) override
        {
            if (appSeconds() <= LastUpdateTime + 0.1f)
                return 0;
            SetProgress(Denominator ? static_cast<FLOAT>(Numerator) / static_cast<FLOAT>(Denominator) : 0.f);
            return Previous->StatusUpdatef(Numerator, Denominator, *Task);
        }

        // 0x0011e818
        void SetProgressRange(FLOAT Start, FLOAT End) override
        {
            if (SlowTaskCount != 1)
                return;
            if (Start < 0.f)
            {
                ProgressStart = ProgressEnd;
                ProgressEnd = (1.f - ProgressEnd) * End + ProgressEnd;
            }
            else
            {
                ProgressStart = Start;
                ProgressEnd = End;
            }
            if (appSeconds() > LastUpdateTime + 0.1f)
                SetProgress(0.f);
        }

    private:
        // 0x0011e7d4
        void SetProgress(FLOAT Fraction)
        {
            const FLOAT Progress = (ProgressEnd - ProgressStart) * Fraction + ProgressStart;
            if (Progress > Engine->LoadProgressFraction)
            {
                Engine->LoadProgressFraction = Progress;
                Engine->UpdateProgress();
                ShowStatus = 1;
                LastUpdateTime = static_cast<FLOAT>(appSeconds());
            }
        }

        UGameEngine*      Engine;
        FFeedbackContext* Previous;
        FString           Task;
        INT               SlowTaskCount;
        FLOAT             LastUpdateTime;
        FLOAT             ProgressStart;
        FLOAT             ProgressEnd;
        UBOOL             ShowStatus;
    };
}

// ---------------------------------------------------------------------------------------------- game engine

// 0x0012227e. without a client (the SERVER parameter) there is no viewport, sound or entry map.
void UGameEngine::Init()
{
    Super::Init();

    // the script of every native class not loaded from a package yet
    UObject::BeginLoad(TEXT("native classes"));
    for (INT i = 0; i < GObjObjects.Num(); i++)
    {
        UClass* Class = Cast<UClass>(GObjObjects[i]);
        if (Class && !Class->GetLinker() && !(Class->ClassFlags & CLASS_NoAutoLoad))
            StaticLoadObject(UClass::StaticClass(), Class->GetOuter(), Class->GetName(), nullptr, LOAD_NoThrow | LOAD_Unknown2, nullptr);
    }
    UObject::EndLoad();

    new FEngineFeedbackContext(this);
    GLevel = nullptr;
    if (GIsClient)
        NOT_DECOMPILED(0x0012233a);

    KInitGameKarma();
    bCheatProtection = !ParseParam(appCmdLine(), TEXT("nocheat"));
    if (!bCheatProtection)
        GWarn->Logf(TEXT("Cheat protection disabled"));
    if (Client)
        NOT_DECOMPILED(0x0012246e);

    FURL DefaultURL(nullptr);
    DefaultURL.LoadURLConfig(TEXT("DefaultPlayer"), TEXT("User"));
    LastURL = DefaultURL;

    // the level to start in: the ini's AutoLevel, or the first word of the command line
    FString AutoLevel;
    GConfig->GetString(TEXT("Engine.GameEngine"), TEXT("AutoLevel"), AutoLevel);
    TCHAR Parm[4096] = TEXT("");
    const TCHAR* Tmp = appCmdLine();
    if (ParseToken(Tmp, Parm, ARRAY_COUNT(Parm), 0) && (appStricmp(Parm, TEXT("SERVER")) != 0 || ParseToken(Tmp, Parm, ARRAY_COUNT(Parm), 0)) && Parm[0] != '-')
        AutoLevel = Parm;
    else
        Parm[0] = 0;
    if (Client)
        NOT_DECOMPILED(0x0012263f);

    GEntry = GLevel;
    static FName NAME_InitLevelLoadingInfo(TEXT("InitLevelLoadingInfo"), FNAME_Intrinsic);
    ProcessEvent(FindFunctionChecked(NAME_InitLevelLoadingInfo), nullptr);
    GLog->Logf(NAME_Init, TEXT("Game engine init time: %f seconds total, %f app"), appSeconds(), appSeconds());
    GEngineStats.Init();

    // with linear saving on, the entry map is saved for linear loading the first time
    const FString EntryPath = FString(TEXT("../LinearMaps/Entry.")) + *FURL::DefaultMapExt;
    if (GSys->bLinearSave && GFileManager->FileSize(*EntryPath) <= 0)
        NOT_DECOMPILED(0x001228c7);

    for (INT i = 0; i < GObjObjects.Num(); i++)
    {
        if (GObjObjects[i])
            GObjObjects[i]->SetFlags(RF_Marked);
    }
    if (Client)
        NOT_DECOMPILED(0x0012293f);

    // browse to the start level, then to the local map if that fails
    ULevel* Entry = GEntry;
    GEntry = GLevel;
    GLevel = Entry;
    FURL URL(&DefaultURL, *AutoLevel, TRAVEL_Partial);
    if (!URL.Valid)
        appErrorf(LocalizeError(TEXT("InvalidUrl"), TEXT("Engine")), *AutoLevel);
    FString Error;
    if (!Browse(URL, nullptr, Error))
    {
        if (FURL::DefaultLocalMap != AutoLevel && Browse(FURL(&DefaultURL, *FURL::DefaultLocalMap, TRAVEL_Partial), nullptr, Error))
            return;
        appErrorf(LocalizeError(TEXT("FailedBrowse"), TEXT("Engine")), *AutoLevel, *Error);
    }
}

// 0x00121d03
void UGameEngine::SetProgress(const TCHAR* Str1, const TCHAR* Str2, FLOAT Seconds)
{
    // only shows anything in a client's viewport
    if (Client)
        NOT_DECOMPILED(0x00121d0f);
}

// 0x00122c5b
INT UGameEngine::Browse(const FURL& InURL, const TMap<FString, FString>* TravelInfo, FString& Error)
{
    Error = TEXT("");
    FURL URL(InURL);
    if (Audio)
        NOT_DECOMPILED(0x00122c8b);

    // a .unreal file names the server to link to
    const TCHAR* Map = *URL.Map;
    const INT Tail = appStrlen(Map) - appStrlen(TEXT(".unreal"));
    const TCHAR* Extension = appStrstr(Map, TEXT(".unreal"));
    if (Extension && Extension - Map == Tail)
    {
        GLog->Logf(TEXT("Link: %s"), *URL.Map);
        FString NewURL;
        if (!GConfig->GetString(TEXT("Link"), TEXT("Server"), NewURL, *URL.Map))
        {
            Error = FString::Printf(LocalizeError(TEXT("InvalidLink"), TEXT("Engine")), *URL.Map);
            return 0;
        }
        URL = FURL(nullptr, *NewURL, TRAVEL_Absolute);
    }

    GLog->Logf(TEXT("Browse: %s %s"), *URL.String(), appTimestamp(1, 1));
    if (!URL.Valid)
    {
        Error = FString::Printf(LocalizeError(TEXT("InvalidUrl"), TEXT("Engine")), *URL.String());
        return 0;
    }
    if (URL.HasOption(TEXT("failed")) || URL.HasOption(TEXT("entry")))
        NOT_DECOMPILED(0x001232ca);
    if (URL.HasOption(TEXT("restart")))
        URL = LastURL;
    else if (URL.GetOption(TEXT("load="), nullptr))
        NOT_DECOMPILED(0x00122efa);

    if (URL.IsLocalInternal())
    {
        const INT Result = LoadMap(URL, nullptr, TravelInfo, Error, 1) != nullptr;
        // the xbox compares the DedicatedServer option with "true" and drops the result
        (void)(FString(URL.GetOption(TEXT("DedicatedServer="), TEXT(""))) == TEXT("true"));
        return Result;
    }
    if (URL.IsInternal() && GIsClient)
        NOT_DECOMPILED(0x00123095);
    if (URL.IsInternal())
    {
        Error = LocalizeError(TEXT("ServerOpen"), TEXT("Engine"));
        return 0;
    }
    // other protocols: the xbox build launches nothing
    URL.String();
    return 0;
}

// 0x004321d8. a client flag the tick clears when there is no viewport; what it means is not known yet.
static UBOOL GUnknown4321D8;

void UGameEngine::Tick(FLOAT DeltaSeconds)
{
    // the client's start of a tick: closed windows, demo replay, attract mode and auto pause
    if (Client)
        NOT_DECOMPILED(0x001266cb);

    if (DeltaSeconds < 0.f)
        GError->Logf(TEXT("Negative delta time!"));
    ElapsedInactiveTime += DeltaSeconds;

    // exits after TimeUntilExit seconds of play, counted once the level has run a second or while it is paused
    if (TimeUntilExit > 0.f && !GIsLoadingLevel)
    {
        ALevelInfo* Info = GLevel ? GLevel->GetLevelInfo() : nullptr;
        if (!Info || Info->TimeSeconds >= 1.f || GLevel->IsPaused())
        {
            ElapsedExitTime += DeltaSeconds;
            // the client can also exit when no movie plays, and stops a playing one first
            if (ExitDuringMovie && TimeUntilExit <= ElapsedExitTime)
            {
                GIsRequestingExit = 1;
                return;
            }
        }
        else
        {
            ElapsedExitTime = 0.f;
        }
    }
    // auto pause needs a client
    if (TimeUntilAutoPause > 0.f && !GIsLoadingLevel)
        ElapsedAutoPauseTime = 0.f;
    GUnknown4321D8 = 0;

    UObject::StaticTick();
    GCache.Tick();

    if (GLevel)
        GLevel->Tick(LEVELTICK_All, DeltaSeconds);

    // server travel once the level's switch countdown runs out
    ALevelInfo* Info = GLevel ? GLevel->GetLevelInfo() : nullptr;
    if (Info && Info->NextURL != TEXT(""))
    {
        Info->NextSwitchCountdown -= DeltaSeconds;
        if (Info->NextSwitchCountdown <= 0.f)
        {
            TMap<FString, FString> TravelInfo;
            if (Info->NextURL == TEXT("?RESTART"))
            {
                TravelInfo = GLevel->TravelInfo;
            }
            else if (Info->bNextItems)
            {
                TravelInfo = GLevel->TravelInfo;
                // each player's pawn exports its inventory under the player's name
                for (INT i = 0; i < GLevel->Actors.Num(); i++)
                {
                    APlayerController* PC = Cast<APlayerController>(GLevel->Actors[i]);
                    if (PC && PC->Player && PC->Pawn)
                        NOT_DECOMPILED(0x0011ea8e);
                }
            }
            const FString URL = Info->NextURL;
            Info->NextURL = TEXT("");
            GLog->Logf(TEXT("Server switch level: %s"), *URL);
            FString Error;
            Browse(FURL(&LastURL, *URL, TRAVEL_Relative), &TravelInfo, Error);
            return;
        }
    }

    // the viewport's own travel, the pending level and the client's tick
    if (GPendingLevel)
        NOT_DECOMPILED(0x001266cb);

    // the xbox also ticks an object at 0x00459600 here; nothing ever sets it
    GStats.Tick();
    CurrentTickRate = 1.f / (static_cast<FLOAT>(FStatRecord::Main().Pad1) * static_cast<FLOAT>(GSecondsPerCycle));

    GFrameCounter++;
}

UBOOL UGameEngine::Exec(const TCHAR* Cmd, FOutputDevice& Ar)
{
    const TCHAR* Str = Cmd;
    if (ParseCommand(&Str, TEXT("OPEN")))
    {
        FString Error;
        // the client travels its first viewport
        if (Client)
            NOT_DECOMPILED(0x001233e7);
        else if (!Browse(FURL(&LastURL, Str, TRAVEL_Partial), nullptr, Error) && Error != TEXT(""))
            Ar.Logf(TEXT("Open failed: %s"), *Error);
    }
    else if (ParseCommand(&Str, TEXT("START")))
    {
        FString Error;
        if (Client)
            NOT_DECOMPILED(0x00123497);
        else if (!Browse(FURL(&LastURL, Str, TRAVEL_Absolute), nullptr, Error) && Error != TEXT(""))
            Ar.Logf(TEXT("Start failed: %s"), *Error);
    }
    else if (ParseCommand(&Str, TEXT("SERVERTRAVEL")) && GIsServer && !GIsClient)
    {
        GLevel->GetLevelInfo()->eventServerTravel(FString(Str), 0);
    }
    else if (ParseCommand(&Str, TEXT("GAMELOG")))
    {
        // the rest goes to the client console's menu as HandleGameLog
        if (GLevel->IsDedicatedServer() && Client)
            NOT_DECOMPILED(0x001235bf);
    }
    else if (ParseCommand(&Str, TEXT("GET_HOST_DETAILS")))
    {
        if (GLevel->IsDedicatedServer())
        {
            ALevelInfo* Info = GLevel->GetLevelInfo();
            Ar.Logf(TEXT("%s - %s"), *Info->Game->GameName, *Info->Title);
        }
    }
    else if (ParseCommand(&Str, TEXT("GET_HOST_PLAYER_COUNT")))
    {
        if (GLevel->IsDedicatedServer())
            Ar.Logf(TEXT("%d"), GLevel->GetLevelInfo()->Game->NumPlayers);
    }
    else if (ParseCommand(&Str, TEXT("GET_HOST_PLAYER_MAX")))
    {
        if (GLevel->IsDedicatedServer())
            Ar.Logf(TEXT("%d"), GLevel->GetLevelInfo()->Game->MaxPlayers);
    }
    else if (GIsServer && !GIsClient && ParseCommand(&Str, TEXT("SAY")))
    {
        GLevel->GetLevelInfo()->Game->eventBroadcast(nullptr, FString(Str), NAME_None);
    }
    else if (ParseCommand(&Str, TEXT("DISCONNECT")))
    {
        FString Error;
        // the client goes back to the menu named by the defaults' DisconnectMenuClass and drops its connections
        if (Client)
            NOT_DECOMPILED(0x001237f7);
        else if (!Browse(FURL(&LastURL, TEXT("?closed?Menu=XInterfaceCommon.MenuConnectionFailed"), TRAVEL_Absolute), nullptr, Error) && Error != TEXT(""))
            Ar.Logf(TEXT("Disconnect failed: %s"), *Error);
    }
    else if (ParseCommand(&Str, TEXT("RECONNECT")))
    {
        FString Error;
        if (Client)
            NOT_DECOMPILED(0x00123a67);
        else if (!Browse(FURL(LastURL), nullptr, Error) && Error != TEXT(""))
            Ar.Logf(TEXT("Reconnect failed: %s"), *Error);
    }
    else if (ParseCommand(&Cmd, TEXT("EXIT")) || ParseCommand(&Cmd, TEXT("QUIT")))
    {
        // closes the control channels of the level's and the pending level's server connections first
        if (GLevel && GLevel->NetDriver && GLevel->NetDriver->ServerConnection)
            NOT_DECOMPILED(0x00124077);
        if (GPendingLevel)
            NOT_DECOMPILED(0x001240b8);
        Ar.Log(TEXT("Closing by request"));
        appRequestExit(0);
    }
    else if (ParseCommand(&Str, TEXT("GETCURRENTTICKRATE")))
    {
        Ar.Logf(TEXT("%f"), CurrentTickRate);
    }
    else if (ParseCommand(&Str, TEXT("GETMAXTICKRATE")))
    {
        Ar.Logf(TEXT("%f"), GetMaxTickRate());
    }
    else if (ParseCommand(&Str, TEXT("SAVEGAME")))
    {
        // saves when the first viewport's pawn is alive in a standalone game. the xbox reads the client unchecked.
        if (Client)
            NOT_DECOMPILED(0x00123bb5);
    }
    else if (ParseCommand(&Cmd, TEXT("CANCEL")))
    {
        static UBOOL InCancel = 0;
        if (!InCancel)
        {
            InCancel = 1;
            if (GPendingLevel)
                NOT_DECOMPILED(0x00123c5a);
            else
                SetProgress(TEXT(""), TEXT(""), 0.f);
            CancelPending();
            InCancel = 0;
        }
    }
    else if (ParseCommand(&Cmd, TEXT("SOUND_REBOOT")))
    {
        // recreates the audio subsystem on the same viewport
        if (Audio)
            NOT_DECOMPILED(0x00123cf0);
    }
    else if (GLevel && ParseCommand(&Cmd, TEXT("DUMPMEM")))
    {
        const FString Label(*Cmd ? Cmd : TEXT("MANUAL"));
        const FString MapName(GLevel->GetOutermostPackage()->GetName());
        // GEngineStats.SaveMem(MapName, Label)
        NOT_DECOMPILED(0x0016bd9d);
    }
    else if (GLevel && ParseCommand(&Cmd, TEXT("DUMPTIME")))
    {
        // the first viewport's location and view rotation
        FString Position;
        if (Client)
            NOT_DECOMPILED(0x00123de3);
        const FString Label(*Cmd ? Cmd : TEXT("MANUAL"));
        const FString MapName(GLevel->GetOutermostPackage()->GetName());
        // GStats.SaveStatsToFile(MapName, Label, Position)
        NOT_DECOMPILED(0x000606cb);
    }
    else
    {
        if (GLevel && GLevel->Exec(Cmd, Ar))
            return 1;
        if (GLevel && GLevel->GetLevelInfo()->Game && GLevel->GetLevelInfo()->Game->ScriptConsoleExec(Cmd, Ar, nullptr))
            return 1;
        // a client may not set properties of actors other than the game info, except config ones
        if (GLevel && GLevel->GetLevelInfo()->NetMode == 3)
        {
            const TCHAR* SetStr = Cmd;
            TCHAR ClassName[256];
            TCHAR PropertyName[256];
            UClass* Class;
            UProperty* Property;
            if (ParseCommand(&SetStr, TEXT("SET"))
                && ParseToken(SetStr, ClassName, ARRAY_COUNT(ClassName), 1)
                && (Class = Cast<UClass>(UObject::StaticFindObject(UClass::StaticClass(), ANY_PACKAGE, ClassName))) != nullptr
                && ParseToken(SetStr, PropertyName, ARRAY_COUNT(PropertyName), 1)
                && (Property = FindField<UProperty>(Class, PropertyName)) != nullptr
                && Class->IsChildOf(AActor::StaticClass())
                && !Class->IsChildOf(AGameInfo::StaticClass())
                && !(Property->PropertyFlags & (CPF_Config | CPF_GlobalConfig)))
            {
                GLog->Logf(TEXT("Not allowing set command: %u %u %u"), Class->IsChildOf(AActor::StaticClass()),
                    !Class->IsChildOf(AGameInfo::StaticClass()), 1);
                return 0;
            }
        }
        return UEngine::Exec(Cmd, Ar) != 0;
    }
    return 1;
}

void UGameEngine::CancelPending()
{
    // closes the pending level's server connection and deletes it
    if (GPendingLevel)
        NOT_DECOMPILED(0x0011d7c8);
}

// 0x0011eb07
FLOAT UGameEngine::GetMaxTickRate()
{
    // the net driver's lan rate applies with -lanplay
    static UBOOL LanPlay = ParseParam(appCmdLine(), TEXT("lanplay"));
    if (!GLevel)
        return 0.f;
    UNetDriver* DemoDriver = GLevel->DemoRecDriver;
    UNetDriver* NetDriver = GLevel->NetDriver;
    // a server recording a demo
    if (DemoDriver && !DemoDriver->ServerConnection && NetDriver && !GIsClient)
        return static_cast<FLOAT>(Clamp(LanPlay ? DemoDriver->LanServerMaxTickRate : DemoDriver->NetServerMaxTickRate, 20, 60));
    if (NetDriver && !GIsClient)
        return static_cast<FLOAT>(Clamp(LanPlay ? NetDriver->LanServerMaxTickRate : NetDriver->NetServerMaxTickRate, 10, 120));
    // a client capping its frame rate: the connection's speed over MoveRepSize, at most 90
    if (NetDriver && NetDriver->ServerConnection && GLevel->GetLevelInfo()->bCapFramerate)
        NOT_DECOMPILED(0x0011ebe6);
    return 0.f;
}

// 0x0011f1ac
void UGameEngine::UnloadCurrentMap()
{
    if (GLevel)
        NOT_DECOMPILED(0x0011f1b5);
    GLevel = GEntry;
}

namespace
{
    // wall clock laps for LoadMap's timing lines (0x0011d849). the xbox keeps a second double beside the time
    // that is never set, so its second lap value is garbage; the logs only print the first.
    struct FLoadTimer
    {
        DOUBLE Time = appSeconds();

        // 0x0011e88b
        FString Lap()
        {
            const DOUBLE Now = appSeconds();
            const DOUBLE Delta = Now - Time;
            Time = Now;
            return FString::Printf(TEXT("%.3f sec"), Delta);
        }
    };

    // written by LoadMap when the level's objects are moved to another package; nothing reads it yet
    UObject* Unknown47EE40;
}

// 0x00124114. the client's loading screen, demo and network travel parts are not decompiled yet.
ULevel* UGameEngine::LoadMap(const FURL& URL, UPendingLevel* Pending, const TMap<FString, FString>* TravelInfo, FString& Error, UBOOL Unknown)
{
    // the xbox logs its allocator's peak use here; the port's allocator keeps none
    GLog->Logf(TEXT("Pre load memory watermark: %d"), 0);
    if (GLevel)
        NOT_DECOMPILED(0x0012414d);

    FFileStats Stats = GFileManager->GetFileStats(1);
    if (Stats.Reads)
        GLog->Logf(TEXT("%s pre-load stats: %s"), *URL.Map, *Stats.Describe());
    FLoadTimer TotalTimer;
    if (Audio)
        NOT_DECOMPILED(0x00124289);
    if (Client)
        NOT_DECOMPILED(0x001242b6);

    const UBOOL bEntry = URL.Map == TEXT("Entry.ctm") && GEntry;
    GIsLoadingLevel = 1;
    if (Pending)
        NOT_DECOMPILED(0x001243be);

    // check every file the level needs exists before unloading the current one
    UObject::BeginLoad(TEXT("level verify"));
    UObject::StaticLoadObject(ULevel::StaticClass(), nullptr, TEXT("MyLevel"), *URL.Map, LOAD_Unknown2 | LOAD_Throw | LOAD_Verify, nullptr);
    UObject::EndLoad();
    if (Unknown && Client && !bEntry)
        NOT_DECOMPILED(0x001244c1);

    GWarn->BeginSlowTask(TEXT("Loading level"), 1);
    GLog->Logf(NAME_Log, TEXT("LoadMap: %s %s"), *URL.String(), appTimestamp(1, 1));
    Error = TEXT("");
    const INT SavedHubStackLevel = GLevel ? GLevel->GetLevelInfo()->HubStackLevel : 0;
    UnloadCurrentMap();
    GLog->Logf(TEXT("Pre load memory watermark: %d"), 0);

    GWarn->SetProgressRange(0.05f, 0.65f);
    FLoadTimer Timer;
    GLevel = Cast<ULevel>(UObject::StaticLoadObject(ULevel::StaticClass(), nullptr, TEXT("MyLevel"), *URL.Map, LOAD_NoFail, nullptr));
    GLog->Logf(TEXT("%s load time: %s"), *URL.Map, *Timer.Lap());

    if (GLevel->Unknown2C && GLevel->Unknown2C != GLevel->GetOuter())
    {
        // the level was saved inside another package: move its objects there
        UObject* Outer = GLevel->GetOuter();
        Unknown47EE40 = Outer;
        for (INT i = 0; i < GObjObjects.Num(); i++)
        {
            UObject* Object = GObjObjects[i];
            if (Object && Object->GetOuter() == Outer)
                Object->Rename(Object->GetName(), GLevel->Unknown2C);
        }
    }
    else if (GSys->bLinearSave && GEntry && GEntry != GLevel)
    {
        NOT_DECOMPILED(0x001244ea);
    }

    ALevelInfo* Info = GLevel->GetLevelInfo();
    Info->ComputerName = appComputerName();
    Info->DeterminePlatform();
    if (URL.HasOption(TEXT("load")))
        ;
    else if (URL.HasOption(TEXT("push")))
        Info->HubStackLevel = SavedHubStackLevel + 1;
    else if (URL.HasOption(TEXT("pop")))
        Info->HubStackLevel = Max(SavedHubStackLevel - 1, 0);
    else
        Info->HubStackLevel = URL.HasOption(TEXT("peer")) ? SavedHubStackLevel : 0;
    if (Pending)
        NOT_DECOMPILED(0x00124ad2);
    if (!URL.GetOption(TEXT("load"), nullptr))
        GLevel->URL = URL;
    Info->EngineVersion = FString::Printf(TEXT("%i"), ENGINE_VERSION);
    Info->MinNetVersion = FString::Printf(TEXT("%i"), ENGINE_MIN_NET_VERSION);
    GLevel->Engine = this;
    if (TravelInfo)
        GLevel->TravelInfo = *TravelInfo;

    for (INT i = 0; i < GObjObjects.Num(); i++)
    {
        UMaterial* Material = Cast<UMaterial>(GObjObjects[i]);
        if (Material)
            Material->ClearCachedPasses();
    }
    if (GLevel->Engine->GRenDev)
        NOT_DECOMPILED(0x00124b8a);

    // flag the entry level's actors that the new level doesn't own (object flag 0x400)
    if (GEntry && GLevel != GEntry)
    {
        UObject* Outer = GLevel->GetOuter();
        for (INT i = 0; i < GObjObjects.Num(); i++)
        {
            AActor* Actor = Cast<AActor>(GObjObjects[i]);
            if (Actor && Actor->IsIn(Outer))
                Actor->SetFlags(0x400);
        }
        for (INT i = 0; i < GLevel->Actors.Num(); i++)
            if (GLevel->Actors[i])
                GLevel->Actors[i]->ClearFlags(0x400);
    }
    if (GCubemapManager)
        NOT_DECOMPILED(0x000b1224);

    Timer = FLoadTimer();
    GWarn->SetProgressRange(-1.f, 0.2f);
    GLevel->SetActorCollision(1, 0);
    GLog->Logf(TEXT(" %s init actor coll time: %s"), *URL.Map, *Timer.Lap());

    // how many zone portals apart each pair of zones is, 255 for unreachable
    QWORD Reach[64];
    for (INT i = 0; i < 64; i++)
    {
        Reach[i] = GLevel->Model->Zones[i].Connectivity;
        for (INT j = 0; j < 64; j++)
            GLevel->ZoneDist[i][j] = i == j ? 0 : 255;
    }
    for (INT Dist = 1; Dist < 64; Dist++)
    {
        for (INT i = 0; i < 64; i++)
            for (INT j = 0; j < 64; j++)
                if (GLevel->ZoneDist[i][j] > Dist && (Reach[i] & (static_cast<QWORD>(1) << j)))
                    GLevel->ZoneDist[i][j] = static_cast<BYTE>(Dist);
        QWORD Next[64] = {};
        for (INT i = 0; i < 64; i++)
            for (INT j = 0; j < 64; j++)
                if (Reach[i] & (static_cast<QWORD>(1) << j))
                    Next[i] |= Reach[j];
        memcpy(Reach, Next, sizeof(Reach));
    }
    GLevel->UpdateTime(Info);

    TCHAR Options[1024] = TEXT("");
    TCHAR GameParam[256] = TEXT("");
    TCHAR MenuParam[256] = TEXT("");
    TCHAR MenuArgsParam[256] = TEXT("");
    FString GameError;
    for (INT i = 0; i < URL.Op.Num(); i++)
    {
        appStrcat(Options, TEXT("?"));
        appStrcat(Options, *URL.Op[i]);
        Parse(*URL.Op[i], TEXT("GAME="), GameParam, ARRAY_COUNT(GameParam));
        Parse(*URL.Op[i], TEXT("MENU="), MenuParam, ARRAY_COUNT(MenuParam));
        Parse(*URL.Op[i], TEXT("MENUARGS="), MenuArgsParam, ARRAY_COUNT(MenuArgsParam));
    }

    if (GLevel->IsServer() && !Info->Game)
    {
        UClass* GameClass = nullptr;
        if (GameParam[0])
            GameClass = UObject::StaticLoadClass(AGameInfo::StaticClass(), nullptr, GameParam, nullptr, LOAD_NoFail, nullptr);
        if (!GameClass && Info->DefaultGameType.Len() > 0)
            GameClass = UObject::StaticLoadClass(AGameInfo::StaticClass(), nullptr, *Info->DefaultGameType, nullptr, LOAD_NoFail, nullptr);
        if (!GameClass)
            GameClass = UObject::StaticLoadClass(AGameInfo::StaticClass(), nullptr,
                Client ? TEXT("ini:Engine.Engine.DefaultGame") : TEXT("ini:Engine.Engine.DefaultServerGame"), nullptr, LOAD_NoFail, nullptr);
        if (!GameClass)
            GameClass = AGameInfo::StaticClass();
        GLog->Logf(NAME_Log, TEXT("Game class is '%s'"), GameClass->GetName());
        Info->Game = Cast<AGameInfo>(GLevel->SpawnActor(GameClass));
    }

    if (!Client || URL.HasOption(TEXT("Listen")))
    {
        if (GPendingLevel)
        {
            delete GPendingLevel;
            GPendingLevel = nullptr;
        }
        FString ListenError;
        if (!GLevel->Listen(ListenError))
            GError->Logf(LocalizeError(TEXT("ServerListen"), TEXT("Engine")), *ListenError);
    }

    Info->DetailMode = 2;
    if (Client)
        NOT_DECOMPILED(0x00124f6a);
    if (GStatGraph)
        GStatGraph->Reset();
    GWarn->SetProgressRange(-1.f, 0.25f);
    GLevel->iFirstDynamicActor = 0;
    GLevel->Ticked = 1;

    if (!Info->bBegunPlay)
    {
        GLog->Logf(TEXT("Level is %s"), GLevel->GetFullName());
        if (!Info->bPathsRebuilt)
            GLog->Logf(TEXT("*** WARNING - PATHS MAY NOT BE VALID ***"));
        GLog->Logf(NAME_Log, TEXT("Bringing %s up for play (%i) %s..."), GLevel->GetFullName(), appRound(GetMaxTickRate()), appTimestamp(1, 1));
        GLevel->UnknownE0 = 0;
        Info->TimeSeconds = 0.f;
        Info->GetDefaultPhysicsVolume()->bNoDelete = 1;

        // a client keeps only the actors it simulates; the rest swap roles
        if (!GLevel->IsServer())
        {
            for (INT i = 0; i < GLevel->Actors.Num(); i++)
            {
                AActor* Actor = GLevel->Actors[i];
                if (!Actor)
                    continue;
                if (Actor->bStatic || Actor->bNoDelete)
                    Exchange(Actor->Role, Actor->RemoteRole);
                else
                    GLevel->DestroyActor(Actor);
            }
        }

        GWarn->StatusUpdatef(1, 10, TEXT(""));
        for (INT i = 0; i < GLevel->Actors.Num(); i++)
            if (GLevel->Actors[i])
                GLevel->Actors[i]->InitExecution();
        Info->bBegunPlay = 1;
        Info->bStartup = 1;
        if (Info->Game)
            Info->Game->eventInitGame(Options, GameError);

        GWarn->StatusUpdatef(2, 10, TEXT(""));
        for (INT i = 0; i < GLevel->Actors.Num(); i++)
            if (GLevel->Actors[i] && !GLevel->Actors[i]->bScriptInitialized)
                GLevel->Actors[i]->eventPreBeginPlay();
        GWarn->StatusUpdatef(3, 10, TEXT(""));
        for (INT i = 0; i < GLevel->Actors.Num(); i++)
            if (GLevel->Actors[i] && !GLevel->Actors[i]->bScriptInitialized)
                GLevel->Actors[i]->eventBeginPlay();
        GWarn->StatusUpdatef(4, 10, TEXT(""));
        for (INT i = 0; i < GLevel->Actors.Num(); i++)
            if (GLevel->Actors[i] && !GLevel->Actors[i]->bScriptInitialized)
                GLevel->Actors[i]->SetZone(1, 1);
        GWarn->StatusUpdatef(5, 10, TEXT(""));
        for (INT i = 0; i < GLevel->Actors.Num(); i++)
        {
            if (GLevel->Actors[i] && !GLevel->Actors[i]->bScriptInitialized)
            {
                GLevel->Actors[i]->eventPostBeginPlay();
                if (GLevel->Actors[i])
                    GLevel->Actors[i]->PostBeginPlay();
            }
        }
        GWarn->StatusUpdatef(6, 10, TEXT(""));
        for (INT i = 0; i < GLevel->Actors.Num(); i++)
            if (GLevel->Actors[i] && !GLevel->Actors[i]->bScriptInitialized)
                GLevel->Actors[i]->eventPostNetBeginPlay();
        GWarn->StatusUpdatef(7, 10, TEXT(""));
        for (INT i = 0; i < GLevel->Actors.Num(); i++)
            if (GLevel->Actors[i] && !GLevel->Actors[i]->bScriptInitialized)
                GLevel->Actors[i]->eventSetInitialState();

        // attach actors to the actor their AttachTag names, or base them on what is below
        GWarn->StatusUpdatef(8, 10, TEXT(""));
        for (INT i = 0; i < GLevel->Actors.Num(); i++)
        {
            AActor* Actor = GLevel->Actors[i];
            if (!Actor)
                continue;
            if (Actor->AttachTag != NAME_None)
            {
                for (INT j = 0; j < GLevel->Actors.Num(); j++)
                {
                    AActor* Other = GLevel->Actors[j];
                    if (Other && (Other->Tag == Actor->AttachTag || Other->GetFName() == Actor->AttachTag))
                    {
                        Actor->SetBase(Other, FVector(0.f, 0.f, 1.f), 0);
                        break;
                    }
                }
            }
            else if (Actor->bCollideWorld && Actor->bShouldBaseAtStartup && (Actor->Physics == 0 || Actor->Physics == 5)) // PHYS_None, PHYS_Rotating
            {
                Actor->FindBase();
            }
        }

        // karma actors only simulated on detailed or client machines lose their bodies here
        GWarn->StatusUpdatef(9, 10, TEXT(""));
        for (INT i = 0; i < GLevel->Actors.Num(); i++)
        {
            AActor* Actor = GLevel->Actors[i];
            if (!Actor || Actor->Physics != 13 /* PHYS_Karma */ || !Cast<UKarmaParams>(Actor->KParams))
                continue;
            UKarmaParams* KParams = Cast<UKarmaParams>(Actor->KParams);
            if (Info->PhysicsDetailLevel < 2 && KParams->bHighDetailOnly)
                NOT_DECOMPILED(0x00089546);
            if (Info->NetMode == 1 && (KParams->bHighDetailOnly || KParams->bClientOnly))
                NOT_DECOMPILED(0x00089546);
        }
        Info->bStartup = 0;
    }
    GLog->Logf(TEXT(" %s init BeginPlay time: %s %s"), *URL.Map, *Timer.Lap(), appTimestamp(1, 1));

    UPawnAudioTable::Reset();
    GWarn->SetProgressRange(-1.f, 0.25f);
    for (INT i = 0; i < GLevel->Actors.Num(); i++)
    {
        AActor* Actor = GLevel->Actors[i];
        if (!Actor)
            continue;
        Actor->LastRenderTime = 0.f;
        Actor->Touching.Empty();
        Actor->CurrentPhysicsVolume = Info->GetDefaultPhysicsVolume();
        if (!Actor->bScriptInitialized)
            Actor->SetVolumes();
        Actor->eventPostLoadBeginPlay();
    }
    GWarn->StatusUpdatef(1, 3, TEXT(""));
    for (INT i = 0; i < GLevel->Actors.Num(); i++)
        if (GLevel->Actors[i])
            GLevel->Actors[i]->Unknown14C();

    // order the actors: the level info and brush, then static actors that are never relevant to the net,
    // then static relevant ones, then everything else
    TArray<AActor*> Sorted;
    Sorted.AddItem(GLevel->Actors[0]);
    Sorted.AddItem(GLevel->Actors[1]);
    for (INT i = 2; i < GLevel->Actors.Num(); i++)
        if (GLevel->Actors[i] && GLevel->Actors[i]->bStatic && !GLevel->Actors[i]->bAlwaysRelevant)
            Sorted.AddItem(GLevel->Actors[i]);
    GLevel->iFirstNetRelevantActor = Sorted.Num();
    for (INT i = 2; i < GLevel->Actors.Num(); i++)
        if (GLevel->Actors[i] && GLevel->Actors[i]->bStatic && GLevel->Actors[i]->bAlwaysRelevant)
            Sorted.AddItem(GLevel->Actors[i]);
    GLevel->iFirstDynamicActor = Sorted.Num();
    for (INT i = 2; i < GLevel->Actors.Num(); i++)
        if (GLevel->Actors[i] && !GLevel->Actors[i]->bStatic)
            Sorted.AddItem(GLevel->Actors[i]);
    if (GUndo)
        NOT_DECOMPILED(0x0011ec2a);
    GLevel->Actors.Empty();
    GLevel->Actors.AddZeroed(Sorted.Num());
    for (INT i = 0; i < Sorted.Num(); i++)
        GLevel->Actors[i] = Sorted[i];

    GWarn->StatusUpdatef(2, 3, TEXT(""));
    if (Info->Game)
        Info->GRI = Info->Game->GameReplicationInfo;
    if (Client)
        NOT_DECOMPILED(0x00125f8c);
    GLevel->DetailChange(Info->DetailMode);

    LastURL = URL;
    LastURL.RemoveOption(TEXT("MENU"));
    LastURL.RemoveOption(TEXT("MENUARGS"));
    if (GIsClient)
    {
        URL.SaveURLConfig(TEXT("DefaultPlayer"), TEXT("Name"), TEXT("User"));
        URL.SaveURLConfig(TEXT("DefaultPlayer"), TEXT("Team"), TEXT("User"));
        URL.SaveURLConfig(TEXT("DefaultPlayer"), TEXT("Class"), TEXT("User"));
        URL.SaveURLConfig(TEXT("DefaultPlayer"), TEXT("Skin"), TEXT("User"));
        URL.SaveURLConfig(TEXT("DefaultPlayer"), TEXT("Face"), TEXT("User"));
        URL.SaveURLConfig(TEXT("DefaultPlayer"), TEXT("Voice"), TEXT("User"));
        URL.SaveURLConfig(TEXT("DefaultPlayer"), TEXT("OverrideClass"), TEXT("User"));
    }
    if (GCubemapManager)
        NOT_DECOMPILED(0x000b170b);
    GWarn->EndSlowTask();
    FramePresentPending = 0;
    if (Client)
        NOT_DECOMPILED(0x001263fe);

    GLog->Logf(TEXT("Post load memory watermark: %d"), 0);
    GLog->Logf(TEXT("%s init time: %s"), *URL.Map, *TotalTimer.Lap());
    Stats = GFileManager->GetFileStats(1);
    GLog->Logf(TEXT("%s file load stats: %s"), GLevel->GetOuter()->GetName(), *Stats.Describe());
    GIsLoadingLevel = 0;
    if (Audio && GLevel && Info->bAsyncCacheFiles)
        NOT_DECOMPILED(0x00126629);
    GLog->Logf(TEXT("Exiting LoadMap %s"), appTimestamp(1, 1));
    return GLevel;
}

void UGameEngine::BuildServerMasterMap(UNetDriver* NetDriver, ULevel* InLevel)
{
    BeginLoad();
    NetDriver->MasterMap->AddLinker(GetPackageLinker(InLevel->GetOutermostPackage(), nullptr, LOAD_NoFail, nullptr, nullptr));
    for (INT i = 0; i < ServerPackages.Num(); i++)
    {
        GLog->Logf(TEXT("Server Package: %s"), *ServerPackages[i]);
        ULinkerLoad* Linker = GetPackageLinker(nullptr, *ServerPackages[i], LOAD_NoFail, nullptr, nullptr);
        if (NetDriver->MasterMap->AddLinker(Linker) == INDEX_NONE)
            GLog->Logf(TEXT("   (server-side only)"));
    }
    NetDriver->MasterMap->Compute();
    EndLoad();
}

// 0x0011e2f0. everything in it draws through the client's viewport.
void UGameEngine::UpdateProgress()
{
    if (Client)
        NOT_DECOMPILED(0x0011e2f0);
}
