#include "mappic/mappic.h"

#include <psyq/libgpu.h>

#include "common.h"

/// Identity transform and self-parented root for the Nursery area shape.
static TmdBone _gMappicS502NurseryShapeSkeleton[] = {
#include "assets/mappic_s5_02_model_0009C_skeleton.inc"
};

/// Vertex count of the Nursery shape's sole part (fourteen vertices).
static u32 _gMappicS502NurseryShapePartVerts[] = {
#include "assets/mappic_s5_02_model_0009C_partVerts.inc"
};

/// Nursery fill vertices in integer model X/Z coordinates, with Y zero.
static SVECTOR _gMappicS502NurseryShapeVerts[] = {
#include "assets/mappic_s5_02_model_0009C_verts.inc"
};

/// Two unlit triangles and five unlit quads, followed by two group ends and the stream end.
///
/// Each face stores packed u16 vertex byte offsets and a colour word. The map
/// uses the geometry to draw textured fills and ignores the stored colours.
static u32 _gMappicS502NurseryShapeStream[] = {
#include "assets/mappic_s5_02_model_0009C_stream.inc"
};

TmdSource gMappicS502NurseryShape = {
    .bufferHalfBytes  = 2 * sizeof(POLY_F3) + 5 * sizeof(POLY_F4),
    .partCount        = ARRAY_SIZE(_gMappicS502NurseryShapeSkeleton),
    .partVertexCounts = _gMappicS502NurseryShapePartVerts,
    .verts            = _gMappicS502NurseryShapeVerts,
    .normals          = &_gMappicS502NurseryShapeVerts[ARRAY_SIZE(_gMappicS502NurseryShapeVerts)], // Empty normal pool at the vertex end.
    .skeleton         = _gMappicS502NurseryShapeSkeleton,
    .stream           = _gMappicS502NurseryShapeStream,
};

/// Identity transform and self-parented root for the Growth room area shape.
static TmdBone _gMappicS502GrowthRoomShapeSkeleton[] = {
#include "assets/mappic_s4_06_model_00214_001F4_skeleton.inc"
};

/// Vertex count of the Growth room shape's sole part (eight vertices).
static u32 _gMappicS502GrowthRoomShapePartVerts[] = {
#include "assets/mappic_s4_06_model_00214_001F4_partVerts.inc"
};

/// Growth room fill vertices in integer model X/Z coordinates, with Y zero.
static SVECTOR _gMappicS502GrowthRoomShapeVerts[] = {
#include "assets/mappic_s4_06_model_00214_001F4_verts.inc"
};

/// Two unlit triangles and two unlit quads, followed by two group ends and the stream end.
///
/// Each face stores packed u16 vertex byte offsets and a colour word. The map
/// uses the geometry to draw textured fills and ignores the stored colours.
static u32 _gMappicS502GrowthRoomShapeStream[] = {
#include "assets/mappic_s4_06_model_00214_001F4_stream.inc"
};

TmdSource gMappicS502GrowthRoomShape = {
    .bufferHalfBytes  = 2 * sizeof(POLY_F3) + 2 * sizeof(POLY_F4),
    .partCount        = ARRAY_SIZE(_gMappicS502GrowthRoomShapeSkeleton),
    .partVertexCounts = _gMappicS502GrowthRoomShapePartVerts,
    .verts            = _gMappicS502GrowthRoomShapeVerts,
    .normals          = &_gMappicS502GrowthRoomShapeVerts[ARRAY_SIZE(_gMappicS502GrowthRoomShapeVerts)], // Empty normal pool at the vertex end.
    .skeleton         = _gMappicS502GrowthRoomShapeSkeleton,
    .stream           = _gMappicS502GrowthRoomShapeStream,
};

/// Identity transform and self-parented root for the shared EVE elevator/Corridor shape.
static TmdBone _gMappicS502EveElevatorCorridorShapeSkeleton[] = {
#include "assets/mappic_s5_02_model_002A0_skeleton.inc"
};

/// Vertex count of the shared EVE elevator/Corridor shape's sole part (twelve vertices).
static u32 _gMappicS502EveElevatorCorridorShapePartVerts[] = {
#include "assets/mappic_s5_02_model_002A0_partVerts.inc"
};

/// Shared EVE elevator/Corridor fill vertices in integer model X/Z coordinates, with Y zero.
static SVECTOR _gMappicS502EveElevatorCorridorShapeVerts[] = {
#include "assets/mappic_s5_02_model_002A0_verts.inc"
};

/// Five unlit quads, followed by two group ends and the stream end.
///
/// Each face stores packed u16 vertex byte offsets and a colour word. The map
/// uses the geometry to draw textured fills and ignores the stored colours.
static u32 _gMappicS502EveElevatorCorridorShapeStream[] = {
#include "assets/mappic_s5_02_model_002A0_stream.inc"
};

TmdSource gMappicS502EveElevatorCorridorShape = {
    .bufferHalfBytes  = 5 * sizeof(POLY_F4),
    .partCount        = ARRAY_SIZE(_gMappicS502EveElevatorCorridorShapeSkeleton),
    .partVertexCounts = _gMappicS502EveElevatorCorridorShapePartVerts,
    .verts            = _gMappicS502EveElevatorCorridorShapeVerts,
    .normals          = &_gMappicS502EveElevatorCorridorShapeVerts[ARRAY_SIZE(_gMappicS502EveElevatorCorridorShapeVerts)], // Empty normal pool at the vertex end.
    .skeleton         = _gMappicS502EveElevatorCorridorShapeSkeleton,
    .stream           = _gMappicS502EveElevatorCorridorShapeStream,
};

/// Identity transform and self-parented root for the Training room area shape.
static TmdBone _gMappicS502TrainingRoomShapeSkeleton[] = {
#include "assets/mappic_s2_02_model_00C48_00384_skeleton.inc"
};

/// Vertex count of the Training room shape's sole part (four vertices).
static u32 _gMappicS502TrainingRoomShapePartVerts[] = {
#include "assets/mappic_s2_02_model_00C48_00384_partVerts.inc"
};

/// Training room fill vertices in integer model X/Z coordinates, with Y zero.
static SVECTOR _gMappicS502TrainingRoomShapeVerts[] = {
#include "assets/mappic_s2_02_model_00C48_00384_verts.inc"
};

/// One unlit quad, followed by two group ends and the stream end.
///
/// The face stores packed u16 vertex byte offsets and a colour word. The map
/// uses the geometry to draw a textured fill and ignores the stored colour.
static u32 _gMappicS502TrainingRoomShapeStream[] = {
#include "assets/mappic_s2_02_model_00C48_00384_stream.inc"
};

TmdSource gMappicS502TrainingRoomShape = {
    .bufferHalfBytes  = sizeof(POLY_F4),
    .partCount        = ARRAY_SIZE(_gMappicS502TrainingRoomShapeSkeleton),
    .partVertexCounts = _gMappicS502TrainingRoomShapePartVerts,
    .verts            = _gMappicS502TrainingRoomShapeVerts,
    .normals          = &_gMappicS502TrainingRoomShapeVerts[ARRAY_SIZE(_gMappicS502TrainingRoomShapeVerts)], // Empty normal pool at the vertex end.
    .skeleton         = _gMappicS502TrainingRoomShapeSkeleton,
    .stream           = _gMappicS502TrainingRoomShapeStream,
};
