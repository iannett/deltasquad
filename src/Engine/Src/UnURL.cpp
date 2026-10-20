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
