#pragma once

// the MathEngine Karma physics library the game links statically: memory and message hooks (Me), the
// collision framework (Mcd) and the dynamics (Mdt). only what the engine has reached is decompiled.

#include <cstdarg>
#include <cstddef>
#include <cstdio>

#include "CoreTypes.h"

// ---------------------------------------------------------------------------------------------- memory

// the allocator the library uses. the game points these at its own allocator.
struct MeMemoryAPI
{
    void* (*Create)(INT Size);
    void* (*CreateZeroed)(INT Size);
    void* (*CreateAligned)(INT Size, INT Alignment);
    void  (*Destroy)(void* Ptr);
    void  (*DestroyAligned)(void* Ptr);
    void* (*Resize)(void* Ptr, INT Size);
};

// 0x0043227c
extern MeMemoryAPI MeMemory;

// a pool of same-sized structs. PoolType 1 is a fixed block, 2 allocates each struct on its own.
struct MePool
{
    INT PoolType;
    union
    {
        struct
        {
            void*  Memory;
            void** FreeList;
            INT    Used;
            INT    StructSize;
            INT    MaxStructs;
            UBOOL  Aligned;
        } Fixed;
        struct
        {
            INT Used;
            INT StructSize;
            INT MaxStructs;
            INT Alignment;
        } Malloc;
    };
};

struct MePoolAPI
{
    void  (*Init)(MePool* Pool, INT MaxStructs, INT StructSize, INT Alignment);
    void  (*Destroy)(MePool* Pool);
    void  (*Reset)(MePool* Pool);
    void* (*GetStruct)(MePool* Pool);
    void  (*PutStruct)(MePool* Pool, void* Struct);
    INT   (*GetUsed)(MePool* Pool);
    INT   (*GetUnused)(MePool* Pool);
};

// 0x004322dc
extern MePoolAPI MePools;

// ---------------------------------------------------------------------------------------------- messages

typedef void (*MeOutputFn)(INT Level, const char* Text);
typedef void (*MeMessageFn)(INT Level, const char* Fmt, va_list Args);

// 0x00432294. where each kind of message is formatted and printed.
struct MeMessageHandlers
{
    MeOutputFn  InfoOutput;
    MeOutputFn  WarningOutput;
    MeOutputFn  LogOutput;
    MeOutputFn  FatalOutput;
    MeOutputFn  DebugOutput;
    MeMessageFn Info;
    MeMessageFn Log;
    MeMessageFn Warning;
    MeMessageFn Fatal;
    MeMessageFn Debug;
    INT         Unknown28;
    INT         Unknown2C;
};

extern MeMessageHandlers MeMessages;

// 0x004ab480, 0x004ab484 and 0x004ab48c. messages above these levels are dropped; log messages use the info level.
extern INT MeInfoLevel;
extern INT MeWarningLevel;
extern INT MeDebugLevel;

MeOutputFn  MeSetInfoOutput(MeOutputFn Fn);
MeOutputFn  MeSetWarningOutput(MeOutputFn Fn);
MeOutputFn  MeSetFatalOutput(MeOutputFn Fn);
MeOutputFn  MeSetDebugOutput(MeOutputFn Fn);
MeMessageFn MeSetInfoHandler(MeMessageFn Fn);
MeMessageFn MeSetWarningHandler(MeMessageFn Fn);
MeMessageFn MeSetFatalHandler(MeMessageFn Fn);
MeMessageFn MeSetDebugHandler(MeMessageFn Fn);

void MeWarning(INT Level, const char* Fmt, ...);
void MeFatalError(INT Level, const char* Fmt, ...);

// ---------------------------------------------------------------------------------------------- collision framework

struct McdFramework;

// what a geometry type does. the signatures are filled in as each is decompiled.
struct McdGeometryType
{
    void  (*Destroy)(void* Geometry);
    void  (*GetAABB)(void* Geometry, void* Min, void* Max);
    void  (*GetBSphere)(void* Geometry, void* Center, void* Radius);
    void  (*MaximumPoint)(void* Geometry, void* Direction, void* Point);
    SWORD (*GetMassProperties)(void* Geometry, void* Unknown1, void* Unknown2, void* Unknown3);
    INT   Unknown14;
    void  (*DebugDraw)(void* Geometry, void* Unknown1, void* Unknown2);
    void  (*Unknown1C)();
    const char* Name;
    UBOOL Registered;
};

