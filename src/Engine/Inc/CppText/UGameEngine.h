
    void Init() override;
    // 0x001266cb. ticks the level and the client, and travels when the level asks to.
    void Tick(FLOAT DeltaSeconds) override;
    // 0x0011eb07. the server's tick rate limit from its net driver's settings.
    FLOAT GetMaxTickRate() override;
    // 0x00121d03
    void SetProgress(const TCHAR* Str1, const TCHAR* Str2, FLOAT Seconds) override;
    // 0x00122c5b
    virtual INT Browse(const FURL& URL, const TMap<FString, FString>* TravelInfo, FString& Error);
    // 0x0011f1ac. leaves the entry level as the current one.
    virtual void UnloadCurrentMap();
    // 0x00124114
    virtual ULevel* LoadMap(const FURL& URL, UPendingLevel* Pending, const TMap<FString, FString>* TravelInfo, FString& Error, UBOOL Unknown);
    // 0x0011e5aa. the master map holds the level's package and the server packages.
    virtual void BuildServerMasterMap(UNetDriver* NetDriver, ULevel* InLevel);
    // 0x0011e2f0. redraws the loading screen.
    virtual void UpdateProgress();
