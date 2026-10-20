add_library(Karma STATIC
    Src/McdFrame.cpp
    Src/McdTypes.cpp
    Src/MeAssetDB.cpp
    Src/MeAssetDBXML.cpp
    Src/MeFAsset.cpp
    Src/MeMemory.cpp
    Src/MeMessage.cpp
    Src/MeXML.cpp
)
target_include_directories(Karma PUBLIC Inc)
target_link_libraries(Karma PUBLIC Core)
