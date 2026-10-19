#pragma once

#include "UnObjBas.h"
#include "UnOutputDevice.h"

// a subsystem: an object that also takes console commands
class USubsystem : public UObject, public FExec
{
    DECLARE_CLASS(USubsystem, UObject, CLASS_Abstract | CLASS_Transient, UObject)
public:
    UBOOL Exec(const TCHAR* Cmd, FOutputDevice& Ar) override { return 0; }
};

// engine settings from [Core.System] in System.ini, mainly where packages are found
class USystem : public USubsystem
{
    DECLARE_CLASS(USystem, USubsystem, CLASS_Config, UObject)
public:
    USystem();
    void StaticConstructor();
    UBOOL Exec(const TCHAR* Cmd, FOutputDevice& Ar) override;

    INT             Unknown2C; // set to 1 by the constructor; nothing in Core reads it
    INT             PurgeCacheDays;
    FStringNoInit   SavePath;
    FStringNoInit   CachePath;
    FStringNoInit   CacheExt;
    FStringNoInit   SourcePath;
    // package search patterns such as ..\System\*.u
    TArrayNoInit<FString> Paths;
    // log events to hide
    TArrayNoInit<FName> Suppress;
    // the Linear paths are searched only with this set
    UBOOL           bLinearLoad;
    UBOOL           bLinearSave;
};

extern USystem* GSys;
