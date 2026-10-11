#include "mappic/mappic.h"

#include <psyq/libgpu.h>

#include "common.h"

/// Identity transform and self-parented root for the Observatory shape.
static TmdBone _gMappicS500ObservatoryShapeSkeleton[] = {
#include "assets/mappic_s5_00_model_000A4_skeleton.inc"
};

/// Vertex count of the Observatory shape's sole part (fifteen vertices).
static u32 _gMappicS500ObservatoryShapePartVerts[] = {
#include "assets/mappic_s5_00_model_000A4_partVerts.inc"
};

/// Observatory's map footprint in integer model X/Z coordinates, with Y zero.
static SVECTOR _gMappicS500ObservatoryShapeVerts[] = {
#include "assets/mappic_s5_00_model_000A4_verts.inc"
};

/// One unlit triangle and six quads with packed vertex byte offsets and white colour, followed by two group ends and the stream end.
static u32 _gMappicS500ObservatoryShapeStream[] = {
#include "assets/mappic_s5_00_model_000A4_stream.inc"
};

TmdSource gMappicS500ObservatoryShape = {
    .bufferHalfBytes  = sizeof(POLY_F3) + 6 * sizeof(POLY_F4),
    .partCount        = ARRAY_SIZE(_gMappicS500ObservatoryShapeSkeleton),
    .partVertexCounts = _gMappicS500ObservatoryShapePartVerts,
    .verts            = _gMappicS500ObservatoryShapeVerts,
    .normals          = &_gMappicS500ObservatoryShapeVerts[ARRAY_SIZE(_gMappicS500ObservatoryShapeVerts)], // Empty normal pool at the vertex end.
    .skeleton         = _gMappicS500ObservatoryShapeSkeleton,
    .stream           = _gMappicS500ObservatoryShapeStream,
};

/// Identity transform and self-parented root for the EVE access tunnel shape.
static TmdBone _gMappicS500EveAccessTunnelShapeSkeleton[] = {
#include "assets/mappic_s2_02_model_00C48_001AC_mappic_s5_00_skeleton.inc"
};

/// Vertex count of the EVE access tunnel shape's sole part (four vertices).
static u32 _gMappicS500EveAccessTunnelShapePartVerts[] = {
#include "assets/mappic_s2_02_model_00C48_001AC_mappic_s5_00_partVerts.inc"
};

/// EVE access tunnel's map footprint in integer model X/Z coordinates, with Y zero.
static SVECTOR _gMappicS500EveAccessTunnelShapeVerts[] = {
#include "assets/mappic_s2_02_model_00C48_001AC_mappic_s5_00_verts.inc"
};

/// One unlit quad's packed vertex byte offsets and white colour, followed by two group ends and the stream end.
static u32 _gMappicS500EveAccessTunnelShapeStream[] = {
#include "assets/mappic_s2_02_model_00C48_001AC_mappic_s5_00_stream.inc"
};

TmdSource gMappicS500EveAccessTunnelShape = {
    .bufferHalfBytes  = sizeof(POLY_F4),
    .partCount        = ARRAY_SIZE(_gMappicS500EveAccessTunnelShapeSkeleton),
    .partVertexCounts = _gMappicS500EveAccessTunnelShapePartVerts,
    .verts            = _gMappicS500EveAccessTunnelShapeVerts,
    .normals          = &_gMappicS500EveAccessTunnelShapeVerts[ARRAY_SIZE(_gMappicS500EveAccessTunnelShapeVerts)], // Empty normal pool at the vertex end.
    .skeleton         = _gMappicS500EveAccessTunnelShapeSkeleton,
    .stream           = _gMappicS500EveAccessTunnelShapeStream,
};

/// Identity transform and self-parented root for the North promenade shape.
static TmdBone _gMappicS500NorthPromenadeShapeSkeleton[] = {
#include "assets/mappic_s5_00_model_00270_skeleton.inc"
};

/// Vertex count of the North promenade shape's sole part (fifteen vertices).
static u32 _gMappicS500NorthPromenadeShapePartVerts[] = {
#include "assets/mappic_s5_00_model_00270_partVerts.inc"
};

/// North promenade's map footprint in integer model X/Z coordinates, with Y zero.
static SVECTOR _gMappicS500NorthPromenadeShapeVerts[] = {
#include "assets/mappic_s5_00_model_00270_verts.inc"
};

