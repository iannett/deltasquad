add_library(XGame STATIC
    Src/XGameClasses.cpp
    Src/XGameNativeStubs.cpp
)
target_include_directories(XGame PUBLIC Inc)
target_link_libraries(XGame PUBLIC Engine)
