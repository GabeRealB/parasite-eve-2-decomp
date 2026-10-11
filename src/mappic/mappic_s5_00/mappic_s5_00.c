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

/// Identity transform and self-parented root for the Shrine shape.
static TmdBone _gMappicS500ShrineShapeSkeleton[] = {
#include "assets/mappic_s5_00_model_00A14_skeleton.inc"
};

/// Vertex count of the Shrine shape's sole part (sixteen vertices).
static u32 _gMappicS500ShrineShapePartVerts[] = {
#include "assets/mappic_s5_00_model_00A14_partVerts.inc"
};

/// Shrine's map footprint in integer model X/Z coordinates, with Y zero.
static SVECTOR _gMappicS500ShrineShapeVerts[] = {
#include "assets/mappic_s5_00_model_00A14_verts.inc"
};

/// Four triangles and five quads with packed vertex byte offsets and unlit white colour.
/// Two group ends and the stream end follow the polygon commands.
static u32 _gMappicS500ShrineShapeStream[] = {
#include "assets/mappic_s5_00_model_00A14_stream.inc"
};

TmdSource gMappicS500ShrineShape = {
    .bufferHalfBytes  = 4 * sizeof(POLY_F3) + 5 * sizeof(POLY_F4),
    .partCount        = ARRAY_SIZE(_gMappicS500ShrineShapeSkeleton),
    .partVertexCounts = _gMappicS500ShrineShapePartVerts,
    .verts            = _gMappicS500ShrineShapeVerts,
    .normals          = &_gMappicS500ShrineShapeVerts[ARRAY_SIZE(_gMappicS500ShrineShapeVerts)], // Empty normal pool at the vertex end.
    .skeleton         = _gMappicS500ShrineShapeSkeleton,
    .stream           = _gMappicS500ShrineShapeStream,
};

/// Identity transform and self-parented root for the Woodland path shape.
static TmdBone _gMappicS500WoodlandPathShapeSkeleton[] = {
#include "assets/mappic_s5_00_model_00B50_skeleton.inc"
};

/// Vertex count of the Woodland path shape's sole part (twelve vertices).
static u32 _gMappicS500WoodlandPathShapePartVerts[] = {
#include "assets/mappic_s5_00_model_00B50_partVerts.inc"
};

/// Woodland path's map footprint in integer model X/Z coordinates, with Y zero.
static SVECTOR _gMappicS500WoodlandPathShapeVerts[] = {
#include "assets/mappic_s5_00_model_00B50_verts.inc"
};

/// Five quads with packed vertex byte offsets and unlit white colour.
/// Two group ends and the stream end follow the polygon commands.
static u32 _gMappicS500WoodlandPathShapeStream[] = {
#include "assets/mappic_s5_00_model_00B50_stream.inc"
};

TmdSource gMappicS500WoodlandPathShape = {
    .bufferHalfBytes  = 5 * sizeof(POLY_F4),
    .partCount        = ARRAY_SIZE(_gMappicS500WoodlandPathShapeSkeleton),
    .partVertexCounts = _gMappicS500WoodlandPathShapePartVerts,
    .verts            = _gMappicS500WoodlandPathShapeVerts,
    .normals          = &_gMappicS500WoodlandPathShapeVerts[ARRAY_SIZE(_gMappicS500WoodlandPathShapeVerts)], // Empty normal pool at the vertex end.
    .skeleton         = _gMappicS500WoodlandPathShapeSkeleton,
    .stream           = _gMappicS500WoodlandPathShapeStream,
};

/// Identity transform and self-parented root for the Bridge shape.
static TmdBone _gMappicS500BridgeShapeSkeleton[] = {
#include "assets/mappic_s2_02_model_00910_00C84_skeleton.inc"
};

/// Vertex count of the Bridge shape's sole part (eight vertices).
static u32 _gMappicS500BridgeShapePartVerts[] = {
#include "assets/mappic_s2_02_model_00910_00C84_partVerts.inc"
};

/// Bridge's map footprint in integer model X/Z coordinates, with Y zero.
static SVECTOR _gMappicS500BridgeShapeVerts[] = {
#include "assets/mappic_s2_02_model_00910_00C84_verts.inc"
};

/// Two triangles and two quads with packed vertex byte offsets and unlit white colour.
/// Two group ends and the stream end follow the polygon commands.
static u32 _gMappicS500BridgeShapeStream[] = {
#include "assets/mappic_s2_02_model_00910_00C84_stream.inc"
};

TmdSource gMappicS500BridgeShape = {
    .bufferHalfBytes  = 2 * sizeof(POLY_F3) + 2 * sizeof(POLY_F4),
    .partCount        = ARRAY_SIZE(_gMappicS500BridgeShapeSkeleton),
    .partVertexCounts = _gMappicS500BridgeShapePartVerts,
    .verts            = _gMappicS500BridgeShapeVerts,
    .normals          = &_gMappicS500BridgeShapeVerts[ARRAY_SIZE(_gMappicS500BridgeShapeVerts)], // Empty normal pool at the vertex end.
    .skeleton         = _gMappicS500BridgeShapeSkeleton,
    .stream           = _gMappicS500BridgeShapeStream,
};