// what happens when two geometry types meet. Swapped marks the entry for the reversed pair.
struct McdInteraction
{
    void (*Unknown0)();
    void (*Unknown4)();
    void (*Unknown8)();
    void (*UnknownC)();
    UBOOL Swapped;
    INT   Unknown14;
    INT   Unknown18;
};

struct McdFrameworkHook
{
    void              (*Fn)();
    McdFrameworkHook* Next;
};

struct McdFramework
{
    INT               GeometryTypeCount;
    INT               RegisteredTypes;
    INT               Unknown8;
    // every geometry made with this framework, in a circular list
    struct McdGeometry* Geometries;
    INT               Unknown10;
    INT               GeometryCount;
    McdGeometryType*  Types;
    McdFrameworkHook* Hooks;
    McdInteraction*   Interactions;
    INT               Unknown24;
    INT               Unknown28[6];
    INT               Unknown40;
    MePool            ModelPool;
    MePool            Unknown60;
    FLOAT             Unknown7C;
    FLOAT             Scale;
    INT*              Unknown84;
    INT*              Unknown88;
    const char*       Version;
};

McdFramework* McdInit(INT GeometryTypeMaxCount, INT ModelCount, INT Unknown3, FLOAT Scale);
void McdGeometryRegisterType(McdFramework* Framework, SWORD TypeId, const char* Name, void (*Destroy)(void*),
    void (*GetAABB)(void*, void*, void*), void (*GetBSphere)(void*, void*, void*), void (*MaximumPoint)(void*, void*, void*),
    SWORD (*GetMassProperties)(void*, void*, void*, void*), void (*DebugDraw)(void*, void*, void*));
void McdFrameworkSetTypeFunction(McdFramework* Framework, INT TypeId, void (*Fn)());
void McdFrameworkSetInteraction(McdFramework* Framework, INT TypeA, INT TypeB, const McdInteraction* Interaction);
void McdFrameworkAddHook(McdFramework* Framework, void (*Fn)());

// the geometry types the game registers
void McdNullRegisterType(McdFramework* Framework);
void McdPrimitivesRegisterTypes(McdFramework* Framework);
void McdConvexMeshRegisterType(McdFramework* Framework);
void McdAggregateRegisterType(McdFramework* Framework);
void McdAggregateRegisterInteractions(McdFramework* Framework);
UBOOL McdPrimitivesRegisterInteractions(McdFramework* Framework);
UBOOL McdConvexMeshRegisterInteractions(McdFramework* Framework);

// a fixed array of pointers; its use is not known yet
struct MeUnknown1C
{
    void** Data;
    INT    Max;
    INT    Unknown8;
    INT    UnknownC;
    INT    Unknown10;
    INT    Unknown14;
    INT    Unknown18;
};

MeUnknown1C* MeUnknown1CCreate(INT Max);

// ---------------------------------------------------------------------------------------------- files

// a file read through stdio or from memory
struct MeFile
{
    char* Name;
    // 0 reads Handle, anything else reads Buffer
    INT   Type;
    FILE* Handle;
    BYTE* Buffer;
    INT   Unknown10;
    INT   Unknown14;
    DWORD Size;
    DWORD Pos;
};

DWORD MeFileRead(void* Dest, DWORD ElementSize, DWORD Count, MeFile* File);
void  MeFileDestroy(MeFile* File);

// ---------------------------------------------------------------------------------------------- xml

struct MeXMLInput;

// an open element while parsing: its tag, then where its attributes start
struct MeXMLElement
{
    char        Tag[1024];
    MeXMLInput* Input;
    INT         Depth;
    char*       Attributes;
};

struct MeXMLHandler;

typedef INT (*MeXMLElementFn)(MeXMLElement* Element, void* Context);
typedef INT (*MeXMLDataFn)(MeXMLInput* Input, MeXMLHandler* Handler, void* Data);
typedef INT (*MeXMLPostFn)(MeXMLElement* Element, void* Data, void* Context);

