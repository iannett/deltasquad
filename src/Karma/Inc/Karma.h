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
    // every model made with this framework, in a circular list
    struct McdModel*  Models;
    // every geometry made with this framework, in a circular list
    struct McdGeometry* Geometries;
    INT               ModelCount;
    INT               GeometryCount;
    McdGeometryType*  Types;
    McdFrameworkHook* Hooks;
    McdInteraction*   Interactions;
    INT               Unknown24;
    INT               Unknown28[6];
    INT               Unknown40;
    MePool            ModelPool;
    MePool            InstancePool;
    // how far each new model's box reaches past its geometry
    FLOAT             DefaultPadding;
    FLOAT             Scale;
    INT*              Unknown84;
    // what a collision query asks for: the most contacts, then whether face normals come first
    INT*              DefaultRequest;
    const char*       Version;
};

const char*   McdGetVersion();
McdFramework* McdInit(INT GeometryTypeMaxCount, INT ModelCount, INT Unknown3, FLOAT Scale);
void McdGeometryRegisterType(McdFramework* Framework, SWORD TypeId, const char* Name, void (*Destroy)(void*),
    void (*GetAABB)(void*, void*, void*), void (*GetBSphere)(void*, void*, void*), void (*MaximumPoint)(void*, void*, void*),
    SWORD (*GetMassProperties)(void*, void*, void*, void*), void (*DebugDraw)(void*, void*, void*));
void McdFrameworkSetTypeFunction(McdFramework* Framework, INT TypeId, void (*Fn)());
void McdFrameworkSetInteraction(McdFramework* Framework, INT TypeA, INT TypeB, const McdInteraction* Interaction);
void McdFrameworkAddHook(McdFramework* Framework, void (*Fn)());
// 0x0018cc40
INT* McdFrameworkGetDefaultRequestPtr(McdFramework* Framework);

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
void         MeUnknown1CDestroy(MeUnknown1C* Array);
void         MeUnknown1CReset(MeUnknown1C* Array);
INT          MeUnknown1CCount10(MeUnknown1C* Array);
INT          MeUnknown1CCount14(MeUnknown1C* Array);

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
    INT           Count;
    DWORD         (*Hash)(const void* Key, DWORD Size);
    INT           (*Compare)(const void* A, const void* B);
    void          (*FreeKey)(void* Key);
    void          (*FreeData)(void* Data);
    MeHashEntry** Table;
    INT           Unknown1C;
};

MeHash* MeHashCreate(INT Size);
void    MeHashSetHashFunction(MeHash* Hash, DWORD (*Fn)(const void*, DWORD));
void    MeHashSetCompareFunction(MeHash* Hash, INT (*Fn)(const void*, const void*));
void    MeHashSetKeyFreeFunction(MeHash* Hash, void (*Fn)(void*));
void*   MeHashRemove(const void* Key, MeHash* Hash);
void    MeHashEmpty(MeHash* Hash);
void    MeHashDestroy(MeHash* Hash);

// walks every entry of a hash, bucket by bucket
struct MeHashIterator
{
    MeHash*      Hash;
    MeHashEntry* Entry;
    INT          Bucket;
};

void  MeHashIteratorInit(MeHashIterator* It, MeHash* Hash);
void* MeHashIteratorGetNext(MeHashIterator* It);

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
    // geometry made from assets, shared by name ("asset::geometry" and "asset::geometry::primitive")
    MeHash*       GeometryByName;
    MeHash*       NameByGeometry;
    McdGeometry*  NullGeometry;
};

MeAssetFactory* MeAssetFactoryCreate(McdFramework* Framework);
// the collision geometry of an asset's geometry: an aggregate of its primitives
McdGeometry*    McdGMCreateGeometry(MeAssetFactory* Factory, MeFGeometry* Geometry, const char* AssetId);
void            McdGMDestroyGeometry(MeAssetFactory* Factory, McdGeometry* Geometry);

// walks one of an asset's lists
typedef MeFAssetNode* MeFAssetIterator;

