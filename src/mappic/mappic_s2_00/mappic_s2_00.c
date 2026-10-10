#include "mappic/mappic.h"

#include <psyq/libgpu.h>

#include "common.h"

/// Identity transform and self-parented root for the Motel room 6 shape.
static TmdBone _gMappicS200MotelRoom6ShapeSkeleton[] = {
#include "assets/mappic_s2_02_model_00C48_00070_mappic_s2_00_skeleton.inc"
};

/// Vertex count of the Motel room 6 shape's sole part (four vertices).
static u32 _gMappicS200MotelRoom6ShapePartVerts[] = {
#include "assets/mappic_s2_02_model_00C48_00070_mappic_s2_00_partVerts.inc"
};

/// Motel room 6's map outline in integer model X/Z coordinates, with Y zero.
static SVECTOR _gMappicS200MotelRoom6ShapeVerts[] = {
#include "assets/mappic_s2_02_model_00C48_00070_mappic_s2_00_verts.inc"
};

/// One unlit quad's packed vertex offsets and colour, followed by two group ends and the stream end.
static u32 _gMappicS200MotelRoom6ShapeStream[] = {
#include "assets/mappic_s2_02_model_00C48_00070_mappic_s2_00_stream.inc"
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

static TmdBone _gMappicS200MappicS202Model00C48002A8Skeleton[1] = {
#include "assets/mappic_s2_02_model_00C48_002A8_skeleton.inc"
};

static u32 _gMappicS200MappicS202Model00C48002A8PartVerts[1] = {
#include "assets/mappic_s2_02_model_00C48_002A8_partVerts.inc"
};

static SVECTOR _gMappicS200MappicS202Model00C48002A8Verts[4] = {
#include "assets/mappic_s2_02_model_00C48_002A8_verts.inc"
};

static u32 _gMappicS200MappicS202Model00C48002A8Stream[9] = {
#include "assets/mappic_s2_02_model_00C48_002A8_stream.inc"
};

TmdSource D_mappic_s2_00_8012F1D8 = {
    0,
    24,
    0,
    1,
    _gMappicS200MappicS202Model00C48002A8PartVerts,
    _gMappicS200MappicS202Model00C48002A8Verts,
    &_gMappicS200MappicS202Model00C48002A8Verts[4],
    _gMappicS200MappicS202Model00C48002A8Skeleton,
    _gMappicS200MappicS202Model00C48002A8Stream,
};

static TmdBone _gMappicS200Model00344Skeleton[1] = {
#include "assets/mappic_s2_00_model_00344_skeleton.inc"
};

static u32 _gMappicS200Model00344PartVerts[1] = {
#include "assets/mappic_s2_00_model_00344_partVerts.inc"
};

static SVECTOR _gMappicS200Model00344Verts[10] = {
#include "assets/mappic_s2_00_model_00344_verts.inc"
};

static u32 _gMappicS200Model00344Stream[18] = {
#include "assets/mappic_s2_00_model_00344_stream.inc"
};

TmdSource D_mappic_s2_00_8012F2BC = {
    0,
    96,
    0,
    1,
    _gMappicS200Model00344PartVerts,
    _gMappicS200Model00344Verts,
    &_gMappicS200Model00344Verts[10],
    _gMappicS200Model00344Skeleton,
    _gMappicS200Model00344Stream,
};
