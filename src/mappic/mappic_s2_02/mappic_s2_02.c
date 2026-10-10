#include "mappic/mappic.h"

#include <psyq/libgpu.h>

#include "common.h"

/// Identity transform and self-parented root for the Gas Station shape.
static TmdBone _gMappicS202GasStationShapeSkeleton[] = {
#include "assets/mappic_s4_00_model_0005C_0008C_skeleton.inc"
};

/// Vertex count of the Gas Station shape's sole part (six vertices).
static u32 _gMappicS202GasStationShapePartVerts[] = {
#include "assets/mappic_s4_00_model_0005C_0008C_partVerts.inc"
};

/// Gas Station's map footprint in integer model X/Z coordinates, with Y zero.
static SVECTOR _gMappicS202GasStationShapeVerts[] = {
#include "assets/mappic_s4_00_model_0005C_0008C_verts.inc"
};

/// Two unlit quads' packed vertex byte offsets and white colour, followed by two group ends and the stream end.
static u32 _gMappicS202GasStationShapeStream[] = {
#include "assets/mappic_s4_00_model_0005C_0008C_stream.inc"
};

TmdSource gMappicS202GasStationShape = {
    .bufferHalfBytes  = 2 * sizeof(POLY_F4),
    .partCount        = ARRAY_SIZE(_gMappicS202GasStationShapeSkeleton),
    .partVertexCounts = _gMappicS202GasStationShapePartVerts,
    .verts            = _gMappicS202GasStationShapeVerts,
    .normals          = &_gMappicS202GasStationShapeVerts[ARRAY_SIZE(_gMappicS202GasStationShapeVerts)], // Empty normal pool at the vertex end.
    .skeleton         = _gMappicS202GasStationShapeSkeleton,
    .stream           = _gMappicS202GasStationShapeStream,
};

/// Identity transform and self-parented root for the Main Street shape.
static TmdBone _gMappicS202MainStreetShapeSkeleton[] = {
#include "assets/mappic_s2_02_model_00C48_0011C_skeleton.inc"
};

/// Vertex count of the Main Street shape's sole part (four vertices).
static u32 _gMappicS202MainStreetShapePartVerts[] = {
#include "assets/mappic_s2_02_model_00C48_0011C_partVerts.inc"
};

/// Main Street's map footprint in integer model X/Z coordinates, with Y zero.
static SVECTOR _gMappicS202MainStreetShapeVerts[] = {
#include "assets/mappic_s2_02_model_00C48_0011C_verts.inc"
};

/// One unlit quad's packed vertex byte offsets and white colour, followed by two group ends and the stream end.
static u32 _gMappicS202MainStreetShapeStream[] = {
#include "assets/mappic_s2_02_model_00C48_0011C_stream.inc"
};

TmdSource gMappicS202MainStreetShape = {
    .bufferHalfBytes  = sizeof(POLY_F4),
    .partCount        = ARRAY_SIZE(_gMappicS202MainStreetShapeSkeleton),
    .partVertexCounts = _gMappicS202MainStreetShapePartVerts,
    .verts            = _gMappicS202MainStreetShapeVerts,
    .normals          = &_gMappicS202MainStreetShapeVerts[ARRAY_SIZE(_gMappicS202MainStreetShapeVerts)], // Empty normal pool at the vertex end.
    .skeleton         = _gMappicS202MainStreetShapeSkeleton,
    .stream           = _gMappicS202MainStreetShapeStream,
};

/// Identity transform and self-parented root for the General Store shape.
static TmdBone _gMappicS202GeneralStoreShapeSkeleton[] = {
#include "assets/mappic_s2_02_model_00C48_001AC_skeleton.inc"
};

/// Vertex count of the General Store shape's sole part (four vertices).
static u32 _gMappicS202GeneralStoreShapePartVerts[] = {
#include "assets/mappic_s2_02_model_00C48_001AC_partVerts.inc"
};