void*        MeFAssetIteratorGetNext(MeFAssetIterator* It);
void         MeFAssetGetGeometryIterator(MeFAsset* Asset, MeFAssetIterator* It);
void         MeFAssetGetJointIterator(MeFAsset* Asset, MeFAssetIterator* It);
INT          MeFAssetGetPartCount(MeFAsset* Asset);
INT          MeFAssetGetJointCount(MeFAsset* Asset);
void         MeFAssetGetPartsSortedByName(MeFAsset* Asset, MeFPart** Parts);
const char*  MeFAssetGetId(MeFAsset* Asset);
MeFAsset*    MeAssetDBLookupAsset(MeAssetDB* DB, const char* Id);
MeFModel*    MeFPartGetModel(MeFPart* Part);
FLOAT*       MeFPartGetTransformPtr(MeFPart* Part);
UBOOL        MeFPartIsCollisionEnabled(MeFPart* Part1, MeFPart* Part2);
MeFGeometry* MeFModelGetGeometry(MeFModel* Model);
void         MeFModelGetInertiaTensor(MeFModel* Model, FLOAT* Inertia);
MeFPart*     MeFJointGetPart(MeFJoint* Joint, INT Body);
INT          MeFJointGetType(MeFJoint* Joint);
void         MeFJointGetPosition(MeFJoint* Joint, INT Body, FLOAT* Pos);
void         MeFJointGetPrimaryAxis(MeFJoint* Joint, INT Body, FLOAT* Axis);
void         MeFJointGetOrthogonalAxis(MeFJoint* Joint, INT Body, FLOAT* Axis);
UBOOL        MeFJointGetReal(MeFJoint* Joint, INT Property, FLOAT* Value);
UBOOL        MeFJointGetInt(MeFJoint* Joint, INT Property, INT* Value);
UBOOL        MeFJointGetEnum(MeFJoint* Joint, INT Property, INT* Value);

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

// ---------------------------------------------------------------------------------------------- math

// 0x0018c8f0. Out is A followed by B, for transforms whose last column is 0, 0, 0, 1.
void MeMatrix4TMCompose(FLOAT* Out, const FLOAT* A, const FLOAT* B);
// Out is A followed by B for any 4x4 matrices
void MeMatrix4MultiplyMatrix(FLOAT* Out, const FLOAT* A, const FLOAT* B);
void MeMatrix4TMTransform(FLOAT* Out, const FLOAT* TM, const FLOAT* V);
void MeMatrix4TMInverseTransform(FLOAT* Out, const FLOAT* TM, const FLOAT* V);
void MeMatrix4TMRotate(FLOAT* Out, const FLOAT* TM, const FLOAT* V);
// scales V to unit length and returns the old length; a zero vector becomes 1, 0, 0
FLOAT MeVector3Normalize(FLOAT* V);
// two unit vectors at right angles to N and each other
void MeVector3PlaneSpace(const FLOAT* N, FLOAT* P, FLOAT* Q);
// the rotation of TM as a quaternion, w first
void MeQuaternionFromTM(FLOAT* Q, const FLOAT* TM);

// ---------------------------------------------------------------------------------------------- dictionaries

// a red-black tree of keys and data, in the layout of kazlib's dict
struct MeDictNode
{
    MeDictNode* Left;
    MeDictNode* Right;
    MeDictNode* Parent;
    INT         Color;
    const void* Key;
    void*       Data;
};

struct MeDict
{
    // the sentinel every leaf points at
    MeDictNode  Nil;
    DWORD       NodeCount;
    DWORD       MaxCount;
    INT         (*Compare)(const void* A, const void* B);
    MeDictNode* (*AllocNode)(void* Context);
    void        (*FreeNode)(MeDictNode* Node, void* Context);
    void*       Context;
    UBOOL       AllowDupes;
};

void MeDictInit(MeDict* Dict, DWORD MaxCount, INT (*Compare)(const void* A, const void* B));
void MeDictAllowDupes(MeDict* Dict);
void MeDictNodeInit(MeDictNode* Node, void* Data);
void MeDictInsert(MeDict* Dict, MeDictNode* Node, const void* Key);
MeDictNode* MeDictDelete(MeDict* Dict, MeDictNode* Node);
MeDictNode* MeDictFirst(MeDict* Dict);
MeDictNode* MeDictNext(MeDict* Dict, MeDictNode* Node);
// 0x00184e90. orders keys by their value.
INT  MeDictCompareKeys(const void* A, const void* B);

// a block of memory that grows as needed
struct MeChunk
{
    void* Data;
    INT   Size;
    // the largest size asked for
    INT   MaxSize;
    INT   Alignment;
    // 2 frees the memory each time it is put back
    INT   Unknown10;
    UBOOL InUse;
};

void  MeChunkInit(MeChunk* Chunk, INT Alignment);
void* MeChunkGetMem(MeChunk* Chunk, INT Size);
void  MeChunkPutMem(MeChunk* Chunk, void* Mem);

// ---------------------------------------------------------------------------------------------- dynamics

