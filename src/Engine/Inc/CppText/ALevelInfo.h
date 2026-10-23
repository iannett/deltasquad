    // 0x001293dc. spawns the level's default physics volume the first time.
    APhysicsVolume* GetDefaultPhysicsVolume();
    // 0x000a69a5. CT's name. whether the client has more than one viewport.
    UBOOL IsSplitScreen() const;
    // 0x000a70a9. CT's name. whether the game engine's last url has the LAN option.
    UBOOL IsSystemLink();
    // 0x000a5e13. CT's name.
    void DeterminePlatform() { bIsConsole = 1; }
    // 0x00129484. the highest priority volume at the location, from the actor's touch list or the collision hash.
    APhysicsVolume* GetPhysicsVolume(const FVector& Loc, AActor* A, UBOOL bUseTouch);
    // 0x001218a4
    void eventServerTravel(const FString& URL, UBOOL bItems)
    {
        static FName NAME_ServerTravel(TEXT("ServerTravel"), FNAME_Intrinsic);
        struct
        {
            FString URL;
            UBOOL   bItems;
        } Parms{URL, bItems};
        ProcessEvent(FindFunctionChecked(NAME_ServerTravel), &Parms);
    }
    // 0x001286f0. the level is its own zone.
    void SetZone(UBOOL bTest, UBOOL bForceRefresh) override;
