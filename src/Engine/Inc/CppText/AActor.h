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
