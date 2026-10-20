#pragma once

#include "UnRenderResource.h"

class UMeshAnimation;
class USkeletalMesh;

// ---------------------------------------------------------------------------------------------- lod meshes

// a triangle of wedges with its material
struct FMeshFace
{
    _WORD Wedges[3];
    _WORD MaterialIndex;

    // 0x0003fab0
    friend FArchive& operator<<(FArchive& Ar, FMeshFace& F)
    {
        return Ar << F.Wedges[0] << F.Wedges[1] << F.Wedges[2] << F.MaterialIndex;
    }
};

// a vertex index with a uv, so one vertex can carry several uvs
struct FMeshWedge
{
    _WORD VertexIndex;
    FLOAT U;
    FLOAT V;

    // 0x0003fa93. the uv uses the code FGenerationInfo's serializer folded into.
    friend FArchive& operator<<(FArchive& Ar, FMeshWedge& W)
    {
        return Ar << W.VertexIndex << W.U << W.V;
    }
};

struct FMeshMaterial
{
    DWORD PolyFlags;
    INT   TextureIndex;

    // 0x000c3165, the same code as FGenerationInfo's
    friend FArchive& operator<<(FArchive& Ar, FMeshMaterial& M)
    {
        return Ar << M.PolyFlags << M.TextureIndex;
    }
};

// stored by lod meshes from version 3 on
struct FLodMeshUnknownE8
{
    UObject* Unknown0;
    FVector  Unknown4;
    FVector  Unknown10;
    FVector  Unknown1C;
    FColor   Unknown28;
    INT      Unknown2C;
    INT      Unknown30;
    INT      Unknown34;

    // 0x0003faea
    friend FArchive& operator<<(FArchive& Ar, FLodMeshUnknownE8& U)
    {
        Ar << U.Unknown0 << U.Unknown1C << U.Unknown10 << U.Unknown4 << U.Unknown28;
        return Ar << U.Unknown2C << U.Unknown30 << U.Unknown34;
    }
};

// ---------------------------------------------------------------------------------------------- skeletal meshes

struct FMeshBone
{
    FName   Name;
    DWORD   Flags;
    FQuat   Orientation;
    FVector Position;
    FLOAT   Length;
    FLOAT   XSize;
    FLOAT   YSize;
    FLOAT   ZSize;
    INT     ParentIndex;
    INT     NumChildren;

    // 0x0004273e, the joint part 0x000426eb
    friend FArchive& operator<<(FArchive& Ar, FMeshBone& B)
    {
        Ar << B.Name << B.Flags;
        Ar << B.Orientation << B.Position << B.Length;
        // the xbox reads XSize twice and never YSize
        Ar << B.XSize << B.XSize << B.ZSize;
        return Ar << B.NumChildren << B.ParentIndex;
    }
};

// a triangle of a skeletal mesh's raw data
struct FSkelMeshFace
{
    _WORD Wedges[3];
    BYTE  MaterialIndex;
    BYTE  Unknown7;
    DWORD SmoothingGroups;

    // 0x00041da2
    friend FArchive& operator<<(FArchive& Ar, FSkelMeshFace& F)
    {
        Ar << F.Wedges[0] << F.Wedges[1] << F.Wedges[2] << F.MaterialIndex << F.Unknown7;
        return Ar << F.SmoothingGroups;
    }
};

// how much a bone moves a point
struct FVertInfluence
{
    FLOAT Weight;
    _WORD PointIndex;
    _WORD BoneIndex;

    // 0x00041df6
    friend FArchive& operator<<(FArchive& Ar, FVertInfluence& I)
    {
        return Ar << I.Weight << I.PointIndex << I.BoneIndex;
    }
};

// a point of a lod model and a value stored with it
struct FSkinPoint
{
    FVector Point;
    DWORD   Unknown0C;

    // 0x00042694
    friend FArchive& operator<<(FArchive& Ar, FSkinPoint& P)
    {
        return Ar << P.Point << P.Unknown0C;
    }
};

// a run of a lod model's triangles drawn with one material
struct FSkelMeshSection
{
    FSkelMeshSection() { memset(this, 0, sizeof(*this)); }

    friend FArchive& operator<<(FArchive& Ar, FSkelMeshSection& S);

    _WORD MaterialIndex;
    _WORD Unknown2;
    _WORD Unknown4;
    _WORD Unknown6;
    _WORD Unknown8;
    _WORD UnknownA;
    _WORD UnknownC;
    _WORD UnknownE;
    _WORD Unknown10;
    _WORD Unknown12;
    INT   Unknown14;
    INT   Unknown18;
    // an object with a vtable that serializes itself; null after loading
    void* Unknown1C;
};

