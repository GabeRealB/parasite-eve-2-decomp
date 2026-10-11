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

/// Identity transform and self-parented root for the Souvenir Shop shape.
static TmdBone _gMappicS302SouvenirShopShapeSkeleton[] = {
#include "assets/mappic_s2_02_model_00C48_002E8_skeleton.inc"
};

/// Vertex count of the Souvenir Shop shape's sole part (four vertices).
static u32 _gMappicS302SouvenirShopShapePartVerts[] = {
#include "assets/mappic_s2_02_model_00C48_002E8_partVerts.inc"
};

/// Souvenir Shop's map footprint in integer model X/Z coordinates, with Y zero.
static SVECTOR _gMappicS302SouvenirShopShapeVerts[] = {
#include "assets/mappic_s2_02_model_00C48_002E8_verts.inc"
};

/// Packed command stream for the Souvenir Shop shape's single unlit quad.
///
/// Four SVECTOR byte offsets and white colour; two group ends precede the
/// stream end. The map reads only the first group.
static u32 _gMappicS302SouvenirShopShapeStream[] = {
#include "assets/mappic_s2_02_model_00C48_002E8_stream.inc"
};

TmdSource gMappicS302SouvenirShopShape = {
    .bufferHalfBytes  = sizeof(POLY_F4),
    .partCount        = ARRAY_SIZE(_gMappicS302SouvenirShopShapeSkeleton),
    .partVertexCounts = _gMappicS302SouvenirShopShapePartVerts,
    .verts            = _gMappicS302SouvenirShopShapeVerts,
    .normals          = &_gMappicS302SouvenirShopShapeVerts[ARRAY_SIZE(_gMappicS302SouvenirShopShapeVerts)], // Empty normal pool at the vertex end.
    .skeleton         = _gMappicS302SouvenirShopShapeSkeleton,
    .stream           = _gMappicS302SouvenirShopShapeStream,
};

/// Identity transform and self-parented root for the Warehouse shape.
static TmdBone _gMappicS302WarehouseShapeSkeleton[] = {
#include "assets/mappic_s2_02_model_00C48_00378_skeleton.inc"
};

/// Vertex count of the Warehouse shape's sole part (four vertices).
static u32 _gMappicS302WarehouseShapePartVerts[] = {
#include "assets/mappic_s2_02_model_00C48_00378_partVerts.inc"
};

/// Warehouse's map footprint in integer model X/Z coordinates, with Y zero.
static SVECTOR _gMappicS302WarehouseShapeVerts[] = {
#include "assets/mappic_s2_02_model_00C48_00378_verts.inc"
};

/// Packed command stream for the Warehouse shape's single unlit quad.
///
/// Four SVECTOR byte offsets and white colour; two group ends precede the
/// stream end. The map reads only the first group.
static u32 _gMappicS302WarehouseShapeStream[] = {
#include "assets/mappic_s2_02_model_00C48_00378_stream.inc"
};

