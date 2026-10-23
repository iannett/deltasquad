#include "Core.h"
#include "UnSystem.h"

USystem* GSys;

// the offset of a member of this class, for native property constructors
#define CPP_PROPERTY(Name) static_cast<INT>(reinterpret_cast<BYTE*>(&this->Name) - reinterpret_cast<BYTE*>(static_cast<UObject*>(this)))

// 0x0003deaa
USystem::USystem()
    : Unknown2C(1)
{
}

// 0x0003c3b6
void USystem::StaticConstructor()
{
    UClass* Class = GetClass();
    new(Class, TEXT("PurgeCacheDays"), RF_Public) UIntProperty(CPP_PROPERTY(PurgeCacheDays), TEXT("Options"), CPF_Config);
    new(Class, TEXT("SavePath"), RF_Public) UStrProperty(CPP_PROPERTY(SavePath), TEXT("Options"), CPF_Config);
    new(Class, TEXT("CachePath"), RF_Public) UStrProperty(CPP_PROPERTY(CachePath), TEXT("Options"), CPF_Config);
    new(Class, TEXT("CacheExt"), RF_Public) UStrProperty(CPP_PROPERTY(CacheExt), TEXT("Options"), CPF_Config);
    new(Class, TEXT("SourcePath"), RF_Public) UStrProperty(CPP_PROPERTY(SourcePath), TEXT("Options"), CPF_Config);
    new(Class, TEXT("bLinearLoad"), RF_Public) UIntProperty(CPP_PROPERTY(bLinearLoad), TEXT("Options"), CPF_Config);
    new(Class, TEXT("bLinearSave"), RF_Public) UIntProperty(CPP_PROPERTY(bLinearSave), TEXT("Options"), CPF_Config);

    UArrayProperty* PathsProperty = new(Class, TEXT("Paths"), RF_Public) UArrayProperty(CPP_PROPERTY(Paths), TEXT("Options"), CPF_Config);
    PathsProperty->Inner = new(PathsProperty, NAME_None, RF_Public) UStrProperty;

    UArrayProperty* SuppressProperty = new(Class, TEXT("Suppress"), RF_Public) UArrayProperty(CPP_PROPERTY(Suppress), TEXT("Options"), CPF_Config);
    SuppressProperty->Inner = new(SuppressProperty, NAME_None, RF_Public) UNameProperty;
}

// 0x0003e8a8
UBOOL USystem::Exec(const TCHAR* Cmd, FOutputDevice& Ar)
{
    if (ParseCommand(&Cmd, TEXT("MEMSTAT")))
    {
        // memory statistics from the xbox kernel's GlobalMemoryStatus
        NOT_DECOMPILED(0x0003e8cd);
    }
    else if (ParseCommand(&Cmd, TEXT("MEMINFO")))
    {
        NOT_DECOMPILED(0x0003e940);
    }
    else if (ParseCommand(&Cmd, TEXT("CONFIGHASH")))
    {
        GConfig->Dump(Ar);
    }
    else if (ParseCommand(&Cmd, TEXT("EXIT")) || ParseCommand(&Cmd, TEXT("QUIT")))
    {
        Ar.Log(TEXT("Closing by request"));
        appRequestExit(0);
    }
    else if (ParseCommand(&Cmd, TEXT("RELAUNCH")))
    {
        GLog->Logf(TEXT("Relaunch: %s"), Cmd);
        GConfig->Flush(0);
        // the xbox starts default.xbe again with XLaunchNewImage
        NOT_DECOMPILED(0x0003ea29);
        appRequestExit(0);
    }
    else if (ParseCommand(&Cmd, TEXT("DEBUG")))
    {
        if (ParseCommand(&Cmd, TEXT("CRASH")))
        {
            GError->Logf(TEXT("%s"), TEXT("Unreal crashed at your request"));
        }
        else if (ParseCommand(&Cmd, TEXT("GPF")))
        {
            Ar.Log(TEXT("Unreal crashing with voluntary GPF"));
            *static_cast<volatile INT*>(nullptr) = 123;
        }
        else if (ParseCommand(&Cmd, TEXT("EATMEM")))
        {
            Ar.Log(TEXT("Eating up all available memory"));
            for (;;)
                memset(appMalloc(65536), 0, 65536);
        }
        else
        {
            return 0;
        }
    }
    else if (ParseCommand(&Cmd, TEXT("DIR")))
    {
        TArray<FString> Files = GFileManager->FindFiles(Cmd, 1, 0);
        TArray<FString> Directories = GFileManager->FindFiles(Cmd, 0, 1);
        // each list is sorted through an array of pointers, ignoring case
        auto Compare = [](FString* const& A, FString* const& B) { return appStricmp(**A, **B); };
        TArray<FString*> Sorted;
        for (INT i = 0; i < Directories.Num(); i++)
            Sorted.AddItem(&Directories[i]);
        Sort(Sorted.GetData(), Sorted.Num(), Compare);
        for (INT i = 0; i < Sorted.Num(); i++)
            Ar.Logf(TEXT("[%s]"), **Sorted[i]);
        Sorted.Empty();
        for (INT i = 0; i < Files.Num(); i++)
            Sorted.AddItem(&Files[i]);
        Sort(Sorted.GetData(), Sorted.Num(), Compare);
        for (INT i = 0; i < Sorted.Num(); i++)
            Ar.Logf(TEXT("%s"), **Sorted[i]);
    }
    else
    {
        return 0;
    }
    return 1;
}

IMPLEMENT_CLASS(USubsystem, Core)
IMPLEMENT_CLASS(ULanguage, Core)
IMPLEMENT_CLASS(USystem, Core)
