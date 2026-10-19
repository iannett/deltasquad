add_library(XInterface STATIC
    Src/XInterfaceClasses.cpp
    Src/XInterfaceNativeStubs.cpp
)
target_include_directories(XInterface PUBLIC Inc)
target_link_libraries(XInterface PUBLIC Engine)
