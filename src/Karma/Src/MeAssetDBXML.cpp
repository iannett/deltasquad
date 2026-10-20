#include <cctype>
#include <cstddef>
#include <cstdlib>
#include <cstring>

#include "Core.h"
#include "Karma.h"

// ---------------------------------------------------------------------------------------------- reading values

namespace
{
    // 0x001cea60. moves past spaces and one comma. nonzero when the separator is wrong or nothing follows it.
    INT SkipSeparator(char* Str, char** Next)
    {
        while (isspace(static_cast<unsigned char>(*Str)))
            Str++;
        *Next = Str;
        if (!*Str)
            return 0;
        if (*Str != ',')
            return 1;
        do
            Str++;
        while (isspace(static_cast<unsigned char>(*Str)));
        *Next = Str;
        return *Str == 0;
    }

    // 0x001cf5e0. Unknown10 comma-separated floats at Unknown0C.
    INT ReadReals(MeXMLInput* Input, MeXMLHandler* Handler, void* Data)
    {
        const INT Line = Input->Line;
        const INT Column = Input->Column;
        FLOAT* Out = reinterpret_cast<FLOAT*>(static_cast<BYTE*>(Data) + Handler->Unknown0C);
        char Text[1024];
        INT Result = MeXMLReadText(Input, Text, sizeof(Text));
        if (Result)
            return Result;
        DWORD Count = 0;
        char* Ch = Text;
        if (*Ch)
        {
            while (Count < static_cast<DWORD>(Handler->Unknown10))
            {
                char* End;
                Out[Count++] = static_cast<FLOAT>(strtod(Ch, &End));
                if (End == Ch || SkipSeparator(End, &Ch))
                {
                    snprintf(Input->Error, sizeof(Input->Error), "line %d, char %d: MeReal array data invalid\n", Line, Column);
                    return 2;
                }
                if (!*Ch)
                    break;
            }
        }
        if (Count < static_cast<DWORD>(Handler->Unknown10))
        {
            snprintf(Input->Error, sizeof(Input->Error), "line %d, char %d: expected %d MeReals, found %d\n", Line, Column, Handler->Unknown10, Count);
            return 2;
        }
        return 0;
    }

    // 0x001cf4c0
    INT ReadReal(MeXMLInput* Input, MeXMLHandler* Handler, void* Data)
    {
        FLOAT* Out = reinterpret_cast<FLOAT*>(static_cast<BYTE*>(Data) + Handler->Unknown0C);
        const INT Line = Input->Line;
        const INT Column = Input->Column;
        char Text[1024];
        INT Result = MeXMLReadText(Input, Text, sizeof(Text));
        if (Result)
            return Result;
        char* End;
        *Out = static_cast<FLOAT>(strtod(Text, &End));
        if (End == Text)
        {
            snprintf(Input->Error, sizeof(Input->Error), "line %d, char %d: Expected MeReal, found %s\n", Line, Column, Text);
            return 2;
        }
        while (isspace(static_cast<unsigned char>(*End)))
            End++;
        if (*End)
        {
            snprintf(Input->Error, sizeof(Input->Error), "line %d, char %d: found garbage after data\n", Line, Column);
            return 2;
        }
        return 0;
    }

    // 0x001cf3a0
    INT ReadInt(MeXMLInput* Input, MeXMLHandler* Handler, void* Data)
    {
        INT* Out = reinterpret_cast<INT*>(static_cast<BYTE*>(Data) + Handler->Unknown0C);
        const INT Line = Input->Line;
        const INT Column = Input->Column;
        char Text[1024];
        INT Result = MeXMLReadText(Input, Text, sizeof(Text));
        if (Result)
            return Result;
        char* End;
        *Out = static_cast<INT>(strtol(Text, &End, 0));
        if (End == Text)
        {
            snprintf(Input->Error, sizeof(Input->Error), "line %d, char %d: Expected int, found %s\n", Line, Column, Text);
            return 2;
        }
        while (isspace(static_cast<unsigned char>(*End)))
            End++;
        if (*End)
        {
            snprintf(Input->Error, sizeof(Input->Error), "line %d, char %d: found garbage after data\n", Line, Column);
            return 2;
        }
        return 0;
    }

    MeXMLHandler ElementHandler(const char* Tag, MeXMLElementFn Fn)
    {
        MeXMLHandler Handler = {};
        Handler.Tag = Tag;
        Handler.Handler = reinterpret_cast<void*>(Fn);
        return Handler;
    }

    MeXMLHandler ValueHandler(const char* Tag, MeXMLDataFn Fn, size_t Offset, INT Count = 0, MeXMLPostFn Post = nullptr)
    {
        MeXMLHandler Handler = {};
        Handler.Tag = Tag;
        Handler.Type = 1;
        Handler.Handler = reinterpret_cast<void*>(Fn);
        Handler.Unknown0C = static_cast<INT>(Offset);
        Handler.Unknown10 = Count;
        Handler.Post = Post;
        return Handler;
    }