/// General Store's map footprint in integer model X/Z coordinates, with Y zero.
static SVECTOR _gMappicS202GeneralStoreShapeVerts[] = {
#include "assets/mappic_s2_02_model_00C48_001AC_verts.inc"
};

/// Packed command stream for the General Store shape's single unlit quad.
///
/// Four SVECTOR byte offsets and white colour; two group ends precede the
/// stream end. The map reads only the first group.
static u32 _gMappicS202GeneralStoreShapeStream[] = {
#include "assets/mappic_s2_02_model_00C48_001AC_stream.inc"
};

TmdSource gMappicS202GeneralStoreShape = {
    .bufferHalfBytes  = sizeof(POLY_F4),
    .partCount        = ARRAY_SIZE(_gMappicS202GeneralStoreShapeSkeleton),
    .partVertexCounts = _gMappicS202GeneralStoreShapePartVerts,
    .verts            = _gMappicS202GeneralStoreShapeVerts,
    .normals          = &_gMappicS202GeneralStoreShapeVerts[ARRAY_SIZE(_gMappicS202GeneralStoreShapeVerts)], // Empty normal pool at the vertex end.
    .skeleton         = _gMappicS202GeneralStoreShapeSkeleton,
    .stream           = _gMappicS202GeneralStoreShapeStream,
};

/// Identity transform and self-parented root for the Back Street shape.
static TmdBone _gMappicS202BackStreetShapeSkeleton[] = {
#include "assets/mappic_s4_00_model_0005C_00258_skeleton.inc"
};

/// Vertex count of the Back Street shape's sole part (six vertices).
static u32 _gMappicS202BackStreetShapePartVerts[] = {
#include "assets/mappic_s4_00_model_0005C_00258_partVerts.inc"
};

/// Back Street's map footprint in integer model X/Z coordinates, with Y zero.
static SVECTOR _gMappicS202BackStreetShapeVerts[] = {
#include "assets/mappic_s4_00_model_0005C_00258_verts.inc"
};

/// Packed command stream for the Back Street shape's two unlit quads.
///
/// Four SVECTOR byte offsets and white colour per quad; two group ends precede
/// the stream end. The map reads only the first group.
static u32 _gMappicS202BackStreetShapeStream[] = {
#include "assets/mappic_s4_00_model_0005C_00258_stream.inc"
};

TmdSource gMappicS202BackStreetShape = {
    .bufferHalfBytes  = 2 * sizeof(POLY_F4),
    .partCount        = ARRAY_SIZE(_gMappicS202BackStreetShapeSkeleton),
    .partVertexCounts = _gMappicS202BackStreetShapePartVerts,
    .verts            = _gMappicS202BackStreetShapeVerts,
    .normals          = &_gMappicS202BackStreetShapeVerts[ARRAY_SIZE(_gMappicS202BackStreetShapeVerts)], // Empty normal pool at the vertex end.
    .skeleton         = _gMappicS202BackStreetShapeSkeleton,
    .stream           = _gMappicS202BackStreetShapeStream,
};

/// Identity transform and self-parented root for the Souvenir Shop shape.
static TmdBone _gMappicS202SouvenirShopShapeSkeleton[] = {
#include "assets/mappic_s2_02_model_00C48_002E8_skeleton.inc"
};

/// Vertex count of the Souvenir Shop shape's sole part (four vertices).
static u32 _gMappicS202SouvenirShopShapePartVerts[] = {
#include "assets/mappic_s2_02_model_00C48_002E8_partVerts.inc"
};

/// Souvenir Shop's map footprint in integer model X/Z coordinates, with Y zero.
static SVECTOR _gMappicS202SouvenirShopShapeVerts[] = {
#include "assets/mappic_s2_02_model_00C48_002E8_verts.inc"
};

/// Packed command stream for the Souvenir Shop shape's single unlit quad.
///
/// Four SVECTOR byte offsets and white colour; two group ends precede the
/// stream end. The map reads only the first group.
static u32 _gMappicS202SouvenirShopShapeStream[] = {
#include "assets/mappic_s2_02_model_00C48_002E8_stream.inc"
};

