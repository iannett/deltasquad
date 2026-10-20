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