/// Identity transform and self-parented root for the Island shape in the altar variant.
static TmdBone _gMappicS500IslandAltarVariantShapeSkeleton[] = {
#include "assets/mappic_s5_00_model_00D28_skeleton.inc"
};

/// Eleven vertices belong to the Island altar variant's sole part.
static u32 _gMappicS500IslandAltarVariantShapePartVerts[] = {
#include "assets/mappic_s5_00_model_00D28_partVerts.inc"
};

/// Island's altar-variant footprint in integer model X/Z coordinates, with Y zero.
static SVECTOR _gMappicS500IslandAltarVariantShapeVerts[] = {
#include "assets/mappic_s5_00_model_00D28_verts.inc"
};

/// One triangle and four quads with packed vertex byte offsets and unlit white colour.
/// Two group ends and the stream end follow the polygon commands.
static u32 _gMappicS500IslandAltarVariantShapeStream[] = {
#include "assets/mappic_s5_00_model_00D28_stream.inc"
};

TmdSource gMappicS500IslandAltarVariantShape = {
    .bufferHalfBytes  = sizeof(POLY_F3) + 4 * sizeof(POLY_F4),
    .partCount        = ARRAY_SIZE(_gMappicS500IslandAltarVariantShapeSkeleton),
    .partVertexCounts = _gMappicS500IslandAltarVariantShapePartVerts,
    .verts            = _gMappicS500IslandAltarVariantShapeVerts,
    .normals          = &_gMappicS500IslandAltarVariantShapeVerts[ARRAY_SIZE(_gMappicS500IslandAltarVariantShapeVerts)], // Empty normal pool at the vertex end.
    .skeleton         = _gMappicS500IslandAltarVariantShapeSkeleton,
    .stream           = _gMappicS500IslandAltarVariantShapeStream,
};

/// Identity transform and self-parented root for the Bridge shape in the altar variant.
static TmdBone _gMappicS500BridgeAltarVariantShapeSkeleton[] = {
#include "assets/mappic_s5_00_model_00E0C_skeleton.inc"
};

/// Seven vertices belong to the Bridge altar variant's sole part.
static u32 _gMappicS500BridgeAltarVariantShapePartVerts[] = {
#include "assets/mappic_s5_00_model_00E0C_partVerts.inc"
};

/// Bridge's altar-variant footprint in integer model X/Z coordinates, with Y zero.
static SVECTOR _gMappicS500BridgeAltarVariantShapeVerts[] = {
#include "assets/mappic_s5_00_model_00E0C_verts.inc"
};

/// One triangle and two quads with packed vertex byte offsets and unlit white colour.
/// Two group ends and the stream end follow the polygon commands.
static u32 _gMappicS500BridgeAltarVariantShapeStream[] = {
#include "assets/mappic_s5_00_model_00E0C_stream.inc"
};

TmdSource gMappicS500BridgeAltarVariantShape = {
    .bufferHalfBytes  = sizeof(POLY_F3) + 2 * sizeof(POLY_F4),
    .partCount        = ARRAY_SIZE(_gMappicS500BridgeAltarVariantShapeSkeleton),
    .partVertexCounts = _gMappicS500BridgeAltarVariantShapePartVerts,
    .verts            = _gMappicS500BridgeAltarVariantShapeVerts,
    .normals          = &_gMappicS500BridgeAltarVariantShapeVerts[ARRAY_SIZE(_gMappicS500BridgeAltarVariantShapeVerts)], // Empty normal pool at the vertex end.
    .skeleton         = _gMappicS500BridgeAltarVariantShapeSkeleton,
    .stream           = _gMappicS500BridgeAltarVariantShapeStream,
};

/// Identity transform and self-parented root for the Pyramid shape.
static TmdBone _gMappicS500PyramidShapeSkeleton[] = {
#include "assets/mappic_s2_02_model_00CF8_00F1C_skeleton.inc"
};

/// Vertex count of the Pyramid shape's sole part (eight vertices).
static u32 _gMappicS500PyramidShapePartVerts[] = {
#include "assets/mappic_s2_02_model_00CF8_00F1C_partVerts.inc"
};

/// Pyramid's map footprint in integer model X/Z coordinates, with Y zero.
static SVECTOR _gMappicS500PyramidShapeVerts[] = {
#include "assets/mappic_s2_02_model_00CF8_00F1C_verts.inc"
};

/// Three quads with packed vertex byte offsets and unlit white colour.
/// Two group ends and the stream end follow the polygon commands.
static u32 _gMappicS500PyramidShapeStream[] = {
#include "assets/mappic_s2_02_model_00CF8_00F1C_stream.inc"
};

TmdSource gMappicS500PyramidShape = {
    .bufferHalfBytes  = 3 * sizeof(POLY_F4),
    .partCount        = ARRAY_SIZE(_gMappicS500PyramidShapeSkeleton),
    .partVertexCounts = _gMappicS500PyramidShapePartVerts,
    .verts            = _gMappicS500PyramidShapeVerts,
    .normals          = &_gMappicS500PyramidShapeVerts[ARRAY_SIZE(_gMappicS500PyramidShapeVerts)], // Empty normal pool at the vertex end.
    .skeleton         = _gMappicS500PyramidShapeSkeleton,
    .stream           = _gMappicS500PyramidShapeStream,
};
