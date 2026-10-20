#pragma once

#include "Core.h"
#include "UnMath.h"

class UEngine;
class UCubemapManager;
class UGlobalTempObjects;

// a value of the second map in FStatGraph; its fields are not known yet
struct FStatGraphUnknown5C
{
    INT Unknown0;
    INT Unknown4;
    INT Unknown8;
};

// graphs of named stats drawn over the game view
class FStatGraph
{
public:
    // 0x00198f0a
    FStatGraph()
        : Unknown0(1), Unknown4(0), Unknown28(250.f), Unknown2C(150.f), Unknown30(100.f), Unknown34(200.f)
        , Unknown38(100), Unknown3C(0), Unknown40(0.33f), Unknown44(100), Unknown50(0)
    {}

    INT                                  Unknown0;
    INT                                  Unknown4;
    TMap<FString, INT>                   Unknown8;
    TArray<INT>                          Unknown18;
    TArray<INT>                          Unknown20;
    FLOAT                                Unknown28;
    FLOAT                                Unknown2C;
    FLOAT                                Unknown30;
    FLOAT                                Unknown34;
    INT                                  Unknown38;
    INT                                  Unknown3C;
    FLOAT                                Unknown40;
    BYTE                                 Unknown44;
    TArray<INT>                          Unknown48;
    INT                                  Unknown50;
    TArray<INT>                          Unknown54;
    TMap<FString, FStatGraphUnknown5C>   Unknown5C;
    TArray<INT>                          Unknown6C;
};

// debug lines and boxes drawn for one frame
class FTempLineBatcher
{
public:
    // 0x000a3f6b
    FTempLineBatcher() = default;

    TArray<FVector> LineStart;
    TArray<FVector> LineEnd;
    TArray<FColor>  LineColor;
    TArray<FBox>    BoxArray;
    TArray<FColor>  BoxColor;
    // likely the lines AddStayingLine keeps between frames
    TArray<FVector> Unknown28;
    TArray<FVector> Unknown30;
    TArray<FColor>  Unknown38;
};

// 0x00462100
extern FMemStack           GEngineMem;
// 0x00462120
extern FMemCache           GCache;
// 0x00462118
extern UEngine*            GEngine;
// 0x0046211c
extern UCubemapManager*    GCubemapManager;
// 0x004ab4b4
extern FStatGraph*         GStatGraph;
// 0x0049f8c8
extern FTempLineBatcher*   GTempLineBatcher;
// 0x00477700
extern UGlobalTempObjects* GGlobalTempObjects;
