#include "mappic/mappic.h"

#include <psyq/libgpu.h>

#include "common.h"

/// Identity transform and self-parented root for the B2 elevator hall area shape.
static TmdBone _gMappicS404B2ElevatorHallShapeSkeleton[] = {
#include "assets/mappic_s4_04_model_0006C_skeleton.inc"
};

/// Vertex count of the B2 elevator hall shape's sole part (8 vertices).
static u32 _gMappicS404B2ElevatorHallShapePartVerts[] = {
#include "assets/mappic_s4_04_model_0006C_partVerts.inc"
};

/// B2 elevator hall fill vertices in integer model X/Z coordinates, with Y zero.
static SVECTOR _gMappicS404B2ElevatorHallShapeVerts[] = {
#include "assets/mappic_s4_04_model_0006C_verts.inc"
};

/// Three unlit TMD quads, followed by two group ends and the stream end.
///
/// Faces store packed u16 vertex byte offsets and a colour word. The map
/// draws textured fills from these faces and ignores their stored colours.
static u32 _gMappicS404B2ElevatorHallShapeStream[] = {
#include "assets/mappic_s4_04_model_0006C_stream.inc"
};

TmdSource gMappicS404B2ElevatorHallShape = {
    .bufferHalfBytes  = 3 * sizeof(POLY_F4),
    .partCount        = ARRAY_SIZE(_gMappicS404B2ElevatorHallShapeSkeleton),
    .partVertexCounts = _gMappicS404B2ElevatorHallShapePartVerts,
    .verts            = _gMappicS404B2ElevatorHallShapeVerts,
    .normals          = &_gMappicS404B2ElevatorHallShapeVerts[ARRAY_SIZE(_gMappicS404B2ElevatorHallShapeVerts)], // Empty normal pool at the vertex end.
    .skeleton         = _gMappicS404B2ElevatorHallShapeSkeleton,
    .stream           = _gMappicS404B2ElevatorHallShapeStream,
};

/// Identity transform and self-parented root for the B2 south maintenance walkway area shape.
static TmdBone _gMappicS404B2SouthMaintenanceWalkwayShapeSkeleton[] = {
#include "assets/mappic_s4_00_model_0005C_00154_mappic_s4_04_skeleton.inc"
};

/// Vertex count of the B2 south maintenance walkway shape's sole part (6 vertices).
static u32 _gMappicS404B2SouthMaintenanceWalkwayShapePartVerts[] = {
#include "assets/mappic_s4_00_model_0005C_00154_mappic_s4_04_partVerts.inc"
};

/// B2 south maintenance walkway fill vertices in integer model X/Z coordinates, with Y zero.
static SVECTOR _gMappicS404B2SouthMaintenanceWalkwayShapeVerts[] = {
#include "assets/mappic_s4_00_model_0005C_00154_mappic_s4_04_verts.inc"
};

/// Two unlit TMD quads, followed by two group ends and the stream end.
///
/// Faces store packed u16 vertex byte offsets and a colour word. The map
/// draws textured fills from these faces and ignores their stored colours.
static u32 _gMappicS404B2SouthMaintenanceWalkwayShapeStream[] = {
#include "assets/mappic_s4_00_model_0005C_00154_mappic_s4_04_stream.inc"
};

TmdSource gMappicS404B2SouthMaintenanceWalkwayShape = {
    .bufferHalfBytes  = 2 * sizeof(POLY_F4),
    .partCount        = ARRAY_SIZE(_gMappicS404B2SouthMaintenanceWalkwayShapeSkeleton),
    .partVertexCounts = _gMappicS404B2SouthMaintenanceWalkwayShapePartVerts,
    .verts            = _gMappicS404B2SouthMaintenanceWalkwayShapeVerts,
    .normals          = &_gMappicS404B2SouthMaintenanceWalkwayShapeVerts[ARRAY_SIZE(_gMappicS404B2SouthMaintenanceWalkwayShapeVerts)], // Empty normal pool at the vertex end.
    .skeleton         = _gMappicS404B2SouthMaintenanceWalkwayShapeSkeleton,
    .stream           = _gMappicS404B2SouthMaintenanceWalkwayShapeStream,
};

