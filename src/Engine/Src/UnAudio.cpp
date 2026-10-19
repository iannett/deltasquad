#include "EngineClasses.h"

// 0x00074d57
void USound::Serialize(FArchive& Ar)
{
    UObject::Serialize(Ar);
    Ar << FileType;
    Data.Serialize(Ar);
    // a sound already handed to the audio system counts its original size
    if (Data.Num() == 0 && Handle >= 0)
    {
        const FMemCount Count = { 1, static_cast<DWORD>(OriginalSize), static_cast<DWORD>(OriginalSize) };
        Ar.CountBytes('A', Count);
    }
    Streaming = 0;
}

// 0x00072ed1
void USoundMultiple::Serialize(FArchive& Ar)
{
    UObject::Serialize(Ar);
    Ar << Sounds << SelectionStyle;
    if (Ar.IsLoading())
    {
        WhichSound = 0;
        TotalLikelihood = -1.f;
        for (INT i = 0; i < Sounds.Num(); i++)
        {
            if (Sounds[i] == this)
            {
                GWarn->Logf(NAME_Warning, TEXT("%s sound %i references self"), GetFullName(), i);
                Sounds.Remove(i--);
            }
        }
    }
    Streaming = 0;
}

// 0x00071039
void USoundStreamed::Serialize(FArchive& Ar)
{
    UObject::Serialize(Ar);
    Ar << Filename;
    Streaming = 1;
}
