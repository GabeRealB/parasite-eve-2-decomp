#include "mappic/mappic.h"

#include <psyq/libgpu.h>

#include "common.h"

/// Identity transform and self-parented root for the Parking garage shape.
static TmdBone _gMappicS503ParkingGarageShapeSkeleton[] = {
#include "assets/mappic_s2_02_model_00C48_00070_mappic_s5_03_skeleton.inc"
};

/// Vertex count of the Parking garage shape's sole part (four vertices).
static u32 _gMappicS503ParkingGarageShapePartVerts[] = {
#include "assets/mappic_s2_02_model_00C48_00070_mappic_s5_03_partVerts.inc"
};

/// Parking garage fill vertices in integer model X/Z coordinates, with Y zero.
static SVECTOR _gMappicS503ParkingGarageShapeVerts[] = {
#include "assets/mappic_s2_02_model_00C48_00070_mappic_s5_03_verts.inc"
};

/// One unlit quad, followed by two group ends and the stream end.
///
/// The face stores packed u16 vertex byte offsets and a colour word. The map
/// uses the geometry to draw a textured fill and ignores the stored colour.
static u32 _gMappicS503ParkingGarageShapeStream[] = {
#include "assets/mappic_s2_02_model_00C48_00070_mappic_s5_03_stream.inc"
};

TmdSource gMappicS503ParkingGarageShape = {
    .bufferHalfBytes  = sizeof(POLY_F4),
    .partCount        = ARRAY_SIZE(_gMappicS503ParkingGarageShapeSkeleton),
    .partVertexCounts = _gMappicS503ParkingGarageShapePartVerts,
    .verts            = _gMappicS503ParkingGarageShapeVerts,
    .normals          = &_gMappicS503ParkingGarageShapeVerts[ARRAY_SIZE(_gMappicS503ParkingGarageShapeVerts)], // Empty normal pool at the vertex end.
    .skeleton         = _gMappicS503ParkingGarageShapeSkeleton,
    .stream           = _gMappicS503ParkingGarageShapeStream,
};

/// Identity transform and self-parented root for the Vehicular airlock shape.
static TmdBone _gMappicS503VehicularAirlockShapeSkeleton[] = {
#include "assets/mappic_s2_02_model_00C48_00100_mappic_s5_03_skeleton.inc"
};

/// Vertex count of the Vehicular airlock shape's sole part (four vertices).
static u32 _gMappicS503VehicularAirlockShapePartVerts[] = {
#include "assets/mappic_s2_02_model_00C48_00100_mappic_s5_03_partVerts.inc"
};

/// Vehicular airlock fill vertices in integer model X/Z coordinates, with Y zero.
static SVECTOR _gMappicS503VehicularAirlockShapeVerts[] = {
#include "assets/mappic_s2_02_model_00C48_00100_mappic_s5_03_verts.inc"
};

/// One unlit quad, followed by two group ends and the stream end.
///
/// The face stores packed u16 vertex byte offsets and a colour word. The map
/// uses the geometry to draw a textured fill and ignores the stored colour.
static u32 _gMappicS503VehicularAirlockShapeStream[] = {
#include "assets/mappic_s2_02_model_00C48_00100_mappic_s5_03_stream.inc"
};

TmdSource gMappicS503VehicularAirlockShape = {
    .bufferHalfBytes  = sizeof(POLY_F4),
    .partCount        = ARRAY_SIZE(_gMappicS503VehicularAirlockShapeSkeleton),
    .partVertexCounts = _gMappicS503VehicularAirlockShapePartVerts,
    .verts            = _gMappicS503VehicularAirlockShapeVerts,
    .normals          = &_gMappicS503VehicularAirlockShapeVerts[ARRAY_SIZE(_gMappicS503VehicularAirlockShapeVerts)], // Empty normal pool at the vertex end.
    .skeleton         = _gMappicS503VehicularAirlockShapeSkeleton,
    .stream           = _gMappicS503VehicularAirlockShapeStream,
};

/// Identity transform and self-parented root for the Bulwark shape.
static TmdBone _gMappicS503BulwarkShapeSkeleton[] = {
#include "assets/mappic_s4_06_model_00214_001E0_skeleton.inc"
};

/// Vertex count of the Bulwark shape's sole part (eight vertices).
static u32 _gMappicS503BulwarkShapePartVerts[] = {
#include "assets/mappic_s4_06_model_00214_001E0_partVerts.inc"
};

/// Bulwark fill vertices in integer model X/Z coordinates, with Y zero.
static SVECTOR _gMappicS503BulwarkShapeVerts[] = {
#include "assets/mappic_s4_06_model_00214_001E0_verts.inc"
};

/// Two unlit triangles and two unlit quads, followed by two group ends and the stream end.
///
/// Each face stores packed u16 vertex byte offsets and a colour word. The map
/// uses the geometry to draw textured fills and ignores the stored colours.
static u32 _gMappicS503BulwarkShapeStream[] = {
#include "assets/mappic_s4_06_model_00214_001E0_stream.inc"
};

TmdSource gMappicS503BulwarkShape = {
    .bufferHalfBytes  = 2 * sizeof(POLY_F3) + 2 * sizeof(POLY_F4),
    .partCount        = ARRAY_SIZE(_gMappicS503BulwarkShapeSkeleton),
    .partVertexCounts = _gMappicS503BulwarkShapePartVerts,
    .verts            = _gMappicS503BulwarkShapeVerts,
    .normals          = &_gMappicS503BulwarkShapeVerts[ARRAY_SIZE(_gMappicS503BulwarkShapeVerts)], // Empty normal pool at the vertex end.
    .skeleton         = _gMappicS503BulwarkShapeSkeleton,
    .stream           = _gMappicS503BulwarkShapeStream,
};

