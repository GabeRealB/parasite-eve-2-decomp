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

/// Identity transform and self-parented root for the Warehouse shape.
static TmdBone _gMappicS202WarehouseShapeSkeleton[] = {
#include "assets/mappic_s2_02_model_00C48_00378_skeleton.inc"
};

/// Vertex count of the Warehouse shape's sole part (four vertices).
static u32 _gMappicS202WarehouseShapePartVerts[] = {
#include "assets/mappic_s2_02_model_00C48_00378_partVerts.inc"
};

/// Warehouse's map footprint in integer model X/Z coordinates, with Y zero.
static SVECTOR _gMappicS202WarehouseShapeVerts[] = {
#include "assets/mappic_s2_02_model_00C48_00378_verts.inc"
};

/// Packed command stream for the Warehouse shape's single unlit quad.
///
/// Four SVECTOR byte offsets and white colour; two group ends precede the
/// stream end. The map reads only the first group.
static u32 _gMappicS202WarehouseShapeStream[] = {
#include "assets/mappic_s2_02_model_00C48_00378_stream.inc"
};

TmdSource gMappicS202WarehouseShape = {
    .bufferHalfBytes  = sizeof(POLY_F4),
    .partCount        = ARRAY_SIZE(_gMappicS202WarehouseShapeSkeleton),
    .partVertexCounts = _gMappicS202WarehouseShapePartVerts,
    .verts            = _gMappicS202WarehouseShapeVerts,
    .normals          = &_gMappicS202WarehouseShapeVerts[ARRAY_SIZE(_gMappicS202WarehouseShapeVerts)], // Empty normal pool at the vertex end.
    .skeleton         = _gMappicS202WarehouseShapeSkeleton,
    .stream           = _gMappicS202WarehouseShapeStream,
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

/// Identity transform and self-parented root for the Motel room 1 shape.
static TmdBone _gMappicS202MotelRoom1ShapeSkeleton[] = {
#include "assets/mappic_s2_02_model_00C48_004B4_skeleton.inc"
};

/// Vertex count of the Motel room 1 shape's sole part (four vertices).
static u32 _gMappicS202MotelRoom1ShapePartVerts[] = {
#include "assets/mappic_s2_02_model_00C48_004B4_partVerts.inc"
};

/// Motel room 1's map footprint in integer model X/Z coordinates, with Y zero.
static SVECTOR _gMappicS202MotelRoom1ShapeVerts[] = {
#include "assets/mappic_s2_02_model_00C48_004B4_verts.inc"
};

/// Packed command stream for the Motel room 1 shape's single unlit quad.
///
/// Four SVECTOR byte offsets and white colour; two group ends precede the
/// stream end. The map reads only the first group.
static u32 _gMappicS202MotelRoom1ShapeStream[] = {
#include "assets/mappic_s2_02_model_00C48_004B4_stream.inc"
};

TmdSource gMappicS202MotelRoom1Shape = {
    .bufferHalfBytes  = sizeof(POLY_F4),
    .partCount        = ARRAY_SIZE(_gMappicS202MotelRoom1ShapeSkeleton),
    .partVertexCounts = _gMappicS202MotelRoom1ShapePartVerts,
    .verts            = _gMappicS202MotelRoom1ShapeVerts,
    .normals          = &_gMappicS202MotelRoom1ShapeVerts[ARRAY_SIZE(_gMappicS202MotelRoom1ShapeVerts)], // Empty normal pool at the vertex end.
    .skeleton         = _gMappicS202MotelRoom1ShapeSkeleton,
    .stream           = _gMappicS202MotelRoom1ShapeStream,
};

/// Identity transform and self-parented root for the Motel room 2 shape.
static TmdBone _gMappicS202MotelRoom2ShapeSkeleton[] = {
#include "assets/mappic_s2_02_model_00C48_00544_skeleton.inc"
};

/// Vertex count of the Motel room 2 shape's sole part (four vertices).
static u32 _gMappicS202MotelRoom2ShapePartVerts[] = {
#include "assets/mappic_s2_02_model_00C48_00544_partVerts.inc"
};

/// Motel room 2's map footprint in integer model X/Z coordinates, with Y zero.
static SVECTOR _gMappicS202MotelRoom2ShapeVerts[] = {
#include "assets/mappic_s2_02_model_00C48_00544_verts.inc"
};

/// Packed command stream for the Motel room 2 shape's single unlit quad.
///
/// Four SVECTOR byte offsets and white colour; two group ends precede the
/// stream end. The map reads only the first group.
static u32 _gMappicS202MotelRoom2ShapeStream[] = {
#include "assets/mappic_s2_02_model_00C48_00544_stream.inc"
};

TmdSource gMappicS202MotelRoom2Shape = {
    .bufferHalfBytes  = sizeof(POLY_F4),
    .partCount        = ARRAY_SIZE(_gMappicS202MotelRoom2ShapeSkeleton),
    .partVertexCounts = _gMappicS202MotelRoom2ShapePartVerts,
    .verts            = _gMappicS202MotelRoom2ShapeVerts,
    .normals          = &_gMappicS202MotelRoom2ShapeVerts[ARRAY_SIZE(_gMappicS202MotelRoom2ShapeVerts)], // Empty normal pool at the vertex end.
    .skeleton         = _gMappicS202MotelRoom2ShapeSkeleton,
    .stream           = _gMappicS202MotelRoom2ShapeStream,
};

/// Identity transform and self-parented root for the Motel room 3 shape.
static TmdBone _gMappicS202MotelRoom3ShapeSkeleton[] = {
#include "assets/mappic_s2_02_model_00C48_005D4_skeleton.inc"
};

/// Vertex count of the Motel room 3 shape's sole part (four vertices).
static u32 _gMappicS202MotelRoom3ShapePartVerts[] = {
#include "assets/mappic_s2_02_model_00C48_005D4_partVerts.inc"
};

/// Motel room 3's map footprint in integer model X/Z coordinates, with Y zero.
static SVECTOR _gMappicS202MotelRoom3ShapeVerts[] = {
#include "assets/mappic_s2_02_model_00C48_005D4_verts.inc"
};

/// Packed command stream for the Motel room 3 shape's single unlit quad.
///
/// Four SVECTOR byte offsets and white colour; two group ends precede the
/// stream end. The map reads only the first group.
static u32 _gMappicS202MotelRoom3ShapeStream[] = {
#include "assets/mappic_s2_02_model_00C48_005D4_stream.inc"
};

TmdSource gMappicS202MotelRoom3Shape = {
    .bufferHalfBytes  = sizeof(POLY_F4),
    .partCount        = ARRAY_SIZE(_gMappicS202MotelRoom3ShapeSkeleton),
    .partVertexCounts = _gMappicS202MotelRoom3ShapePartVerts,
    .verts            = _gMappicS202MotelRoom3ShapeVerts,
    .normals          = &_gMappicS202MotelRoom3ShapeVerts[ARRAY_SIZE(_gMappicS202MotelRoom3ShapeVerts)], // Empty normal pool at the vertex end.
    .skeleton         = _gMappicS202MotelRoom3ShapeSkeleton,
    .stream           = _gMappicS202MotelRoom3ShapeStream,
};

/// Identity transform and self-parented root for the Motel room 4 shape.
static TmdBone _gMappicS202MotelRoom4ShapeSkeleton[] = {
#include "assets/mappic_s2_02_model_00C48_00664_skeleton.inc"
};

/// Vertex count of the Motel room 4 shape's sole part (four vertices).
static u32 _gMappicS202MotelRoom4ShapePartVerts[] = {
#include "assets/mappic_s2_02_model_00C48_00664_partVerts.inc"
};

/// Motel room 4's map footprint in integer model X/Z coordinates, with Y zero.
static SVECTOR _gMappicS202MotelRoom4ShapeVerts[] = {
#include "assets/mappic_s2_02_model_00C48_00664_verts.inc"
};

/// Packed command stream for the Motel room 4 shape's single unlit quad.
///
/// Four SVECTOR byte offsets and white colour; two group ends precede the
/// stream end. The map reads only the first group.
static u32 _gMappicS202MotelRoom4ShapeStream[] = {
#include "assets/mappic_s2_02_model_00C48_00664_stream.inc"
};

TmdSource gMappicS202MotelRoom4Shape = {
    .bufferHalfBytes  = sizeof(POLY_F4),
    .partCount        = ARRAY_SIZE(_gMappicS202MotelRoom4ShapeSkeleton),
    .partVertexCounts = _gMappicS202MotelRoom4ShapePartVerts,
    .verts            = _gMappicS202MotelRoom4ShapeVerts,
    .normals          = &_gMappicS202MotelRoom4ShapeVerts[ARRAY_SIZE(_gMappicS202MotelRoom4ShapeVerts)], // Empty normal pool at the vertex end.
    .skeleton         = _gMappicS202MotelRoom4ShapeSkeleton,
    .stream           = _gMappicS202MotelRoom4ShapeStream,
};

/// Identity transform and self-parented root for the Parking lot shape.
static TmdBone _gMappicS202ParkingLotShapeSkeleton[] = {
#include "assets/mappic_s2_02_model_00710_skeleton.inc"
};

/// Vertex count of the Parking lot shape's sole part (twelve vertices).
static u32 _gMappicS202ParkingLotShapePartVerts[] = {
#include "assets/mappic_s2_02_model_00710_partVerts.inc"
};

/// Parking lot's map footprint in integer model X/Z coordinates, with Y zero.
static SVECTOR _gMappicS202ParkingLotShapeVerts[] = {
#include "assets/mappic_s2_02_model_00710_verts.inc"
};

/// Packed command stream for the Parking lot shape's five unlit quads.
///
/// Each quad has four SVECTOR byte offsets and white colour; two group ends
/// precede the stream end. The map reads only the first group.
static u32 _gMappicS202ParkingLotShapeStream[] = {
#include "assets/mappic_s2_02_model_00710_stream.inc"
};

TmdSource gMappicS202ParkingLotShape = {
    .bufferHalfBytes  = 5 * sizeof(POLY_F4),
    .partCount        = ARRAY_SIZE(_gMappicS202ParkingLotShapeSkeleton),
    .partVertexCounts = _gMappicS202ParkingLotShapePartVerts,
    .verts            = _gMappicS202ParkingLotShapeVerts,
    .normals          = &_gMappicS202ParkingLotShapeVerts[ARRAY_SIZE(_gMappicS202ParkingLotShapeVerts)], // Empty normal pool at the vertex end.
    .skeleton         = _gMappicS202ParkingLotShapeSkeleton,
    .stream           = _gMappicS202ParkingLotShapeStream,
};

/// Identity transform and self-parented root for the Toilet shape.
static TmdBone _gMappicS202ToiletShapeSkeleton[] = {
#include "assets/mappic_s2_02_model_00C48_007F4_skeleton.inc"
};

/// Vertex count of the Toilet shape's sole part (four vertices).
static u32 _gMappicS202ToiletShapePartVerts[] = {
#include "assets/mappic_s2_02_model_00C48_007F4_partVerts.inc"
};

/// Toilet's map footprint in integer model X/Z coordinates, with Y zero.
static SVECTOR _gMappicS202ToiletShapeVerts[] = {
#include "assets/mappic_s2_02_model_00C48_007F4_verts.inc"
};

/// Packed command stream for the Toilet shape's unlit quad.
///
/// Four SVECTOR byte offsets and white colour precede two group ends and
/// the stream end. The map reads only the first group.
static u32 _gMappicS202ToiletShapeStream[] = {
#include "assets/mappic_s2_02_model_00C48_007F4_stream.inc"
};

TmdSource gMappicS202ToiletShape = {
    .bufferHalfBytes  = sizeof(POLY_F4),
    .partCount        = ARRAY_SIZE(_gMappicS202ToiletShapeSkeleton),
    .partVertexCounts = _gMappicS202ToiletShapePartVerts,
    .verts            = _gMappicS202ToiletShapeVerts,
    .normals          = &_gMappicS202ToiletShapeVerts[ARRAY_SIZE(_gMappicS202ToiletShapeVerts)], // Empty normal pool at the vertex end.
    .skeleton         = _gMappicS202ToiletShapeSkeleton,
    .stream           = _gMappicS202ToiletShapeStream,
};

/// Identity transform and self-parented root for the Motel lobby shape.
static TmdBone _gMappicS202MotelLobbyShapeSkeleton[] = {
#include "assets/mappic_s2_02_model_00C48_00884_skeleton.inc"
};

/// Vertex count of the Motel lobby shape's sole part (four vertices).
static u32 _gMappicS202MotelLobbyShapePartVerts[] = {
#include "assets/mappic_s2_02_model_00C48_00884_partVerts.inc"
};

/// Motel lobby's map footprint in integer model X/Z coordinates, with Y zero.
static SVECTOR _gMappicS202MotelLobbyShapeVerts[] = {
#include "assets/mappic_s2_02_model_00C48_00884_verts.inc"
};

/// Packed command stream for the Motel lobby shape's single unlit quad.
///
/// Four SVECTOR byte offsets and white colour; two group ends precede the
/// stream end. The map reads only the first group.
static u32 _gMappicS202MotelLobbyShapeStream[] = {
#include "assets/mappic_s2_02_model_00C48_00884_stream.inc"
};

TmdSource gMappicS202MotelLobbyShape = {
    .bufferHalfBytes  = sizeof(POLY_F4),
    .partCount        = ARRAY_SIZE(_gMappicS202MotelLobbyShapeSkeleton),
    .partVertexCounts = _gMappicS202MotelLobbyShapePartVerts,
    .verts            = _gMappicS202MotelLobbyShapeVerts,
    .normals          = &_gMappicS202MotelLobbyShapeVerts[ARRAY_SIZE(_gMappicS202MotelLobbyShapeVerts)], // Empty normal pool at the vertex end.
    .skeleton         = _gMappicS202MotelLobbyShapeSkeleton,
    .stream           = _gMappicS202MotelLobbyShapeStream,
};

/// Identity transform and self-parented root for the Saloon G & R shape.
static TmdBone _gMappicS202SaloonGRShapeSkeleton[] = {
#include "assets/mappic_s2_02_model_00910_skeleton.inc"
};

/// Vertex count of the Saloon G & R shape's sole part (eight vertices).
static u32 _gMappicS202SaloonGRShapePartVerts[] = {
#include "assets/mappic_s2_02_model_00910_partVerts.inc"
};

/// Saloon G & R's map footprint in integer model X/Z coordinates, with Y zero.
static SVECTOR _gMappicS202SaloonGRShapeVerts[] = {
#include "assets/mappic_s2_02_model_00910_verts.inc"
};

/// Packed command stream for the Saloon G & R shape's two unlit triangles and two unlit quads.
///
/// Elements contain SVECTOR byte offsets and white colour; two group ends
/// precede the stream end. The map reads only the first group.
static u32 _gMappicS202SaloonGRShapeStream[] = {
#include "assets/mappic_s2_02_model_00910_stream.inc"
};

TmdSource gMappicS202SaloonGRShape = {
    .bufferHalfBytes  = 2 * sizeof(POLY_F3) + 2 * sizeof(POLY_F4),
    .partCount        = ARRAY_SIZE(_gMappicS202SaloonGRShapeSkeleton),
    .partVertexCounts = _gMappicS202SaloonGRShapePartVerts,
    .verts            = _gMappicS202SaloonGRShapeVerts,
    .normals          = &_gMappicS202SaloonGRShapeVerts[ARRAY_SIZE(_gMappicS202SaloonGRShapeVerts)], // Empty normal pool at the vertex end.
    .skeleton         = _gMappicS202SaloonGRShapeSkeleton,
    .stream           = _gMappicS202SaloonGRShapeStream,
};

/// Identity transform and self-parented root for the G & R Kitchen shape.
static TmdBone _gMappicS202GRKitchenShapeSkeleton[] = {
#include "assets/mappic_s2_02_model_00C48_009F4_skeleton.inc"
};

/// Vertex count of the G & R Kitchen shape's sole part (four vertices).
static u32 _gMappicS202GRKitchenShapePartVerts[] = {
#include "assets/mappic_s2_02_model_00C48_009F4_partVerts.inc"
};

/// G & R Kitchen's map footprint in integer model X/Z coordinates, with Y zero.
static SVECTOR _gMappicS202GRKitchenShapeVerts[] = {
#include "assets/mappic_s2_02_model_00C48_009F4_verts.inc"
};

/// Packed command stream for the G & R Kitchen shape's single unlit quad.
///
/// Four SVECTOR byte offsets and white colour; two group ends precede the
/// stream end. The map reads only the first group.
static u32 _gMappicS202GRKitchenShapeStream[] = {
#include "assets/mappic_s2_02_model_00C48_009F4_stream.inc"
};

TmdSource gMappicS202GRKitchenShape = {
    .bufferHalfBytes  = sizeof(POLY_F4),
    .partCount        = ARRAY_SIZE(_gMappicS202GRKitchenShapeSkeleton),
    .partVertexCounts = _gMappicS202GRKitchenShapePartVerts,
    .verts            = _gMappicS202GRKitchenShapeVerts,
    .normals          = &_gMappicS202GRKitchenShapeVerts[ARRAY_SIZE(_gMappicS202GRKitchenShapeVerts)], // Empty normal pool at the vertex end.
    .skeleton         = _gMappicS202GRKitchenShapeSkeleton,
    .stream           = _gMappicS202GRKitchenShapeStream,
};

/// Identity transform and self-parented root for the Water Tower shape.
static TmdBone _gMappicS202WaterTowerShapeSkeleton[] = {
#include "assets/mappic_s4_00_model_0005C_00AA0_skeleton.inc"
};

/// Vertex count of the Water Tower shape's sole part (six vertices).
static u32 _gMappicS202WaterTowerShapePartVerts[] = {
#include "assets/mappic_s4_00_model_0005C_00AA0_partVerts.inc"
};

/// Water Tower's map footprint in integer model X/Z coordinates, with Y zero.
static SVECTOR _gMappicS202WaterTowerShapeVerts[] = {
#include "assets/mappic_s4_00_model_0005C_00AA0_verts.inc"
};

/// Packed command stream for the Water Tower shape's two unlit quads.
///
/// Each quad has four packed SVECTOR byte offsets and white colour; two group
/// ends precede the stream end. The map reads only the first group.
static u32 _gMappicS202WaterTowerShapeStream[] = {
#include "assets/mappic_s4_00_model_0005C_00AA0_stream.inc"
};

TmdSource gMappicS202WaterTowerShape = {
    .bufferHalfBytes  = 2 * sizeof(POLY_F4),
    .partCount        = ARRAY_SIZE(_gMappicS202WaterTowerShapeSkeleton),
    .partVertexCounts = _gMappicS202WaterTowerShapePartVerts,
    .verts            = _gMappicS202WaterTowerShapeVerts,
    .normals          = &_gMappicS202WaterTowerShapeVerts[ARRAY_SIZE(_gMappicS202WaterTowerShapeVerts)], // Empty normal pool at the vertex end.
    .skeleton         = _gMappicS202WaterTowerShapeSkeleton,
    .stream           = _gMappicS202WaterTowerShapeStream,
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
