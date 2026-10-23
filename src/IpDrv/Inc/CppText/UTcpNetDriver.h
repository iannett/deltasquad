    UBOOL       AllowPlayerPortUnreach;
    UBOOL       LogPortUnreach;
    INT         MaxConnPerIPPerMinute;
    UBOOL       LogMaxConnPerIPPerMin;
    sockaddr_in LocalAddr;
    SOCKET      Socket;
    // cleared by InitBase; the system link code uses it
    INT         UnknownB8;
    UBOOL       StoredHostInfo;
    FString     StoredHostSessionID;
    FString     StoredHostExchangeKey;

    // 0x0022bc96
    UTcpNetDriver() = default;
    // 0x0022b228
    void StaticConstructor();

    // 0x0022cf39
    UBOOL Exec(const TCHAR* Cmd, FOutputDevice& Ar) override;

    // 0x0022b4f5
    void LowLevelDestroy() override;
    // 0x0022ba5a
    FString LowLevelGetNetworkNumber() override;
    // 0x0022c527
    UBOOL InitConnect(FNetworkNotify* InNotify, FURL& ConnectURL, FString& Error) override;
    // 0x0022c79d
    UBOOL InitListen(FNetworkNotify* InNotify, FURL& ListenURL, FString& Error) override;
    // 0x0022c9a0
    void TickDispatch(FLOAT DeltaTime) override;
    // 0x0022cf22
    void PreServerTravel() override;

    // 0x0022bd1b. opens and binds the socket; clients bind the first free port from 2000.
    UBOOL InitBase(UBOOL Connect, FNetworkNotify* InNotify, FURL& URL, FString& Error);