    MeXMLHandler EndHandler()
    {
        MeXMLHandler Handler = {};
        Handler.Type = 2;
        return Handler;
    }

    // ------------------------------------------------------------------------------------------ elements

    // 0x001cccf0. adds the vertex just read to the list, doubling it when full.
    INT AddPrimitiveVertex(MeXMLElement* Element, void* Data, void* Context)
    {
        MeXMLPrimitive* Primitive = static_cast<MeXMLPrimitive*>(Data);
        FLOAT* Vertex = Primitive->Vertices + Primitive->VertexCount * 3;
        Vertex[0] = Primitive->Vertex[0];
        Vertex[1] = Primitive->Vertex[1];
        Vertex[2] = Primitive->Vertex[2];
        if (++Primitive->VertexCount >= Primitive->VertexCapacity)
        {
            Primitive->VertexCapacity *= 2;
            Primitive->Vertices = static_cast<FLOAT*>(MeMemory.Resize(Primitive->Vertices, Primitive->VertexCapacity * 3 * static_cast<INT>(sizeof(FLOAT))));
        }
        return 0;
    }

    // 0x001ccd50
    void DestroyPrimitive(void* Data)
    {
        MeMemory.Destroy(static_cast<MeXMLPrimitive*>(Data)->Vertices);
        MeMemory.Destroy(Data);
    }

    // 0x001ccd70
    INT ReadPrimitive(MeXMLElement* Element, void* Context)
    {
        MeXMLHandler Handlers[] = {
            ValueHandler("RADIUS", ReadReal, offsetof(MeXMLPrimitive, Dims)),
            ValueHandler("HEIGHT", ReadReal, offsetof(MeXMLPrimitive, Dims) + sizeof(FLOAT)),
            ValueHandler("DIMS", ReadReals, offsetof(MeXMLPrimitive, Dims), 3),
            ValueHandler("VERTEX", ReadReals, offsetof(MeXMLPrimitive, Vertex), 3, AddPrimitiveVertex),
            ValueHandler("TM", ReadReals, offsetof(MeXMLPrimitive, TM), 16),
            EndHandler(),
        };
        MeXMLPrimitive* Primitive = static_cast<MeXMLPrimitive*>(MeMemory.CreateZeroed(sizeof(MeXMLPrimitive)));
        Primitive->VertexCount = 0;
        Primitive->VertexCapacity = 1;
        Primitive->Vertices = static_cast<FLOAT*>(MeMemory.Create(3 * sizeof(FLOAT)));
        MeXMLNode* Node = MeXMLNodeCreate(MEXML_Primitive, Element, Primitive, DestroyPrimitive, Element->Attributes);
        MeXMLNodeAddChild(Node, static_cast<MeXMLNode*>(Context));
        return MeXMLParseElements(Element, Handlers, Primitive, Node);
    }

    // 0x001ccf00
    INT ReadGeometry(MeXMLElement* Element, void* Context)
    {
        MeXMLHandler Handlers[] = { ElementHandler("PRIMITIVE", ReadPrimitive), EndHandler() };
        MeXMLNode* Node = MeXMLNodeCreate(MEXML_Geometry, Element, nullptr, nullptr, Element->Attributes);
        MeXMLNodeAddChild(Node, static_cast<MeXMLNode*>(Context));
        return MeXMLParseElements(Element, Handlers, nullptr, Node);
    }

    // 0x001ccf90
    INT ReadDynamics(MeXMLElement* Element, void* Context)
    {
        MeXMLHandler Handlers[] = {
            ValueHandler("MASS_OFFSET", ReadReals, offsetof(MeXMLDynamics, MassOffset), 3),
            ValueHandler("MASS", ReadReal, offsetof(MeXMLDynamics, Mass)),
            ValueHandler("DENSITY", ReadReal, offsetof(MeXMLDynamics, Density)),
            ValueHandler("INERTIA", ReadReals, offsetof(MeXMLDynamics, Inertia), 6),
            ValueHandler("LIN_DAMP", ReadReal, offsetof(MeXMLDynamics, LinearDamping)),
            ValueHandler("ANG_DAMP", ReadReal, offsetof(MeXMLDynamics, AngularDamping)),
            ValueHandler("FAST_SPIN", ReadReals, offsetof(MeXMLDynamics, FastSpin), 3),
            ValueHandler("USE_FAST_SPIN", ReadInt, offsetof(MeXMLDynamics, UseFastSpin)),
            EndHandler(),
        };
        MeXMLDynamics* Dynamics = static_cast<MeXMLDynamics*>(MeMemory.CreateZeroed(sizeof(MeXMLDynamics)));
        MeXMLNode* Node = MeXMLNodeCreate(MEXML_Dynamics, Element, Dynamics, MeMemory.Destroy, Element->Attributes);
        MeXMLNodeAddChild(Node, static_cast<MeXMLNode*>(Context));
        Dynamics->Density = 1.f;
        Dynamics->Mass = 1.f;
        Dynamics->FastSpin[1] = 1.f;
        Dynamics->Inertia[0] = 0.4f;
        Dynamics->Inertia[3] = 0.4f;
        Dynamics->Inertia[5] = 0.4f;
        Dynamics->AngularDamping = 0.f;
        Dynamics->LinearDamping = 0.f;
        Dynamics->FastSpin[0] = 0.f;
        Dynamics->FastSpin[2] = 0.f;
        Dynamics->UseFastSpin = 0;
        return MeXMLParseElements(Element, Handlers, Dynamics, Node);
    }

