add_library(Engine STATIC
    Src/EngineClasses.cpp
)
target_include_directories(Engine PUBLIC Inc)
target_link_libraries(Engine PUBLIC Core)
