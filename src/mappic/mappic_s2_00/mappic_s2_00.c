#include "mappic/mappic.h"

#include <psyq/libgpu.h>

#include "common.h"

/// Identity transform and self-parented root for the Motel room 6 shape.
static TmdBone _gMappicS200MotelRoom6ShapeSkeleton[] = {
#include "assets/mappic_s2_02_model_00C48_skeleton.inc"
};

/// Vertex count of the Motel room 6 shape's sole part (four vertices).
static u32 _gMappicS200MotelRoom6ShapePartVerts[] = {
#include "assets/mappic_s2_02_model_00C48_partVerts.inc"
};

/// Motel room 6's map outline in integer model X/Z coordinates, with Y zero.
static SVECTOR _gMappicS200MotelRoom6ShapeVerts[] = {
#include "assets/mappic_s2_02_model_00C48_verts.inc"
};

/// One unlit quad's packed vertex offsets and colour, followed by two group ends and the stream end.
static u32 _gMappicS200MotelRoom6ShapeStream[] = {
#include "assets/mappic_s2_02_model_00C48_stream.inc"
};

TmdSource gMappicS200MotelRoom6Shape = {
    .bufferHalfBytes  = sizeof(POLY_F4),
    .partCount        = ARRAY_SIZE(_gMappicS200MotelRoom6ShapeSkeleton),
    .partVertexCounts = _gMappicS200MotelRoom6ShapePartVerts,
    .verts            = _gMappicS200MotelRoom6ShapeVerts,
    .normals          = &_gMappicS200MotelRoom6ShapeVerts[ARRAY_SIZE(_gMappicS200MotelRoom6ShapeVerts)], // Empty normal pool at the vertex end.
    .skeleton         = _gMappicS200MotelRoom6ShapeSkeleton,
    .stream           = _gMappicS200MotelRoom6ShapeStream,
};

/// Identity transform and self-parented root for the motel balcony shape.
static TmdBone _gMappicS200MotelBalconyShapeSkeleton[] = {
#include "assets/mappic_s2_00_model_0011C_skeleton.inc"
};

/// Vertex count of the motel balcony shape's sole part (twelve vertices).
static u32 _gMappicS200MotelBalconyShapePartVerts[] = {
#include "assets/mappic_s2_00_model_0011C_partVerts.inc"
};

/// Motel balcony fill vertices in integer model X/Z coordinates, with Y zero.
static SVECTOR _gMappicS200MotelBalconyShapeVerts[] = {
#include "assets/mappic_s2_00_model_0011C_verts.inc"
};

/// Two unlit triangles and four unlit quads, followed by two group ends and the stream end.
///
/// Each face stores packed u16 vertex byte offsets and a colour word. The map
/// uses the geometry to draw textured fills and ignores the stored colours.
static u32 _gMappicS200MotelBalconyShapeStream[] = {
#include "assets/mappic_s2_00_model_0011C_stream.inc"
};

TmdSource gMappicS200MotelBalconyShape = {
    .bufferHalfBytes  = 2 * sizeof(POLY_F3) + 4 * sizeof(POLY_F4),
    .partCount        = ARRAY_SIZE(_gMappicS200MotelBalconyShapeSkeleton),
    .partVertexCounts = _gMappicS200MotelBalconyShapePartVerts,
    .verts            = _gMappicS200MotelBalconyShapeVerts,
    .normals          = &_gMappicS200MotelBalconyShapeVerts[ARRAY_SIZE(_gMappicS200MotelBalconyShapeVerts)], // Empty normal pool at the vertex end.
    .skeleton         = _gMappicS200MotelBalconyShapeSkeleton,
    .stream           = _gMappicS200MotelBalconyShapeStream,
};

/// Identity transform and self-parented root for the Motel loft shape.
static TmdBone _gMappicS200MotelLoftShapeSkeleton[] = {
#include "assets/mappic_s2_02_model_00C48_00218_skeleton.inc"
};

/// Vertex count of the Motel loft shape's sole part (four vertices).
static u32 _gMappicS200MotelLoftShapePartVerts[] = {
#include "assets/mappic_s2_02_model_00C48_00218_partVerts.inc"
};

/// Motel loft fill vertices in integer model X/Z coordinates, with Y zero.
static SVECTOR _gMappicS200MotelLoftShapeVerts[] = {
#include "assets/mappic_s2_02_model_00C48_00218_verts.inc"
};

