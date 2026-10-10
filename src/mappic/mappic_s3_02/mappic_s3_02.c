#include "mappic/mappic.h"

#include <psyq/libgpu.h>

#include "common.h"

/// Identity transform and self-parented root for the Gas Station shape.
static TmdBone _gMappicS302GasStationShapeSkeleton[] = {
#include "assets/mappic_s4_00_model_0005C_0008C_skeleton.inc"
};

/// Vertex count of the Gas Station shape's sole part (six vertices).
static u32 _gMappicS302GasStationShapePartVerts[] = {
#include "assets/mappic_s4_00_model_0005C_0008C_partVerts.inc"
};

/// Gas Station's map footprint in integer model X/Z coordinates, with Y zero.
static SVECTOR _gMappicS302GasStationShapeVerts[] = {
#include "assets/mappic_s4_00_model_0005C_0008C_verts.inc"
};

/// Two unlit quads' packed vertex byte offsets and white colour, followed by two group ends and the stream end.
static u32 _gMappicS302GasStationShapeStream[] = {
#include "assets/mappic_s4_00_model_0005C_0008C_stream.inc"
};

TmdSource gMappicS302GasStationShape = {
    .bufferHalfBytes  = 2 * sizeof(POLY_F4),
    .partCount        = ARRAY_SIZE(_gMappicS302GasStationShapeSkeleton),
    .partVertexCounts = _gMappicS302GasStationShapePartVerts,
    .verts            = _gMappicS302GasStationShapeVerts,
    .normals          = &_gMappicS302GasStationShapeVerts[ARRAY_SIZE(_gMappicS302GasStationShapeVerts)], // Empty normal pool at the vertex end.
    .skeleton         = _gMappicS302GasStationShapeSkeleton,
    .stream           = _gMappicS302GasStationShapeStream,
};

/// Identity transform and self-parented root for the Main Street shape.
static TmdBone _gMappicS302MainStreetShapeSkeleton[] = {
#include "assets/mappic_s2_02_model_00C48_0011C_skeleton.inc"
};

/// Vertex count of the Main Street shape's sole part (four vertices).
static u32 _gMappicS302MainStreetShapePartVerts[] = {
#include "assets/mappic_s2_02_model_00C48_0011C_partVerts.inc"
};

/// Main Street's map footprint in integer model X/Z coordinates, with Y zero.
static SVECTOR _gMappicS302MainStreetShapeVerts[] = {
#include "assets/mappic_s2_02_model_00C48_0011C_verts.inc"
};

/// One unlit quad's packed vertex byte offsets and white colour, followed by two group ends and the stream end.
static u32 _gMappicS302MainStreetShapeStream[] = {
#include "assets/mappic_s2_02_model_00C48_0011C_stream.inc"
};

TmdSource gMappicS302MainStreetShape = {
    .bufferHalfBytes  = sizeof(POLY_F4),
    .partCount        = ARRAY_SIZE(_gMappicS302MainStreetShapeSkeleton),
    .partVertexCounts = _gMappicS302MainStreetShapePartVerts,
    .verts            = _gMappicS302MainStreetShapeVerts,
    .normals          = &_gMappicS302MainStreetShapeVerts[ARRAY_SIZE(_gMappicS302MainStreetShapeVerts)], // Empty normal pool at the vertex end.
    .skeleton         = _gMappicS302MainStreetShapeSkeleton,
    .stream           = _gMappicS302MainStreetShapeStream,
};

/// Identity transform and self-parented root for the General Store shape.
static TmdBone _gMappicS302GeneralStoreShapeSkeleton[] = {
#include "assets/mappic_s2_02_model_00C48_001AC_skeleton.inc"
};

/// Vertex count of the General Store shape's sole part (four vertices).
static u32 _gMappicS302GeneralStoreShapePartVerts[] = {
#include "assets/mappic_s2_02_model_00C48_001AC_partVerts.inc"
};

/// General Store's map footprint in integer model X/Z coordinates, with Y zero.
static SVECTOR _gMappicS302GeneralStoreShapeVerts[] = {
#include "assets/mappic_s2_02_model_00C48_001AC_verts.inc"
};

