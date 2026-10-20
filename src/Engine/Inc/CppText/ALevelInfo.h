    // 0x001293dc. spawns the level's default physics volume the first time.
    APhysicsVolume* GetDefaultPhysicsVolume();
    // 0x000a5e13. CT's name.
    void DeterminePlatform() { bIsConsole = 1; }
    // 0x00129484. the highest priority volume at the location, from the actor's touch list or the collision hash.
    APhysicsVolume* GetPhysicsVolume(const FVector& Loc, AActor* A, UBOOL bUseTouch);
