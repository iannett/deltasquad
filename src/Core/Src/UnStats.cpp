#include "Core.h"
#include "UnMath.h"

FStats GStats;

FStatRecord::FStatRecord(const TCHAR* InTag, FStatRecord* InParent)
    : DWORD0(0), Count(0), Pad1(0), Pad2(0), Tag(InTag), DWORD20(0), Parent(nullptr), Children(nullptr), NextChild(nullptr)
{
    if (InParent)
        InParent->AddChild(this);
}

FStatRecord::FStatRecord(const TCHAR* InTag, const TCHAR* ParentTag)
    : DWORD0(0), Count(0), Pad1(0), Pad2(0), Tag(InTag), DWORD20(0), Children(nullptr), NextChild(nullptr)
{
    Parent = Main().FindStat(ParentTag, true);
    if (Parent)
        Parent->AddChild(this);
}

void FStatRecord::AddChild(FStatRecord* Child)
{
    FStatRecord** Link = &Children;
    for (; *Link; Link = &(*Link)->NextChild)
    {
        if (appStricmp((*Link)->Tag, Child->Tag) == 0)
        {
            (*Link)->AddChild(Child);
            Child->Tag = TEXT("");
            return;
        }
    }
    *Link = Child;
    Child->Parent = this;
}

FStatRecord* FStatRecord::FindStat(const TCHAR* InTag, bool Recursive)
{
    if (Tag && appStricmp(Tag, InTag) == 0)
        return this;
    for (FStatRecord* Child = Children; Child; Child = Child->NextChild)
    {
        if (appStricmp(Child->Tag, InTag) == 0)
            return Child;
    }
    if (Recursive)
    {
        for (FStatRecord* Child = Children; Child; Child = Child->NextChild)
        {
            if (FStatRecord* Found = Child->FindStat(InTag, true))
                return Found;
        }
    }
    return nullptr;
}

void FStatRecord::Tick()
{
    ChildrenShown = false;
    for (FStatRecord* Child = Children; Child; Child = Child->NextChild)
    {
        Child->Tick();
        if (Child != &Ignore())
            DWORD0 += Child->DWORD0;
        if (Child->DWORD20 || Child->ChildrenShown)
            ChildrenShown = true;
    }
}

void FStatRecord::UpdateAverage(FLOAT Fraction)
{
    for (FStatRecord* Child = Children; Child; Child = Child->NextChild)
        Child->UpdateAverage(Fraction);
    DWORD0 = static_cast<DWORD>(static_cast<INT>(Pad1 - DWORD0) * Fraction + static_cast<FLOAT>(DWORD0));
    Count = static_cast<DWORD>(static_cast<INT>(Pad2 - Count) * Fraction + static_cast<FLOAT>(Count));
}

FStatRecord& FStatRecord::Main()
{
    static FStatRecord Frame(TEXT("Frame"), static_cast<FStatRecord*>(nullptr));
    return Frame;
}

FStatRecord& FStatRecord::Ignore()
{
    static FStatRecord IgnoreRecord(TEXT("Ignore"), &Main());
    return IgnoreRecord;
}

void FStatIterator::operator++()
{
    if (bDone)
        return;
    if (Record->Children)
    {
        Record = Record->Children;
        return;
    }
    if (Record->NextChild)
    {
        Record = Record->NextChild;
        return;
    }
    // up until a parent has a next sibling
    do
    {
        Record = Record->Parent;
        if (!Record)
            return;
    } while (!Record->NextChild);
    Record = Record->NextChild;
}

FStats::FStats()
    : Unknown0(1), Unknown1(0), Unknown2(0), Unknown4(1.f), Unknown8(0.f)
{
}

void FStats::Clear()
{
    Unknown8 = 0.f;
    UnknownC.Empty();
}

void FStats::Tick()
{
    if (Unknown8 == 0.f)
    {
        for (FStatIterator It(&FStatRecord::Main()); It; ++It)
        {
            It->Count = 0;
            It->DWORD0 = 0;
            It->Pad2 = 0;
            It->Pad1 = 0;
        }
        Unknown8 = 1e-6f;
    }

    FStatRecord& Frame = FStatRecord::Main();
    if (Unknown0 && Frame.DWORD0)
    {
        // the frame's own cycles, outside any child
        static FStatRecord Misc(TEXT("Misc"), &Frame);
        const DWORD OwnCycles = Frame.DWORD0;
        Frame.Tick();
        const FLOAT FrameSeconds = static_cast<FLOAT>(Frame.DWORD0) * static_cast<FLOAT>(GSecondsPerCycle);
        Misc.DWORD0 = OwnCycles;
        Unknown8 += FrameSeconds;

        if (!Unknown2)
        {
            UnknownC.Empty();
        }
        else
        {
            INT FrameRate = appRound(1.f / FrameSeconds);
            if (FrameRate >= 0)
            {
                if (FrameRate > 254)
                    FrameRate = 255;
                if (FrameRate > 0)
                    UnknownC.AddItem(static_cast<BYTE>(FrameRate));
            }
        }

        if (Unknown4 != 0.f)
        {
            FLOAT Window = Unknown8;
            if (Unknown4 >= 0.f && Unknown4 <= Unknown8)
                Window = Unknown4;
            if (FrameSeconds < Window)
                Frame.UpdateAverage(1.f - FrameSeconds / Window);
        }

        if (!Unknown1 || Frame.Pad1 < Frame.DWORD0)
        {
            for (FStatIterator It(&Frame); It; ++It)
            {
                It->Pad1 = It->DWORD0;
                It->Pad2 = It->Count;
            }
        }
    }

    for (FStatIterator It(&FStatRecord::Main()); It; ++It)
    {
        It->Count = 0;
        It->DWORD0 = 0;
    }
}