    // 0x001cd200
    INT ReadModel(MeXMLElement* Element, void* Context)
    {
        MeXMLHandler Handlers[] = { ElementHandler("DYNAMICS", ReadDynamics), EndHandler() };
        MeXMLNode* Node = MeXMLNodeCreate(MEXML_Model, Element, nullptr, nullptr, Element->Attributes);
        MeXMLNodeAddChild(Node, static_cast<MeXMLNode*>(Context));
        return MeXMLParseElements(Element, Handlers, nullptr, Node);
    }

    // 0x001cd290
    INT ReadJoint(MeXMLElement* Element, void* Context)
    {
        MeXMLHandler Handlers[] = {
            ValueHandler("POS1", ReadReals, offsetof(MeXMLJoint, Pos1), 3),
            ValueHandler("POS2", ReadReals, offsetof(MeXMLJoint, Pos2), 3),
            ValueHandler("PRIMARY_AXIS1", ReadReals, offsetof(MeXMLJoint, PrimaryAxis1), 3),
            ValueHandler("PRIMARY_AXIS2", ReadReals, offsetof(MeXMLJoint, PrimaryAxis2), 3),
            ValueHandler("ORTHOGONAL_AXIS1", ReadReals, offsetof(MeXMLJoint, OrthogonalAxis1), 3),
            ValueHandler("ORTHOGONAL_AXIS2", ReadReals, offsetof(MeXMLJoint, OrthogonalAxis2), 3),
            ValueHandler("HINGE_MAX_FORCE", ReadReal, offsetof(MeXMLJoint, Unknown8C)),
            ValueHandler("HINGE_DES_VEL", ReadReal, offsetof(MeXMLJoint, UnknownA4)),
            ValueHandler("STEERING_MAX_FORCE", ReadReal, offsetof(MeXMLJoint, Unknown90)),
            ValueHandler("STEERING_DES_VEL", ReadReal, offsetof(MeXMLJoint, UnknownA8)),
            ValueHandler("STEERING_LOCK", ReadInt, offsetof(MeXMLJoint, UnknownB8)),
            ValueHandler("SUSP_HIGH_LIMIT", ReadReal, offsetof(MeXMLJoint, Unknown48)),
            ValueHandler("SUSP_LOW_LIMIT", ReadReal, offsetof(MeXMLJoint, Unknown4C)),
            ValueHandler("SUSP_STIFFNESS", ReadReal, offsetof(MeXMLJoint, Unknown54)),
            ValueHandler("SUSP_SOFTNESS", ReadReal, offsetof(MeXMLJoint, Unknown58)),
            ValueHandler("SUSP_DAMPING", ReadReal, offsetof(MeXMLJoint, Unknown6C)),
            ValueHandler("SUSP_REF", ReadReal, offsetof(MeXMLJoint, UnknownAC)),
            ValueHandler("HIGH_LIMIT", ReadReal, offsetof(MeXMLJoint, Unknown48)),
            ValueHandler("LOW_LIMIT", ReadReal, offsetof(MeXMLJoint, Unknown4C)),
            ValueHandler("HIGH_STIFFNESS", ReadReal, offsetof(MeXMLJoint, Unknown54)),
            ValueHandler("LOW_STIFFNESS", ReadReal, offsetof(MeXMLJoint, Unknown58)),
            ValueHandler("LIMITED", ReadInt, offsetof(MeXMLJoint, Limited)),
            ValueHandler("MOTORIZED", ReadInt, offsetof(MeXMLJoint, Motorized)),
            ValueHandler("DES_VEL", ReadReal, offsetof(MeXMLJoint, UnknownA4)),
            ValueHandler("MAX_FORCE", ReadReal, offsetof(MeXMLJoint, Unknown8C)),
            ValueHandler("L0", ReadReal, offsetof(MeXMLJoint, Unknown8C)),
            ValueHandler("L1", ReadReal, offsetof(MeXMLJoint, Unknown90)),
            ValueHandler("L2", ReadReal, offsetof(MeXMLJoint, Unknown94)),
            ValueHandler("A0", ReadReal, offsetof(MeXMLJoint, Unknown98)),
            ValueHandler("A1", ReadReal, offsetof(MeXMLJoint, Unknown9C)),
            ValueHandler("A2", ReadReal, offsetof(MeXMLJoint, UnknownA0)),
            ValueHandler("HALF_ANGLE", ReadReal, offsetof(MeXMLJoint, Unknown48)),
            ValueHandler("STIFFNESS", ReadReal, offsetof(MeXMLJoint, Unknown54)),
            ValueHandler("CONE_TYPE", ReadInt, offsetof(MeXMLJoint, ConeType)),
            ValueHandler("CONE_HALF_ANGLE_X", ReadReal, offsetof(MeXMLJoint, Unknown48)),
            ValueHandler("CONE_HALF_ANGLE_Y", ReadReal, offsetof(MeXMLJoint, Unknown4C)),
            ValueHandler("CONE_STIFFNESS", ReadReal, offsetof(MeXMLJoint, Unknown54)),
            ValueHandler("CONE_DAMPING", ReadReal, offsetof(MeXMLJoint, Unknown6C)),
            ValueHandler("TWIST_TYPE", ReadInt, offsetof(MeXMLJoint, TwistType)),
            ValueHandler("TWIST_HALF_ANGLE", ReadReal, offsetof(MeXMLJoint, Unknown50)),
            ValueHandler("TWIST_STIFFNESS", ReadReal, offsetof(MeXMLJoint, Unknown58)),
            ValueHandler("TWIST_DAMPING", ReadReal, offsetof(MeXMLJoint, Unknown70)),
            ValueHandler("ROTATION_ENABLED", ReadInt, offsetof(MeXMLJoint, UnknownB8)),
            ValueHandler("STIFFNESS", ReadReal, offsetof(MeXMLJoint, Unknown54)),
            ValueHandler("DAMPING", ReadReal, offsetof(MeXMLJoint, Unknown6C)),
            ValueHandler("LINEAR_STIFF_X", ReadReal, offsetof(MeXMLJoint, Unknown54)),
            ValueHandler("LINEAR_STIFF_Y", ReadReal, offsetof(MeXMLJoint, Unknown58)),
            ValueHandler("LINEAR_STIFF_Z", ReadReal, offsetof(MeXMLJoint, Unknown5C)),
            ValueHandler("LINEAR_DAMP_X", ReadReal, offsetof(MeXMLJoint, Unknown6C)),
            ValueHandler("LINEAR_DAMP_Y", ReadReal, offsetof(MeXMLJoint, Unknown70)),
            ValueHandler("LINEAR_DAMP_Z", ReadReal, offsetof(MeXMLJoint, Unknown74)),
            ValueHandler("ANGULAR_STIFF_X", ReadReal, offsetof(MeXMLJoint, Unknown60)),
            ValueHandler("ANGULAR_STIFF_Y", ReadReal, offsetof(MeXMLJoint, Unknown60) + sizeof(FLOAT)),
            ValueHandler("ANGULAR_STIFF_Z", ReadReal, offsetof(MeXMLJoint, Unknown60) + 2 * sizeof(FLOAT)),
            ValueHandler("ANGULAR_DAMP_X", ReadReal, offsetof(MeXMLJoint, Unknown78)),
            ValueHandler("ANGULAR_DAMP_Y", ReadReal, offsetof(MeXMLJoint, Unknown78) + sizeof(FLOAT)),
            ValueHandler("ANGULAR_DAMP_Z", ReadReal, offsetof(MeXMLJoint, Unknown78) + 2 * sizeof(FLOAT)),
            EndHandler(),
        };
        MeXMLJoint* Joint = static_cast<MeXMLJoint*>(MeMemory.CreateZeroed(sizeof(MeXMLJoint)));
        MeXMLNode* Node = MeXMLNodeCreate(MEXML_Joint, Element, Joint, MeMemory.Destroy, Element->Attributes);
        MeXMLNodeAddChild(Node, static_cast<MeXMLNode*>(Context));
        return MeXMLParseElements(Element, Handlers, Joint, Node);
    }

