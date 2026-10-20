    // 0x000a8c4b
    ULevel();

    // 0x000a6459
    void Destroy() override;
    // 0x000a7384
    void PostLoad() override;
    // 0x000a8e6d
    void Serialize(FArchive& Ar) override;

    using UObject::Modify;
    // 0x000a5c1e
    virtual void Modify(UBOOL DoTransArrays = 0);
    // 0x000a6397
    virtual void SetActorCollision(UBOOL bCollision, UBOOL Unknown);
    // 0x00131c90
    virtual void Tick(ELevelTick TickType, FLOAT DeltaSeconds);
    // 0x001332cb
    virtual void TickNetClient(FLOAT DeltaSeconds);
    // 0x00133478
    virtual void TickNetServer(FLOAT DeltaSeconds);
    // 0x00131109
    virtual INT ServerTickClients(FLOAT DeltaSeconds);
    // 0x000a6b89
    virtual void ReconcileActors();
    // 0x000a5f39
    virtual void RememberActors();
    // 0x000a64c0
    virtual UBOOL Exec(const TCHAR* Cmd, FOutputDevice& Ar);
    // 0x000a5c16
    virtual void ShrinkLevel();
    // 0x0012cec5
    virtual void CompactActors();
    // 0x000a73fc
    virtual UBOOL Listen(FString& Error);
    // 0x000a5c32
    virtual UBOOL IsServer();
    // 0x000a67e4
    virtual UBOOL IsDedicatedServer();
    // 0x0012a005
    virtual UBOOL MoveActor(AActor* Actor, const FVector& Delta, const FRotator& NewRotation, FCheckResult& Hit,
        UBOOL Test = 0, UBOOL IgnorePawns = 0, UBOOL bIgnoreBases = 0, UBOOL bNoFail = 0, DWORD Unknown = 0);
    // 0x00129074
    virtual UBOOL FarMoveActor(AActor* Actor, const FVector& DestLocation, UBOOL Test = 0, UBOOL bNoCheck = 0,
        UBOOL bAttachedMove = 0);
    // 0x00129d5b
    virtual UBOOL DestroyActor(AActor* Actor, UBOOL bNetForce = 0);
    // 0x001284e7
    virtual void CleanupDestroyed(UBOOL bForce);
    // 0x0012caed
    virtual AActor* SpawnActor(UClass* Class, FName InName = NAME_None, const FVector& Location = FVector(0, 0, 0),
        FRotator Rotation = FRotator(0, 0, 0), AActor* Template = nullptr, UBOOL bNoCollisionFail = 0,
        UBOOL bRemoteOwned = 0, AActor* Owner = nullptr, APawn* Instigator = nullptr);
    // 0x00128849
    virtual ABrush* SpawnBrush();
    // 0x00128898
    virtual void SpawnViewActor(UViewport* Viewport);
    // 0x0012dbbf
    virtual APlayerController* SpawnPlayActor(UPlayer* Player, BYTE RemoteRole, const FURL& URL, FString& Error);
    // 0x00128d56
    virtual UBOOL FindSpot(const FVector& Extent, FVector& Location);
    // 0x00128a1b
    virtual UBOOL CheckSlice(FVector& Location, const FVector& Extent, INT& bKeepTrying);
    // 0x0012c094
    virtual UBOOL CheckEncroachment(AActor* Actor, const FVector& TestLocation, const FRotator& TestRotation,
        UBOOL bTouchNotify);
    // 0x0012859f
    virtual UBOOL SinglePointCheck(FCheckResult& Hit, const FVector& Location, const FVector& Extent,
        DWORD ExtraNodeFlags, ALevelInfo* Level, UBOOL bActors);
    // 0x0012868f
    virtual UBOOL EncroachingWorldGeometry(FCheckResult& Hit, const FVector& Location, const FVector& Extent,
        DWORD ExtraNodeFlags, ALevelInfo* Level);
    // 0x00129313
    virtual UBOOL SingleLineCheck(FCheckResult& Hit, AActor* SourceActor, const FVector& End, const FVector& Start,
        DWORD TraceFlags, const FVector& Extent = FVector(0, 0, 0));
    // 0x0012d08f
    virtual FCheckResult* MultiPointCheck(FMemStack& Mem, const FVector& Location, const FVector& Extent,
        DWORD ExtraNodeFlags, ALevelInfo* Level, UBOOL bActors, UBOOL Unknown1, UBOOL Unknown2);
    // 0x0012d353
    virtual FCheckResult* MultiLineCheck(FMemStack& Mem, const FVector& End, const FVector& Start,
        const FVector& Size, ALevelInfo* LevelInfo, DWORD ExtraNodeFlags, AActor* SourceActor);
    // 0x000a6331
    virtual void DetailChange(BYTE NewDetail);
    // 0x00131a24
    virtual INT TickDemoRecord(FLOAT DeltaSeconds);
    // 0x00131be5
    virtual INT TickDemoPlayback(FLOAT DeltaSeconds);
    // 0x000a5d5e
    virtual void UpdateTime(ALevelInfo* Info);
    // 0x00131c65
    virtual UBOOL IsPaused();
    // 0x000a6eea
    virtual void WelcomePlayer(UNetConnection* Connection, TCHAR* Optional = nullptr);
    // 0x000a8815
    virtual void BuildRenderData();
    // 0x000a5fc9
    virtual UBOOL IsAudibleAt(const FVector& SourceLocation, const FVector& ListenerLocation, AActor* Source);
    // 0x000a5da0
    virtual FLOAT CalculateRadiusMultiplier(INT Unknown1, INT Unknown2);

    // 0x000a6de2
    EAcceptConnection NotifyAcceptingConnection() override;
    // 0x000a6e7e
    void NotifyAcceptedConnection(UNetConnection* Connection) override;
    // 0x000a5c53
    UBOOL NotifyAcceptingChannel(UChannel* Channel) override;
    // 0x000a8cdf
    ULevel* NotifyGetLevel() override { return this; }
    // 0x000a7d4b
    void NotifyReceivedText(UNetConnection* Connection, const TCHAR* Text) override;
    // 0x000a5d1b
    UBOOL NotifySendingFile(UNetConnection* Connection, FGuid GUID) override;
    // 0x000a5d06
    void NotifyReceivedFile(UNetConnection* Connection, INT PackageIndex, const TCHAR* Error, UBOOL Skipped) override;

    ALevelInfo* GetLevelInfo() { return reinterpret_cast<ALevelInfo*>(Actors[0]); }

    UModel*                     Model;
    UTextBuffer*                TextBlocks[16];
    TMap<FString, FString>      TravelInfo;
    FCollisionHashBase*         Hash;
    UObject*                    UnknownD0;
    INT                         UnknownD4;
    INT                         UnknownD8;
    INT                         UnknownDC;
    INT                         UnknownE0;
    INT                         iFirstDynamicActor;
    INT                         iFirstNetRelevantActor;
    INT                         NetTag;
    // how many zone portals apart each pair of zones is; 255 when unconnected
    BYTE                        ZoneDist[64][64];
    TArray<FLevelUnknown10F0>   Unknown10F0;
    TArray<TArray<INT>>         Unknown10F8;
    TArray<TArray<INT>>         Unknown1100;
    // reference counted, with the count at +4
    TArray<void*>               Unknown1108;
    TArray<INT>                 Unknown1110;
    INT                         Unknown1118;
    INT                         Unknown111C;
    INT                         Unknown1120;
    INT                         Unknown1124;
    // karma objects made by the level's karma init (0x0008f45a)
    void*                       Unknown1128;
    void*                       Unknown112C;
    INT                         Unknown1130;
    void*                       Unknown1134;
    TArray<INT>                 Unknown1138;
    TArray<INT>                 Unknown1140;
    TArray<INT>                 Unknown1148;
    TMap<INT, INT>              Unknown1150;
    TMap<INT, INT>              Unknown1160;