// how to handle one tag. Type 0 parses the element itself, 1 reads its contents as data, 2 ends the list.
struct MeXMLHandler
{
    const char* Tag;
    INT         Type;
    void*       Handler;
    INT         Unknown0C;
    INT         Unknown10;
    INT         Unknown14;
    UBOOL       Found;
    MeXMLPostFn Post;
};

struct MeXMLInput
{
    char    Buffer[0x1000];
    INT     Pos;
    INT     Length;
    char    Pushback[8];
    INT     PushbackCount;
    MeFile* File;
    INT     Line;
    INT     Column;
    UBOOL   EndOfFile;
    void*   UserData;
    char    Error[0x500];
};

MeXMLInput* MeXMLInputCreate(MeFile* File);
void        MeXMLInputSetUserData(MeXMLInput* Input, void* UserData);
void*       MeXMLInputGetUserData(MeXMLInput* Input);
const char* MeXMLInputGetErrorString(MeXMLInput* Input);
INT         MeXMLInputProcess(MeXMLInput* Input, MeXMLHandler* Handlers, void* Context);
INT         MeXMLParseElements(MeXMLElement* Parent, MeXMLHandler* Handlers, void* Data, void* Context);
MeXMLInput* MeXMLElementGetInput(MeXMLElement* Element);

// a parsed element kept as a tree, with its attributes
struct MeXMLAttribute
{
    char* Name;
    char* Value;
};

struct MeXMLList
{
    void*      Item;
    MeXMLList* Next;
};

struct MeXMLNode
{
    INT        Type;
    MeXMLList* Children;
    void*      Data;
    void       (*DestroyData)(void* Data);
    MeXMLList* Attributes;
};

// the second argument is not used; the game passes the element or the root's name
MeXMLNode*  MeXMLNodeCreate(INT Type, const void* Unused, void* Data, void (*DestroyData)(void*), char* Attributes);
INT         MeXMLReadText(MeXMLInput* Input, char* Text, INT Size);
void        MeXMLNodeAddChild(MeXMLNode* Child, MeXMLNode* Parent);
const char* MeXMLNodeGetAttribute(MeXMLNode* Node, const char* Name);
void        MeXMLNodeWalk(MeXMLNode* Node, void (*Fn)(MeXMLNode* Node, MeXMLNode* Parent, void* Context), UBOOL PreOrder, void* Context);
void        MeXMLNodeDestroyChildren(MeXMLNode* Node, MeXMLNode* Parent, void* Context);
void        MeXMLNodeDestroy(MeXMLNode* Node);

// ---------------------------------------------------------------------------------------------- geometry

// the start of every geometry: its type and its place in the framework's list
struct McdGeometry
{
    INT           TypeId;
    McdGeometry*  Next;
    McdGeometry*  Prev;
    McdFramework* Framework;
};

void         McdGeometryInit(McdGeometry* Geometry, McdFramework* Framework, SWORD TypeId);
McdGeometry* McdNullCreate(McdFramework* Framework);

// ---------------------------------------------------------------------------------------------- hash tables

struct MeHashEntry;

struct MeHash
{
    INT           Size;
    INT           Unknown4;
    DWORD         (*Hash)(const void* Key, DWORD Size);
    INT           (*Compare)(const void* A, const void* B);
    void          (*FreeData)(void* Data);
    INT           Unknown14;
    MeHashEntry** Table;
    INT           Unknown1C;
};

MeHash* MeHashCreate(INT Size);
void    MeHashSetHashFunction(MeHash* Hash, DWORD (*Fn)(const void*, DWORD));
void    MeHashSetCompareFunction(MeHash* Hash, INT (*Fn)(const void*, const void*));
void    MeHashSetDataFreeFunction(MeHash* Hash, void (*Fn)(void*));

// ---------------------------------------------------------------------------------------------- assets

struct MeAssetDB;

struct MeFAsset;

// a list node of the asset's lists, with the list itself as a node without an item
struct MeFAssetNode
{
    MeFAssetNode* Next;
    void*         Item;
    MeFAssetNode* Prev;
};