    // 0x001ce0f0
    INT ReadPart(MeXMLElement* Element, void* Context)
    {
        MeXMLHandler Handlers[] = { ValueHandler("TM", ReadReals, offsetof(MeXMLPart, TM), 16), EndHandler() };
        MeXMLPart* Part = static_cast<MeXMLPart*>(MeMemory.CreateZeroed(sizeof(MeXMLPart)));
        MeXMLNode* Node = MeXMLNodeCreate(MEXML_Part, Element, Part, MeMemory.Destroy, Element->Attributes);
        MeXMLNodeAddChild(Node, static_cast<MeXMLNode*>(Context));
        return MeXMLParseElements(Element, Handlers, Part, Node);
    }

    // 0x001ce190. its contents are only attributes.
    INT ReadNoCollision(MeXMLElement* Element, void* Context)
    {
        MeXMLHandler Handlers[] = { EndHandler() };
        MeXMLNode* Node = MeXMLNodeCreate(MEXML_NoCollision, Element, nullptr, nullptr, Element->Attributes);
        MeXMLNodeAddChild(Node, static_cast<MeXMLNode*>(Context));
        return MeXMLParseElements(Element, Handlers, nullptr, Node);
    }

    // 0x001ce1f0
    INT ReadAsset(MeXMLElement* Element, void* Context)
    {
        MeXMLHandler Handlers[] = {
            ElementHandler("GEOMETRY", ReadGeometry),
            ElementHandler("MODEL", ReadModel),
            ElementHandler("JOINT", ReadJoint),
            ElementHandler("PART", ReadPart),
            ElementHandler("NO_COLLISION", ReadNoCollision),
            EndHandler(),
        };
        MeXMLInputGetUserData(MeXMLElementGetInput(Element));
        MeXMLNode* Node = MeXMLNodeCreate(MEXML_Asset, Element, nullptr, nullptr, Element->Attributes);
        MeXMLNodeAddChild(Node, static_cast<MeXMLNode*>(Context));
        return MeXMLParseElements(Element, Handlers, nullptr, Node);
    }
}

