#include "EngineClasses.h"
#include "Karma.h"

#define CPP_PROPERTY(Name) static_cast<INT>(reinterpret_cast<BYTE*>(&this->Name) - reinterpret_cast<BYTE*>(static_cast<UObject*>(this)))

// ---------------------------------------------------------------------------------------------- primitives

// 0x000b2743, reached through the jump at 0x000a0a50
void UPrimitive::Serialize(FArchive& Ar)
{
    Super::Serialize(Ar);
    Ar << BoundingBox;
    // the sphere is copied as 16 raw bytes
    Ar << static_cast<FPlane&>(BoundingSphere);
}

FBox UPrimitive::GetCollisionBoundingBox(const AActor* Owner) const
{
    if (!Owner)
        return BoundingBox;
    const FLOAT Radius = Owner->CollisionRadius + 1.f;
    const FLOAT Height = Owner->CollisionHeight + 1.f;
    const FVector& Location = Owner->Location;
    FBox Box;
    Box.Min = FVector(Location.X - Radius, Location.Y - Radius, Location.Z - Height);
    Box.Max = FVector(Location.X + Radius, Location.Y + Radius, Location.Z + Height);
    Box.IsValid = 1;
    return Box;
}

UBOOL UPrimitive::PointCheck(FCheckResult& Result, AActor* Owner, const FVector& Location, const FVector& Extent, DWORD ExtraNodeFlags)
{
    if (!Owner)
        return 1;
    const FLOAT DeltaZ = Owner->Location.Z - Location.Z;
    const FLOAT Height = Owner->CollisionHeight + Extent.Z;
    if (Height * Height <= DeltaZ * DeltaZ)
        return 1;
    const FLOAT DeltaY = Owner->Location.Y - Location.Y;
    const FLOAT DeltaX = Owner->Location.X - Location.X;
    const FLOAT Radius = Owner->CollisionRadius + Extent.X;
    if (Radius * Radius <= DeltaY * DeltaY + DeltaX * DeltaX)
        return 1;

    Result.Actor = Owner;
    Result.Normal = (Location - Owner->Location).SafeNormal();
    // the xbox puts the hit at the bottom of the cylinder whether the point is above or below it
    if (Result.Normal.Z < -0.5f || Result.Normal.Z > 0.5f)
    {
        Result.Location = FVector(Location.X, Location.Y, Owner->Location.Z - Extent.Z);
    }
    else
    {
        const FVector Side = FVector(Result.Normal.X, Result.Normal.Y, Result.Normal.Z * 0.f).SafeNormal();
        Result.Location = FVector(Owner->Location.X - Side.X * Extent.X, Owner->Location.Y - Side.Y * Extent.X,
            Owner->Location.Z - Side.Z * Extent.X + Location.Z);
    }
    return 0;
}

FVector UPrimitive::GetEncroachExtent(AActor* Owner)
{
    return FVector(Owner->CollisionRadius, Owner->CollisionRadius, Owner->CollisionHeight);
}

FVector UPrimitive::GetEncroachCenter(AActor* Owner)
{
    return Owner->Location;
}

UBOOL UStaticMesh::PointCheck(FCheckResult& Result, AActor* Owner, const FVector& Location, const FVector& Extent, DWORD ExtraNodeFlags)
{
    NOT_DECOMPILED(0x001960bf);
    return 1;
}

UBOOL UFluidSurfacePrimitive::PointCheck(FCheckResult& Result, AActor* Owner, const FVector& Location, const FVector& Extent, DWORD ExtraNodeFlags)
{
    NOT_DECOMPILED(0x000a220b);
    return 1;
}

UBOOL UProjectorPrimitive::PointCheck(FCheckResult& Result, AActor* Owner, const FVector& Location, const FVector& Extent, DWORD ExtraNodeFlags)
{
    NOT_DECOMPILED(0x00076d18);
    return 1;
}

UBOOL UTerrainPrimitive::PointCheck(FCheckResult& Result, AActor* Owner, const FVector& Location, const FVector& Extent, DWORD ExtraNodeFlags)
{
    NOT_DECOMPILED(0x00069648);
    return 1;
}

