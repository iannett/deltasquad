#include <cerrno>
#include <sys/socket.h>
#include <unistd.h>

#include "IpDrvClasses.h"

#define CPP_PROPERTY(Name) static_cast<INT>(reinterpret_cast<BYTE*>(&this->Name) - reinterpret_cast<BYTE*>(static_cast<UObject*>(this)))

namespace
{
    // 0x004e7348 and on. the ports client drivers hold, so two clients on one machine get different ones.
    INT  GNumUsedPorts;
    INT  GUsedPorts[10];
    // 0x004e7379
    UBOOL GUsedPortsInited;
}

void UTcpNetDriver::StaticConstructor()
{
    UClass* Class = GetClass();
    new(Class, TEXT("AllowPlayerPortUnreach"), RF_Public) UBoolProperty(CPP_PROPERTY(AllowPlayerPortUnreach), TEXT("Client"), CPF_Config);
    new(Class, TEXT("LogPortUnreach"), RF_Public) UBoolProperty(CPP_PROPERTY(LogPortUnreach), TEXT("Client"), CPF_Config);
    new(Class, TEXT("MaxConnPerIPPerMinute"), RF_Public) UIntProperty(CPP_PROPERTY(MaxConnPerIPPerMinute), TEXT("Client"), CPF_Config);
    new(Class, TEXT("LogMaxConnPerIPPerMin"), RF_Public) UBoolProperty(CPP_PROPERTY(LogMaxConnPerIPPerMin), TEXT("Client"), CPF_Config);
    new(Class, TEXT("StoredHostInfo"), RF_Public) UBoolProperty(CPP_PROPERTY(StoredHostInfo), TEXT("UTcpNetDriver"), CPF_Config);
    new(Class, TEXT("StoredHostSessionID"), RF_Public) UStrProperty(CPP_PROPERTY(StoredHostSessionID), TEXT("UTcpNetDriver"), CPF_Config);
    new(Class, TEXT("StoredHostExchangeKey"), RF_Public) UStrProperty(CPP_PROPERTY(StoredHostExchangeKey), TEXT("UTcpNetDriver"), CPF_Config);
}

UBOOL UTcpNetDriver::Exec(const TCHAR* Cmd, FOutputDevice& Ar)
{
    if (ParseCommand(&Cmd, TEXT("RETRY_XNET")))
    {
        // a system link client decodes its stored session key and translates the host's address with XNet
        NOT_DECOMPILED(0x0022cf5a);
        return 1;
    }
    return UNetDriver::Exec(Cmd, Ar);
}

void UTcpNetDriver::LowLevelDestroy()
{
    if (Socket)
    {
        if (close(Socket))
            GLog->Logf(NAME_Exit, TEXT("WinSock closesocket error (%i)"), errno);
        Socket = 0;
        GLog->Logf(NAME_Exit, TEXT("WinSock shut down"));
    }
    // a system link driver also gives its port back and releases its session key
    if (GetNetworkState() == 2)
        NOT_DECOMPILED(0x0022b4f5);
}

FString UTcpNetDriver::LowLevelGetNetworkNumber()
{
    return IpString(LocalAddr.sin_addr);
}

UBOOL UTcpNetDriver::InitConnect(FNetworkNotify* InNotify, FURL& ConnectURL, FString& Error)
{
    NOT_DECOMPILED(0x0022c527);
    return 0;
}

UBOOL UTcpNetDriver::InitListen(FNetworkNotify* InNotify, FURL& ListenURL, FString& Error)
{
    if (!UNetDriver::InitListen(InNotify, ListenURL, Error) || !InitBase(0, InNotify, ListenURL, Error))
        return 0;
    ListenURL.Host = IpString(LocalAddr.sin_addr);
    ListenURL.Port = ntohs(LocalAddr.sin_port);
    GLog->Logf(NAME_DevNet, TEXT("TcpNetDriver on port %i"), ListenURL.Port);
    // a system link host registers its session key
    if (!ParseParam(appCmdLine(), TEXT("XREBOOTLIVE")) && GetNetworkState() == 2)
        NOT_DECOMPILED(0x0022c79d);
    return 1;
}

void UTcpNetDriver::TickDispatch(FLOAT DeltaTime)
{
    UNetDriver::TickDispatch(DeltaTime);

    // read every waiting packet. the xbox charges the reads to RecvCycles.
    for (;;)
    {
        BYTE Data[1024];
        sockaddr_in FromAddr;
        socklen_t FromSize = sizeof(FromAddr);
        const INT Size = static_cast<INT>(recvfrom(Socket, Data, sizeof(Data), 0, reinterpret_cast<sockaddr*>(&FromAddr), &FromSize));
        if (Size == SOCKET_ERROR)
        {
            const INT Code = errno;
            if (Code == EWOULDBLOCK)
                return;
            // winsock's connection reset is an icmp port unreachable; posix reports it as connection refused
            if (Code != ECONNRESET && Code != ECONNREFUSED)
            {
                static UBOOL FirstError = 1;
                if (FirstError)
                    GLog->Logf(TEXT("UDP recvfrom error: %i from %s:%d"), Code, *IpString(FromAddr.sin_addr), ntohs(FromAddr.sin_port));
                FirstError = 0;
                return;
            }
        }
        // the packet's connection, a new one, or the unreachable client's disconnect
        NOT_DECOMPILED(0x0022cb34);
    }
}