// how two touching surfaces behave
struct MdtContactParams
{
    // 0 no friction, 1 friction in one direction, 2 in both
    INT   Type;
    // 0 box friction, 1 friction scaled by the normal force
    INT   PrimaryFrictionModel;
    INT   SecondaryFrictionModel;
    // 4 is set while the contact is soft
    DWORD Options;
    FLOAT Restitution;
    FLOAT VelocityThreshold;
    FLOAT Softness;
    FLOAT Unknown1C;
    FLOAT PrimaryFriction;
    FLOAT PrimarySlip;
    FLOAT Unknown28;
    FLOAT SecondaryFriction;
    FLOAT SecondarySlip;
    FLOAT Unknown34;
    FLOAT Unknown38;
    FLOAT Unknown3C;
    INT   Unknown40;
    INT   Unknown44;
};

void MdtContactParamsReset(MdtContactParams* Params);
void MdtContactParamsSetType(MdtContactParams* Params, INT Type);
void MdtContactParamsSetFrictionModel(MdtContactParams* Params, INT Model);
void MdtContactParamsSetSoftness(MdtContactParams* Params, FLOAT Softness);
void MdtContactParamsSetFriction(MdtContactParams* Params, FLOAT Friction);
void MdtContactParamsSetPrimarySlip(MdtContactParams* Params, FLOAT Slip);

typedef void (*MdtBodyCB)(void* Body);

// the dynamics of a level. its bodies and constraints come from fixed pools.
struct MdtWorld
{
    MeDict      Bodies;
    // the constraints that are enabled
    MeDict      EnabledConstraints;
    // the bodies that are enabled
    MeDict      EnabledBodies;
    MeChunk     Unknown9C;
    INT         MaxBodies;
    INT         BodyCount;
    INT         EnabledBodyCount;
    MePool      BodyPool;
    MeChunk     UnknownDC;
    INT         MaxConstraints;
    INT         EnabledConstraintCount;
    MePool      ConstraintPool;
    INT         Unknown118[7];
    MeChunk     Unknown134;
    MeChunk     Unknown14C;
    FLOAT       Gravity[3];
    INT         Unknown170;
    INT         Unknown174;
    INT         Unknown178;
    FLOAT       LengthScale;
    FLOAT       MassScale;
    // mass over length cubed
    FLOAT       DensityScale;
    FLOAT       Gamma;
    FLOAT       Unknown18C;
    UBOOL       AutoDisable;
    FLOAT       AutoDisableVelocityThreshold;
    FLOAT       AutoDisableAngularVelocityThreshold;
    FLOAT       AutoDisableAccelerationThreshold;
    FLOAT       AutoDisableAngularAccelerationThreshold;
    FLOAT       AutoDisableAliveWindow;
    INT         Unknown1A8;
    INT         Unknown1AC;
    FLOAT       Unknown1B0;
    FLOAT       Unknown1B4;
    FLOAT       Unknown1B8;
    FLOAT       Unknown1BC;
    FLOAT       Unknown1C0;
    FLOAT       Unknown1C4;
    INT         Unknown1C8;
    MdtBodyCB   BodyEnableCB;
    MdtBodyCB   BodyDisableCB;
    // called with a type 5 constraint when it is destroyed
    MdtBodyCB   Unknown1D4;
    const char* Unknown1D8;
    INT         Unknown1DC;
    FLOAT       Epsilon;
    FLOAT       Unknown1E4;
    INT         Unknown1E8;
    FLOAT       Unknown1EC;
    INT         Unknown1F0[2];
    INT         Unknown1F8;
    INT         Unknown1FC;
    INT         Unknown200;
    INT         Unknown204;
    INT         Unknown208;
    INT         Unknown20C;
    INT         Unknown210;
    INT         Unknown214;
    INT         Unknown218[4];
    void        (*Unknown228)(void* Unknown1, void* Bodies, INT Count);
    UBOOL       Unknown22C;
    INT         Unknown230;
};

MdtWorld* MdtWorldCreate(INT MaxBodies, INT MaxConstraints, FLOAT LengthScale, FLOAT MassScale);
void MdtWorldSetEpsilon(MdtWorld* World, FLOAT Epsilon);
void MdtWorldSetAutoDisable(MdtWorld* World, UBOOL AutoDisable);
void MdtWorldSetAutoDisableVelocityThreshold(MdtWorld* World, FLOAT Threshold);
void MdtWorldSetAutoDisableAngularVelocityThreshold(MdtWorld* World, FLOAT Threshold);
void MdtWorldSetAutoDisableAccelerationThreshold(MdtWorld* World, FLOAT Threshold);
void MdtWorldSetAutoDisableAngularAccelerationThreshold(MdtWorld* World, FLOAT Threshold);
void MdtWorldSetAutoDisableAliveWindow(MdtWorld* World, FLOAT Time);
void MdtWorldSetUnknown22C(MdtWorld* World, UBOOL Value);
void MdtWorldSetGamma(MdtWorld* World, FLOAT Gamma);

