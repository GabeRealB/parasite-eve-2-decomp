#include "mappic/mappic.h"

#include <psyq/libgpu.h>

#include "common.h"

/// Identity transform and self-parented root for the Square shape.
static TmdBone _gMappicS100SquareShapeSkeleton[] = {
#include "assets/mappic_s2_02_model_00CF8_skeleton.inc"
};

/// Vertex count of the Square shape's sole part (eight vertices).
static u32 _gMappicS100SquareShapePartVerts[] = {
#include "assets/mappic_s2_02_model_00CF8_partVerts.inc"
};

/// Square's map footprint in integer model X/Z coordinates, with Y zero.
static SVECTOR _gMappicS100SquareShapeVerts[] = {
#include "assets/mappic_s2_02_model_00CF8_verts.inc"
};

/// Three unlit quads' packed vertex byte offsets and white colour, followed by two group ends and the stream end.
static u32 _gMappicS100SquareShapeStream[] = {
#include "assets/mappic_s2_02_model_00CF8_stream.inc"
};

TmdSource gMappicS100SquareShape = {
    .bufferHalfBytes  = 3 * sizeof(POLY_F4),
    .partCount        = ARRAY_SIZE(_gMappicS100SquareShapeSkeleton),
    .partVertexCounts = _gMappicS100SquareShapePartVerts,
    .verts            = _gMappicS100SquareShapeVerts,
    .normals          = &_gMappicS100SquareShapeVerts[ARRAY_SIZE(_gMappicS100SquareShapeVerts)], // Empty normal pool at the vertex end.
    .skeleton         = _gMappicS100SquareShapeSkeleton,
    .stream           = _gMappicS100SquareShapeStream,
};

/// Identity transform and self-parented root for the East elevator hall shape.
static TmdBone _gMappicS100EastElevatorHallShapeSkeleton[] = {
#include "assets/mappic_s1_00_model_00174_skeleton.inc"
};

/// Vertex count of the East elevator hall shape's sole part (sixteen vertices).
static u32 _gMappicS100EastElevatorHallShapePartVerts[] = {
#include "assets/mappic_s1_00_model_00174_partVerts.inc"
};

/// East elevator hall's map footprint in integer model X/Z coordinates, with Y zero.
static SVECTOR _gMappicS100EastElevatorHallShapeVerts[] = {
#include "assets/mappic_s1_00_model_00174_verts.inc"
};

/// Two unlit triangles and six quads with packed vertex byte offsets and white colour, followed by two group ends and the stream end.
static u32 _gMappicS100EastElevatorHallShapeStream[] = {
#include "assets/mappic_s1_00_model_00174_stream.inc"
};

TmdSource gMappicS100EastElevatorHallShape = {
    .bufferHalfBytes  = 2 * sizeof(POLY_F3) + 6 * sizeof(POLY_F4),
    .partCount        = ARRAY_SIZE(_gMappicS100EastElevatorHallShapeSkeleton),
    .partVertexCounts = _gMappicS100EastElevatorHallShapePartVerts,
    .verts            = _gMappicS100EastElevatorHallShapeVerts,
    .normals          = &_gMappicS100EastElevatorHallShapeVerts[ARRAY_SIZE(_gMappicS100EastElevatorHallShapeVerts)], // Empty normal pool at the vertex end.
    .skeleton         = _gMappicS100EastElevatorHallShapeSkeleton,
    .stream           = _gMappicS100EastElevatorHallShapeStream,
};

/// Identity transform and self-parented root for the Patio shape.
static TmdBone _gMappicS100PatioShapeSkeleton[] = {
#include "assets/mappic_s1_00_model_00294_skeleton.inc"
};

/// Vertex count of the Patio shape's sole part (ten vertices).
static u32 _gMappicS100PatioShapePartVerts[] = {
#include "assets/mappic_s1_00_model_00294_partVerts.inc"
};

