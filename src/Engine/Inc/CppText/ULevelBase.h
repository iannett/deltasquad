    // 0x00058f79
    ULevelBase();

    // 0x000a5bcb
    void Destroy() override;
    // 0x000a78a5
    void Serialize(FArchive& Ar) override;

    // 0x000a5bf8
    void NotifyProgress(const TCHAR* Str1, const TCHAR* Str2, FLOAT Seconds) override;

    // set to the outer when loaded as null. LoadMap renames the outer's objects into it when they differ.
    UObject*               Unknown2C;
    TTransArray<AActor*>   Actors;
    UNetDriver*            NetDriver;
    UEngine*               Engine;
    FURL                   URL;
    UNetDriver*            DemoRecDriver;
