#include "mappic/mappic.h"

#include <psyq/libgpu.h>

#include "common.h"

/// Identity transform and self-parented root for the Water hole shape.
static TmdBone _gMappicS303WaterHoleShapeSkeleton[] = {
#include "assets/mappic_s2_02_model_00CF8_000A8_skeleton.inc"
};

/// Vertex count of the Water hole shape's sole part (eight vertices).
static u32 _gMappicS303WaterHoleShapePartVerts[] = {
#include "assets/mappic_s2_02_model_00CF8_000A8_partVerts.inc"
};

/// Water hole's map outline in integer model X/Z coordinates, with Y zero.
static SVECTOR _gMappicS303WaterHoleShapeVerts[] = {
#include "assets/mappic_s2_02_model_00CF8_000A8_verts.inc"
};

/// Three unlit quads' packed vertex byte offsets and colours, then two group ends and the stream end.
static u32 _gMappicS303WaterHoleShapeStream[] = {
#include "assets/mappic_s2_02_model_00CF8_000A8_stream.inc"
};

TmdSource gMappicS303WaterHoleShape = {
    .bufferHalfBytes  = 3 * sizeof(POLY_F4),
    .partCount        = ARRAY_SIZE(_gMappicS303WaterHoleShapeSkeleton),
    .partVertexCounts = _gMappicS303WaterHoleShapePartVerts,
    .verts            = _gMappicS303WaterHoleShapeVerts,
    .normals          = &_gMappicS303WaterHoleShapeVerts[ARRAY_SIZE(_gMappicS303WaterHoleShapeVerts)], // Empty normal pool at the vertex end.
    .skeleton         = _gMappicS303WaterHoleShapeSkeleton,
    .stream           = _gMappicS303WaterHoleShapeStream,
};

static TmdBone _gMappicS303MappicS400Model0005C00154Skeleton[1] = {
#include "assets/mappic_s4_00_model_0005C_00154_skeleton.inc"
};

static u32 _gMappicS303MappicS400Model0005C00154PartVerts[1] = {
#include "assets/mappic_s4_00_model_0005C_00154_partVerts.inc"
};

static SVECTOR _gMappicS303MappicS400Model0005C00154Verts[6] = {
#include "assets/mappic_s4_00_model_0005C_00154_verts.inc"
};

static u32 _gMappicS303MappicS400Model0005C00154Stream[12] = {
#include "assets/mappic_s4_00_model_0005C_00154_stream.inc"
};

TmdSource D_mappic_s3_03_8012F084 = {
    0,
    48,
    0,
    1,
    _gMappicS303MappicS400Model0005C00154PartVerts,
    _gMappicS303MappicS400Model0005C00154Verts,
    &_gMappicS303MappicS400Model0005C00154Verts[6],
    _gMappicS303MappicS400Model0005C00154Skeleton,
    _gMappicS303MappicS400Model0005C00154Stream,
};

static TmdBone _gMappicS303MappicS203Model001F000250Skeleton[1] = {
#include "assets/mappic_s2_03_model_001F0_00250_skeleton.inc"
};

static u32 _gMappicS303MappicS203Model001F000250PartVerts[1] = {
#include "assets/mappic_s2_03_model_001F0_00250_partVerts.inc"
};

static SVECTOR _gMappicS303MappicS203Model001F000250Verts[10] = {
#include "assets/mappic_s2_03_model_001F0_00250_verts.inc"
};

static u32 _gMappicS303MappicS203Model001F000250Stream[24] = {
#include "assets/mappic_s2_03_model_001F0_00250_stream.inc"
};

TmdSource D_mappic_s3_03_8012F180 = {
    0,
    112,
    0,
    1,
    _gMappicS303MappicS203Model001F000250PartVerts,
    _gMappicS303MappicS203Model001F000250Verts,
    &_gMappicS303MappicS203Model001F000250Verts[10],
    _gMappicS303MappicS203Model001F000250Skeleton,
    _gMappicS303MappicS203Model001F000250Stream,
};
