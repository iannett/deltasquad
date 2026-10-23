#pragma once

// port: posix sockets stand in for the xbox's winsock. the calls keep their winsock names and meaning.
#include <netinet/in.h>

#include "Core.h"

typedef INT SOCKET;
enum
{
    INVALID_SOCKET = -1,
    SOCKET_ERROR   = -1,
};

// the name both drivers log for the socket layer
#define SOCKET_API TEXT("WinSock")

// 0x00234a65. the xbox only reports whether its network stack started.
UBOOL InitSockets(FString& Error);
// 0x00234a6b. the error's winsock name; -1 means the last error.
const TCHAR* SocketError(INT Code = -1);
// 0x00234d7d
UBOOL SetNonBlocking(SOCKET Socket);
// 0x00234d9e
UBOOL SetSocketReuseAddr(SOCKET Socket, UBOOL ReUse = 1);
// 0x00234e4c. "a.b.c.d", with ":port" when a port is given.
FString IpString(in_addr Addr, INT Port = 0);

// 0x00234d57
inline void IpSetBytes(in_addr& Addr, BYTE A, BYTE B, BYTE C, BYTE D)
{
    BYTE* Bytes = reinterpret_cast<BYTE*>(&Addr.s_addr);
    Bytes[0] = A;
    Bytes[1] = B;
    Bytes[2] = C;
    Bytes[3] = D;
}

// 0x00234d7a
inline void IpSetInt(in_addr& Addr, DWORD Value)
{
    Addr.s_addr = Value;
}

// port: stands in for XNetGetTitleXnAddr's address, the machine's own ipv4 address. 0 when it has none.
in_addr GetTitleAddr();

// the address sockets bind to: a MULTIHOME= address, else any. PRIMARYNET binds the machine's own address.
// inlined by both the net driver and BindPort on the xbox; Out gets the bad address message.
in_addr GetLocalHostAddr(FOutputDevice& Out);
