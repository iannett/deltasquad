#include <cfloat>

#include "EngineClasses.h"
#include "UnEngineGlobals.h"

// ---------------------------------------------------------------------------------------------- old formats

namespace
{
    // faces before lod mesh version 2, with a uv per corner
    struct FLodMeshLegacyFace
    {
        _WORD Wedges[3];
        FLOAT UVs[3][2];
        INT   Unknown20;
        INT   Unknown24;

        // 0x0003fa26
        friend FArchive& operator<<(FArchive& Ar, FLodMeshLegacyFace& F)
        {
            Ar << F.Wedges[0] << F.Wedges[1] << F.Wedges[2];
            for (INT i = 0; i < 3; i++)
                Ar << F.UVs[i][0] << F.UVs[i][1];
            return Ar << F.Unknown20 << F.Unknown24;
        }
    };

    // read and dropped by packages before version 138
    struct FSkeletalMeshLegacy14
    {
        FName   Name;
        FVector Unknown4;
        INT     Unknown10;

        // 0x00042782
        friend FArchive& operator<<(FArchive& Ar, FSkeletalMeshLegacy14& L)
        {
            return Ar << L.Name << L.Unknown4 << L.Unknown10;
        }
    };

    struct FSkeletalMeshLegacy1C
    {
        FName   Name;
        FVector Unknown4;
        FVector Unknown10;

        // 0x000427b1
        friend FArchive& operator<<(FArchive& Ar, FSkeletalMeshLegacy1C& L)
        {
            return Ar << L.Name << L.Unknown4 << L.Unknown10;
        }
    };

    // influence dwords of a skin entry, at most 3
    DWORD SkinInfluenceCount(DWORD Tag)
    {
        const DWORD Num = (Tag & 7) + 1;
        return Num > 3 ? 3 : Num;
    }

    // a group of skin entries with the same number of influences in PackSkinData
    struct FSkinGroup
    {
        INT Count;
        INT Start;
        INT Next;
        INT NextOrder;
        INT Size;
    };

    // the entries that refer back to one weighted entry in PackSkinData
    struct FSkinLink
    {
        SWORD        Count;
        SWORD        Indices[15];
        const DWORD* Entries[15];
    };

    // 0x00050be9. rewrites a lod model's skin data into groups by influence count, each group headed by its
    // count, and puts the points in the order of the new entries. an entry is a header dword with the
    // influence count in bits 28 to 30, more influence dwords, and two dwords of data. a top nibble of 15
    // marks a link to an earlier entry instead, by its position in the data divided by 6.
    void PackSkinData(TArray<DWORD>& SkinData, TArray<FSkinPoint>* Points)
    {
        FSkinGroup Groups[10];
        memset(Groups, 0, sizeof(Groups));
        for (INT g = 0; g < 10; g++)
            Groups[g].Next = -1;

        TArray<FSkinLink> Links;
        Links.AddZeroed(3000);

        // count the entries in each group and gather the links
        const DWORD* Data = SkinData.GetData();
        const DWORD* P = Data;
        INT Entry = 0;
        INT MaxLinks = 0;
        while (*P != 0xffffffff)
        {
            const DWORD Tag = *P >> 28;
            if (Tag == 15)
            {
                FSkinLink& Link = Links[(*P & 0x0fffffff) / 6];
                Link.Indices[Link.Count] = static_cast<SWORD>(Entry);
                Link.Entries[Link.Count] = P;
                Link.Count++;
                if (MaxLinks < Link.Count)
                    MaxLinks = Link.Count;
                P += 1;
            }
            else
            {
                const DWORD Num = SkinInfluenceCount(Tag);
                Groups[Num].Count++;
                P += Num;
            }
            P += 2;
            Entry++;
        }

        // the size of each group
        INT NumOut = 0;
        const FSkinLink* Link = Links.GetData();
        for (P = Data; *P != 0xffffffff; P += 2)
        {
            const DWORD Value = *P;
            const DWORD Tag = Value >> 28;
            INT Group;
            INT Size;
            if (Tag == 15)
            {
                Group = 9;
                Size = 0;
                P += 1;
            }
            else
            {
                const DWORD Num = SkinInfluenceCount(Tag);
                Size = (Num >> 1) * 4 + 4;
                if (static_cast<INT>(Value) < 0)
                {
                    Size += Link->Count * 12 + 4;
                    Link++;
                }
                Size += 8;
                NumOut++;
                P += Num;
                Group = Num;
            }
            Groups[Group].Size += Size;
        }

        // place the groups after the two header dwords
        DWORD Offset = 8;
        INT First = 0;
        for (INT g = 0; g < 10; g++)
        {
            if (Groups[g].Count)
            {
                Groups[g].Size += 4;
                Groups[g].NextOrder = First;
                First += Groups[g].Count;
                Groups[g].Start = Offset >> 2;
                Groups[g].Next = Offset >> 2;
                Offset += Groups[g].Size;
            }
        }
        const DWORD TotalBytes = Offset + 4;
        const INT TotalDwords = TotalBytes >> 2;
        TArray<DWORD> Out;
        Out.AddZeroed(TotalDwords);

        for (INT g = 0; g < 10; g++)
        {
            if (Groups[g].Count)
            {
                const DWORD Header = g == 9 ? (Groups[9].Count & 0xffff) : ((Groups[g].Count & 0xffff) | (g << 16));
                Out[Groups[g].Next++] = Header;
            }
        }

        // write each entry into its group
        TArray<INT> Order;
        Order.AddZeroed(NumOut);
        INT OutIndex = 0;
        Entry = 0;
        Link = Links.GetData();
        P = Data;
        while (*P != 0xffffffff)
        {
            DWORD Value = *P;
            const DWORD Tag = Value >> 28;
            if (Tag == 15)
            {
                P += 1;
                Entry++;
            }
            else
            {
                const DWORD FullNum = (Tag & 7) + 1;
                const DWORD Num = SkinInfluenceCount(Tag);
                FSkinGroup& Group = Groups[Num];
                BYTE* Dest = reinterpret_cast<BYTE*>(&Out[Group.Next]);
                const UBOOL Linked = static_cast<INT>(Value) < 0;
                *reinterpret_cast<_WORD*>(Dest) = static_cast<_WORD>(Entry) | (Linked ? 0x8000 : 0);
                Order[Group.NextOrder++] = OutIndex;

                // each influence becomes a byte of its bits 20 and up and a byte of its low 12 bits over 6
                _WORD* W = reinterpret_cast<_WORD*>(Dest + 2);
                for (DWORD i = 0; i < Num; i++)
                {
                    P++;
                    *W++ = static_cast<_WORD>((((Value & 0xfff) / 6) << 8) | ((Value >> 20) & 0xff));
                    Value = *P;
                }
                P += FullNum - Num;
                if (!(Num & 1))
                    *W++ = 0;

                DWORD* D = reinterpret_cast<DWORD*>(W);
                D[0] = P[0];
                D[1] = P[1];
                D += 2;
                if (Linked)
                {
                    *D++ = Link->Count;
                    for (INT i = 0; i < Link->Count; i++)
                    {
                        D[0] = Link->Indices[i];
                        D[1] = Link->Entries[i][1];
                        D[2] = Link->Entries[i][2];
                        D += 3;
                    }
                    Link++;
                }
                Group.Next += static_cast<INT>((reinterpret_cast<BYTE*>(D) - Dest) >> 2);
                Entry++;
                OutIndex++;
            }
            P += 2;
        }

        if (Points)
        {
            TArray<FSkinPoint> Sorted;
            Sorted.AddZeroed(NumOut);
            for (INT i = 0; i < NumOut; i++)
                Sorted[i] = (*Points)[Order[i]];
            memcpy(Points->GetData(), Sorted.GetData(), NumOut * sizeof(FSkinPoint));
        }

        Out[0] = TotalBytes;
        Out[1] = Entry;
        Out[Out.Num() - 1] = 0x12345678;
        SkinData.Empty();
        SkinData.AddZeroed(TotalDwords);
        memcpy(SkinData.GetData(), Out.GetData(), Out.Num() * sizeof(DWORD));
    }
}

