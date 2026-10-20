#include "Core.h"
#include "FConfigCacheIni.h"

FString GUserIniDir;
FString GInvalidIniDir(TEXT("<INVALID>"));
FString GSystemIniDir(GInvalidIniDir);

// 0x00013a2a
FConfigFile::FConfigFile(UBOOL InUnknown13)
    : Dirty(0)
    , NoSave(0)
    , QuoteValues(0)
    , Unknown13(static_cast<BYTE>(InUnknown13))
{
}

// 0x00013a4a
void FConfigFile::Read(const TCHAR* Filename)
{
    FString Text;
    if (!appLoadFileToString(Text, Filename))
        return;

    TCHAR* Ptr = const_cast<TCHAR*>(*Text);
    FConfigSection* CurrentSection = nullptr;
    UBOOL Done = 0;
    while (!Done)
    {
        while (*Ptr == '\r' || *Ptr == '\n')
            Ptr++;
        TCHAR* Start = Ptr;
        while (*Ptr && *Ptr != '\r' && *Ptr != '\n')
            Ptr++;
        if (!*Ptr)
            Done = 1;
        *Ptr++ = 0;

        if (*Start == '[' && Start[appStrlen(Start) - 1] == ']')
        {
            Start++;
            Start[appStrlen(Start) - 1] = 0;
            CurrentSection = TMap::Find(FString(Start));
            if (!CurrentSection)
                CurrentSection = &Set(Start, FConfigSection());
        }
        else if (CurrentSection && *Start)
        {
            TCHAR* Value = appStrstr(Start, TEXT("="));
            if (!Value)
                continue;

            // +Key=Value adds another entry instead of replacing
            UBOOL Add = 0;
            if (Value > Start && Value[-1] == '+')
            {
                Add = 1;
                Value[-1] = 0;
            }
            *Value++ = 0;
            if (*Value == '"' && Value[appStrlen(Value) - 1] == '"')
            {
                Value++;
                Value[appStrlen(Value) - 1] = 0;
            }
            SetString(CurrentSection, Start, Value, Add);
        }
    }
}

// 0x00013bb3
void FConfigFile::ClearDirty()
{
    Dirty = 0;
    for (INT i = 0; i < Num(); i++)
    {
        FConfigSection& Section = ValueAt(i);
        Section.Dirty = 0;
        for (INT j = 0; j < Section.Num(); j++)
            Section.ValueAt(j).Dirty = 0;
    }
}

// 0x00013c10
UBOOL FConfigFile::Write(const TCHAR* Filename, UBOOL CommentUnchanged, const TCHAR* Section)
{
    if (!Dirty || NoSave)
        return 1;
    Dirty = 0;

    FString Text;
    for (INT i = 0; i < Num(); i++)
    {
        const FString& SectionName = KeyAt(i);
        FConfigSection& Sec = ValueAt(i);
        Text += FString::Printf(TEXT("[%s]\r\n"), *SectionName);
        for (INT j = 0; j < Sec.Num(); j++)
        {
            FConfigValue& Value = Sec.ValueAt(j);
            if (CommentUnchanged && !Value.Dirty && (!Section || SectionName == Section))
                Text += TEXT(";  ");
            Text += *Sec.KeyAt(j);

            TArray<FConfigValue> Values;
            Sec.MultiFind(Sec.KeyAt(j), Values);
            Text += Values.Num() > 1 ? TEXT("+=") : TEXT("=");
            if (QuoteValues)
            {
                Text += TEXT("\"");
                Text += Value.Text;
                Text += TEXT("\"\r\n");
            }
            else
            {
                Text += Value.Text;
                Text += TEXT("\r\n");
            }
        }
        Text += TEXT("\r\n");
    }

    // User.ini goes to the profile's directory, everything else to the system directory
    FString Path;
    if (!appStricmp(Filename, TEXT("User.ini")))
    {
        if (GUserIniDir.Len() <= 0)
            return 1;
        Path = GUserIniDir * Filename;
    }
    else
    {
        if (GSystemIniDir == GInvalidIniDir)
            return 0;
        Path = GSystemIniDir * Filename;
    }

    // a new file needs 16k free on the save drive
    if (GFileManager->FileSize(*Path) <= 0 && GFileManager->GetDiskFreeSpace(TEXT("U:")) < 0x4000)
        return 0;
    return appSaveStringToFile(Text, *Path);
}

// 0x00013ff7
void FConfigFile::SetString(FConfigSection* Section, const TCHAR* Key, const TCHAR* Value, UBOOL Add)
{
    if (*Key == ';')
        return;

    const FName KeyName(Key, FNAME_Add);
    FConfigValue* Entry = Section->Find(KeyName);
    if (!Entry || Add)
    {
        Entry = &Section->Add(KeyName, FConfigValue(Value));
    }
    else
    {
        if (!appStricmp(**Entry, Value))
            return;
        // the same nonzero number written differently is not a change
        const FLOAT Old = appAtof(**Entry);
        if (Old == appAtof(Value) && Old != 0.f)
            return;
        *Entry = FConfigValue(Value);
    }
    Dirty = 1;
    Section->Dirty = 1;
    Entry->Dirty = 1;
}