/// One unlit triangle and six quads with packed vertex byte offsets and white colour, followed by two group ends and the stream end.
static u32 _gMappicS500NorthPromenadeShapeStream[] = {
#include "assets/mappic_s5_00_model_00270_stream.inc"
};

TmdSource gMappicS500NorthPromenadeShape = {
    .bufferHalfBytes  = sizeof(POLY_F3) + 6 * sizeof(POLY_F4),
    .partCount        = ARRAY_SIZE(_gMappicS500NorthPromenadeShapeSkeleton),
    .partVertexCounts = _gMappicS500NorthPromenadeShapePartVerts,
    .verts            = _gMappicS500NorthPromenadeShapeVerts,
    .normals          = &_gMappicS500NorthPromenadeShapeVerts[ARRAY_SIZE(_gMappicS500NorthPromenadeShapeVerts)], // Empty normal pool at the vertex end.
    .skeleton         = _gMappicS500NorthPromenadeShapeSkeleton,
    .stream           = _gMappicS500NorthPromenadeShapeStream,
};

/// Identity transform and self-parented root for the Forest zone shape.
static TmdBone _gMappicS500ForestZoneShapeSkeleton[] = {
#include "assets/mappic_s2_02_model_00C48_00378_mappic_s5_00_skeleton.inc"
};

/// Vertex count of the Forest zone shape's sole part (four vertices).
static u32 _gMappicS500ForestZoneShapePartVerts[] = {
#include "assets/mappic_s2_02_model_00C48_00378_mappic_s5_00_partVerts.inc"
};

/// Forest zone's map footprint in integer model X/Z coordinates, with Y zero.
static SVECTOR _gMappicS500ForestZoneShapeVerts[] = {
#include "assets/mappic_s2_02_model_00C48_00378_mappic_s5_00_verts.inc"
};

/// One unlit quad's packed vertex byte offsets and white colour, followed by two group ends and the stream end.
static u32 _gMappicS500ForestZoneShapeStream[] = {
#include "assets/mappic_s2_02_model_00C48_00378_mappic_s5_00_stream.inc"
};

TmdSource gMappicS500ForestZoneShape = {
    .bufferHalfBytes  = sizeof(POLY_F4),
    .partCount        = ARRAY_SIZE(_gMappicS500ForestZoneShapeSkeleton),
    .partVertexCounts = _gMappicS500ForestZoneShapePartVerts,
    .verts            = _gMappicS500ForestZoneShapeVerts,
    .normals          = &_gMappicS500ForestZoneShapeVerts[ARRAY_SIZE(_gMappicS500ForestZoneShapeVerts)], // Empty normal pool at the vertex end.
    .skeleton         = _gMappicS500ForestZoneShapeSkeleton,
    .stream           = _gMappicS500ForestZoneShapeStream,
};

/// Identity transform and self-parented root for the Submarine tunnel shape.
static TmdBone _gMappicS500SubmarineTunnelShapeSkeleton[] = {
#include "assets/mappic_s2_02_model_00C48_00408_skeleton.inc"
};

/// Vertex count of the Submarine tunnel shape's sole part (four vertices).
static u32 _gMappicS500SubmarineTunnelShapePartVerts[] = {
#include "assets/mappic_s2_02_model_00C48_00408_partVerts.inc"
};

/// Submarine tunnel's map footprint in integer model X/Z coordinates, with Y zero.
static SVECTOR _gMappicS500SubmarineTunnelShapeVerts[] = {
#include "assets/mappic_s2_02_model_00C48_00408_verts.inc"
};

/// One unlit quad's packed vertex byte offsets and white colour, followed by two group ends and the stream end.
static u32 _gMappicS500SubmarineTunnelShapeStream[] = {
#include "assets/mappic_s2_02_model_00C48_00408_stream.inc"
};

TmdSource gMappicS500SubmarineTunnelShape = {
    .bufferHalfBytes  = sizeof(POLY_F4),
    .partCount        = ARRAY_SIZE(_gMappicS500SubmarineTunnelShapeSkeleton),
    .partVertexCounts = _gMappicS500SubmarineTunnelShapePartVerts,
    .verts            = _gMappicS500SubmarineTunnelShapeVerts,
    .normals          = &_gMappicS500SubmarineTunnelShapeVerts[ARRAY_SIZE(_gMappicS500SubmarineTunnelShapeVerts)], // Empty normal pool at the vertex end.
    .skeleton         = _gMappicS500SubmarineTunnelShapeSkeleton,
    .stream           = _gMappicS500SubmarineTunnelShapeStream,
};

