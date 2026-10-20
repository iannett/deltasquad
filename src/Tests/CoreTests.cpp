#include <alloca.h>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

#include "Core.h"
#include "UnMath.h"
#include "CTMarkersClasses.h"
#include "IpDrvClasses.h"
#include "XGameClasses.h"
#include "XInterfaceLiveClasses.h"
#include "FConfigCacheIni.h"

namespace
{
    INT Failures = 0;
    UBOOL HaveGame = 0;

    void Check(bool Condition, const char* What)
    {
        if (!Condition)
        {
            printf("FAIL: %s\n", What);
            Failures++;
        }
    }

    // in-memory archive for round trip tests
    class FTestArchive : public FArchive
    {
    public:
        std::vector<BYTE> Bytes;
        size_t Pos = 0;

        void StartSaving() { ArIsSaving = true; ArIsLoading = false; }
        void StartLoading() { ArIsSaving = false; ArIsLoading = true; Pos = 0; }

        void Serialize(void* V, INT Length) override
        {
            if (ArIsSaving)
            {
                Bytes.insert(Bytes.end(), static_cast<BYTE*>(V), static_cast<BYTE*>(V) + Length);
            }
            else
            {
                memcpy(V, &Bytes[Pos], Length);
                Pos += Length;
            }
        }
    };

    void TestCompactIndex()
    {
        const INT Values[] = { 0, 1, -1, 63, 64, -64, 127, 128, 8191, 8192, 1048575, 1048576, 0x7FFFFFFF, -0x7FFFFFFF };
        const size_t Sizes[] = { 1, 1, 1, 1, 2, 2, 2, 2, 2, 3, 3, 4, 5, 5 };
        for (size_t i = 0; i < ARRAY_COUNT(Values); i++)
        {
            FTestArchive Ar;
            Ar.StartSaving();
            INT Value = Values[i];
            Ar << AR_INDEX(Value);
            Check(Ar.Bytes.size() == Sizes[i], "compact index encoded size");

            Ar.StartLoading();
            INT Loaded = 0;
            Ar << AR_INDEX(Loaded);
            Check(Loaded == Values[i], "compact index round trip");
        }
    }

    void TestNameEntry()
    {
        FNameEntry Saved = {};
        appStrcpy(Saved.Name, TEXT("CloneCommando"));
        Saved.Flags = 0x1234;

        FTestArchive Ar;
        Ar.StartSaving();
        Ar << Saved;

        FNameEntry Loaded = {};
        Ar.StartLoading();
        Ar << Loaded;
        Check(appStrcmp(Loaded.Name, Saved.Name) == 0 && Loaded.Flags == Saved.Flags, "name entry round trip");
    }

    void TestNames()
    {
        const FName Found(TEXT("objectproperty"), FNAME_Find);
        Check(Found == NAME_ObjectProperty, "find hardcoded name ignoring case");
        Check(appStrcmp(*Found, TEXT("ObjectProperty")) == 0, "hardcoded spelling kept");
        Check(FName(TEXT("NotARealName"), FNAME_Find) == NAME_None, "find missing name");
        Check(FName(TEXT("None")) == NAME_None, "None is null");

        const FName Added(TEXT("SquadMember"));
        Check(Added != NAME_None && FName(TEXT("squadmember"), FNAME_Find) == Added, "added name is found");
    }

    // native classes register only once per process, so appInit boots the object system in main and TestExit closes it
    void TestRegistration()
    {

        UObject* ObjectClass = UObject::StaticFindObject(UClass::StaticClass(), ANY_PACKAGE, TEXT("Object"));
        Check(ObjectClass == UObject::StaticClass(), "find Core.Object by name");
        Check(UObject::StaticFindObject(nullptr, nullptr, TEXT("Core.IntProperty")) == UIntProperty::StaticClass(), "find by dotted path");
        Check(appStrcmp(UIntProperty::StaticClass()->GetPathName(), TEXT("Core.IntProperty")) == 0, "class path name");
        Check(UBoolProperty::StaticClass()->IsChildOf(UProperty::StaticClass()), "class hierarchy");
        Check(UBitArray2D::StaticClass()->IsChildOf(UBitArray::StaticClass()), "bit array classes registered");
        Check((UState::StaticClass()->ClassFlags & CLASS_IsAUState) != 0, "cast flags survive registration");
        Check((UFunction::StaticClass()->ClassFlags & CLASS_Abstract) == 0, "abstract flag not inherited");
        Check(UClass::StaticClass()->GetDefaultObject() != nullptr, "class defaults created");
    }