// ---------------------------------------------------------------------------------------------- mesh data

// 0x00041bfd
FArchive& operator<<(FArchive& Ar, FSkelMeshSection& S)
{
    Ar << S.MaterialIndex << S.Unknown2 << S.Unknown4 << S.Unknown6 << S.Unknown8;
    Ar << S.UnknownC << S.UnknownE << S.Unknown10 << S.Unknown12;
    // calls vtable slot 0x18 of the object with the archive
    if (S.Unknown1C)
        NOT_DECOMPILED(0x00041bfd);
    return Ar;
}

// 0x00047dfd
FArchive& operator<<(FArchive& Ar, FSkinVertexStream& S)
{
    S.FRenderResource::Serialize(Ar);
    if (!Ar.IsPersistent())
    {
        UObject* Skipped = nullptr;
        Ar << Skipped;
    }
    Ar << S.Revision;
    INT Skipped1 = 0;
    INT Skipped2 = 0;
    Ar << Skipped1 << Skipped2;
    SerializeStreamData(Ar, S.Vertices);
    return Ar;
}

// 0x0005132d
FArchive& operator<<(FArchive& Ar, FStaticLODModel& M)
{
    if (Ar.IsSaving())
        M.Unknown0 = 1;
    if (Ar.Ver() < 146)
    {
        if (Ar.IsLoading())
            M.Unknown0 = 0;
    }
    else
    {
        Ar << M.Unknown0;
        if (Ar.Ver() < 150)
            M.Unknown0 = 0;
    }

    Ar << M.SkinData;
    Ar << M.SkinPoints;
    Ar << M.Unknown14;
    Ar << M.Sections;
    Ar << M.Sections2;
    Ar << M.IndexBuffer;
    Ar << M.IndexBuffer2;
    Ar << M.VertexStream;
    M.Influences.Serialize(Ar);
    M.Wedges.Serialize(Ar);
    M.Faces.Serialize(Ar);
    M.Points.Serialize(Ar);
    // with lazy loading on, the raw data is never loaded at all
    if (Ar.IsLoading() && !Ar.ArUnknown16 && GLazyLoad)
    {
        Ar.DetachLazyLoader(&M.Influences);
        Ar.DetachLazyLoader(&M.Wedges);
        Ar.DetachLazyLoader(&M.Faces);
        Ar.DetachLazyLoader(&M.Points);
    }
    Ar << M.UnknownC8 << M.UnknownCC << M.UnknownD0 << M.UnknownD4 << M.UnknownD8 << M.UnknownDC;
    if (!Ar.IsPersistent())
        Ar << M.Unknown70;

    if (Ar.IsLoading() && !Ar.ArUnknown16 && M.SkinData.Num())
    {
        PackSkinData(M.SkinData, &M.SkinPoints);
        M.Unknown0 = 1;
    }
    return Ar;
}

// 0x00047e62
FArchive& operator<<(FArchive& Ar, FMeshAnimLinkup& L)
{
    if (Ar.Ver() < 152)
        L.Unknown0 = 1;
    else
        Ar << L.Unknown0;
    // a cleared flag drops the animation reference
    if (!L.Unknown0)
    {
        Ar.ArSkipObjectRefs = true;
        Ar << L.Anim;
        Ar.ArSkipObjectRefs = false;
    }
    else
    {
        Ar << L.Anim;
    }
    if (!Ar.IsPersistent())
        Ar << L.BoneMap;
    return Ar;
}

// 0x0004a812
void FMeshAnimLinkup::Setup(USkeletalMesh* Mesh)
{
    const INT NumBones = Mesh->RefSkeleton.Num();
    if (BoneMap.Num() == NumBones || !Anim)
        return;

    INT Matched = 0;
    BoneMap.Empty(NumBones);
    BoneMap.AddZeroed(NumBones);
    for (INT i = 0; i < NumBones; i++)
    {
        BoneMap[i] = INDEX_NONE;
        for (INT j = 0; j < Anim->RefBones.Num(); j++)
        {
            if (Mesh->RefSkeleton[i].Name == Anim->RefBones[j].Name)
            {
                Matched++;
                BoneMap[i] = j;
                break;
            }
        }
    }
    if (Matched == 0)
        GLog->Logf(TEXT("Warning: Unable to match any animation tracks from [%s] to skeletal mesh [%s]."), Anim->GetName(), Mesh->GetName());
}

// ---------------------------------------------------------------------------------------------- meshes

// 0x0007bca3
void UMesh::Serialize(FArchive& Ar)
{
    Super::Serialize(Ar);
    if (!Ar.IsPersistent())
        Ar << *reinterpret_cast<UObject**>(&DefMeshInstance);
}

UClass* ULodMesh::MeshGetInstanceClass()
{
    return ULodMeshInstance::StaticClass();
}

// 0x000412e8
void ULodMesh::Serialize(FArchive& Ar)
{
    Super::Serialize(Ar);
    if (Ar.IsSaving())
        LodMeshVersion = 8;
    Ar << LodMeshVersion << Unknown5C;
    Ar << Unknown60;
    if (LodMeshVersion < 2)
    {
        TArray<FLodMeshLegacyFace> OldFaces;
        Ar << OldFaces;
    }
    Ar << Unknown68;
    Ar << Unknown70 << Unknown7C << Unknown88;
    if (LodMeshVersion < 2)
    {
        TArray<_WORD> Skipped;
        Ar << Skipped;
    }
    Ar << Unknown94;
    Ar << Faces;
    Ar << UnknownA4;
    Ar << Wedges;
    Ar << Materials;
    Ar << UnknownBC << UnknownD0 << UnknownC0 << UnknownC4 << UnknownC8 << UnknownCC;
    if (LodMeshVersion > 2)
        Ar << UnknownE4 << UnknownE8;
    if (LodMeshVersion > 3)
        Ar << Unknown120;
    if (LodMeshVersion > 6)
        Ar << UnknownD4 << UnknownD8 << UnknownDC << UnknownE0;
}

// 0x0004ab2c
void USkeletalMesh::AddSkelAnim(UMeshAnimation* Anim, INT Flag)
{
    if (!Anim)
        return;
    for (INT i = 0; i < AnimLinkups.Num(); i++)
    {
        if (AnimLinkups[i].Anim == Anim)
            return;
    }
    FMeshAnimLinkup* Linkup = new(&AnimLinkups[AnimLinkups.AddZeroed()]) FMeshAnimLinkup;
    Linkup->Anim = Anim;
    Linkup->Unknown0 = Flag;
    Linkup->Setup(this);
}