// 0x001ce360
INT MeAssetDBXMLReadKarma(MeXMLElement* Element, void* Context)
{
    MeXMLHandler Handlers[] = { ElementHandler("ASSET", ReadAsset), EndHandler() };
    return MeXMLParseElements(Element, Handlers, nullptr, Context);
}

namespace
{
    // reads up to three comma-separated floats; the rest stay as they were
    void ReadOffset(const char* Text, FLOAT* Offset)
    {
        char* Ch = const_cast<char*>(Text);
        for (INT i = 0; *Ch && i < 3; i++)
        {
            char* End;
            Offset[i] = static_cast<FLOAT>(strtod(Ch, &End));
            if (Ch != End)
                SkipSeparator(End, &Ch);
        }
    }

    // 0x001cc400
    MeFPrimitive* CreatePrimitive(MeFGeometry* Geometry, MeXMLNode* Node)
    {
        const MeXMLPrimitive* Data = static_cast<MeXMLPrimitive*>(Node->Data);
        const char* Id = MeXMLNodeGetAttribute(Node, "id");
        const char* Type = MeXMLNodeGetAttribute(Node, "type");
        if (!Type)
            return nullptr;
        MeFPrimitive* Primitive;
        if (!strcmp(Type, "sphere"))
        {
            Primitive = MeFPrimitiveCreate(Id, 1);
            MeFPrimitiveSetRadius(Primitive, Data->Dims[0]);
        }
        else if (!strcmp(Type, "box"))
        {
            Primitive = MeFPrimitiveCreate(Id, 2);
            MeFPrimitiveSetDims(Primitive, Data->Dims[0], Data->Dims[1], Data->Dims[2]);
        }
        else if (!strcmp(Type, "cylinder") || !strcmp(Type, "sphyl"))
        {
            Primitive = MeFPrimitiveCreate(Id, !strcmp(Type, "cylinder") ? 3 : 4);
            MeFPrimitiveSetRadius(Primitive, Data->Dims[0]);
            MeFPrimitiveSetHeight(Primitive, Data->Dims[1]);
        }
        else if (!strcmp(Type, "plane"))
        {
            Primitive = MeFPrimitiveCreate(Id, 5);
        }
        else if (!strcmp(Type, "convex"))
        {
            Primitive = MeFPrimitiveCreate(Id, 6);
            for (INT i = 0; i < Data->VertexCount; i++)
                MeFPrimitiveAddVertex(Primitive, Data->Vertices + i * 3);
        }
        else
        {
            return nullptr;
        }
        if (Primitive)
            MeFPrimitiveSetTM(Primitive, Data->TM);
        return Primitive;
    }

    // 0x001ce3d0
    MeFGeometry* CreateGeometry(MeFAsset* Asset, MeXMLNode* Node)
    {
        MeFGeometry* Geometry = MeFGeometryCreate(MeXMLNodeGetAttribute(Node, "id"));
        for (MeXMLList* Entry = Node->Children; Entry; Entry = Entry->Next)
        {
            MeXMLNode* Child = static_cast<MeXMLNode*>(Entry->Item);
            if (Child->Type == MEXML_Primitive)
            {
                if (MeFPrimitive* Primitive = CreatePrimitive(Geometry, Child))
                    MeFGeometryInsertPrimitive(Geometry, Primitive);
            }
        }
        if (const char* Graphic = MeXMLNodeGetAttribute(Node, "graphic"))
            MeFGeometrySetGraphic(Geometry, Graphic);
        if (const char* Scale = MeXMLNodeGetAttribute(Node, "scale"))
            MeFGeometrySetScale(Geometry, static_cast<FLOAT>(strtod(Scale, nullptr)));
        if (const char* Text = MeXMLNodeGetAttribute(Node, "graphic_offset"))
        {
            FLOAT Offset[3] = {};
            ReadOffset(Text, Offset);
            MeFGeometrySetGraphicOffset(Geometry, Offset[0], Offset[1], Offset[2]);
        }
        return Geometry;
    }

