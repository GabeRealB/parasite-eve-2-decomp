#include "mappic/mappic.h"

#include <psyq/libgpu.h>

#include "common.h"

/// Identity transform and self-parented root for the Cavern shape.
static TmdBone _gMappicS400CavernShapeSkeleton[] = {
#include "assets/mappic_s4_00_model_0005C_0008C_mappic_s4_00_skeleton.inc"
};

/// Six vertices in the Cavern shape's sole part.
static u32 _gMappicS400CavernShapePartVerts[] = {
#include "assets/mappic_s4_00_model_0005C_0008C_mappic_s4_00_partVerts.inc"
};

/// Cavern's map footprint in integer model X/Z coordinates, with Y zero.
static SVECTOR _gMappicS400CavernShapeVerts[] = {
#include "assets/mappic_s4_00_model_0005C_0008C_mappic_s4_00_verts.inc"
};

/// Packed command stream for the Cavern shape's two unlit quads.
///
/// Elements contain four SVECTOR byte offsets and white colour; two group
/// ends precede the stream end. The map reads only the first group.
static u32 _gMappicS400CavernShapeStream[] = {
#include "assets/mappic_s4_00_model_0005C_0008C_mappic_s4_00_stream.inc"
};

TmdSource gMappicS400CavernShape = {
    .bufferHalfBytes  = 2 * sizeof(POLY_F4),
    .partCount        = ARRAY_SIZE(_gMappicS400CavernShapeSkeleton),
    .partVertexCounts = _gMappicS400CavernShapePartVerts,
    .verts            = _gMappicS400CavernShapeVerts,
    .normals          = &_gMappicS400CavernShapeVerts[ARRAY_SIZE(_gMappicS400CavernShapeVerts)], // Empty normal pool at the vertex end.
    .skeleton         = _gMappicS400CavernShapeSkeleton,
    .stream           = _gMappicS400CavernShapeStream,
};

/// Identity transform and self-parented root for the Secret Passage shape.
static TmdBone _gMappicS400SecretPassageShapeSkeleton[] = {
#include "assets/mappic_s4_00_model_00148_skeleton.inc"
};

/// Fourteen vertices in the Secret Passage shape's sole part.
static u32 _gMappicS400SecretPassageShapePartVerts[] = {
#include "assets/mappic_s4_00_model_00148_partVerts.inc"
};

/// Secret Passage's map footprint in integer model X/Z coordinates, with Y zero.
static SVECTOR _gMappicS400SecretPassageShapeVerts[] = {
#include "assets/mappic_s4_00_model_00148_verts.inc"
};

/// Packed command stream for the Secret Passage shape's six unlit quads.
///
/// Elements contain four SVECTOR byte offsets and white colour; two group
/// ends precede the stream end. The map reads only the first group.
static u32 _gMappicS400SecretPassageShapeStream[] = {
#include "assets/mappic_s4_00_model_00148_stream.inc"
};

TmdSource gMappicS400SecretPassageShape = {
    .bufferHalfBytes  = 6 * sizeof(POLY_F4),
    .partCount        = ARRAY_SIZE(_gMappicS400SecretPassageShapeSkeleton),
    .partVertexCounts = _gMappicS400SecretPassageShapePartVerts,
    .verts            = _gMappicS400SecretPassageShapeVerts,
    .normals          = &_gMappicS400SecretPassageShapeVerts[ARRAY_SIZE(_gMappicS400SecretPassageShapeVerts)], // Empty normal pool at the vertex end.
    .skeleton         = _gMappicS400SecretPassageShapeSkeleton,
    .stream           = _gMappicS400SecretPassageShapeStream,
};