TmdSource gMappicS302WarehouseShape = {
    .bufferHalfBytes  = sizeof(POLY_F4),
    .partCount        = ARRAY_SIZE(_gMappicS302WarehouseShapeSkeleton),
    .partVertexCounts = _gMappicS302WarehouseShapePartVerts,
    .verts            = _gMappicS302WarehouseShapeVerts,
    .normals          = &_gMappicS302WarehouseShapeVerts[ARRAY_SIZE(_gMappicS302WarehouseShapeVerts)], // Empty normal pool at the vertex end.
    .skeleton         = _gMappicS302WarehouseShapeSkeleton,
    .stream           = _gMappicS302WarehouseShapeStream,
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

/// Identity transform and self-parented root for the Motel room 1 shape.
static TmdBone _gMappicS302MotelRoom1ShapeSkeleton[] = {
#include "assets/mappic_s2_02_model_00C48_004B4_skeleton.inc"
};

/// Vertex count of the Motel room 1 shape's sole part (four vertices).
static u32 _gMappicS302MotelRoom1ShapePartVerts[] = {
#include "assets/mappic_s2_02_model_00C48_004B4_partVerts.inc"
};

/// Motel room 1's map footprint in integer model X/Z coordinates, with Y zero.
static SVECTOR _gMappicS302MotelRoom1ShapeVerts[] = {
#include "assets/mappic_s2_02_model_00C48_004B4_verts.inc"
};

/// Packed command stream for the Motel room 1 shape's single unlit quad.
///
/// Four SVECTOR byte offsets and white colour; two group ends precede the
/// stream end. The map reads only the first group.
static u32 _gMappicS302MotelRoom1ShapeStream[] = {
#include "assets/mappic_s2_02_model_00C48_004B4_stream.inc"
};

TmdSource gMappicS302MotelRoom1Shape = {
    .bufferHalfBytes  = sizeof(POLY_F4),
    .partCount        = ARRAY_SIZE(_gMappicS302MotelRoom1ShapeSkeleton),
    .partVertexCounts = _gMappicS302MotelRoom1ShapePartVerts,
    .verts            = _gMappicS302MotelRoom1ShapeVerts,
    .normals          = &_gMappicS302MotelRoom1ShapeVerts[ARRAY_SIZE(_gMappicS302MotelRoom1ShapeVerts)], // Empty normal pool at the vertex end.
    .skeleton         = _gMappicS302MotelRoom1ShapeSkeleton,
    .stream           = _gMappicS302MotelRoom1ShapeStream,
};

/// Identity transform and self-parented root for the Motel room 2 shape.
static TmdBone _gMappicS302MotelRoom2ShapeSkeleton[] = {
#include "assets/mappic_s2_02_model_00C48_00544_skeleton.inc"
};

/// Vertex count of the Motel room 2 shape's sole part (four vertices).
static u32 _gMappicS302MotelRoom2ShapePartVerts[] = {
#include "assets/mappic_s2_02_model_00C48_00544_partVerts.inc"
};

/// Motel room 2's map footprint in integer model X/Z coordinates, with Y zero.
static SVECTOR _gMappicS302MotelRoom2ShapeVerts[] = {
#include "assets/mappic_s2_02_model_00C48_00544_verts.inc"
};

/// Packed command stream for the Motel room 2 shape's single unlit quad.
///
/// Four SVECTOR byte offsets and white colour; two group ends precede the
/// stream end. The map reads only the first group.
static u32 _gMappicS302MotelRoom2ShapeStream[] = {
#include "assets/mappic_s2_02_model_00C48_00544_stream.inc"
};

TmdSource gMappicS302MotelRoom2Shape = {
    .bufferHalfBytes  = sizeof(POLY_F4),
    .partCount        = ARRAY_SIZE(_gMappicS302MotelRoom2ShapeSkeleton),
    .partVertexCounts = _gMappicS302MotelRoom2ShapePartVerts,
    .verts            = _gMappicS302MotelRoom2ShapeVerts,
    .normals          = &_gMappicS302MotelRoom2ShapeVerts[ARRAY_SIZE(_gMappicS302MotelRoom2ShapeVerts)], // Empty normal pool at the vertex end.
    .skeleton         = _gMappicS302MotelRoom2ShapeSkeleton,
    .stream           = _gMappicS302MotelRoom2ShapeStream,
};

/// Identity transform and self-parented root for the Motel room 3 shape.
static TmdBone _gMappicS302MotelRoom3ShapeSkeleton[] = {
#include "assets/mappic_s2_02_model_00C48_005D4_skeleton.inc"
};

/// Vertex count of the Motel room 3 shape's sole part (four vertices).
static u32 _gMappicS302MotelRoom3ShapePartVerts[] = {
#include "assets/mappic_s2_02_model_00C48_005D4_partVerts.inc"
};

/// Motel room 3's map footprint in integer model X/Z coordinates, with Y zero.
static SVECTOR _gMappicS302MotelRoom3ShapeVerts[] = {
#include "assets/mappic_s2_02_model_00C48_005D4_verts.inc"
};

/// Packed command stream for the Motel room 3 shape's single unlit quad.
///
/// Four SVECTOR byte offsets and white colour; two group ends precede the
/// stream end. The map reads only the first group.
static u32 _gMappicS302MotelRoom3ShapeStream[] = {
#include "assets/mappic_s2_02_model_00C48_005D4_stream.inc"
};

TmdSource gMappicS302MotelRoom3Shape = {
    .bufferHalfBytes  = sizeof(POLY_F4),
    .partCount        = ARRAY_SIZE(_gMappicS302MotelRoom3ShapeSkeleton),
    .partVertexCounts = _gMappicS302MotelRoom3ShapePartVerts,
    .verts            = _gMappicS302MotelRoom3ShapeVerts,
    .normals          = &_gMappicS302MotelRoom3ShapeVerts[ARRAY_SIZE(_gMappicS302MotelRoom3ShapeVerts)], // Empty normal pool at the vertex end.
    .skeleton         = _gMappicS302MotelRoom3ShapeSkeleton,
    .stream           = _gMappicS302MotelRoom3ShapeStream,
};

/// Identity transform and self-parented root for the Motel room 4 shape.
static TmdBone _gMappicS302MotelRoom4ShapeSkeleton[] = {
#include "assets/mappic_s2_02_model_00C48_00664_skeleton.inc"
};

/// Vertex count of the Motel room 4 shape's sole part (four vertices).
static u32 _gMappicS302MotelRoom4ShapePartVerts[] = {
#include "assets/mappic_s2_02_model_00C48_00664_partVerts.inc"
};

/// Motel room 4's map footprint in integer model X/Z coordinates, with Y zero.
static SVECTOR _gMappicS302MotelRoom4ShapeVerts[] = {
#include "assets/mappic_s2_02_model_00C48_00664_verts.inc"
};

/// Packed command stream for the Motel room 4 shape's single unlit quad.
///
/// Four SVECTOR byte offsets and white colour; two group ends precede the
/// stream end. The map reads only the first group.
static u32 _gMappicS302MotelRoom4ShapeStream[] = {
#include "assets/mappic_s2_02_model_00C48_00664_stream.inc"
};

TmdSource gMappicS302MotelRoom4Shape = {
    .bufferHalfBytes  = sizeof(POLY_F4),
    .partCount        = ARRAY_SIZE(_gMappicS302MotelRoom4ShapeSkeleton),
    .partVertexCounts = _gMappicS302MotelRoom4ShapePartVerts,
    .verts            = _gMappicS302MotelRoom4ShapeVerts,
    .normals          = &_gMappicS302MotelRoom4ShapeVerts[ARRAY_SIZE(_gMappicS302MotelRoom4ShapeVerts)], // Empty normal pool at the vertex end.
    .skeleton         = _gMappicS302MotelRoom4ShapeSkeleton,
    .stream           = _gMappicS302MotelRoom4ShapeStream,
};

/// Identity transform and self-parented root for the Parking lot shape.
static TmdBone _gMappicS302ParkingLotShapeSkeleton[] = {
#include "assets/mappic_s2_02_model_00710_skeleton.inc"
};

/// Vertex count of the Parking lot shape's sole part (twelve vertices).
static u32 _gMappicS302ParkingLotShapePartVerts[] = {
#include "assets/mappic_s2_02_model_00710_partVerts.inc"
};

/// Parking lot's map footprint in integer model X/Z coordinates, with Y zero.
static SVECTOR _gMappicS302ParkingLotShapeVerts[] = {
#include "assets/mappic_s2_02_model_00710_verts.inc"
};

/// Packed command stream for the Parking lot shape's five unlit quads.
///
/// Each quad has four SVECTOR byte offsets and white colour; two group ends
/// precede the stream end. The map reads only the first group.
static u32 _gMappicS302ParkingLotShapeStream[] = {
#include "assets/mappic_s2_02_model_00710_stream.inc"
};

TmdSource gMappicS302ParkingLotShape = {
    .bufferHalfBytes  = 5 * sizeof(POLY_F4),
    .partCount        = ARRAY_SIZE(_gMappicS302ParkingLotShapeSkeleton),
    .partVertexCounts = _gMappicS302ParkingLotShapePartVerts,
    .verts            = _gMappicS302ParkingLotShapeVerts,
    .normals          = &_gMappicS302ParkingLotShapeVerts[ARRAY_SIZE(_gMappicS302ParkingLotShapeVerts)], // Empty normal pool at the vertex end.
    .skeleton         = _gMappicS302ParkingLotShapeSkeleton,
    .stream           = _gMappicS302ParkingLotShapeStream,
};

/// Identity transform and self-parented root for the Toilet shape.
static TmdBone _gMappicS302ToiletShapeSkeleton[] = {
#include "assets/mappic_s2_02_model_00C48_007F4_skeleton.inc"
};

/// Vertex count of the Toilet shape's sole part (four vertices).
static u32 _gMappicS302ToiletShapePartVerts[] = {
#include "assets/mappic_s2_02_model_00C48_007F4_partVerts.inc"
};

/// Toilet's map footprint in integer model X/Z coordinates, with Y zero.
static SVECTOR _gMappicS302ToiletShapeVerts[] = {
#include "assets/mappic_s2_02_model_00C48_007F4_verts.inc"
};

/// Packed command stream for the Toilet shape's unlit quad.
///
/// Four SVECTOR byte offsets and white colour precede two group ends and
/// the stream end. The map reads only the first group.
static u32 _gMappicS302ToiletShapeStream[] = {
#include "assets/mappic_s2_02_model_00C48_007F4_stream.inc"
};

TmdSource gMappicS302ToiletShape = {
    .bufferHalfBytes  = sizeof(POLY_F4),
    .partCount        = ARRAY_SIZE(_gMappicS302ToiletShapeSkeleton),
    .partVertexCounts = _gMappicS302ToiletShapePartVerts,
    .verts            = _gMappicS302ToiletShapeVerts,
    .normals          = &_gMappicS302ToiletShapeVerts[ARRAY_SIZE(_gMappicS302ToiletShapeVerts)], // Empty normal pool at the vertex end.
    .skeleton         = _gMappicS302ToiletShapeSkeleton,
    .stream           = _gMappicS302ToiletShapeStream,
};

/// Identity transform and self-parented root for the Motel lobby shape.
static TmdBone _gMappicS302MotelLobbyShapeSkeleton[] = {
#include "assets/mappic_s2_02_model_00C48_00884_skeleton.inc"
};

/// Vertex count of the Motel lobby shape's sole part (four vertices).
static u32 _gMappicS302MotelLobbyShapePartVerts[] = {
#include "assets/mappic_s2_02_model_00C48_00884_partVerts.inc"
};

/// Motel lobby's map footprint in integer model X/Z coordinates, with Y zero.
static SVECTOR _gMappicS302MotelLobbyShapeVerts[] = {
#include "assets/mappic_s2_02_model_00C48_00884_verts.inc"
};

/// Packed command stream for the Motel lobby shape's single unlit quad.
///
/// Four SVECTOR byte offsets and white colour; two group ends precede the
/// stream end. The map reads only the first group.
static u32 _gMappicS302MotelLobbyShapeStream[] = {
#include "assets/mappic_s2_02_model_00C48_00884_stream.inc"
};

TmdSource gMappicS302MotelLobbyShape = {
    .bufferHalfBytes  = sizeof(POLY_F4),
    .partCount        = ARRAY_SIZE(_gMappicS302MotelLobbyShapeSkeleton),
    .partVertexCounts = _gMappicS302MotelLobbyShapePartVerts,
    .verts            = _gMappicS302MotelLobbyShapeVerts,
    .normals          = &_gMappicS302MotelLobbyShapeVerts[ARRAY_SIZE(_gMappicS302MotelLobbyShapeVerts)], // Empty normal pool at the vertex end.
    .skeleton         = _gMappicS302MotelLobbyShapeSkeleton,
    .stream           = _gMappicS302MotelLobbyShapeStream,
};

/// Identity transform and self-parented root for the Saloon G & R shape.
static TmdBone _gMappicS302SaloonGRShapeSkeleton[] = {
#include "assets/mappic_s2_02_model_00910_skeleton.inc"
};

/// Vertex count of the Saloon G & R shape's sole part (eight vertices).
static u32 _gMappicS302SaloonGRShapePartVerts[] = {
#include "assets/mappic_s2_02_model_00910_partVerts.inc"
};

/// Saloon G & R's map footprint in integer model X/Z coordinates, with Y zero.
static SVECTOR _gMappicS302SaloonGRShapeVerts[] = {
#include "assets/mappic_s2_02_model_00910_verts.inc"
};

/// Packed command stream for the Saloon G & R shape's two unlit triangles and two unlit quads.
///
/// Elements contain SVECTOR byte offsets and white colour; two group ends
/// precede the stream end. The map reads only the first group.
static u32 _gMappicS302SaloonGRShapeStream[] = {
#include "assets/mappic_s2_02_model_00910_stream.inc"
};

TmdSource gMappicS302SaloonGRShape = {
    .bufferHalfBytes  = 2 * sizeof(POLY_F3) + 2 * sizeof(POLY_F4),
    .partCount        = ARRAY_SIZE(_gMappicS302SaloonGRShapeSkeleton),
    .partVertexCounts = _gMappicS302SaloonGRShapePartVerts,
    .verts            = _gMappicS302SaloonGRShapeVerts,
    .normals          = &_gMappicS302SaloonGRShapeVerts[ARRAY_SIZE(_gMappicS302SaloonGRShapeVerts)], // Empty normal pool at the vertex end.
    .skeleton         = _gMappicS302SaloonGRShapeSkeleton,
    .stream           = _gMappicS302SaloonGRShapeStream,
};

/// Identity transform and self-parented root for the G & R Kitchen shape.
static TmdBone _gMappicS302GRKitchenShapeSkeleton[] = {
#include "assets/mappic_s2_02_model_00C48_009F4_skeleton.inc"
};

/// Vertex count of the G & R Kitchen shape's sole part (four vertices).
static u32 _gMappicS302GRKitchenShapePartVerts[] = {
#include "assets/mappic_s2_02_model_00C48_009F4_partVerts.inc"
};

/// G & R Kitchen's map footprint in integer model X/Z coordinates, with Y zero.
static SVECTOR _gMappicS302GRKitchenShapeVerts[] = {
#include "assets/mappic_s2_02_model_00C48_009F4_verts.inc"
};

/// Packed command stream for the G & R Kitchen shape's single unlit quad.
///
/// Four SVECTOR byte offsets and white colour; two group ends precede the
/// stream end. The map reads only the first group.
static u32 _gMappicS302GRKitchenShapeStream[] = {
#include "assets/mappic_s2_02_model_00C48_009F4_stream.inc"
};

TmdSource gMappicS302GRKitchenShape = {
    .bufferHalfBytes  = sizeof(POLY_F4),
    .partCount        = ARRAY_SIZE(_gMappicS302GRKitchenShapeSkeleton),
    .partVertexCounts = _gMappicS302GRKitchenShapePartVerts,
    .verts            = _gMappicS302GRKitchenShapeVerts,
    .normals          = &_gMappicS302GRKitchenShapeVerts[ARRAY_SIZE(_gMappicS302GRKitchenShapeVerts)], // Empty normal pool at the vertex end.
    .skeleton         = _gMappicS302GRKitchenShapeSkeleton,
    .stream           = _gMappicS302GRKitchenShapeStream,
};

/// Identity transform and self-parented root for the Water Tower shape.
static TmdBone _gMappicS302WaterTowerShapeSkeleton[] = {
#include "assets/mappic_s4_00_model_0005C_00AA0_skeleton.inc"
};

/// Vertex count of the Water Tower shape's sole part (six vertices).
static u32 _gMappicS302WaterTowerShapePartVerts[] = {
#include "assets/mappic_s4_00_model_0005C_00AA0_partVerts.inc"
};

/// Water Tower's map footprint in integer model X/Z coordinates, with Y zero.
static SVECTOR _gMappicS302WaterTowerShapeVerts[] = {
#include "assets/mappic_s4_00_model_0005C_00AA0_verts.inc"
};

/// Packed command stream for the Water Tower shape's two unlit quads.
///
/// Each quad has four packed SVECTOR byte offsets and white colour; two group
/// ends precede the stream end. The map reads only the first group.
static u32 _gMappicS302WaterTowerShapeStream[] = {
#include "assets/mappic_s4_00_model_0005C_00AA0_stream.inc"
};

TmdSource gMappicS302WaterTowerShape = {
    .bufferHalfBytes  = 2 * sizeof(POLY_F4),
    .partCount        = ARRAY_SIZE(_gMappicS302WaterTowerShapeSkeleton),
    .partVertexCounts = _gMappicS302WaterTowerShapePartVerts,
    .verts            = _gMappicS302WaterTowerShapeVerts,
    .normals          = &_gMappicS302WaterTowerShapeVerts[ARRAY_SIZE(_gMappicS302WaterTowerShapeVerts)], // Empty normal pool at the vertex end.
    .skeleton         = _gMappicS302WaterTowerShapeSkeleton,
    .stream           = _gMappicS302WaterTowerShapeStream,
};

/// Identity transform and self-parented root for the Breezeway shape.
static TmdBone _gMappicS302BreezewayShapeSkeleton[] = {
#include "assets/mappic_s4_00_model_0005C_00B4C_skeleton.inc"
};

/// Vertex count of the Breezeway shape's sole part (six vertices).
static u32 _gMappicS302BreezewayShapePartVerts[] = {
#include "assets/mappic_s4_00_model_0005C_00B4C_partVerts.inc"
};

/// Breezeway's map footprint in integer model X/Z coordinates, with Y zero.
static SVECTOR _gMappicS302BreezewayShapeVerts[] = {
#include "assets/mappic_s4_00_model_0005C_00B4C_verts.inc"
};

/// Packed command stream for the Breezeway shape's two unlit quads.
///
/// Each quad has four packed SVECTOR byte offsets and white colour; two group
/// ends precede the stream end. The map reads only the first group.
static u32 _gMappicS302BreezewayShapeStream[] = {
#include "assets/mappic_s4_00_model_0005C_00B4C_stream.inc"
};

TmdSource gMappicS302BreezewayShape = {
    .bufferHalfBytes  = 2 * sizeof(POLY_F4),
    .partCount        = ARRAY_SIZE(_gMappicS302BreezewayShapeSkeleton),
    .partVertexCounts = _gMappicS302BreezewayShapePartVerts,
    .verts            = _gMappicS302BreezewayShapeVerts,
    .normals          = &_gMappicS302BreezewayShapeVerts[ARRAY_SIZE(_gMappicS302BreezewayShapeVerts)], // Empty normal pool at the vertex end.
    .skeleton         = _gMappicS302BreezewayShapeSkeleton,
    .stream           = _gMappicS302BreezewayShapeStream,
};

/// Identity transform and self-parented root for the Factory shape.
static TmdBone _gMappicS302FactoryShapeSkeleton[] = {
#include "assets/mappic_s2_02_model_00C48_00BDC_skeleton.inc"
};

/// Vertex count of the Factory shape's sole part (four vertices).
static u32 _gMappicS302FactoryShapePartVerts[] = {
#include "assets/mappic_s2_02_model_00C48_00BDC_partVerts.inc"
};

/// Factory's map footprint in integer model X/Z coordinates, with Y zero.
static SVECTOR _gMappicS302FactoryShapeVerts[] = {
#include "assets/mappic_s2_02_model_00C48_00BDC_verts.inc"
};

/// Packed command stream for the Factory shape's single unlit quad.
///
/// Four vertex byte offsets and white colour are followed by two group ends
/// and the stream end. The map reads only the first group.
static u32 _gMappicS302FactoryShapeStream[] = {
#include "assets/mappic_s2_02_model_00C48_00BDC_stream.inc"
};

TmdSource gMappicS302FactoryShape = {
    .bufferHalfBytes  = sizeof(POLY_F4),
    .partCount        = ARRAY_SIZE(_gMappicS302FactoryShapeSkeleton),
    .partVertexCounts = _gMappicS302FactoryShapePartVerts,
    .verts            = _gMappicS302FactoryShapeVerts,
    .normals          = &_gMappicS302FactoryShapeVerts[ARRAY_SIZE(_gMappicS302FactoryShapeVerts)], // Empty normal pool at the vertex end.
    .skeleton         = _gMappicS302FactoryShapeSkeleton,
    .stream           = _gMappicS302FactoryShapeStream,
};

/// Identity transform and self-parented root for the Garage shape.
static TmdBone _gMappicS302GarageShapeSkeleton[] = {
#include "assets/mappic_s2_02_model_00C48_00C6C_skeleton.inc"
};

/// Vertex count of the Garage shape's sole part (four vertices).
static u32 _gMappicS302GarageShapePartVerts[] = {
#include "assets/mappic_s2_02_model_00C48_00C6C_partVerts.inc"
};

/// Garage's map footprint in integer model X/Z coordinates, with Y zero.
static SVECTOR _gMappicS302GarageShapeVerts[] = {
#include "assets/mappic_s2_02_model_00C48_00C6C_verts.inc"
};

/// Packed command stream for the Garage shape's single unlit quad.
///
/// Four vertex byte offsets and white colour are followed by two group ends
/// and the stream end. The map reads only the first group.
static u32 _gMappicS302GarageShapeStream[] = {
#include "assets/mappic_s2_02_model_00C48_00C6C_stream.inc"
};

TmdSource gMappicS302GarageShape = {
    .bufferHalfBytes  = sizeof(POLY_F4),
    .partCount        = ARRAY_SIZE(_gMappicS302GarageShapeSkeleton),
    .partVertexCounts = _gMappicS302GarageShapePartVerts,
    .verts            = _gMappicS302GarageShapeVerts,
    .normals          = &_gMappicS302GarageShapeVerts[ARRAY_SIZE(_gMappicS302GarageShapeVerts)], // Empty normal pool at the vertex end.
    .skeleton         = _gMappicS302GarageShapeSkeleton,
    .stream           = _gMappicS302GarageShapeStream,
};

/// Identity transform and self-parented root for the Driveway shape.
static TmdBone _gMappicS302DrivewayShapeSkeleton[] = {
#include "assets/mappic_s2_02_model_00CF8_00D34_skeleton.inc"
};

/// Vertex count of the Driveway shape's sole part (eight vertices).
static u32 _gMappicS302DrivewayShapePartVerts[] = {
#include "assets/mappic_s2_02_model_00CF8_00D34_partVerts.inc"
};

/// Driveway's map footprint in integer model X/Z coordinates, with Y zero.
static SVECTOR _gMappicS302DrivewayShapeVerts[] = {
#include "assets/mappic_s2_02_model_00CF8_00D34_verts.inc"
};

/// Packed command stream for the Driveway shape's three unlit quads.
///
/// Each quad has four vertex byte offsets and white colour. Two group ends
/// and the stream end follow; the map reads only the first group.
static u32 _gMappicS302DrivewayShapeStream[] = {
#include "assets/mappic_s2_02_model_00CF8_00D34_stream.inc"
};

TmdSource gMappicS302DrivewayShape = {
    .bufferHalfBytes  = 3 * sizeof(POLY_F4),
    .partCount        = ARRAY_SIZE(_gMappicS302DrivewayShapeSkeleton),
    .partVertexCounts = _gMappicS302DrivewayShapePartVerts,
    .verts            = _gMappicS302DrivewayShapeVerts,
    .normals          = &_gMappicS302DrivewayShapeVerts[ARRAY_SIZE(_gMappicS302DrivewayShapeVerts)], // Empty normal pool at the vertex end.
    .skeleton         = _gMappicS302DrivewayShapeSkeleton,
    .stream           = _gMappicS302DrivewayShapeStream,
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