/// Patio's map footprint in integer model X/Z coordinates, with Y zero.
static SVECTOR _gMappicS100PatioShapeVerts[] = {
#include "assets/mappic_s1_00_model_00294_verts.inc"
};

/// Two unlit triangles and three quads with packed vertex byte offsets and white colour, followed by two group ends and the stream end.
static u32 _gMappicS100PatioShapeStream[] = {
#include "assets/mappic_s1_00_model_00294_stream.inc"
};

TmdSource gMappicS100PatioShape = {
    .bufferHalfBytes  = 2 * sizeof(POLY_F3) + 3 * sizeof(POLY_F4),
    .partCount        = ARRAY_SIZE(_gMappicS100PatioShapeSkeleton),
    .partVertexCounts = _gMappicS100PatioShapePartVerts,
    .verts            = _gMappicS100PatioShapeVerts,
    .normals          = &_gMappicS100PatioShapeVerts[ARRAY_SIZE(_gMappicS100PatioShapeVerts)], // Empty normal pool at the vertex end.
    .skeleton         = _gMappicS100PatioShapeSkeleton,
    .stream           = _gMappicS100PatioShapeStream,
};

/// Identity transform and self-parented root for the Cafeteria shape.
static TmdBone _gMappicS100CafeteriaShapeSkeleton[] = {
#include "assets/mappic_s4_00_model_0005C_skeleton.inc"
};

/// Vertex count of the Cafeteria shape's sole part (six vertices).
static u32 _gMappicS100CafeteriaShapePartVerts[] = {
#include "assets/mappic_s4_00_model_0005C_partVerts.inc"
};

/// Cafeteria's map footprint in integer model X/Z coordinates, with Y zero.
static SVECTOR _gMappicS100CafeteriaShapeVerts[] = {
#include "assets/mappic_s4_00_model_0005C_verts.inc"
};

/// Two unlit quads' packed vertex byte offsets and white colour, followed by two group ends and the stream end.
static u32 _gMappicS100CafeteriaShapeStream[] = {
#include "assets/mappic_s4_00_model_0005C_stream.inc"
};

TmdSource gMappicS100CafeteriaShape = {
    .bufferHalfBytes  = 2 * sizeof(POLY_F4),
    .partCount        = ARRAY_SIZE(_gMappicS100CafeteriaShapeSkeleton),
    .partVertexCounts = _gMappicS100CafeteriaShapePartVerts,
    .verts            = _gMappicS100CafeteriaShapeVerts,
    .normals          = &_gMappicS100CafeteriaShapeVerts[ARRAY_SIZE(_gMappicS100CafeteriaShapeVerts)], // Empty normal pool at the vertex end.
    .skeleton         = _gMappicS100CafeteriaShapeSkeleton,
    .stream           = _gMappicS100CafeteriaShapeStream,
};

/// Identity transform and self-parented root for the Security room shape.
static TmdBone _gMappicS100SecurityRoomShapeSkeleton[] = {
#include "assets/mappic_s2_02_model_00C48_skeleton.inc"
};

/// Vertex count of the Security room shape's sole part (four vertices).
static u32 _gMappicS100SecurityRoomShapePartVerts[] = {
#include "assets/mappic_s2_02_model_00C48_partVerts.inc"
};

/// Security room's map footprint in integer model X/Z coordinates, with Y zero.
static SVECTOR _gMappicS100SecurityRoomShapeVerts[] = {
#include "assets/mappic_s2_02_model_00C48_verts.inc"
};

/// One unlit quad's packed vertex byte offsets and white colour, followed by two group ends and the stream end.
static u32 _gMappicS100SecurityRoomShapeStream[] = {
#include "assets/mappic_s2_02_model_00C48_stream.inc"
};

