#include "EngineClasses.h"

namespace
{
    // fonts before version 122 keep their characters in pages, one texture each
    struct FFontPageCharacter
    {
        INT StartU;
        INT StartV;
        INT USize;
        INT VSize;

        // 0x000b17ef, the same code as FGuid's
        friend FArchive& operator<<(FArchive& Ar, FFontPageCharacter& Ch)
        {
            return Ar << Ch.StartU << Ch.StartV << Ch.USize << Ch.VSize;
        }
    };

    struct FFontPage
    {
        UTexture*                  Texture;
        TArray<FFontPageCharacter> Characters;

        // 0x000b2d88
        friend FArchive& operator<<(FArchive& Ar, FFontPage& Page)
        {
            return Ar << Page.Texture << Page.Characters;
        }
    };
}

// 0x000b2ea7
void UFont::Serialize(FArchive& Ar)
{
    Super::Serialize(Ar);

    // font textures are read right away
    const UBOOL SavedLazyLoad = GLazyLoad;
    GLazyLoad = 0;
    if (Ar.Ver() < 122)
    {
        TArray<FFontPage> Pages;
        INT CharactersPerPage;
        Ar << Pages;
        Ar << CharactersPerPage;
        for (INT i = 0; i < Pages.Num(); i++)
        {
            Textures.AddItem(Pages[i].Texture);
            for (INT j = 0; j < Pages[i].Characters.Num(); j++)
            {
                FFontCharacter& Ch = Characters[Characters.AddZeroed()];
                Ch.TextureIndex = static_cast<BYTE>(i);
                Ch.StartU = Pages[i].Characters[j].StartU;
                Ch.StartV = Pages[i].Characters[j].StartV;
                Ch.USize = Pages[i].Characters[j].USize;
                Ch.VSize = Pages[i].Characters[j].VSize;
            }
        }
    }
    else
    {
        Ar << Characters << Textures;
    }
    if (Ar.Ver() >= 119)
        Ar << Unknown4C;

    if (Ar.IsLoading() && !GLazyLoad)
    {
        for (INT i = 0; i < Textures.Num(); i++)
        {
            if (Textures[i])
            {
                for (INT j = 0; j < Textures[i]->Mips.Num(); j++)
                    Textures[i]->Mips[j].DataArray.Load();
            }
        }
    }
    GLazyLoad = SavedLazyLoad;

    if (Ar.Ver() < 69)
        IsRemapped = 0;
    else
        Ar << CharRemap << IsRemapped;
}