FBox UFluidSurfacePrimitive::GetCollisionBoundingBox(const AActor* Owner) const
{
    NOT_DECOMPILED(0x000a2329);
    return FBox{};
}

FBox UProjectorPrimitive::GetCollisionBoundingBox(const AActor* Owner) const
{
    NOT_DECOMPILED(0x000763dc);
    return FBox{};
}

// ---------------------------------------------------------------------------------------------- old formats

namespace
{
    // the point where three planes meet, or zero when they nearly don't
    FVector IntersectPlanes(const FPlane& P1, const FPlane& P2, const FPlane& P3)
    {
        const FLOAT Det = P1 | (P2 ^ P3);
        if (Det * Det < 0x1.0c6f7cp-20f)
            return FVector(0.f, 0.f, 0.f);
        return ((P2 ^ P3) * P1.W + (P3 ^ P1) * P2.W + (P1 ^ P2) * P3.W) / Det;
    }

    // a node of the collision tree before version 112. leaves hold a triangle.
    struct FStaticMeshLegacyCollision
    {
        // 0x00095e4a
        FStaticMeshLegacyCollision() : Material(nullptr) {}

        // 0x000974c4
        friend FArchive& operator<<(FArchive& Ar, FStaticMeshLegacyCollision& C)
        {
            Ar << C.Children;
            Ar << C.Bounds;
            if (C.Children.Num() == 0)
            {
                Ar << C.Plane;
                FPlane SidePlanes[3];
                if (Ar.Ver() < 83)
                    Ar << SidePlanes[0] << SidePlanes[1] << SidePlanes[2];
                // before version 80 the corners are worked out from the triangle's plane and its edge planes
                if (Ar.Ver() < 80)
                {
                    C.Vertices[0] = IntersectPlanes(C.Plane, SidePlanes[2], SidePlanes[0]);
                    C.Vertices[1] = IntersectPlanes(C.Plane, SidePlanes[0], SidePlanes[1]);
                    C.Vertices[2] = IntersectPlanes(C.Plane, SidePlanes[1], SidePlanes[2]);
                }
                else
                {
                    Ar << C.Vertices[0] << C.Vertices[1] << C.Vertices[2];
                }
                if (Ar.Ver() > 94)
                    Ar << C.Material;
            }
            return Ar;
        }

        TArray<INT> Children;
        FBox        Bounds;
        FPlane      Plane;
        FVector     Vertices[3];
        UObject*    Material;
    };

    // collision triangles before version 133
    struct FStaticMeshLegacyCollisionTriangle
    {
        // 0x00095565
        friend FArchive& operator<<(FArchive& Ar, FStaticMeshLegacyCollisionTriangle& T)
        {
            Ar << T.Planes[0] << T.Planes[1] << T.Planes[2] << T.Planes[3];
            return Ar << AR_INDEX(T.Indices[0]) << AR_INDEX(T.Indices[1]) << AR_INDEX(T.Indices[2]) << AR_INDEX(T.Indices[3]);
        }

        FPlane Planes[4];
        INT    Indices[4];
    };

    // collision nodes before version 133
    struct FStaticMeshLegacyCollisionNode
    {
        // 0x00095e6e
        friend FArchive& operator<<(FArchive& Ar, FStaticMeshLegacyCollisionNode& N)
        {
            Ar << AR_INDEX(N.Indices[0]) << AR_INDEX(N.Indices[1]) << AR_INDEX(N.Indices[2]) << AR_INDEX(N.Indices[3]);
            return Ar << N.Bounds;
        }

        INT  Indices[4];
        FBox Bounds;
    };

}

// ---------------------------------------------------------------------------------------------- mesh data

// 0x00095312
FArchive& operator<<(FArchive& Ar, FStaticMeshSection& S)
{
    Ar << S.IsStrip << S.FirstIndex << S.MinVertexIndex << S.MaxVertexIndex << S.NumTriangles << S.NumPrimitives;
    // calls vtable slot 0x18 of the object with the archive
    if (S.Unknown14)
        NOT_DECOMPILED(0x00095312);
    return Ar;
}

// 0x000990bb
FArchive& operator<<(FArchive& Ar, FStaticMeshVertexStream& S)
{
    S.FRenderResource::Serialize(Ar);
    SerializeStreamData(Ar, S.Vertices);
    return Ar << S.Revision;
}

