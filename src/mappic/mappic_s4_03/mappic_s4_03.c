#include "mappic/mappic.h"

#include <psyq/libgpu.h>

#include "common.h"

/// Identity transform and self-parented root for the B1 elevator hall shape.
static TmdBone _gMappicS403B1ElevatorHallShapeSkeleton[] = {
#include "assets/mappic_s4_06_model_0014C_skeleton.inc"
};

/// Vertex count of the B1 elevator hall shape's sole part (eight vertices).
static u32 _gMappicS403B1ElevatorHallShapePartVerts[] = {
#include "assets/mappic_s4_06_model_0014C_partVerts.inc"
};

/// B1 elevator hall's map footprint in integer model X/Z coordinates, with Y zero.
static SVECTOR _gMappicS403B1ElevatorHallShapeVerts[] = {
#include "assets/mappic_s4_06_model_0014C_verts.inc"
};

/// Three unlit quads' packed vertex byte offsets and white colour, followed by two group ends and the stream end.
static u32 _gMappicS403B1ElevatorHallShapeStream[] = {
#include "assets/mappic_s4_06_model_0014C_stream.inc"
};

TmdSource gMappicS403B1ElevatorHallShape = {
    .bufferHalfBytes  = 3 * sizeof(POLY_F4),
    .partCount        = ARRAY_SIZE(_gMappicS403B1ElevatorHallShapeSkeleton),
    .partVertexCounts = _gMappicS403B1ElevatorHallShapePartVerts,
    .verts            = _gMappicS403B1ElevatorHallShapeVerts,
    .normals          = &_gMappicS403B1ElevatorHallShapeVerts[ARRAY_SIZE(_gMappicS403B1ElevatorHallShapeVerts)], // Empty normal pool at the vertex end.
    .skeleton         = _gMappicS403B1ElevatorHallShapeSkeleton,
    .stream           = _gMappicS403B1ElevatorHallShapeStream,
};

/// Identity transform and self-parented root for the B1 south maintenance walkway shape.
static TmdBone _gMappicS403B1SouthMaintenanceWalkwayShapeSkeleton[] = {
#include "assets/mappic_s4_00_model_0005C_00154_mappic_s4_03_skeleton.inc"
};

/// Vertex count of the B1 south maintenance walkway shape's sole part (six vertices).
static u32 _gMappicS403B1SouthMaintenanceWalkwayShapePartVerts[] = {
#include "assets/mappic_s4_00_model_0005C_00154_mappic_s4_03_partVerts.inc"
};

/// B1 south maintenance walkway's map footprint in integer model X/Z coordinates, with Y zero.
static SVECTOR _gMappicS403B1SouthMaintenanceWalkwayShapeVerts[] = {
#include "assets/mappic_s4_00_model_0005C_00154_mappic_s4_03_verts.inc"
};

/// Two unlit quads' packed vertex byte offsets and white colour, followed by two group ends and the stream end.
static u32 _gMappicS403B1SouthMaintenanceWalkwayShapeStream[] = {
#include "assets/mappic_s4_00_model_0005C_00154_mappic_s4_03_stream.inc"
};

TmdSource gMappicS403B1SouthMaintenanceWalkwayShape = {
    .bufferHalfBytes  = 2 * sizeof(POLY_F4),
    .partCount        = ARRAY_SIZE(_gMappicS403B1SouthMaintenanceWalkwayShapeSkeleton),
    .partVertexCounts = _gMappicS403B1SouthMaintenanceWalkwayShapePartVerts,
    .verts            = _gMappicS403B1SouthMaintenanceWalkwayShapeVerts,
    .normals          = &_gMappicS403B1SouthMaintenanceWalkwayShapeVerts[ARRAY_SIZE(_gMappicS403B1SouthMaintenanceWalkwayShapeVerts)], // Empty normal pool at the vertex end.
    .skeleton         = _gMappicS403B1SouthMaintenanceWalkwayShapeSkeleton,
    .stream           = _gMappicS403B1SouthMaintenanceWalkwayShapeStream,
};

/// Identity transform and self-parented root for the B1 storeroom shape.
static TmdBone _gMappicS403B1StoreroomShapeSkeleton[] = {
#include "assets/mappic_s2_02_model_00C48_001E4_skeleton.inc"
};

/// Vertex count of the B1 storeroom shape's sole part (four vertices).
static u32 _gMappicS403B1StoreroomShapePartVerts[] = {
#include "assets/mappic_s2_02_model_00C48_001E4_partVerts.inc"
};

