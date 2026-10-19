#pragma once

#include "UnObjBas.h"
#include "UnString.h"

// a command line tool written in script. the help strings come from the package's localization.
class UCommandlet : public UObject
{
    DECLARE_CLASS(UCommandlet, UObject, CLASS_Abstract | CLASS_Transient | CLASS_Localized, UObject)
public:
    // 0x00026782
    UCommandlet() {}

    virtual INT Main(const TCHAR* Parms);
    DECLARE_FUNCTION(execMain)

    FStringNoInit HelpCmd;
    FStringNoInit HelpOneLiner;
    FStringNoInit HelpUsage;
    FStringNoInit HelpWebLink;
    FStringNoInit HelpParm[16];
    FStringNoInit HelpDesc[16];
    BITFIELD      LogToStdout : 1;
    BITFIELD      IsServer : 1;
    BITFIELD      IsClient : 1;
    BITFIELD      IsEditor : 1;
    BITFIELD      LazyLoad : 1;
    BITFIELD      ShowErrorCount : 1;
    BITFIELD      ShowBanner : 1;
};

// the sample commandlet. its Main is script.
class UHelloWorldCommandlet : public UCommandlet
{
    DECLARE_CLASS(UHelloWorldCommandlet, UCommandlet, CLASS_Transient, UObject)
public:
    // 0x000b30e3
    UHelloWorldCommandlet() {}

    INT           intparm;
    FStringNoInit strparm;
};
