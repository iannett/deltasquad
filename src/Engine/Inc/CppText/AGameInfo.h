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
