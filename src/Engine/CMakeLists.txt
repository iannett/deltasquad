add_library(Engine STATIC
    Src/EngineClasses.cpp
    Src/EngineNativeStubs.cpp
    Src/UnAudio.cpp
    Src/UnEngine.cpp
    Src/UnFont.cpp
    Src/UnKarma.cpp
    Src/UnModel.cpp
    Src/UnURL.cpp
    Src/UnRenderResource.cpp
    Src/UnSkeletalMesh.cpp
    Src/UnStaticMesh.cpp
    Src/UnTex.cpp
)
target_include_directories(Engine PUBLIC Inc)
target_link_libraries(Engine PUBLIC Core)
