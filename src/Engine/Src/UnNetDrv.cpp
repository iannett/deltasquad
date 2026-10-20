#include "EngineClasses.h"

#define CPP_PROPERTY(Name) static_cast<INT>(reinterpret_cast<BYTE*>(&this->Name) - reinterpret_cast<BYTE*>(static_cast<UObject*>(this)))

INT UNetDriver::NetworkState = 0;

UNetDriver::UNetDriver()
    : ClientConnections()
    , Time(0.0)
{
    RoleProperty = static_cast<UProperty*>(StaticFindObjectChecked(UProperty::StaticClass(), AActor::StaticClass(), TEXT("Role")));
    RemoteRoleProperty = static_cast<UProperty*>(StaticFindObjectChecked(UProperty::StaticClass(), AActor::StaticClass(), TEXT("RemoteRole")));
    MasterMap = new UPackageMap;
    ProfileStats = ParseParam(appCmdLine(), TEXT("profilestats"));
}

void UNetDriver::StaticConstructor()
{
    UClass* Class = GetClass();
    new(Class, TEXT("ConnectionTimeout"), RF_Public) UFloatProperty(CPP_PROPERTY(ConnectionTimeout), TEXT("Client"), CPF_Config);
    new(Class, TEXT("InitialConnectTimeout"), RF_Public) UFloatProperty(CPP_PROPERTY(InitialConnectTimeout), TEXT("Client"), CPF_Config);
    new(Class, TEXT("KeepAliveTime"), RF_Public) UFloatProperty(CPP_PROPERTY(KeepAliveTime), TEXT("Client"), CPF_Config);
    new(Class, TEXT("RelevantTimeout"), RF_Public) UFloatProperty(CPP_PROPERTY(RelevantTimeout), TEXT("Client"), CPF_Config);
    new(Class, TEXT("SpawnPrioritySeconds"), RF_Public) UFloatProperty(CPP_PROPERTY(SpawnPrioritySeconds), TEXT("Client"), CPF_Config);
    new(Class, TEXT("ServerTravelPause"), RF_Public) UFloatProperty(CPP_PROPERTY(ServerTravelPause), TEXT("Client"), CPF_Config);
    new(Class, TEXT("MaxClientRate"), RF_Public) UIntProperty(CPP_PROPERTY(MaxClientRate), TEXT("Client"), CPF_Config);
    new(Class, TEXT("MaxInternetClientRate"), RF_Public) UIntProperty(CPP_PROPERTY(MaxInternetClientRate), TEXT("Client"), CPF_Config);
    new(Class, TEXT("NetServerMaxTickRate"), RF_Public) UIntProperty(CPP_PROPERTY(NetServerMaxTickRate), TEXT("Client"), CPF_Config);
    new(Class, TEXT("LanServerMaxTickRate"), RF_Public) UIntProperty(CPP_PROPERTY(LanServerMaxTickRate), TEXT("Client"), CPF_Config);
    new(Class, TEXT("AllowDownloads"), RF_Public) UBoolProperty(CPP_PROPERTY(AllowDownloads), TEXT("Client"), CPF_Config);
    new(Class, TEXT("MaxDownloadSize"), RF_Public) UIntProperty(CPP_PROPERTY(MaxDownloadSize), TEXT("Client"), CPF_Config);
    UArrayProperty* Managers = new(Class, TEXT("DownloadManagers"), RF_Public) UArrayProperty(CPP_PROPERTY(DownloadManagers), TEXT("Client"), CPF_Config);
    Managers->Inner = new(Managers, NAME_None, RF_Public) UStrProperty;
    MaxClientRate = 10000;
    MaxInternetClientRate = 7000;
}

void UNetDriver::Destroy()
{
    delete ServerConnection;
    while (ClientConnections.Num())
        delete ClientConnections[0];
    LowLevelDestroy();
    delete MasterMap;
    Super::Destroy();
}

void UNetDriver::Serialize(FArchive& Ar)
{
    Super::Serialize(Ar);
    ClientConnections.CountBytes(Ar);
    Ar << ServerConnection << MasterMap << RoleProperty << RemoteRoleProperty;
}

UBOOL UNetDriver::Exec(const TCHAR* Cmd, FOutputDevice& Ar)
{
    NOT_DECOMPILED(0x000543eb);
    return 0;
}

UBOOL UNetDriver::InitConnect(FNetworkNotify* InNotify, FURL& ConnectURL, FString& Error)
{
    Notify = InNotify;
    return 1;
}

UBOOL UNetDriver::InitListen(FNetworkNotify* InNotify, FURL& ListenURL, FString& Error)
{
    Notify = InNotify;
    return 1;
}

void UNetDriver::TickFlush()
{
    // each connection ticks and sends what it has queued. UNetConnection is not decompiled yet.
    if (ServerConnection)
        NOT_DECOMPILED(0x000538bb);
    for (INT i = 0; i < ClientConnections.Num(); i++)
        NOT_DECOMPILED(0x000538ce);
}

void UNetDriver::TickDispatch(FLOAT DeltaTime)
{
    RecvCycles = 0;
    SendCycles = 0;
    // the xbox adds in single precision
    Time = static_cast<FLOAT>(Time) + DeltaTime;
    // a server deletes its closed client connections
    if (!ServerConnection)
    {
        for (INT i = ClientConnections.Num() - 1; i >= 0; i--)
            NOT_DECOMPILED(0x00053913);
    }
}

void UNetDriver::NotifyActorDestroyed(AActor* Actor)
{
    NOT_DECOMPILED(0x000552b7);
}