struct McdModel;

// a rigid body. bodies come from the world's body pool.
struct MdtBody
{
    // 100 while in use, -1 once destroyed
    INT          Tag;
    INT          Unknown4;
    FLOAT        InvMass;
    // 1 uses the fast spin axis; 4 and 8 are set and cleared by name only (MdtBodySetFlag4 and so on)
    DWORD        Flags;
    FLOAT        Unknown10[4];
    FLOAT        Unknown20[4];
    // rows of three with a pad
    FLOAT        InvInertia[3][4];
    FLOAT        Inertia[3][4];
    // a model following the body reads these through its velocity pointers
    FLOAT        LinearVelocity[4];
    FLOAT        AngularVelocity[4];
    FLOAT        Quaternion[4];
    FLOAT        UnknownC0[4];
    FLOAT        UnknownD0[4];
    FLOAT        FastSpinAxis[4];
    // the transform of the center of mass
    FLOAT        ComTM[16];
    FLOAT        TM[16];
    FLOAT        Unknown170[4];
    FLOAT        Unknown180[4];
    // where the center of mass sits in the body's frame
    FLOAT        ComOffset[3];
    UBOOL        HasComOffset;
    INT          Unknown1A0;
    MdtWorld*    World;
    INT          Key;
    // in the world's body dictionary
    MeDictNode   WorldNode;
    // in the world's enabled body dictionary while enabled
    MeDictNode   EnabledNode;
    void*        UserData;
    INT          Unknown1E0;
    INT          Unknown1E4;
    INT          Unknown1E8;
    // 1 while enabled
    DWORD        EnabledFlags;
    FLOAT        Mass;
    FLOAT        LinearDamping;
    FLOAT        AngularDamping;
    // the constraints on this body
    MeDict       Constraints;
    // reset whenever the body is enabled or disabled
    INT          Unknown230;
    McdModel*    Model;
    FLOAT        Unknown238;
};

MdtBody* MdtBodyCreate(MdtWorld* World);
void     MdtBodyReset(MdtBody* Body);
void     MdtBodyDestroy(MdtBody* Body);
void     MdtBodyEnable(MdtBody* Body);
void     MdtBodyDisable(MdtBody* Body);
UBOOL    MdtBodyIsEnabled(MdtBody* Body);
void     MdtBodySetTransform(MdtBody* Body, const FLOAT* TM);
FLOAT*   MdtBodyGetTransformPtr(MdtBody* Body);
FLOAT*   MdtBodyGetLinearVelocityPtr(MdtBody* Body);
FLOAT*   MdtBodyGetAngularVelocityPtr(MdtBody* Body);
void     MdtBodySetMass(MdtBody* Body, FLOAT Mass);
void     MdtBodySetInertiaTensor(MdtBody* Body, const FLOAT* Inertia);
void     MdtBodyGetInertiaTensor(MdtBody* Body, FLOAT* Inertia);
void     MdtBodySetCenterOfMassRelativePosition(MdtBody* Body, const FLOAT* Pos);
void     MdtBodySetLinearVelocityDamping(MdtBody* Body, FLOAT Damping);
void     MdtBodySetAngularVelocityDamping(MdtBody* Body, FLOAT Damping);
void     MdtBodySetFastSpinAxis(MdtBody* Body, FLOAT X, FLOAT Y, FLOAT Z);
void     MdtBodySetFlag4(MdtBody* Body);
void     MdtBodySetFlag8(MdtBody* Body);
void     MdtBodyClearFlag8(MdtBody* Body);
void     MdtBodyForAllConstraints(MdtBody* Body, void (*Fn)(struct MdtConstraint* Constraint, void* Context), void* Context);

// world to body frame conversions. a null body is the world frame.
void MdtConvertVector(MdtBody* From, const FLOAT* V, MdtBody* To, FLOAT* Out);
void MdtConvertPositionVector(MdtBody* From, const FLOAT* V, MdtBody* To, FLOAT* Out);