    // needs the game files; skipped when game/System/Core.u is missing
    void TestLinkerTables()
    {
        const TCHAR* Filename = DELTASQUAD_GAME_DIR "/System/Core.u";
        if (GFileManager->FileSize(Filename) < 0)
        {
            printf("skipped linker tables: no %s\n", Filename);
            return;
        }

        GIsClient = GIsServer = 1;

        UObject* Package = UObject::CreatePackage(nullptr, TEXT("Core"));
        ULinkerLoad* Linker = new ULinkerLoad(Package, Filename, 0);
        Check(Linker->Success == 1, "linker opened Core.u");
        Check(Linker->Summary.FileVersion == 0x0001009F, "Core.u is version 159, licensee 1");
        Check(Linker->NameMap.Num() == 408, "Core.u name count");
        Check(Linker->ImportMap.Num() == 17, "Core.u import count");
        Check(Linker->ExportMap.Num() == 809, "Core.u export count");
        Check(Linker->Summary.Generations.Num() == 1, "Core.u generation count");
        Check(FName(TEXT("Object"), FNAME_Find) != NAME_None, "Core.u names added");

        INT Found = INDEX_NONE;
        for (INT i = 0; i < Linker->ExportMap.Num(); i++)
        {
            if (Linker->ExportMap[i].ObjectName == FName(TEXT("Object"), FNAME_Find) && Linker->GetExportClassName(i) == NAME_Class)
                Found = i;
        }
        Check(Found != INDEX_NONE, "Core.u exports class Object");
        if (Found != INDEX_NONE)
            Check(Linker->ExportMap[Found].SerialSize > 0, "class Object has data");

        Check(GetPackageLinker(Package, nullptr, 0, nullptr, nullptr) == Linker, "package linker found by package");
        Check(Linker->ImportMap[0].ClassName == NAME_Package && Linker->IndexToObject(-1) == Package, "first import is the Core package");

        // every import of Core.u resolves inside Core
        Linker->Verify();
        INT Resolved = 0;
        for (INT i = 0; i < Linker->ImportMap.Num(); i++)
        {
            const FObjectImport& Import = Linker->ImportMap[i];
            if (Import.XObject || (Import.SourceLinker == Linker && Import.SourceIndex != INDEX_NONE))
                Resolved++;
            else
                printf("unresolved import: %s\n", *Linker->GetImportFullName(i));
        }
        Check(Resolved == Linker->ImportMap.Num(), "Core.u imports resolve");
        Check(Linker->Create(UClass::StaticClass(), FName(NAME_Object), Package, LOAD_Verify, 0) == reinterpret_cast<UObject*>(-1),
            "Create finds class Object without creating it");
    }

    // needs the game files; runs after TestLinkerTables, which opens Core.u
    void TestLoadCore()
    {
        UObject* Package = UObject::StaticFindObject(UPackage::StaticClass(), nullptr, TEXT("Core"));
        ULinkerLoad* Linker = Package ? GetPackageLinker(Package, nullptr, 0, nullptr, nullptr) : nullptr;
        if (!Linker)
        {
            printf("skipped loading Core: no linker\n");
            return;
        }

        // every export of Core.u serializes to exactly its stored size, or Preload stops with an error.
        // exports outside the linker's context (the editor-only script text) are never created.
        UObject::BeginLoad();
        INT Loaded = 0;
        INT InContext = 0;
        for (INT i = 0; i < Linker->ExportMap.Num(); i++)
        {
            if (Linker->ExportMap[i].ObjectFlags & Linker->ContextFlags)
                InContext++;
            if (UObject* Object = Linker->CreateExport(i))
            {
                Linker->Preload(Object);
                Loaded++;
            }
        }
        UObject::EndLoad();
        Check(Loaded > 0 && Loaded == InContext, "every Core.u export in context loads");

        // native classes keep their C++ layout; script classes start after it
        UClass* ObjectClass = UObject::StaticClass();
        UClass* Commandlet = static_cast<UClass*>(UObject::StaticFindObject(UClass::StaticClass(), Package, TEXT("Commandlet")));
        Check(ObjectClass->GetPropertiesSize() == static_cast<INT>(sizeof(UObject)), "Object is the size of UObject");
        Check(USubsystem::StaticClass()->GetPropertiesSize() == static_cast<INT>(sizeof(USubsystem)), "Subsystem is the size of USubsystem");
        Check(Commandlet && Commandlet->PropertyLink && Commandlet->PropertyLink->Offset == static_cast<INT>(sizeof(UObject)),
            "Commandlet properties start after UObject");

        UFunction* Function = static_cast<UFunction*>(UObject::StaticFindObject(UFunction::StaticClass(), ObjectClass, TEXT("Log")));
        Check(Function && Function->NumParms > 0, "Object.Log has parameters");
        UBOOL HasCode = 0;
        for (TFieldIterator<UFunction> It(ObjectClass); It; ++It)
            HasCode |= It->Script.Num() > 0;
        Check(HasCode, "some Object function has script code");

        Check(UObject::LoadPackage(nullptr, TEXT("Core"), 0) == Package, "LoadPackage finds the Core package");

        // script structs with a C++ type take its size, so arrays of them step the same way
        UStruct* Box = static_cast<UStruct*>(UObject::StaticFindObject(UStruct::StaticClass(), ObjectClass, TEXT("Box")));
        UStruct* Scale = static_cast<UStruct*>(UObject::StaticFindObject(UStruct::StaticClass(), ObjectClass, TEXT("Scale")));
        Check(Box && Box->GetPropertiesSize() == static_cast<INT>(sizeof(FBox)), "Object.Box is the size of FBox");
        Check(Scale && Scale->GetPropertiesSize() == static_cast<INT>(sizeof(FScale)), "Object.Scale is the size of FScale");
    }