/// Identity transform and self-parented root for the Pavilion shape.
static TmdBone _gMappicS500PavilionShapeSkeleton[] = {
#include "assets/mappic_s2_02_model_00C48_00498_skeleton.inc"
};

/// Vertex count of the Pavilion shape's sole part (four vertices).
static u32 _gMappicS500PavilionShapePartVerts[] = {
#include "assets/mappic_s2_02_model_00C48_00498_partVerts.inc"
};

/// Pavilion's map footprint in integer model X/Z coordinates, with Y zero.
static SVECTOR _gMappicS500PavilionShapeVerts[] = {
#include "assets/mappic_s2_02_model_00C48_00498_verts.inc"
};

/// One unlit quad's packed vertex byte offsets and white colour, followed by two group ends and the stream end.
static u32 _gMappicS500PavilionShapeStream[] = {
#include "assets/mappic_s2_02_model_00C48_00498_stream.inc"
};

TmdSource gMappicS500PavilionShape = {
    .bufferHalfBytes  = sizeof(POLY_F4),
    .partCount        = ARRAY_SIZE(_gMappicS500PavilionShapeSkeleton),
    .partVertexCounts = _gMappicS500PavilionShapePartVerts,
    .verts            = _gMappicS500PavilionShapeVerts,
    .normals          = &_gMappicS500PavilionShapeVerts[ARRAY_SIZE(_gMappicS500PavilionShapeVerts)], // Empty normal pool at the vertex end.
    .skeleton         = _gMappicS500PavilionShapeSkeleton,
    .stream           = _gMappicS500PavilionShapeStream,
};

/// Identity transform and self-parented root for the Island shape.
static TmdBone _gMappicS500IslandShapeSkeleton[] = {
#include "assets/mappic_s4_06_model_0014C_00560_skeleton.inc"
};

/// Vertex count of the Island shape's sole part (eight vertices).
static u32 _gMappicS500IslandShapePartVerts[] = {
#include "assets/mappic_s4_06_model_0014C_00560_partVerts.inc"
};

/// Island's map footprint in integer model X/Z coordinates, with Y zero.
static SVECTOR _gMappicS500IslandShapeVerts[] = {
#include "assets/mappic_s4_06_model_0014C_00560_verts.inc"
};

/// Three unlit quads with packed vertex byte offsets and white colour, followed by two group ends and the stream end.
static u32 _gMappicS500IslandShapeStream[] = {
#include "assets/mappic_s4_06_model_0014C_00560_stream.inc"
};

TmdSource gMappicS500IslandShape = {
    .bufferHalfBytes  = 3 * sizeof(POLY_F4),
    .partCount        = ARRAY_SIZE(_gMappicS500IslandShapeSkeleton),
    .partVertexCounts = _gMappicS500IslandShapePartVerts,
    .verts            = _gMappicS500IslandShapeVerts,
    .normals          = &_gMappicS500IslandShapeVerts[ARRAY_SIZE(_gMappicS500IslandShapeVerts)], // Empty normal pool at the vertex end.
    .skeleton         = _gMappicS500IslandShapeSkeleton,
    .stream           = _gMappicS500IslandShapeStream,
};

/// Identity transform and self-parented root for the Garden shape.
static TmdBone _gMappicS500GardenShapeSkeleton[] = {
#include "assets/mappic_s5_00_model_005FC_skeleton.inc"
};

/// Vertex count of the Garden shape's sole part (ten vertices).
static u32 _gMappicS500GardenShapePartVerts[] = {
#include "assets/mappic_s5_00_model_005FC_partVerts.inc"
};

/// Garden's map footprint in integer model X/Z coordinates, with Y zero.
static SVECTOR _gMappicS500GardenShapeVerts[] = {
#include "assets/mappic_s5_00_model_005FC_verts.inc"
};

/// Two unlit triangles and three quads with packed vertex byte offsets and white colour, followed by two group ends and the stream end.
static u32 _gMappicS500GardenShapeStream[] = {
#include "assets/mappic_s5_00_model_005FC_stream.inc"
};