/// Identity transform and self-parented root for the B2 operating room area shape.
static TmdBone _gMappicS404B2OperatingRoomShapeSkeleton[] = {
#include "assets/mappic_s2_02_model_00C48_001E4_mappic_s4_04_skeleton.inc"
};

/// Vertex count of the B2 operating room shape's sole part (4 vertices).
static u32 _gMappicS404B2OperatingRoomShapePartVerts[] = {
#include "assets/mappic_s2_02_model_00C48_001E4_mappic_s4_04_partVerts.inc"
};

/// B2 operating room fill vertices in integer model X/Z coordinates, with Y zero.
static SVECTOR _gMappicS404B2OperatingRoomShapeVerts[] = {
#include "assets/mappic_s2_02_model_00C48_001E4_mappic_s4_04_verts.inc"
};

/// One unlit TMD quad, followed by two group ends and the stream end.
///
/// Faces store packed u16 vertex byte offsets and a colour word. The map
/// draws textured fills from these faces and ignores their stored colours.
static u32 _gMappicS404B2OperatingRoomShapeStream[] = {
#include "assets/mappic_s2_02_model_00C48_001E4_mappic_s4_04_stream.inc"
};

TmdSource gMappicS404B2OperatingRoomShape = {
    .bufferHalfBytes  = sizeof(POLY_F4),
    .partCount        = ARRAY_SIZE(_gMappicS404B2OperatingRoomShapeSkeleton),
    .partVertexCounts = _gMappicS404B2OperatingRoomShapePartVerts,
    .verts            = _gMappicS404B2OperatingRoomShapeVerts,
    .normals          = &_gMappicS404B2OperatingRoomShapeVerts[ARRAY_SIZE(_gMappicS404B2OperatingRoomShapeVerts)], // Empty normal pool at the vertex end.
    .skeleton         = _gMappicS404B2OperatingRoomShapeSkeleton,
    .stream           = _gMappicS404B2OperatingRoomShapeStream,
};

/// Identity transform and self-parented root for the B2 north maintenance walkway area shape.
static TmdBone _gMappicS404B2NorthMaintenanceWalkwayShapeSkeleton[] = {
#include "assets/mappic_s4_00_model_0005C_00290_mappic_s4_04_skeleton.inc"
};

/// Vertex count of the B2 north maintenance walkway shape's sole part (6 vertices).
static u32 _gMappicS404B2NorthMaintenanceWalkwayShapePartVerts[] = {
#include "assets/mappic_s4_00_model_0005C_00290_mappic_s4_04_partVerts.inc"
};

/// B2 north maintenance walkway fill vertices in integer model X/Z coordinates, with Y zero.
static SVECTOR _gMappicS404B2NorthMaintenanceWalkwayShapeVerts[] = {
#include "assets/mappic_s4_00_model_0005C_00290_mappic_s4_04_verts.inc"
};

/// Two unlit TMD quads, followed by two group ends and the stream end.
///
/// Faces store packed u16 vertex byte offsets and a colour word. The map
/// draws textured fills from these faces and ignores their stored colours.
static u32 _gMappicS404B2NorthMaintenanceWalkwayShapeStream[] = {
#include "assets/mappic_s4_00_model_0005C_00290_mappic_s4_04_stream.inc"
};

TmdSource gMappicS404B2NorthMaintenanceWalkwayShape = {
    .bufferHalfBytes  = 2 * sizeof(POLY_F4),
    .partCount        = ARRAY_SIZE(_gMappicS404B2NorthMaintenanceWalkwayShapeSkeleton),
    .partVertexCounts = _gMappicS404B2NorthMaintenanceWalkwayShapePartVerts,
    .verts            = _gMappicS404B2NorthMaintenanceWalkwayShapeVerts,
    .normals          = &_gMappicS404B2NorthMaintenanceWalkwayShapeVerts[ARRAY_SIZE(_gMappicS404B2NorthMaintenanceWalkwayShapeVerts)], // Empty normal pool at the vertex end.
    .skeleton         = _gMappicS404B2NorthMaintenanceWalkwayShapeSkeleton,
    .stream           = _gMappicS404B2NorthMaintenanceWalkwayShapeStream,
};

