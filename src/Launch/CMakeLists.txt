add_executable(deltasquad Src/Launch.cpp)
# whole archives, so every module's classes register even when nothing references them directly
target_link_libraries(deltasquad PRIVATE "$<LINK_LIBRARY:WHOLE_ARCHIVE,Core,Engine,Gameplay,CTGame,CTMarkers,IpDrv,XGame,XInterface,XInterfaceLive>")