/// Packed command stream for the General Store shape's single unlit quad.
///
/// Four SVECTOR byte offsets and white colour; two group ends precede the
/// stream end. The map reads only the first group.
static u32 _gMappicS302GeneralStoreShapeStream[] = {
#include "assets/mappic_s2_02_model_00C48_001AC_stream.inc"
};

TmdSource gMappicS302GeneralStoreShape = {
    .bufferHalfBytes  = sizeof(POLY_F4),
    .partCount        = ARRAY_SIZE(_gMappicS302GeneralStoreShapeSkeleton),
    .partVertexCounts = _gMappicS302GeneralStoreShapePartVerts,
    .verts            = _gMappicS302GeneralStoreShapeVerts,
    .normals          = &_gMappicS302GeneralStoreShapeVerts[ARRAY_SIZE(_gMappicS302GeneralStoreShapeVerts)], // Empty normal pool at the vertex end.
    .skeleton         = _gMappicS302GeneralStoreShapeSkeleton,
    .stream           = _gMappicS302GeneralStoreShapeStream,
};

/// Identity transform and self-parented root for the Back Street shape.
static TmdBone _gMappicS302BackStreetShapeSkeleton[] = {
#include "assets/mappic_s4_00_model_0005C_00258_skeleton.inc"
};

/// Vertex count of the Back Street shape's sole part (six vertices).
static u32 _gMappicS302BackStreetShapePartVerts[] = {
#include "assets/mappic_s4_00_model_0005C_00258_partVerts.inc"
};

/// Back Street's map footprint in integer model X/Z coordinates, with Y zero.
static SVECTOR _gMappicS302BackStreetShapeVerts[] = {
#include "assets/mappic_s4_00_model_0005C_00258_verts.inc"
};

/// Packed command stream for the Back Street shape's two unlit quads.
///
/// Four SVECTOR byte offsets and white colour per quad; two group ends precede
/// the stream end. The map reads only the first group.
static u32 _gMappicS302BackStreetShapeStream[] = {
#include "assets/mappic_s4_00_model_0005C_00258_stream.inc"
};

TmdSource gMappicS302BackStreetShape = {
    .bufferHalfBytes  = 2 * sizeof(POLY_F4),
    .partCount        = ARRAY_SIZE(_gMappicS302BackStreetShapeSkeleton),
    .partVertexCounts = _gMappicS302BackStreetShapePartVerts,
    .verts            = _gMappicS302BackStreetShapeVerts,
    .normals          = &_gMappicS302BackStreetShapeVerts[ARRAY_SIZE(_gMappicS302BackStreetShapeVerts)], // Empty normal pool at the vertex end.
    .skeleton         = _gMappicS302BackStreetShapeSkeleton,
    .stream           = _gMappicS302BackStreetShapeStream,
};

static TmdBone _gMappicS302MappicS202Model00C48002E8Skeleton[1] = {
#include "assets/mappic_s2_02_model_00C48_002E8_skeleton.inc"
};

static u32 _gMappicS302MappicS202Model00C48002E8PartVerts[1] = {
#include "assets/mappic_s2_02_model_00C48_002E8_partVerts.inc"
};

static SVECTOR _gMappicS302MappicS202Model00C48002E8Verts[4] = {
#include "assets/mappic_s2_02_model_00C48_002E8_verts.inc"
};

static u32 _gMappicS302MappicS202Model00C48002E8Stream[9] = {
#include "assets/mappic_s2_02_model_00C48_002E8_stream.inc"
};

TmdSource D_mappic_s3_02_8012F218 = {
    0,
    24,
    0,
    1,
    _gMappicS302MappicS202Model00C48002E8PartVerts,
    _gMappicS302MappicS202Model00C48002E8Verts,
    &_gMappicS302MappicS202Model00C48002E8Verts[4],
    _gMappicS302MappicS202Model00C48002E8Skeleton,
    _gMappicS302MappicS202Model00C48002E8Stream,
};