TmdSource gMappicS500GardenShape = {
    .bufferHalfBytes  = 2 * sizeof(POLY_F3) + 3 * sizeof(POLY_F4),
    .partCount        = ARRAY_SIZE(_gMappicS500GardenShapeSkeleton),
    .partVertexCounts = _gMappicS500GardenShapePartVerts,
    .verts            = _gMappicS500GardenShapeVerts,
    .normals          = &_gMappicS500GardenShapeVerts[ARRAY_SIZE(_gMappicS500GardenShapeVerts)], // Empty normal pool at the vertex end.
    .skeleton         = _gMappicS500GardenShapeSkeleton,
    .stream           = _gMappicS500GardenShapeStream,
};

/// Identity transform and self-parented root for the shared Power plant shape.
static TmdBone _gMappicS500PowerPlantsShapeSkeleton[] = {
#include "assets/mappic_s2_02_model_00C48_006EC_skeleton.inc"
};

/// Vertex count of the shared Power plant shape's sole part (four vertices).
static u32 _gMappicS500PowerPlantsShapePartVerts[] = {
#include "assets/mappic_s2_02_model_00C48_006EC_partVerts.inc"
};

/// Power plants 1 and 2's shared map footprint in integer model X/Z coordinates, with Y zero.
static SVECTOR _gMappicS500PowerPlantsShapeVerts[] = {
#include "assets/mappic_s2_02_model_00C48_006EC_verts.inc"
};

/// One unlit quad's packed vertex byte offsets and white colour, followed by two group ends and the stream end.
static u32 _gMappicS500PowerPlantsShapeStream[] = {
#include "assets/mappic_s2_02_model_00C48_006EC_stream.inc"
};

TmdSource gMappicS500PowerPlantsShape = {
    .bufferHalfBytes  = sizeof(POLY_F4),
    .partCount        = ARRAY_SIZE(_gMappicS500PowerPlantsShapeSkeleton),
    .partVertexCounts = _gMappicS500PowerPlantsShapePartVerts,
    .verts            = _gMappicS500PowerPlantsShapeVerts,
    .normals          = &_gMappicS500PowerPlantsShapeVerts[ARRAY_SIZE(_gMappicS500PowerPlantsShapeVerts)], // Empty normal pool at the vertex end.
    .skeleton         = _gMappicS500PowerPlantsShapeSkeleton,
    .stream           = _gMappicS500PowerPlantsShapeStream,
};

/// Identity transform and self-parented root for the Savanna zone shape.
static TmdBone _gMappicS500SavannaZoneShapeSkeleton[] = {
#include "assets/mappic_s2_02_model_00C48_0077C_skeleton.inc"
};

/// Vertex count of the Savanna zone shape's sole part (four vertices).
static u32 _gMappicS500SavannaZoneShapePartVerts[] = {
#include "assets/mappic_s2_02_model_00C48_0077C_partVerts.inc"
};

/// Savanna zone's map footprint in integer model X/Z coordinates, with Y zero.
static SVECTOR _gMappicS500SavannaZoneShapeVerts[] = {
#include "assets/mappic_s2_02_model_00C48_0077C_verts.inc"
};

/// One unlit quad's packed vertex byte offsets and white colour, followed by two group ends and the stream end.
static u32 _gMappicS500SavannaZoneShapeStream[] = {
#include "assets/mappic_s2_02_model_00C48_0077C_stream.inc"
};

TmdSource gMappicS500SavannaZoneShape = {
    .bufferHalfBytes  = sizeof(POLY_F4),
    .partCount        = ARRAY_SIZE(_gMappicS500SavannaZoneShapeSkeleton),
    .partVertexCounts = _gMappicS500SavannaZoneShapePartVerts,
    .verts            = _gMappicS500SavannaZoneShapeVerts,
    .normals          = &_gMappicS500SavannaZoneShapeVerts[ARRAY_SIZE(_gMappicS500SavannaZoneShapeVerts)], // Empty normal pool at the vertex end.
    .skeleton         = _gMappicS500SavannaZoneShapeSkeleton,
    .stream           = _gMappicS500SavannaZoneShapeStream,
};

/// Identity transform and self-parented root for the South promenade shape.
static TmdBone _gMappicS500SouthPromenadeShapeSkeleton[] = {
#include "assets/mappic_s5_00_model_00840_skeleton.inc"
};

/// Vertex count of the South promenade shape's sole part (fifteen vertices).
static u32 _gMappicS500SouthPromenadeShapePartVerts[] = {
#include "assets/mappic_s5_00_model_00840_partVerts.inc"
};

/// South promenade's map footprint in integer model X/Z coordinates, with Y zero.
static SVECTOR _gMappicS500SouthPromenadeShapeVerts[] = {
#include "assets/mappic_s5_00_model_00840_verts.inc"
};

