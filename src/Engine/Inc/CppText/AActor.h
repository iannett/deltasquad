    void Serialize(FArchive& Ar) override;
    // 0x001052f3. events only run once the level has begun play, and never while garbage is collected.
    void ProcessEvent(UFunction* Function, void* Parms, void* Result = nullptr) override;
    // 0x0007e27c. the collision model karma made for the actor.
    virtual McdModel* getKModel() const;
    // 0x001328d2. ticks the actor's owner and base first, then its animation, script, timers and physics.
    virtual UBOOL Tick(FLOAT DeltaTime, ELevelTick TickType);
    // 0x000e46af. runs the latent action and the state code.
    void ProcessState(FLOAT DeltaSeconds) override;
    // 0x00105279
    virtual void UpdateAnimation(FLOAT DeltaSeconds);
    // 0x0013283c. the Tick event, state code, timers, life span and physics.
    virtual void TickAuthoritative(FLOAT DeltaSeconds);
    // 0x00130661
    virtual void TickSimulated(FLOAT DeltaSeconds) { TickAuthoritative(DeltaSeconds); }
    // 0x0005ec28. does nothing.
    virtual void TickSpecial(FLOAT DeltaSeconds) {}
    // 0x000afd8f
    virtual UBOOL PlayerControlled() { return 0; }
    // 0x00116acc
    virtual void performPhysics(FLOAT DeltaSeconds);
    // 0x0002ff90. the player controller at the top of the owner chain.
    virtual APlayerController* GetTopPlayerController();
    // 0x000df6f2. the latent half of Sleep: counts LatentFloat down.
    DECLARE_FUNCTION(execPollSleep)
    // 0x001327bf. counts TimerCounter up to TimerRate and sends Timer.
    void UpdateTimers(FLOAT DeltaSeconds);
    // 0x001321c8
    void eventTick(FLOAT DeltaTime)
    {
        if (IsProbing(FName(NAME_Tick)))
        {
            struct { FLOAT DeltaTime; } Parms{DeltaTime};
            ProcessEvent(FindFunctionChecked(FName(NAME_Tick)), &Parms);
        }
    }
    // 0x00132244
    void eventTimer()
    {
        if (IsProbing(FName(NAME_Timer)))
            ProcessEvent(FindFunctionChecked(FName(NAME_Timer)), nullptr);
    }
    // 0x00052bf0. CT's name; called by DestroyActor after the Destroyed event.
    virtual void PostScriptDestroyed() {}
    // 0x0012962d
    void eventDestroyed()
    {
        if (IsProbing(FName(NAME_Destroyed)))
            ProcessEvent(FindFunctionChecked(FName(NAME_Destroyed)), nullptr);
    }
    // 0x000bd7d6
    void eventEndState() { ProcessEvent(FindFunctionChecked(FName(NAME_EndState)), nullptr); }
    // 0x00052bf0. CT's name; called on every actor while destroyed actors are deleted.
    virtual void FixupSubObjectPointers() {}
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
    // 0x0002f064. scales by DrawScale and DrawScale3D about PrePivot, rotates and moves to Location.
    virtual FMatrix LocalToWorld() const;
    // 0x0002f358. the inverse of LocalToWorld.
    virtual FMatrix WorldToLocal() const;
    // 0x00105313. whether Other is this actor, one of its bases, or shares a joined tag with one.
    UBOOL IsJoinedTo(const AActor* Other) const;
    // 0x00109c08. whether Other stops this actor's movement.
    UBOOL IsBlockedBy(const AActor* Other) const;
    // 0x0010983e. whether the two actors' collision overlaps; Hit gets the primitive check's result.
    UBOOL IsOverlapping(AActor* Other, FCheckResult* Hit = nullptr);
    // 0x0010c3b7. each actor touches the other, unless the first touch event ends it.
    void BeginTouch(AActor* Other);
    // 0x0010bed2
    void EndTouch(AActor* Other, UBOOL bNoNotifySelf);
    // 0x001109e0. moves by Delta, sliding along what it hits.
    UBOOL moveSmooth(const FVector& Delta);
    // 0x000f0526
    void eventTouch(AActor* Other)
    {
        if (IsProbing(FName(NAME_Touch)))
        {
            struct { AActor* Other; } Parms{Other};
            ProcessEvent(FindFunctionChecked(FName(NAME_Touch)), &Parms);
        }
    }
    // 0x0010b43d
    void eventUnTouch(AActor* Other)
    {
        if (IsProbing(FName(NAME_UnTouch)))
        {
            struct { AActor* Other; } Parms{Other};
            ProcessEvent(FindFunctionChecked(FName(NAME_UnTouch)), &Parms);
        }
    }
    // 0x00129787
    UBOOL eventEncroachingOn(AActor* Other)
    {
        struct { AActor* Other; UBOOL ReturnValue; } Parms{Other, 0};
        if (IsProbing(FName(NAME_EncroachingOn)))
            ProcessEvent(FindFunctionChecked(FName(NAME_EncroachingOn)), &Parms);
        return Parms.ReturnValue;
    }
    // 0x0012980c
    void eventEncroachedBy(AActor* Other)
    {
        if (IsProbing(FName(NAME_EncroachedBy)))
        {
            struct { AActor* Other; } Parms{Other};
            ProcessEvent(FindFunctionChecked(FName(NAME_EncroachedBy)), &Parms);
        }
    }
    // 0x0012c71b. finds the zone and volume the actor is in.
    virtual void SetZone(UBOOL bTest, UBOOL bForceRefresh);
    // 0x0012cf50. touches the volumes the actor starts inside.
    virtual void SetVolumes();
    // 0x00052bf0. native follow-up to the PostBeginPlay event; does nothing here.
    virtual void PostBeginPlay() {}
    // 0x00128721. replays the replicated animation on each SimAnim channel that has one. SpawnActor calls it
    // before PostBeginPlay.
    virtual void Unknown14C();
    // 0x0010b687. CT's name. plays SimAnim[Channel] on the mesh instance.
    void PlayReplicatedAnim(INT Channel);
    // 0x0010c9c0
    virtual void SetBase(AActor* NewBase, const FVector& NewFloor, UBOOL bNotifyActor);
    // 0x00110e8e. bases the actor on what is below it.
    void FindBase();
    // 0x00052bf0. called by SpawnActor before the actor's first events.
    virtual void Spawned() {}
    // 0x0010ba9d
    void SetOwner(AActor* NewOwner);
    // 0x0010df4b. CT's name. touches what it newly overlaps and leaves what it stops colliding with.
    void SetCollision(UBOOL NewCollideActors, UBOOL NewBlockActors, UBOOL NewBlockPlayers);
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