/// B1 storeroom's map footprint in integer model X/Z coordinates, with Y zero.
static SVECTOR _gMappicS403B1StoreroomShapeVerts[] = {
#include "assets/mappic_s2_02_model_00C48_001E4_verts.inc"
};

/// One unlit quad's packed vertex byte offsets and white colour, followed by two group ends and the stream end.
static u32 _gMappicS403B1StoreroomShapeStream[] = {
#include "assets/mappic_s2_02_model_00C48_001E4_stream.inc"
};

TmdSource gMappicS403B1StoreroomShape = {
    .bufferHalfBytes  = sizeof(POLY_F4),
    .partCount        = ARRAY_SIZE(_gMappicS403B1StoreroomShapeSkeleton),
    .partVertexCounts = _gMappicS403B1StoreroomShapePartVerts,
    .verts            = _gMappicS403B1StoreroomShapeVerts,
    .normals          = &_gMappicS403B1StoreroomShapeVerts[ARRAY_SIZE(_gMappicS403B1StoreroomShapeVerts)], // Empty normal pool at the vertex end.
    .skeleton         = _gMappicS403B1StoreroomShapeSkeleton,
    .stream           = _gMappicS403B1StoreroomShapeStream,
};

/// Identity transform and self-parented root for the B1 north maintenance walkway shape.
static TmdBone _gMappicS403B1NorthMaintenanceWalkwayShapeSkeleton[] = {
#include "assets/mappic_s4_00_model_0005C_00290_skeleton.inc"
};

/// Vertex count of the B1 north maintenance walkway shape's sole part (six vertices).
static u32 _gMappicS403B1NorthMaintenanceWalkwayShapePartVerts[] = {
#include "assets/mappic_s4_00_model_0005C_00290_partVerts.inc"
};

/// B1 north maintenance walkway's map footprint in integer model X/Z coordinates, with Y zero.
static SVECTOR _gMappicS403B1NorthMaintenanceWalkwayShapeVerts[] = {
#include "assets/mappic_s4_00_model_0005C_00290_verts.inc"
};

/// Two unlit quads' packed vertex byte offsets and white colour, followed by two group ends and the stream end.
static u32 _gMappicS403B1NorthMaintenanceWalkwayShapeStream[] = {
#include "assets/mappic_s4_00_model_0005C_00290_stream.inc"
};

TmdSource gMappicS403B1NorthMaintenanceWalkwayShape = {
    .bufferHalfBytes  = 2 * sizeof(POLY_F4),
    .partCount        = ARRAY_SIZE(_gMappicS403B1NorthMaintenanceWalkwayShapeSkeleton),
    .partVertexCounts = _gMappicS403B1NorthMaintenanceWalkwayShapePartVerts,
    .verts            = _gMappicS403B1NorthMaintenanceWalkwayShapeVerts,
    .normals          = &_gMappicS403B1NorthMaintenanceWalkwayShapeVerts[ARRAY_SIZE(_gMappicS403B1NorthMaintenanceWalkwayShapeVerts)], // Empty normal pool at the vertex end.
    .skeleton         = _gMappicS403B1NorthMaintenanceWalkwayShapeSkeleton,
    .stream           = _gMappicS403B1NorthMaintenanceWalkwayShapeStream,
};

/// Identity transform and self-parented root for the B1 armory shape.
static TmdBone _gMappicS403B1ArmoryShapeSkeleton[] = {
#include "assets/mappic_s4_00_model_0005C_0033C_skeleton.inc"
};

/// Vertex count of the B1 armory shape's sole part (six vertices).
static u32 _gMappicS403B1ArmoryShapePartVerts[] = {
#include "assets/mappic_s4_00_model_0005C_0033C_partVerts.inc"
};

/// B1 armory's map footprint in integer model X/Z coordinates, with Y zero.
static SVECTOR _gMappicS403B1ArmoryShapeVerts[] = {
#include "assets/mappic_s4_00_model_0005C_0033C_verts.inc"
};

/// Two unlit quads' packed vertex byte offsets and white colour, followed by two group ends and the stream end.
static u32 _gMappicS403B1ArmoryShapeStream[] = {
#include "assets/mappic_s4_00_model_0005C_0033C_stream.inc"
};