/// One unlit triangle and six quads with packed vertex byte offsets and white colour, followed by two group ends and the stream end.
static u32 _gMappicS500SouthPromenadeShapeStream[] = {
#include "assets/mappic_s5_00_model_00840_stream.inc"
};

TmdSource gMappicS500SouthPromenadeShape = {
    .bufferHalfBytes  = sizeof(POLY_F3) + 6 * sizeof(POLY_F4),
    .partCount        = ARRAY_SIZE(_gMappicS500SouthPromenadeShapeSkeleton),
    .partVertexCounts = _gMappicS500SouthPromenadeShapePartVerts,
    .verts            = _gMappicS500SouthPromenadeShapeVerts,
    .normals          = &_gMappicS500SouthPromenadeShapeVerts[ARRAY_SIZE(_gMappicS500SouthPromenadeShapeVerts)], // Empty normal pool at the vertex end.
    .skeleton         = _gMappicS500SouthPromenadeShapeSkeleton,
    .stream           = _gMappicS500SouthPromenadeShapeStream,
};

/// Identity transform and self-parented root for the Altar shape.
static TmdBone _gMappicS500AltarShapeSkeleton[] = {
#include "assets/mappic_s2_02_model_00C48_00948_skeleton.inc"
};

/// Vertex count of the Altar shape's sole part (four vertices).
static u32 _gMappicS500AltarShapePartVerts[] = {
#include "assets/mappic_s2_02_model_00C48_00948_partVerts.inc"
};

/// Altar's map footprint in integer model X/Z coordinates, with Y zero.
static SVECTOR _gMappicS500AltarShapeVerts[] = {
#include "assets/mappic_s2_02_model_00C48_00948_verts.inc"
};

/// One unlit quad's packed vertex byte offsets and white colour, followed by two group ends and the stream end.
static u32 _gMappicS500AltarShapeStream[] = {
#include "assets/mappic_s2_02_model_00C48_00948_stream.inc"
};

TmdSource gMappicS500AltarShape = {
    .bufferHalfBytes  = sizeof(POLY_F4),
    .partCount        = ARRAY_SIZE(_gMappicS500AltarShapeSkeleton),
    .partVertexCounts = _gMappicS500AltarShapePartVerts,
    .verts            = _gMappicS500AltarShapeVerts,
    .normals          = &_gMappicS500AltarShapeVerts[ARRAY_SIZE(_gMappicS500AltarShapeVerts)], // Empty normal pool at the vertex end.
    .skeleton         = _gMappicS500AltarShapeSkeleton,
    .stream           = _gMappicS500AltarShapeStream,
};

static TmdBone _gMappicS500Model00A14Skeleton[1] = {
#include "assets/mappic_s5_00_model_00A14_skeleton.inc"
};

static u32 _gMappicS500Model00A14PartVerts[1] = {
#include "assets/mappic_s5_00_model_00A14_partVerts.inc"
};

static SVECTOR _gMappicS500Model00A14Verts[16] = {
#include "assets/mappic_s5_00_model_00A14_verts.inc"
};

static u32 _gMappicS500Model00A14Stream[36] = {
#include "assets/mappic_s5_00_model_00A14_stream.inc"
};

TmdSource D_mappic_s5_00_8012F9D4 = {
    0,
    200,
    0,
    1,
    _gMappicS500Model00A14PartVerts,
    _gMappicS500Model00A14Verts,
    &_gMappicS500Model00A14Verts[16],
    _gMappicS500Model00A14Skeleton,
    _gMappicS500Model00A14Stream,
};

static TmdBone _gMappicS500Model00B50Skeleton[1] = {
#include "assets/mappic_s5_00_model_00B50_skeleton.inc"
};

static u32 _gMappicS500Model00B50PartVerts[1] = {
#include "assets/mappic_s5_00_model_00B50_partVerts.inc"
};

static SVECTOR _gMappicS500Model00B50Verts[12] = {
#include "assets/mappic_s5_00_model_00B50_verts.inc"
};

static u32 _gMappicS500Model00B50Stream[21] = {
#include "assets/mappic_s5_00_model_00B50_stream.inc"
};