// 0x0001415f
FConfigFile* FConfigCacheIni::Find(const TCHAR* InFilename, UBOOL CreateIfNotFound)
{
    FString Text = InFilename ? FString(InFilename) : SystemIni;
    const INT Len = Text.Len();
    if (Len < 5 || ((*Text)[Len - 4] != '.' && (*Text)[Len - 5] != '.'))
        Text += TEXT(".ini");

    UBOOL IsUser = 0;
    if (Text == TEXT("User.ini"))
    {
        Text = UserIni;
        IsUser = 1;
    }
    else if (Text == TEXT("System.ini"))
    {
        Text = SystemIni;
    }

    FConfigFile* Result = TMap::Find(Text);
    if (Result)
        return Result;

    if (GFileManager->FileSize(*Text) < 0)
        return &Set(Text, FConfigFile(!CreateIfNotFound));

    Result = &Set(Text, FConfigFile(0));
    Result->Read(*Text);
    Result->ClearDirty();

    // ini files are then overridden by the copy saved in the profile or system directory
    if (Text.Right(4) == TEXT(".ini"))
    {
        if (IsUser)
        {
            Result->ClearDirty();
            if (GUserIniDir.Len() > 0)
                Result->Read(*(GUserIniDir * *Text));
            else
                Result->Read(*Text);
        }
        else
        {
            Result->Read(*(GSystemIniDir * *Text));
        }
    }
    return Result;
}

// 0x00014869
FConfigValue* FConfigCacheIni::FindValue(const TCHAR* Section, const TCHAR* Key, const TCHAR* Filename)
{
    FConfigFile* File = Find(Filename, 0);
    if (!File)
        return nullptr;
    FConfigSection* Sec = File->TMap::Find(FString(Section));
    if (!Sec)
        return nullptr;
    const FName KeyName(Key, FNAME_Find);
    if (KeyName == NAME_None)
        return nullptr;
    return Sec->Find(KeyName);
}

// 0x000149dc
UBOOL FConfigCacheIni::GetBool(const TCHAR* Section, const TCHAR* Key, UBOOL& Value, const TCHAR* Filename)
{
    TCHAR Text[80];
    if (!GetString(Section, Key, Text, ARRAY_COUNT(Text), Filename))
        return 0;
    Value = !appStricmp(Text, TEXT("True")) ? 1 : appAtoi(Text) == 1;
    return 1;
}

// 0x0001496e
UBOOL FConfigCacheIni::GetInt(const TCHAR* Section, const TCHAR* Key, INT& Value, const TCHAR* Filename)
{
    TCHAR Text[80];
    if (!GetString(Section, Key, Text, ARRAY_COUNT(Text), Filename))
        return 0;
    Value = appAtoi(Text);
    return 1;
}

// 0x000149a6
UBOOL FConfigCacheIni::GetFloat(const TCHAR* Section, const TCHAR* Key, FLOAT& Value, const TCHAR* Filename)
{
    TCHAR Text[80];
    if (!GetString(Section, Key, Text, ARRAY_COUNT(Text), Filename))
        return 0;
    Value = appAtof(Text);
    return 1;
}

// 0x00014911
UBOOL FConfigCacheIni::GetString(const TCHAR* Section, const TCHAR* Key, FString& Value, const TCHAR* Filename)
{
    if (FConfigValue* Found = FindValue(Section, Key, Filename))
    {
        Value = Found->Text;
        return 1;
    }
    Value = TEXT("");
    return 0;
}

// 0x000148cc
UBOOL FConfigCacheIni::GetString(const TCHAR* Section, const TCHAR* Key, TCHAR* Value, INT Size, const TCHAR* Filename)
{
    if (FConfigValue* Found = FindValue(Section, Key, Filename))
    {
        appStrncpy(Value, **Found, Size);
        return 1;
    }
    *Value = 0;
    return 0;
}

// 0x00014944
const TCHAR* FConfigCacheIni::GetStr(const TCHAR* Section, const TCHAR* Key, const TCHAR* Filename)
{
    FConfigValue* Found = FindValue(Section, Key, Filename);
    return Found ? **Found : TEXT("");
}

// 0x00014580
UBOOL FConfigCacheIni::GetSection(const TCHAR* Section, TCHAR* Result, INT Size, const TCHAR* Filename)
{
    *Result = 0;
    FConfigFile* File = Find(Filename, 0);
    if (!File)
        return 0;
    FConfigSection* Sec = File->TMap::Find(FString(Section));
    if (!Sec)
        return 0;

    // only the key length is checked against Size, as in the original
    TCHAR* End = Result;
    for (INT i = 0; i < Sec->Num(); i++)
    {
        if (appStrlen(*Sec->KeyAt(i)) + static_cast<INT>(End - Result) + 1 >= Size)
            break;
        End += appSprintf(End, TEXT("%s=%s"), *Sec->KeyAt(i), *Sec->ValueAt(i)) + 1;
    }
    *End = 0;
    return 1;
}

