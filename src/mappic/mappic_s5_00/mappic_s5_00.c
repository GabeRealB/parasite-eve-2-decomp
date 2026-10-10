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

static TmdBone _gMappicS500MappicS406Model0014C00560Skeleton[1] = {
#include "assets/mappic_s4_06_model_0014C_00560_skeleton.inc"
};

static u32 _gMappicS500MappicS406Model0014C00560PartVerts[1] = {
#include "assets/mappic_s4_06_model_0014C_00560_partVerts.inc"
};

static SVECTOR _gMappicS500MappicS406Model0014C00560Verts[8] = {
#include "assets/mappic_s4_06_model_0014C_00560_verts.inc"
};

static u32 _gMappicS500MappicS406Model0014C00560Stream[15] = {
#include "assets/mappic_s4_06_model_0014C_00560_stream.inc"
};

TmdSource D_mappic_s5_00_8012F490 = {
    0,
    72,
    0,
    1,
    _gMappicS500MappicS406Model0014C00560PartVerts,
    _gMappicS500MappicS406Model0014C00560Verts,
    &_gMappicS500MappicS406Model0014C00560Verts[8],
    _gMappicS500MappicS406Model0014C00560Skeleton,
    _gMappicS500MappicS406Model0014C00560Stream,
};

static TmdBone _gMappicS500Model005FCSkeleton[1] = {
#include "assets/mappic_s5_00_model_005FC_skeleton.inc"
};

static u32 _gMappicS500Model005FCPartVerts[1] = {
#include "assets/mappic_s5_00_model_005FC_partVerts.inc"
};

static SVECTOR _gMappicS500Model005FCVerts[10] = {
#include "assets/mappic_s5_00_model_005FC_verts.inc"
};

static u32 _gMappicS500Model005FCStream[24] = {
#include "assets/mappic_s5_00_model_005FC_stream.inc"
};

TmdSource D_mappic_s5_00_8012F58C = {
    0,
    112,
    0,
    1,
    _gMappicS500Model005FCPartVerts,
    _gMappicS500Model005FCVerts,
    &_gMappicS500Model005FCVerts[10],
    _gMappicS500Model005FCSkeleton,
    _gMappicS500Model005FCStream,
};

static TmdBone _gMappicS500MappicS202Model00C48006ECSkeleton[1] = {
#include "assets/mappic_s2_02_model_00C48_006EC_skeleton.inc"
};

static u32 _gMappicS500MappicS202Model00C48006ECPartVerts[1] = {
#include "assets/mappic_s2_02_model_00C48_006EC_partVerts.inc"
};

static SVECTOR _gMappicS500MappicS202Model00C48006ECVerts[4] = {
#include "assets/mappic_s2_02_model_00C48_006EC_verts.inc"
};

static u32 _gMappicS500MappicS202Model00C48006ECStream[9] = {
#include "assets/mappic_s2_02_model_00C48_006EC_stream.inc"
};

TmdSource D_mappic_s5_00_8012F61C = {
    0,
    24,
    0,
    1,
    _gMappicS500MappicS202Model00C48006ECPartVerts,
    _gMappicS500MappicS202Model00C48006ECVerts,
    &_gMappicS500MappicS202Model00C48006ECVerts[4],
    _gMappicS500MappicS202Model00C48006ECSkeleton,
    _gMappicS500MappicS202Model00C48006ECStream,
};

static TmdBone _gMappicS500MappicS202Model00C480077CSkeleton[1] = {
#include "assets/mappic_s2_02_model_00C48_0077C_skeleton.inc"
};

static u32 _gMappicS500MappicS202Model00C480077CPartVerts[1] = {
#include "assets/mappic_s2_02_model_00C48_0077C_partVerts.inc"
};

static SVECTOR _gMappicS500MappicS202Model00C480077CVerts[4] = {
#include "assets/mappic_s2_02_model_00C48_0077C_verts.inc"
};

static u32 _gMappicS500MappicS202Model00C480077CStream[9] = {
#include "assets/mappic_s2_02_model_00C48_0077C_stream.inc"
};

TmdSource D_mappic_s5_00_8012F6AC = {
    0,
    24,
    0,
    1,
    _gMappicS500MappicS202Model00C480077CPartVerts,
    _gMappicS500MappicS202Model00C480077CVerts,
    &_gMappicS500MappicS202Model00C480077CVerts[4],
    _gMappicS500MappicS202Model00C480077CSkeleton,
    _gMappicS500MappicS202Model00C480077CStream,
};

static TmdBone _gMappicS500Model00840Skeleton[1] = {
#include "assets/mappic_s5_00_model_00840_skeleton.inc"
};

static u32 _gMappicS500Model00840PartVerts[1] = {
#include "assets/mappic_s5_00_model_00840_partVerts.inc"
};

static SVECTOR _gMappicS500Model00840Verts[15] = {
#include "assets/mappic_s5_00_model_00840_verts.inc"
};

static u32 _gMappicS500Model00840Stream[30] = {
#include "assets/mappic_s5_00_model_00840_stream.inc"
};

TmdSource D_mappic_s5_00_8012F7E8 = {
    0,
    164,
    0,
    1,
    _gMappicS500Model00840PartVerts,
    _gMappicS500Model00840Verts,
    &_gMappicS500Model00840Verts[15],
    _gMappicS500Model00840Skeleton,
    _gMappicS500Model00840Stream,
};

static TmdBone _gMappicS500MappicS202Model00C4800948Skeleton[1] = {
#include "assets/mappic_s2_02_model_00C48_00948_skeleton.inc"
};

static u32 _gMappicS500MappicS202Model00C4800948PartVerts[1] = {
#include "assets/mappic_s2_02_model_00C48_00948_partVerts.inc"
};

static SVECTOR _gMappicS500MappicS202Model00C4800948Verts[4] = {
#include "assets/mappic_s2_02_model_00C48_00948_verts.inc"
};

static u32 _gMappicS500MappicS202Model00C4800948Stream[9] = {
#include "assets/mappic_s2_02_model_00C48_00948_stream.inc"
};

TmdSource D_mappic_s5_00_8012F878 = {
    0,
    24,
    0,
    1,
    _gMappicS500MappicS202Model00C4800948PartVerts,
    _gMappicS500MappicS202Model00C4800948Verts,
    &_gMappicS500MappicS202Model00C4800948Verts[4],
    _gMappicS500MappicS202Model00C4800948Skeleton,
    _gMappicS500MappicS202Model00C4800948Stream,
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