static TmdBone _gMappicS302MappicS202Model00C4800378Skeleton[1] = {
#include "assets/mappic_s2_02_model_00C48_00378_skeleton.inc"
};

static u32 _gMappicS302MappicS202Model00C4800378PartVerts[1] = {
#include "assets/mappic_s2_02_model_00C48_00378_partVerts.inc"
};

static SVECTOR _gMappicS302MappicS202Model00C4800378Verts[4] = {
#include "assets/mappic_s2_02_model_00C48_00378_verts.inc"
};

static u32 _gMappicS302MappicS202Model00C4800378Stream[9] = {
#include "assets/mappic_s2_02_model_00C48_00378_stream.inc"
};

TmdSource D_mappic_s3_02_8012F2A8 = {
    0,
    24,
    0,
    1,
    _gMappicS302MappicS202Model00C4800378PartVerts,
    _gMappicS302MappicS202Model00C4800378Verts,
    &_gMappicS302MappicS202Model00C4800378Verts[4],
    _gMappicS302MappicS202Model00C4800378Skeleton,
    _gMappicS302MappicS202Model00C4800378Stream,
};

static TmdBone _gMappicS302Model003F4Skeleton[1] = {
#include "assets/mappic_s3_02_model_003F4_skeleton.inc"
};

static u32 _gMappicS302Model003F4PartVerts[1] = {
#include "assets/mappic_s3_02_model_003F4_partVerts.inc"
};

static SVECTOR _gMappicS302Model003F4Verts[6] = {
#include "assets/mappic_s3_02_model_003F4_verts.inc"
};

static u32 _gMappicS302Model003F4Stream[18] = {
#include "assets/mappic_s3_02_model_003F4_stream.inc"
};

TmdSource D_mappic_s3_02_8012F36C = {
    0,
    80,
    0,
    1,
    _gMappicS302Model003F4PartVerts,
    _gMappicS302Model003F4Verts,
    &_gMappicS302Model003F4Verts[6],
    _gMappicS302Model003F4Skeleton,
    _gMappicS302Model003F4Stream,
};

static TmdBone _gMappicS302MappicS202Model00C48004B4Skeleton[1] = {
#include "assets/mappic_s2_02_model_00C48_004B4_skeleton.inc"
};

static u32 _gMappicS302MappicS202Model00C48004B4PartVerts[1] = {
#include "assets/mappic_s2_02_model_00C48_004B4_partVerts.inc"
};

static SVECTOR _gMappicS302MappicS202Model00C48004B4Verts[4] = {
#include "assets/mappic_s2_02_model_00C48_004B4_verts.inc"
};

static u32 _gMappicS302MappicS202Model00C48004B4Stream[9] = {
#include "assets/mappic_s2_02_model_00C48_004B4_stream.inc"
};

TmdSource D_mappic_s3_02_8012F3FC = {
    0,
    24,
    0,
    1,
    _gMappicS302MappicS202Model00C48004B4PartVerts,
    _gMappicS302MappicS202Model00C48004B4Verts,
    &_gMappicS302MappicS202Model00C48004B4Verts[4],
    _gMappicS302MappicS202Model00C48004B4Skeleton,
    _gMappicS302MappicS202Model00C48004B4Stream,
};

static TmdBone _gMappicS302MappicS202Model00C4800544Skeleton[1] = {
#include "assets/mappic_s2_02_model_00C48_00544_skeleton.inc"
};

static u32 _gMappicS302MappicS202Model00C4800544PartVerts[1] = {
#include "assets/mappic_s2_02_model_00C48_00544_partVerts.inc"
};

static SVECTOR _gMappicS302MappicS202Model00C4800544Verts[4] = {
#include "assets/mappic_s2_02_model_00C48_00544_verts.inc"
};

static u32 _gMappicS302MappicS202Model00C4800544Stream[9] = {
#include "assets/mappic_s2_02_model_00C48_00544_stream.inc"
};

