
    virtual void Init();
    // 0x00011074. does nothing.
    // 0x00074cc8. no limit.
    virtual FLOAT GetMaxTickRate() { return 0.f; }
    virtual void SetProgress(const TCHAR* Str1, const TCHAR* Str2, FLOAT Seconds) {}
