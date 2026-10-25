#include "rooms/dryfield_night_water_tower.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>

#include "gte.h"
#include "common.h"

#include "dryfield_night_water_tower_private.h"

#include "actors/actor_04400.h"
#include "actors/actor_01600.h"
#include "actors/actor_02500.h"
#include "actors/actor_300700.h"
#include "actors/actor_400600.h"

#include "gameplay/area.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gfx.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound_ids.h"
#include "main/sound_types.h"
#include "main/task_types.h"

#include "mapui/map_dryfield_full.h"

#include "overlay.h"

#include "rooms/room_common.h"
#include "../../shared/effects/glow_draw.h"

// The flag symbol is four bytes; the gate writes the first.
#define ROOM_EVENT_ACTIVE gRoomEventActive.eventStarted
#include "../../shared/room/room_events.h"
#include "../../shared/water_tower.h"

/// Shaft endpoints and flare anchors in signed whole world-coordinate units.
///
/// Drawing borrows these fixed positions only for the call. Each `SVECTOR`
/// includes the fourth halfword read alongside Z by the GTE.
typedef struct {
    SVECTOR shaftEndpoints[2];    // Endpoint pair of the shaft in logical views 3 and 4
    SVECTOR views3And4FlarePoint; // Flare centre in logical views 3 and 4
    SVECTOR view5FlarePoint;      // Flare centre in logical view 5
    SVECTOR views2And4FlarePoint; // Flare centre in logical views 2 and 4
} _DryfieldNightWaterTowerGlowPoints;
STATIC_ASSERT_SIZEOF(_DryfieldNightWaterTowerGlowPoints, 0x28);

static WorldCollisionGrid    _gDryfieldNightWaterTowerCollisionGrid;
static WorldCollisionTrigger _gDryfieldNightWaterTowerViewBoundaryTriggers[14];
static WorldCollisionTrigger _gDryfieldNightWaterTowerActionTriggers[9];
static WorldCoordRoomLights  _gDryfieldNightWaterTowerRoomLights;

TaskDesc gRoomEventTaskDesc = { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, roomEventTask, { .value = 0 } };

TaskMessageEntry gDryfieldNightWaterTowerRoomMessages[6] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, roomVariantWaterTowerMsg },
    { ROOM_MESSAGE_USE_KEY_ITEM, dryfieldNightWaterTowerRejectKeyItemUse },
    { DIRECTION_MESSAGE_ROOM_ACTION, dryfieldNightWaterTowerIgnoreActionMessage },
    { ROOM_MESSAGE_SOUND, waterTowerSoundMsg },
    { ROOM_MESSAGE_COMMAND, dryfieldNightWaterTowerCommandMessage },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// World-space shaft endpoints and single-flare anchors for the night Water Tower.
///
/// The glow task borrows these points during drawing while this overlay is loaded.
static _DryfieldNightWaterTowerGlowPoints _gDryfieldNightWaterTowerGlowPoints = {
    .shaftEndpoints       = { { -2850, -2160, -5970, 0 }, { -2390, -2160, -5970, 0 } },
    .views3And4FlarePoint = { -5020, -3050, -3120, 0 },
    .view5FlarePoint      = { -3190, -3050, 4630, 0 },
    .views2And4FlarePoint = { 4170, -3050, -3710, 0 },
};

/// World-space anchor for two coincident flare passes in views 7 and 10.
///
/// Signed whole game-coordinate units; borrowed only during the glow draw call.
static SVECTOR _gDryfieldNightWaterTowerDoubleFlarePoint = { 4290, -3050, 1810, 0 };

WorldCoordRoomLighting gDryfieldNightWaterTowerRoomLighting[1] = {
    { .lights = &_gDryfieldNightWaterTowerRoomLights, .ambientTable = NULL },
};

WorldCollisionRoomResources gDryfieldNightWaterTowerCollisionRooms[1] = {
    {
        .grid                 = &_gDryfieldNightWaterTowerCollisionGrid,
        .viewBoundaryTriggers = _gDryfieldNightWaterTowerViewBoundaryTriggers,
        .actionTriggers       = _gDryfieldNightWaterTowerActionTriggers,
        .occluders            = NULL,
    },
};

u8* gDryfieldNightWaterTowerViewMaps[1] = {
    gViewIdentityMap,
};

enum { DRYFIELD_NIGHT_WATER_TOWER_LOGICAL_VIEW_COUNT = 10 };

ViewCount gDryfieldNightWaterTowerViewCounts[1] = { DRYFIELD_NIGHT_WATER_TOWER_LOGICAL_VIEW_COUNT };

/// Night Water Tower endpoint IDs, trigger-byte packing and map-marker flags.
///
/// Trigger bytes put the departure endpoint in the high nibble and the
/// destination's arrival endpoint in the low nibble.
enum {
    DRYFIELD_NIGHT_WATER_TOWER_WARP_KITCHEN            = 1,
    DRYFIELD_NIGHT_WATER_TOWER_WARP_BREEZEWAY          = 2,
    DRYFIELD_NIGHT_WATER_TOWER_WARP_WATER_TANK         = 3,
    DRYFIELD_NIGHT_WATER_TOWER_WARP_MOTEL_ROOM_6       = 4,
    DRYFIELD_NIGHT_WATER_TOWER_WARP_ENDPOINT_SHIFT     = 4,
    DRYFIELD_NIGHT_WATER_TOWER_WARP_KITCHEN_MAP_FLAG   = 477,
    DRYFIELD_NIGHT_WATER_TOWER_WARP_BREEZEWAY_MAP_FLAG = 476,
};

DirectionWarpEntry gDryfieldNightWaterTowerWarpEntries[4] = {
    [DRYFIELD_NIGHT_WATER_TOWER_WARP_KITCHEN - 1] = {
        .player         = { { .word = 0 }, -2437, 0, -5662 },
        .unknown_10     = { 0, 0, 0, 0 },
        .companion      = { { .word = 0 }, -1900, 0, 600 },
        .unknown_24     = { 0, 0, 0, 0 },
        .arrivalSound   = SOUND_AREA(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_WATER_TOWER, 4),
        .departureSound = SOUND_AREA(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_WATER_TOWER, 3),
        .blockedSound   = DIRECTION_WARP_SOUND_NONE,
        .initialView    = 3,
        .flags          = DIRECTION_WARP_FLAG_NONE,
        .mapFlagId      = DRYFIELD_NIGHT_WATER_TOWER_WARP_KITCHEN_MAP_FLAG,
    },
    [DRYFIELD_NIGHT_WATER_TOWER_WARP_BREEZEWAY - 1] = {
        .player         = { { .word = 3072 }, 5646, 0, 1800 },
        .unknown_10     = { 0, 0, 0, 0 },
        .companion      = { { .word = 3072 }, 5646, 0, 1800 },
        .unknown_24     = { 0, 0, 0, 0 },
        .arrivalSound   = SOUND_AREA(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_WATER_TOWER, 2),
        .departureSound = SOUND_AREA(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_WATER_TOWER, 1),
        .blockedSound   = SOUND_AREA(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_WATER_TOWER, 8),
        .initialView    = 10,
        .flags          = DIRECTION_WARP_FLAG_NONE,
        .mapFlagId      = DRYFIELD_NIGHT_WATER_TOWER_WARP_BREEZEWAY_MAP_FLAG,
    },
    [DRYFIELD_NIGHT_WATER_TOWER_WARP_WATER_TANK - 1] = {
        .player         = { { .word = 1024 }, -2186, 1, 170 },
        .unknown_10     = { 0, 0, 0, 0 },
        .companion      = { { .word = 0 }, -1900, 0, 600 },
        .unknown_24     = { 0, 0, 0, 0 },
        .arrivalSound   = SOUND_AREA(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_WATER_TOWER, 5),
        .departureSound = SOUND_AREA(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_WATER_TOWER, 5),
        .blockedSound   = DIRECTION_WARP_SOUND_NONE,
        .initialView    = 8,
        .flags          = DIRECTION_WARP_FLAG_FADE_DEPARTURE,
        .mapFlagId      = DIRECTION_WARP_MAP_FLAG_NONE,
    },
    [DRYFIELD_NIGHT_WATER_TOWER_WARP_MOTEL_ROOM_6 - 1] = {
        .player         = { { .word = 3072 }, 5500, 1, -5310 },
        .unknown_10     = { 0, 0, 0, 0 },
        .companion      = { { .word = 0 }, -1900, 0, 600 },
        .unknown_24     = { 0, 0, 0, 0 },
        .arrivalSound   = DIRECTION_WARP_SOUND_NONE,
        .departureSound = SOUND_AREA(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_WATER_TOWER, 5),
        .blockedSound   = DIRECTION_WARP_SOUND_NONE,
        .initialView    = 2,
        .flags          = DIRECTION_WARP_FLAG_FADE_DEPARTURE,
        .mapFlagId      = DIRECTION_WARP_MAP_FLAG_NONE,
    },
};

/// Dimensions of the collision grid's X-major cell table.
enum {
    DRYFIELD_NIGHT_WATER_TOWER_COLLISION_CELL_COUNT_X = 4,
    DRYFIELD_NIGHT_WATER_TOWER_COLLISION_CELL_COUNT_Z = 4,
};

/// Room-space collision normals, with 4096 representing unit length.
static SVECTOR _gDryfieldNightWaterTowerCollisionGridNormals[25] = {
#include "assets/dryfield_night_water_tower_collision_01E34_normals.inc"
};

/// Room-space collision vertices in signed whole game-coordinate units.
static SVECTOR _gDryfieldNightWaterTowerCollisionGridVerts[159] = {
#include "assets/dryfield_night_water_tower_collision_01E34_verts.inc"
};

/// Indexed room collision quads with room-local surface classes.
static WorldCollisionGridFace _gDryfieldNightWaterTowerCollisionGridFaces[69] = {
#include "assets/dryfield_night_water_tower_collision_01E34_faces.inc"
};