TmdSource D_mappic_s3_02_8012F48C = {
    0,
    24,
    0,
    1,
    _gMappicS302MappicS202Model00C4800544PartVerts,
    _gMappicS302MappicS202Model00C4800544Verts,
    &_gMappicS302MappicS202Model00C4800544Verts[4],
    _gMappicS302MappicS202Model00C4800544Skeleton,
    _gMappicS302MappicS202Model00C4800544Stream,
};

static TmdBone _gMappicS302MappicS202Model00C48005D4Skeleton[1] = {
#include "assets/mappic_s2_02_model_00C48_005D4_skeleton.inc"
};

static u32 _gMappicS302MappicS202Model00C48005D4PartVerts[1] = {
#include "assets/mappic_s2_02_model_00C48_005D4_partVerts.inc"
};

static SVECTOR _gMappicS302MappicS202Model00C48005D4Verts[4] = {
#include "assets/mappic_s2_02_model_00C48_005D4_verts.inc"
};

static u32 _gMappicS302MappicS202Model00C48005D4Stream[9] = {
#include "assets/mappic_s2_02_model_00C48_005D4_stream.inc"
};

TmdSource D_mappic_s3_02_8012F51C = {
    0,
    24,
    0,
    1,
    _gMappicS302MappicS202Model00C48005D4PartVerts,
    _gMappicS302MappicS202Model00C48005D4Verts,
    &_gMappicS302MappicS202Model00C48005D4Verts[4],
    _gMappicS302MappicS202Model00C48005D4Skeleton,
    _gMappicS302MappicS202Model00C48005D4Stream,
};

static TmdBone _gMappicS302MappicS202Model00C4800664Skeleton[1] = {
#include "assets/mappic_s2_02_model_00C48_00664_skeleton.inc"
};

static u32 _gMappicS302MappicS202Model00C4800664PartVerts[1] = {
#include "assets/mappic_s2_02_model_00C48_00664_partVerts.inc"
};

static SVECTOR _gMappicS302MappicS202Model00C4800664Verts[4] = {
#include "assets/mappic_s2_02_model_00C48_00664_verts.inc"
};

static u32 _gMappicS302MappicS202Model00C4800664Stream[9] = {
#include "assets/mappic_s2_02_model_00C48_00664_stream.inc"
};

TmdSource D_mappic_s3_02_8012F5AC = {
    0,
    24,
    0,
    1,
    _gMappicS302MappicS202Model00C4800664PartVerts,
    _gMappicS302MappicS202Model00C4800664Verts,
    &_gMappicS302MappicS202Model00C4800664Verts[4],
    _gMappicS302MappicS202Model00C4800664Skeleton,
    _gMappicS302MappicS202Model00C4800664Stream,
};

static TmdBone _gMappicS302MappicS202Model00710Skeleton[1] = {
#include "assets/mappic_s2_02_model_00710_skeleton.inc"
};

static u32 _gMappicS302MappicS202Model00710PartVerts[1] = {
#include "assets/mappic_s2_02_model_00710_partVerts.inc"
};

static SVECTOR _gMappicS302MappicS202Model00710Verts[12] = {
#include "assets/mappic_s2_02_model_00710_verts.inc"
};

static u32 _gMappicS302MappicS202Model00710Stream[21] = {
#include "assets/mappic_s2_02_model_00710_stream.inc"
};

TmdSource D_mappic_s3_02_8012F6AC = {
    0,
    120,
    0,
    1,
    _gMappicS302MappicS202Model00710PartVerts,
    _gMappicS302MappicS202Model00710Verts,
    &_gMappicS302MappicS202Model00710Verts[12],
    _gMappicS302MappicS202Model00710Skeleton,
    _gMappicS302MappicS202Model00710Stream,
};

static TmdBone _gMappicS302MappicS202Model00C48007F4Skeleton[1] = {
#include "assets/mappic_s2_02_model_00C48_007F4_skeleton.inc"
};

static u32 _gMappicS302MappicS202Model00C48007F4PartVerts[1] = {
#include "assets/mappic_s2_02_model_00C48_007F4_partVerts.inc"
};

static SVECTOR _gMappicS302MappicS202Model00C48007F4Verts[4] = {
#include "assets/mappic_s2_02_model_00C48_007F4_verts.inc"
};

