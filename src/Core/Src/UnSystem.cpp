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

// 0x0003e8a8. console memory statistics from the xbox kernel.
UBOOL USystem::Exec(const TCHAR* Cmd, FOutputDevice& Ar)
{
    NOT_DECOMPILED(0x0003e8a8);
    return 0;
}

IMPLEMENT_CLASS(USubsystem, Core)
IMPLEMENT_CLASS(USystem, Core)
