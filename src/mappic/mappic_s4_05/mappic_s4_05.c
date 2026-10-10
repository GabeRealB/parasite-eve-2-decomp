#include "mappic/mappic.h"

#include <psyq/libgpu.h>

#include "common.h"

/// Identity transform and self-parented root for the Dumping Hole shape.
static TmdBone _gMappicS405DumpingHoleShapeSkeleton[] = {
#include "assets/mappic_s2_02_model_00910_000C0_skeleton.inc"
};

/// Vertex count of the Dumping Hole shape's sole part (eight vertices).
static u32 _gMappicS405DumpingHoleShapePartVerts[] = {
#include "assets/mappic_s2_02_model_00910_000C0_partVerts.inc"
};

/// Dumping Hole's map footprint in integer model X/Z coordinates, with Y zero.
static SVECTOR _gMappicS405DumpingHoleShapeVerts[] = {
#include "assets/mappic_s2_02_model_00910_000C0_verts.inc"
};

/// Two unlit triangles and two quads with packed vertex byte offsets and white colour.
///
/// The first group contains the mesh; an empty group and `TMD_STREAM_END` follow.
static u32 _gMappicS405DumpingHoleShapeStream[] = {
#include "assets/mappic_s2_02_model_00910_000C0_stream.inc"
};

TmdSource gMappicS405DumpingHoleShape = {
    .bufferHalfBytes  = 2 * sizeof(POLY_F3) + 2 * sizeof(POLY_F4),
    .partCount        = ARRAY_SIZE(_gMappicS405DumpingHoleShapeSkeleton),
    .partVertexCounts = _gMappicS405DumpingHoleShapePartVerts,
    .verts            = _gMappicS405DumpingHoleShapeVerts,
    .normals          = &_gMappicS405DumpingHoleShapeVerts[ARRAY_SIZE(_gMappicS405DumpingHoleShapeVerts)], // Empty normal pool at the vertex end.
    .skeleton         = _gMappicS405DumpingHoleShapeSkeleton,
    .stream           = _gMappicS405DumpingHoleShapeStream,
};

/// Identity transform and self-parented root for the Garbage Incinerator shape.
static TmdBone _gMappicS405GarbageIncineratorShapeSkeleton[] = {
#include "assets/mappic_s4_05_model_0018C_skeleton.inc"
};

/// Vertex count of the Garbage Incinerator shape's sole part (sixteen vertices).
static u32 _gMappicS405GarbageIncineratorShapePartVerts[] = {
#include "assets/mappic_s4_05_model_0018C_partVerts.inc"
};

/// Garbage Incinerator's map footprint in integer model X/Z coordinates, with Y zero.
static SVECTOR _gMappicS405GarbageIncineratorShapeVerts[] = {
#include "assets/mappic_s4_05_model_0018C_verts.inc"
};

/// Four unlit triangles and five quads with packed vertex byte offsets and white colour.
///
/// The first group contains the mesh; an empty group and `TMD_STREAM_END` follow.
static u32 _gMappicS405GarbageIncineratorShapeStream[] = {
#include "assets/mappic_s4_05_model_0018C_stream.inc"
};

TmdSource gMappicS405GarbageIncineratorShape = {
    .bufferHalfBytes  = 4 * sizeof(POLY_F3) + 5 * sizeof(POLY_F4),
    .partCount        = ARRAY_SIZE(_gMappicS405GarbageIncineratorShapeSkeleton),
    .partVertexCounts = _gMappicS405GarbageIncineratorShapePartVerts,
    .verts            = _gMappicS405GarbageIncineratorShapeVerts,
    .normals          = &_gMappicS405GarbageIncineratorShapeVerts[ARRAY_SIZE(_gMappicS405GarbageIncineratorShapeVerts)], // Empty normal pool at the vertex end.
    .skeleton         = _gMappicS405GarbageIncineratorShapeSkeleton,
    .stream           = _gMappicS405GarbageIncineratorShapeStream,
};

/// Identity transform and self-parented root for the Incinerator Control Room shape.
static TmdBone _gMappicS405IncineratorControlRoomShapeSkeleton[] = {
#include "assets/mappic_s4_00_model_0005C_002C8_skeleton.inc"
};

/// Vertex count of the Incinerator Control Room shape's sole part (six vertices).
static u32 _gMappicS405IncineratorControlRoomShapePartVerts[] = {
#include "assets/mappic_s4_00_model_0005C_002C8_partVerts.inc"
};

/// Incinerator Control Room's map footprint in integer model X/Z coordinates, with Y zero.
static SVECTOR _gMappicS405IncineratorControlRoomShapeVerts[] = {
#include "assets/mappic_s4_00_model_0005C_002C8_verts.inc"
};

/// Two unlit quads with packed vertex byte offsets and white colour.
///
/// The first group contains the mesh; an empty group and `TMD_STREAM_END` follow.
static u32 _gMappicS405IncineratorControlRoomShapeStream[] = {
#include "assets/mappic_s4_00_model_0005C_002C8_stream.inc"
};

TmdSource gMappicS405IncineratorControlRoomShape = {
    .bufferHalfBytes  = 2 * sizeof(POLY_F4),
    .partCount        = ARRAY_SIZE(_gMappicS405IncineratorControlRoomShapeSkeleton),
    .partVertexCounts = _gMappicS405IncineratorControlRoomShapePartVerts,
    .verts            = _gMappicS405IncineratorControlRoomShapeVerts,
    .normals          = &_gMappicS405IncineratorControlRoomShapeVerts[ARRAY_SIZE(_gMappicS405IncineratorControlRoomShapeVerts)], // Empty normal pool at the vertex end.
    .skeleton         = _gMappicS405IncineratorControlRoomShapeSkeleton,
    .stream           = _gMappicS405IncineratorControlRoomShapeStream,
};

/// Identity transform and self-parented root for the B3 Elevator Hall shape.
static TmdBone _gMappicS405ElevatorHallShapeSkeleton[] = {
#include "assets/mappic_s4_05_model_00384_skeleton.inc"
};

/// Vertex count of the B3 Elevator Hall shape's sole part (fourteen vertices).
static u32 _gMappicS405ElevatorHallShapePartVerts[] = {
#include "assets/mappic_s4_05_model_00384_partVerts.inc"
};

/// B3 Elevator Hall's map footprint in integer model X/Z coordinates, with Y zero.
static SVECTOR _gMappicS405ElevatorHallShapeVerts[] = {
#include "assets/mappic_s4_05_model_00384_verts.inc"
};

/// Two unlit triangles and five quads with packed vertex byte offsets and white colour.
///
/// The first group contains the mesh; an empty group and `TMD_STREAM_END` follow.
static u32 _gMappicS405ElevatorHallShapeStream[] = {
#include "assets/mappic_s4_05_model_00384_stream.inc"
};

TmdSource gMappicS405ElevatorHallShape = {
    .bufferHalfBytes  = 2 * sizeof(POLY_F3) + 5 * sizeof(POLY_F4),
    .partCount        = ARRAY_SIZE(_gMappicS405ElevatorHallShapeSkeleton),
    .partVertexCounts = _gMappicS405ElevatorHallShapePartVerts,
    .verts            = _gMappicS405ElevatorHallShapeVerts,
    .normals          = &_gMappicS405ElevatorHallShapeVerts[ARRAY_SIZE(_gMappicS405ElevatorHallShapeVerts)], // Empty normal pool at the vertex end.
    .skeleton         = _gMappicS405ElevatorHallShapeSkeleton,
    .stream           = _gMappicS405ElevatorHallShapeStream,
};
