#include <new>

#include "EngineClasses.h"

// 0x00477f94 to 0x00477fd8
FString FURL::DefaultProtocol;
FString FURL::DefaultProtocolDescription;
FString FURL::DefaultName;
FString FURL::DefaultMap;
FString FURL::DefaultLocalMap;
FString FURL::DefaultHost;
FString FURL::DefaultPortal;
FString FURL::DefaultMapExt;
FString FURL::DefaultSaveExt;
INT     FURL::DefaultPort;

// 0x000a97ad
void FURL::StaticInit()
{
    DefaultProtocol = GConfig->GetStr(TEXT("URL"), TEXT("Protocol"));
    DefaultProtocolDescription = GConfig->GetStr(TEXT("URL"), TEXT("ProtocolDescription"));
    DefaultName = GConfig->GetStr(TEXT("URL"), TEXT("Name"));
    DefaultMap = GConfig->GetStr(TEXT("URL"), TEXT("Map"));
    DefaultLocalMap = GConfig->GetStr(TEXT("URL"), TEXT("LocalMap"));
    DefaultHost = GConfig->GetStr(TEXT("URL"), TEXT("Host"));
    DefaultPortal = GConfig->GetStr(TEXT("URL"), TEXT("Portal"));
    DefaultMapExt = GConfig->GetStr(TEXT("URL"), TEXT("MapExt"));
    DefaultSaveExt = GConfig->GetStr(TEXT("URL"), TEXT("SaveExt"));
    DefaultPort = appAtoi(GConfig->GetStr(TEXT("URL"), TEXT("Port")));
}

// 0x000a9afb
FURL::FURL(const TCHAR* LocalFilename)
{
    new(&Protocol) FString(DefaultProtocol);
    new(&Host) FString(DefaultHost);
    Port = DefaultPort;
    new(&Map) FString(LocalFilename ? FString(LocalFilename) : DefaultMap);
    new(&Op) TArray<FString>;
    new(&Portal) FString(DefaultPortal);
    Valid = 1;
}

FURL::FURL(const FURL& Other)
{
    new(&Protocol) FString(Other.Protocol);
    new(&Host) FString(Other.Host);
    Port = Other.Port;
    new(&Map) FString(Other.Map);
    new(&Op) TArray<FString>;
    Op = Other.Op;
    new(&Portal) FString(Other.Portal);
    Valid = Other.Valid;
}

// 0x00058f28
FURL& FURL::operator=(const FURL& Other)
{
    Protocol = Other.Protocol;
    Host = Other.Host;
    Port = Other.Port;
    Map = Other.Map;
    Op = Other.Op;
    Portal = Other.Portal;
    Valid = Other.Valid;
    return *this;
}