// what every constraint starts with. the type decides what follows.
struct MdtConstraint
{
    MdtWorld*   World;
    INT         Key;
    // in the world's enabled constraint dictionary while enabled
    MeDictNode  WorldNode;
    // in each body's constraint dictionary while enabled
    MeDictNode  BodyNode[2];
    INT         Unknown50;
    MdtBody*    Body[2];
    // 1 while enabled
    DWORD       Flags;
    // adds the constraint's rows for the solver
    void        (*AddRows)(void* Unknown1, MdtConstraint* Constraint, void* Unknown2);
    INT         MaxRows;
    FLOAT       Unknown68[4];
    FLOAT       Unknown78[4];
    FLOAT       Unknown88[4];
    FLOAT       Unknown98[4];
    void        (*SetBodies)(MdtConstraint* Constraint, MdtBody* Body1, MdtBody* Body2);
    void        (*SetAxis)(MdtConstraint* Constraint, FLOAT X, FLOAT Y, FLOAT Z);
    // 0 ball and socket, 1 hinge, 5 has a world callback when destroyed, 9 skeletal
    INT         Type;
    INT         UnknownB4;
    INT         UnknownB8;
    // a frame for each body in that body's own frame: primary, orthogonal and their cross, then the position
    struct
    {
        FLOAT   Primary[4];
        FLOAT   Orthogonal[4];
        FLOAT   Third[4];
        FLOAT   Pos[4];
    }           Frame[2];
    FLOAT       Unknown13C[3];
    FLOAT       Unknown148[3];
    INT         Unknown154[3];
};

// one end of a limit
struct MdtSingleLimit
{
    FLOAT Stop;
    FLOAT Stiffness;
    FLOAT Damping;
    FLOAT Restitution;
};

// limits and a motor along one degree of freedom
struct MdtLimit
{
    UBOOL          Limited;
    // the limit has something to do: limited, motorized or Unknown1C
    UBOOL          Active;
    // KInitSkeletonKarma sets this on every hinge
    INT            Unknown8;
    INT            UnknownC;
    INT            Unknown10;
    INT            Unknown14;
    INT            Unknown18;
    INT            Unknown1C;
    INT            Unknown20;
    INT            Unknown24;
    INT            Unknown28;
    FLOAT          Unknown2C;
    MdtSingleLimit Lower;
    MdtSingleLimit Upper;
    UBOOL          Motorized;
    FLOAT          DesiredVelocity;
    FLOAT          MaxForce;
};

void            MdtLimitReset(MdtLimit* Limit);
MdtSingleLimit* MdtLimitGetLowerLimit(MdtLimit* Limit);
MdtSingleLimit* MdtLimitGetUpperLimit(MdtLimit* Limit);
void            MdtLimitSetLimited(MdtLimit* Limit, UBOOL Limited);
void            MdtLimitSetLimitedForceMotor(MdtLimit* Limit, FLOAT DesiredVelocity, FLOAT MaxForce);
void            MdtLimitActivateMotor(MdtLimit* Limit, UBOOL Motorized);
void            MdtSingleLimitSetStop(MdtSingleLimit* Limit, FLOAT Stop);
void            MdtSingleLimitSetStiffness(MdtSingleLimit* Limit, FLOAT Stiffness);

struct MdtBSJoint
{
    MdtConstraint Base;
};

struct MdtHinge
{
    MdtConstraint Base;
    MdtLimit      Limit;
};

// a cone around the primary axis with a limited twist
struct MdtSkeletal
{
    MdtConstraint Base;
    INT           Unknown160;
    INT           ConeType;
    INT           TwistType;
    // cosines of the half angles
    FLOAT         CosTwistHalfAngle;
    FLOAT         CosConeHalfAngleX;
    FLOAT         CosConeHalfAngleY;
    FLOAT         TwistStiffness;
    FLOAT         TwistDamping;
    FLOAT         ConeStiffness;
    FLOAT         ConeDamping;
};

// what the constraint pool hands out; every type fits. the xbox entries are 0x1ec bytes.
union MdtConstraintStorage
{
    MdtBSJoint  BSJoint;
    MdtHinge    Hinge;
    MdtSkeletal Skeletal;
    BYTE        Xbox[0x1ec];
};