TmdSource gMappicS100SecurityRoomShape = {
    .bufferHalfBytes  = sizeof(POLY_F4),
    .partCount        = ARRAY_SIZE(_gMappicS100SecurityRoomShapeSkeleton),
    .partVertexCounts = _gMappicS100SecurityRoomShapePartVerts,
    .verts            = _gMappicS100SecurityRoomShapeVerts,
    .normals          = &_gMappicS100SecurityRoomShapeVerts[ARRAY_SIZE(_gMappicS100SecurityRoomShapeVerts)], // Empty normal pool at the vertex end.
    .skeleton         = _gMappicS100SecurityRoomShapeSkeleton,
    .stream           = _gMappicS100SecurityRoomShapeStream,
};

/// Identity transform and self-parented root for the Hallway shape.
static TmdBone _gMappicS100HallwayShapeSkeleton[] = {
#include "assets/mappic_s2_02_model_00C48_004C0_skeleton.inc"
};

/// Vertex count of the Hallway shape's sole part (four vertices).
static u32 _gMappicS100HallwayShapePartVerts[] = {
#include "assets/mappic_s2_02_model_00C48_004C0_partVerts.inc"
};

/// Hallway's map footprint in integer model X/Z coordinates, with Y zero.
static SVECTOR _gMappicS100HallwayShapeVerts[] = {
#include "assets/mappic_s2_02_model_00C48_004C0_verts.inc"
};

/// One unlit quad's packed vertex byte offsets and white colour, followed by two group ends and the stream end.
static u32 _gMappicS100HallwayShapeStream[] = {
#include "assets/mappic_s2_02_model_00C48_004C0_stream.inc"
};

TmdSource gMappicS100HallwayShape = {
    .bufferHalfBytes  = sizeof(POLY_F4),
    .partCount        = ARRAY_SIZE(_gMappicS100HallwayShapeSkeleton),
    .partVertexCounts = _gMappicS100HallwayShapePartVerts,
    .verts            = _gMappicS100HallwayShapeVerts,
    .normals          = &_gMappicS100HallwayShapeVerts[ARRAY_SIZE(_gMappicS100HallwayShapeVerts)], // Empty normal pool at the vertex end.
    .skeleton         = _gMappicS100HallwayShapeSkeleton,
    .stream           = _gMappicS100HallwayShapeStream,
};

/// Identity transform and self-parented root for the Fountain shape.
static TmdBone _gMappicS100FountainShapeSkeleton[] = {
#include "assets/mappic_s1_00_model_00564_skeleton.inc"
};

/// Vertex count of the Fountain shape's sole part (eleven vertices).
static u32 _gMappicS100FountainShapePartVerts[] = {
#include "assets/mappic_s1_00_model_00564_partVerts.inc"
};

/// Fountain's map footprint in integer model X/Z coordinates, with Y zero.
static SVECTOR _gMappicS100FountainShapeVerts[] = {
#include "assets/mappic_s1_00_model_00564_verts.inc"
};

/// One unlit triangle and four quads with packed vertex byte offsets and white colour.
///
/// Two group ends and the stream end follow the faces; the map reads only the first group.
static u32 _gMappicS100FountainShapeStream[] = {
#include "assets/mappic_s1_00_model_00564_stream.inc"
};

TmdSource gMappicS100FountainShape = {
    .bufferHalfBytes  = sizeof(POLY_F3) + 4 * sizeof(POLY_F4),
    .partCount        = ARRAY_SIZE(_gMappicS100FountainShapeSkeleton),
    .partVertexCounts = _gMappicS100FountainShapePartVerts,
    .verts            = _gMappicS100FountainShapeVerts,
    .normals          = &_gMappicS100FountainShapeVerts[ARRAY_SIZE(_gMappicS100FountainShapeVerts)], // Empty normal pool at the vertex end.
    .skeleton         = _gMappicS100FountainShapeSkeleton,
    .stream           = _gMappicS100FountainShapeStream,
};

/// Identity transform and self-parented root for the Forked road shape.
static TmdBone _gMappicS100ForkedRoadShapeSkeleton[] = {
#include "assets/mappic_s1_00_model_00650_skeleton.inc"
};