    // keeps what script logs and passes it on to the real log
    class FCaptureLog : public FOutputDevice
    {
    public:
        FOutputDevice* Next = nullptr;
        FString Text;

        void Serialize(const TCHAR* V, EName Event) override
        {
            Text += V;
            Text += TEXT("\n");
            Next->Serialize(V, Event);
        }
    };

    // runs real bytecode: HelloWorldCommandlet.Main from Core.u, through ProcessEvent
    void TestRunScript()
    {
        UClass* HelloClass = static_cast<UClass*>(UObject::StaticFindObject(UClass::StaticClass(), ANY_PACKAGE, TEXT("HelloWorldCommandlet")));
        if (!HelloClass)
        {
            printf("skipped running script: Core.u not loaded\n");
            return;
        }
        UObject* Hello = UObject::StaticConstructObject(HelloClass);
        UFunction* Main = Hello->FindFunctionChecked(FName(TEXT("Main"), FNAME_Add));
        Check(Main->Script.Num() > 0 && !(Main->FunctionFlags & FUNC_Native), "HelloWorldCommandlet.Main is script");

        // parameters: string Parms, then the int return value
        BYTE* Parms = static_cast<BYTE*>(alloca(Main->ParmsSize));
        memset(Parms, 0, Main->ParmsSize);
        UProperty* ParmsProp = Main->PropertyLink;
        new (Parms + ParmsProp->Offset) FString(TEXT("-test"));
        *reinterpret_cast<INT*>(Parms + Main->ReturnValueOffset) = 123;

        FCaptureLog Capture;
        Capture.Next = GLog;
        GLog = &Capture;
        Hello->ProcessEvent(Main, Parms);
        GLog = Capture.Next;

        Check(appStrstr(*Capture.Text, TEXT("Hello, world!")) != nullptr, "script logs hello world");
        Check(appStrstr(*Capture.Text, TEXT("Command line parameters=-test")) != nullptr, "script concatenates the parameter");
        Check(appStrstr(*Capture.Text, TEXT("intparm")) == nullptr, "script skips the zero intparm branch");
        Check(*reinterpret_cast<INT*>(Parms + Main->ReturnValueOffset) == 0, "Main returns 0");
        reinterpret_cast<FString*>(Parms + ParmsProp->Offset)->~FString();
    }

