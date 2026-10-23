add_library(IpDrv STATIC
    Src/IpDrvClasses.cpp
    Src/IpDrvNativeStubs.cpp
    Src/InternetLink.cpp
    Src/TcpNetDriver.cpp
    Src/UnSocket.cpp
)
target_include_directories(IpDrv PUBLIC Inc)
target_link_libraries(IpDrv PUBLIC Engine)