// 0x0001466c
FConfigSection* FConfigCacheIni::GetSectionPrivate(const TCHAR* Section, UBOOL Force, UBOOL Const, const TCHAR* Filename)
{
    FConfigFile* File = Find(Filename, Force);
    if (!File)
        return nullptr;

    FConfigSection* Sec = File->TMap::Find(FString(Section));
    if (!Sec)
    {
        if (!Force)
            return nullptr;
        Sec = &File->Set(Section, FConfigSection());
    }
    if (Force || !Const)
    {
        Sec->Dirty = 1;
        File->Dirty = 1;
    }
    return Sec;
}

// 0x00014770
void FConfigCacheIni::EmptySection(const TCHAR* Section, const TCHAR* Filename)
{
    FConfigFile* File = Find(Filename, 0);
    if (!File)
        return;
    FConfigSection* Sec = File->TMap::Find(FString(Section));
    if (Sec && Sec->Num())
    {
        Sec->Empty();
        Sec->Dirty = 1;
        File->Dirty = 1;
    }
}

// 0x00014aa7
void FConfigCacheIni::SetBool(const TCHAR* Section, const TCHAR* Key, UBOOL Value, const TCHAR* Filename)
{
    SetString(Section, Key, Value ? TEXT("True") : TEXT("False"), Filename);
}

// 0x00014a36
void FConfigCacheIni::SetInt(const TCHAR* Section, const TCHAR* Key, INT Value, const TCHAR* Filename)
{
    TCHAR Text[32];
    appSprintf(Text, TEXT("%i"), Value);
    SetString(Section, Key, Text, Filename);
}

// 0x00014a6c
void FConfigCacheIni::SetFloat(const TCHAR* Section, const TCHAR* Key, FLOAT Value, const TCHAR* Filename)
{
    TCHAR Text[32];
    appSprintf(Text, TEXT("%g"), static_cast<DOUBLE>(Value));
    SetString(Section, Key, Text, Filename);
}

// 0x000146fc
void FConfigCacheIni::SetString(const TCHAR* Section, const TCHAR* Key, const TCHAR* Value, const TCHAR* Filename)
{
    FConfigFile* File = Find(Filename, 1);
    FConfigSection* Sec = File->TMap::Find(FString(Section));
    if (!Sec)
        Sec = &File->Set(Section, FConfigSection());
    File->SetString(Sec, Key, Value, 0);
}

// 0x00014412
void FConfigCacheIni::Flush(UBOOL Read, const TCHAR* Filename, const TCHAR* Section)
{
    for (INT i = 0; i < Num(); i++)
    {
        if (!Filename || KeyAt(i) == Filename)
            ValueAt(i).Write(*KeyAt(i), 1, Section);
    }
    if (Read)
    {
        if (Filename)
            Remove(FString(Filename));
        else
            Empty();
    }
}

// 0x000144a8
void FConfigCacheIni::UnloadFile(const TCHAR* Filename)
{
    if (Find(Filename, 1))
        Remove(FString(Filename));
}

// 0x000144ca
void FConfigCacheIni::UnloadLocalizedFiles(const TCHAR* Filename)
{
    TCHAR Text[256];
    appSprintf(Text, TEXT("%s.%s"), Filename, UObject::GetLanguage());
    if (Find(Text, 0))
        Remove(FString(Text));

    if (appStricmp(UObject::GetLanguage(), TEXT("int")))
    {
        appSprintf(Text, TEXT("%s.int"), Filename);
        if (Find(Text, 0))
            Remove(FString(Text));
    }
}

// 0x0001456a
void FConfigCacheIni::Detach(const TCHAR* Filename)
{
    if (FConfigFile* File = Find(Filename, 1))
        File->NoSave = 1;
}

// 0x000147fb. RequireConfig is unused.
void FConfigCacheIni::Init(const TCHAR* InSystem, const TCHAR* InUser, UBOOL RequireConfig)
{
    SystemIni = InSystem;
    UserIni = InUser;
    Find(InSystem, 1);
    Find(InUser, 1);
}

// 0x00014834
void FConfigCacheIni::Exit()
{
    Flush(1, nullptr, nullptr);
}

// 0x00014840
void FConfigCacheIni::Dump(FOutputDevice& Ar)
{
    Ar.Log(TEXT("Files map:"));
    TMap::Dump(Ar);
}

// 0x00014861
void FConfigCacheIni::Serialize(FArchive& Ar)
{
    NOT_DECOMPILED(0x00014861);
}

// 0x00014acd
FConfigCache* CreateConfigCacheIni()
{
    return new FConfigCacheIni;
}