TmdSource gMappicS202SouvenirShopShape = {
    .bufferHalfBytes  = sizeof(POLY_F4),
    .partCount        = ARRAY_SIZE(_gMappicS202SouvenirShopShapeSkeleton),
    .partVertexCounts = _gMappicS202SouvenirShopShapePartVerts,
    .verts            = _gMappicS202SouvenirShopShapeVerts,
    .normals          = &_gMappicS202SouvenirShopShapeVerts[ARRAY_SIZE(_gMappicS202SouvenirShopShapeVerts)], // Empty normal pool at the vertex end.
    .skeleton         = _gMappicS202SouvenirShopShapeSkeleton,
    .stream           = _gMappicS202SouvenirShopShapeStream,
};

static TmdBone _gMappicS202Model00C4800378Skeleton[1] = {
#include "assets/mappic_s2_02_model_00C48_00378_skeleton.inc"
};

static u32 _gMappicS202Model00C4800378PartVerts[1] = {
#include "assets/mappic_s2_02_model_00C48_00378_partVerts.inc"
};

static SVECTOR _gMappicS202Model00C4800378Verts[4] = {
#include "assets/mappic_s2_02_model_00C48_00378_verts.inc"
};

static u32 _gMappicS202Model00C4800378Stream[9] = {
#include "assets/mappic_s2_02_model_00C48_00378_stream.inc"
};

TmdSource D_mappic_s2_02_8012F2A8 = {
    0,
    24,
    0,
    1,
    _gMappicS202Model00C4800378PartVerts,
    _gMappicS202Model00C4800378Verts,
    &_gMappicS202Model00C4800378Verts[4],
    _gMappicS202Model00C4800378Skeleton,
    _gMappicS202Model00C4800378Stream,
};

static TmdBone _gMappicS202MappicS400Model0005C00424Skeleton[1] = {
#include "assets/mappic_s4_00_model_0005C_00424_skeleton.inc"
};

static u32 _gMappicS202MappicS400Model0005C00424PartVerts[1] = {
#include "assets/mappic_s4_00_model_0005C_00424_partVerts.inc"
};

static SVECTOR _gMappicS202MappicS400Model0005C00424Verts[6] = {
#include "assets/mappic_s4_00_model_0005C_00424_verts.inc"
};

static u32 _gMappicS202MappicS400Model0005C00424Stream[12] = {
#include "assets/mappic_s4_00_model_0005C_00424_stream.inc"
};

TmdSource D_mappic_s2_02_8012F354 = {
    0,
    48,
    0,
    1,
    _gMappicS202MappicS400Model0005C00424PartVerts,
    _gMappicS202MappicS400Model0005C00424Verts,
    &_gMappicS202MappicS400Model0005C00424Verts[6],
    _gMappicS202MappicS400Model0005C00424Skeleton,
    _gMappicS202MappicS400Model0005C00424Stream,
};

static TmdBone _gMappicS202Model00C48004B4Skeleton[1] = {
#include "assets/mappic_s2_02_model_00C48_004B4_skeleton.inc"
};

static u32 _gMappicS202Model00C48004B4PartVerts[1] = {
#include "assets/mappic_s2_02_model_00C48_004B4_partVerts.inc"
};

static SVECTOR _gMappicS202Model00C48004B4Verts[4] = {
#include "assets/mappic_s2_02_model_00C48_004B4_verts.inc"
};

static u32 _gMappicS202Model00C48004B4Stream[9] = {
#include "assets/mappic_s2_02_model_00C48_004B4_stream.inc"
};

TmdSource D_mappic_s2_02_8012F3E4 = {
    0,
    24,
    0,
    1,
    _gMappicS202Model00C48004B4PartVerts,
    _gMappicS202Model00C48004B4Verts,
    &_gMappicS202Model00C48004B4Verts[4],
    _gMappicS202Model00C48004B4Skeleton,
    _gMappicS202Model00C48004B4Stream,
};

