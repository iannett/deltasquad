#include <cfloat>

#include "EngineClasses.h"

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
        Ar << Unknown54;
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
    NOT_DECOMPILED(0x0016834b);
    return FBox{};
}

UBOOL USkeletalMesh::PointCheck(FCheckResult& Result, AActor* Owner, const FVector& Location, const FVector& Extent, DWORD ExtraNodeFlags)
{
    NOT_DECOMPILED(0x00168308);
    return 1;
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
    Ar << Unknown18 << Unknown1C << Unknown20;
    if (Ar.Ver() < 143 && Unknown1C < 0.f)
        Unknown1C = 0.f;
    if (Ar.Ver() < 144)
        Unknown30 = 1.f;
    else
        Ar << Unknown30;
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
