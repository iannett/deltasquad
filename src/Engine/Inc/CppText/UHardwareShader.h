    // 0x0009db87. the first texture's.
    INT MaterialUSize() override { return Textures[0] ? Textures[0]->MaterialUSize() : 0; }
    // 0x0009dba1
    INT MaterialVSize() override { return Textures[0] ? Textures[0]->MaterialVSize() : 0; }