// 0x00098e34
FArchive& operator<<(FArchive& Ar, FStaticMeshColorStream& S)
{
    S.FRenderResource::Serialize(Ar);
    SerializeStreamData(Ar, S.Colors);
    return Ar << S.Revision;
}

// 0x00098799
FArchive& operator<<(FArchive& Ar, FStaticMeshUVStream& S)
{
    S.FRenderResource::Serialize(Ar);
    SerializeStreamData(Ar, S.UVs);
    return Ar << S.CoordinateIndex << S.Revision;
}

// 0x00098e5d
FArchive& operator<<(FArchive& Ar, FStaticMeshTangentStream& S)
{
    S.FRenderResource::Serialize(Ar);
    Ar << S.Tangents;
    return Ar << S.Revision;
}

// 0x00095ea3
FArchive& operator<<(FArchive& Ar, FStaticMeshTriangle& T)
{
    if (Ar.Ver() < 111)
    {
        // one uv channel and white vertex colours
        FUntransformedVertex V[3];
        Ar << V[0] << V[1] << V[2];
        for (INT i = 0; i < 3; i++)
        {
            T.Vertices[i] = V[i].Position;
            T.UVs[i][0].U = V[i].U;
            T.UVs[i][0].V = V[i].V;
            T.Colors[i].R = T.Colors[i].G = T.Colors[i].B = T.Colors[i].A = 255;
        }
        T.NumUVs = 1;
    }
    else
    {
        Ar << T.Vertices[0] << T.Vertices[1] << T.Vertices[2];
        Ar << T.NumUVs;
        if (Ar.Ver() >= 124)
            Ar << T.Unknown24[0] << T.Unknown24[1] << T.Unknown24[2];
        for (INT i = 0; i < T.NumUVs; i++)
            Ar << T.UVs[0][i] << T.UVs[1][i] << T.UVs[2][i];
        Ar << T.Colors[0] << T.Colors[1] << T.Colors[2];
    }

    if (Ar.Ver() < 112)
    {
        BYTE Skipped[12];
        Ar.Serialize(Skipped, sizeof(Skipped));
        Ar << T.LegacyMaterial << T.LegacyPolyFlags;
    }
    else
    {
        Ar << T.MaterialIndex;
    }
    return Ar << T.SmoothingMask;
}

// ---------------------------------------------------------------------------------------------- static meshes

// 0x001937f7. the mesh's box turned with the actor, grown by the collision model and the actor's karma model.
FBox UStaticMesh::GetCollisionBoundingBox(const AActor* Owner) const
{
    FBox Box;
    if (!Owner->bUseCylinderCollision)
    {
        const FMatrix LocalToWorld = Owner->LocalToWorld();
        Box = FBox();
        for (INT X = 0; X < 2; X++)
            for (INT Y = 0; Y < 2; Y++)
                for (INT Z = 0; Z < 2; Z++)
                {
                    const FVector Corner(X ? BoundingBox.Max.X : BoundingBox.Min.X, Y ? BoundingBox.Max.Y : BoundingBox.Min.Y,
                        Z ? BoundingBox.Max.Z : BoundingBox.Min.Z);
                    Box += LocalToWorld.TransformFVector(Corner);
                }
        if (CollisionModel)
            Box += CollisionModel->GetCollisionBoundingBox(Owner);
    }
    else
    {
        Box = UPrimitive::GetCollisionBoundingBox(Owner);
    }
    if (Owner->bBlockKarma)
    {
        if (McdModel* Model = Owner->getKModel())
        {
            FLOAT KMin[3], KMax[3];
            McdModelGetAABB(Model, KMin, KMax);
            FBox KBox;
            KBox.Min = KME2UPosition(KMin);
            KBox.Max = KME2UPosition(KMax);
            KBox.IsValid = 1;
            Box += KBox;
        }
    }
    return Box;
}

