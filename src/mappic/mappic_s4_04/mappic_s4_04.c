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

/// Identity transform and self-parented root for the B2 main corridor area shape.
static TmdBone _gMappicS404B2MainCorridorShapeSkeleton[] = {
#include "assets/mappic_s4_04_model_004B4_skeleton.inc"
};

/// Vertex count of the B2 main corridor shape's sole part (16 vertices).
static u32 _gMappicS404B2MainCorridorShapePartVerts[] = {
#include "assets/mappic_s4_04_model_004B4_partVerts.inc"
};

/// B2 main corridor fill vertices in integer model X/Z coordinates, with Y zero.
static SVECTOR _gMappicS404B2MainCorridorShapeVerts[] = {
#include "assets/mappic_s4_04_model_004B4_verts.inc"
};

/// Two unlit TMD triangles and six quads, followed by two group ends and the stream end.
///
/// Faces store packed u16 vertex byte offsets and a colour word. The map
/// draws textured fills from these faces and ignores their stored colours.
static u32 _gMappicS404B2MainCorridorShapeStream[] = {
#include "assets/mappic_s4_04_model_004B4_stream.inc"
};

TmdSource gMappicS404B2MainCorridorShape = {
    .bufferHalfBytes  = 2 * sizeof(POLY_F3) + 6 * sizeof(POLY_F4),
    .partCount        = ARRAY_SIZE(_gMappicS404B2MainCorridorShapeSkeleton),
    .partVertexCounts = _gMappicS404B2MainCorridorShapePartVerts,
    .verts            = _gMappicS404B2MainCorridorShapeVerts,
    .normals          = &_gMappicS404B2MainCorridorShapeVerts[ARRAY_SIZE(_gMappicS404B2MainCorridorShapeVerts)], // Empty normal pool at the vertex end.
    .skeleton         = _gMappicS404B2MainCorridorShapeSkeleton,
    .stream           = _gMappicS404B2MainCorridorShapeStream,
};

/// Identity transform and self-parented root for the B2 septic tank area shape.
static TmdBone _gMappicS404B2SepticTankShapeSkeleton[] = {
#include "assets/mappic_s2_02_model_00C48_005C8_skeleton.inc"
};

/// Vertex count of the B2 septic tank shape's sole part (4 vertices).
static u32 _gMappicS404B2SepticTankShapePartVerts[] = {
#include "assets/mappic_s2_02_model_00C48_005C8_partVerts.inc"
};

/// B2 septic tank fill vertices in integer model X/Z coordinates, with Y zero.
static SVECTOR _gMappicS404B2SepticTankShapeVerts[] = {
#include "assets/mappic_s2_02_model_00C48_005C8_verts.inc"
};

/// One unlit TMD quad, followed by two group ends and the stream end.
///
/// Faces store packed u16 vertex byte offsets and a colour word. The map
/// draws textured fills from these faces and ignores their stored colours.
static u32 _gMappicS404B2SepticTankShapeStream[] = {
#include "assets/mappic_s2_02_model_00C48_005C8_stream.inc"
};

TmdSource gMappicS404B2SepticTankShape = {
    .bufferHalfBytes  = sizeof(POLY_F4),
    .partCount        = ARRAY_SIZE(_gMappicS404B2SepticTankShapeSkeleton),
    .partVertexCounts = _gMappicS404B2SepticTankShapePartVerts,
    .verts            = _gMappicS404B2SepticTankShapeVerts,
    .normals          = &_gMappicS404B2SepticTankShapeVerts[ARRAY_SIZE(_gMappicS404B2SepticTankShapeVerts)], // Empty normal pool at the vertex end.
    .skeleton         = _gMappicS404B2SepticTankShapeSkeleton,
    .stream           = _gMappicS404B2SepticTankShapeStream,
};

/// Identity transform and self-parented root for the B2 pod access tunnel area shape.
static TmdBone _gMappicS404B2PodAccessTunnelShapeSkeleton[] = {
#include "assets/mappic_s4_04_model_00664_skeleton.inc"
};

/// Vertex count of the B2 pod access tunnel shape's sole part (10 vertices).
static u32 _gMappicS404B2PodAccessTunnelShapePartVerts[] = {
#include "assets/mappic_s4_04_model_00664_partVerts.inc"
};

/// B2 pod access tunnel fill vertices in integer model X/Z coordinates, with Y zero.
static SVECTOR _gMappicS404B2PodAccessTunnelShapeVerts[] = {
#include "assets/mappic_s4_04_model_00664_verts.inc"
};

/// Two unlit TMD triangles and three quads, followed by two group ends and the stream end.
///
/// Faces store packed u16 vertex byte offsets and a colour word. The map
/// draws textured fills from these faces and ignores their stored colours.
static u32 _gMappicS404B2PodAccessTunnelShapeStream[] = {
#include "assets/mappic_s4_04_model_00664_stream.inc"
};

TmdSource gMappicS404B2PodAccessTunnelShape = {
    .bufferHalfBytes  = 2 * sizeof(POLY_F3) + 3 * sizeof(POLY_F4),
    .partCount        = ARRAY_SIZE(_gMappicS404B2PodAccessTunnelShapeSkeleton),
    .partVertexCounts = _gMappicS404B2PodAccessTunnelShapePartVerts,
    .verts            = _gMappicS404B2PodAccessTunnelShapeVerts,
    .normals          = &_gMappicS404B2PodAccessTunnelShapeVerts[ARRAY_SIZE(_gMappicS404B2PodAccessTunnelShapeVerts)], // Empty normal pool at the vertex end.
    .skeleton         = _gMappicS404B2PodAccessTunnelShapeSkeleton,
    .stream           = _gMappicS404B2PodAccessTunnelShapeStream,
};

/// Identity transform and self-parented root for the B2 pod bottom area shape.
static TmdBone _gMappicS404B2PodBottomShapeSkeleton[] = {
#include "assets/mappic_s2_02_model_00CF8_0078C_skeleton.inc"
};

/// Vertex count of the B2 pod bottom shape's sole part (8 vertices).
static u32 _gMappicS404B2PodBottomShapePartVerts[] = {
#include "assets/mappic_s2_02_model_00CF8_0078C_partVerts.inc"
};

/// B2 pod bottom fill vertices in integer model X/Z coordinates, with Y zero.
static SVECTOR _gMappicS404B2PodBottomShapeVerts[] = {
#include "assets/mappic_s2_02_model_00CF8_0078C_verts.inc"
};

/// Three unlit TMD quads, followed by two group ends and the stream end.
///
/// Faces store packed u16 vertex byte offsets and a colour word. The map
/// draws textured fills from these faces and ignores their stored colours.
static u32 _gMappicS404B2PodBottomShapeStream[] = {
#include "assets/mappic_s2_02_model_00CF8_0078C_stream.inc"
};

TmdSource gMappicS404B2PodBottomShape = {
    .bufferHalfBytes  = 3 * sizeof(POLY_F4),
    .partCount        = ARRAY_SIZE(_gMappicS404B2PodBottomShapeSkeleton),
    .partVertexCounts = _gMappicS404B2PodBottomShapePartVerts,
    .verts            = _gMappicS404B2PodBottomShapeVerts,
    .normals          = &_gMappicS404B2PodBottomShapeVerts[ARRAY_SIZE(_gMappicS404B2PodBottomShapeVerts)], // Empty normal pool at the vertex end.
    .skeleton         = _gMappicS404B2PodBottomShapeSkeleton,
    .stream           = _gMappicS404B2PodBottomShapeStream,
};