/// Packed signed face-index lists, each ending at `WORLD_COLLISION_GRID_CELL_END`.
static s16 _gDryfieldNightWaterTowerCollisionGridCells[310] = {
#include "assets/dryfield_night_water_tower_collision_01E34_cells.inc"
};

/// Supplies this grid's cell face-list addresses to its generated table.
///
/// Captures `_gDryfieldNightWaterTowerCollisionGridCells`.
/// `halfwordOffset` is a constant s16-element offset in 0..309 to a list's
/// first entry, evaluated once; the list ends at WORLD_COLLISION_GRID_CELL_END.
/// Returns a borrowed s16* without modifying the pool. `cell_face_list_macro`
/// selects this binding for the table include; it is undefined afterwards.
#define DRYFIELD_NIGHT_WATER_TOWER_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gDryfieldNightWaterTowerCollisionGridCells[(halfwordOffset)])
/// X-major 4-by-4 directory of face-index lists borrowed by the collision grid.
static s16* _gDryfieldNightWaterTowerCollisionGridTable[DRYFIELD_NIGHT_WATER_TOWER_COLLISION_CELL_COUNT_X * DRYFIELD_NIGHT_WATER_TOWER_COLLISION_CELL_COUNT_Z] = {
#include "assets/dryfield_night_water_tower_collision_01E34_table.inc"
};
#undef DRYFIELD_NIGHT_WATER_TOWER_CELL_FACE_LIST_AT_OFFSET

/// Mutable room collision mesh and its 4-by-4 XZ face index.
///
/// Cells are 4000 game units wide with origin (-7000, -7000). Setup binds the
/// view coordinate before activation; keep this record and all five pools
/// loaded while active. The 69 faces use surface classes 0..2.
static WorldCollisionGrid _gDryfieldNightWaterTowerCollisionGrid = {
    .viewCoord   = NULL,
    .normals     = _gDryfieldNightWaterTowerCollisionGridNormals,
    .vertices    = _gDryfieldNightWaterTowerCollisionGridVerts,
    .faces       = _gDryfieldNightWaterTowerCollisionGridFaces,
    .cellFaceIds = _gDryfieldNightWaterTowerCollisionGridTable,
    .xBias       = 7000,
    .zBias       = 7000,
    .cellCountX  = DRYFIELD_NIGHT_WATER_TOWER_COLLISION_CELL_COUNT_X,
    .cellCountZ  = DRYFIELD_NIGHT_WATER_TOWER_COLLISION_CELL_COUNT_Z,
    .cellSize    = 4000,
    .faceCount   = ARRAY_SIZE(_gDryfieldNightWaterTowerCollisionGridFaces),
};

ViewCamera gDryfieldNightWaterTowerViewCameras[DRYFIELD_NIGHT_WATER_TOWER_LOGICAL_VIEW_COUNT] = {
    { { { { ONE, 0, 0 }, { 0, 0, -ONE }, { 0, ONE, 0 } }, { 0, 0x5DC0, 0 } }, 322 },
    { { { { 0, 0, -ONE }, { 23, 4095, 0 }, { 4095, -23, 0 } }, { 2600, 1169, 4500 } }, 257 },
    { { { { -119, 0, 4094 }, { -19, 4095, 0 }, { -4094, -19, -119 } }, { -2900, 1170, 4250 } }, 257 },
    { { { { -3811, 0, -1500 }, { -121, 4082, 307 }, { 1495, 330, -3798 } }, { 5810, 1779, -4040 } }, 257 },
    { { { { 4000, 0, -878 }, { 74, 4081, 338 }, { 875, -346, 3986 } }, { 5550, 1170, 2970 } }, 257 },
    { { { { 1168, 0, 3925 }, { 3159, 2431, -939 }, { -2330, 3296, 693 } }, { -4290, 6370, -3260 } }, 257 },
    { { { { 4001, 0, 876 }, { 118, 4058, -539 }, { -868, 552, 3964 } }, { -5510, 1500, 5900 } }, 257 },
    { { { { 4002, 0, -870 }, { -738, 2168, -3395 }, { 460, 3475, 2118 } }, { 700, 5770, 3420 } }, 230 },
    { { { { -3657, 0, 1843 }, { 0, ONE, 0 }, { -1843, 0, -3657 } }, { -1430, 1250, -4770 } }, 257 },
    { { { { 1485, 0, -3816 }, { -3580, 1417, -1394 }, { 1321, 3842, 514 } }, { -3517, 4818, -1078 } }, 257 },
};