TmdSource gMappicS403B1ArmoryShape = {
    .bufferHalfBytes  = 2 * sizeof(POLY_F4),
    .partCount        = ARRAY_SIZE(_gMappicS403B1ArmoryShapeSkeleton),
    .partVertexCounts = _gMappicS403B1ArmoryShapePartVerts,
    .verts            = _gMappicS403B1ArmoryShapeVerts,
    .normals          = &_gMappicS403B1ArmoryShapeVerts[ARRAY_SIZE(_gMappicS403B1ArmoryShapeVerts)], // Empty normal pool at the vertex end.
    .skeleton         = _gMappicS403B1ArmoryShapeSkeleton,
    .stream           = _gMappicS403B1ArmoryShapeStream,
};

/// Identity transform and self-parented root for the B1 sleeping quarters shape.
static TmdBone _gMappicS403B1SleepingQuartersShapeSkeleton[] = {
#include "assets/mappic_s4_00_model_0005C_003E8_skeleton.inc"
};

/// Vertex count of the B1 sleeping quarters shape's sole part (six vertices).
static u32 _gMappicS403B1SleepingQuartersShapePartVerts[] = {
#include "assets/mappic_s4_00_model_0005C_003E8_partVerts.inc"
};

/// B1 sleeping quarters' map footprint in integer model X/Z coordinates, with Y zero.
static SVECTOR _gMappicS403B1SleepingQuartersShapeVerts[] = {
#include "assets/mappic_s4_00_model_0005C_003E8_verts.inc"
};

/// Two unlit quads' packed vertex byte offsets and white colour, followed by two group ends and the stream end.
static u32 _gMappicS403B1SleepingQuartersShapeStream[] = {
#include "assets/mappic_s4_00_model_0005C_003E8_stream.inc"
};

TmdSource gMappicS403B1SleepingQuartersShape = {
    .bufferHalfBytes  = 2 * sizeof(POLY_F4),
    .partCount        = ARRAY_SIZE(_gMappicS403B1SleepingQuartersShapeSkeleton),
    .partVertexCounts = _gMappicS403B1SleepingQuartersShapePartVerts,
    .verts            = _gMappicS403B1SleepingQuartersShapeVerts,
    .normals          = &_gMappicS403B1SleepingQuartersShapeVerts[ARRAY_SIZE(_gMappicS403B1SleepingQuartersShapeVerts)], // Empty normal pool at the vertex end.
    .skeleton         = _gMappicS403B1SleepingQuartersShapeSkeleton,
    .stream           = _gMappicS403B1SleepingQuartersShapeStream,
};

static TmdBone _gMappicS403MappicS202Model00910004C8Skeleton[1] = {
#include "assets/mappic_s2_02_model_00910_004C8_skeleton.inc"
};

static u32 _gMappicS403MappicS202Model00910004C8PartVerts[1] = {
#include "assets/mappic_s2_02_model_00910_004C8_partVerts.inc"
};

static SVECTOR _gMappicS403MappicS202Model00910004C8Verts[8] = {
#include "assets/mappic_s2_02_model_00910_004C8_verts.inc"
};

static u32 _gMappicS403MappicS202Model00910004C8Stream[21] = {
#include "assets/mappic_s2_02_model_00910_004C8_stream.inc"
};

TmdSource D_mappic_s4_03_8012F3F8 = {
    0,
    88,
    0,
    1,
    _gMappicS403MappicS202Model00910004C8PartVerts,
    _gMappicS403MappicS202Model00910004C8Verts,
    &_gMappicS403MappicS202Model00910004C8Verts[8],
    _gMappicS403MappicS202Model00910004C8Skeleton,
    _gMappicS403MappicS202Model00910004C8Stream,
};

static TmdBone _gMappicS403MappicS202Model00C4800558Skeleton[1] = {
#include "assets/mappic_s2_02_model_00C48_00558_skeleton.inc"
};

static u32 _gMappicS403MappicS202Model00C4800558PartVerts[1] = {
#include "assets/mappic_s2_02_model_00C48_00558_partVerts.inc"
};

static SVECTOR _gMappicS403MappicS202Model00C4800558Verts[4] = {
#include "assets/mappic_s2_02_model_00C48_00558_verts.inc"
};

static u32 _gMappicS403MappicS202Model00C4800558Stream[9] = {
#include "assets/mappic_s2_02_model_00C48_00558_stream.inc"
};

TmdSource D_mappic_s4_03_8012F488 = {
    0,
    24,
    0,
    1,
    _gMappicS403MappicS202Model00C4800558PartVerts,
    _gMappicS403MappicS202Model00C4800558Verts,
    &_gMappicS403MappicS202Model00C4800558Verts[4],
    _gMappicS403MappicS202Model00C4800558Skeleton,
    _gMappicS403MappicS202Model00C4800558Stream,
};

