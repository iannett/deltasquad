add_library(CTGame STATIC
    Src/CTGameClasses.cpp
    Src/CTGameNativeStubs.cpp
)
target_include_directories(CTGame PUBLIC Inc)
target_link_libraries(CTGame PUBLIC Gameplay)