UClass* USkeletalMesh::MeshGetInstanceClass()
{
    return USkeletalMeshInstance::StaticClass();
}

// 0x0004aa60
void USkeletalMesh::ActualizeAnimLinkups()
{
    for (INT i = 0; i < AnimLinkups.Num(); i++)
        AnimLinkups[i].Setup(this);
}

// 0x00045672
INT USkeletalMesh::MatchRefBone(FName Name, FMatrix* AliasOffset)
{
    if (Name == NAME_None)
        return -1;
    for (INT i = 0; i < AttachAliases.Num(); i++)
    {
        if (AttachAliases[i].Alias == Name)
        {
            Name = AttachAliases[i].BoneName;
            if (AliasOffset)
                *AliasOffset = AttachAliases[i].Offset;
            break;
        }
    }
    for (INT i = 0; i < RefSkeleton.Num(); i++)
    {
        if (RefSkeleton[i].Name == Name)
            return i;
    }
    return -1;
}

// 0x00051a68
void USkeletalMesh::PostLoad()
{
    UObject::PostLoad();
    MoveBone = RefSkeleton.Num() >= 1 && RefSkeleton[0].Name == FName(TEXT("Move")) ? 0 : -1;
    if (RefSkeleton.Num())
        RefSkeleton[0].ParentIndex = -1;
    // bones follow their parents, so each bone's counts are cleared before its children add to them
    for (INT i = 0; i < RefSkeleton.Num(); i++)
    {
        RefSkeleton[i].Depth = 0;
        RefSkeleton[i].NumChildren = 0;
        for (INT Parent = RefSkeleton[i].ParentIndex; Parent >= 0; Parent = RefSkeleton[Parent].ParentIndex)
        {
            RefSkeleton[i].Depth++;
            RefSkeleton[Parent].NumChildren++;
        }
    }
    if (LodMeshVersion < 2)
        NOT_DECOMPILED(0x00051b38);
    if (!LODModels.Num())
        NOT_DECOMPILED(0x00051b4d);
    // the client's skeletal detail setting drops the finest lod models
    if (GEngine && GEngine->Client)
        NOT_DECOMPILED(0x00051bfc);
    // the rest makes xbox push buffers for each lod model's sections
}

// 0x000455a6
const FMeshAnimSeq* USkeletalMesh::GetAnimNamed(FName Name, UBOOL bRandom)
{
    if (Name == NAME_None)
        return nullptr;
    for (INT i = 0; i < AnimLinkups.Num(); i++)
    {
        UMeshAnimation* Anim = AnimLinkups[i].Anim;
        if (!Anim)
            continue;
        if (bRandom)
        {
            for (INT j = 0; j < Anim->MultiAnims.Num(); j++)
            {
                if (Anim->MultiAnims[j].Name == Name)
                    return Anim->MultiAnims[j].SelectAnim();
            }
        }
        for (INT j = 0; j < Anim->Moves.Num(); j++)
        {
            if (Anim->Moves[j].Name == Name)
                return &Anim->Moves[j];
        }
    }
    return nullptr;
}

// 0x000522f6
void USkeletalMesh::Serialize(FArchive& Ar)
{
    Super::Serialize(Ar);
    Ar << Unknown1D0;
    Ar << RefSkeleton;
    // before version 142 every bone but the root has its rotation inverted
    if (Ar.Ver() < 142)
    {
        for (INT i = 1; i < RefSkeleton.Num(); i++)
        {
            FQuat& Q = RefSkeleton[i].Orientation;
            Q = FQuat(0.f - Q.X, 0.f - Q.Y, 0.f - Q.Z, Q.W);
        }
    }

    if (!Ar.IsLoading() || LodMeshVersion >= 5)
    {
        Ar << AnimLinkups;
        for (INT i = 0; i < AnimLinkups.Num(); i++)
        {
            if (!AnimLinkups[i].Anim)
            {
                AnimLinkups.Remove(i, 1);
                i--;
            }
        }
    }
    else
    {
        // older meshes name a single animation
        UMeshAnimation* Anim = nullptr;
        Ar << Anim;
        AddSkelAnim(Anim, 1);
    }
    Ar << Unknown1BC;
    Ar << Unknown1D8;
    Ar << Unknown1E0;

    if (Ar.Ver() < 140)
    {
        // attach aliases were three separate arrays
        TArray<FName> Aliases;
        TArray<FName> BoneNames;
        TArray<FMatrix> Offsets;
        Ar << Aliases << BoneNames << Offsets;
        const INT Count = Aliases.Num();
        if (Count < AttachAliases.Num())
            AttachAliases.Remove(Count, AttachAliases.Num() - Count);
        else
            AttachAliases.AddZeroed(Count - AttachAliases.Num());
        for (INT i = 0; i < AttachAliases.Num(); i++)
        {
            AttachAliases[i].Alias = Aliases[i];
            AttachAliases[i].BoneName = BoneNames[i];
            AttachAliases[i].Offset = Offsets[i];
        }
    }
    else
    {
        Ar << AttachAliases;
    }

    if (LodMeshVersion < 2)
    {
        TArray<FSkelMeshRigidPart> Skipped1;
        TArray<FSkelMeshRigidPart> Skipped2;
        TArray<_WORD> Skipped3;
        Ar << Skipped1 << Skipped2 << Skipped3;
    }
    else
    {
        // the raw mesh data always stays in the file until asked for
        const UBOOL SavedLazyLoad = GLazyLoad;
        GLazyLoad = 1;
        if (LodMeshVersion < 6)
            Unknown1C4 = 0;
        else
            Ar << Unknown1C4;
        Ar << LODModels;
        if (LodMeshVersion < 5)
        {
            UObject* Skipped = nullptr;
            Ar << Skipped;
        }
        Points.Serialize(Ar);
        RawWedges.Serialize(Ar);
        RawFaces.Serialize(Ar);
        RawInfluences.Serialize(Ar);
        Unknown18C.Serialize(Ar);
        Unknown1A0.Serialize(Ar);
        GLazyLoad = SavedLazyLoad;
    }

    if (LodMeshVersion > 7)
        Ar << Unknown29C << Unknown2A8;
    if (!Ar.IsPersistent())
    {
        Ar << Unknown288;
        for (INT i = 0; i < 4; i++)
            Ar << Unknown1F0[i];
        Ar << Unknown280;
    }
    if (Ar.Ver() >= 120)
        Ar << Unknown124;
    if (Ar.Ver() > 133)
    {
        Ar << Unknown298;
        if (Ar.Ver() < 138)
        {
            TArray<FSkeletalMeshLegacy14> Skipped1;
            TArray<FSkeletalMeshLegacy1C> Skipped2;
            TArray<UObject*> Skipped3;
            Ar << Skipped1 << Skipped2 << Skipped3;
        }
    }
}

FBox USkeletalMesh::GetCollisionBoundingBox(const AActor* Owner) const
{
    // a skeletal instance can keep its own box at +0x180 (the flags at +0x178/+0x17a and an array at +0x168 decide)
    if (Owner->MeshInstance && Owner->MeshInstance->IsA(USkeletalMeshInstance::StaticClass()))
        NOT_DECOMPILED(0x00168373);
    return UPrimitive::GetCollisionBoundingBox(Owner);
}