/// View 1's empty sprite list, retaining decoded background-strip presentation.
///
/// The zero-count first record is followed by `SPRITE_BATCH_END`; the source
/// slot retains this list's address but supplies no `SpriteSource` elements.
static SpriteBatch _gDryfieldNightWaterTowerView1SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 2's 58 texture rectangles and sorting depths for scene sprites.
///
/// Positions and sizes are pixels; depths are unscaled OT sorting values.
/// Sources have neutral RGB modulation and opaque code flags, with no terminator.
/// The view's batches borrow element ranges while this overlay is loaded.
static SpriteSource _gDryfieldNightWaterTowerView2SpriteSources[58] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -80, 8, 1261, { .fields = { 64, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -72, 8, 1416, { .fields = { 40, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 120 } }, -160, -120, 613, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 120 } }, -160, 0, 643, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 120 } }, -144, -120, 685, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 120 } }, -144, 0, 714, { .fields = { 96, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 104 } }, -128, -104, 790, { .fields = { 80, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 96 } }, -128, 0, 797, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, -112, -88, 955, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, -112, 0, 918, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -96, -80, 1048, { .fields = { 56, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -88, -72, 1148, { .fields = { 32, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -80, -64, 1101, { .fields = { 48, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -72, -56, 1358, { .fields = { 8, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -96, 0, 1250, { .fields = { 0, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -96, 40, 1250, { .fields = { 24, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -88, 56, 1115, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -88, 8, 1135, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -80, 32, 1321, { .fields = { 16, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -72, 40, 1398, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -112, -120, 1300, { .fields = { 48, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -104, -120, 1250, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -104, -64, 1250, { .fields = { 48, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -96, 0, 1250, { .fields = { 8, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -112, 32, 1250, { .fields = { 24, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -104, 24, 1250, { .fields = { 56, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -96, 24, 1250, { .fields = { 24, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -88, -40, 1250, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -88, 24, 1250, { .fields = { 16, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -80, -120, 1463, { .fields = { 16, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -80, -80, 1461, { .fields = { 40, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -80, -40, 1340, { .fields = { 32, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -80, 8, 1340, { .fields = { 32, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -128, -32, 1250, { .fields = { 16, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -64, -16, 1250, { .fields = { 8, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -136, 8, 1250, { .fields = { 32, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -88, 8, 1143, { .fields = { 64, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -64, 24, 1250, { .fields = { 72, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 112 } }, -160, -56, 1250, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -152, -48, 1250, { .fields = { 64, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -152, 16, 638, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -152, 32, 665, { .fields = { 0, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -136, 32, 1250, { .fields = { 0, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -120, 32, 1250, { .fields = { 120, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -104, 32, 1250, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -88, 24, 1250, { .fields = { 0, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -136, -48, 1250, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -136, -24, 1250, { .fields = { 0, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -128, -8, 1250, { .fields = { 0, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -120, -56, 1250, { .fields = { 32, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -120, 8, 1250, { .fields = { 120, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -104, -48, 1250, { .fields = { 8, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -104, -16, 1250, { .fields = { 16, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -88, -56, 1250, { .fields = { 88, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -80, -56, 1250, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -80, -16, 1250, { .fields = { 8, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -72, 16, 1250, { .fields = { 80, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -112, 16, 1250, { .fields = { 120, 184 } }, 128, 128, 128, 0 },
};

/// View 2's drawing ranges into `_gDryfieldNightWaterTowerView2SpriteSources`.
///
/// The zero-count first record keeps decoded background strips; the final
/// record ends the list with `SPRITE_BATCH_END`. Visibility is mutable while loaded.
static SpriteBatch _gDryfieldNightWaterTowerView2SpriteBatches[6] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 20, 0, 0, { 3, 0 } },
    { 20, 9, 0, 0, { 0, 0 } },
    { 29, 4, 0, 0, { 2, 0 } },
    { 33, 25, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 3's 48 texture rectangles and sorting depths for scene sprites.
///
/// Positions and sizes are pixels; depths are unscaled OT sorting values.
/// Sources have neutral RGB modulation and opaque code flags, with no terminator.
/// The view's batches borrow element ranges while this overlay is loaded.
static SpriteSource _gDryfieldNightWaterTowerView3SpriteSources[48] = {
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 88, -120, 1250, { .fields = { 8, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 80, -112, 1250, { .fields = { 0, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, 80, -72, 1250, { .fields = { 64, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 72, -40, 1250, { .fields = { 16, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 64, 24, 1250, { .fields = { 0, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 72, 0, 1250, { .fields = { 48, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 88, 24, 1250, { .fields = { 120, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 80, -48, 1625, { .fields = { 8, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 80, 0, 1625, { .fields = { 8, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 96, -56, 1625, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 96, -8, 1625, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 112, -56, 1625, { .fields = { 24, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 112, -8, 1625, { .fields = { 40, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 128, -56, 1625, { .fields = { 24, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 128, -8, 1625, { .fields = { 24, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 144, -56, 1625, { .fields = { 24, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 144, -8, 1625, { .fields = { 64, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 64, -32, 1625, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 56, 8, 1625, { .fields = { 16, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, 64, 16, 1625, { .fields = { 120, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 56, 48, 1625, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 64, -48, 1625, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 56, -40, 1625, { .fields = { 96, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 72, 56, 1239, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 144, -120, 550, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 144, -40, 550, { .fields = { 80, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 144, 40, 550, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 128, -120, 625, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 128, -40, 625, { .fields = { 80, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 128, 40, 625, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 112, -120, 687, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 112, -40, 715, { .fields = { 96, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, 112, 40, 715, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 96, -104, 824, { .fields = { 64, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 96, -40, 800, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 96, 40, 833, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 88, -88, 939, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 88, -16, 931, { .fields = { 48, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 80, 56, 1054, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 80, 0, 1051, { .fields = { 48, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 88, 48, 930, { .fields = { 0, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 72, 8, 1174, { .fields = { 0, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 80, -80, 1065, { .fields = { 8, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 72, -64, 1191, { .fields = { 40, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 64, -64, 1375, { .fields = { 56, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 64, 0, 1329, { .fields = { 56, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 56, 0, 1463, { .fields = { 40, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 56, -56, 1458, { .fields = { 40, 168 } }, 128, 128, 128, 0 },
};

/// View 3's drawing ranges into `_gDryfieldNightWaterTowerView3SpriteSources`.
///
/// The zero-count first record keeps decoded background strips; the final
/// record ends the list with `SPRITE_BATCH_END`. Visibility is mutable while loaded.
static SpriteBatch _gDryfieldNightWaterTowerView3SpriteBatches[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 7, 0, 0, { 1, 0 } },
    { 7, 16, 0, 0, { 2, 0 } },
    { 23, 25, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 4's 88 texture rectangles and sorting depths for scene sprites.
///
/// Positions and sizes are pixels; depths are unscaled OT sorting values.
/// Sources have neutral RGB modulation and opaque code flags, with no terminator.
/// The view's batches borrow element ranges while this overlay is loaded.
static SpriteSource _gDryfieldNightWaterTowerView4SpriteSources[88] = {
    { 143, 0x3FC0, { .fields = { 8, 104 } }, -88, -120, 1149, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, -80, -120, 4608, { .fields = { 120, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, -72, -120, 1254, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 96 } }, -88, -16, 1061, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 96 } }, -80, -16, 1084, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 88 } }, -72, -16, 1155, { .fields = { 32, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -8, 0, 1850, { .fields = { 120, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -48, 8, 1362, { .fields = { 56, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 96 } }, -160, -72, 750, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 96 } }, -160, 24, 750, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, -144, -64, 812, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 96 } }, -144, 24, 812, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, -128, 24, 875, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, -128, -64, 875, { .fields = { 40, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, -112, -64, 950, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -112, 24, 950, { .fields = { 40, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 64 } }, -96, 24, 1000, { .fields = { 104, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -96, -56, 1000, { .fields = { 24, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 64 } }, -80, -56, 1125, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 64 } }, -80, 16, 1125, { .fields = { 104, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -64, -56, 1200, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -64, 16, 1250, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -48, 16, 1300, { .fields = { 112, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -48, -48, 1325, { .fields = { 120, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -40, -48, 1400, { .fields = { 96, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, -40, -8, 1400, { .fields = { 96, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -40, 32, 1400, { .fields = { 88, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -32, 0, 1500, { .fields = { 72, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -32, -48, 1437, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -24, -48, 1550, { .fields = { 80, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -16, -48, 1700, { .fields = { 80, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -8, -40, 1850, { .fields = { 72, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -24, 8, 1600, { .fields = { 8, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, -16, 0, 1675, { .fields = { 96, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -8, 24, 1850, { .fields = { 8, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -32, 32, 1800, { .fields = { 64, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -24, -16, 1750, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -16, 0, 1750, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -40, 32, 1750, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 16 } }, -160, -40, 1750, { .fields = { 32, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 16 } }, -112, -40, 1750, { .fields = { 32, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 16 } }, -64, -40, 1750, { .fields = { 16, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -152, -8, 1750, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -136, 24, 1750, { .fields = { 32, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -88, 24, 1750, { .fields = { 32, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -136, 0, 1750, { .fields = { 16, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -128, -8, 1750, { .fields = { 64, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -112, -24, 1750, { .fields = { 80, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -96, -24, 1750, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -80, -24, 1750, { .fields = { 72, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -64, -16, 1750, { .fields = { 72, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -48, 0, 1750, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -32, 0, 1750, { .fields = { 88, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -8, -24, 1750, { .fields = { 72, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -16, 24, 1750, { .fields = { 72, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -40, -120, 1750, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -32, -96, 1750, { .fields = { 56, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -24, -32, 1750, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -32, 16, 1750, { .fields = { 88, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -16, 16, 1750, { .fields = { 80, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -160, -120, 2250, { .fields = { 8, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -160, -40, 2250, { .fields = { 8, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, -144, 0, 2250, { .fields = { 88, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 104, 104, 782, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 112, 104, 542, { .fields = { 64, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 120, 104, 519, { .fields = { 72, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 128, 104, 484, { .fields = { 24, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 136, 104, 427, { .fields = { 64, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 112, 112, 553, { .fields = { 64, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 120, 112, 506, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 128, 112, 486, { .fields = { 72, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 136, 112, 448, { .fields = { 72, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 144, 112, 404, { .fields = { 64, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 152, 112, 387, { .fields = { 72, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, 80, 64, 1150, { .fields = { 104, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, 88, 48, 1125, { .fields = { 96, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 56 } }, 96, 40, 1075, { .fields = { 120, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, 104, 24, 1075, { .fields = { 24, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 96 } }, 112, 8, 1000, { .fields = { 72, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 136, -88, 2320, { .fields = { 80, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 96 } }, 120, 8, 500, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 96 } }, 128, 8, 500, { .fields = { 56, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 104 } }, 144, 8, 450, { .fields = { 104, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 120, -16, 1312, { .fields = { 88, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 128, -40, 1175, { .fields = { 104, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 136, -48, 1125, { .fields = { 0, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 64 } }, 144, -120, 450, { .fields = { 120, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 64 } }, 144, -56, 450, { .fields = { 120, 136 } }, 128, 128, 128, 0 },
};

/// View 4's drawing ranges into `_gDryfieldNightWaterTowerView4SpriteSources`.
///
/// The zero-count first record keeps decoded background strips; the final
/// record ends the list with `SPRITE_BATCH_END`. Visibility is mutable while loaded.
static SpriteBatch _gDryfieldNightWaterTowerView4SpriteBatches[9] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 6, 0, 0, { 3, 0 } },
    { 6, 30, 0, 0, { 4, 0 } },
    { 36, 19, 0, 0, { 1, 0 } },
    { 55, 5, 0, 0, { 5, 0 } },
    { 60, 3, 0, 0, { 0, 0 } },
    { 63, 25, 0, 0, { 6, 0 } },
    { 88, 0, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 5's 56 texture rectangles and sorting depths for scene sprites.
///
/// Positions and sizes are pixels; depths are unscaled OT sorting values.
/// Sources have neutral RGB modulation and opaque code flags, with no terminator.
/// The view's batches borrow element ranges while this overlay is loaded.
static SpriteSource _gDryfieldNightWaterTowerView5SpriteSources[56] = {
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 136, -120, 875, { .fields = { 0, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 144, -112, 875, { .fields = { 80, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 152, -120, 875, { .fields = { 0, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 136, -80, 875, { .fields = { 16, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, 144, -72, 875, { .fields = { 120, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 152, -72, 875, { .fields = { 56, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 136, -32, 875, { .fields = { 56, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 144, -32, 875, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 152, -32, 875, { .fields = { 120, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 144, 40, 875, { .fields = { 80, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 152, 40, 875, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 48, -24, 1625, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 48, 24, 1625, { .fields = { 24, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 64, -24, 1625, { .fields = { 24, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 80, -24, 1625, { .fields = { 24, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 96, -24, 1625, { .fields = { 8, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 112, -24, 1625, { .fields = { 0, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 128, -24, 1625, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 144, -24, 1625, { .fields = { 8, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 64, 24, 1625, { .fields = { 8, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 80, 24, 1625, { .fields = { 40, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 96, 24, 1625, { .fields = { 40, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 112, 24, 1625, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 128, 24, 1625, { .fields = { 112, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 144, 24, 1625, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 72, -120, 1375, { .fields = { 24, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 64, -72, 1375, { .fields = { 88, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 80, -72, 1375, { .fields = { 16, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, 56, 8, 1375, { .fields = { 64, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 48, 40, 1375, { .fields = { 0, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 72, 8, 1375, { .fields = { 24, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 72, 40, 1375, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 64, 0, 1375, { .fields = { 104, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 56, 24, 1450, { .fields = { 96, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 64, 24, 1375, { .fields = { 88, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 40, 48, 1625, { .fields = { 64, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 40, -24, 1625, { .fields = { 0, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 40, 16, 1625, { .fields = { 8, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 48, -32, 1550, { .fields = { 104, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 48, 16, 1550, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, 56, -40, 1450, { .fields = { 120, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 56, 48, 1450, { .fields = { 48, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 64, -40, 1375, { .fields = { 40, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, 64, 40, 1375, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, 64, 64, 1375, { .fields = { 120, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 72, -40, 1300, { .fields = { 40, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 72, 24, 1300, { .fields = { 64, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 72, 72, 1300, { .fields = { 32, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, 88, -48, 1125, { .fields = { 56, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, 88, 24, 1125, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 104, -56, 1000, { .fields = { 88, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, 104, 24, 1000, { .fields = { 72, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, 120, -64, 900, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 120, 24, 900, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 104 } }, 136, -80, 825, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 96 } }, 136, 24, 825, { .fields = { 104, 104 } }, 128, 128, 128, 0 },
};

/// View 5's drawing ranges into `_gDryfieldNightWaterTowerView5SpriteSources`.
///
/// The zero-count first record keeps decoded background strips; the final
/// record ends the list with `SPRITE_BATCH_END`. Visibility is mutable while loaded.
static SpriteBatch _gDryfieldNightWaterTowerView5SpriteBatches[6] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 11, 0, 0, { 3, 0 } },
    { 11, 14, 0, 0, { 0, 0 } },
    { 25, 7, 0, 0, { 2, 0 } },
    { 32, 24, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 6's 84 texture rectangles and sorting depths for scene sprites.
///
/// Positions and sizes are pixels; depths are unscaled OT sorting values.
/// Sources have neutral RGB modulation and opaque code flags, with no terminator.
/// The view's batches borrow element ranges while this overlay is loaded.
static SpriteSource _gDryfieldNightWaterTowerView6SpriteSources[84] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 0, -120, 1125, { .fields = { 88, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -80, 88, 1125, { .fields = { 32, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -72, 72, 1125, { .fields = { 32, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -64, 48, 1125, { .fields = { 40, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -56, 24, 1125, { .fields = { 32, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -48, 0, 1125, { .fields = { 32, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -40, -24, 1125, { .fields = { 32, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -32, -48, 1125, { .fields = { 48, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -24, -72, 1125, { .fields = { 48, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -16, -96, 1125, { .fields = { 32, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -8, -120, 1125, { .fields = { 32, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -160, 24, 375, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -144, 40, 450, { .fields = { 88, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -128, 56, 500, { .fields = { 88, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -112, 72, 575, { .fields = { 56, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -96, 88, 700, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -80, 104, 825, { .fields = { 0, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -96, -112, 2000, { .fields = { 48, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -128, 80, 491, { .fields = { 40, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -88, 80, 1000, { .fields = { 72, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -160, -64, 562, { .fields = { 112, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, -144, -48, 621, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -160, 48, 400, { .fields = { 0, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -144, 48, 455, { .fields = { 0, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -160, 72, 825, { .fields = { 16, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -144, 72, 475, { .fields = { 0, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -136, -120, 900, { .fields = { 0, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -128, -104, 912, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -120, -80, 912, { .fields = { 16, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -112, -48, 900, { .fields = { 16, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -112, -120, 900, { .fields = { 0, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -104, -104, 900, { .fields = { 40, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -88, -96, 2000, { .fields = { 16, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -80, -64, 2000, { .fields = { 16, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -128, -24, 675, { .fields = { 56, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -128, 24, 812, { .fields = { 72, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -112, -24, 900, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -112, 24, 817, { .fields = { 88, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -96, -32, 1000, { .fields = { 72, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -96, 24, 840, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -80, 8, 1000, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -80, 24, 925, { .fields = { 112, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -72, 56, 1000, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -64, 72, 1125, { .fields = { 64, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -56, 80, 1000, { .fields = { 56, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -48, 88, 900, { .fields = { 40, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -96, -64, 900, { .fields = { 72, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -88, -64, 2000, { .fields = { 80, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -48, 112, 1500, { .fields = { 72, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -80, -104, 1500, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -72, -80, 1500, { .fields = { 104, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -64, -48, 1500, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -56, -8, 1500, { .fields = { 104, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -48, 24, 1500, { .fields = { 120, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -40, 64, 1500, { .fields = { 80, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -32, 104, 1500, { .fields = { 0, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -160, -96, 2000, { .fields = { 48, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -160, -48, 2000, { .fields = { 8, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -152, -104, 2000, { .fields = { 56, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -152, -40, 2000, { .fields = { 16, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -136, -104, 2000, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -136, -80, 2000, { .fields = { 16, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -136, -48, 2000, { .fields = { 0, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -120, -48, 2000, { .fields = { 120, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -120, -72, 2000, { .fields = { 0, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -120, -104, 2000, { .fields = { 16, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -104, -112, 2000, { .fields = { 16, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -104, -72, 2000, { .fields = { 0, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -104, -48, 2000, { .fields = { 120, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -88, -56, 2000, { .fields = { 0, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -88, -72, 2000, { .fields = { 120, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -80, -88, 2000, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -88, -112, 2000, { .fields = { 0, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -72, -80, 2000, { .fields = { 40, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -56, -8, 1400, { .fields = { 88, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -48, 0, 1400, { .fields = { 8, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -40, -8, 1425, { .fields = { 96, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -32, -16, 1500, { .fields = { 40, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -24, -24, 1625, { .fields = { 48, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -56, 16, 1250, { .fields = { 56, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -48, 16, 1425, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -40, 16, 1500, { .fields = { 72, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -32, 16, 1450, { .fields = { 40, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -24, 32, 1750, { .fields = { 8, 176 } }, 128, 128, 128, 0 },
};

/// View 6's drawing ranges into `_gDryfieldNightWaterTowerView6SpriteSources`.
///
/// The zero-count first record keeps decoded background strips; the final
/// record ends the list with `SPRITE_BATCH_END`. Visibility is mutable while loaded.
static SpriteBatch _gDryfieldNightWaterTowerView6SpriteBatches[9] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 11, 0, 0, { 3, 0 } },
    { 11, 6, 0, 0, { 4, 0 } },
    { 17, 31, 0, 0, { 1, 0 } },
    { 48, 8, 0, 0, { 5, 0 } },
    { 56, 18, 0, 0, { 0, 0 } },
    { 74, 0, 0, 0, { 6, 0 } },
    { 74, 10, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 7's 69 texture rectangles and sorting depths for scene sprites.
///
/// Positions and sizes are pixels; depths are unscaled OT sorting values.
/// Sources have neutral RGB modulation and opaque code flags, with no terminator.
/// The view's batches borrow element ranges while this overlay is loaded.
static SpriteSource _gDryfieldNightWaterTowerView7SpriteSources[69] = {
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -32, -56, 1750, { .fields = { 120, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -24, -56, 1875, { .fields = { 88, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -32, -40, 1750, { .fields = { 0, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 96 } }, -40, -72, 1875, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -32, -24, 1750, { .fields = { 56, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -24, 0, 1875, { .fields = { 32, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -24, -32, 1875, { .fields = { 56, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -16, -64, 2000, { .fields = { 48, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -144, -112, 950, { .fields = { 8, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -144, -80, 812, { .fields = { 0, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -144, -40, 812, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 96 } }, -136, -112, 812, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 96 } }, -136, -16, 850, { .fields = { 72, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, -120, -104, 850, { .fields = { 40, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 96 } }, -120, -16, 862, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -104, -96, 1000, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -104, -16, 1000, { .fields = { 24, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, -56, 8, 1350, { .fields = { 104, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -72, -16, 1200, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -64, -16, 1300, { .fields = { 120, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -64, -80, 1300, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -72, -88, 1200, { .fields = { 104, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -88, 8, 1075, { .fields = { 64, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, -80, 8, 1150, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, -80, -32, 1150, { .fields = { 120, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -88, -16, 1075, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -80, -88, 1125, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 72 } }, -88, -96, 1050, { .fields = { 120, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -24, -24, 2250, { .fields = { 104, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -16, -24, 2250, { .fields = { 112, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 96 } }, -32, -72, 2250, { .fields = { 120, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -24, 0, 2250, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -24, -64, 2250, { .fields = { 96, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -16, -64, 2250, { .fields = { 96, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -16, -8, 2250, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -128, 0, 831, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 96 } }, -160, -104, 832, { .fields = { 88, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -160, -8, 878, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -144, -8, 864, { .fields = { 8, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 96 } }, -144, -104, 817, { .fields = { 56, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -128, 56, 873, { .fields = { 24, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 72 } }, -128, -120, 850, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -120, -48, 850, { .fields = { 72, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 80 } }, -120, 0, 850, { .fields = { 16, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 96 } }, -160, -120, 825, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 96 } }, -160, -24, 875, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 96 } }, -144, -120, 812, { .fields = { 104, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 104 } }, -144, -24, 862, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 72 } }, -128, 0, 850, { .fields = { 104, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -112, 40, 875, { .fields = { 96, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -112, 0, 2250, { .fields = { 88, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -160, -48, 2250, { .fields = { 88, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -160, -24, 2250, { .fields = { 88, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -160, -8, 2250, { .fields = { 88, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -144, -56, 2250, { .fields = { 8, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -144, -24, 2250, { .fields = { 88, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -128, -56, 2250, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -128, -32, 2250, { .fields = { 88, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -128, 0, 2250, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -112, -56, 2250, { .fields = { 104, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 72 } }, -96, -64, 2250, { .fields = { 120, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 72 } }, -80, -64, 2250, { .fields = { 104, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -64, -64, 2250, { .fields = { 0, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -48, -56, 2250, { .fields = { 88, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -48, -24, 2250, { .fields = { 88, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -48, 0, 2250, { .fields = { 88, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -32, -64, 2250, { .fields = { 88, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -32, -24, 2250, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -16, -64, 2250, { .fields = { 40, 184 } }, 128, 128, 128, 0 },
};

/// View 7's drawing ranges into `_gDryfieldNightWaterTowerView7SpriteSources`.
///
/// The zero-count first record keeps decoded background strips; the final
/// record ends the list with `SPRITE_BATCH_END`. Visibility is mutable while loaded.
static SpriteBatch _gDryfieldNightWaterTowerView7SpriteBatches[9] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 8, 0, 0, { 3, 0 } },
    { 8, 20, 0, 0, { 4, 0 } },
    { 28, 7, 0, 0, { 1, 0 } },
    { 35, 6, 0, 0, { 5, 0 } },
    { 41, 3, 0, 0, { 0, 0 } },
    { 44, 6, 0, 0, { 6, 0 } },
    { 50, 19, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 8's 99 texture rectangles and sorting depths for scene sprites.
///
/// Positions and sizes are pixels; depths are unscaled OT sorting values.
/// Sources have neutral RGB modulation and opaque code flags, with no terminator.
/// The view's batches borrow element ranges while this overlay is loaded.
static SpriteSource _gDryfieldNightWaterTowerView8SpriteSources[99] = {
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -144, -56, 1250, { .fields = { 0, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -128, -32, 1500, { .fields = { 24, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -112, -16, 1625, { .fields = { 8, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -96, 8, 1625, { .fields = { 80, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -152, -40, 1375, { .fields = { 48, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, 64, 947, { .fields = { 80, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, 64, 874, { .fields = { 72, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 144, 72, 972, { .fields = { 112, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, 72, 1129, { .fields = { 96, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, 80, 1384, { .fields = { 64, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 152, 80, 1359, { .fields = { 112, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -48, 112, 900, { .fields = { 24, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, -16, 104, 900, { .fields = { 8, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 16, 96, 900, { .fields = { 48, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, 40, 88, 900, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, 72, 80, 875, { .fields = { 48, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 32 } }, 104, 64, 875, { .fields = { 40, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -160, -40, 1000, { .fields = { 16, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, -144, -120, 1175, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -152, 8, 1175, { .fields = { 72, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -152, 64, 1175, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -160, 64, 1000, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -160, 8, 1000, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -152, -40, 1175, { .fields = { 24, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -152, -80, 1175, { .fields = { 96, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -136, -120, 1250, { .fields = { 80, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -136, -80, 1250, { .fields = { 32, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -120, -96, 1750, { .fields = { 24, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -136, -48, 1500, { .fields = { 40, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -136, 8, 1500, { .fields = { 32, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -136, 64, 1250, { .fields = { 48, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -120, 64, 1500, { .fields = { 56, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -120, 8, 1625, { .fields = { 56, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -120, -80, 1750, { .fields = { 32, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -104, -64, 1875, { .fields = { 0, 240 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 24 } }, -120, -48, 1828, { .fields = { 40, 72 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 32 } }, -120, -24, 1692, { .fields = { 40, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 56, -80, 1997, { .fields = { 80, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 64, -88, 2064, { .fields = { 96, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, 64, -120, 1909, { .fields = { 96, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 80, -120, 1807, { .fields = { 96, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 80, -80, 1733, { .fields = { 0, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 96, -120, 1513, { .fields = { 120, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 96, -72, 2030, { .fields = { 64, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 112, -104, 2024, { .fields = { 88, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -144, -120, 2000, { .fields = { 8, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -128, -120, 2000, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -112, -120, 2000, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -96, -120, 2000, { .fields = { 96, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -80, -120, 2000, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -64, -120, 2000, { .fields = { 112, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -48, -120, 2000, { .fields = { 80, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -32, -120, 2000, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -16, -120, 2000, { .fields = { 64, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 0, -120, 2000, { .fields = { 64, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 16, -120, 2000, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 32, -120, 2000, { .fields = { 16, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, 48, -104, 2000, { .fields = { 72, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 64, -120, 2000, { .fields = { 16, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -48, -120, 250, { .fields = { 104, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -160, -80, 250, { .fields = { 80, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -144, -88, 250, { .fields = { 40, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -128, -104, 250, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -112, -120, 250, { .fields = { 112, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -96, -120, 250, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -80, -120, 250, { .fields = { 64, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -64, -120, 250, { .fields = { 56, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, -160, 88, 675, { .fields = { 40, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, -128, 80, 675, { .fields = { 24, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, -88, 72, 675, { .fields = { 32, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, -56, 64, 675, { .fields = { 16, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, -16, 56, 675, { .fields = { 8, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, 24, 48, 675, { .fields = { 16, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, 56, 40, 675, { .fields = { 32, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, 96, 32, 675, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 136, 24, 675, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 128, 0x3FC0, { .fields = { 8, 8 } }, 40, -104, 1250, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 16 } }, 24, -16, 1625, { .fields = { 0, 104 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 16 } }, 32, -32, 1500, { .fields = { 8, 88 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 16 } }, 32, -48, 1375, { .fields = { 8, 72 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 16 } }, 32, -64, 1250, { .fields = { 8, 56 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 16 } }, 40, -80, 1250, { .fields = { 16, 40 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 16 } }, 40, -96, 1250, { .fields = { 16, 24 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 24 } }, 48, -120, 1250, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, 64, -120, 1875, { .fields = { 64, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, 64, -88, 2000, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 80, -120, 1750, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 80, -80, 1875, { .fields = { 0, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 112, -8, 1625, { .fields = { 80, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 128, -24, 1500, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 128, 24, 1500, { .fields = { 88, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 144, -48, 1375, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 144, 8, 1375, { .fields = { 48, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 144, 48, 1375, { .fields = { 80, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 64 } }, 96, -120, 1625, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -136, -120, 1597, { .fields = { 72, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -120, -120, 226, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -120, -80, 1814, { .fields = { 112, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, -104, -80, 1868, { .fields = { 104, 144 } }, 128, 128, 128, 0 },
};

/// View 8's drawing ranges into `_gDryfieldNightWaterTowerView8SpriteSources`.
///
/// The zero-count first record keeps decoded background strips; the final
/// record ends the list with `SPRITE_BATCH_END`. Visibility is mutable while loaded.
static SpriteBatch _gDryfieldNightWaterTowerView8SpriteBatches[11] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 5, 0, 0, { 0, 0 } },
    { 5, 12, 0, 0, { 5, 0 } },
    { 17, 20, 0, 0, { 4, 0 } },
    { 37, 8, 0, 0, { 6, 0 } },
    { 45, 14, 0, 0, { 1, 0 } },
    { 59, 17, 0, 0, { 7, 0 } },
    { 76, 8, 0, 0, { 3, 0 } },
    { 84, 11, 0, 0, { 8, 0 } },
    { 95, 4, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 9's empty sprite list, retaining decoded background-strip presentation.
///
/// The zero-count first record is followed by `SPRITE_BATCH_END`; the source
/// slot retains this list's address but supplies no `SpriteSource` elements.
static SpriteBatch _gDryfieldNightWaterTowerView9SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 10's 20 texture rectangles and sorting depths for scene sprites.
///
/// Positions and sizes are pixels; depths are unscaled OT sorting values.
/// Sources have neutral RGB modulation and opaque code flags, with no terminator.
/// The view's batches borrow element ranges while this overlay is loaded.
static SpriteSource _gDryfieldNightWaterTowerView10SpriteSources[20] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 0, 112, 1000, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -160, 40, 575, { .fields = { 112, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -144, 64, 612, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -128, 64, 650, { .fields = { 96, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -112, 64, 725, { .fields = { 112, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -96, 72, 625, { .fields = { 96, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -80, 80, 650, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -64, 88, 700, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -48, 96, 750, { .fields = { 96, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -32, 96, 1000, { .fields = { 112, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -16, 104, 1050, { .fields = { 80, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -160, -16, 462, { .fields = { 80, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -144, -24, 462, { .fields = { 80, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -128, -32, 462, { .fields = { 80, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -112, -40, 462, { .fields = { 80, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -96, -48, 462, { .fields = { 80, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -80, -80, 462, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -64, -80, 462, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -48, -64, 462, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -32, -48, 462, { .fields = { 88, 176 } }, 128, 128, 128, 0 },
};

/// View 10's drawing ranges into `_gDryfieldNightWaterTowerView10SpriteSources`.
///
/// The zero-count first record keeps decoded background strips; the final
/// record ends the list with `SPRITE_BATCH_END`. Visibility is mutable while loaded.
static SpriteBatch _gDryfieldNightWaterTowerView10SpriteBatches[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 11, 0, 0, { 1, 0 } },
    { 11, 9, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView gDryfieldNightWaterTowerSpriteViews[DRYFIELD_NIGHT_WATER_TOWER_LOGICAL_VIEW_COUNT] = {
    { .sources = { .empty = _gDryfieldNightWaterTowerView1SpriteBatches }, .batches = _gDryfieldNightWaterTowerView1SpriteBatches, .drawAreas = NULL },
    { .sources = { .elements = _gDryfieldNightWaterTowerView2SpriteSources }, .batches = _gDryfieldNightWaterTowerView2SpriteBatches, .drawAreas = NULL },
    { .sources = { .elements = _gDryfieldNightWaterTowerView3SpriteSources }, .batches = _gDryfieldNightWaterTowerView3SpriteBatches, .drawAreas = NULL },
    { .sources = { .elements = _gDryfieldNightWaterTowerView4SpriteSources }, .batches = _gDryfieldNightWaterTowerView4SpriteBatches, .drawAreas = NULL },
    { .sources = { .elements = _gDryfieldNightWaterTowerView5SpriteSources }, .batches = _gDryfieldNightWaterTowerView5SpriteBatches, .drawAreas = NULL },
    { .sources = { .elements = _gDryfieldNightWaterTowerView6SpriteSources }, .batches = _gDryfieldNightWaterTowerView6SpriteBatches, .drawAreas = NULL },
    { .sources = { .elements = _gDryfieldNightWaterTowerView7SpriteSources }, .batches = _gDryfieldNightWaterTowerView7SpriteBatches, .drawAreas = NULL },
    { .sources = { .elements = _gDryfieldNightWaterTowerView8SpriteSources }, .batches = _gDryfieldNightWaterTowerView8SpriteBatches, .drawAreas = NULL },
    { .sources = { .empty = _gDryfieldNightWaterTowerView9SpriteBatches }, .batches = _gDryfieldNightWaterTowerView9SpriteBatches, .drawAreas = NULL },
    { .sources = { .elements = _gDryfieldNightWaterTowerView10SpriteSources }, .batches = _gDryfieldNightWaterTowerView10SpriteBatches, .drawAreas = NULL },
};

/// Four all-view directional lights for the night Water Tower's model shading.
///
/// Local translations supply direction vectors; RGB intensities have 12 fractional
/// bits. Binding parents the transforms to the view; queries overwrite attenuation.
static WorldCoordLight _gDryfieldNightWaterTowerDirectionalLights[4] = {
    { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 1000, -1000, 0 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 750, 750, 750 }, { 0, 0 } },
    { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -1000, -1000, 0 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 750, 750, 750 }, { 0, 0 } },
    { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, -1000, -1000 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 750, 750, 750 }, { 0, 0 } },
    { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, -1000, 1000 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 750, 750, 750 }, { 0, 0 } },
};

/// Five all-view point lights with room-space positions and distance falloff.
///
/// Positions and inner/outer radii use whole game units; RGB has 12 fractional
/// bits. View parents, composition caches and attenuation are mutable while loaded.
static WorldCoordPointLight _gDryfieldNightWaterTowerPointLights[5] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -3840, -3761, 4020 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 6148, 6148, 6148 }, { 0, 0 } }, 4463, 4464 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -5000, -2541, -3142 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 5124, 5124, 5124 }, { 0, 0 } }, 2382, 3321 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 4200, -3100, -3479 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 6148, 6148, 6148 }, { 0, 0 } }, 3405, 3602 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 4200, -2599, 1900 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 6148, 6148, 6148 }, { 0, 0 } }, 2541, 3061 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -2339, -2000, -6000 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3076, 3076, 3076 }, { 0, 0 } }, 2000, 3000 },
};

/// Model-shading light collection for room 1, with no cone lights.
///
/// Borrowed with its mutable light arrays while the night Water Tower is loaded.
static WorldCoordRoomLights _gDryfieldNightWaterTowerRoomLights = {
    .directionalLightCount = ARRAY_SIZE(_gDryfieldNightWaterTowerDirectionalLights),
    .directionalLights     = _gDryfieldNightWaterTowerDirectionalLights,
    .pointLightCount       = ARRAY_SIZE(_gDryfieldNightWaterTowerPointLights),
    .pointLights           = _gDryfieldNightWaterTowerPointLights,
    .coneLightCount        = 0,
    .coneLights            = NULL,
};

/// Directed boundaries between logical views 2/3, 3/4, 4/5, 5/6, 2/7, 7/8 and 7/10.
///
/// parameter0 is the source view and parameter1 the destination. Setup binds
/// the view coordinate and links/enables all 14 records, including the LAST
/// record. Links and hit latches are mutable; unlink before unloading.
static WorldCollisionTrigger _gDryfieldNightWaterTowerViewBoundaryTriggers[] = {
    {
        .origin       = { 447, -1888, -4529, 0 },
        .vertices     = { { -602, -2912, -2368, 0 }, { 595, -2912, 2364, 0 }, { -602, 2912, -2368, 0 }, { 595, 2912, 2364, 0 } },
        .normal       = { 3978, 0, -1007, 0 },
        .facingNormal = { 0, 0, 4096, 0 },
        .radius       = 3797,
        .control      = 0,
        .parameter0   = 2,
        .parameter1   = 3,
        .flags        = WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY,
    },
    {
        .origin       = { 607, -1984, -4417, 0 },
        .vertices     = { { 594, -3008, 2362, 0 }, { -603, -3008, -2369, 0 }, { 594, 3008, 2362, 0 }, { -603, 3008, -2369, 0 } },
        .normal       = { -3981, 0, 1006, 0 },
        .facingNormal = { 0, 0, 4096, 0 },
        .radius       = 3865,
        .control      = 0,
        .parameter0   = 3,
        .parameter1   = 2,
        .flags        = WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY,
    },
    {
        .origin       = { -4449, -1712, -4673, 0 },
        .vertices     = { { 1730, -2736, 1722, 0 }, { -1730, -2736, -1722, 0 }, { 1730, 2736, 1722, 0 }, { -1730, 2736, -1722, 0 } },
        .normal       = { -2892, 0, 2904, 0 },
        .facingNormal = { 0, 0, 4096, 0 },
        .radius       = 3665,
        .control      = 0,
        .parameter0   = 4,
        .parameter1   = 3,
        .flags        = WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY,
    },
    {
        .origin       = { -4640, -1632, -4544, 0 },
        .vertices     = { { -1730, -2656, -1722, 0 }, { 1730, -2656, 1722, 0 }, { -1730, 2656, -1722, 0 }, { 1730, 2656, 1722, 0 } },
        .normal       = { 2897, 0, -2912, 0 },
        .facingNormal = { 0, 0, 4096, 0 },
        .radius       = 3602,
        .control      = 0,
        .parameter0   = 3,
        .parameter1   = 4,
        .flags        = WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY,
    },
    {
        .origin       = { -4865, -1680, 575, 0 },
        .vertices     = { { -2441, -2704, 6, 0 }, { 2441, -2704, -6, 0 }, { -2441, 2704, 6, 0 }, { 2441, 2704, -6, 0 } },
        .normal       = { -11, 0, -4102, 0 },
        .facingNormal = { 0, 0, 4096, 0 },
        .radius       = 3638,
        .control      = 0,
        .parameter0   = 4,
        .parameter1   = 5,
        .flags        = WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY,
    },
    {
        .origin       = { -4770, -1776, 446, 0 },
        .vertices     = { { 2441, -2800, -17, 0 }, { -2440, -2800, 17, 0 }, { 2441, 2800, -17, 0 }, { -2440, 2800, 17, 0 } },
        .normal       = { 28, 0, 4105, 0 },
        .facingNormal = { 0, 0, 4096, 0 },
        .radius       = 3709,
        .control      = 0,
        .parameter0   = 5,
        .parameter1   = 4,
        .flags        = WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY,
    },
    {
        .origin       = { -3138, -1696, 3709, 0 },
        .vertices     = { { 279, -2720, -1510, 0 }, { -278, -2720, 1511, 0 }, { 279, 2720, -1510, 0 }, { -278, 2720, 1511, 0 } },
        .normal       = { 4035, 0, 743, 0 },
        .facingNormal = { 0, 0, 4096, 0 },
        .radius       = 3114,
        .control      = 0,
        .parameter0   = 6,
        .parameter1   = 5,
        .flags        = WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY,
    },
    {
        .origin       = { -2816, -1792, 3518, 0 },
        .vertices     = { { -238, -2816, 1330, 0 }, { 238, -2816, -1330, 0 }, { -238, 2816, 1330, 0 }, { 238, 2816, -1330, 0 } },
        .normal       = { -4039, 0, -724, 0 },
        .facingNormal = { 0, 0, 4096, 0 },
        .radius       = 3114,
        .control      = 0,
        .parameter0   = 5,
        .parameter1   = 6,
        .flags        = WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY,
    },
    {
        .origin       = { 5343, -1824, -2755, 0 },
        .vertices     = { { 2712, -2848, 15, 0 }, { -2711, -2848, -14, 0 }, { 2712, 2848, 15, 0 }, { -2711, 2848, -14, 0 } },
        .normal       = { -23, 0, 4103, 0 },
        .facingNormal = { 0, 0, 4096, 0 },
        .radius       = 3924,
        .control      = 0,
        .parameter0   = 7,
        .parameter1   = 2,
        .flags        = WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY,
    },
    {
        .origin       = { 5280, -1904, -2561, 0 },
        .vertices     = { { -2757, -2928, -12, 0 }, { 2756, -2928, 11, 0 }, { -2757, 2928, -12, 0 }, { 2756, 2928, 11, 0 } },
        .normal       = { 16, 0, -4105, 0 },
        .facingNormal = { 0, 0, 4096, 0 },
        .radius       = 4015,
        .control      = 0,
        .parameter0   = 2,
        .parameter1   = 7,
        .flags        = WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY,
    },
    {
        .origin       = { 3023, -2016, -65, 0 },
        .vertices     = { { 4, -3040, 1207, 0 }, { -3, -3040, -1206, 0 }, { 4, 3040, 1207, 0 }, { -3, 3040, -1206, 0 } },
        .normal       = { -4104, 0, 11, 0 },
        .facingNormal = { 0, 0, 4096, 0 },
        .radius       = 3268,
        .control      = 0,
        .parameter0   = 8,
        .parameter1   = 7,
        .flags        = WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY,
    },
    {
        .origin       = { 2847, -2128, -33, 0 },
        .vertices     = { { -3, -3152, -1206, 0 }, { 4, -3152, 1207, 0 }, { -3, 3152, -1206, 0 }, { 4, 3152, 1207, 0 } },
        .normal       = { 4099, 0, -13, 0 },
        .facingNormal = { 0, 0, 4096, 0 },
        .radius       = 3367,
        .control      = 0,
        .parameter0   = 7,
        .parameter1   = 8,
        .flags        = WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY,
    },
    {
        .origin       = { 4431, -2048, 511, 0 },
        .vertices     = { { -1504, -2928, 554, 0 }, { 1490, -2928, -570, 0 }, { -1504, 2928, 554, 0 }, { 1490, 2928, -570, 0 } },
        .normal       = { -1447, 0, -3853, 0 },
        .facingNormal = { 0, 0, 4096, 0 },
        .radius       = 3328,
        .control      = 0,
        .parameter0   = 7,
        .parameter1   = 10,
        .flags        = WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY,
    },
    {
        .origin       = { 4382, -1888, 477, 0 },
        .vertices     = { { 1497, -2928, -562, 0 }, { -1497, -2928, 562, 0 }, { 1497, 2928, -562, 0 }, { -1497, 2928, 562, 0 } },
        .normal       = { 1445, 0, 3851, 0 },
        .facingNormal = { 0, 0, 4096, 0 },
        .radius       = 3328,
        .control      = 0,
        .parameter0   = 10,
        .parameter1   = 7,
        .flags        = WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST,
    },
};

/// Integer commands carried by the room's non-warp action triggers.
enum {
    DRYFIELD_NIGHT_WATER_TOWER_TRIGGER_MECHANISM_COMMAND = 7,
    DRYFIELD_NIGHT_WATER_TOWER_TRIGGER_CAP_COMMAND_12    = 12,
    DRYFIELD_NIGHT_WATER_TOWER_TRIGGER_ROOM_ACTION_1     = 1,
};

/// Night Water Tower interaction and departure quads.
///
/// Five quads use four warp endpoints; the water-tank endpoint has two quads.
/// Other records request mechanism command 7, ignored room action 1, or CAP
/// command 12 at two heights. Setup binds and enables all nine records,
/// including LAST; links and hit latches stay mutable until unlinked.
static WorldCollisionTrigger _gDryfieldNightWaterTowerActionTriggers[] = {
    {
        .origin       = { -2832, -48, -5760, 0 },
        .vertices     = { { -624, 0, -416, 0 }, { 624, 0, -416, 0 }, { -624, 0, 416, 0 }, { 624, 0, 416, 0 } },
        .normal       = { 0, 4095, 0, 0 },
        .facingNormal = { 0, 0, 4096, 0 },
        .radius       = 749,
        .control      = WORLD_COLLISION_TRIGGER_ACTION_WARP,
        .parameter0   = GAME_AREA_DRYFIELD_G_R_KITCHEN,
        .parameter1   = (DRYFIELD_NIGHT_WATER_TOWER_WARP_KITCHEN << DRYFIELD_NIGHT_WATER_TOWER_WARP_ENDPOINT_SHIFT) | 2,
        .flags        = WORLD_COLLISION_TRIGGER_FACING_QUAD,
    },
    {
        .origin       = { 5760, -48, 1696, 0 },
        .vertices     = { { 416, 0, -736, 0 }, { 416, 0, 736, 0 }, { -416, 0, -736, 0 }, { -416, 0, 736, 0 } },
        .normal       = { 0, 4103, 0, 0 },
        .facingNormal = { -4096, 0, 0, 0 },
        .radius       = 844,
        .control      = WORLD_COLLISION_TRIGGER_ACTION_WARP,
        .parameter0   = GAME_AREA_DRYFIELD_BREEZEWAY,
        .parameter1   = (DRYFIELD_NIGHT_WATER_TOWER_WARP_BREEZEWAY << DRYFIELD_NIGHT_WATER_TOWER_WARP_ENDPOINT_SHIFT) | 1,
        .flags        = WORLD_COLLISION_TRIGGER_FACING_QUAD,
    },
    {
        .origin       = { -2290, -48, 0, 0 },
        .vertices     = { { -591, 0, -480, 0 }, { 592, 0, -480, 0 }, { -591, 0, 480, 0 }, { 592, 0, 480, 0 } },
        .normal       = { 0, 4121, 0, 0 },
        .facingNormal = { 4096, 0, 0, 0 },
        .radius       = 759,
        .control      = WORLD_COLLISION_TRIGGER_ACTION_WARP,
        .parameter0   = GAME_AREA_DRYFIELD_WATER_TANK,
        .parameter1   = (DRYFIELD_NIGHT_WATER_TOWER_WARP_WATER_TANK << DRYFIELD_NIGHT_WATER_TOWER_WARP_ENDPOINT_SHIFT) | 1,
        .flags        = WORLD_COLLISION_TRIGGER_FACING_QUAD,
    },
    {
        .origin       = { 1008, -64, 3664, 0 },
        .vertices     = { { -400, 0, -336, 0 }, { 528, 0, -592, 0 }, { -848, 0, 464, 0 }, { 720, 0, 464, 0 } },
        .normal       = { 0, 4105, 0, 0 },
        .facingNormal = { 201, 0, 4091, 0 },
        .radius       = 966,
        .control      = WORLD_COLLISION_TRIGGER_ACTION_CAP,
        .parameter0   = DRYFIELD_NIGHT_WATER_TOWER_TRIGGER_MECHANISM_COMMAND,
        .parameter1   = WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE,
        .flags        = WORLD_COLLISION_TRIGGER_FACING_QUAD,
    },
    {
        .origin       = { -4336, -64, -4416, 0 },
        .vertices     = { { -1888, 0, -2304, 0 }, { 1888, 0, 1472, 0 }, { -1888, 0, -1472, 0 }, { 1888, 0, 2304, 0 } },
        .normal       = { 0, 4104, 0, 0 },
        .facingNormal = { 0, 0, 4096, 0 },
        .radius       = 2974,
        .control      = WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC,
        .parameter0   = DRYFIELD_NIGHT_WATER_TOWER_TRIGGER_ROOM_ACTION_1,
        .parameter1   = 0,
        .flags        = WORLD_COLLISION_TRIGGER_QUAD,
    },
    {
        .origin       = { 5568, -48, -5248, 0 },
        .vertices     = { { 416, 0, -624, 0 }, { 416, 0, 624, 0 }, { -416, 0, -624, 0 }, { -416, 0, 624, 0 } },
        .normal       = { 0, 4095, 0, 0 },
        .facingNormal = { -4096, 0, 0, 0 },
        .radius       = 749,
        .control      = WORLD_COLLISION_TRIGGER_ACTION_WARP,
        .parameter0   = GAME_AREA_DRYFIELD_MOTEL_ROOM_6,
        .parameter1   = (DRYFIELD_NIGHT_WATER_TOWER_WARP_MOTEL_ROOM_6 << DRYFIELD_NIGHT_WATER_TOWER_WARP_ENDPOINT_SHIFT) | 2,
        .flags        = WORLD_COLLISION_TRIGGER_FACING_QUAD,
    },
    {
        .origin       = { -2304, -64, 416, 0 },
        .vertices     = { { -591, 0, -480, 0 }, { 592, 0, -480, 0 }, { -591, 0, 480, 0 }, { 592, 0, 480, 0 } },
        .normal       = { 0, 4121, 0, 0 },
        .facingNormal = { 201, 0, 4091, 0 },
        .radius       = 759,
        .control      = WORLD_COLLISION_TRIGGER_ACTION_WARP,
        .parameter0   = GAME_AREA_DRYFIELD_WATER_TANK,
        .parameter1   = (DRYFIELD_NIGHT_WATER_TOWER_WARP_WATER_TANK << DRYFIELD_NIGHT_WATER_TOWER_WARP_ENDPOINT_SHIFT) | 1,
        .flags        = WORLD_COLLISION_TRIGGER_FACING_QUAD,
    },
    {
        .origin       = { -3520, -64, -1248, 0 },
        .vertices     = { { -1552, 0, -1536, 0 }, { 432, 0, -1536, 0 }, { -1552, 0, 1536, 0 }, { 432, 0, 1536, 0 } },
        .normal       = { 0, 4097, 0, 0 },
        .facingNormal = { 0, 0, 4096, 0 },
        .radius       = 2172,
        .control      = WORLD_COLLISION_TRIGGER_ACTION_CAP,
        .parameter0   = DRYFIELD_NIGHT_WATER_TOWER_TRIGGER_CAP_COMMAND_12,
        .parameter1   = 0,
        .flags        = WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD,
    },
    {
        .origin       = { -3520, -192, -1280, 0 },
        .vertices     = { { -1552, 0, -1536, 0 }, { 432, 0, -1536, 0 }, { -1552, 0, 1536, 0 }, { 432, 0, 1536, 0 } },
        .normal       = { 0, 4097, 0, 0 },
        .facingNormal = { 0, 0, 4096, 0 },
        .radius       = 2172,
        .control      = WORLD_COLLISION_TRIGGER_ACTION_CAP,
        .parameter0   = DRYFIELD_NIGHT_WATER_TOWER_TRIGGER_CAP_COMMAND_12,
        .parameter1   = 0,
        .flags        = WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST,
    },
};

/// Scavenger and Moth load/spawn resources for night Water Tower variant 1.
///
/// Placement IDs 16 and 8 select base files 101600 and 300700 and descriptor
/// zero. `AREA_PLACEMENT_END` terminates the table. Keep the room table live
/// during loading/spawning and each actor overlay live until its tasks end.
static AreaResource _gDryfieldNightWaterTowerVariant1Resources[] = {
    {
        .entryId        = 16,
        .fileNumber     = 16,
        .fileGroupIndex = LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR,
        .taskIndex      = 0,
        .unknown_6      = { 0, 0 },
        .taskTable      = &gActor101600ScavengerTaskDesc,
    },
    {
        .entryId        = 8,
        .fileNumber     = 7,
        .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_30,
        .taskIndex      = 0,
        .unknown_6      = { 0, 0 },
        .taskTable      = &gActor300700MothTaskDesc,
    },
    {
        .entryId        = AREA_PLACEMENT_END,
        .fileNumber     = 0,
        .fileGroupIndex = 0,
        .taskIndex      = 0,
        .unknown_6      = { 0, 0 },
        .taskTable      = NULL,
    },
};

/// Zebra Stalker load/spawn resource for night Water Tower variant 2.
///
/// Placement ID 6 selects base file 400600 and descriptor zero; the paired
/// layout places three bodies. Includes `AREA_PLACEMENT_END`. The room table
/// is borrowed during loading/spawning and the actor overlay outlives its tasks.
static AreaResource _gDryfieldNightWaterTowerVariant2Resources[] = {
    {
        .entryId        = 6,
        .fileNumber     = 6,
        .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_40,
        .taskIndex      = 0,
        .unknown_6      = { 0, 0 },
        .taskTable      = &gActor400600ZebraStalkerTaskDesc,
    },
    {
        .entryId        = AREA_PLACEMENT_END,
        .fileNumber     = 0,
        .fileGroupIndex = 0,
        .taskIndex      = 0,
        .unknown_6      = { 0, 0 },
        .taskTable      = NULL,
    },
};

/// Ordinary Mad Chaser load/spawn resource for night Water Tower variant 11.
///
/// Placement ID 44 selects base file 104400 and the ordinary descriptor for
/// one body. Includes `AREA_PLACEMENT_END`. The room table is borrowed during
/// loading/spawning and the actor overlay outlives its task.
static AreaResource _gDryfieldNightWaterTowerVariant11Resources[] = {
    {
        .entryId        = 44,
        .fileNumber     = 44,
        .fileGroupIndex = LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR,
        .taskIndex      = ACTOR_04400_MAD_CHASER_TASK_ORDINARY,
        .unknown_6      = { 0, 0 },
        .taskTable      = gActor04400MadChaserTaskDescs,
    },
    {
        .entryId        = AREA_PLACEMENT_END,
        .fileNumber     = 0,
        .fileGroupIndex = 0,
        .taskIndex      = 0,
        .unknown_6      = { 0, 0 },
        .taskTable      = NULL,
    },
};

/// Zebra Stalker and Scorpion load/spawn resources for night Water Tower variant 21.
///
/// Placement IDs 6 and 25 select base files 400600 and 302500 and descriptor
/// zero. The paired layout places two Stalkers and four Scorpions. Includes
/// `AREA_PLACEMENT_END`; keep the room table live during loading/spawning and
/// each actor overlay live until its tasks end.
static AreaResource _gDryfieldNightWaterTowerVariant21Resources[] = {
    {
        .entryId        = 6,
        .fileNumber     = 6,
        .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_40,
        .taskIndex      = 0,
        .unknown_6      = { 0, 0 },
        .taskTable      = &gActor400600ZebraStalkerTaskDesc,
    },
    {
        .entryId        = 25,
        .fileNumber     = 25,
        .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_30,
        .taskIndex      = 0,
        .unknown_6      = { 0, 0 },
        .taskTable      = gActor02500Tasks,
    },
    {
        .entryId        = AREA_PLACEMENT_END,
        .fileNumber     = 0,
        .fileGroupIndex = 0,
        .taskIndex      = 0,
        .unknown_6      = { 0, 0 },
        .taskTable      = NULL,
    },
};

AreaVariant gDryfieldNightWaterTowerAreaVariants[DRYFIELD_NIGHT_WATER_TOWER_AREA_VARIANT_COUNT] = {
    [AREA_DEFAULT_VARIANT] = { .placements = gMapDryfieldFullWaterTowerVariant1Placements, .resources = _gDryfieldNightWaterTowerVariant1Resources },
    [2]                    = { .placements = gMapDryfieldFullWaterTowerVariant2Placements, .resources = _gDryfieldNightWaterTowerVariant2Resources },
    [11]                   = { .placements = gMapDryfieldFullWaterTowerVariant11Placements, .resources = _gDryfieldNightWaterTowerVariant11Resources },
    [21]                   = { .placements = gMapDryfieldFullWaterTowerVariant21Placements, .resources = _gDryfieldNightWaterTowerVariant21Resources },
};

/// Cue-2 footstep bases in the loaded type-1 sound bank.
enum {
    DRYFIELD_NIGHT_WATER_TOWER_WALK_FOOTSTEP_SOUND = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x49,
    DRYFIELD_NIGHT_WATER_TOWER_RUN_FOOTSTEP_SOUND  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x4B,
};

/// Footstep cue bases shared by surface classes 0 and 2, and aliases 3 through 7.
///
/// Walking and scripted jumps share one pair; running uses the next pair.
/// Cue 1 adds one to the base, and companions add 100. Borrowed while loaded.
static WorldCollisionFootstepSounds _gDryfieldNightWaterTowerFootstepSounds = {
    .walk         = DRYFIELD_NIGHT_WATER_TOWER_WALK_FOOTSTEP_SOUND,
    .run          = DRYFIELD_NIGHT_WATER_TOWER_RUN_FOOTSTEP_SOUND,
    .scriptedJump = DRYFIELD_NIGHT_WATER_TOWER_WALK_FOOTSTEP_SOUND,
};

/// Blocking, impact-enabled surface policy for class 0 and aliases 3 through 7.
///
/// Applies grid pushback and supplies footstep cues; the first byte's role is unproven.
static WorldCollisionSurfaceProperties _gDryfieldNightWaterTowerSurfaceClass0Properties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gDryfieldNightWaterTowerFootstepSounds,
};

/// Class-1 policy allowing probes through and suppressing weapon impacts and footsteps.
///
/// Grid pushback remains enabled; the first byte's role is unproven.
static WorldCollisionSurfaceProperties _gDryfieldNightWaterTowerSurfaceClass1Properties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_PASS_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

/// Separate class-2 policy blocking probes and enabling weapon impacts and footsteps.
///
/// Applies grid pushback, like class 0; the first byte's role is unproven.
static WorldCollisionSurfaceProperties _gDryfieldNightWaterTowerSurfaceClass2Properties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gDryfieldNightWaterTowerFootstepSounds,
};

WorldCollisionSurfaceProperties* gDryfieldNightWaterTowerSurfacePropertiesByClass[8] = {
    &_gDryfieldNightWaterTowerSurfaceClass0Properties,
    &_gDryfieldNightWaterTowerSurfaceClass1Properties,
    &_gDryfieldNightWaterTowerSurfaceClass2Properties,
    &_gDryfieldNightWaterTowerSurfaceClass0Properties,
    &_gDryfieldNightWaterTowerSurfaceClass0Properties,
    &_gDryfieldNightWaterTowerSurfaceClass0Properties,
    &_gDryfieldNightWaterTowerSurfaceClass0Properties,
    &_gDryfieldNightWaterTowerSurfaceClass0Properties,
};

static void _glowDrawFlare(const SVECTOR* worldPoint, s32 textureIndex, s32 radiusScale);
static void _glowDrawShaft(const SVECTOR worldPoints[2], s32 radiusScale);

/// Queues two coincident additive flare passes to reinforce one world-point glow.
///
/// Borrows one word-aligned, readable `SVECTOR` for the call. The signed low
/// halfword of `textureIndex` selects a 40-texel texture column and palette
/// offset; the signed low halfword of `radiusScale` gives each square's pixel
/// half-extent as `radiusScale * 39 / depth`, with depth equal to camera Z / 4.
/// Negative GTE flags emit no packets; accepted points require nonzero depth.
/// Both passes use the same frame's flicker intensity and projected bounds.
///
/// Requires the current view matrix, an initialized scratch stack with room
/// for one `GlowCentreScratch`, and a frame packet arena with room for two
/// `POLY_FT4`s. Queued packets remain live until GPU completion; the world
/// point is not retained.
static inline void _dryfieldNightWaterTowerDrawDoubleFlare(const SVECTOR* worldPoint, s32 textureIndex, s32 radiusScale)
{
    _glowDrawFlare(worldPoint, textureIndex, radiusScale);
    _glowDrawFlare(worldPoint, textureIndex, radiusScale);
}

void dryfieldNightWaterTowerDrawGlowsTask(Task* task)
{
    enum {
        DRYFIELD_NIGHT_WATER_TOWER_FLARE_TEXTURE_INDEX = 2,     // 40-texel column and palette offset
        DRYFIELD_NIGHT_WATER_TOWER_FLARE_RADIUS_SCALE  = 0x400, // Pixel half-extent = scale * 39 / (camera Z / 4)
        DRYFIELD_NIGHT_WATER_TOWER_SHAFT_RADIUS_SCALE  = 0x100, // Pixel end radius = scale * 64 / (camera Z / 4)
    };

    gRoomEffectState->roomEffectMode = ROOM_EFFECT_VIEW_ENABLED;
    // Select glow anchors by the room-local view ID.
    switch (gGameSession->location.view) {
        case 2:
            _glowDrawFlare(&_gDryfieldNightWaterTowerGlowPoints.views2And4FlarePoint, DRYFIELD_NIGHT_WATER_TOWER_FLARE_TEXTURE_INDEX,
                           DRYFIELD_NIGHT_WATER_TOWER_FLARE_RADIUS_SCALE);
            break;
        case 3:
            _glowDrawShaft(_gDryfieldNightWaterTowerGlowPoints.shaftEndpoints, DRYFIELD_NIGHT_WATER_TOWER_SHAFT_RADIUS_SCALE);
            _glowDrawFlare(&_gDryfieldNightWaterTowerGlowPoints.views3And4FlarePoint, DRYFIELD_NIGHT_WATER_TOWER_FLARE_TEXTURE_INDEX,
                           DRYFIELD_NIGHT_WATER_TOWER_FLARE_RADIUS_SCALE);
            break;
        case 4:
            _glowDrawShaft(_gDryfieldNightWaterTowerGlowPoints.shaftEndpoints, DRYFIELD_NIGHT_WATER_TOWER_SHAFT_RADIUS_SCALE);
            _glowDrawFlare(&_gDryfieldNightWaterTowerGlowPoints.views3And4FlarePoint, DRYFIELD_NIGHT_WATER_TOWER_FLARE_TEXTURE_INDEX,
                           DRYFIELD_NIGHT_WATER_TOWER_FLARE_RADIUS_SCALE);
            _glowDrawFlare(&_gDryfieldNightWaterTowerGlowPoints.views2And4FlarePoint, DRYFIELD_NIGHT_WATER_TOWER_FLARE_TEXTURE_INDEX,
                           DRYFIELD_NIGHT_WATER_TOWER_FLARE_RADIUS_SCALE);
            break;
        case 5:
            _glowDrawFlare(&_gDryfieldNightWaterTowerGlowPoints.view5FlarePoint, DRYFIELD_NIGHT_WATER_TOWER_FLARE_TEXTURE_INDEX,
                           DRYFIELD_NIGHT_WATER_TOWER_FLARE_RADIUS_SCALE);
            break;
        case 7:
        case 10:
            _dryfieldNightWaterTowerDrawDoubleFlare(&_gDryfieldNightWaterTowerDoubleFlarePoint, DRYFIELD_NIGHT_WATER_TOWER_FLARE_TEXTURE_INDEX,
                                                    DRYFIELD_NIGHT_WATER_TOWER_FLARE_RADIUS_SCALE);
            break;
    }
}

#include "../../shared/effects/glow_draw_shaft.inc.c"

#include "../../shared/effects/glow_draw_flare.inc.c"