// hands out small ids, one bit each
struct MeIDPool
{
    DWORD* Bits;
    INT    Word;
    INT    Words;
    INT    Capacity;
    INT    Used;
};

MeIDPool* MeIDPoolCreate();
INT       MeIDPoolRequestID(MeIDPool* Pool);
void      MeIDPoolReset(MeIDPool* Pool);

// a shape inside a geometry
struct MeFPrimitive
{
    char*  Name;
    struct MeFGeometry* Owner;
    FLOAT* Vertices;
    INT    VertexCount;
    INT    VertexCapacity;
    INT    Type;
    FLOAT  Dims[3];
    FLOAT  TM[16];
};

struct MeFGeometry
{
    char*         Name;
    MeFAsset*     Owner;
    MeFAssetNode* Primitives;
    char*         Graphic;
    FLOAT         Scale;
    FLOAT         GraphicOffset[3];
    INT           PrimitiveCount;
};

struct MeFModel
{
    char*     Name;
    // 0 has dynamics and geometry, 1 only dynamics, 2 only geometry
    INT       Type;
    MeFAsset* Owner;
    char*     Geometry;
    FLOAT     MassOffset[3];
    FLOAT     Mass;
    FLOAT     Density;
    FLOAT     Inertia00;
    FLOAT     Inertia01;
    FLOAT     Inertia02;
    FLOAT     Inertia11;
    FLOAT     Inertia12;
    FLOAT     Inertia22;
    FLOAT     LinearDamping;
    FLOAT     AngularDamping;
    FLOAT     FastSpin[3];
    INT       UseFastSpin;
};

struct MeFPart
{
    char*     Name;
    MeFAsset* Owner;
    char*     Model;
    char*     Graphic;
    char*     Parent;
    FLOAT     TM[16];
    FLOAT     Scale;
    FLOAT     GraphicOffset[3];
    INT       Id;
};

union MeFJointValue
{
    FLOAT Real;
    INT   Int;
};

struct MeFJoint
{
    char*         Name;
    char*         Parts[2];
    MeFAsset*     Owner;
    INT           Type;
    FLOAT         Pos[2][3];
    FLOAT         PrimaryAxis[2][3];
    FLOAT         OrthogonalAxis[2][3];
    MeFJointValue Values[29];
};

// an asset: parts with models and geometry, joined by joints
struct MeFAsset
{
    char*         Id;
    MeAssetDB*    Owner;
    MeFAssetNode* Geometries;
    MeFAssetNode* Models;
    MeFAssetNode* Parts;
    MeFAssetNode* Joints;
    MeHash*       GeometryHash;
    MeHash*       ModelHash;
    MeHash*       PartHash;
    MeHash*       JointHash;
    char*         Graphic;
    char*         RefPart;
    // a bit for each pair of parts that must not collide
    DWORD*        NoCollision;
    MeIDPool*     PartIDs;
    INT           AssetId;
    FLOAT         Scale;
    INT           GeometryCount;
    INT           ModelCount;
    INT           JointCount;
    INT           PartCount;
    INT           MaxParts;
    FLOAT         MassScale;
    FLOAT         LengthScale;
};

void* MeHashInsert(const void* Key, void* Data, MeHash* Hash);
void* MeHashLookup(const void* Key, MeHash* Hash);

MeFAsset*     MeFAssetCreate(const char* Id, INT AssetId);
void          MeFAssetInsertGeometry(MeFAsset* Asset, MeFGeometry* Geometry);
void          MeFAssetInsertModel(MeFAsset* Asset, MeFModel* Model);
void          MeFAssetInsertPart(MeFAsset* Asset, MeFPart* Part);
void          MeFAssetInsertJoint(MeFAsset* Asset, MeFJoint* Joint);
MeFGeometry*  MeFAssetLookupGeometry(MeFAsset* Asset, const char* Name);
MeFModel*     MeFAssetLookupModel(MeFAsset* Asset, const char* Name);
MeFPart*      MeFAssetLookupPart(MeFAsset* Asset, const char* Name);
void          MeFAssetSetGraphic(MeFAsset* Asset, const char* Graphic);
void          MeFAssetSetRefPart(MeFAsset* Asset, const char* RefPart);
void          MeFAssetSetLengthScale(MeFAsset* Asset, FLOAT Scale);