/// Identity transform and self-parented root for the B2 laboratory area shape.
static TmdBone _gMappicS404B2LaboratoryShapeSkeleton[] = {
#include "assets/mappic_s4_00_model_0005C_0033C_mappic_s4_04_skeleton.inc"
};

/// Vertex count of the B2 laboratory shape's sole part (6 vertices).
static u32 _gMappicS404B2LaboratoryShapePartVerts[] = {
#include "assets/mappic_s4_00_model_0005C_0033C_mappic_s4_04_partVerts.inc"
};

/// B2 laboratory fill vertices in integer model X/Z coordinates, with Y zero.
static SVECTOR _gMappicS404B2LaboratoryShapeVerts[] = {
#include "assets/mappic_s4_00_model_0005C_0033C_mappic_s4_04_verts.inc"
};

/// Two unlit TMD quads, followed by two group ends and the stream end.
///
/// Faces store packed u16 vertex byte offsets and a colour word. The map
/// draws textured fills from these faces and ignores their stored colours.
static u32 _gMappicS404B2LaboratoryShapeStream[] = {
#include "assets/mappic_s4_00_model_0005C_0033C_mappic_s4_04_stream.inc"
};

TmdSource gMappicS404B2LaboratoryShape = {
    .bufferHalfBytes  = 2 * sizeof(POLY_F4),
    .partCount        = ARRAY_SIZE(_gMappicS404B2LaboratoryShapeSkeleton),
    .partVertexCounts = _gMappicS404B2LaboratoryShapePartVerts,
    .verts            = _gMappicS404B2LaboratoryShapeVerts,
    .normals          = &_gMappicS404B2LaboratoryShapeVerts[ARRAY_SIZE(_gMappicS404B2LaboratoryShapeVerts)], // Empty normal pool at the vertex end.
    .skeleton         = _gMappicS404B2LaboratoryShapeSkeleton,
    .stream           = _gMappicS404B2LaboratoryShapeStream,
};

/// Identity transform and self-parented root for the B2 breeding room area shape.
static TmdBone _gMappicS404B2BreedingRoomShapeSkeleton[] = {
#include "assets/mappic_s4_00_model_0005C_003E8_mappic_s4_04_skeleton.inc"
};

/// Vertex count of the B2 breeding room shape's sole part (6 vertices).
static u32 _gMappicS404B2BreedingRoomShapePartVerts[] = {
#include "assets/mappic_s4_00_model_0005C_003E8_mappic_s4_04_partVerts.inc"
};

/// B2 breeding room fill vertices in integer model X/Z coordinates, with Y zero.
static SVECTOR _gMappicS404B2BreedingRoomShapeVerts[] = {
#include "assets/mappic_s4_00_model_0005C_003E8_mappic_s4_04_verts.inc"
};

/// Two unlit TMD quads, followed by two group ends and the stream end.
///
/// Faces store packed u16 vertex byte offsets and a colour word. The map
/// draws textured fills from these faces and ignores their stored colours.
static u32 _gMappicS404B2BreedingRoomShapeStream[] = {
#include "assets/mappic_s4_00_model_0005C_003E8_mappic_s4_04_stream.inc"
};

TmdSource gMappicS404B2BreedingRoomShape = {
    .bufferHalfBytes  = 2 * sizeof(POLY_F4),
    .partCount        = ARRAY_SIZE(_gMappicS404B2BreedingRoomShapeSkeleton),
    .partVertexCounts = _gMappicS404B2BreedingRoomShapePartVerts,
    .verts            = _gMappicS404B2BreedingRoomShapeVerts,
    .normals          = &_gMappicS404B2BreedingRoomShapeVerts[ARRAY_SIZE(_gMappicS404B2BreedingRoomShapeVerts)], // Empty normal pool at the vertex end.
    .skeleton         = _gMappicS404B2BreedingRoomShapeSkeleton,
    .stream           = _gMappicS404B2BreedingRoomShapeStream,
};