static TmdBone _gMappicS202Model00C4800544Skeleton[1] = {
#include "assets/mappic_s2_02_model_00C48_00544_skeleton.inc"
};

static u32 _gMappicS202Model00C4800544PartVerts[1] = {
#include "assets/mappic_s2_02_model_00C48_00544_partVerts.inc"
};

static SVECTOR _gMappicS202Model00C4800544Verts[4] = {
#include "assets/mappic_s2_02_model_00C48_00544_verts.inc"
};

static u32 _gMappicS202Model00C4800544Stream[9] = {
#include "assets/mappic_s2_02_model_00C48_00544_stream.inc"
};

TmdSource D_mappic_s2_02_8012F474 = {
    0,
    24,
    0,
    1,
    _gMappicS202Model00C4800544PartVerts,
    _gMappicS202Model00C4800544Verts,
    &_gMappicS202Model00C4800544Verts[4],
    _gMappicS202Model00C4800544Skeleton,
    _gMappicS202Model00C4800544Stream,
};

static TmdBone _gMappicS202Model00C48005D4Skeleton[1] = {
#include "assets/mappic_s2_02_model_00C48_005D4_skeleton.inc"
};

static u32 _gMappicS202Model00C48005D4PartVerts[1] = {
#include "assets/mappic_s2_02_model_00C48_005D4_partVerts.inc"
};

static SVECTOR _gMappicS202Model00C48005D4Verts[4] = {
#include "assets/mappic_s2_02_model_00C48_005D4_verts.inc"
};

static u32 _gMappicS202Model00C48005D4Stream[9] = {
#include "assets/mappic_s2_02_model_00C48_005D4_stream.inc"
};

TmdSource D_mappic_s2_02_8012F504 = {
    0,
    24,
    0,
    1,
    _gMappicS202Model00C48005D4PartVerts,
    _gMappicS202Model00C48005D4Verts,
    &_gMappicS202Model00C48005D4Verts[4],
    _gMappicS202Model00C48005D4Skeleton,
    _gMappicS202Model00C48005D4Stream,
};

static TmdBone _gMappicS202Model00C4800664Skeleton[1] = {
#include "assets/mappic_s2_02_model_00C48_00664_skeleton.inc"
};

static u32 _gMappicS202Model00C4800664PartVerts[1] = {
#include "assets/mappic_s2_02_model_00C48_00664_partVerts.inc"
};

static SVECTOR _gMappicS202Model00C4800664Verts[4] = {
#include "assets/mappic_s2_02_model_00C48_00664_verts.inc"
};

static u32 _gMappicS202Model00C4800664Stream[9] = {
#include "assets/mappic_s2_02_model_00C48_00664_stream.inc"
};

TmdSource D_mappic_s2_02_8012F594 = {
    0,
    24,
    0,
    1,
    _gMappicS202Model00C4800664PartVerts,
    _gMappicS202Model00C4800664Verts,
    &_gMappicS202Model00C4800664Verts[4],
    _gMappicS202Model00C4800664Skeleton,
    _gMappicS202Model00C4800664Stream,
};

static TmdBone _gMappicS202Model00710Skeleton[1] = {
#include "assets/mappic_s2_02_model_00710_skeleton.inc"
};

static u32 _gMappicS202Model00710PartVerts[1] = {
#include "assets/mappic_s2_02_model_00710_partVerts.inc"
};

static SVECTOR _gMappicS202Model00710Verts[12] = {
#include "assets/mappic_s2_02_model_00710_verts.inc"
};

static u32 _gMappicS202Model00710Stream[21] = {
#include "assets/mappic_s2_02_model_00710_stream.inc"
};

TmdSource D_mappic_s2_02_8012F694 = {
    0,
    120,
    0,
    1,
    _gMappicS202Model00710PartVerts,
    _gMappicS202Model00710Verts,
    &_gMappicS202Model00710Verts[12],
    _gMappicS202Model00710Skeleton,
    _gMappicS202Model00710Stream,
};