// 0x000a9cd8
FURL::FURL(const FURL* Base, const TCHAR* TextURL, ETravelType Type)
{
    new(&Protocol) FString(DefaultProtocol);
    new(&Host) FString(DefaultHost);
    Port = DefaultPort;
    new(&Map) FString(DefaultMap);
    new(&Op) TArray<FString>;
    new(&Portal) FString(DefaultPortal);
    Valid = 1;

    TCHAR TempURL[1024];
    appStrncpy(TempURL, TextURL, ARRAY_COUNT(TempURL));
    TCHAR* URL = TempURL;

    if (Type == TRAVEL_Relative)
    {
        Protocol = Base->Protocol;
        Host = Base->Host;
        Map = Base->Map;
        Portal = Base->Portal;
        Port = Base->Port;
    }
    if (Type == TRAVEL_Relative || Type == TRAVEL_Partial)
    {
        // options that only make sense for one trip are not carried over
        for (INT i = 0; i < Base->Op.Num(); i++)
        {
            const TCHAR* Option = *Base->Op[i];
            if (appStricmp(Option, TEXT("PUSH")) && appStricmp(Option, TEXT("POP")) && appStricmp(Option, TEXT("PEER"))
                && appStricmp(Option, TEXT("LOAD")) && appStricmp(Option, TEXT("QUIET")))
                new(&Op[Op.Add()]) FString(Base->Op[i]);
        }
    }

    while (*URL == ' ')
        URL++;

    // 0x000a9557 finds the first of the two. options start with ? and a portal with #
    auto FindOptionChar = [](TCHAR* Str) -> TCHAR*
    {
        TCHAR* Question = appStrchr(Str, '?');
        TCHAR* Hash = appStrchr(Str, '#');
        return Question && (!Hash || Question < Hash) ? Question : Hash;
    };
    TCHAR* s = FindOptionChar(URL);
    if (s)
    {
        TCHAR OptionChar = *s;
        TCHAR NextOptionChar = 0;
        *s++ = 0;
        do
        {
            TCHAR* t = FindOptionChar(s);
            if (t)
            {
                NextOptionChar = *t;
                *t++ = 0;
            }
            if (appStrchr(s, ' '))
            {
                // the xbox passes the option as the format string
                GLog->Log(s);
                *this = FURL(nullptr);
                Valid = 0;
                return;
            }
            if (OptionChar == '?')
                AddOption(s);
            else
                Portal = s;
            OptionChar = NextOptionChar;
            s = t;
        } while (s);
    }

    UBOOL HostProvided = 0;
    UBOOL MapProvided = 0;
    if (appStrlen(URL) > 2 && URL[1] == ':')
    {
        // a drive letter: a local file
        Protocol = DefaultProtocol;
        Host = DefaultHost;
        Map = URL;
        Portal = DefaultPortal;
        URL = nullptr;
        HostProvided = 1;
        MapProvided = 1;
        Host = TEXT("");
    }
    else
    {
        if (appStrchr(URL, ':') && appStrchr(URL, ':') > URL + 1 && (!appStrchr(URL, '.') || appStrchr(URL, ':') < appStrchr(URL, '.')))
        {
            TCHAR* ProtocolStart = URL;
            URL = appStrchr(URL, ':');
            *URL++ = 0;
            Protocol = ProtocolStart;
        }
        if (*URL == '/')
        {
            if (URL[1] != '/')
            {
                *this = FURL(nullptr);
                Valid = 0;
                return;
            }
            URL += 2;
            HostProvided = 1;
            Host = TEXT("");
        }
        // a dot not followed by a map or save extension makes this a host name
        TCHAR* Dot = appStrchr(URL, '.');
        if (Dot && Dot - URL > 0)
        {
            const INT MapExtLen = DefaultMapExt.Len();
            const INT SaveExtLen = DefaultSaveExt.Len();
            const UBOOL IsMap = appStrnicmp(Dot + 1, *DefaultMapExt, MapExtLen) == 0 && !appIsAlnum(Dot[MapExtLen + 1]);
            const UBOOL IsSave = appStrnicmp(Dot + 1, *DefaultSaveExt, SaveExtLen) == 0 && !appIsAlnum(Dot[SaveExtLen + 1]);
            if (!IsMap && !IsSave)
            {
                TCHAR* HostStart = URL;
                URL = appStrchr(URL, '/');
                if (URL)
                    *URL++ = 0;
                TCHAR* Colon = appStrchr(HostStart, ':');
                if (Colon)
                {
                    *Colon++ = 0;
                    Port = appAtoi(Colon);
                }
                Host = HostStart;
                if (appStricmp(*Protocol, *DefaultProtocol) == 0)
                    Map = DefaultMap;
                else
                    Map = TEXT("");
                HostProvided = 1;
            }
        }
    }

    // an absolute trip inside the game keeps the player's identity
    if (Type == TRAVEL_Absolute && Base && IsInternal())
    {
        for (INT i = 0; i < Base->Op.Num(); i++)
        {
            const TCHAR* Option = *Base->Op[i];
            if (appStrnicmp(Option, TEXT("Name="), 5) == 0 || appStrnicmp(Option, TEXT("Team="), 5) == 0
                || appStrnicmp(Option, TEXT("Class="), 6) == 0 || appStrnicmp(Option, TEXT("Skin="), 5) == 0
                || appStrnicmp(Option, TEXT("Face="), 5) == 0 || appStrnicmp(Option, TEXT("Voice="), 6) == 0
                || appStrnicmp(Option, TEXT("OverrideClass="), 14) == 0)
            {
                TCHAR Key[100];
                const TCHAR* Equals = appStrchr(Option, '=');
                if (Equals)
                    appStrncpy(Key, Option, static_cast<INT>(Equals - Option) + 1);
                else
                    appStrcpy(Key, Option);
                if (appStricmp(GetOption(Key, TEXT("")), TEXT("")) == 0)
                {
                    GLog->Logf(TEXT("URL: Adding default option %s"), Option);
                    new(&Op[Op.Add()]) FString(Base->Op[i]);
                }
            }
        }
    }

    if (URL && *URL)
    {
        if (IsInternal())
        {
            MapProvided = 1;
            TCHAR* PortalStart = appStrchr(URL, '/');
            if (PortalStart)
            {
                *PortalStart++ = 0;
                TCHAR* Extra = appStrchr(PortalStart, '/');
                if (Extra)
                {
                    *Extra = 0;
                    if (Extra[1])
                    {
                        *this = FURL(nullptr);
                        Valid = 0;
                        return;
                    }
                }
                Portal = PortalStart;
            }
        }
        Map = URL;
    }

    if (appStrchr(*Protocol, ' ') || appStrchr(*Host, ' ') || appStrchr(*Portal, ' ') || (!HostProvided && !MapProvided && !Op.Num()))
    {
        *this = FURL(nullptr);
        Valid = 0;
    }
}

