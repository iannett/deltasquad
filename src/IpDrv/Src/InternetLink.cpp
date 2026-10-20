#include <sys/socket.h>
#include <unistd.h>

#include "IpDrvClasses.h"

// ---------------------------------------------------------------------------------------------- internet links

AInternetLink::AInternetLink()
{
    FString Error;
    InitSockets(Error);
    Socket = INVALID_SOCKET;
    RemoteSocket = INVALID_SOCKET;
    LinkMode = 0;
    ReceiveMode = 1;
    DataPending = 0;
    Port = 0;
}

// ---------------------------------------------------------------------------------------------- udp links

// 0x00230d7f
void AUdpLink::execBindPort(FFrame& Stack, RESULT_DECL)
{
    P_GET_INT_OPTX(InPort, 0);
    P_GET_UBOOL(bUseNextAvailable);
    P_FINISH;

    FString Error;
    if (!InitSockets(Error))
    {
        Stack.Logf(TEXT("BindPort: winsock failed"));
        *static_cast<DWORD*>(Result) = 0;
        return;
    }
    if (Socket != INVALID_SOCKET)
    {
        Stack.Logf(TEXT("BindPort: already bound"));
        *static_cast<DWORD*>(Result) = 0;
        return;
    }

    Socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (Socket == INVALID_SOCKET)
    {
        Stack.Logf(TEXT("BindPort: socket failed"));
    }
    else
    {
        INT TrueValue = 1;
        if (setsockopt(Socket, SOL_SOCKET, SO_BROADCAST, &TrueValue, sizeof(TrueValue)))
        {
            Stack.Logf(TEXT("BindPort: setsockopt failed"));
        }
        else
        {
            sockaddr_in Addr = {};
            Addr.sin_family = AF_INET;
            if (UNetDriver::GetNetworkState() == 2)
                IpSetInt(Addr.sin_addr, 0);
            else
                Addr.sin_addr = GetLocalHostAddr(Stack);
            Addr.sin_port = htons(static_cast<_WORD>(InPort));

            // with bUseNextAvailable the next 19 ports up are tried as well
            const INT Tries = bUseNextAvailable ? 20 : 1;
            INT BoundPort = 0;
            for (INT i = 0; i < Tries; i++)
            {
                if (!bind(Socket, reinterpret_cast<sockaddr*>(&Addr), sizeof(Addr)))
                {
                    if (ntohs(Addr.sin_port) != 0)
                    {
                        BoundPort = ntohs(Addr.sin_port);
                    }
                    else
                    {
                        sockaddr_in BoundAddr;
                        socklen_t AddrSize = sizeof(BoundAddr);
                        getsockname(Socket, reinterpret_cast<sockaddr*>(&BoundAddr), &AddrSize);
                        BoundPort = ntohs(BoundAddr.sin_port);
                    }
                    break;
                }
                if (Addr.sin_port == 0)
                    break;
                Addr.sin_port = htons(static_cast<_WORD>(ntohs(Addr.sin_port) + 1));
            }

            if (!BoundPort)
            {
                Stack.Logf(TEXT("BindPort: bind failed"));
            }
            else if (!SetNonBlocking(Socket))
            {
                Stack.Logf(TEXT("BindPort: ioctlsocket failed"));
            }
            else
            {
                *static_cast<DWORD*>(Result) = BoundPort;
                Port = ntohs(Addr.sin_port);
                return;
            }
        }
    }
    // the xbox leaves the socket 0 here instead of invalid
    close(Socket);
    Socket = 0;
    *static_cast<DWORD*>(Result) = 0;
}