void     MdtConstraintReset(MdtConstraint* Constraint);
void     MdtConstraintEnable(MdtConstraint* Constraint);
void     MdtConstraintDisable(MdtConstraint* Constraint);
UBOOL    MdtConstraintIsEnabled(MdtConstraint* Constraint);
void     MdtConstraintDestroy(MdtConstraint* Constraint);
void     MdtConstraintSetBodies(MdtConstraint* Constraint, MdtBody* Body1, MdtBody* Body2);
MdtBody* MdtConstraintGetBody(MdtConstraint* Constraint, INT Index);
void     MdtConstraintBodySetPositionRel(MdtConstraint* Constraint, INT Index, FLOAT X, FLOAT Y, FLOAT Z);
void     MdtConstraintBodySetPosition(MdtConstraint* Constraint, INT Index, FLOAT X, FLOAT Y, FLOAT Z);
void     MdtConstraintBodySetAxesRel(MdtConstraint* Constraint, INT Index, FLOAT PX, FLOAT PY, FLOAT PZ, FLOAT OX, FLOAT OY, FLOAT OZ);
void     MdtConstraintBodySetAxes(MdtConstraint* Constraint, INT Index, FLOAT PX, FLOAT PY, FLOAT PZ, FLOAT OX, FLOAT OY, FLOAT OZ);

MdtBSJoint*  MdtBSJointCreate(MdtWorld* World);
MdtHinge*    MdtHingeCreate(MdtWorld* World);
MdtSkeletal* MdtSkeletalCreate(MdtWorld* World);
MdtBSJoint*  MdtConstraintDCastBSJoint(MdtConstraint* Constraint);
MdtHinge*    MdtConstraintDCastHinge(MdtConstraint* Constraint);
MdtSkeletal* MdtConstraintDCastSkeletal(MdtConstraint* Constraint);
MdtLimit*    MdtHingeGetLimit(MdtHinge* Hinge);
void MdtSkeletalSetConeType(MdtSkeletal* Joint, INT Type);
void MdtSkeletalSetTwistType(MdtSkeletal* Joint, INT Type);
void MdtSkeletalSetConeHalfAngleX(MdtSkeletal* Joint, FLOAT Angle);
void MdtSkeletalSetConeHalfAngleY(MdtSkeletal* Joint, FLOAT Angle);
void MdtSkeletalSetTwistHalfAngle(MdtSkeletal* Joint, FLOAT Angle);
void MdtSkeletalSetConeStiffness(MdtSkeletal* Joint, FLOAT Stiffness);
void MdtSkeletalSetConeDamping(MdtSkeletal* Joint, FLOAT Damping);
void MdtSkeletalSetTwistStiffness(MdtSkeletal* Joint, FLOAT Stiffness);
void MdtSkeletalSetTwistDamping(MdtSkeletal* Joint, FLOAT Damping);

// the bodies and constraints of a world split into groups that move independently
struct MdtPartitionInfo
{
    INT Unknown0;
    INT Unknown4;
    INT Unknown8;
};

struct MdtPartitionOutput
{
    void**            Constraints;
    INT*              ConstraintsStart;
    INT*              ConstraintsSize;
    INT               TotalConstraints;
    INT               MaxConstraints;
    void**            Bodies;
    INT*              BodiesStart;
    INT*              BodiesSize;
    INT               TotalBodies;
    INT               MaxBodies;
    MdtPartitionInfo* Info;
    INT               NumPartitions;
    INT               MaxPartitions;
    // the info of every partition added up
    INT               Unknown34;
    INT               Unknown38;
    INT               Unknown3C;
};

MdtPartitionOutput* MdtPartitionOutputCreateInChunk(MeChunk* Chunk, INT MaxBodies, INT MaxConstraints);

// ---------------------------------------------------------------------------------------------- models

// a geometry placed in the world, with the box around it. an aggregate's parts get child instances.
struct McdGeometryInstance
{
    McdGeometry*         Geometry;
    FLOAT*               TM;
    FLOAT                Min[3];
    FLOAT                Max[3];
    INT                  Unknown20;
    McdGeometryInstance* Prev;
    McdGeometryInstance* Next;
    McdGeometryInstance* Parent;
    McdGeometryInstance* Child;
};

void McdGeometryInstanceReset(McdGeometryInstance* Instance);
void McdGeometryInstanceSetGeometry(McdGeometryInstance* Instance, McdGeometry* Geometry);
void McdGeometryInstanceSetTransformPtr(McdGeometryInstance* Instance, FLOAT* TM);
void McdGeometryInstanceUpdateAABB(McdGeometryInstance* Instance, void* Unknown1, void* Unknown2);
void McdGeometryInstanceGetAABB(McdGeometryInstance* Instance, FLOAT* Min, FLOAT* Max);