class FSkinVertexStream : public FRenderResource
{
public:
    // 0x00047cb2
    FSkinVertexStream() { CacheId = MakeCacheId(RRT_VertexStream); Revision = 0; }

    friend FArchive& operator<<(FArchive& Ar, FSkinVertexStream& S);

    TArray<FUntransformedVertex> Vertices;
};

// one level of detail, ready to draw
struct FStaticLODModel
{
    // 0x000510e4
    FStaticLODModel() : Unknown0(0), Unknown14(0), UnknownC8(0), UnknownCC(0), UnknownD0(0), UnknownD4(0), UnknownD8(0), UnknownDC(0) {}

    friend FArchive& operator<<(FArchive& Ar, FStaticLODModel& M);

    // set once SkinData is in the packed form PackSkinData makes
    INT                        Unknown0;
    TArray<DWORD>              SkinData;
    TArray<FSkinPoint>         SkinPoints;
    INT                        Unknown14;
    TArray<FSkelMeshSection>   Sections;
    TArray<FSkelMeshSection>   Sections2;
    FRawIndexBuffer            IndexBuffer;
    FRawIndexBuffer            IndexBuffer2;
    FSkinVertexStream          VertexStream;
    TArray<_WORD>              Unknown70;
    TLazyArray<FVertInfluence> Influences;
    TLazyArray<FMeshWedge>     Wedges;
    TLazyArray<FMeshFace>      Faces;
    TLazyArray<FVector>        Points;
    INT                        UnknownC8;
    INT                        UnknownCC;
    INT                        UnknownD0;
    INT                        UnknownD4;
    INT                        UnknownD8;
    INT                        UnknownDC;
};

// which bone of an animation drives each bone of the mesh
struct FMeshAnimLinkup
{
    FMeshAnimLinkup() : Unknown0(0), Anim(nullptr) {}

    void Setup(USkeletalMesh* Mesh);

    friend FArchive& operator<<(FArchive& Ar, FMeshAnimLinkup& L);

    INT             Unknown0;
    UMeshAnimation* Anim;
    TArray<INT>     BoneMap;
};

// a named bone with an offset, for attaching things to the mesh
struct FAttachAlias
{
    FName   Alias;
    FName   BoneName;
    FMatrix Offset;

    // 0x000427e0
    friend FArchive& operator<<(FArchive& Ar, FAttachAlias& A)
    {
        return Ar << A.Alias << A.BoneName << A.Offset;
    }
};

// a vertex stream used by rigid parts
class FRigidVertexStream : public FRenderResource
{
public:
    // 0x00038c01
    FRigidVertexStream() : Unknown18(nullptr), Unknown1C(0) { CacheId = MakeCacheId(RRT_VertexStream); Revision = 0; }

    // 0x0003932e
    friend FArchive& operator<<(FArchive& Ar, FRigidVertexStream& S)
    {
        S.FRenderResource::Serialize(Ar);
        Ar << S.Vertices;
        return Ar << S.Revision;
    }

    TArray<FUntransformedVertex> Vertices;
    void*                        Unknown18;
    INT                          Unknown1C;
};

// part of a mesh that one bone moves, drawn on its own
struct FSkelMeshRigidPart
{
    FSkelMeshRigidPart() { for (INT i = 0; i < 9; i++) Unknown0[i] = 0; }

    // 0x00039fa7
    friend FArchive& operator<<(FArchive& Ar, FSkelMeshRigidPart& P)
    {
        for (INT i = 0; i < 9; i++)
            Ar << P.Unknown0[i];
        if (!Ar.IsPersistent())
            Ar << P.VertexStream << P.IndexBuffer;
        return Ar;
    }

    _WORD              Unknown0[9];
    FRigidVertexStream VertexStream;
    FRawIndexBuffer    IndexBuffer;
};

// kept only in memory, never in packages
struct FSkeletalMeshUnknown1F0
{
    FSkeletalMeshUnknown1F0() : Unknown0(0) {}

    // 0x0003ab51
    friend FArchive& operator<<(FArchive& Ar, FSkeletalMeshUnknown1F0& U)
    {
        return Ar << U.Unknown0 << U.IndexBuffer << U.RigidParts;
    }

    INT                        Unknown0;
    FRawIndexBuffer            IndexBuffer;
    TArray<FSkelMeshRigidPart> RigidParts;
};

struct FSkeletalMeshUnknown1D8
{
    FSkeletalMeshUnknown1D8() : Unknown8(0) {}