    // 0x001cc570. the type comes from the type attribute, or from whether there is geometry and dynamics.
    MeFModel* CreateModel(MeFAsset* Asset, MeXMLNode* Node)
    {
        INT Type = 1;
        MeFGeometry* Geometry = nullptr;
        const char* Id = MeXMLNodeGetAttribute(Node, "id");
        if (const char* GeometryName = MeXMLNodeGetAttribute(Node, "geometry"))
            Geometry = MeFAssetLookupGeometry(Asset, GeometryName);
        MeXMLList* Children = Node->Children;
        const char* TypeName = MeXMLNodeGetAttribute(Node, "type");
        if (!TypeName)
        {
            if (!Geometry)
            {
                if (!Children)
                    return nullptr;
                Type = 1;
            }
            else
            {
                Type = Children ? 0 : 2;
            }
        }
        else if (!strcmp(TypeName, "dynamics_only"))
        {
            Type = 1;
        }
        else if (!strcmp(TypeName, "geometry_only"))
        {
            Type = 2;
        }
        else if (!strcmp(TypeName, "dynamics_and_geometry"))
        {
            Type = 0;
        }
        MeFModel* Model = MeFModelCreate(Id, Type);
        if (Children)
        {
            // the first child is taken to be the DYNAMICS
            const MeXMLDynamics* Dynamics = static_cast<MeXMLDynamics*>(static_cast<MeXMLNode*>(Children->Item)->Data);
            MeFModelSetMassOffset(Model, Dynamics->MassOffset);
            MeFModelSetMass(Model, Dynamics->Mass);
            MeFModelSetDensity(Model, Dynamics->Density);
            const FLOAT* I = Dynamics->Inertia;
            const FLOAT Inertia[9] = { I[0], I[1], I[2], I[1], I[3], I[4], I[2], I[4], I[5] };
            MeFModelSetInertiaTensor(Model, Inertia);
            MeFModelSetLinearDamping(Model, Dynamics->LinearDamping);
            MeFModelSetAngularDamping(Model, Dynamics->AngularDamping);
            MeFModelSetFastSpinAxis(Model, Dynamics->FastSpin[0], Dynamics->FastSpin[1], Dynamics->FastSpin[2]);
            MeFModelSetUseFastSpinAxis(Model, Dynamics->UseFastSpin);
        }
        if (Geometry)
            MeFModelSetGeometry(Model, Geometry);
        return Model;
    }

    // 0x001cc700
    MeFPart* CreatePart(MeFAsset* Asset, MeXMLNode* Node)
    {
        const MeXMLPart* Data = static_cast<MeXMLPart*>(Node->Data);
        const char* Id = MeXMLNodeGetAttribute(Node, "id");
        MeFModel* Model = MeFAssetLookupModel(Asset, MeXMLNodeGetAttribute(Node, "model"));
        if (!Model)
            return nullptr;
        MeFPart* Part = MeFPartCreate(Id, Model, Data->TM);
        if (const char* Graphic = MeXMLNodeGetAttribute(Node, "graphic"))
            MeFPartSetGraphic(Part, Graphic);
        if (const char* Scale = MeXMLNodeGetAttribute(Node, "scale"))
            MeFPartSetScale(Part, static_cast<FLOAT>(strtod(Scale, nullptr)));
        if (const char* Parent = MeXMLNodeGetAttribute(Node, "parent"))
            MeFPartSetParent(Part, Parent);
        if (const char* Text = MeXMLNodeGetAttribute(Node, "graphic_offset"))
        {
            FLOAT Offset[3] = {};
            ReadOffset(Text, Offset);
            MeFPartSetGraphicOffset(Part, Offset[0], Offset[1], Offset[2]);
        }
        return Part;
    }