/// Identity transform and self-parented root for the Heliport shape.
static TmdBone _gMappicS503HeliportShapeSkeleton[] = {
#include "assets/mappic_s2_02_model_00910_002C0_skeleton.inc"
};

/// Vertex count of the Heliport shape's sole part (eight vertices).
static u32 _gMappicS503HeliportShapePartVerts[] = {
#include "assets/mappic_s2_02_model_00910_002C0_partVerts.inc"
};

/// Heliport fill vertices in integer model X/Z coordinates, with Y zero.
static SVECTOR _gMappicS503HeliportShapeVerts[] = {
#include "assets/mappic_s2_02_model_00910_002C0_verts.inc"
};

/// Two unlit triangles and two unlit quads, followed by two group ends and the stream end.
///
/// Each face stores packed u16 vertex byte offsets and a colour word. The map
/// uses the geometry to draw textured fills and ignores the stored colours.
static u32 _gMappicS503HeliportShapeStream[] = {
#include "assets/mappic_s2_02_model_00910_002C0_stream.inc"
};

TmdSource gMappicS503HeliportShape = {
    .bufferHalfBytes  = 2 * sizeof(POLY_F3) + 2 * sizeof(POLY_F4),
    .partCount        = ARRAY_SIZE(_gMappicS503HeliportShapeSkeleton),
    .partVertexCounts = _gMappicS503HeliportShapePartVerts,
    .verts            = _gMappicS503HeliportShapeVerts,
    .normals          = &_gMappicS503HeliportShapeVerts[ARRAY_SIZE(_gMappicS503HeliportShapeVerts)], // Empty normal pool at the vertex end.
    .skeleton         = _gMappicS503HeliportShapeSkeleton,
    .stream           = _gMappicS503HeliportShapeStream,
};

/// Identity transform and self-parented root for the Airlock shape.
static TmdBone _gMappicS503AirlockShapeSkeleton[] = {
#include "assets/mappic_s2_02_model_00CF8_00388_skeleton.inc"
};

/// Vertex count of the Airlock shape's sole part (eight vertices).
static u32 _gMappicS503AirlockShapePartVerts[] = {
#include "assets/mappic_s2_02_model_00CF8_00388_partVerts.inc"
};

/// Airlock fill vertices in integer model X/Z coordinates, with Y zero.
static SVECTOR _gMappicS503AirlockShapeVerts[] = {
#include "assets/mappic_s2_02_model_00CF8_00388_verts.inc"
};

/// Three unlit quads, followed by two group ends and the stream end.
///
/// Each face stores packed u16 vertex byte offsets and a colour word. The map
/// uses the geometry to draw textured fills and ignores the stored colours.
static u32 _gMappicS503AirlockShapeStream[] = {
#include "assets/mappic_s2_02_model_00CF8_00388_stream.inc"
};

TmdSource gMappicS503AirlockShape = {
    .bufferHalfBytes  = 3 * sizeof(POLY_F4),
    .partCount        = ARRAY_SIZE(_gMappicS503AirlockShapeSkeleton),
    .partVertexCounts = _gMappicS503AirlockShapePartVerts,
    .verts            = _gMappicS503AirlockShapeVerts,
    .normals          = &_gMappicS503AirlockShapeVerts[ARRAY_SIZE(_gMappicS503AirlockShapeVerts)], // Empty normal pool at the vertex end.
    .skeleton         = _gMappicS503AirlockShapeSkeleton,
    .stream           = _gMappicS503AirlockShapeStream,
};

/// Identity transform and self-parented root for the Guardroom shape.
static TmdBone _gMappicS503GuardroomShapeSkeleton[] = {
#include "assets/mappic_s2_02_model_00C48_00418_skeleton.inc"
};

/// Vertex count of the Guardroom shape's sole part (four vertices).
static u32 _gMappicS503GuardroomShapePartVerts[] = {
#include "assets/mappic_s2_02_model_00C48_00418_partVerts.inc"
};

/// Guardroom fill vertices in integer model X/Z coordinates, with Y zero.
static SVECTOR _gMappicS503GuardroomShapeVerts[] = {
#include "assets/mappic_s2_02_model_00C48_00418_verts.inc"
};

/// One unlit quad, followed by two group ends and the stream end.
///
/// The face stores packed u16 vertex byte offsets and a colour word. The map
/// uses the geometry to draw a textured fill and ignores the stored colour.
static u32 _gMappicS503GuardroomShapeStream[] = {
#include "assets/mappic_s2_02_model_00C48_00418_stream.inc"
};

TmdSource gMappicS503GuardroomShape = {
    .bufferHalfBytes  = sizeof(POLY_F4),
    .partCount        = ARRAY_SIZE(_gMappicS503GuardroomShapeSkeleton),
    .partVertexCounts = _gMappicS503GuardroomShapePartVerts,
    .verts            = _gMappicS503GuardroomShapeVerts,
    .normals          = &_gMappicS503GuardroomShapeVerts[ARRAY_SIZE(_gMappicS503GuardroomShapeVerts)], // Empty normal pool at the vertex end.
    .skeleton         = _gMappicS503GuardroomShapeSkeleton,
    .stream           = _gMappicS503GuardroomShapeStream,
};