/// Vertex count of the Forked road shape's sole part (eight vertices).
static u32 _gMappicS100ForkedRoadShapePartVerts[] = {
#include "assets/mappic_s1_00_model_00650_partVerts.inc"
};

/// Forked road's map footprint in integer model X/Z coordinates, with Y zero.
static SVECTOR _gMappicS100ForkedRoadShapeVerts[] = {
#include "assets/mappic_s1_00_model_00650_verts.inc"
};

/// Two unlit triangles and two quads with packed vertex byte offsets and white colour.
///
/// Two group ends and the stream end follow the faces; the map reads only the first group.
static u32 _gMappicS100ForkedRoadShapeStream[] = {
#include "assets/mappic_s1_00_model_00650_stream.inc"
};

TmdSource gMappicS100ForkedRoadShape = {
    .bufferHalfBytes  = 2 * sizeof(POLY_F3) + 2 * sizeof(POLY_F4),
    .partCount        = ARRAY_SIZE(_gMappicS100ForkedRoadShapeSkeleton),
    .partVertexCounts = _gMappicS100ForkedRoadShapePartVerts,
    .verts            = _gMappicS100ForkedRoadShapeVerts,
    .normals          = &_gMappicS100ForkedRoadShapeVerts[ARRAY_SIZE(_gMappicS100ForkedRoadShapeVerts)], // Empty normal pool at the vertex end.
    .skeleton         = _gMappicS100ForkedRoadShapeSkeleton,
    .stream           = _gMappicS100ForkedRoadShapeStream,
};

/// Identity transform and self-parented root for the Observatory shape.
static TmdBone _gMappicS100ObservatoryShapeSkeleton[] = {
#include "assets/mappic_s1_00_model_00718_skeleton.inc"
};

/// Vertex count of the Observatory shape's sole part (five vertices).
static u32 _gMappicS100ObservatoryShapePartVerts[] = {
#include "assets/mappic_s1_00_model_00718_partVerts.inc"
};

/// Observatory's map footprint in integer model X/Z coordinates, with Y zero.
static SVECTOR _gMappicS100ObservatoryShapeVerts[] = {
#include "assets/mappic_s1_00_model_00718_verts.inc"
};

/// One unlit triangle and one quad with packed vertex byte offsets and white colour.
///
/// Two group ends and the stream end follow the faces; the map reads only the first group.
static u32 _gMappicS100ObservatoryShapeStream[] = {
#include "assets/mappic_s1_00_model_00718_stream.inc"
};

TmdSource gMappicS100ObservatoryShape = {
    .bufferHalfBytes  = sizeof(POLY_F3) + sizeof(POLY_F4),
    .partCount        = ARRAY_SIZE(_gMappicS100ObservatoryShapeSkeleton),
    .partVertexCounts = _gMappicS100ObservatoryShapePartVerts,
    .verts            = _gMappicS100ObservatoryShapeVerts,
    .normals          = &_gMappicS100ObservatoryShapeVerts[ARRAY_SIZE(_gMappicS100ObservatoryShapeVerts)], // Empty normal pool at the vertex end.
    .skeleton         = _gMappicS100ObservatoryShapeSkeleton,
    .stream           = _gMappicS100ObservatoryShapeStream,
};

/// Identity transform and self-parented root for the Promenade shape.
static TmdBone _gMappicS100PromenadeShapeSkeleton[] = {
#include "assets/mappic_s1_00_model_007E8_skeleton.inc"
};

/// Vertex count of the Promenade shape's sole part (nine vertices).
static u32 _gMappicS100PromenadeShapePartVerts[] = {
#include "assets/mappic_s1_00_model_007E8_partVerts.inc"
};

/// Promenade's map footprint in integer model X/Z coordinates, with Y zero.
static SVECTOR _gMappicS100PromenadeShapeVerts[] = {
#include "assets/mappic_s1_00_model_007E8_verts.inc"
};