static TmdBone _gMappicS202Model00C48007F4Skeleton[1] = {
#include "assets/mappic_s2_02_model_00C48_007F4_skeleton.inc"
};

static u32 _gMappicS202Model00C48007F4PartVerts[1] = {
#include "assets/mappic_s2_02_model_00C48_007F4_partVerts.inc"
};

static SVECTOR _gMappicS202Model00C48007F4Verts[4] = {
#include "assets/mappic_s2_02_model_00C48_007F4_verts.inc"
};

static u32 _gMappicS202Model00C48007F4Stream[9] = {
#include "assets/mappic_s2_02_model_00C48_007F4_stream.inc"
};

TmdSource D_mappic_s2_02_8012F724 = {
    0,
    24,
    0,
    1,
    _gMappicS202Model00C48007F4PartVerts,
    _gMappicS202Model00C48007F4Verts,
    &_gMappicS202Model00C48007F4Verts[4],
    _gMappicS202Model00C48007F4Skeleton,
    _gMappicS202Model00C48007F4Stream,
};

static TmdBone _gMappicS202Model00C4800884Skeleton[1] = {
#include "assets/mappic_s2_02_model_00C48_00884_skeleton.inc"
};

static u32 _gMappicS202Model00C4800884PartVerts[1] = {
#include "assets/mappic_s2_02_model_00C48_00884_partVerts.inc"
};

static SVECTOR _gMappicS202Model00C4800884Verts[4] = {
#include "assets/mappic_s2_02_model_00C48_00884_verts.inc"
};

static u32 _gMappicS202Model00C4800884Stream[9] = {
#include "assets/mappic_s2_02_model_00C48_00884_stream.inc"
};

TmdSource D_mappic_s2_02_8012F7B4 = {
    0,
    24,
    0,
    1,
    _gMappicS202Model00C4800884PartVerts,
    _gMappicS202Model00C4800884Verts,
    &_gMappicS202Model00C4800884Verts[4],
    _gMappicS202Model00C4800884Skeleton,
    _gMappicS202Model00C4800884Stream,
};

static TmdBone _gMappicS202Model00910Skeleton[1] = {
#include "assets/mappic_s2_02_model_00910_skeleton.inc"
};

static u32 _gMappicS202Model00910PartVerts[1] = {
#include "assets/mappic_s2_02_model_00910_partVerts.inc"
};

static SVECTOR _gMappicS202Model00910Verts[8] = {
#include "assets/mappic_s2_02_model_00910_verts.inc"
};

static u32 _gMappicS202Model00910Stream[21] = {
#include "assets/mappic_s2_02_model_00910_stream.inc"
};

TmdSource D_mappic_s2_02_8012F894 = {
    0,
    88,
    0,
    1,
    _gMappicS202Model00910PartVerts,
    _gMappicS202Model00910Verts,
    &_gMappicS202Model00910Verts[8],
    _gMappicS202Model00910Skeleton,
    _gMappicS202Model00910Stream,
};

static TmdBone _gMappicS202Model00C48009F4Skeleton[1] = {
#include "assets/mappic_s2_02_model_00C48_009F4_skeleton.inc"
};

static u32 _gMappicS202Model00C48009F4PartVerts[1] = {
#include "assets/mappic_s2_02_model_00C48_009F4_partVerts.inc"
};

static SVECTOR _gMappicS202Model00C48009F4Verts[4] = {
#include "assets/mappic_s2_02_model_00C48_009F4_verts.inc"
};

static u32 _gMappicS202Model00C48009F4Stream[9] = {
#include "assets/mappic_s2_02_model_00C48_009F4_stream.inc"
};