void UTcpNetDriver::PreServerTravel()
{
    if (GetNetworkState() == 2)
        NOT_DECOMPILED(0x0022cf22);
}

UBOOL UTcpNetDriver::InitBase(UBOOL Connect, FNetworkNotify* InNotify, FURL& URL, FString& Error)
{
    if (!InitSockets(Error))
        return 0;
    // the xbox also clears its system link key buffers here
    if (!GUsedPortsInited)
    {
        GNumUsedPorts = 0;
        for (INT i = 0; i < 10; i++)
            GUsedPorts[i] = 0;
        GUsedPortsInited = 1;
    }
    UnknownB8 = 0;

    // port: the error codes are printed as numbers. the xbox passes the error's name to %i, which prints its address.
    Socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (Socket == INVALID_SOCKET)
    {
        Socket = 0;
        Error = FString::Printf(TEXT("WinSock: socket failed (%i)"), errno);
        return 0;
    }
    INT TrueValue = 1;
    if (setsockopt(Socket, SOL_SOCKET, SO_BROADCAST, &TrueValue, sizeof(TrueValue)))
    {
        Error = FString::Printf(TEXT("%s: setsockopt SO_BROADCAST failed (%i)"), SOCKET_API, errno);
        return 0;
    }
    if (!SetSocketReuseAddr(Socket))
        GLog->Logf(TEXT("setsockopt with SO_REUSEADDR failed"));
    // the xbox checks for IP_RECVERR here too, but its check always succeeds

    INT RecvSize = Connect ? 0x8000 : 0x20000;
    INT SendSize = RecvSize;
    socklen_t SizeSize = sizeof(RecvSize);
    setsockopt(Socket, SOL_SOCKET, SO_RCVBUF, &RecvSize, SizeSize);
    getsockopt(Socket, SOL_SOCKET, SO_RCVBUF, &RecvSize, &SizeSize);
    setsockopt(Socket, SOL_SOCKET, SO_SNDBUF, &SendSize, SizeSize);
    getsockopt(Socket, SOL_SOCKET, SO_SNDBUF, &SendSize, &SizeSize);
    GLog->Logf(NAME_Init, TEXT("%s: Socket queue %i / %i"), SOCKET_API, RecvSize, SendSize);

    // system link binds any address
    LocalAddr.sin_family = AF_INET;
    if (GetNetworkState() == 2)
        IpSetInt(LocalAddr.sin_addr, 0);
    else
        LocalAddr.sin_addr = GetLocalHostAddr(*GLog);

    // servers take PORT= or the url's port, clients the lowest port from 2000 no other client holds
    LocalAddr.sin_port = 0;
    UBOOL HardcodedPort = 0;
    if (!Connect)
    {
        HardcodedPort = Parse(appCmdLine(), TEXT("PORT="), URL.Port);
        LocalAddr.sin_port = htons(static_cast<_WORD>(URL.Port));
    }
    else if (GNumUsedPorts != 10)
    {
        INT Port = 2000;
        for (INT i = 0; i < GNumUsedPorts; )
        {
            if (GUsedPorts[i] == Port)
            {
                Port++;
                i = 0;
            }
            else
            {
                i++;
            }
        }
        GUsedPorts[GNumUsedPorts++] = Port;
        LocalAddr.sin_port = htons(static_cast<_WORD>(Port));
    }

    // try the next ports up when it is taken
    const INT AttemptPort = ntohs(LocalAddr.sin_port);
    const INT Tries = HardcodedPort ? 1 : 20;
    INT BoundPort = 0;
    for (INT i = 0; i < Tries; i++)
    {
        if (!bind(Socket, reinterpret_cast<sockaddr*>(&LocalAddr), sizeof(LocalAddr)))
        {
            if (ntohs(LocalAddr.sin_port) != 0)
            {
                BoundPort = ntohs(LocalAddr.sin_port);
            }
            else
            {
                // port 0 lets the system pick one
                sockaddr_in BoundAddr;
                socklen_t AddrSize = sizeof(BoundAddr);
                getsockname(Socket, reinterpret_cast<sockaddr*>(&BoundAddr), &AddrSize);
                BoundPort = ntohs(BoundAddr.sin_port);
            }
            break;
        }
        if (LocalAddr.sin_port == 0)
            break;
        LocalAddr.sin_port = htons(static_cast<_WORD>(ntohs(LocalAddr.sin_port) + 1));
    }
    if (!BoundPort)
    {
        Error = FString::Printf(TEXT("%s: binding to port %i failed (%i)"), SOCKET_API, AttemptPort, errno);
        return 0;
    }
    if (!SetNonBlocking(Socket))
    {
        Error = FString::Printf(TEXT("%s: SetNonBlocking failed (%i)"), SOCKET_API, errno);
        return 0;
    }
    return 1;
}