TmdSource D_mappic_s5_00_8012FAD4 = {
    0,
    120,
    0,
    1,
    _gMappicS500Model00B50PartVerts,
    _gMappicS500Model00B50Verts,
    &_gMappicS500Model00B50Verts[12],
    _gMappicS500Model00B50Skeleton,
    _gMappicS500Model00B50Stream,
};

static TmdBone _gMappicS500MappicS202Model0091000C84Skeleton[1] = {
#include "assets/mappic_s2_02_model_00910_00C84_skeleton.inc"
};

static u32 _gMappicS500MappicS202Model0091000C84PartVerts[1] = {
#include "assets/mappic_s2_02_model_00910_00C84_partVerts.inc"
};

static SVECTOR _gMappicS500MappicS202Model0091000C84Verts[8] = {
#include "assets/mappic_s2_02_model_00910_00C84_verts.inc"
};

static u32 _gMappicS500MappicS202Model0091000C84Stream[21] = {
#include "assets/mappic_s2_02_model_00910_00C84_stream.inc"
};

TmdSource D_mappic_s5_00_8012FBB4 = {
    0,
    88,
    0,
    1,
    _gMappicS500MappicS202Model0091000C84PartVerts,
    _gMappicS500MappicS202Model0091000C84Verts,
    &_gMappicS500MappicS202Model0091000C84Verts[8],
    _gMappicS500MappicS202Model0091000C84Skeleton,
    _gMappicS500MappicS202Model0091000C84Stream,
};

static TmdBone _gMappicS500Model00D28Skeleton[1] = {
#include "assets/mappic_s5_00_model_00D28_skeleton.inc"
};

static u32 _gMappicS500Model00D28PartVerts[1] = {
#include "assets/mappic_s5_00_model_00D28_partVerts.inc"
};

static SVECTOR _gMappicS500Model00D28Verts[11] = {
#include "assets/mappic_s5_00_model_00D28_verts.inc"
};

static u32 _gMappicS500Model00D28Stream[24] = {
#include "assets/mappic_s5_00_model_00D28_stream.inc"
};

TmdSource D_mappic_s5_00_8012FCB8 = {
    0,
    116,
    0,
    1,
    _gMappicS500Model00D28PartVerts,
    _gMappicS500Model00D28Verts,
    &_gMappicS500Model00D28Verts[11],
    _gMappicS500Model00D28Skeleton,
    _gMappicS500Model00D28Stream,
};

static TmdBone _gMappicS500Model00E0CSkeleton[1] = {
#include "assets/mappic_s5_00_model_00E0C_skeleton.inc"
};

static u32 _gMappicS500Model00E0CPartVerts[1] = {
#include "assets/mappic_s5_00_model_00E0C_partVerts.inc"
};

static SVECTOR _gMappicS500Model00E0CVerts[7] = {
#include "assets/mappic_s5_00_model_00E0C_verts.inc"
};

static u32 _gMappicS500Model00E0CStream[18] = {
#include "assets/mappic_s5_00_model_00E0C_stream.inc"
};

TmdSource D_mappic_s5_00_8012FD84 = {
    0,
    68,
    0,
    1,
    _gMappicS500Model00E0CPartVerts,
    _gMappicS500Model00E0CVerts,
    &_gMappicS500Model00E0CVerts[7],
    _gMappicS500Model00E0CSkeleton,
    _gMappicS500Model00E0CStream,
};

static TmdBone _gMappicS500MappicS202Model00CF800F1CSkeleton[1] = {
#include "assets/mappic_s2_02_model_00CF8_00F1C_skeleton.inc"
};

static u32 _gMappicS500MappicS202Model00CF800F1CPartVerts[1] = {
#include "assets/mappic_s2_02_model_00CF8_00F1C_partVerts.inc"
};

static SVECTOR _gMappicS500MappicS202Model00CF800F1CVerts[8] = {
#include "assets/mappic_s2_02_model_00CF8_00F1C_verts.inc"
};

static u32 _gMappicS500MappicS202Model00CF800F1CStream[15] = {
#include "assets/mappic_s2_02_model_00CF8_00F1C_stream.inc"
};

TmdSource D_mappic_s5_00_8012FE4C = {
    0,
    72,
    0,
    1,
    _gMappicS500MappicS202Model00CF800F1CPartVerts,
    _gMappicS500MappicS202Model00CF800F1CVerts,
    &_gMappicS500MappicS202Model00CF800F1CVerts[8],
    _gMappicS500MappicS202Model00CF800F1CSkeleton,
    _gMappicS500MappicS202Model00CF800F1CStream,
};