    // needs the game files; skipped when game/System/System.ini is missing
    void TestConfig()
    {
        const TCHAR* SystemIni = DELTASQUAD_GAME_DIR "/System/System.ini";
        const TCHAR* UserIni = DELTASQUAD_GAME_DIR "/System/User.ini";
        if (GFileManager->FileSize(SystemIni) < 0)
        {
            printf("skipped config: no %s\n", SystemIni);
            return;
        }

        FConfigCache* Config = CreateConfigCacheIni();
        Config->Init(SystemIni, UserIni, 1);

        FString Language;
        Check(Config->GetString(TEXT("Engine.Engine"), TEXT("Language"), Language, TEXT("System")) && Language == TEXT("int"),
            "System.ini language is int");
        Check(!appStricmp(Config->GetStr(TEXT("Core.System"), TEXT("CacheExt")), TEXT(".uxx")), "null filename means System.ini");

        // Paths+= lines add entries; MultiFind returns them newest first
        FConfigSection* Sec = Config->GetSectionPrivate(TEXT("Core.System"), 0, 1, TEXT("System"));
        TArray<FConfigValue> Paths;
        if (Sec)
            Sec->MultiFind(FName(TEXT("Paths"), FNAME_Add), Paths);
        Check(Paths.Num() >= 10, "System.ini has the package paths");
        Check(Paths.Num() && !appStricmp(*Paths.Last(), TEXT("..\\System\\*.u")), "first path is System");

        Config->SetInt(TEXT("DeltaSquadTest"), TEXT("Value"), 42, TEXT("System"));
        INT Value = 0;
        Check(Config->GetInt(TEXT("DeltaSquadTest"), TEXT("Value"), Value, TEXT("System")) && Value == 42, "set and get an int");
        UBOOL Missing = 1;
        Check(!Config->GetBool(TEXT("DeltaSquadTest"), TEXT("Nope"), Missing, TEXT("System")) && Missing, "missing key leaves the value");

        // nothing is written: the system ini directory is still <INVALID>
        Config->Flush(1, nullptr, nullptr);
        delete Config;
    }

    // finding packages through the [Core.System] paths; needs the game files
    void TestPackageSearch()
    {
        if (!HaveGame)
        {
            printf("skipped package search: no game files\n");
            return;
        }
        Check(GSys && GSys->Paths.Num() >= 10, "GSys has the System.ini paths");

        TCHAR Found[256];
        Check(appFindPackageFile(TEXT("Engine"), nullptr, Found) && appStrstr(Found, TEXT("Engine.u")), "Engine found through the paths");
        Check(!appFindPackageFile(TEXT("NotAPackage"), nullptr, Found), "missing package not found");
        Check(!appFindPackageFile(TEXT("Core.dll"), nullptr, Found), "dll names are never package files");
    }

    bool Near(FLOAT A, FLOAT B, FLOAT Tolerance = 0.001f)
    {
        return fabsf(A - B) <= Tolerance;
    }

    void TestMath()
    {
        Check(GMath.SinTab(16384) == 1.f && GMath.CosTab(0) == 1.f && GMath.SinTab(0) == 0.f, "trig table");
        const FVector Left = FRotator(0, 16384, 0).Vector();
        Check(Near(Left.X, 0.f) && Near(Left.Y, 1.f) && Near(Left.Z, 0.f), "yaw 16384 points along y");

        const FRotator R(4096, 20000, -3000);
        const FRotator Back = FRotationTranslationMatrix(R, FVector(0.f, 0.f, 0.f)).Rotator();
        Check(Back.Pitch == 4096 && Back.Yaw == 20000 && Back.Roll == (-3000 & 0xffff), "matrix back to rotator");

        const FMatrix M = FRotationTranslationMatrix(R, FVector(10.f, 20.f, 30.f));
        const FMatrix I = M.Inverse();
        FLOAT MaxError = 0.f;
        for (INT i = 0; i < 4; i++)
            for (INT j = 0; j < 4; j++)
            {
                FLOAT Sum = 0.f;
                for (INT k = 0; k < 4; k++)
                    Sum += M.M[i][k] * I.M[k][j];
                MaxError = fmaxf(MaxError, fabsf(Sum - (i == j ? 1.f : 0.f)));
            }
        Check(MaxError < 0.001f, "matrix times inverse is the identity");

        const FRotator Small(1000, 2000, 500);
        const FRotator QuatBack = FQuatRotationTranslationMatrix(FQuat(FRotationTranslationMatrix(Small, FVector(0.f, 0.f, 0.f))), FVector(0.f, 0.f, 0.f)).Rotator();
        Check(abs(QuatBack.Pitch - 1000) <= 1 && abs(QuatBack.Yaw - 2000) <= 1 && abs(QuatBack.Roll - 500) <= 1, "rotator through a quaternion");

        FInterpCurve Curve;
        Curve.Points.AddItem({ 0.f, 0.f });
        Curve.Points.AddItem({ 2.f, 10.f });
        Check(Curve.Eval(1.f) == 5.f && Curve.Eval(3.f) == 0.f, "interp curve");
    }

