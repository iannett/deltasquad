add_library(IpDrv STATIC
    Src/IpDrvClasses.cpp
    Src/IpDrvNativeStubs.cpp
)
target_include_directories(IpDrv PUBLIC Inc)
target_link_libraries(IpDrv PUBLIC Engine)