UBOOL USkeletalMesh::PointCheck(FCheckResult& Result, AActor* Owner, const FVector& Location, const FVector& Extent, DWORD ExtraNodeFlags)
{
    NOT_DECOMPILED(0x00168308);
    return 1;
}

void USkeletalMeshInstance::UpdateAttachments()
{
    NOT_DECOMPILED(0x00048cfe);
}

// ---------------------------------------------------------------------------------------------- mesh instances

void KInitSkeletonKarma(USkeletalMeshInstance* Instance);

// 0x0007bd00. an instance still being updated is told to delete itself instead of being deleted.
UMeshInstance* UMesh::MeshGetInstance(const AActor* Owner)
{
    UMeshInstance** Instance;
    if (!Owner || Owner->bDeleteMe)
    {
        Instance = &DefMeshInstance;
        if (*Instance)
            return *Instance;
    }
    else
    {
        Instance = &const_cast<AActor*>(Owner)->MeshInstance;
        if (*Instance)
        {
            if ((*Instance)->GetActor() == Owner && (*Instance)->Mesh == this)
                return *Instance;
            if ((*Instance)->GetStatus() & 1)
                (*Instance)->SetStatus(2);
            else
                delete *Instance;
        }
    }
    UClass* InstanceClass = MeshGetInstanceClass();
    if (!InstanceClass)
        InstanceClass = UMeshInstance::StaticClass();
    *Instance = static_cast<UMeshInstance*>(StaticConstructObject(InstanceClass, GetOuter(), NAME_None, RF_Transient | RF_Public | 0x80000, nullptr, GError));
    (*Instance)->SetActor(const_cast<AActor*>(Owner));
    (*Instance)->SetMesh(this);
    return *Instance;
}

// 0x000407ee
void ULodMeshInstance::SetMesh(UMesh* InMesh)
{
    Mesh = Cast<ULodMesh>(InMesh);
}

// 0x00040811
UBOOL ULodMeshInstance::PlayAnim(const FPlayAnim& Anim)
{
    const FMeshAnimSeq* Seq = Mesh->GetAnimNamed(Anim.Sequence, 1);
    if ((!Seq && Anim.Sequence != NAME_None) || !GetActor())
        return 0;
    AActor* Owner = GetActor();
    FAnimChannel* Channel = GetChannel(Anim);
    if (!Channel)
        return 0;

    // a quarter second, or half a sequence that doesn't loop, but never shorter than the sequence asks
    FLOAT TweenTime = 0.25f;
    if (Seq)
    {
        if (!Anim.bLoop)
        {
            const FLOAT Half = (Seq->NumFrames / Seq->Rate) * 0.5f;
            if (Half < 0.25f)
                TweenTime = Half;
        }
        if (TweenTime < Seq->MinTweenTime)
            TweenTime = Seq->MinTweenTime;
    }

    UBOOL bStarted = 0;
    if (Channel->Sequence == Anim.Sequence && (Channel->Rate != 0.f || Anim.Rate == 0.f))
    {
        // already playing: only jump to the frame asked for
        if (Anim.Frame >= 0.f)
            Channel->StartFrame = Channel->Frame = Anim.Frame;
    }
    else
    {
        Channel->Sequence = Anim.Sequence;
        Channel->AnimSeq = Seq;
        if (!Seq)
        {
            // stopping a channel tweens the channels before it at its rate
            Channel->LastFrame = 0.f;
            for (INT i = 0; i < Channels.Num() && &Channels[i] != Channel; i++)
            {
                Channels[i].TweenRate = Channel->TweenRate;
                const FLOAT Alpha = Channel->TweenRate == 0.f ? 1.f : 0.f;
                Channels[i].StartTweenAlpha = Alpha;
                Channels[i].TweenAlpha = Alpha;
            }
        }
        else
        {
            Channel->Alpha = 0.f;
            Channel->LastFrame = 1.f - 1.f / Seq->NumFrames;
            if (Anim.Frame < 0.f)
            {
                if (Seq->bRandomStart)
                    Channel->StartFrame = Channel->Frame = appRand() * (1.f / 32767.f);
                else if (Channel->Rate <= 0.f || !Channel->bLoop || !Anim.bLoop)
                    Channel->StartFrame = Channel->Frame = 0.f;
            }
            else
            {
                Channel->StartFrame = Channel->Frame = Anim.Frame;
            }
        }
        Channel->RateOffset = 0.f;
        if (TweenTime == 0.f || !bTween || (Anim.Sequence != NAME_None && Anim.Frame >= 0.f))
        {
            Channel->TweenRate = 0.f;
            Channel->TweenAlpha = 1.f;
        }
        else
        {
            Channel->TweenRate = 1.f / TweenTime;
            Channel->TweenAlpha = 0.f;
        }
        Channel->StartTweenAlpha = Channel->TweenAlpha;
        bStarted = 1;
    }

    Channel->bLoop = Anim.bLoop;
    Channel->Rate = Anim.Rate;
    Channel->BlendAlpha = Seq ? Anim.BlendAlpha : 0.f;
    if (Channel->BlendRate == 0.f)
        Channel->Alpha = Channel->BlendAlpha;
    if (TweenTime == 0.f)
        Channel->TweenAlpha = 1.f;
    if (bStarted)
    {
        PostInitAnim(*Channel);
        const INT Index = static_cast<INT>(Channel - &Channels[0]);
        Owner->ReplicateAnim(Index, Index, *Channel, 1);
    }
    Unknown61 = 0;
    return 1;
}