static u32 _gMappicS302MappicS202Model00C48007F4Stream[9] = {
#include "assets/mappic_s2_02_model_00C48_007F4_stream.inc"
};

TmdSource D_mappic_s3_02_8012F73C = {
    0,
    24,
    0,
    1,
    _gMappicS302MappicS202Model00C48007F4PartVerts,
    _gMappicS302MappicS202Model00C48007F4Verts,
    &_gMappicS302MappicS202Model00C48007F4Verts[4],
    _gMappicS302MappicS202Model00C48007F4Skeleton,
    _gMappicS302MappicS202Model00C48007F4Stream,
};

static TmdBone _gMappicS302MappicS202Model00C4800884Skeleton[1] = {
#include "assets/mappic_s2_02_model_00C48_00884_skeleton.inc"
};

static u32 _gMappicS302MappicS202Model00C4800884PartVerts[1] = {
#include "assets/mappic_s2_02_model_00C48_00884_partVerts.inc"
};

static SVECTOR _gMappicS302MappicS202Model00C4800884Verts[4] = {
#include "assets/mappic_s2_02_model_00C48_00884_verts.inc"
};

static u32 _gMappicS302MappicS202Model00C4800884Stream[9] = {
#include "assets/mappic_s2_02_model_00C48_00884_stream.inc"
};

TmdSource D_mappic_s3_02_8012F7CC = {
    0,
    24,
    0,
    1,
    _gMappicS302MappicS202Model00C4800884PartVerts,
    _gMappicS302MappicS202Model00C4800884Verts,
    &_gMappicS302MappicS202Model00C4800884Verts[4],
    _gMappicS302MappicS202Model00C4800884Skeleton,
    _gMappicS302MappicS202Model00C4800884Stream,
};

static TmdBone _gMappicS302MappicS202Model00910Skeleton[1] = {
#include "assets/mappic_s2_02_model_00910_skeleton.inc"
};

static u32 _gMappicS302MappicS202Model00910PartVerts[1] = {
#include "assets/mappic_s2_02_model_00910_partVerts.inc"
};

static SVECTOR _gMappicS302MappicS202Model00910Verts[8] = {
#include "assets/mappic_s2_02_model_00910_verts.inc"
};

static u32 _gMappicS302MappicS202Model00910Stream[21] = {
#include "assets/mappic_s2_02_model_00910_stream.inc"
};

TmdSource D_mappic_s3_02_8012F8AC = {
    0,
    88,
    0,
    1,
    _gMappicS302MappicS202Model00910PartVerts,
    _gMappicS302MappicS202Model00910Verts,
    &_gMappicS302MappicS202Model00910Verts[8],
    _gMappicS302MappicS202Model00910Skeleton,
    _gMappicS302MappicS202Model00910Stream,
};

static TmdBone _gMappicS302MappicS202Model00C48009F4Skeleton[1] = {
#include "assets/mappic_s2_02_model_00C48_009F4_skeleton.inc"
};

static u32 _gMappicS302MappicS202Model00C48009F4PartVerts[1] = {
#include "assets/mappic_s2_02_model_00C48_009F4_partVerts.inc"
};

static SVECTOR _gMappicS302MappicS202Model00C48009F4Verts[4] = {
#include "assets/mappic_s2_02_model_00C48_009F4_verts.inc"
};

static u32 _gMappicS302MappicS202Model00C48009F4Stream[9] = {
#include "assets/mappic_s2_02_model_00C48_009F4_stream.inc"
};

TmdSource D_mappic_s3_02_8012F93C = {
    0,
    24,
    0,
    1,
    _gMappicS302MappicS202Model00C48009F4PartVerts,
    _gMappicS302MappicS202Model00C48009F4Verts,
    &_gMappicS302MappicS202Model00C48009F4Verts[4],
    _gMappicS302MappicS202Model00C48009F4Skeleton,
    _gMappicS302MappicS202Model00C48009F4Stream,
};

static TmdBone _gMappicS302MappicS400Model0005C00AA0Skeleton[1] = {
#include "assets/mappic_s4_00_model_0005C_00AA0_skeleton.inc"
};

