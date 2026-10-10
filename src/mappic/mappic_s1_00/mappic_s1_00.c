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

static TmdBone _gMappicS100Model00564Skeleton[1] = {
#include "assets/mappic_s1_00_model_00564_skeleton.inc"
};

static u32 _gMappicS100Model00564PartVerts[1] = {
#include "assets/mappic_s1_00_model_00564_partVerts.inc"
};

static SVECTOR _gMappicS100Model00564Verts[11] = {
#include "assets/mappic_s1_00_model_00564_verts.inc"
};

static u32 _gMappicS100Model00564Stream[24] = {
#include "assets/mappic_s1_00_model_00564_stream.inc"
};

TmdSource D_mappic_s1_00_8012F4F4 = {
    0,
    116,
    0,
    1,
    _gMappicS100Model00564PartVerts,
    _gMappicS100Model00564Verts,
    &_gMappicS100Model00564Verts[11],
    _gMappicS100Model00564Skeleton,
    _gMappicS100Model00564Stream,
};

static TmdBone _gMappicS100Model00650Skeleton[1] = {
#include "assets/mappic_s1_00_model_00650_skeleton.inc"
};

static u32 _gMappicS100Model00650PartVerts[1] = {
#include "assets/mappic_s1_00_model_00650_partVerts.inc"
};

static SVECTOR _gMappicS100Model00650Verts[8] = {
#include "assets/mappic_s1_00_model_00650_verts.inc"
};

static u32 _gMappicS100Model00650Stream[21] = {
#include "assets/mappic_s1_00_model_00650_stream.inc"
};

TmdSource D_mappic_s1_00_8012F5D4 = {
    0,
    88,
    0,
    1,
    _gMappicS100Model00650PartVerts,
    _gMappicS100Model00650Verts,
    &_gMappicS100Model00650Verts[8],
    _gMappicS100Model00650Skeleton,
    _gMappicS100Model00650Stream,
};

static TmdBone _gMappicS100Model00718Skeleton[1] = {
#include "assets/mappic_s1_00_model_00718_skeleton.inc"
};

static u32 _gMappicS100Model00718PartVerts[1] = {
#include "assets/mappic_s1_00_model_00718_partVerts.inc"
};

static SVECTOR _gMappicS100Model00718Verts[5] = {
#include "assets/mappic_s1_00_model_00718_verts.inc"
};

static u32 _gMappicS100Model00718Stream[15] = {
#include "assets/mappic_s1_00_model_00718_stream.inc"
};

TmdSource D_mappic_s1_00_8012F684 = {
    0,
    44,
    0,
    1,
    _gMappicS100Model00718PartVerts,
    _gMappicS100Model00718Verts,
    &_gMappicS100Model00718Verts[5],
    _gMappicS100Model00718Skeleton,
    _gMappicS100Model00718Stream,
};

static TmdBone _gMappicS100Model007E8Skeleton[1] = {
#include "assets/mappic_s1_00_model_007E8_skeleton.inc"
};

static u32 _gMappicS100Model007E8PartVerts[1] = {
#include "assets/mappic_s1_00_model_007E8_partVerts.inc"
};

static SVECTOR _gMappicS100Model007E8Verts[9] = {
#include "assets/mappic_s1_00_model_007E8_verts.inc"
};

static u32 _gMappicS100Model007E8Stream[21] = {
#include "assets/mappic_s1_00_model_007E8_stream.inc"
};

TmdSource D_mappic_s1_00_8012F76C = {
    0,
    92,
    0,
    1,
    _gMappicS100Model007E8PartVerts,
    _gMappicS100Model007E8Verts,
    &_gMappicS100Model007E8Verts[9],
    _gMappicS100Model007E8Skeleton,
    _gMappicS100Model007E8Stream,
};

static TmdBone _gMappicS100MappicS202Model00C48008CCSkeleton[1] = {
#include "assets/mappic_s2_02_model_00C48_008CC_skeleton.inc"
};

static u32 _gMappicS100MappicS202Model00C48008CCPartVerts[1] = {
#include "assets/mappic_s2_02_model_00C48_008CC_partVerts.inc"
};

static SVECTOR _gMappicS100MappicS202Model00C48008CCVerts[4] = {
#include "assets/mappic_s2_02_model_00C48_008CC_verts.inc"
};

static u32 _gMappicS100MappicS202Model00C48008CCStream[9] = {
#include "assets/mappic_s2_02_model_00C48_008CC_stream.inc"
};

TmdSource D_mappic_s1_00_8012F7FC = {
    0,
    24,
    0,
    1,
    _gMappicS100MappicS202Model00C48008CCPartVerts,
    _gMappicS100MappicS202Model00C48008CCVerts,
    &_gMappicS100MappicS202Model00C48008CCVerts[4],
    _gMappicS100MappicS202Model00C48008CCSkeleton,
    _gMappicS100MappicS202Model00C48008CCStream,
};

static TmdBone _gMappicS100MappicS202Model00C480095CSkeleton[1] = {
#include "assets/mappic_s2_02_model_00C48_0095C_skeleton.inc"
};

static u32 _gMappicS100MappicS202Model00C480095CPartVerts[1] = {
#include "assets/mappic_s2_02_model_00C48_0095C_partVerts.inc"
};

static SVECTOR _gMappicS100MappicS202Model00C480095CVerts[4] = {
#include "assets/mappic_s2_02_model_00C48_0095C_verts.inc"
};

static u32 _gMappicS100MappicS202Model00C480095CStream[9] = {
#include "assets/mappic_s2_02_model_00C48_0095C_stream.inc"
};

TmdSource D_mappic_s1_00_8012F88C = {
    0,
    24,
    0,
    1,
    _gMappicS100MappicS202Model00C480095CPartVerts,
    _gMappicS100MappicS202Model00C480095CVerts,
    &_gMappicS100MappicS202Model00C480095CVerts[4],
    _gMappicS100MappicS202Model00C480095CSkeleton,
    _gMappicS100MappicS202Model00C480095CStream,
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