// something that collides
struct McdModel
{
    McdFramework*       Framework;
    McdModel*           Prev;
    McdModel*           Next;
    // the collision space the model is in, if any
    void*               Space;
    // pointers to the velocities of the body the model follows
    FLOAT*              LinearVelocity;
    FLOAT*              AngularVelocity;
    FLOAT               Padding;
    // the model's slot in its space
    INT                 SpaceIndex;
    _WORD               Unknown20;
    // the dynamics body the model follows
    MdtBody*            Body;
    INT                 Unknown28;
    void                (*Unknown2C)(McdModel* Model);
    // when set, the instance's transform is RelTM followed by BodyTM
    FLOAT*              RelTM;
    FLOAT*              BodyTM;
    INT                 Unknown38;
    // bit 0: the model owns RelTM and the instance's transform
    DWORD               Flags;
    McdGeometryInstance Instance;
    void*               UserData;
};

McdModel* McdModelCreate(McdGeometry* Geometry);
void      McdModelSetTransformPtr(McdModel* Model, FLOAT* TM);
void      McdModelSetUserData(McdModel* Model, void* UserData);
void      McdModelUpdate(McdModel* Model);
FLOAT*       McdModelGetTransformPtr(McdModel* Model);
McdGeometry* McdModelGetGeometry(McdModel* Model);
MdtBody*     McdModelGetBody(McdModel* Model);
void         McdModelSetBody(McdModel* Model, MdtBody* Body);
void         McdModelDestroy(McdModel* Model);
void*        McdModelGetSpace(McdModel* Model);
FLOAT*       McdModelGetLinearVelocityPtr(McdModel* Model);
void         McdModelSetLinearVelocityPtr(McdModel* Model, FLOAT* V);
FLOAT*       McdModelGetAngularVelocityPtr(McdModel* Model);
void         McdModelSetAngularVelocityPtr(McdModel* Model, FLOAT* V);
void      McdModelGetAABB(McdModel* Model, FLOAT* Min, FLOAT* Max);

void McdGeometryIncrementReferenceCount(McdGeometry* Geometry);
void McdGeometryDecrementReferenceCount(McdGeometry* Geometry);

// geometry made of others, each with its own transform
struct McdAggregateElement
{
    FLOAT        TM[16];
    McdGeometry* Geometry;
};

struct McdAggregate
{
    McdGeometry          Base;
    // ElementCount slots, UsedCount of them with a geometry
    McdAggregateElement* Elements;
    INT                  UsedCount;
    INT                  ElementCount;
};

McdAggregate* McdAggregateCreate(McdFramework* Framework, INT MaxElements);
INT           McdAggregateAddElement(McdAggregate* Aggregate, McdGeometry* Geometry, const FLOAT* TM);
void          McdAggregateRemoveElement(McdAggregate* Aggregate, INT Index);
INT           McdAggregateGetElementCount(McdAggregate* Aggregate);
McdGeometry*  McdAggregateGetElementGeometry(McdAggregate* Aggregate, INT Index);

struct McdSphere
{
    McdGeometry Base;
    FLOAT       Radius;
};

struct McdBox
{
    McdGeometry Base;
    FLOAT       Radii[3];
    // of the sphere around the box
    FLOAT       BoundRadius;
};

struct McdCylinder
{
    McdGeometry Base;
    FLOAT       Radius;
    FLOAT       HalfHeight;
    FLOAT       BoundRadius;
};

// a capsule: a cylinder with half spheres on its ends
struct McdSphyl
{
    McdGeometry Base;
    FLOAT       Radius;
    FLOAT       HalfHeight;
};

McdSphere*   McdSphereCreate(McdFramework* Framework, FLOAT Radius);
McdBox*      McdBoxCreate(McdFramework* Framework, FLOAT X, FLOAT Y, FLOAT Z);
McdCylinder* McdCylinderCreate(McdFramework* Framework, FLOAT Radius, FLOAT Height);
McdSphyl*    McdSphylCreate(McdFramework* Framework, FLOAT Radius, FLOAT Height);
McdGeometry* McdPlaneCreate(McdFramework* Framework);
INT          McdGeometryGetReferenceCount(McdGeometry* Geometry);
void         McdGeometryDestroy(McdGeometry* Geometry);

// triangles a callback hands out for whatever a model's box touches
typedef INT (*McdTriangleListGenerator)(void* Pair, void* Triangles, FLOAT* Position, FLOAT Radius, INT MaxTriangles);

struct McdTriangleList
{
    McdGeometry              Base;
    FLOAT                    Center[3];
    FLOAT                    Extent[3];
    INT                      MaxTriangles;
    void*                    UserData;
    McdTriangleListGenerator Generator;
    void*                    Unknown34;
};