static u32 _gMappicS302MappicS400Model0005C00AA0PartVerts[1] = {
#include "assets/mappic_s4_00_model_0005C_00AA0_partVerts.inc"
};

static SVECTOR _gMappicS302MappicS400Model0005C00AA0Verts[6] = {
#include "assets/mappic_s4_00_model_0005C_00AA0_verts.inc"
};

static u32 _gMappicS302MappicS400Model0005C00AA0Stream[12] = {
#include "assets/mappic_s4_00_model_0005C_00AA0_stream.inc"
};

TmdSource D_mappic_s3_02_8012F9E8 = {
    0,
    48,
    0,
    1,
    _gMappicS302MappicS400Model0005C00AA0PartVerts,
    _gMappicS302MappicS400Model0005C00AA0Verts,
    &_gMappicS302MappicS400Model0005C00AA0Verts[6],
    _gMappicS302MappicS400Model0005C00AA0Skeleton,
    _gMappicS302MappicS400Model0005C00AA0Stream,
};

static TmdBone _gMappicS302MappicS400Model0005C00B4CSkeleton[1] = {
#include "assets/mappic_s4_00_model_0005C_00B4C_skeleton.inc"
};

static u32 _gMappicS302MappicS400Model0005C00B4CPartVerts[1] = {
#include "assets/mappic_s4_00_model_0005C_00B4C_partVerts.inc"
};

static SVECTOR _gMappicS302MappicS400Model0005C00B4CVerts[6] = {
#include "assets/mappic_s4_00_model_0005C_00B4C_verts.inc"
};

static u32 _gMappicS302MappicS400Model0005C00B4CStream[12] = {
#include "assets/mappic_s4_00_model_0005C_00B4C_stream.inc"
};

TmdSource D_mappic_s3_02_8012FA94 = {
    0,
    48,
    0,
    1,
    _gMappicS302MappicS400Model0005C00B4CPartVerts,
    _gMappicS302MappicS400Model0005C00B4CVerts,
    &_gMappicS302MappicS400Model0005C00B4CVerts[6],
    _gMappicS302MappicS400Model0005C00B4CSkeleton,
    _gMappicS302MappicS400Model0005C00B4CStream,
};

static TmdBone _gMappicS302MappicS202Model00C4800BDCSkeleton[1] = {
#include "assets/mappic_s2_02_model_00C48_00BDC_skeleton.inc"
};

static u32 _gMappicS302MappicS202Model00C4800BDCPartVerts[1] = {
#include "assets/mappic_s2_02_model_00C48_00BDC_partVerts.inc"
};

static SVECTOR _gMappicS302MappicS202Model00C4800BDCVerts[4] = {
#include "assets/mappic_s2_02_model_00C48_00BDC_verts.inc"
};

static u32 _gMappicS302MappicS202Model00C4800BDCStream[9] = {
#include "assets/mappic_s2_02_model_00C48_00BDC_stream.inc"
};

TmdSource D_mappic_s3_02_8012FB24 = {
    0,
    24,
    0,
    1,
    _gMappicS302MappicS202Model00C4800BDCPartVerts,
    _gMappicS302MappicS202Model00C4800BDCVerts,
    &_gMappicS302MappicS202Model00C4800BDCVerts[4],
    _gMappicS302MappicS202Model00C4800BDCSkeleton,
    _gMappicS302MappicS202Model00C4800BDCStream,
};

static TmdBone _gMappicS302MappicS202Model00C4800C6CSkeleton[1] = {
#include "assets/mappic_s2_02_model_00C48_00C6C_skeleton.inc"
};

static u32 _gMappicS302MappicS202Model00C4800C6CPartVerts[1] = {
#include "assets/mappic_s2_02_model_00C48_00C6C_partVerts.inc"
};

static SVECTOR _gMappicS302MappicS202Model00C4800C6CVerts[4] = {
#include "assets/mappic_s2_02_model_00C48_00C6C_verts.inc"
};