TmdSource D_mappic_s2_02_8012F924 = {
    0,
    24,
    0,
    1,
    _gMappicS202Model00C48009F4PartVerts,
    _gMappicS202Model00C48009F4Verts,
    &_gMappicS202Model00C48009F4Verts[4],
    _gMappicS202Model00C48009F4Skeleton,
    _gMappicS202Model00C48009F4Stream,
};

static TmdBone _gMappicS202MappicS400Model0005C00AA0Skeleton[1] = {
#include "assets/mappic_s4_00_model_0005C_00AA0_skeleton.inc"
};

static u32 _gMappicS202MappicS400Model0005C00AA0PartVerts[1] = {
#include "assets/mappic_s4_00_model_0005C_00AA0_partVerts.inc"
};

static SVECTOR _gMappicS202MappicS400Model0005C00AA0Verts[6] = {
#include "assets/mappic_s4_00_model_0005C_00AA0_verts.inc"
};

static u32 _gMappicS202MappicS400Model0005C00AA0Stream[12] = {
#include "assets/mappic_s4_00_model_0005C_00AA0_stream.inc"
};

TmdSource D_mappic_s2_02_8012F9D0 = {
    0,
    48,
    0,
    1,
    _gMappicS202MappicS400Model0005C00AA0PartVerts,
    _gMappicS202MappicS400Model0005C00AA0Verts,
    &_gMappicS202MappicS400Model0005C00AA0Verts[6],
    _gMappicS202MappicS400Model0005C00AA0Skeleton,
    _gMappicS202MappicS400Model0005C00AA0Stream,
};

static TmdBone _gMappicS202MappicS400Model0005C00B4CSkeleton[1] = {
#include "assets/mappic_s4_00_model_0005C_00B4C_skeleton.inc"
};

static u32 _gMappicS202MappicS400Model0005C00B4CPartVerts[1] = {
#include "assets/mappic_s4_00_model_0005C_00B4C_partVerts.inc"
};

static SVECTOR _gMappicS202MappicS400Model0005C00B4CVerts[6] = {
#include "assets/mappic_s4_00_model_0005C_00B4C_verts.inc"
};

static u32 _gMappicS202MappicS400Model0005C00B4CStream[12] = {
#include "assets/mappic_s4_00_model_0005C_00B4C_stream.inc"
};

TmdSource D_mappic_s2_02_8012FA7C = {
    0,
    48,
    0,
    1,
    _gMappicS202MappicS400Model0005C00B4CPartVerts,
    _gMappicS202MappicS400Model0005C00B4CVerts,
    &_gMappicS202MappicS400Model0005C00B4CVerts[6],
    _gMappicS202MappicS400Model0005C00B4CSkeleton,
    _gMappicS202MappicS400Model0005C00B4CStream,
};

static TmdBone _gMappicS202Model00C4800BDCSkeleton[1] = {
#include "assets/mappic_s2_02_model_00C48_00BDC_skeleton.inc"
};

static u32 _gMappicS202Model00C4800BDCPartVerts[1] = {
#include "assets/mappic_s2_02_model_00C48_00BDC_partVerts.inc"
};

static SVECTOR _gMappicS202Model00C4800BDCVerts[4] = {
#include "assets/mappic_s2_02_model_00C48_00BDC_verts.inc"
};

static u32 _gMappicS202Model00C4800BDCStream[9] = {
#include "assets/mappic_s2_02_model_00C48_00BDC_stream.inc"
};

TmdSource D_mappic_s2_02_8012FB0C = {
    0,
    24,
    0,
    1,
    _gMappicS202Model00C4800BDCPartVerts,
    _gMappicS202Model00C4800BDCVerts,
    &_gMappicS202Model00C4800BDCVerts[4],
    _gMappicS202Model00C4800BDCSkeleton,
    _gMappicS202Model00C4800BDCStream,
};

static TmdBone _gMappicS202Model00C4800C6CSkeleton[1] = {
#include "assets/mappic_s2_02_model_00C48_00C6C_skeleton.inc"
};

