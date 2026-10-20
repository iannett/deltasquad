    TArray<UNetConnection*> ClientConnections;
    UNetConnection*         ServerConnection;
    FNetworkNotify*         Notify;
    UPackageMap*            MasterMap;
    DOUBLE                  Time;
    FLOAT                   ConnectionTimeout;
    FLOAT                   InitialConnectTimeout;
    FLOAT                   KeepAliveTime;
    FLOAT                   RelevantTimeout;
    FLOAT                   SpawnPrioritySeconds;
    FLOAT                   ServerTravelPause;
    INT                     MaxClientRate;
    INT                     MaxInternetClientRate;
    INT                     NetServerMaxTickRate;
    INT                     LanServerMaxTickRate;
    UBOOL                   AllowDownloads;
    // set by the profilestats command line switch
    UBOOL                   ProfileStats;
    UProperty*              RoleProperty;
    UProperty*              RemoteRoleProperty;
    INT                     Unknown80;
    INT                     Unknown84;
    INT                     MaxDownloadSize;
    TArray<FString>         DownloadManagers;

    // 0x000561f2
    UNetDriver();
    // 0x00053554. the config properties are declared here instead of in script.
    void StaticConstructor();

    // 0x00053937
    void Destroy() override;
    // 0x00054c61
    void Serialize(FArchive& Ar) override;
    // 0x000543eb
    UBOOL Exec(const TCHAR* Cmd, FOutputDevice& Ar) override;

    virtual void LowLevelDestroy() = 0;
    virtual FString LowLevelGetNetworkNumber() = 0;
    // 0x00052bf0
    virtual void AssertValid() {}
    // 0x00052d83, shared with InitListen
    virtual UBOOL InitConnect(FNetworkNotify* InNotify, FURL& ConnectURL, FString& Error);
    // 0x00052d83
    virtual UBOOL InitListen(FNetworkNotify* InNotify, FURL& ListenURL, FString& Error);
    // 0x000538ad
    virtual void TickFlush();
    // 0x000538ec
    virtual void TickDispatch(FLOAT DeltaTime);
    // 0x000552b7
    virtual void NotifyActorDestroyed(AActor* Actor);
    // 0x00070cad
    virtual UBOOL IsDemoDriver() { return 0; }
    // 0x00052bf0
    virtual void PreServerTravel() {}

    // 0x00052d76. 2 while hosting or joining a system link game.
    static INT GetNetworkState() { return NetworkState; }

private:
    // 0x0045fd88
    static INT NetworkState;
