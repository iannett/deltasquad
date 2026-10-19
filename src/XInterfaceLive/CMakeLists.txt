add_library(XInterfaceLive STATIC
    Src/XInterfaceLiveClasses.cpp
    Src/XInterfaceLiveNativeStubs.cpp
)
target_include_directories(XInterfaceLive PUBLIC Inc)
target_link_libraries(XInterfaceLive PUBLIC XInterface)