static TmdBone _gMappicS403MappicS203Model001F000654Skeleton[1] = {
#include "assets/mappic_s2_03_model_001F0_00654_skeleton.inc"
};

static u32 _gMappicS403MappicS203Model001F000654PartVerts[1] = {
#include "assets/mappic_s2_03_model_001F0_00654_partVerts.inc"
};

static SVECTOR _gMappicS403MappicS203Model001F000654Verts[10] = {
#include "assets/mappic_s2_03_model_001F0_00654_verts.inc"
};

static u32 _gMappicS403MappicS203Model001F000654Stream[24] = {
#include "assets/mappic_s2_03_model_001F0_00654_stream.inc"
};

TmdSource D_mappic_s4_03_8012F584 = {
    0,
    112,
    0,
    1,
    _gMappicS403MappicS203Model001F000654PartVerts,
    _gMappicS403MappicS203Model001F000654Verts,
    &_gMappicS403MappicS203Model001F000654Verts[10],
    _gMappicS403MappicS203Model001F000654Skeleton,
    _gMappicS403MappicS203Model001F000654Stream,
};

static TmdBone _gMappicS403MappicS406Model00214Skeleton[1] = {
#include "assets/mappic_s4_06_model_00214_skeleton.inc"
};

static u32 _gMappicS403MappicS406Model00214PartVerts[1] = {
#include "assets/mappic_s4_06_model_00214_partVerts.inc"
};

static SVECTOR _gMappicS403MappicS406Model00214Verts[8] = {
#include "assets/mappic_s4_06_model_00214_verts.inc"
};

static u32 _gMappicS403MappicS406Model00214Stream[21] = {
#include "assets/mappic_s4_06_model_00214_stream.inc"
};

TmdSource D_mappic_s4_03_8012F664 = {
    0,
    88,
    0,
    1,
    _gMappicS403MappicS406Model00214PartVerts,
    _gMappicS403MappicS406Model00214Verts,
    &_gMappicS403MappicS406Model00214Verts[8],
    _gMappicS403MappicS406Model00214Skeleton,
    _gMappicS403MappicS406Model00214Stream,
};

static TmdBone _gMappicS403MappicS400Model0005C007E0Skeleton[1] = {
#include "assets/mappic_s4_00_model_0005C_007E0_skeleton.inc"
};

static u32 _gMappicS403MappicS400Model0005C007E0PartVerts[1] = {
#include "assets/mappic_s4_00_model_0005C_007E0_partVerts.inc"
};

static SVECTOR _gMappicS403MappicS400Model0005C007E0Verts[6] = {
#include "assets/mappic_s4_00_model_0005C_007E0_verts.inc"
};

static u32 _gMappicS403MappicS400Model0005C007E0Stream[12] = {
#include "assets/mappic_s4_00_model_0005C_007E0_stream.inc"
};

TmdSource D_mappic_s4_03_8012F710 = {
    0,
    48,
    0,
    1,
    _gMappicS403MappicS400Model0005C007E0PartVerts,
    _gMappicS403MappicS400Model0005C007E0Verts,
    &_gMappicS403MappicS400Model0005C007E0Verts[6],
    _gMappicS403MappicS400Model0005C007E0Skeleton,
    _gMappicS403MappicS400Model0005C007E0Stream,
};

static TmdBone _gMappicS403MappicS202Model00910008C0Skeleton[1] = {
#include "assets/mappic_s2_02_model_00910_008C0_skeleton.inc"
};

static u32 _gMappicS403MappicS202Model00910008C0PartVerts[1] = {
#include "assets/mappic_s2_02_model_00910_008C0_partVerts.inc"
};

static SVECTOR _gMappicS403MappicS202Model00910008C0Verts[8] = {
#include "assets/mappic_s2_02_model_00910_008C0_verts.inc"
};

static u32 _gMappicS403MappicS202Model00910008C0Stream[21] = {
#include "assets/mappic_s2_02_model_00910_008C0_stream.inc"
};

TmdSource D_mappic_s4_03_8012F7F0 = {
    0,
    88,
    0,
    1,
    _gMappicS403MappicS202Model00910008C0PartVerts,
    _gMappicS403MappicS202Model00910008C0Verts,
    &_gMappicS403MappicS202Model00910008C0Verts[8],
    _gMappicS403MappicS202Model00910008C0Skeleton,
    _gMappicS403MappicS202Model00910008C0Stream,
};