/// One unlit quad, followed by two group ends and the stream end.
///
/// The face stores packed u16 vertex byte offsets and a colour word. The map
/// uses the geometry to draw a textured fill and ignores the stored colour.
static u32 _gMappicS200MotelLoftShapeStream[] = {
#include "assets/mappic_s2_02_model_00C48_00218_stream.inc"
};

TmdSource gMappicS200MotelLoftShape = {
    .bufferHalfBytes  = sizeof(POLY_F4),
    .partCount        = ARRAY_SIZE(_gMappicS200MotelLoftShapeSkeleton),
    .partVertexCounts = _gMappicS200MotelLoftShapePartVerts,
    .verts            = _gMappicS200MotelLoftShapeVerts,
    .normals          = &_gMappicS200MotelLoftShapeVerts[ARRAY_SIZE(_gMappicS200MotelLoftShapeVerts)], // Empty normal pool at the vertex end.
    .skeleton         = _gMappicS200MotelLoftShapeSkeleton,
    .stream           = _gMappicS200MotelLoftShapeStream,
};

/// Identity transform and self-parented root for the Motel room 5 shape.
static TmdBone _gMappicS200MotelRoom5ShapeSkeleton[] = {
#include "assets/mappic_s2_02_model_00C48_002A8_skeleton.inc"
};

/// Vertex count of the Motel room 5 shape's sole part (four vertices).
static u32 _gMappicS200MotelRoom5ShapePartVerts[] = {
#include "assets/mappic_s2_02_model_00C48_002A8_partVerts.inc"
};

/// Motel room 5 fill vertices in integer model X/Z coordinates, with Y zero.
static SVECTOR _gMappicS200MotelRoom5ShapeVerts[] = {
#include "assets/mappic_s2_02_model_00C48_002A8_verts.inc"
};

/// One unlit quad, followed by two group ends and the stream end.
///
/// The face stores packed u16 vertex byte offsets and a colour word. The map
/// uses the geometry to draw a textured fill and ignores the stored colour.
static u32 _gMappicS200MotelRoom5ShapeStream[] = {
#include "assets/mappic_s2_02_model_00C48_002A8_stream.inc"
};

TmdSource gMappicS200MotelRoom5Shape = {
    .bufferHalfBytes  = sizeof(POLY_F4),
    .partCount        = ARRAY_SIZE(_gMappicS200MotelRoom5ShapeSkeleton),
    .partVertexCounts = _gMappicS200MotelRoom5ShapePartVerts,
    .verts            = _gMappicS200MotelRoom5ShapeVerts,
    .normals          = &_gMappicS200MotelRoom5ShapeVerts[ARRAY_SIZE(_gMappicS200MotelRoom5ShapeVerts)], // Empty normal pool at the vertex end.
    .skeleton         = _gMappicS200MotelRoom5ShapeSkeleton,
    .stream           = _gMappicS200MotelRoom5ShapeStream,
};

/// Identity transform and self-parented root for the Water tank area shape.
static TmdBone _gMappicS200WaterTankShapeSkeleton[] = {
#include "assets/mappic_s2_00_model_00344_skeleton.inc"
};

/// Vertex count of the Water tank shape's sole part (ten vertices).
static u32 _gMappicS200WaterTankShapePartVerts[] = {
#include "assets/mappic_s2_00_model_00344_partVerts.inc"
};

/// Water tank fill vertices in integer model X/Z coordinates, with Y zero.
static SVECTOR _gMappicS200WaterTankShapeVerts[] = {
#include "assets/mappic_s2_00_model_00344_verts.inc"
};

/// Four unlit quads, followed by two group ends and the stream end.
///
/// Each face stores packed u16 vertex byte offsets and a colour word. The map
/// uses the geometry to draw textured fills and ignores the stored colours.
static u32 _gMappicS200WaterTankShapeStream[] = {
#include "assets/mappic_s2_00_model_00344_stream.inc"
};

TmdSource gMappicS200WaterTankShape = {
    .bufferHalfBytes  = 4 * sizeof(POLY_F4),
    .partCount        = ARRAY_SIZE(_gMappicS200WaterTankShapeSkeleton),
    .partVertexCounts = _gMappicS200WaterTankShapePartVerts,
    .verts            = _gMappicS200WaterTankShapeVerts,
    .normals          = &_gMappicS200WaterTankShapeVerts[ARRAY_SIZE(_gMappicS200WaterTankShapeVerts)], // Empty normal pool at the vertex end.
    .skeleton         = _gMappicS200WaterTankShapeSkeleton,
    .stream           = _gMappicS200WaterTankShapeStream,
};
