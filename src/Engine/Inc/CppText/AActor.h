    // 0x00070cad. the pawn when a player controls it.
    virtual APawn* GetPlayerPawn() const { return nullptr; }
    // 0x00109ea9. whether a trace with these flags from SourceActor can hit this actor.
    virtual UBOOL ShouldTrace(AActor* SourceActor, DWORD TraceFlags);
    // 0x00109bbb. the mesh, static mesh or anti portal, else the engine's cylinder.
    virtual UPrimitive* GetPrimitive();
    // 0x00070cad, the class overrides return 1
    virtual UBOOL IsABrush() { return 0; }
    virtual UBOOL IsAMover() { return 0; }
    virtual UBOOL IsAVolume() { return 0; }
    virtual UBOOL IsAPlayerController() { return 0; }
    virtual UBOOL IsAPawn() { return 0; }
    virtual UBOOL IsAProjectile() { return 0; }
    // a mover or a karma body, which other actors may let pass
    UBOOL IsEncroacher();
    // 0x0012c71b. finds the zone and volume the actor is in.
    virtual void SetZone(UBOOL bTest, UBOOL bForceRefresh);
    // 0x0012cf50. touches the volumes the actor starts inside.
    virtual void SetVolumes();
    // 0x00052bf0. native follow-up to the PostBeginPlay event; does nothing here.
    virtual void PostBeginPlay() {}
    // 0x00128721. walks six timer slots at 0x23c when 0xb0 is set; its purpose is not known yet.
    virtual void Unknown14C();
    // 0x0010c9c0
    virtual void SetBase(AActor* NewBase, const FVector& NewFloor, UBOOL bNotifyActor);
    // 0x00110e8e. bases the actor on what is below it.
    void FindBase();
    // 0x00052bf0. called by SpawnActor before the actor's first events.
    virtual void Spawned() {}
    // 0x0010ba9d
    void SetOwner(AActor* NewOwner);
    // 0x0010bf4f. also takes an anti portal actor out of its zones' lists.
    void ClearRenderData();

    // 0x00129697
    void eventZoneChange(AZoneInfo* NewZone)
    {
        if (IsProbing(FName(NAME_ZoneChange)))
        {
            struct { AZoneInfo* NewZone; } Parms{NewZone};
            ProcessEvent(FindFunctionChecked(FName(NAME_ZoneChange)), &Parms);
        }
    }
    // 0x0012970f
    void eventPhysicsVolumeChange(APhysicsVolume* NewVolume)
    {
        if (IsProbing(FName(NAME_PhysicsVolumeChange)))
        {
            struct { APhysicsVolume* NewVolume; } Parms{NewVolume};
            ProcessEvent(FindFunctionChecked(FName(NAME_PhysicsVolumeChange)), &Parms);
        }
    }
    // 0x0010b34d
    void eventGainedChild(AActor* Other)
    {
        if (IsProbing(FName(NAME_GainedChild)))
        {
            struct { AActor* Other; } Parms{Other};
            ProcessEvent(FindFunctionChecked(FName(NAME_GainedChild)), &Parms);
        }
    }
    // 0x0010b3c5
    void eventLostChild(AActor* Other)
    {
        if (IsProbing(FName(NAME_LostChild)))
        {
            struct { AActor* Other; } Parms{Other};
            ProcessEvent(FindFunctionChecked(FName(NAME_LostChild)), &Parms);
        }
    }

    // 0x0011ecd5
    void eventPreBeginPlay()
    {
        if (IsProbing(FName(NAME_PreBeginPlay)))
            ProcessEvent(FindFunctionChecked(FName(NAME_PreBeginPlay)), nullptr);
    }
    // 0x0011ec6b
    void eventBeginPlay()
    {
        if (IsProbing(FName(NAME_BeginPlay)))
            ProcessEvent(FindFunctionChecked(FName(NAME_BeginPlay)), nullptr);
    }
    // 0x0011ed3f
    void eventPostBeginPlay()
    {
        if (IsProbing(FName(NAME_PostBeginPlay)))
            ProcessEvent(FindFunctionChecked(FName(NAME_PostBeginPlay)), nullptr);
    }
    // 0x0011eda9
    void eventPostLoadBeginPlay()
    {
        if (IsProbing(FName(NAME_PostLoadBeginPlay)))
            ProcessEvent(FindFunctionChecked(FName(NAME_PostLoadBeginPlay)), nullptr);
    }
    // 0x00052f20
    void eventPostNetBeginPlay()
    {
        static FName NAME_PostNetBeginPlay(TEXT("PostNetBeginPlay"), FNAME_Intrinsic);
        ProcessEvent(FindFunctionChecked(NAME_PostNetBeginPlay), nullptr);
    }
    // 0x0011dafb
    void eventSetInitialState()
    {
        static FName NAME_SetInitialState(TEXT("SetInitialState"), FNAME_Intrinsic);
        ProcessEvent(FindFunctionChecked(NAME_SetInitialState), nullptr);
    }
