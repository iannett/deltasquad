add_library(Core STATIC
    Src/Core.cpp
    Src/UnArc.cpp
    Src/UnBits.cpp
    Src/UnCache.cpp
    Src/FConfigCacheIni.cpp
    Src/FileManagerPosix.cpp
    Src/UnClass.cpp
    Src/CoreStructLayouts.cpp
    Src/UnCommandlet.cpp
    Src/UnLinker.cpp
    Src/UnMath.cpp
    Src/UnMem.cpp
    Src/UnMisc.cpp
    Src/UnName.cpp
    Src/UnObj.cpp
    Src/UnProp.cpp
    Src/UnScript.cpp
    Src/UnString.cpp
    Src/UnSystem.cpp
)
target_include_directories(Core PUBLIC Inc)