    // every generated table: members in script order, inside the C++ size, bitfields found
    void TestNativeLayouts()
    {
        Check(AActor::StaticClass() != nullptr, "Engine classes are linked in");
        Check(UScriptedAction::StaticClass() != nullptr, "Gameplay classes are linked in");
        Check(ACTGameInfo::StaticClass() != nullptr, "CTGame classes are linked in");
        Check(AExplosiveCharge::StaticClass() != nullptr, "CTMarkers classes are linked in");
        Check(AInternetLink::StaticClass() != nullptr, "IpDrv classes are linked in");
        Check(UxUtil::StaticClass() != nullptr, "XGame classes are linked in");
        Check(AMenuBase::StaticClass() != nullptr, "XInterface classes are linked in");
        Check(AMenuGamerList::StaticClass() != nullptr, "XInterfaceLive classes are linked in");
        INT Tables = 0;
        UBOOL Ordered = 1;
        for (const FNativeLayout* Layout = FNativeLayout::First(); Layout; Layout = Layout->Next)
        {
            Tables++;
            INT Last = 0;
            for (INT i = 0; i < Layout->Count; i++)
            {
                const INT Offset = Layout->Properties[i].Offset;
                if (Offset < Last || Offset >= Layout->Size)
                {
                    printf("bad offset %d for %s\n", Offset, Layout->Properties[i].Name);
                    Ordered = 0;
                }
                Last = Offset;
            }
        }
        Check(Ordered, "native layout offsets are in order and inside the class");
        Check(Tables > 300, "Core and Engine layouts are registered");
    }

    // Engine.u loads with its natives bound and its native classes at their C++ size
    void TestLoadEngine()
    {
        if (!HaveGame)
        {
            printf("skipped loading Engine: no game files\n");
            return;
        }
        UObject* Package = UObject::LoadPackage(nullptr, TEXT("Engine"), 0);
        Check(Package != nullptr, "Engine.u loads");
        if (!Package)
            return;
        Check(AActor::StaticClass()->GetPropertiesSize() == static_cast<INT>(sizeof(AActor)), "Actor is the size of AActor");
        UFunction* Function = static_cast<UFunction*>(UObject::StaticFindObject(UFunction::StaticClass(), AActor::StaticClass(), TEXT("Spawn")));
        Check(Function && Function->Func == static_cast<Native>(&AActor::execSpawn), "Actor.Spawn is bound to its native");
    }

    // the game's other packages load on top of Engine.u
    void TestLoadGamePackages()
    {
        if (!HaveGame)
        {
            printf("skipped loading game packages: no game files\n");
            return;
        }
        const TCHAR* Packages[] = { TEXT("GamePlay"), TEXT("CTGame"), TEXT("CTMarkers"), TEXT("IpDrv"), TEXT("XGame"), TEXT("XInterface"), TEXT("XInterfaceLive"),
            TEXT("CTAudio"), TEXT("CTCharacters"), TEXT("CTInventory"), TEXT("MPGame"), TEXT("UWindow"), TEXT("UDebugMenu"),
            TEXT("XInterfaceCommon"), TEXT("XInterfaceCTMenus"), TEXT("XInterfaceGamespy"), TEXT("XInterfaceMP") };
        for (const TCHAR* Name : Packages)
        {
            TCHAR Message[256];
            appSprintf(Message, TEXT("%s.u loads"), Name);
            Check(UObject::LoadPackage(nullptr, Name, 0) != nullptr, Message);
        }
    }

    void TestExit()
    {
        UObject::StaticExit();
        Check(!GObjInitialized, "object system closed");
    }
}

int main()
{
    GIsStarted = 1;
    GIsScriptable = 1;
    GFileManager = CreatePosixFileManager();

    // with the game files, appInit reads the game's own ini files from its System directory
    TCHAR CmdLine[1024] = TEXT("");
    HaveGame = GFileManager->FileSize(DELTASQUAD_GAME_DIR "/System/System.ini") >= 0;
    if (HaveGame)
    {
        appSetBaseDir(DELTASQUAD_GAME_DIR "/System/");
        appSprintf(CmdLine, TEXT("INI=\"%s/System/System.ini\" USERINI=\"%s/System/User.ini\""), DELTASQUAD_GAME_DIR, DELTASQUAD_GAME_DIR);
    }
    appInit(CmdLine, GLog, GError, GWarn, CreateConfigCacheIni, 0);

    TestCompactIndex();
    TestNameEntry();
    TestNames();
    TestRegistration();
    TestLinkerTables();
    TestLoadCore();
    TestRunScript();
    TestConfig();
    TestPackageSearch();
    TestMath();
    TestNativeLayouts();
    TestLoadEngine();
    TestLoadGamePackages();
    TestExit();

    printf(Failures ? "%d test(s) failed\n" : "all tests passed\n", Failures);
    return Failures ? 1 : 0;
}
