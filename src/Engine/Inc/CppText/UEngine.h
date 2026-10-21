
    virtual void Init();
    // 0x000a424a. the subsystems' commands, then STAT, FLUSH, CRACKURL and TVSAFEZONE.
    UBOOL Exec(const TCHAR* Cmd, FOutputDevice& Ar) override;
    // 0x000a3afc. empties the cache and the client's caches.
    virtual void Flush(UBOOL AllowPrecache);
    // 0x0005ec28. does nothing.
    virtual void Tick(FLOAT DeltaSeconds) {}
    // 0x00011074. does nothing.
    // 0x00074cc8. no limit.
    virtual FLOAT GetMaxTickRate() { return 0.f; }
    virtual void SetProgress(const TCHAR* Str1, const TCHAR* Str2, FLOAT Seconds) {}