static u32 _gMappicS202Model00C4800C6CPartVerts[1] = {
#include "assets/mappic_s2_02_model_00C48_00C6C_partVerts.inc"
};

static SVECTOR _gMappicS202Model00C4800C6CVerts[4] = {
#include "assets/mappic_s2_02_model_00C48_00C6C_verts.inc"
};

static u32 _gMappicS202Model00C4800C6CStream[9] = {
#include "assets/mappic_s2_02_model_00C48_00C6C_stream.inc"
};

TmdSource D_mappic_s2_02_8012FB9C = {
    0,
    24,
    0,
    1,
    _gMappicS202Model00C4800C6CPartVerts,
    _gMappicS202Model00C4800C6CVerts,
    &_gMappicS202Model00C4800C6CVerts[4],
    _gMappicS202Model00C4800C6CSkeleton,
    _gMappicS202Model00C4800C6CStream,
};

static TmdBone _gMappicS202Model00CF800D34Skeleton[1] = {
#include "assets/mappic_s2_02_model_00CF8_00D34_skeleton.inc"
};

static u32 _gMappicS202Model00CF800D34PartVerts[1] = {
#include "assets/mappic_s2_02_model_00CF8_00D34_partVerts.inc"
};

static SVECTOR _gMappicS202Model00CF800D34Verts[8] = {
#include "assets/mappic_s2_02_model_00CF8_00D34_verts.inc"
};

static u32 _gMappicS202Model00CF800D34Stream[15] = {
#include "assets/mappic_s2_02_model_00CF8_00D34_stream.inc"
};

TmdSource D_mappic_s2_02_8012FC64 = {
    0,
    72,
    0,
    1,
    _gMappicS202Model00CF800D34PartVerts,
    _gMappicS202Model00CF800D34Verts,
    &_gMappicS202Model00CF800D34Verts[8],
    _gMappicS202Model00CF800D34Skeleton,
    _gMappicS202Model00CF800D34Stream,
};

static TmdBone _gMappicS202Model00DD0Skeleton[1] = {
#include "assets/mappic_s2_02_model_00DD0_skeleton.inc"
};

static u32 _gMappicS202Model00DD0PartVerts[1] = {
#include "assets/mappic_s2_02_model_00DD0_partVerts.inc"
};

static SVECTOR _gMappicS202Model00DD0Verts[10] = {
#include "assets/mappic_s2_02_model_00DD0_verts.inc"
};

static u32 _gMappicS202Model00DD0Stream[24] = {
#include "assets/mappic_s2_02_model_00DD0_stream.inc"
};

TmdSource D_mappic_s2_02_8012FD60 = {
    0,
    112,
    0,
    1,
    _gMappicS202Model00DD0PartVerts,
    _gMappicS202Model00DD0Verts,
    &_gMappicS202Model00DD0Verts[10],
    _gMappicS202Model00DD0Skeleton,
    _gMappicS202Model00DD0Stream,
};

static TmdBone _gMappicS202Model00C4800EC0Skeleton[1] = {
#include "assets/mappic_s2_02_model_00C48_00EC0_skeleton.inc"
};

static u32 _gMappicS202Model00C4800EC0PartVerts[1] = {
#include "assets/mappic_s2_02_model_00C48_00EC0_partVerts.inc"
};

static SVECTOR _gMappicS202Model00C4800EC0Verts[4] = {
#include "assets/mappic_s2_02_model_00C48_00EC0_verts.inc"
};

static u32 _gMappicS202Model00C4800EC0Stream[9] = {
#include "assets/mappic_s2_02_model_00C48_00EC0_stream.inc"
};

TmdSource D_mappic_s2_02_8012FDF0 = {
    0,
    24,
    0,
    1,
    _gMappicS202Model00C4800EC0PartVerts,
    _gMappicS202Model00C4800EC0Verts,
    &_gMappicS202Model00C4800EC0Verts[4],
    _gMappicS202Model00C4800EC0Skeleton,
    _gMappicS202Model00C4800EC0Stream,
};