McdTriangleList* McdTriangleListCreate(McdFramework* Framework, const FLOAT* Min, const FLOAT* Max, INT MaxTriangles,
    McdTriangleListGenerator Generator);
void McdTriangleListSetUserData(McdTriangleList* List, void* UserData);

// ---------------------------------------------------------------------------------------------- bridge

// buffers the bridge collides with (name invented)
struct MstBridgeBuffer
{
    void*  Data;
    INT    Unknown4;
    INT    Max;
};

struct MstBridgeBuffers
{
    INT              Unknown0;
    // a slot for each pair of geometry types
    INT*             PairTable;
    McdFramework*    Framework;
    INT              TypeCount;
    void*            Unknown10;
    void*            Unknown14;
    INT              Unknown18;
    INT              Unknown1C;
    void*            Unknown20;
    INT              Unknown24;
    INT              Unknown28;
    void*            Unknown2C;
    INT              Unknown30;
    INT              Unknown34;
    MstBridgeBuffer* Unknown38;
    INT              Unknown3C;
    INT              Unknown40;
    INT              Unknown44[3];
};

// what happens between two materials. the callback names follow Karma's bridge but their order is a guess.
struct MstMaterialPair
{
    MdtContactParams Params;
    INT              (*IntersectCB)(void* Unknown1, void* Unknown2, void* Unknown3);
    INT              (*PerContactCB)(void* Unknown1, void* Unknown2);
    void*            PerPairCB;
};

// turns collisions into contacts for the dynamics
struct MstBridge
{
    INT               MaxMaterials;
    INT               MaterialCount;
    // one for each pair of materials
    MstMaterialPair*  Pairs;
    MeUnknown1C*      Unknown0C;
    MstBridgeBuffers* Buffers;
    void*             Unknown14;
    INT               Unknown18;
};

MstBridge*        MstBridgeCreate(McdFramework* Framework, INT MaxMaterials);
void              MstBridgeSetUnknown0C(MstBridge* Bridge, INT Max);
MdtContactParams* MstBridgeGetContactParams(MstBridge* Bridge, DWORD Material1, DWORD Material2);
void              MstBridgeSetIntersectCB(MstBridge* Bridge, DWORD Material1, DWORD Material2,
    INT (*Fn)(void* Unknown1, void* Unknown2, void* Unknown3));
void              MstBridgeSetPerContactCB(MstBridge* Bridge, DWORD Material1, DWORD Material2, INT (*Fn)(void* Unknown1, void* Unknown2));

// ---------------------------------------------------------------------------------------------- asset instances

struct MeAssetInstance;

// how an asset instance makes its parts. each post callback gets its context.
struct MeAssetInstanceFactory
{
    MeAssetFactory* Factory;
    void            (*GeometryPostCreate)(McdGeometry* Geometry, MeFGeometry* Source, void* Context);
    void*           GeometryContext;
    McdModel*       (*CreateModel)(MeFPart* Part, McdGeometry* Geometry, MdtWorld* World, const FLOAT* TM);
    void            (*ModelPostCreate)(McdModel* Model, MeFPart* Part, void* Context);
    void*           ModelContext;
    MdtConstraint*  (*CreateJoint)(MeFJoint* Joint, MdtWorld* World, McdModel* Model1, McdModel* Model2, const FLOAT* TM);
    void            (*JointPostCreate)(MdtConstraint* Constraint, MeFJoint* Joint, void* Context);
    void*           JointContext;
};

// the collision and dynamics objects made from one asset, by name
struct MeAssetInstance
{
    MeAssetInstanceFactory* Factory;
    MdtWorld*               World;
    void*                   Unknown8;
    // never set by the game's creator; only read when the instance owns its parts
    void*                   UnknownC;
    MeFAsset*               Asset;
    MeHash*                 Geometries;
    MeHash*                 Models;
    MeHash*                 Joints;
    // destroying the instance destroys its parts too
    UBOOL                   OwnsParts;
};

void           MeAssetInstanceDestroy(MeAssetInstance* Instance);
McdModel*      MeAssetInstanceGetModel(MeAssetInstance* Instance, const char* Name);
MdtConstraint* MeAssetInstanceGetJoint(MeAssetInstance* Instance, const char* Name);
void           MeAssetInstanceGetGeometryIterator(MeAssetInstance* Instance, MeHashIterator* It);

MdtConstraint* MdtConstraintCreateFromMeFJoint(MeFJoint* Joint, MdtWorld* World, McdModel* Model1, McdModel* Model2, const FLOAT* TM);
