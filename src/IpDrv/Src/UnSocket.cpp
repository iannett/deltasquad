#include <cerrno>
#include <fcntl.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <sys/socket.h>

#include "UnSocket.h"

UBOOL InitSockets(FString& Error)
{
    // port: posix sockets need no start up
    return 1;
}

const TCHAR* SocketError(INT Code)
{
    if (Code == -1)
        Code = errno;
    switch (Code)
    {
    case 0:               return TEXT("WSANO_ERROR");
    case EINTR:           return TEXT("WSAEINTR");
    case EBADF:           return TEXT("WSAEBADF");
    case EACCES:          return TEXT("WSAEACCES");
    case EFAULT:          return TEXT("WSAEFAULT");
    case EINVAL:          return TEXT("WSAEINVAL");
    case EMFILE:          return TEXT("WSAEMFILE");
    case EWOULDBLOCK:     return TEXT("WSAEWOULDBLOCK");
    case EINPROGRESS:     return TEXT("WSAEINPROGRESS");
    case EALREADY:        return TEXT("WSAEALREADY");
    case ENOTSOCK:        return TEXT("WSAENOTSOCK");
    case EDESTADDRREQ:    return TEXT("WSAEDESTADDRREQ");
    case EMSGSIZE:        return TEXT("WSAEMSGSIZE");
    case EPROTOTYPE:      return TEXT("WSAEPROTOTYPE");
    case ENOPROTOOPT:     return TEXT("WSAENOPROTOOPT");
    case EPROTONOSUPPORT: return TEXT("WSAEPROTONOSUPPORT");
    case ESOCKTNOSUPPORT: return TEXT("WSAESOCKTNOSUPPORT");
    case EOPNOTSUPP:      return TEXT("WSAEOPNOTSUPP");
    case EPFNOSUPPORT:    return TEXT("WSAEPFNOSUPPORT");
    case EAFNOSUPPORT:    return TEXT("WSAEAFNOSUPPORT");
    case EADDRINUSE:      return TEXT("WSAEADDRINUSE");
    case EADDRNOTAVAIL:   return TEXT("WSAEADDRNOTAVAIL");
    case ENETDOWN:        return TEXT("WSAENETDOWN");
    case ENETUNREACH:     return TEXT("WSAENETUNREACH");
    case ENETRESET:       return TEXT("WSAENETRESET");
    case ECONNABORTED:    return TEXT("WSAECONNABORTED");
    case ECONNRESET:      return TEXT("WSAECONNRESET");
    case ENOBUFS:         return TEXT("WSAENOBUFS");
    case EISCONN:         return TEXT("WSAEISCONN");
    case ENOTCONN:        return TEXT("WSAENOTCONN");
    case ESHUTDOWN:       return TEXT("WSAESHUTDOWN");
    case ETOOMANYREFS:    return TEXT("WSAETOOMANYREFS");
    case ETIMEDOUT:       return TEXT("WSAETIMEDOUT");
    case ECONNREFUSED:    return TEXT("WSAECONNREFUSED");
    case ELOOP:           return TEXT("WSAELOOP");
    case ENAMETOOLONG:    return TEXT("WSAENAMETOOLONG");
    case EHOSTDOWN:       return TEXT("WSAEHOSTDOWN");
    case EHOSTUNREACH:    return TEXT("WSAEHOSTUNREACH");
    case ENOTEMPTY:       return TEXT("WSAENOTEMPTY");
#ifdef EPROCLIM
    case EPROCLIM:        return TEXT("WSAEPROCLIM");
#endif
    case EUSERS:          return TEXT("WSAEUSERS");
    case EDQUOT:          return TEXT("WSAEDQUOT");
    case ESTALE:          return TEXT("WSAESTALE");
    case EREMOTE:         return TEXT("WSAEREMOTE");
    default:              return TEXT("WSA_Unknown");
    }
}

UBOOL SetNonBlocking(SOCKET Socket)
{
    const INT Flags = fcntl(Socket, F_GETFL, 0);
    return Flags != -1 && fcntl(Socket, F_SETFL, Flags | O_NONBLOCK) == 0;
}

UBOOL SetSocketReuseAddr(SOCKET Socket, UBOOL ReUse)
{
    INT Value = ReUse ? 1 : 0;
    return setsockopt(Socket, SOL_SOCKET, SO_REUSEADDR, &Value, sizeof(Value)) == 0;
}

FString IpString(in_addr Addr, INT Port)
{
    const BYTE* Bytes = reinterpret_cast<const BYTE*>(&Addr.s_addr);
    FString Result = FString::Printf(TEXT("%i.%i.%i.%i"), Bytes[0], Bytes[1], Bytes[2], Bytes[3]);
    if (Port)
        Result += FString::Printf(TEXT(":%i"), Port);
    return Result;
}

in_addr GetLocalHostAddr(FOutputDevice& Out)
{
    in_addr HostAddr;
    IpSetInt(HostAddr, 0);
    TCHAR Home[256] = TEXT("");
    if (Parse(appCmdLine(), TEXT("MULTIHOME="), Home, ARRAY_COUNT(Home)))
    {
        TCHAR* B = appStrchr(Home, '.');
        TCHAR* C = B ? appStrchr(B + 1, '.') : nullptr;
        TCHAR* D = C ? appStrchr(C + 1, '.') : nullptr;
        if (D)
            IpSetBytes(HostAddr, appAtoi(Home), appAtoi(B + 1), appAtoi(C + 1), appAtoi(D + 1));
        else
            Out.Logf(TEXT("Invalid multihome IP address %s"), Home);
        return HostAddr;
    }
    HostAddr = GetTitleAddr();
    const UBOOL AnyAddr = !ParseParam(appCmdLine(), TEXT("PRIMARYNET"));
    // 0x004e7194
    static UBOOL First = 1;
    if (First)
    {
        First = 0;
        GLog->Logf(NAME_Init, TEXT("%s: XBox doesn't support gethostname() (%s)"), SOCKET_API, *IpString(HostAddr));
    }
    if (AnyAddr)
        IpSetInt(HostAddr, 0);
    return HostAddr;
}

in_addr GetTitleAddr()
{
    in_addr Result;
    Result.s_addr = 0;
    ifaddrs* List = nullptr;
    if (getifaddrs(&List) != 0)
        return Result;
    for (ifaddrs* It = List; It; It = It->ifa_next)
    {
        if (It->ifa_addr && It->ifa_addr->sa_family == AF_INET && !(It->ifa_flags & IFF_LOOPBACK))
        {
            Result = reinterpret_cast<sockaddr_in*>(It->ifa_addr)->sin_addr;
            break;
        }
    }
    freeifaddrs(List);
    return Result;
}