static TmdBone _gMappicS403MappicS406Model00214009A0Skeleton[1] = {
#include "assets/mappic_s4_06_model_00214_009A0_skeleton.inc"
};

static u32 _gMappicS403MappicS406Model00214009A0PartVerts[1] = {
#include "assets/mappic_s4_06_model_00214_009A0_partVerts.inc"
};

static SVECTOR _gMappicS403MappicS406Model00214009A0Verts[8] = {
#include "assets/mappic_s4_06_model_00214_009A0_verts.inc"
};

static u32 _gMappicS403MappicS406Model00214009A0Stream[21] = {
#include "assets/mappic_s4_06_model_00214_009A0_stream.inc"
};

TmdSource D_mappic_s4_03_8012F8D0 = {
    0,
    88,
    0,
    1,
    _gMappicS403MappicS406Model00214009A0PartVerts,
    _gMappicS403MappicS406Model00214009A0Verts,
    &_gMappicS403MappicS406Model00214009A0Verts[8],
    _gMappicS403MappicS406Model00214009A0Skeleton,
    _gMappicS403MappicS406Model00214009A0Stream,
};

static TmdBone _gMappicS403MappicS202Model00C4800A30Skeleton[1] = {
#include "assets/mappic_s2_02_model_00C48_00A30_skeleton.inc"
};

static u32 _gMappicS403MappicS202Model00C4800A30PartVerts[1] = {
#include "assets/mappic_s2_02_model_00C48_00A30_partVerts.inc"
};

static SVECTOR _gMappicS403MappicS202Model00C4800A30Verts[4] = {
#include "assets/mappic_s2_02_model_00C48_00A30_verts.inc"
};

static u32 _gMappicS403MappicS202Model00C4800A30Stream[9] = {
#include "assets/mappic_s2_02_model_00C48_00A30_stream.inc"
};

TmdSource D_mappic_s4_03_8012F960 = {
    0,
    24,
    0,
    1,
    _gMappicS403MappicS202Model00C4800A30PartVerts,
    _gMappicS403MappicS202Model00C4800A30Verts,
    &_gMappicS403MappicS202Model00C4800A30Verts[4],
    _gMappicS403MappicS202Model00C4800A30Skeleton,
    _gMappicS403MappicS202Model00C4800A30Stream,
};

static TmdBone _gMappicS403MappicS202Model00C4800AC0Skeleton[1] = {
#include "assets/mappic_s2_02_model_00C48_00AC0_skeleton.inc"
};

static u32 _gMappicS403MappicS202Model00C4800AC0PartVerts[1] = {
#include "assets/mappic_s2_02_model_00C48_00AC0_partVerts.inc"
};

static SVECTOR _gMappicS403MappicS202Model00C4800AC0Verts[4] = {
#include "assets/mappic_s2_02_model_00C48_00AC0_verts.inc"
};

static u32 _gMappicS403MappicS202Model00C4800AC0Stream[9] = {
#include "assets/mappic_s2_02_model_00C48_00AC0_stream.inc"
};

TmdSource D_mappic_s4_03_8012F9F0 = {
    0,
    24,
    0,
    1,
    _gMappicS403MappicS202Model00C4800AC0PartVerts,
    _gMappicS403MappicS202Model00C4800AC0Verts,
    &_gMappicS403MappicS202Model00C4800AC0Verts[4],
    _gMappicS403MappicS202Model00C4800AC0Skeleton,
    _gMappicS403MappicS202Model00C4800AC0Stream,
};

static TmdBone _gMappicS403Model00B5CSkeleton[1] = {
#include "assets/mappic_s4_03_model_00B5C_skeleton.inc"
};

static u32 _gMappicS403Model00B5CPartVerts[1] = {
#include "assets/mappic_s4_03_model_00B5C_partVerts.inc"
};

static SVECTOR _gMappicS403Model00B5CVerts[10] = {
#include "assets/mappic_s4_03_model_00B5C_verts.inc"
};

static u32 _gMappicS403Model00B5CStream[24] = {
#include "assets/mappic_s4_03_model_00B5C_stream.inc"
};

TmdSource D_mappic_s4_03_8012FAEC = {
    0,
    112,
    0,
    1,
    _gMappicS403Model00B5CPartVerts,
    _gMappicS403Model00B5CVerts,
    &_gMappicS403Model00B5CVerts[10],
    _gMappicS403Model00B5CSkeleton,
    _gMappicS403Model00B5CStream,
};
