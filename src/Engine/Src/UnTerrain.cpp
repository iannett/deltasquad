#include "EngineClasses.h"

// ---------------------------------------------------------------------------------------------- serializers

// 0x00062166
static FArchive& operator<<(FArchive& Ar, FDecoInfo& D)
{
    return Ar << D.Location << D.Rotation << D.Scale << D.TempScale << D.Color << D.Distance;
}

// 0x0006952e
static FArchive& operator<<(FArchive& Ar, FDecoSectorInfo& D)
{
    return Ar << D.DecoInfo << D.Location << D.Radius;
}

// 0x00069cde
static FArchive& operator<<(FArchive& Ar, FDecorationLayerData& D)
{
    return Ar << D.Sectors;
}

// 0x00062142
static FArchive& operator<<(FArchive& Ar, FNormalPair& N)
{
    return Ar << N.Normal1 << N.Normal2;
}

// ---------------------------------------------------------------------------------------------- terrain sector

// 0x0006b6b9
void UTerrainSector::Serialize(FArchive& Ar)
{
    Super::Serialize(Ar);
    if (Ar.Ver() < 94)
    {
        FBox OldBox;
        INT  Unused[4];
        Ar << OldBox;
        Ar.Serialize(Unused, sizeof(Unused));
    }
    Ar << Info << Unknown58 << Unknown5C << Unknown60 << Unknown64;
    if (Ar.Ver() < 117)
    {
        // the eight corners of the old box
        BoundingBox = FBox();
        for (INT i = 0; i < 8; i++)
        {
            FVector Corner;
            Ar << Corner;
            BoundingBox += Corner;
        }
    }
    else
    {
        Ar << BoundingBox;
    }
    Ar << Unknown30;
    if (Ar.Ver() < 89)
    {
        UObject* OldObject = nullptr;
        Ar << OldObject;
    }
    if (Ar.Ver() < 117)
    {
        FRawColorStream OldColors;
        Ar << OldColors;
    }
    // the rest is rebuilt after loading and only counted
    if (Ar.IsLoading())
        Unknown80 = INDEX_NONE;
    if (!Ar.IsLoading() && !Ar.IsSaving())
    {
        Ar << Unknown88 << UnknownA0 << UnknownA4 << UnknownA8 << UnknownAC;
        Ar << Projectors << Unknown28 << Unknown80;
    }
}

// 0x0006b831. drops the entries whose object didn't load.
void UTerrainSector::PostLoad()
{
    Super::PostLoad();
    for (INT i = 0; i < Unknown30.Num(); )
    {
        if (Unknown30[i].Unknown0)
            i++;
        else
            Unknown30.Remove(i);
    }
}

// ---------------------------------------------------------------------------------------------- terrain info

// 0x000701eb
void ATerrainInfo::Serialize(FArchive& Ar)
{
    Super::Serialize(Ar);
    Ar << Sectors << Vertices << SectorsX << SectorsY << FaceNormals;
    if (Ar.Ver() < 83)
    {
        TArray<FQuat> OldQuats;
        Ar << OldQuats;
    }
    Ar.Serialize(ToWorld, sizeof(ToWorld));
    Ar.Serialize(ToHeightmap, sizeof(ToHeightmap));
    if (Ar.Ver() < 76)
        Ar << OldHeightmap;
    Ar << HeightmapX << HeightmapY;
    if (Ar.Ver() > 74 && Ar.Ver() < 82)
    {
        // sixteen old layer records
        for (INT i = 0; i < 16; i++)
        {
            INT     Unused;
            FString UnusedString;
            for (INT j = 0; j < 7; j++)
                Ar << Unused;
            Ar << UnusedString;
        }
    }
    if (Ar.IsLoading())
        OldTerrainMap = TerrainMap;
    if (Ar.Ver() == 86 || Ar.Ver() == 87)
        Ar << QuadVisibilityBitmap;
    if (Ar.Ver() < 115)
    {
        for (INT i = 0; i < DecoLayers.Num(); i++)
        {
            DecoLayers[i].LitDirectional = 0;
            DecoLayers[i].DisregardTerrainLighting = 0;
        }
    }
    if (Ar.Ver() < 119)
    {
        for (INT i = 0; i < DecoLayers.Num(); i++)
            DecoLayers[i].RandomYaw = 0;
    }
    if (Ar.Ver() >= 117)
        Ar << VertexColors;
    if (!Ar.IsSaving() && !Ar.IsLoading())
    {
        Ar << SelectedVertices << DecoLayerData << ShowGrid << OldTerrainMap << Projectors << Primitive;
        Ar << VertexStreams << PaintedColor << RenderCombinations << CollapsedLayers;
    }
}