// 0x00040b00
void ULodMeshInstance::UpdateAnimation(FLOAT DeltaSeconds)
{
    SetStatus(1);
    if (DeltaSeconds != 0.f && !(GetStatus() & 2))
    {
        for (INT i = 0; i < Channels.Num(); i++)
        {
            FAnimChannel* Channel = &Channels[i];
            Channel->TweenAlpha = Channel->TweenRate * DeltaSeconds + Channel->TweenAlpha;
            if (Channel->TweenAlpha > 1.f)
                Channel->TweenAlpha = 1.f;
            // the blend alpha moves to its target at BlendRate
            if (Channel->BlendAlpha > Channel->Alpha)
                Channel->Alpha = Min(Channel->BlendRate * DeltaSeconds + Channel->Alpha, Channel->BlendAlpha);
            else if (Channel->Alpha > Channel->BlendAlpha)
                Channel->Alpha = Max(Channel->Alpha - Channel->BlendRate * DeltaSeconds, Channel->BlendAlpha);

            // the time left is used up across notifies, loops and the end, at most four times
            FLOAT TimeLeft = DeltaSeconds;
            INT Steps = 0;
            while (Channel->Sequence != NAME_None && Channel->Rate != 0.f && TimeLeft > 0.f && ++Steps <= 4)
            {
                const FMeshAnimSeq* Seq = Channel->AnimSeq;
                const FLOAT OldFrame = Channel->Frame;
                if (!Seq)
                    continue;
                if (Seq->RateJitter > 0.f)
                {
                    Channel->RateOffset += (appRand() * (1.f / 16383.5f) - 1.f) * Seq->RateJitter * TimeLeft * 10.f;
                    Channel->RateOffset = Clamp(Channel->RateOffset, 0.f - Seq->RateJitter, Seq->RateJitter);
                }
                Channel->Frame += ((Channel->RateOffset + 1.f) / Seq->NumFrames) * Seq->Rate * Channel->Rate * TimeLeft;

                // stop at the nearest notify passed, then carry on with the time after it
                if (Seq->Notifys.Num() && GetActor()->Mesh && !Channel->bNoEvents)
                {
                    INT Best = -1;
                    FLOAT BestDistance = 100000.f;
                    for (INT j = 0; j < Seq->Notifys.Num(); j++)
                    {
                        const FLOAT Time = *reinterpret_cast<const FLOAT*>(&Seq->Notifys[j].Unknown0);
                        if (OldFrame < Time && Time <= Channel->Frame && (Best == -1 || Time - OldFrame < BestDistance))
                        {
                            Best = j;
                            BestDistance = Time - OldFrame;
                        }
                    }
                    if (Best >= 0)
                    {
                        const FMeshAnimNotify& Notify = Seq->Notifys[Best];
                        const FLOAT NewFrame = Channel->Frame;
                        const FLOAT Time = *reinterpret_cast<const FLOAT*>(&Notify.Unknown0);
                        Channel->Frame = Time;
                        TimeLeft = ((NewFrame - Time) * TimeLeft) / (NewFrame - OldFrame);
                        if (UAnimNotify* Object = static_cast<UAnimNotify*>(Notify.Notify))
                        {
                            Object->Notify(this, GetActor());
                            if (Channels.Num() <= i)
                                return;
                            Channel = &Channels[i];
                        }
                        continue;
                    }
                }

                if (Channel->Frame < Channel->LastFrame)
                    break;
                if (!Channel->bLoop)
                {
                    TimeLeft = ((Channel->Frame - Channel->LastFrame) * TimeLeft) / (Channel->Frame - OldFrame);
                    Channel->Frame = Channel->LastFrame;
                    if (Channel->Rate <= 0.f)
                        continue;
                    // a finished channel other than the base one is stopped
                    if (Channel->StartBone > 0 || Channel->Channel > 0)
                    {
                        FPlayAnim Stop = *Channel;
                        Stop.Sequence = NAME_None;
                        PlayAnim(Stop);
                    }
                    Channel->Rate = 0.f;
                }
                else
                {
                    if (Channel->Frame >= 1.f)
                    {
                        TimeLeft = ((Channel->Frame - 1.f) * TimeLeft) / (Channel->Frame - OldFrame);
                        Channel->Frame = 0.f;
                    }
                    else
                    {
                        TimeLeft = 0.f;
                    }
                    if (Channel->LastFrame <= OldFrame)
                        continue;
                }
                if (!Channel->bNoEvents)
                    GetActor()->NotifyAnimEnd(Channel->Channel);
            }
            GetActor()->ReplicateAnim(i, i, *Channel, 1);
        }
    }

    Unknown61 = 0;
    if (GetStatus() & 2)
        delete this;
    else
        SetStatus(0);
}

// 0x00040f4b
UBOOL ULodMeshInstance::IsAnimating(INT Channel)
{
    if (Channel >= Channels.Num() || Channel < 0)
        return 0;
    return Channels[Channel].Sequence != NAME_None && Channels[Channel].Rate != 0.f;
}

// 0x0004126a
void ULodMeshInstance::DebugAnim(FOutputDevice& Out)
{
    for (INT i = 0; i < Channels.Num(); i++)
    {
        const FAnimChannel& C = Channels[i];
        if (C.Sequence != NAME_None)
            Out.Logf(TEXT("Sequence: %s Channel: %d Bone: %s Time: %.2f Alpha: %.2f Tween Alpha: %.2f Rate: %.2f\n"), *C.Sequence,
                     C.Channel, *C.Bone, C.Frame, C.Alpha, C.TweenAlpha, C.Rate);
    }
}

// 0x00040f8e
void ULodMeshInstance::StopAnimating(UBOOL ClearAllButBase)
{
    for (INT i = 0; i < Channels.Num(); i++)
        Channels[i].Rate = 0.f;
    if (ClearAllButBase)
    {
        for (INT i = 1; i < Channels.Num(); i++)
        {
            Channels[i].Alpha = 0.f;
            Channels[i].BlendAlpha = 0.f;
            Channels[i].Sequence = NAME_None;
            Channels[i].AnimSeq = nullptr;
        }
        Unknown61 = 0;
    }
}

// 0x0004100c
UBOOL ULodMeshInstance::IsPlayingAnim(FName Sequence, FName Bone)
{
    for (INT i = 0; i < Channels.Num(); i++)
    {
        if (Channels[i].Sequence == Sequence && (Bone == NAME_None || Channels[i].Bone == Bone) && Channels[i].Rate != 0.f)
            return 1;
    }
    return 0;
}

// 0x000404d0
UBOOL ULodMeshInstance::IsAnimLooping(INT Channel)
{
    if (Channel >= Channels.Num() || Channel < 0)
        return 0;
    return Channels[Channel].bLoop;
}

// 0x000404f6
UBOOL ULodMeshInstance::IsAnimPastLastFrame(INT Channel)
{
    if (Channel >= Channels.Num() || Channel < 0)
        return 0;
    return !(Channels[Channel].Frame < Channels[Channel].LastFrame);
}

// 0x0004052d
UBOOL ULodMeshInstance::AnimStopLooping(INT Channel)
{
    if (Channel >= Channels.Num() || Channel < 0)
        return 0;
    Channels[Channel].bLoop = 0;
    return 1;
}

// 0x0004105a. GetChannel makes the channel when there is none on the bone yet.
FPlayAnim ULodMeshInstance::GetActiveAnim(FName Bone)
{
    FPlayAnim Anim;
    Anim.Bone = Bone;
    Anim.Rate = 1.f;
    if (FAnimChannel* Channel = GetChannel(Anim))
        Anim = *Channel;
    return Anim;
}

// 0x0004aa9c
void USkeletalMeshInstance::SetMesh(UMesh* InMesh)
{
    USkeletalMesh* NewMesh = Cast<USkeletalMesh>(InMesh);
    if (Mesh == NewMesh)
        return;
    Mesh = NewMesh;
    // the arrays at +0xf8, +0x100, +0x168 and +0x170 are emptied here too, once they are in the port
    Directors.Empty();
    SpaceBases.Empty();
    NewMesh->ActualizeAnimLinkups();
    KInitSkeletonKarma(this);
    Unknown61 = 0;
}

// 0x000434c9
void USkeletalMeshInstance::UpdateAnimation(FLOAT DeltaSeconds)
{
    ULodMeshInstance::UpdateAnimation(DeltaSeconds);
    if (Directors.Num())
        NOT_DECOMPILED(0x000434d7);
}

// 0x00047f6b. clearing also drops the directors that aren't kept (+0x4a).
void USkeletalMeshInstance::StopAnimating(UBOOL ClearAllButBase)
{
    ULodMeshInstance::StopAnimating(ClearAllButBase);
    if (ClearAllButBase && Directors.Num())
        NOT_DECOMPILED(0x00047f7d);
}