MeFGeometry*  MeFGeometryCreate(const char* Name);
void          MeFGeometryInsertPrimitive(MeFGeometry* Geometry, MeFPrimitive* Primitive);
void          MeFGeometrySetScale(MeFGeometry* Geometry, FLOAT Scale);
void          MeFGeometrySetGraphicOffset(MeFGeometry* Geometry, FLOAT X, FLOAT Y, FLOAT Z);

MeFPrimitive* MeFPrimitiveCreate(const char* Name, INT Type);
void          MeFPrimitiveSetRadius(MeFPrimitive* Primitive, FLOAT Radius);
void          MeFPrimitiveSetDims(MeFPrimitive* Primitive, FLOAT X, FLOAT Y, FLOAT Z);
void          MeFPrimitiveAddVertex(MeFPrimitive* Primitive, const FLOAT* Vertex);
void          MeFPrimitiveSetTM(MeFPrimitive* Primitive, const FLOAT* TM);

MeFModel*     MeFModelCreate(const char* Name, INT Type);
void          MeFModelSetMassOffset(MeFModel* Model, const FLOAT* Offset);
void          MeFModelSetDensity(MeFModel* Model, FLOAT Density);
void          MeFModelSetInertiaTensor(MeFModel* Model, const FLOAT* Inertia);
void          MeFModelSetAngularDamping(MeFModel* Model, FLOAT Damping);
void          MeFModelSetFastSpinAxis(MeFModel* Model, FLOAT X, FLOAT Y, FLOAT Z);
void          MeFModelSetUseFastSpinAxis(MeFModel* Model, INT Use);
void          MeFModelSetGeometry(MeFModel* Model, MeFGeometry* Geometry);

MeFPart*      MeFPartCreate(const char* Name, MeFModel* Model, const FLOAT* TM);
void          MeFPartSetGraphic(MeFPart* Part, const char* Graphic);
void          MeFPartSetParent(MeFPart* Part, const char* Parent);
void          MeFPartSetGraphicOffset(MeFPart* Part, FLOAT X, FLOAT Y, FLOAT Z);
void          MeFPartSetCollision(MeFPart* Part1, MeFPart* Part2, UBOOL Enable);

MeFJoint*     MeFJointCreate(const char* Name, INT Type);
void          MeFJointSetReal(MeFJoint* Joint, INT Property, FLOAT Value);
void          MeFJointSetInt(MeFJoint* Joint, INT Property, INT Value);
void          MeFJointSetEnum(MeFJoint* Joint, INT Property, INT Value);
void          MeFJointSetPosition(MeFJoint* Joint, INT Body, FLOAT X, FLOAT Y, FLOAT Z);
void          MeFJointSetPrimaryAxis(MeFJoint* Joint, INT Body, FLOAT X, FLOAT Y, FLOAT Z);
void          MeFJointSetOrthogonalAxis(MeFJoint* Joint, INT Body, FLOAT X, FLOAT Y, FLOAT Z);
void          MeFJointSetPart(MeFJoint* Joint, const char* Part, INT Body);

// one store each; the xbox shares each between two types whose fields sit at the same offset
void          MeFAssetSetScale(MeFAsset* Asset, FLOAT Scale);
void          MeFModelSetLinearDamping(MeFModel* Model, FLOAT Damping);
void          MeFAssetSetMassScale(MeFAsset* Asset, FLOAT Scale);
void          MeFPartSetScale(MeFPart* Part, FLOAT Scale);
void          MeFModelSetMass(MeFModel* Model, FLOAT Mass);
void          MeFPrimitiveSetHeight(MeFPrimitive* Primitive, FLOAT Height);
void          MeFGeometrySetGraphic(MeFGeometry* Geometry, const char* Graphic);

void          MeStrSet(char** Field, const char* Str);

struct MeAssetDBNode
{
    MeAssetDBNode* Next;
    MeFAsset*      Asset;
    MeAssetDBNode* Prev;
};

