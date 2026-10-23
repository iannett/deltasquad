add_library(Karma STATIC
    Src/McdFrame.cpp
    Src/McdModel.cpp
    Src/McdTypes.cpp
    Src/MdtBody.cpp
    Src/MdtConstraint.cpp
    Src/MdtWorld.cpp
    Src/MeAssetInstance.cpp
    Src/MeAssetDB.cpp
    Src/MeAssetDBXML.cpp
    Src/MeDict.cpp
    Src/MeFAsset.cpp
    Src/MeMath.cpp
    Src/MeMemory.cpp
    Src/MeMessage.cpp
    Src/MeXML.cpp
    Src/MstBridge.cpp
)
target_include_directories(Karma PUBLIC Inc)
target_link_libraries(Karma PUBLIC Core)
