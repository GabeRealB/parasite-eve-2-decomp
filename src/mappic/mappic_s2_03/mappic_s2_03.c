#include "mappic/mappic.h"

#include <psyq/libgpu.h>

#include "common.h"

/// Identity transform and self-parented root for the Water hole shape.
static TmdBone _gMappicS203WaterHoleShapeSkeleton[] = {
#include "assets/mappic_s2_02_model_00CF8_000A8_skeleton.inc"
};

/// Vertex count of the Water hole shape's sole part (eight vertices).
static u32 _gMappicS203WaterHoleShapePartVerts[] = {
#include "assets/mappic_s2_02_model_00CF8_000A8_partVerts.inc"
};

/// Water hole's map outline in integer model X/Z coordinates, with Y zero.
static SVECTOR _gMappicS203WaterHoleShapeVerts[] = {
#include "assets/mappic_s2_02_model_00CF8_000A8_verts.inc"
};

/// Three unlit quads' packed vertex byte offsets and colours, then two group ends and the stream end.
static u32 _gMappicS203WaterHoleShapeStream[] = {
#include "assets/mappic_s2_02_model_00CF8_000A8_stream.inc"
};

TmdSource gMappicS203WaterHoleShape = {
    .bufferHalfBytes  = 3 * sizeof(POLY_F4),
    .partCount        = ARRAY_SIZE(_gMappicS203WaterHoleShapeSkeleton),
    .partVertexCounts = _gMappicS203WaterHoleShapePartVerts,
    .verts            = _gMappicS203WaterHoleShapeVerts,
    .normals          = &_gMappicS203WaterHoleShapeVerts[ARRAY_SIZE(_gMappicS203WaterHoleShapeVerts)], // Empty normal pool at the vertex end.
    .skeleton         = _gMappicS203WaterHoleShapeSkeleton,
    .stream           = _gMappicS203WaterHoleShapeStream,
};

/// Initial identity pose of the Cellar shape's single self-parented root.
static TmdBone _gMappicS203CellarShapeSkeleton[] = {
#include "assets/mappic_s4_00_model_0005C_00154_skeleton.inc"
};

/// Six vertices in the Cellar shape's sole part.
static u32 _gMappicS203CellarShapePartVerts[] = {
#include "assets/mappic_s4_00_model_0005C_00154_partVerts.inc"
};

/// Cellar area corners in integer model X/Z coordinates, with Y zero.
static SVECTOR _gMappicS203CellarShapeVerts[] = {
#include "assets/mappic_s4_00_model_0005C_00154_verts.inc"
};

/// Two unlit quads' packed vertex byte offsets and colours, then two group ends and the stream end.
static u32 _gMappicS203CellarShapeStream[] = {
#include "assets/mappic_s4_00_model_0005C_00154_stream.inc"
};

TmdSource gMappicS203CellarShape = {
    .bufferHalfBytes  = 2 * sizeof(POLY_F4),
    .partCount        = ARRAY_SIZE(_gMappicS203CellarShapeSkeleton),
    .partVertexCounts = _gMappicS203CellarShapePartVerts,
    .verts            = _gMappicS203CellarShapeVerts,
    .normals          = &_gMappicS203CellarShapeVerts[ARRAY_SIZE(_gMappicS203CellarShapeVerts)], // Empty normal pool at the vertex end.
    .skeleton         = _gMappicS203CellarShapeSkeleton,
    .stream           = _gMappicS203CellarShapeStream,
};

static TmdBone _gMappicS203Model001F0Skeleton[1] = {
#include "assets/mappic_s2_03_model_001F0_skeleton.inc"
};

static u32 _gMappicS203Model001F0PartVerts[1] = {
#include "assets/mappic_s2_03_model_001F0_partVerts.inc"
};

static SVECTOR _gMappicS203Model001F0Verts[10] = {
#include "assets/mappic_s2_03_model_001F0_verts.inc"
};

static u32 _gMappicS203Model001F0Stream[24] = {
#include "assets/mappic_s2_03_model_001F0_stream.inc"
};

TmdSource D_mappic_s2_03_8012F180 = {
    0,
    112,
    0,
    1,
    _gMappicS203Model001F0PartVerts,
    _gMappicS203Model001F0Verts,
    &_gMappicS203Model001F0Verts[10],
    _gMappicS203Model001F0Skeleton,
    _gMappicS203Model001F0Stream,
};