// 0x000a95bc
UBOOL FURL::IsInternal() const
{
    return Protocol == DefaultProtocol;
}

// 0x000a9947
void FURL::AddOption(const TCHAR* Str)
{
    const INT Match = appStrchr(Str, '=') ? static_cast<INT>(appStrchr(Str, '=') - Str) + 1 : appStrlen(Str) + 1;
    INT i;
    for (i = 0; i < Op.Num(); i++)
    {
        if (appStrnicmp(*Op[i], Str, Match) == 0)
            break;
    }
    if (i == Op.Num())
        new(&Op[Op.Add()]) FString(Str);
    else
        Op[i] = Str;
}

// 0x000a96d9
const TCHAR* FURL::GetOption(const TCHAR* Match, const TCHAR* Default) const
{
    for (INT i = 0; i < Op.Num(); i++)
    {
        if (appStrnicmp(*Op[i], Match, appStrlen(Match)) == 0)
            return *Op[i] + appStrlen(Match);
    }
    return Default;
}

// 0x000a9aa4
void FURL::LoadURLConfig(const TCHAR* Section, const TCHAR* Filename)
{
    TCHAR Text[32000];
    GConfig->GetSection(Section, Text, ARRAY_COUNT(Text), Filename);
    for (TCHAR* Ptr = Text; *Ptr; Ptr += appStrlen(Ptr) + 1)
        AddOption(Ptr);
}

// 0x000a95f1
void FURL::SaveURLConfig(const TCHAR* Section, const TCHAR* Key, const TCHAR* Filename) const
{
    for (INT i = 0; i < Op.Num(); i++)
    {
        TCHAR Temp[1024];
        appStrncpy(Temp, *Op[i], ARRAY_COUNT(Temp));
        TCHAR* Value = appStrchr(Temp, '=');
        if (Value)
        {
            *Value++ = 0;
            if (appStricmp(Temp, Key) == 0)
                GConfig->SetString(Section, Temp, Value, Filename);
        }
    }
}

// 0x000a9a10
void FURL::RemoveOption(const TCHAR* Key)
{
    const TCHAR* Equals = appStrchr(Key, '=');
    const INT Len = Equals ? static_cast<INT>(Equals - Key) + 1 : appStrlen(Key);
    for (INT i = 0; i < Op.Num(); i++)
    {
        if (appStrnicmp(*Op[i], Key, Len) == 0)
        {
            Op.Remove(i);
            return;
        }
    }
}

// 0x000a95ca
UBOOL FURL::IsLocalInternal() const
{
    return IsInternal() && Host.Len() == 0;
}

// 0x000a9688
UBOOL FURL::HasOption(const TCHAR* Test) const
{
    for (INT i = 0; i < Op.Num(); i++)
    {
        if (appStricmp(*Op[i], Test) == 0)
            return 1;
    }
    return 0;
}

// 0x000a9b81
FString FURL::String(UBOOL FullyQualified) const
{
    FString Result;
    if (Protocol != DefaultProtocol || FullyQualified)
    {
        Result += Protocol;
        Result += TEXT(":");
        if (Host != DefaultHost)
            Result += TEXT("//");
    }
    if (Host != DefaultHost || Port != DefaultPort)
    {
        Result += Host;
        if (Port != DefaultPort)
        {
            Result += TEXT(":");
            TCHAR PortText[32];
            appSprintf(PortText, TEXT("%i"), Port);
            Result += PortText;
        }
        Result += TEXT("/");
    }
    Result += Map;
    for (INT i = 0; i < Op.Num(); i++)
    {
        Result += TEXT("?");
        Result += Op[i];
    }
    if (Portal.Len())
    {
        Result += TEXT("#");
        Result += Portal;
    }
    return Result;
}
