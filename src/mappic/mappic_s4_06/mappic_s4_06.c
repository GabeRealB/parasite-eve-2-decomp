#include "mappic/mappic.h"

#include <psyq/libgpu.h>

#include "common.h"

/// Identity transform and self-parented root for the Reservoir shape.
static TmdBone _gMappicS406ReservoirShapeSkeleton[] = {
#include "assets/mappic_s2_02_model_00910_000C0_mappic_s4_06_skeleton.inc"
};

/// Vertex count of the Reservoir shape's sole part (eight vertices).
static u32 _gMappicS406ReservoirShapePartVerts[] = {
#include "assets/mappic_s2_02_model_00910_000C0_mappic_s4_06_partVerts.inc"
};

/// Reservoir's map footprint in integer model X/Z coordinates, with Y zero.
static SVECTOR _gMappicS406ReservoirShapeVerts[] = {
#include "assets/mappic_s2_02_model_00910_000C0_mappic_s4_06_verts.inc"
};

/// Packed command stream for the Reservoir shape's two unlit triangles and two quads.
///
/// Three words per face hold packed SVECTOR byte offsets and white colour.
/// Two group ends precede the stream end; the map reads only the first group.
static u32 _gMappicS406ReservoirShapeStream[] = {
#include "assets/mappic_s2_02_model_00910_000C0_mappic_s4_06_stream.inc"
};

TmdSource gMappicS406ReservoirShape = {
    .bufferHalfBytes  = 2 * sizeof(POLY_F3) + 2 * sizeof(POLY_F4),
    .partCount        = ARRAY_SIZE(_gMappicS406ReservoirShapeSkeleton),
    .partVertexCounts = _gMappicS406ReservoirShapePartVerts,
    .verts            = _gMappicS406ReservoirShapeVerts,
    .normals          = &_gMappicS406ReservoirShapeVerts[ARRAY_SIZE(_gMappicS406ReservoirShapeVerts)], // Empty normal pool at the vertex end.
    .skeleton         = _gMappicS406ReservoirShapeSkeleton,
    .stream           = _gMappicS406ReservoirShapeStream,
};

/// Identity transform and self-parented root for the Upper sewer shape.
static TmdBone _gMappicS406UpperSewerShapeSkeleton[] = {
#include "assets/mappic_s4_06_model_0014C_00188_skeleton.inc"
};

/// Vertex count of the Upper sewer shape's sole part (eight vertices).
static u32 _gMappicS406UpperSewerShapePartVerts[] = {
#include "assets/mappic_s4_06_model_0014C_00188_partVerts.inc"
};

/// Upper sewer's map footprint in integer model X/Z coordinates, with Y zero.
static SVECTOR _gMappicS406UpperSewerShapeVerts[] = {
#include "assets/mappic_s4_06_model_0014C_00188_verts.inc"
};

/// Packed command stream for the Upper sewer shape's three unlit quads.
///
/// Three words per face hold packed SVECTOR byte offsets and white colour.
/// Two group ends precede the stream end; the map reads only the first group.
static u32 _gMappicS406UpperSewerShapeStream[] = {
#include "assets/mappic_s4_06_model_0014C_00188_stream.inc"
};

TmdSource gMappicS406UpperSewerShape = {
    .bufferHalfBytes  = 3 * sizeof(POLY_F4),
    .partCount        = ARRAY_SIZE(_gMappicS406UpperSewerShapeSkeleton),
    .partVertexCounts = _gMappicS406UpperSewerShapePartVerts,
    .verts            = _gMappicS406UpperSewerShapeVerts,
    .normals          = &_gMappicS406UpperSewerShapeVerts[ARRAY_SIZE(_gMappicS406UpperSewerShapeVerts)], // Empty normal pool at the vertex end.
    .skeleton         = _gMappicS406UpperSewerShapeSkeleton,
    .stream           = _gMappicS406UpperSewerShapeStream,
};

/// Identity transform and self-parented root for the Water supply shape.
static TmdBone _gMappicS406WaterSupplyShapeSkeleton[] = {
#include "assets/mappic_s4_06_model_00214_00268_skeleton.inc"
};

/// Vertex count of the Water supply shape's sole part (eight vertices).
static u32 _gMappicS406WaterSupplyShapePartVerts[] = {
#include "assets/mappic_s4_06_model_00214_00268_partVerts.inc"
};

/// Water supply's map footprint in integer model X/Z coordinates, with Y zero.
static SVECTOR _gMappicS406WaterSupplyShapeVerts[] = {
#include "assets/mappic_s4_06_model_00214_00268_verts.inc"
};

/// Packed command stream for the Water supply shape's two unlit triangles and two quads.
///
/// Three words per face hold packed SVECTOR byte offsets and white colour.
/// Two group ends precede the stream end; the map reads only the first group.
static u32 _gMappicS406WaterSupplyShapeStream[] = {
#include "assets/mappic_s4_06_model_00214_00268_stream.inc"
};

TmdSource gMappicS406WaterSupplyShape = {
    .bufferHalfBytes  = 2 * sizeof(POLY_F3) + 2 * sizeof(POLY_F4),
    .partCount        = ARRAY_SIZE(_gMappicS406WaterSupplyShapeSkeleton),
    .partVertexCounts = _gMappicS406WaterSupplyShapePartVerts,
    .verts            = _gMappicS406WaterSupplyShapeVerts,
    .normals          = &_gMappicS406WaterSupplyShapeVerts[ARRAY_SIZE(_gMappicS406WaterSupplyShapeVerts)], // Empty normal pool at the vertex end.
    .skeleton         = _gMappicS406WaterSupplyShapeSkeleton,
    .stream           = _gMappicS406WaterSupplyShapeStream,
};

/// Identity transform and self-parented root for the Lower sewer shape.
static TmdBone _gMappicS406LowerSewerShapeSkeleton[] = {
#include "assets/mappic_s2_02_model_00CF8_00330_skeleton.inc"
};

/// Vertex count of the Lower sewer shape's sole part (eight vertices).
static u32 _gMappicS406LowerSewerShapePartVerts[] = {
#include "assets/mappic_s2_02_model_00CF8_00330_partVerts.inc"
};

/// Lower sewer's map footprint in integer model X/Z coordinates, with Y zero.
static SVECTOR _gMappicS406LowerSewerShapeVerts[] = {
#include "assets/mappic_s2_02_model_00CF8_00330_verts.inc"
};

/// Packed command stream for the Lower sewer shape's three unlit quads.
///
/// Three words per face hold packed SVECTOR byte offsets and white colour.
/// Two group ends precede the stream end; the map reads only the first group.
static u32 _gMappicS406LowerSewerShapeStream[] = {
#include "assets/mappic_s2_02_model_00CF8_00330_stream.inc"
};

TmdSource gMappicS406LowerSewerShape = {
    .bufferHalfBytes  = 3 * sizeof(POLY_F4),
    .partCount        = ARRAY_SIZE(_gMappicS406LowerSewerShapeSkeleton),
    .partVertexCounts = _gMappicS406LowerSewerShapePartVerts,
    .verts            = _gMappicS406LowerSewerShapeVerts,
    .normals          = &_gMappicS406LowerSewerShapeVerts[ARRAY_SIZE(_gMappicS406LowerSewerShapeVerts)], // Empty normal pool at the vertex end.
    .skeleton         = _gMappicS406LowerSewerShapeSkeleton,
    .stream           = _gMappicS406LowerSewerShapeStream,
};
