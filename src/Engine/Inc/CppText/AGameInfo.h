    // 0x001217a9
    void eventInitGame(const FString& Options, FString& Error)
    {
        static FName NAME_InitGame(TEXT("InitGame"), FNAME_Intrinsic);
        struct
        {
            FString Options;
            FString Error;
        } Parms{Options, Error};
        ProcessEvent(FindFunctionChecked(NAME_InitGame), &Parms);
        Error = Parms.Error;
    }
    // 0x00121834
    void eventBroadcast(AActor* Sender, const FString& Msg, FName Type)
    {
        static FName NAME_Broadcast(TEXT("Broadcast"), FNAME_Intrinsic);
        struct
        {
            AActor* Sender;
            FString Msg;
            FName   Type;
        } Parms{Sender, Msg, Type};
        ProcessEvent(FindFunctionChecked(NAME_Broadcast), &Parms);
    }
    // 0x000a5f02
    void eventDetailChange()
    {
        static FName NAME_DetailChange(TEXT("DetailChange"), FNAME_Intrinsic);
        ProcessEvent(FindFunctionChecked(NAME_DetailChange), nullptr);
    }
