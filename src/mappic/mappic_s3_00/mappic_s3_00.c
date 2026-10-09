#include "mappic/mappic.h"

#include <psyq/libgpu.h>

#include "common.h"

static TmdBone _gMappicS300MappicS200Model00344Skeleton[1] = {
#include "assets/mappic_s2_00_model_00344_skeleton.inc"
};

static u32 _gMappicS300MappicS200Model00344PartVerts[1] = {
#include "assets/mappic_s2_00_model_00344_partVerts.inc"
};

static SVECTOR _gMappicS300MappicS200Model00344Verts[10] = {
#include "assets/mappic_s2_00_model_00344_verts.inc"
};

static u32 _gMappicS300MappicS200Model00344Stream[18] = {
#include "assets/mappic_s2_00_model_00344_stream.inc"
};

TmdSource D_mappic_s3_00_8012EFF4 = {
    0,
    96,
    0,
    1,
    _gMappicS300MappicS200Model00344PartVerts,
    _gMappicS300MappicS200Model00344Verts,
    &_gMappicS300MappicS200Model00344Verts[10],
    _gMappicS300MappicS200Model00344Skeleton,
    _gMappicS300MappicS200Model00344Stream,
};

static TmdBone _gMappicS300MappicS202Model00C48002A8Skeleton[1] = {
#include "assets/mappic_s2_02_model_00C48_002A8_skeleton.inc"
};

static u32 _gMappicS300MappicS202Model00C48002A8PartVerts[1] = {
#include "assets/mappic_s2_02_model_00C48_002A8_partVerts.inc"
};

static SVECTOR _gMappicS300MappicS202Model00C48002A8Verts[4] = {
#include "assets/mappic_s2_02_model_00C48_002A8_verts.inc"
};

static u32 _gMappicS300MappicS202Model00C48002A8Stream[9] = {
#include "assets/mappic_s2_02_model_00C48_002A8_stream.inc"
};

TmdSource D_mappic_s3_00_8012F084 = {
    0,
    24,
    0,
    1,
    _gMappicS300MappicS202Model00C48002A8PartVerts,
    _gMappicS300MappicS202Model00C48002A8Verts,
    &_gMappicS300MappicS202Model00C48002A8Verts[4],
    _gMappicS300MappicS202Model00C48002A8Skeleton,
    _gMappicS300MappicS202Model00C48002A8Stream,
};

static TmdBone _gMappicS300MappicS200Model0011CSkeleton[1] = {
#include "assets/mappic_s2_00_model_0011C_skeleton.inc"
};

static u32 _gMappicS300MappicS200Model0011CPartVerts[1] = {
#include "assets/mappic_s2_00_model_0011C_partVerts.inc"
};

static SVECTOR _gMappicS300MappicS200Model0011CVerts[12] = {
#include "assets/mappic_s2_00_model_0011C_verts.inc"
};

static u32 _gMappicS300MappicS200Model0011CStream[27] = {
#include "assets/mappic_s2_00_model_0011C_stream.inc"
};

TmdSource D_mappic_s3_00_8012F19C = {
    0,
    136,
    0,
    1,
    _gMappicS300MappicS200Model0011CPartVerts,
    _gMappicS300MappicS200Model0011CVerts,
    &_gMappicS300MappicS200Model0011CVerts[12],
    _gMappicS300MappicS200Model0011CSkeleton,
    _gMappicS300MappicS200Model0011CStream,
};

/// Identity transform and self-parented root for the Motel room 6 shape.
static TmdBone _gMappicS300MotelRoom6ShapeSkeleton[] = {
#include "assets/mappic_s2_02_model_00C48_00070_mappic_s2_00_skeleton.inc"
};

/// Vertex count of the Motel room 6 shape's sole part (four vertices).
static u32 _gMappicS300MotelRoom6ShapePartVerts[] = {
#include "assets/mappic_s2_02_model_00C48_00070_mappic_s2_00_partVerts.inc"
};

/// Motel room 6's map outline in integer model X/Z coordinates, with Y zero.
static SVECTOR _gMappicS300MotelRoom6ShapeVerts[] = {
#include "assets/mappic_s2_02_model_00C48_00070_mappic_s2_00_verts.inc"
};

/// One unlit quad's packed vertex offsets and colour, followed by two group ends and the stream end.
static u32 _gMappicS300MotelRoom6ShapeStream[] = {
#include "assets/mappic_s2_02_model_00C48_00070_mappic_s2_00_stream.inc"
};

TmdSource gMappicS300MotelRoom6Shape = {
    .bufferHalfBytes  = sizeof(POLY_F4),
    .partCount        = ARRAY_SIZE(_gMappicS300MotelRoom6ShapeSkeleton),
    .partVertexCounts = _gMappicS300MotelRoom6ShapePartVerts,
    .verts            = _gMappicS300MotelRoom6ShapeVerts,
    .normals          = &_gMappicS300MotelRoom6ShapeVerts[ARRAY_SIZE(_gMappicS300MotelRoom6ShapeVerts)], // Empty normal pool at the vertex end.
    .skeleton         = _gMappicS300MotelRoom6ShapeSkeleton,
    .stream           = _gMappicS300MotelRoom6ShapeStream,
};

static TmdBone _gMappicS300MappicS202Model00C4800218Skeleton[1] = {
#include "assets/mappic_s2_02_model_00C48_00218_skeleton.inc"
};

static u32 _gMappicS300MappicS202Model00C4800218PartVerts[1] = {
#include "assets/mappic_s2_02_model_00C48_00218_partVerts.inc"
};

static SVECTOR _gMappicS300MappicS202Model00C4800218Verts[4] = {
#include "assets/mappic_s2_02_model_00C48_00218_verts.inc"
};

static u32 _gMappicS300MappicS202Model00C4800218Stream[9] = {
#include "assets/mappic_s2_02_model_00C48_00218_stream.inc"
};

TmdSource D_mappic_s3_00_8012F2BC = {
    0,
    24,
    0,
    1,
    _gMappicS300MappicS202Model00C4800218PartVerts,
    _gMappicS300MappicS202Model00C4800218Verts,
    &_gMappicS300MappicS202Model00C4800218Verts[4],
    _gMappicS300MappicS202Model00C4800218Skeleton,
    _gMappicS300MappicS202Model00C4800218Stream,
};