    // 0x00047ebe
    friend FArchive& operator<<(FArchive& Ar, FSkeletalMeshUnknown1D8& U)
    {
        return Ar << U.Unknown0 << U.Unknown8;
    }

    TArray<_WORD> Unknown0;
    INT           Unknown8;
};

struct FSkeletalMeshUnknown1E0
{
    _WORD Unknown0;
    _WORD Unknown2;

    // 0x00041d82
    friend FArchive& operator<<(FArchive& Ar, FSkeletalMeshUnknown1E0& U)
    {
        return Ar << U.Unknown0 << U.Unknown2;
    }
};

struct FSkeletalMeshUnknown288
{
    _WORD Unknown0[8];

    // 0x00041cec
    friend FArchive& operator<<(FArchive& Ar, FSkeletalMeshUnknown288& U)
    {
        for (INT i = 0; i < 8; i++)
            Ar << U.Unknown0[i];
        return Ar;
    }
};

// ---------------------------------------------------------------------------------------------- mesh animations

// a bone an animation has tracks for
struct FNamedBone
{
    FName Name;
    DWORD Flags;
    INT   ParentIndex;

    // 0x00041d57
    friend FArchive& operator<<(FArchive& Ar, FNamedBone& B)
    {
        return Ar << B.Name << B.Flags << B.ParentIndex;
    }
};

// a rotation in 6 bytes: the three smallest components of the unit quaternion. the low bits of the first two
// words say which component was dropped and the low bit of the third gives its sign.
struct FPackedQuat
{
    FPackedQuat() = default;
    // 0x0004284b
    explicit FPackedQuat(const FQuat& Q);
    // 0x00042d41
    FQuat ToQuat() const;

    friend FArchive& operator<<(FArchive& Ar, FPackedQuat& P)
    {
        return Ar << P.X << P.Y << P.Z;
    }

    _WORD X;
    _WORD Y;
    _WORD Z;
};

// a position in 6 bytes, as a fraction of its track's PosScale
struct FPackedPos
{
    FPackedPos() = default;
    // 0x00041e23
    FPackedPos(const FVector& V, FLOAT InvScale);

    FVector ToVector(FLOAT PosScale) const;

    friend FArchive& operator<<(FArchive& Ar, FPackedPos& P)
    {
        return Ar << P.X << P.Y << P.Z;
    }

    SWORD X;
    SWORD Y;
    SWORD Z;
};

// the keys of one bone
struct AnalogTrack
{
    void Serialize(FArchive& Ar);
    // 0x00042ea2
    void UpdatePosScale(const FVector& Pos);

    friend FArchive& operator<<(FArchive& Ar, AnalogTrack& T)
    {
        T.Serialize(Ar);
        return Ar;
    }

    TArray<FPackedQuat> KeyQuat;
    TArray<FPackedPos>  KeyPos;
    // the largest position component, so packed positions span it
    FLOAT               PosScale;
    // frames from each key to the next
    TArray<BYTE>        KeyTime;
};

struct FMeshAnimNotify
{
    // 0x0007bcc7
    friend FArchive& operator<<(FArchive& Ar, FMeshAnimNotify& N)
    {
        Ar << N.Unknown0 << N.Unknown4;
        if (Ar.Ver() < 112)
            N.Notify = nullptr;
        else
            Ar << N.Notify;
        return Ar;
    }

    INT      Unknown0;
    FName    Unknown4;
    UObject* Notify;
};

struct FMeshAnimSeq
{
    // 0x000392f8
    FMeshAnimSeq()
        : Unknown0C(0), NumFrames(0), Rate(30.f), Unknown18(0), Unknown1C(0.f), Unknown20(0), Unknown2C(0), Unknown30(1.f)
    {
    }

    void Serialize(FArchive& Ar);

    FName                   Name;
    TArray<FName>           Groups;
    INT                     Unknown0C;
    INT                     NumFrames;
    FLOAT                   Rate;
    INT                     Unknown18;
    FLOAT                   Unknown1C;
    BYTE                    Unknown20;
    TArray<FMeshAnimNotify> Notifys;
    INT                     Unknown2C;
    FLOAT                   Unknown30;
};

// a sequence with the bone tracks that play it
class FSkelAnimSeq : public FMeshAnimSeq
{
public:
    void Serialize(FArchive& Ar);

    friend FArchive& operator<<(FArchive& Ar, FSkelAnimSeq& S)
    {
        S.Serialize(Ar);
        return Ar;
    }

    // never serialized
    INT                 Unknown34[7];
    INT                 Unknown50;
    INT                 Unknown54;
    FLOAT               Unknown58;
    TArray<AnalogTrack> Tracks;
};
