    TArray<FFontCharacter> Characters;
    TArray<UTexture*>      Textures;
    TMap<_WORD, _WORD>     CharRemap;
    UBOOL                  IsRemapped;
    INT                    Unknown4C; // read from version 119 on. nothing else found using it.

    void Serialize(FArchive& Ar) override;
