    // 0x00032743
    UBOOL IsAPlayerController() override { return 1; }
    // 0x00100860. frees the input and cheat objects.
    void PostScriptDestroyed() override;
    // 0x000fdc07. whether a local viewport plays this controller.
    UBOOL LocalPlayerController() override;