    // 0x001cc810
    MeFJoint* CreateJoint(MeFAsset* Asset, MeXMLNode* Node)
    {
        const MeXMLJoint* D = static_cast<MeXMLJoint*>(Node->Data);
        const char* Id = MeXMLNodeGetAttribute(Node, "id");
        const char* Type = MeXMLNodeGetAttribute(Node, "type");
        if (!Type)
            return nullptr;
        const char* Part1 = MeXMLNodeGetAttribute(Node, "part1");
        if (!Part1)
            return nullptr;
        MeFJoint* Joint;
        if (!strcmp(Type, "ballandsocket"))
        {
            Joint = MeFJointCreate(Id, 3);
        }
        else if (!strcmp(Type, "hinge") || !strcmp(Type, "prismatic"))
        {
            Joint = MeFJointCreate(Id, !strcmp(Type, "hinge") ? 2 : 7);
            MeFJointSetReal(Joint, 0, D->Unknown48);
            MeFJointSetReal(Joint, 1, D->Unknown4C);
            MeFJointSetReal(Joint, 3, D->Unknown54);
            MeFJointSetReal(Joint, 4, D->Unknown58);
            MeFJointSetInt(Joint, 15, D->Limited);
            MeFJointSetInt(Joint, 16, D->Motorized);
            MeFJointSetReal(Joint, 23, D->UnknownA4);
            MeFJointSetReal(Joint, 17, D->Unknown8C);
        }
        else if (!strcmp(Type, "carwheel"))
        {
            Joint = MeFJointCreate(Id, 1);
            MeFJointSetReal(Joint, 23, D->UnknownA4);
            MeFJointSetReal(Joint, 17, D->Unknown8C);
            MeFJointSetReal(Joint, 24, D->UnknownA8);
            MeFJointSetReal(Joint, 18, D->Unknown90);
            MeFJointSetInt(Joint, 28, D->UnknownB8);
            MeFJointSetReal(Joint, 9, D->Unknown6C);
            MeFJointSetReal(Joint, 0, D->Unknown48);
            MeFJointSetReal(Joint, 1, D->Unknown4C);
            MeFJointSetReal(Joint, 25, D->UnknownAC);
            MeFJointSetReal(Joint, 3, D->Unknown54);
            MeFJointSetReal(Joint, 4, D->Unknown58);
        }
        else if (!strcmp(Type, "conelimit"))
        {
            Joint = MeFJointCreate(Id, 4);
            MeFJointSetReal(Joint, 0, D->Unknown48);
            MeFJointSetReal(Joint, 3, D->Unknown54);
        }
        else if (!strcmp(Type, "universal"))
        {
            Joint = MeFJointCreate(Id, 5);
        }
        else if (!strcmp(Type, "rpro"))
        {
            Joint = MeFJointCreate(Id, 6);
            MeFJointSetReal(Joint, 17, D->Unknown8C);
            MeFJointSetReal(Joint, 18, D->Unknown90);
            MeFJointSetReal(Joint, 19, D->Unknown94);
            MeFJointSetReal(Joint, 20, D->Unknown98);
            MeFJointSetReal(Joint, 21, D->Unknown9C);
            MeFJointSetReal(Joint, 22, D->UnknownA0);
        }
        else if (!strcmp(Type, "skeletal"))
        {
            Joint = MeFJointCreate(Id, 8);
            MeFJointSetEnum(Joint, 26, D->ConeType);
            MeFJointSetReal(Joint, 0, D->Unknown48);
            MeFJointSetReal(Joint, 1, D->Unknown4C);
            MeFJointSetReal(Joint, 3, D->Unknown54);
            MeFJointSetReal(Joint, 9, D->Unknown6C);
            MeFJointSetEnum(Joint, 27, D->TwistType);
            MeFJointSetReal(Joint, 2, D->Unknown50);
            MeFJointSetReal(Joint, 4, D->Unknown58);
            MeFJointSetReal(Joint, 10, D->Unknown70);
            MeFJointSetInt(Joint, 28, D->UnknownB8);
        }
        else if (!strcmp(Type, "angular3"))
        {
            Joint = MeFJointCreate(Id, 9);
            MeFJointSetInt(Joint, 28, D->UnknownB8);
            MeFJointSetReal(Joint, 3, D->Unknown54);
            MeFJointSetReal(Joint, 9, D->Unknown6C);
        }
        else if (!strcmp(Type, "spring6"))
        {
            Joint = MeFJointCreate(Id, 10);
            MeFJointSetReal(Joint, 3, D->Unknown54);
            MeFJointSetReal(Joint, 4, D->Unknown58);
            MeFJointSetReal(Joint, 5, D->Unknown5C);
            MeFJointSetReal(Joint, 6, D->Unknown60[0]);
            MeFJointSetReal(Joint, 7, D->Unknown60[1]);
            MeFJointSetReal(Joint, 8, D->Unknown60[2]);
            MeFJointSetReal(Joint, 9, D->Unknown6C);
            MeFJointSetReal(Joint, 10, D->Unknown70);
            MeFJointSetReal(Joint, 11, D->Unknown74);
            MeFJointSetReal(Joint, 12, D->Unknown78[0]);
            MeFJointSetReal(Joint, 13, D->Unknown78[1]);
            MeFJointSetReal(Joint, 14, D->Unknown78[2]);
        }
        else
        {
            return nullptr;
        }
        if (Joint)
        {
            const FLOAT* Pos[2] = { D->Pos1, D->Pos2 };
            const FLOAT* Primary[2] = { D->PrimaryAxis1, D->PrimaryAxis2 };
            const FLOAT* Orthogonal[2] = { D->OrthogonalAxis1, D->OrthogonalAxis2 };
            for (INT i = 0; i < 2; i++)
            {
                MeFJointSetPosition(Joint, i, Pos[i][0], Pos[i][1], Pos[i][2]);
                MeFJointSetPrimaryAxis(Joint, i, Primary[i][0], Primary[i][1], Primary[i][2]);
                MeFJointSetOrthogonalAxis(Joint, i, Orthogonal[i][0], Orthogonal[i][1], Orthogonal[i][2]);
            }
            const char* Part2 = MeXMLNodeGetAttribute(Node, "part2");
            MeFJointSetPart(Joint, Part1, 0);
            if (Part2)
                MeFJointSetPart(Joint, Part2, 1);
        }
        return Joint;
    }