// assets kept in order of their ids
struct MeAssetDB
{
    MeAssetDBNode* Assets;
    INT            Count;
};

MeAssetDB* MeAssetDBCreate();
void       MeAssetDBInsertAsset(MeAssetDB* DB, MeFAsset* Asset);
void       MeAssetDBRemoveAsset(MeFAsset* Asset);
INT        MeAssetDBGetAssetCount(MeAssetDB* DB);

// turns assets into collision and dynamics objects
struct MeAssetFactory
{
    McdFramework* Framework;
    MeHash*       Unknown4;
    MeHash*       Unknown8;
    McdGeometry*  NullGeometry;
};

MeAssetFactory* MeAssetFactoryCreate(McdFramework* Framework);

struct MeAssetDBXMLInput
{
    MeAssetDB* DB;
    void*      Unknown4;
};

MeAssetDBXMLInput* MeAssetDBXMLInputCreate(MeAssetDB* DB, void* Unknown);
UBOOL              MeAssetDBXMLInputRead(MeAssetDBXMLInput* Input, MeFile* File);
void               MeAssetDBXMLInputDestroy(MeAssetDBXMLInput* Input);

// 0x004322c4. how assets are read from and written to xml.
struct MeAssetDBXMLFormat
{
    INT            Unknown0;
    INT            Unknown4;
    const char*    Version;
    MeXMLElementFn ReadKarma;
    MeFAsset*      (*CreateAsset)(MeAssetDB* DB, void* Unknown, MeXMLNode* Node);
    void*          (*WriteAsset)(void* Output, MeFAsset* Asset, void* Unknown);
};

extern MeAssetDBXMLFormat MeAssetDBXML;

// ---------------------------------------------------------------------------------------------- xml asset elements

// what PRIMITIVE reads. a sphere uses the radius, a box the dimensions, a cylinder the radius and height.
struct MeXMLPrimitive
{
    FLOAT  Dims[3];
    INT    VertexCount;
    FLOAT* Vertices;
    INT    VertexCapacity;
    FLOAT  Vertex[3];
    FLOAT  TM[16];
};

// what DYNAMICS reads
struct MeXMLDynamics
{
    FLOAT MassOffset[3];
    FLOAT Mass;
    FLOAT Density;
    FLOAT Inertia[6];
    FLOAT LinearDamping;
    FLOAT AngularDamping;
    FLOAT FastSpin[3];
    INT   UseFastSpin;
};

// what PART reads
struct MeXMLPart
{
    FLOAT TM[16];
};

// what JOINT reads. many tags share a field, depending on the joint type.
struct MeXMLJoint
{
    FLOAT Pos1[3];
    FLOAT Pos2[3];
    FLOAT PrimaryAxis1[3];
    FLOAT PrimaryAxis2[3];
    FLOAT OrthogonalAxis1[3];
    FLOAT OrthogonalAxis2[3];
    FLOAT Unknown48;
    FLOAT Unknown4C;
    FLOAT Unknown50;
    FLOAT Unknown54;
    FLOAT Unknown58;
    FLOAT Unknown5C;
    FLOAT Unknown60[3];
    FLOAT Unknown6C;
    FLOAT Unknown70;
    FLOAT Unknown74;
    FLOAT Unknown78[3];
    INT   Limited;
    INT   Motorized;
    FLOAT Unknown8C;
    FLOAT Unknown90;
    FLOAT Unknown94;
    FLOAT Unknown98;
    FLOAT Unknown9C;
    FLOAT UnknownA0;
    FLOAT UnknownA4;
    FLOAT UnknownA8;
    FLOAT UnknownAC;
    INT   ConeType;
    INT   TwistType;
    INT   UnknownB8;
};

// the tree node types the reader makes
enum EMeXMLNodeType
{
    MEXML_Root        = 0,
    MEXML_Geometry    = 1,
    MEXML_Primitive   = 2,
    MEXML_Model       = 3,
    MEXML_Dynamics    = 4,
    MEXML_Joint       = 5,
    MEXML_Asset       = 6,
    MEXML_Part        = 7,
    MEXML_NoCollision = 8,
};

INT MeAssetDBXMLReadKarma(MeXMLElement* Element, void* Context);