/// One unlit triangle and three quads with packed vertex byte offsets and white colour.
///
/// Two group ends and the stream end follow the faces; the map reads only the first group.
static u32 _gMappicS100PromenadeShapeStream[] = {
#include "assets/mappic_s1_00_model_007E8_stream.inc"
};

TmdSource gMappicS100PromenadeShape = {
    .bufferHalfBytes  = sizeof(POLY_F3) + 3 * sizeof(POLY_F4),
    .partCount        = ARRAY_SIZE(_gMappicS100PromenadeShapeSkeleton),
    .partVertexCounts = _gMappicS100PromenadeShapePartVerts,
    .verts            = _gMappicS100PromenadeShapeVerts,
    .normals          = &_gMappicS100PromenadeShapeVerts[ARRAY_SIZE(_gMappicS100PromenadeShapeVerts)], // Empty normal pool at the vertex end.
    .skeleton         = _gMappicS100PromenadeShapeSkeleton,
    .stream           = _gMappicS100PromenadeShapeStream,
};

/// Identity transform and self-parented root for the Sanctuary shape.
static TmdBone _gMappicS100SanctuaryShapeSkeleton[] = {
#include "assets/mappic_s2_02_model_00C48_008CC_skeleton.inc"
};

/// Vertex count of the Sanctuary shape's sole part (four vertices).
static u32 _gMappicS100SanctuaryShapePartVerts[] = {
#include "assets/mappic_s2_02_model_00C48_008CC_partVerts.inc"
};

/// Sanctuary's map footprint in integer model X/Z coordinates, with Y zero.
static SVECTOR _gMappicS100SanctuaryShapeVerts[] = {
#include "assets/mappic_s2_02_model_00C48_008CC_verts.inc"
};

/// One unlit quad's packed vertex byte offsets and white colour.
///
/// Two group ends and the stream end follow the face; the map reads only the first group.
static u32 _gMappicS100SanctuaryShapeStream[] = {
#include "assets/mappic_s2_02_model_00C48_008CC_stream.inc"
};

TmdSource gMappicS100SanctuaryShape = {
    .bufferHalfBytes  = sizeof(POLY_F4),
    .partCount        = ARRAY_SIZE(_gMappicS100SanctuaryShapeSkeleton),
    .partVertexCounts = _gMappicS100SanctuaryShapePartVerts,
    .verts            = _gMappicS100SanctuaryShapeVerts,
    .normals          = &_gMappicS100SanctuaryShapeVerts[ARRAY_SIZE(_gMappicS100SanctuaryShapeVerts)], // Empty normal pool at the vertex end.
    .skeleton         = _gMappicS100SanctuaryShapeSkeleton,
    .stream           = _gMappicS100SanctuaryShapeStream,
};

/// Identity transform and self-parented root for the Roof garden shape.
static TmdBone _gMappicS100RoofGardenShapeSkeleton[] = {
#include "assets/mappic_s2_02_model_00C48_0095C_skeleton.inc"
};

/// Vertex count of the Roof garden shape's sole part (four vertices).
static u32 _gMappicS100RoofGardenShapePartVerts[] = {
#include "assets/mappic_s2_02_model_00C48_0095C_partVerts.inc"
};

/// Roof garden's map footprint in integer model X/Z coordinates, with Y zero.
static SVECTOR _gMappicS100RoofGardenShapeVerts[] = {
#include "assets/mappic_s2_02_model_00C48_0095C_verts.inc"
};

/// One unlit quad's packed vertex byte offsets and white colour.
///
/// Two group ends and the stream end follow the face; the map reads only the first group.
static u32 _gMappicS100RoofGardenShapeStream[] = {
#include "assets/mappic_s2_02_model_00C48_0095C_stream.inc"
};

