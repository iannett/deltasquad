add_library(CTMarkers STATIC
    Src/CTMarkersClasses.cpp
    Src/CTMarkersNativeStubs.cpp
)
target_include_directories(CTMarkers PUBLIC Inc)
target_link_libraries(CTMarkers PUBLIC CTGame)
