#include "EngineClasses.h"

// 0x0008ee9e
FArchive& operator<<(FArchive& Ar, FKAggregateGeom& G)
{
    if (Ar.Ver() < 113)
    {
        // the sphere array is read twice; the second read replaces the first
        Ar << G.SphereElems << G.BoxElems << G.SphereElems << G.CylinderElems;
    }
    else
    {
        Ar << G.SphereElems << G.BoxElems << G.CylinderElems << G.ConvexElems;
        if (Ar.Ver() > 156)
            Ar << G.Unknown20;
    }
    return Ar;
}

// 0x0008ee27
void UKMeshProps::Serialize(FArchive& Ar)
{
    Super::Serialize(Ar);
    for (INT i = 0; i < 6; i++)
        Ar << Unknown28[i];
    Ar << Unknown40 << AggGeom;
}