static u32 _gMappicS302MappicS202Model00C4800C6CStream[9] = {
#include "assets/mappic_s2_02_model_00C48_00C6C_stream.inc"
};

TmdSource D_mappic_s3_02_8012FBB4 = {
    0,
    24,
    0,
    1,
    _gMappicS302MappicS202Model00C4800C6CPartVerts,
    _gMappicS302MappicS202Model00C4800C6CVerts,
    &_gMappicS302MappicS202Model00C4800C6CVerts[4],
    _gMappicS302MappicS202Model00C4800C6CSkeleton,
    _gMappicS302MappicS202Model00C4800C6CStream,
};

static TmdBone _gMappicS302MappicS202Model00CF800D34Skeleton[1] = {
#include "assets/mappic_s2_02_model_00CF8_00D34_skeleton.inc"
};

static u32 _gMappicS302MappicS202Model00CF800D34PartVerts[1] = {
#include "assets/mappic_s2_02_model_00CF8_00D34_partVerts.inc"
};

static SVECTOR _gMappicS302MappicS202Model00CF800D34Verts[8] = {
#include "assets/mappic_s2_02_model_00CF8_00D34_verts.inc"
};

static u32 _gMappicS302MappicS202Model00CF800D34Stream[15] = {
#include "assets/mappic_s2_02_model_00CF8_00D34_stream.inc"
};

TmdSource D_mappic_s3_02_8012FC7C = {
    0,
    72,
    0,
    1,
    _gMappicS302MappicS202Model00CF800D34PartVerts,
    _gMappicS302MappicS202Model00CF800D34Verts,
    &_gMappicS302MappicS202Model00CF800D34Verts[8],
    _gMappicS302MappicS202Model00CF800D34Skeleton,
    _gMappicS302MappicS202Model00CF800D34Stream,
};

static TmdBone _gMappicS302MappicS202Model00DD0Skeleton[1] = {
#include "assets/mappic_s2_02_model_00DD0_skeleton.inc"
};

static u32 _gMappicS302MappicS202Model00DD0PartVerts[1] = {
#include "assets/mappic_s2_02_model_00DD0_partVerts.inc"
};

static SVECTOR _gMappicS302MappicS202Model00DD0Verts[10] = {
#include "assets/mappic_s2_02_model_00DD0_verts.inc"
};

static u32 _gMappicS302MappicS202Model00DD0Stream[24] = {
#include "assets/mappic_s2_02_model_00DD0_stream.inc"
};

TmdSource D_mappic_s3_02_8012FD78 = {
    0,
    112,
    0,
    1,
    _gMappicS302MappicS202Model00DD0PartVerts,
    _gMappicS302MappicS202Model00DD0Verts,
    &_gMappicS302MappicS202Model00DD0Verts[10],
    _gMappicS302MappicS202Model00DD0Skeleton,
    _gMappicS302MappicS202Model00DD0Stream,
};

static TmdBone _gMappicS302MappicS202Model00C4800EC0Skeleton[1] = {
#include "assets/mappic_s2_02_model_00C48_00EC0_skeleton.inc"
};

static u32 _gMappicS302MappicS202Model00C4800EC0PartVerts[1] = {
#include "assets/mappic_s2_02_model_00C48_00EC0_partVerts.inc"
};

static SVECTOR _gMappicS302MappicS202Model00C4800EC0Verts[4] = {
#include "assets/mappic_s2_02_model_00C48_00EC0_verts.inc"
};

static u32 _gMappicS302MappicS202Model00C4800EC0Stream[9] = {
#include "assets/mappic_s2_02_model_00C48_00EC0_stream.inc"
};

TmdSource D_mappic_s3_02_8012FE08 = {
    0,
    24,
    0,
    1,
    _gMappicS302MappicS202Model00C4800EC0PartVerts,
    _gMappicS302MappicS202Model00C4800EC0Verts,
    &_gMappicS302MappicS202Model00C4800EC0Verts[4],
    _gMappicS302MappicS202Model00C4800EC0Skeleton,
    _gMappicS302MappicS202Model00C4800EC0Stream,
};
