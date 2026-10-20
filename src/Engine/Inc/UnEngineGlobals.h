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

    // 0x00199041
    void Reset()
    {
        Unknown18.Empty();
        Unknown8.Empty();
    }

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

// a memory budget and what has been counted against it
struct FMemBudget
{
    INT Budget;
    INT Unknown4;
    INT Unknown8;
    INT UnknownC;
};

// memory and frame stats for the on-screen display. only the memory budgets are decompiled so far.
class FEngineStats
{
public:
    // 0x00169cb3
    void Init() { InitMemBudgets(); }
    // 0x00168b27. splits the xbox's 64 MB between the budgets.
    void InitMemBudgets();
    // 0x00168a57
    void UpdateMemTotals();

    INT        TotalMemory;
    INT        Unknown4[9];
    FMemBudget Unknown28;
    FMemBudget Unknown38[13];
    FMemBudget Unknown108;
    FMemBudget Unknown118[8];
    INT        Unknown198;
    FMemBudget Unknown19C;
    INT        Unknown1AC;
    INT        Unknown1B0;
};

// 0x004a9508
extern FEngineStats        GEngineStats;
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
// 0x0045a768. set while LoadMap runs.
extern UBOOL               GIsLoadingLevel;
// 0x0045a724. CT's name; nothing sets it on the server.
extern UBOOL               GUseFrontEnd;
