    // members are left alone, as for every struct inside an object
    FURL() = default;
    // 0x000a9afb. the default url, for LocalFilename's map when given.
    explicit FURL(const TCHAR* LocalFilename);
    // 0x000a9cd8. parses TextURL; an unparsable url becomes the default one with Valid cleared.
    FURL(const FURL* Base, const TCHAR* TextURL, ETravelType Type);
    FURL(const FURL& Other);
    // 0x00058f28
    FURL& operator=(const FURL& Other);

    // 0x000a95bc. served by this game rather than another protocol.
    UBOOL IsInternal() const;
    // 0x000a95ca. served by this game on this machine.
    UBOOL IsLocalInternal() const;
    // 0x000a9688
    UBOOL HasOption(const TCHAR* Test) const;
    // 0x000a9b81. leaves out the parts that match the defaults unless FullyQualified.
    FString String(UBOOL FullyQualified = 0) const;
    // 0x000a9947. replaces an option with the same key.
    void AddOption(const TCHAR* Str);
    // 0x000a96d9. the text after Match in the first option starting with it.
    const TCHAR* GetOption(const TCHAR* Match, const TCHAR* Default) const;
    // 0x000a9aa4. adds every line of an ini section as an option.
    void LoadURLConfig(const TCHAR* Section, const TCHAR* Filename = nullptr);
    // 0x000a95f1. writes the option named Key, if present, to an ini section.
    void SaveURLConfig(const TCHAR* Section, const TCHAR* Key, const TCHAR* Filename = nullptr) const;
    // 0x000a9a10. removes the first option whose key matches, up to and including any '=' in Key.
    void RemoveOption(const TCHAR* Key);

    // read from the [URL] section of the game ini by StaticInit
    static FString DefaultProtocol;
    static FString DefaultProtocolDescription;
    static FString DefaultName;
    static FString DefaultMap;
    static FString DefaultLocalMap;
    static FString DefaultHost;
    static FString DefaultPortal;
    static FString DefaultMapExt;
    static FString DefaultSaveExt;
    static INT     DefaultPort;

    static void StaticInit();

    // 0x000a975c
    friend FArchive& operator<<(FArchive& Ar, FURL& U);