// 0x0004a914
FAnimChannel* USkeletalMeshInstance::GetChannel(const FPlayAnim& Anim)
{
    USkeletalMesh* SkelMesh = static_cast<USkeletalMesh*>(Mesh);
    const FName Bone = Anim.Bone == NAME_None || Channels.Num() < 1 ? SkelMesh->RefSkeleton[0].Name : Anim.Bone;
    for (INT i = 0; i < Channels.Num(); i++)
    {
        if (Channels[i].Bone == Bone && Channels[i].Channel == Anim.Channel)
            return &Channels[i];
    }

    const INT BoneIndex = SkelMesh ? SkelMesh->MatchRefBone(Bone) : -1;
    if (BoneIndex < 0)
        return nullptr;
    // channels stay sorted by channel number, then by bone
    INT Index = 0;
    for (; Index < Channels.Num(); Index++)
    {
        if (Anim.Channel < Channels[Index].Channel || (Channels[Index].Channel == Anim.Channel && BoneIndex < Channels[Index].StartBone))
            break;
    }
    Channels.InsertZeroed(Index);
    FAnimChannel* Channel = &Channels[Index];
    Channel->Bone = Bone;
    Channel->StartBone = BoneIndex;
    Channel->EndBone = SkelMesh->RefSkeleton[BoneIndex].NumChildren + 1 + BoneIndex;
    Channel->Channel = Anim.Channel;
    return Channel;
}

// 0x000430f4. undoes the root's pose so far from where the actor stands.
void USkeletalMeshInstance::PostInitAnim(FAnimChannel& Channel)
{
    if (GetMoveCoords(&MoveStart, 0))
        MoveStart = MoveStart.Inverse() * GetActor()->LocalToWorld();
}

// 0x00043af1
UBOOL USkeletalMeshInstance::IsChannelActive(INT Channel, INT Bone)
{
    if (Channel < 0 || Channel >= Channels.Num())
        return 0;
    const FAnimChannel& C = Channels[Channel];
    if (Channel != 0 && !(C.Alpha > 0.f))
        return 0;
    if (Bone >= 0 && (C.StartBone > Bone || C.EndBone <= Bone))
        return 0;
    for (INT i = Channel + 1; i < Channels.Num(); i++)
    {
        const FAnimChannel& Later = Channels[i];
        if (Later.Alpha * Later.TweenAlpha != 1.f)
            continue;
        if (Bone >= 0)
        {
            if (Later.StartBone <= Bone && Bone < Later.EndBone)
                return 0;
        }
        else if (C.StartBone >= Later.StartBone && C.EndBone <= Later.EndBone)
        {
            return 0;
        }
    }
    return 1;
}

// 0x00043bb3. root motion comes from base channels on the move bone whose sequence moves the root.
UBOOL USkeletalMeshInstance::GetMoveCoords(FMatrix* Out, UBOOL bWorld)
{
    USkeletalMesh* SkelMesh = static_cast<USkeletalMesh*>(Mesh);
    INT Count = 0;
    for (INT i = 0; i < Channels.Num(); i++)
    {
        const FAnimChannel& C = Channels[i];
        if (C.bLoop || C.StartBone != SkelMesh->MoveBone || !IsChannelActive(i, -1) || C.Rate == 0.f)
            continue;
        const FSkelAnimSeq* Seq = static_cast<const FSkelAnimSeq*>(GetSequence(i));
        if (!Seq || !(Seq->Unknown54 & 1))
            continue;
        if (Out)
        {
            INT LODRequest;
            GetFrame(GetActor(), nullptr, nullptr, 0, LODRequest, 3);
            *Out = SpaceBases[0];
            if (bWorld)
                *Out = *Out * MoveStart;
        }
        Count++;
    }
    return Count > 0;
}

// 0x0004c082
void USkeletalMeshInstance::GetFrame(AActor* Owner, FLevelSceneNode* SceneNode, FVector* Verts, INT Size, INT& LODRequest, DWORD TaskFlag)
{
    ApplyAnimation(Owner, TaskFlag == 3);
}

void USkeletalMeshInstance::ApplyAnimation(const AActor* Owner, UBOOL bUnknown)
{
    NOT_DECOMPILED(0x0004adcb);
}

UBOOL UMeshInstance::PointCheck(FCheckResult& Result, AActor* Owner, const FVector& Location, const FVector& Extent, DWORD ExtraNodeFlags)
{
    NOT_DECOMPILED(0x0003831d);
    return 1;
}

FBox UMeshInstance::GetCollisionBoundingBox(const AActor* Owner) const
{
    NOT_DECOMPILED(0x00038369);
    return FBox{};
}

// ---------------------------------------------------------------------------------------------- mesh animations

namespace
{
    // 0x0045f80c, 0x0045f810 and 0x0045f814, set at startup. no packed component is larger than sqrt(0.5).
    const FLOAT GQuatPackMax = static_cast<FLOAT>(sqrt(0.5));
    const INT GQuatPackScale = static_cast<INT>(32767.f / GQuatPackMax);
    const FLOAT GQuatUnpackScale = 1.f / static_cast<FLOAT>(GQuatPackScale);

    // 0x0045f808. the frame count of the sequence being loaded, for the last key of its old tracks.
    INT GOldSeqNumFrames;

    FLOAT Largest(FLOAT A, FLOAT B, FLOAT C)
    {
        const FLOAT BC = B >= C ? B : C;
        return A >= BC ? A : BC;
    }

    // 0x00042b99. rotations before version 151 are a rotation axis scaled by the angle, pi per 32767.
    FQuat UnpackOldQuat(const FPackedQuat& P)
    {
        const FLOAT Scale = 3.14159265f / 32767.f;
        FVector V(static_cast<SWORD>(P.X) * Scale, static_cast<SWORD>(P.Y) * Scale, static_cast<SWORD>(P.Z) * Scale);
        const FLOAT Angle = V.Size();
        if (Angle > 0.f)
            V = V * static_cast<FLOAT>(sin(0.5 * Angle) / Angle);
        FLOAT WSquared = 1.f - V.X * V.X - V.Y * V.Y - V.Z * V.Z;
        if (WSquared < 0.f)
            WSquared = 0.f;
        return FQuat(V.X, V.Y, V.Z, appSqrt(WSquared));
    }
}

// 0x0004284b. the largest component is dropped after the rotation is turned towards the identity.
FPackedQuat::FPackedQuat(const FQuat& InQ)
{
    FQuat Q = InQ;
    Q.AlignWith(FQuat(0.f, 0.f, 0.f, 1.f));
    Q.Normalize();
    const FLOAT AbsX = fabsf(Q.X), AbsY = fabsf(Q.Y), AbsZ = fabsf(Q.Z), AbsW = fabsf(Q.W);
    const FLOAT Scale = static_cast<FLOAT>(GQuatPackScale);
    _WORD Sign;
    if (AbsW >= Largest(AbsX, AbsY, AbsZ))
    {
        X = appRound(Scale * Q.X) & ~1;
        Y = appRound(Scale * Q.Y) & ~1;
        Z = appRound(Scale * Q.Z) & ~1;
        Sign = Q.W < 0.f;
    }
    else if (AbsZ >= Largest(AbsW, AbsX, AbsY))
    {
        X = (appRound(Scale * Q.W) & ~1) | 1;
        Y = appRound(Scale * Q.X) & ~1;
        Z = appRound(Scale * Q.Y) & ~1;
        Sign = Q.Z < 0.f;
    }
    else if (AbsY >= Largest(AbsZ, AbsW, AbsX))
    {
        X = appRound(Scale * Q.Z) & ~1;
        Y = (appRound(Scale * Q.W) & ~1) | 1;
        Z = appRound(Scale * Q.X) & ~1;
        Sign = Q.Y < 0.f;
    }
    else if (AbsX >= Largest(AbsY, AbsZ, AbsW))
    {
        X = (appRound(Scale * Q.Y) & ~1) | 1;
        Y = (appRound(Scale * Q.Z) & ~1) | 1;
        Z = appRound(Scale * Q.W) & ~1;
        Sign = Q.X < 0.f;
    }
    else
    {
        // only reached with a nan; the words are left as they were
        return;
    }
    Z |= Sign;
}

