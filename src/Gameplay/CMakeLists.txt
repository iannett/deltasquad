add_library(Gameplay STATIC
    Src/GameplayClasses.cpp
    Src/GameplayNativeStubs.cpp
)
target_include_directories(Gameplay PUBLIC Inc)
target_link_libraries(Gameplay PUBLIC Engine)