// 0x00095a27
void UStaticMesh::StaticConstructor()
{
    UClass* Class = GetClass();
    new(Class, TEXT("TypeOfMaterial"), RF_Public) UByteProperty(CPP_PROPERTY(TypeOfMaterial), TEXT(""), CPF_Edit);
    new(Class, TEXT("UseSimpleLineCollision"), RF_Public) UBoolProperty(CPP_PROPERTY(UseSimpleLineCollision), TEXT(""), CPF_Edit);
    new(Class, TEXT("UseSimpleBoxCollision"), RF_Public) UBoolProperty(CPP_PROPERTY(UseSimpleBoxCollision), TEXT(""), CPF_Edit);
    new(Class, TEXT("UseSimpleKarmaCollision"), RF_Public) UBoolProperty(CPP_PROPERTY(UseSimpleKarmaCollision), TEXT(""), CPF_Edit);
    new(Class, TEXT("UseVertexColor"), RF_Public) UBoolProperty(CPP_PROPERTY(UseVertexColor), TEXT(""), CPF_Edit);
    new(Class, TEXT("ShaderUsage"), RF_Public) UByteProperty(CPP_PROPERTY(ShaderUsage), TEXT(""), CPF_Transient);
    new(Class, TEXT("UseCollisionForBakedShadowsOnly"), RF_Public) UBoolProperty(CPP_PROPERTY(UseCollisionForBakedShadowsOnly), TEXT(""), CPF_EditConst);
    new(Class, TEXT("LoadVertexColorInGame"), RF_Public) UBoolProperty(CPP_PROPERTY(LoadVertexColorInGame), TEXT(""), CPF_Edit);
    new(Class, TEXT("AlwaysBuildBumpData"), RF_Public) UBoolProperty(CPP_PROPERTY(AlwaysBuildBumpData), TEXT(""), CPF_Edit);

    // the Materials elements, described for script
    UStruct* MaterialStruct = new(Class, TEXT("StaticMeshMaterial")) UStruct(nullptr);
    new(MaterialStruct, TEXT("Material"), RF_Public) UObjectProperty(static_cast<INT>(offsetof(FStaticMeshMaterial, Material)), TEXT(""), CPF_Edit, UMaterial::StaticClass());
    new(MaterialStruct, TEXT("EnableCollision"), RF_Public) UBoolProperty(static_cast<INT>(offsetof(FStaticMeshMaterial, EnableCollision)), TEXT(""), CPF_Edit);
    MaterialStruct->PropertiesSize = sizeof(FStaticMeshMaterial);
    // port: the struct holds a pointer, which needs 8-byte alignment on 64-bit targets
    MaterialStruct->MinAlignment = alignof(FStaticMeshMaterial);
    FArchive ArDummy;
    MaterialStruct->Link(ArDummy, 0);

    UArrayProperty* MaterialsProperty = new(Class, TEXT("Materials"), RF_Public) UArrayProperty(CPP_PROPERTY(Materials), TEXT(""), CPF_Edit | CPF_EditConstArray);
    MaterialsProperty->Inner = new(MaterialsProperty, NAME_None, RF_Public) UStructProperty(0, TEXT(""), CPF_Edit, MaterialStruct);

    UseSimpleLineCollision = 1;
    UseSimpleBoxCollision = 1;
    UseSimpleKarmaCollision = 1;
    TypeOfMaterial = 0;
    UseVertexColor = 0;
    LoadVertexColorInGame = 0;
    AlwaysBuildBumpData = 0;
    ShaderUsage = 0;
}