TmdSource gMappicS100RoofGardenShape = {
    .bufferHalfBytes  = sizeof(POLY_F4),
    .partCount        = ARRAY_SIZE(_gMappicS100RoofGardenShapeSkeleton),
    .partVertexCounts = _gMappicS100RoofGardenShapePartVerts,
    .verts            = _gMappicS100RoofGardenShapeVerts,
    .normals          = &_gMappicS100RoofGardenShapeVerts[ARRAY_SIZE(_gMappicS100RoofGardenShapeVerts)], // Empty normal pool at the vertex end.
    .skeleton         = _gMappicS100RoofGardenShapeSkeleton,
    .stream           = _gMappicS100RoofGardenShapeStream,
};

static TmdBone _gMappicS100MappicS202Model00C48009ECSkeleton[1] = {
#include "assets/mappic_s2_02_model_00C48_009EC_skeleton.inc"
};

static u32 _gMappicS100MappicS202Model00C48009ECPartVerts[1] = {
#include "assets/mappic_s2_02_model_00C48_009EC_partVerts.inc"
};

static SVECTOR _gMappicS100MappicS202Model00C48009ECVerts[4] = {
#include "assets/mappic_s2_02_model_00C48_009EC_verts.inc"
};

static u32 _gMappicS100MappicS202Model00C48009ECStream[9] = {
#include "assets/mappic_s2_02_model_00C48_009EC_stream.inc"
};

TmdSource D_mappic_s1_00_8012F91C = {
    0,
    24,
    0,
    1,
    _gMappicS100MappicS202Model00C48009ECPartVerts,
    _gMappicS100MappicS202Model00C48009ECVerts,
    &_gMappicS100MappicS202Model00C48009ECVerts[4],
    _gMappicS100MappicS202Model00C48009ECSkeleton,
    _gMappicS100MappicS202Model00C48009ECStream,
};

static TmdBone _gMappicS100MappicS202Model00C4800A7CSkeleton[1] = {
#include "assets/mappic_s2_02_model_00C48_00A7C_skeleton.inc"
};

static u32 _gMappicS100MappicS202Model00C4800A7CPartVerts[1] = {
#include "assets/mappic_s2_02_model_00C48_00A7C_partVerts.inc"
};

static SVECTOR _gMappicS100MappicS202Model00C4800A7CVerts[4] = {
#include "assets/mappic_s2_02_model_00C48_00A7C_verts.inc"
};

static u32 _gMappicS100MappicS202Model00C4800A7CStream[9] = {
#include "assets/mappic_s2_02_model_00C48_00A7C_stream.inc"
};

TmdSource D_mappic_s1_00_8012F9AC = {
    0,
    24,
    0,
    1,
    _gMappicS100MappicS202Model00C4800A7CPartVerts,
    _gMappicS100MappicS202Model00C4800A7CVerts,
    &_gMappicS100MappicS202Model00C4800A7CVerts[4],
    _gMappicS100MappicS202Model00C4800A7CSkeleton,
    _gMappicS100MappicS202Model00C4800A7CStream,
};

static TmdBone _gMappicS100Model00B48Skeleton[1] = {
#include "assets/mappic_s1_00_model_00B48_skeleton.inc"
};

static u32 _gMappicS100Model00B48PartVerts[1] = {
#include "assets/mappic_s1_00_model_00B48_partVerts.inc"
};

static SVECTOR _gMappicS100Model00B48Verts[16] = {
#include "assets/mappic_s1_00_model_00B48_verts.inc"
};

static u32 _gMappicS100Model00B48Stream[33] = {
#include "assets/mappic_s1_00_model_00B48_stream.inc"
};

TmdSource D_mappic_s1_00_8012FAFC = {
    0,
    184,
    0,
    1,
    _gMappicS100Model00B48PartVerts,
    _gMappicS100Model00B48Verts,
    &_gMappicS100Model00B48Verts[16],
    _gMappicS100Model00B48Skeleton,
    _gMappicS100Model00B48Stream,
};