// 0x00042d41
FQuat FPackedQuat::ToQuat() const
{
    const FLOAT A = static_cast<SWORD>(X & ~1) * GQuatUnpackScale;
    const FLOAT B = static_cast<SWORD>(Y & ~1) * GQuatUnpackScale;
    const FLOAT C = static_cast<SWORD>(Z & ~1) * GQuatUnpackScale;
    FLOAT D = appSqrt(1.f - A * A - B * B - C * C);
    if (Z & 1)
        D = 0.f - D;
    if (Y & 1)
        return (X & 1) ? FQuat(D, A, B, C) : FQuat(C, D, A, B);
    return (X & 1) ? FQuat(B, C, D, A) : FQuat(A, B, C, D);
}

// 0x00041e23
FPackedPos::FPackedPos(const FVector& V, FLOAT InvScale)
{
    const FLOAT Scale = InvScale * 32767.f;
    X = static_cast<SWORD>(appRound(V.X * Scale));
    Y = static_cast<SWORD>(appRound(V.Y * Scale));
    Z = static_cast<SWORD>(appRound(V.Z * Scale));
}

// inlined by AnalogTrack::Serialize
FVector FPackedPos::ToVector(FLOAT PosScale) const
{
    const FLOAT Scale = PosScale * (1.f / 32767.f);
    return FVector(X * Scale, Y * Scale, Z * Scale);
}

// 0x00042ea2
void AnalogTrack::UpdatePosScale(const FVector& Pos)
{
    const FLOAT AbsMax = Pos.GetAbsMax();
    if (PosScale < AbsMax)
        PosScale = AbsMax;
}

// 0x0004a330
void AnalogTrack::Serialize(FArchive& Ar)
{
    if (Ar.Ver() >= 141)
    {
        Ar << PosScale << KeyPos << KeyQuat << KeyTime;
        if (Ar.Ver() < 151)
        {
            for (INT i = 0; i < KeyQuat.Num(); i++)
                KeyQuat[i] = FPackedQuat(UnpackOldQuat(KeyQuat[i]));
        }
        return;
    }

    // older tracks have full keys and float frame times, packed here
    INT Unused;
    TArray<FQuat> OldQuats;
    TArray<FVector> OldPositions;
    TArray<FLOAT> OldTimes;
    Ar << Unused << OldQuats << OldPositions << OldTimes;
    KeyQuat.Set(OldQuats.Num());
    KeyPos.Set(OldPositions.Num());
    const INT NumTimes = OldTimes.Num();
    KeyTime.Set(NumTimes > 1 ? NumTimes : 0);

    for (INT i = 0; i < KeyQuat.Num(); i++)
        KeyQuat[i] = FPackedQuat(OldQuats[i]);
    PosScale = 0.f;
    for (INT i = 0; i < KeyPos.Num(); i++)
        UpdatePosScale(OldPositions[i]);
    const FLOAT InvScale = 1.f / (PosScale + FLT_MIN);
    for (INT i = 0; i < KeyPos.Num(); i++)
        KeyPos[i] = FPackedPos(OldPositions[i], InvScale);

    // times become byte deltas; a gap over 255 frames gets keys inserted, blended between its neighbours
    INT Old = 0;
    for (INT i = 0; i < KeyTime.Num(); i++, Old++)
    {
        const FLOAT Next = Old < NumTimes - 1 ? OldTimes[Old + 1] : static_cast<FLOAT>(GOldSeqNumFrames);
        INT Delta = appRound(Next - OldTimes[Old]);
        while (Delta > 255)
        {
            KeyTime.InsertZeroed(i);
            KeyTime[i] = 255;
            if (KeyQuat.Num() > 1)
            {
                KeyQuat.InsertZeroed(i + 1);
                KeyQuat[i + 1] = FPackedQuat(FQuat::Slerp(KeyQuat[i].ToQuat(), KeyQuat[i + 2].ToQuat(), 255.f / Delta));
            }
            if (KeyPos.Num() > 1)
            {
                KeyPos.InsertZeroed(i + 1);
                const FVector Prev = KeyPos[i].ToVector(PosScale);
                const FVector Next = KeyPos[i + 2].ToVector(PosScale);
                KeyPos[i + 1] = FPackedPos((Next - Prev) * (255.f / Delta) + Prev, InvScale);
            }
            Delta -= 255;
            i++;
        }
        KeyTime[i] = static_cast<BYTE>(Delta);
    }
}

// 0x0007bf13
void FMeshAnimSeq::Serialize(FArchive& Ar)
{
    Ar << Unknown2C << Name << Groups << Unknown0C << NumFrames << Notifys << Rate;
    Ar << RateJitter << MinTweenTime << bRandomStart;
    if (Ar.Ver() < 143 && MinTweenTime < 0.f)
        MinTweenTime = 0.f;
    if (Ar.Ver() < 144)
        RandomWeight = 1.f;
    else
        Ar << RandomWeight;
}

// 0x000512a8
void FSkelAnimSeq::Serialize(FArchive& Ar)
{
    FMeshAnimSeq::Serialize(Ar);
    if (Ar.Ver() < 143 && Ar.IsLoading())
    {
        INT Unused;
        Ar << Unused;
    }
    Ar << Unknown50 << Unknown54;
    if (Ar.Ver() < 143)
        Unknown58 = 0.f;
    else
        Ar << Unknown58;
    if (Ar.Ver() < 141)
        GOldSeqNumFrames = NumFrames;
    Ar << Tracks;
}

// 0x000522c4
void UMeshAnimation::Serialize(FArchive& Ar)
{
    Super::Serialize(Ar);
    Ar << Version << RefBones << Moves;
}