// 0x00099217
void UStaticMesh::Serialize(FArchive& Ar)
{
    if (Ar.Ver() < 85)
        UObject::Serialize(Ar);
    else
        Super::Serialize(Ar);

    if (Ar.Ver() < 92)
        Ar << Unknown188 << Unknown18C;
    Ar << Sections;
    Ar << BoundingBox;
    if (Ar.Ver() < 74)
    {
        UObject* Skipped = nullptr;
        Ar << Skipped;
    }
    if (Ar.Ver() < 92)
    {
        TArray<FStaticMeshLegacyMaterial> OldMaterials;
        Ar << OldMaterials;
        BYTE Skipped[0x30];
        Ar.Serialize(Skipped, sizeof(Skipped));
    }
    if (Ar.Ver() >= 74)
    {
        if (Ar.Ver() < 112)
        {
            TArray<FStaticMeshLegacyCollision> OldCollision;
            Ar << OldCollision;
        }
        if (Ar.Ver() < 92)
        {
            INT Skipped;
            Ar << Skipped;
        }
    }

    if (Ar.Ver() >= 112)
    {
        Ar << VertexStream;
        if (Ar.Ver() >= 155)
            Ar << AlwaysBuildBumpData;
        if (Ar.Ver() >= 149)
            Ar << LoadVertexColorInGame;
        // vertex colours are read past unless the mesh asks for them
        if (!LoadVertexColorInGame)
        {
            FStaticMeshColorStream Skipped;
            Ar << Skipped;
        }
        else
        {
            Ar << ColorStream;
        }
        Ar << AlphaStream;
        Ar << UVStreams;
        Ar << IndexBuffer;
        Ar << WireframeIndexBuffer;
        Ar << CollisionModel;
        WireframeIndexBuffer.Indices.Empty();

        if (Ar.Ver() < 133)
        {
            TArray<FStaticMeshLegacyCollisionTriangle> OldTriangles;
            TArray<FStaticMeshLegacyCollisionNode> OldNodes;
            Ar << OldTriangles;
            Ar << OldNodes;
        }
        else
        {
            if (Ar.Ver() >= 150)
                Ar << UseCollisionForBakedShadowsOnly;
            UseCollisionForBakedShadowsOnly = 1;
            // from version 148 the collision data is read past
            const bool SkipCollision = Ar.Ver() >= 148;
            Ar.ArSkipObjectRefs = SkipCollision;
            Ar << CollisionTriangles;
            Ar.ArSkipObjectRefs = SkipCollision;
            Ar << CollisionNodes;
            Ar.ArSkipObjectRefs = SkipCollision;
            Ar.Serialize(Unknown138, sizeof(Unknown138));
            Ar.ArSkipObjectRefs = false;
        }
        if (Ar.Ver() < 114)
            Ar << UseSimpleLineCollision << UseSimpleBoxCollision << UseVertexColor;
        if (Ar.LicenseeVer() >= 1)
            Ar << TangentStream;
    }

    if (Ar.Ver() >= 77 && Ar.Ver() < 92)
        Ar << Unknown190;

    if (Ar.Ver() >= 79)
    {
        if (Ar.Ver() < 97)
        {
            // stored as a plain array before version 97
            TArray<FStaticMeshTriangle> OldTriangles;
            OldTriangles = RawTriangles;
            Ar << OldTriangles;
            if (Ar.IsLoading())
            {
                const INT Count = OldTriangles.Num();
                RawTriangles.Empty(Count);
                RawTriangles.AddZeroed(Count);
                for (INT i = 0; i < Count; i++)
                    RawTriangles[i] = OldTriangles[i];
            }
        }
        else
        {
            // the raw triangles always stay in the file until asked for
            const UBOOL SavedLazyLoad = GLazyLoad;
            GLazyLoad = 1;
            RawTriangles.Serialize(Ar);
            GLazyLoad = SavedLazyLoad;
        }
    }

    if (Ar.Ver() < 81)
    {
        InternalVersion = -1;
    }
    else if (Ar.IsLoading())
    {
        Ar << InternalVersion;
    }
    else if (Ar.IsSaving())
    {
        InternalVersion = 9;
        Ar << InternalVersion;
    }
    if (Ar.Ver() >= 100)
        Ar << Unknown194;
    if (Ar.Ver() >= 120)
        Ar << Unknown184;
    if (Ar.Ver() >= 145)
        Ar << TypeOfMaterial;
    if (Ar.Ver() >= 148 && Ar.Ver() < 150)
    {
        Ar << UseCollisionForBakedShadowsOnly;
        UseCollisionForBakedShadowsOnly = 1;
    }
}

// ---------------------------------------------------------------------------------------------- static mesh instance

// 0x000991b5
void UStaticMeshInstance::Serialize(FArchive& Ar)
{
    Super::Serialize(Ar);
    if (Ar.Ver() < 112)
    {
        TArray<FRawColorStream> OldStreams;
        Ar << OldStreams;
    }
    else
    {
        Ar << Unknown30;
    }
    if (Ar.Ver() >= 110)
        Ar << Unknown28;
    if (Ar.Ver() >= 147)
        Ar << Unknown48;
}