static TmdBone _gMappicS404Model004B4Skeleton[1] = {
#include "assets/mappic_s4_04_model_004B4_skeleton.inc"
};

static u32 _gMappicS404Model004B4PartVerts[1] = {
#include "assets/mappic_s4_04_model_004B4_partVerts.inc"
};

static SVECTOR _gMappicS404Model004B4Verts[16] = {
#include "assets/mappic_s4_04_model_004B4_verts.inc"
};

static u32 _gMappicS404Model004B4Stream[33] = {
#include "assets/mappic_s4_04_model_004B4_stream.inc"
};

TmdSource D_mappic_s4_04_8012F468 = {
    0,
    184,
    0,
    1,
    _gMappicS404Model004B4PartVerts,
    _gMappicS404Model004B4Verts,
    &_gMappicS404Model004B4Verts[16],
    _gMappicS404Model004B4Skeleton,
    _gMappicS404Model004B4Stream,
};

static TmdBone _gMappicS404MappicS202Model00C48005C8Skeleton[1] = {
#include "assets/mappic_s2_02_model_00C48_005C8_skeleton.inc"
};

static u32 _gMappicS404MappicS202Model00C48005C8PartVerts[1] = {
#include "assets/mappic_s2_02_model_00C48_005C8_partVerts.inc"
};

static SVECTOR _gMappicS404MappicS202Model00C48005C8Verts[4] = {
#include "assets/mappic_s2_02_model_00C48_005C8_verts.inc"
};

static u32 _gMappicS404MappicS202Model00C48005C8Stream[9] = {
#include "assets/mappic_s2_02_model_00C48_005C8_stream.inc"
};

TmdSource D_mappic_s4_04_8012F4F8 = {
    0,
    24,
    0,
    1,
    _gMappicS404MappicS202Model00C48005C8PartVerts,
    _gMappicS404MappicS202Model00C48005C8Verts,
    &_gMappicS404MappicS202Model00C48005C8Verts[4],
    _gMappicS404MappicS202Model00C48005C8Skeleton,
    _gMappicS404MappicS202Model00C48005C8Stream,
};

static TmdBone _gMappicS404Model00664Skeleton[1] = {
#include "assets/mappic_s4_04_model_00664_skeleton.inc"
};

static u32 _gMappicS404Model00664PartVerts[1] = {
#include "assets/mappic_s4_04_model_00664_partVerts.inc"
};

static SVECTOR _gMappicS404Model00664Verts[10] = {
#include "assets/mappic_s4_04_model_00664_verts.inc"
};

static u32 _gMappicS404Model00664Stream[24] = {
#include "assets/mappic_s4_04_model_00664_stream.inc"
};

TmdSource D_mappic_s4_04_8012F5F4 = {
    0,
    112,
    0,
    1,
    _gMappicS404Model00664PartVerts,
    _gMappicS404Model00664Verts,
    &_gMappicS404Model00664Verts[10],
    _gMappicS404Model00664Skeleton,
    _gMappicS404Model00664Stream,
};

static TmdBone _gMappicS404MappicS202Model00CF80078CSkeleton[1] = {
#include "assets/mappic_s2_02_model_00CF8_0078C_skeleton.inc"
};

static u32 _gMappicS404MappicS202Model00CF80078CPartVerts[1] = {
#include "assets/mappic_s2_02_model_00CF8_0078C_partVerts.inc"
};

static SVECTOR _gMappicS404MappicS202Model00CF80078CVerts[8] = {
#include "assets/mappic_s2_02_model_00CF8_0078C_verts.inc"
};

static u32 _gMappicS404MappicS202Model00CF80078CStream[15] = {
#include "assets/mappic_s2_02_model_00CF8_0078C_stream.inc"
};

TmdSource D_mappic_s4_04_8012F6BC = {
    0,
    72,
    0,
    1,
    _gMappicS404MappicS202Model00CF80078CPartVerts,
    _gMappicS404MappicS202Model00CF80078CVerts,
    &_gMappicS404MappicS202Model00CF80078CVerts[8],
    _gMappicS404MappicS202Model00CF80078CSkeleton,
    _gMappicS404MappicS202Model00CF80078CStream,
};