// 0x00051f89
void UMeshAnimation::PostLoad()
{
    Super::PostLoad();
    for (INT i = 0; i < Moves.Num(); i++)
    {
        FSkelAnimSeq& Move = Moves[i];
        Move.Owner = this;
        Move.UpdateOldNotifies(GetOuter());
        if (RefBones[0].Name != FName(TEXT("Move")) || Move.Unknown50 != 0)
            continue;

        const AnalogTrack& Root = Move.Tracks[0];
        Move.RootTranslation = Root.KeyPos[Root.KeyPos.Num() - 1].ToVector(Root.PosScale) - Root.KeyPos[0].ToVector(Root.PosScale);
        // the last key's rotation times the inverse of the first
        const FQuat A = Root.KeyQuat[Root.KeyQuat.Num() - 1].ToQuat();
        const FQuat First = Root.KeyQuat[0].ToQuat();
        const FQuat B(0.f - First.X, 0.f - First.Y, 0.f - First.Z, First.W);
        Move.RootRotation = FQuat(
            (A.Y * B.Z + A.W * B.X + B.W * A.X) - A.Z * B.Y,
            (A.W * B.Y - B.Z * A.X) + A.Y * B.W + A.Z * B.X,
            ((A.W * B.Z + B.Y * A.X) - A.Y * B.X) + A.Z * B.W,
            ((A.W * B.W - A.X * B.X) - A.Y * B.Y) - A.Z * B.Z);

        for (INT j = 1; j < Root.KeyPos.Num(); j++)
        {
            const FPackedPos& P = Root.KeyPos[j];
            if (P.X != Root.KeyPos[0].X || P.Y != Root.KeyPos[0].Y || P.Z != Root.KeyPos[0].Z)
            {
                Move.Unknown54 |= 1;
                break;
            }
        }
        if (Move.Unknown54 & 1)
            continue;
        for (INT j = 1; j < Root.KeyQuat.Num(); j++)
        {
            const FQuat Q0 = Root.KeyQuat[0].ToQuat();
            const FQuat Q = Root.KeyQuat[j].ToQuat();
            if (Q.X != Q0.X || Q.Y != Q0.Y || Q.Z != Q0.Z || Q.W != Q0.W)
            {
                Move.Unknown54 |= 1;
                break;
            }
        }
    }
    ConstructMultiAnims();
}

// 0x000517b9. a group holds every move named like another move plus trailing digits ("Idle", "Idle1", "Idle2"),
// and replaces the groups the moves were saved with.
void UMeshAnimation::ConstructMultiAnims()
{
    MultiAnims.Empty();
    for (INT i = 0; i < Moves.Num(); i++)
        Moves[i].Groups.Empty();

    for (INT i = 0; i < Moves.Num(); i++)
    {
        FMeshMultiAnim Multi;
        Multi.Name = Moves[i].Name;
        for (INT j = 0; j < Moves.Num(); j++)
        {
            FString Base(*Moves[j].Name);
            while (Base.Len() && appIsDigit((*Base)[Base.Len() - 1]))
                Base = Base.Left(Base.Len() - 1);
            if (FString(*Multi.Name) == Base)
            {
                Multi.Anims.AddItem(&Moves[j]);
                Moves[j].Groups.AddUniqueItem(Multi.Name);
            }
        }
        if (Multi.Anims.Num() > 1)
            MultiAnims.AddItem(Multi);
        else
            Moves[i].Groups.Empty();
    }

    for (INT i = 0; i < MultiAnims.Num(); i++)
    {
        FMeshMultiAnim& Multi = MultiAnims[i];
        Multi.TotalWeight = 0.f;
        for (INT j = 0; j < Multi.Anims.Num(); j++)
            Multi.TotalWeight += Multi.Anims[j]->RandomWeight;
    }
}

// 0x00042ecd. the last sequence takes whatever weight the others leave.
FMeshAnimSeq* FMeshMultiAnim::SelectAnim() const
{
    const FLOAT Pick = appRand() * TotalWeight * (1.f / 32767.f);
    FLOAT Sum = 0.f;
    INT i = 0;
    for (; i < Anims.Num() - 1; i++)
    {
        Sum += Anims[i]->RandomWeight;
        if (Pick < Sum)
            break;
    }
    return Anims[i];
}

// 0x0007bdba
void FMeshAnimSeq::UpdateOldNotifies(UObject* Outer)
{
    for (INT i = 0; i < Notifys.Num(); i++)
    {
        FMeshAnimNotify& N = Notifys[i];
        if (N.Unknown4 == NAME_None)
            continue;
        UAnimNotify_Script* Script = static_cast<UAnimNotify_Script*>(
            UObject::StaticConstructObject(UAnimNotify_Script::StaticClass(), Outer, NAME_None, RF_Public, nullptr, GError));
        Script->NotifyName = N.Unknown4;
        N.Notify = Script;
        N.Unknown4 = NAME_None;
    }
}

// ---------------------------------------------------------------------------------------------- anim notifies

void UAnimNotify_AnimLoopEnd::Notify(UMeshInstance* Instance, AActor* Owner)
{
    NOT_DECOMPILED(0x0004452d);
}

void UAnimNotify_BreakJoint::Notify(UMeshInstance* Instance, AActor* Owner)
{
    NOT_DECOMPILED(0x000506f9);
}

void UAnimNotify_CreateProjectile::Notify(UMeshInstance* Instance, AActor* Owner)
{
    NOT_DECOMPILED(0x0004938c);
}

void UAnimNotify_DestroyEffect::Notify(UMeshInstance* Instance, AActor* Owner)
{
    NOT_DECOMPILED(0x00046bdf);
}

void UAnimNotify_Effect::Notify(UMeshInstance* Instance, AActor* Owner)
{
    NOT_DECOMPILED(0x00049173);
}

void UAnimNotify_Footstep::Notify(UMeshInstance* Instance, AActor* Owner)
{
    NOT_DECOMPILED(0x00044685);
}

void UAnimNotify_GoRagdoll::Notify(UMeshInstance* Instance, AActor* Owner)
{
    NOT_DECOMPILED(0x0004966e);
}

void UAnimNotify_HolsterAttachment::Notify(UMeshInstance* Instance, AActor* Owner)
{
    NOT_DECOMPILED(0x00044600);
}

void UAnimNotify_LoadAttachment::Notify(UMeshInstance* Instance, AActor* Owner)
{
    NOT_DECOMPILED(0x000445a0);
}

void UAnimNotify_MatSubAction::Notify(UMeshInstance* Instance, AActor* Owner)
{
    NOT_DECOMPILED(0x0005059d);
}

void UAnimNotify_MeleeHit::Notify(UMeshInstance* Instance, AActor* Owner)
{
    NOT_DECOMPILED(0x00046d0f);
}

void UAnimNotify_ReleaseGrenade::Notify(UMeshInstance* Instance, AActor* Owner)
{
    NOT_DECOMPILED(0x00049a9b);
}

void UAnimNotify_Script::Notify(UMeshInstance* Instance, AActor* Owner)
{
    NOT_DECOMPILED(0x000495d0);
}

void UAnimNotify_Scripted::Notify(UMeshInstance* Instance, AActor* Owner)
{
    NOT_DECOMPILED(0x00046d03);
}

void UAnimNotify_Sound::Notify(UMeshInstance* Instance, AActor* Owner)
{
    NOT_DECOMPILED(0x00046c61);
}

void UAnimNotify_SoundCue::Notify(UMeshInstance* Instance, AActor* Owner)
{
    NOT_DECOMPILED(0x00046c8a);
}

void UAnimNotify_ThrowScav::Notify(UMeshInstance* Instance, AActor* Owner)
{
    NOT_DECOMPILED(0x00044499);
}

void UAnimNotify_ToggleFlashlight::Notify(UMeshInstance* Instance, AActor* Owner)
{
    NOT_DECOMPILED(0x000444f1);
}

void UAnimNotify_TurretFire::Notify(UMeshInstance* Instance, AActor* Owner)
{
    NOT_DECOMPILED(0x00044660);
}