    // 0x001ce4d0. geometry first, so models can find it, then models, parts, joints and collision pairs.
    MeFAsset* CreateAssetFromNode(MeAssetDB* DB, void* PartIDs, MeXMLNode* Node)
    {
        const char* Id = MeXMLNodeGetAttribute(Node, "id");
        if (!Id)
            return nullptr;
        MeFAsset* Asset = MeFAssetCreate(Id, PartIDs ? MeIDPoolRequestID(static_cast<MeIDPool*>(PartIDs)) : 0);
        for (MeXMLList* Entry = Node->Children; Entry; Entry = Entry->Next)
        {
            MeXMLNode* Child = static_cast<MeXMLNode*>(Entry->Item);
            if (Child->Type == MEXML_Geometry)
            {
                if (MeFGeometry* Geometry = CreateGeometry(Asset, Child))
                    MeFAssetInsertGeometry(Asset, Geometry);
            }
        }
        for (MeXMLList* Entry = Node->Children; Entry; Entry = Entry->Next)
        {
            MeXMLNode* Child = static_cast<MeXMLNode*>(Entry->Item);
            if (Child->Type == MEXML_Model)
            {
                if (MeFModel* Model = CreateModel(Asset, Child))
                    MeFAssetInsertModel(Asset, Model);
            }
        }
        for (MeXMLList* Entry = Node->Children; Entry; Entry = Entry->Next)
        {
            MeXMLNode* Child = static_cast<MeXMLNode*>(Entry->Item);
            if (Child->Type == MEXML_Part)
            {
                if (MeFPart* Part = CreatePart(Asset, Child))
                    MeFAssetInsertPart(Asset, Part);
            }
        }
        for (MeXMLList* Entry = Node->Children; Entry; Entry = Entry->Next)
        {
            MeXMLNode* Child = static_cast<MeXMLNode*>(Entry->Item);
            if (Child->Type == MEXML_Joint)
            {
                if (MeFJoint* Joint = CreateJoint(Asset, Child))
                    MeFAssetInsertJoint(Asset, Joint);
            }
        }
        for (MeXMLList* Entry = Node->Children; Entry; Entry = Entry->Next)
        {
            MeXMLNode* Child = static_cast<MeXMLNode*>(Entry->Item);
            if (Child->Type == MEXML_NoCollision)
            {
                MeFPart* Part1 = MeFAssetLookupPart(Asset, MeXMLNodeGetAttribute(Child, "part1"));
                MeFPart* Part2 = MeFAssetLookupPart(Asset, MeXMLNodeGetAttribute(Child, "part2"));
                if (Part1 && Part2)
                    MeFPartSetCollision(Part1, Part2, 0);
            }
        }
        if (const char* Graphic = MeXMLNodeGetAttribute(Node, "graphic"))
            MeFAssetSetGraphic(Asset, Graphic);
        if (const char* Scale = MeXMLNodeGetAttribute(Node, "scale"))
            MeFAssetSetScale(Asset, static_cast<FLOAT>(strtod(Scale, nullptr)));
        if (const char* RefPart = MeXMLNodeGetAttribute(Node, "ref_part"))
            MeFAssetSetRefPart(Asset, RefPart);
        if (const char* MassScale = MeXMLNodeGetAttribute(Node, "mass_scale"))
            MeFAssetSetMassScale(Asset, static_cast<FLOAT>(strtod(MassScale, nullptr)));
        if (const char* LengthScale = MeXMLNodeGetAttribute(Node, "length_scale"))
            MeFAssetSetLengthScale(Asset, static_cast<FLOAT>(strtod(LengthScale, nullptr)));
        return Asset;
    }

    // 0x001cc160. writes an asset as xml.
    void* WriteAsset(void* Output, MeFAsset* Asset, void* Unknown)
    {
        NOT_DECOMPILED(0x001cc160);
        return nullptr;
    }
}

MeAssetDBXMLFormat MeAssetDBXML = { 1, 1, "1.0", MeAssetDBXMLReadKarma, CreateAssetFromNode, WriteAsset };
