#include "rooms/dryfield_night_junk_yard.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "gte.h"
#include "common.h"

#include "actors/actor_00100.h"
#include "actors/actor_01500.h"
#include "actors/actor_02300.h"
#include "actors/actor_05600.h"
#include "actors/actor_400600.h"

#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/area.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/effects.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/light.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/coord.h"
#include "main/areas.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gamemain.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound_ids.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "mapui/map_dryfield_full.h"

#include "rooms/room_common.h"
#include "../../shared/room/room_visual_effects.h"
#include "../../shared/effects/glow_draw.h"

/// Perspective radius scales for the room's capsule and its two flare sizes.
///
/// The capsule scales by 64 / depth and flares by 39 / depth, with depth = camera Z / 4.
enum {
    DRYFIELD_NIGHT_JUNK_YARD_CAPSULE_RADIUS_SCALE     = 0x180,
    DRYFIELD_NIGHT_JUNK_YARD_LARGE_FLARE_RADIUS_SCALE = 0x300,
    DRYFIELD_NIGHT_JUNK_YARD_SMALL_FLARE_RADIUS_SCALE = 0x200
};

static WorldCollisionGrid     _gDryfieldNightJunkYardCollisionGrid;
static WorldCollisionOccluder _gDryfieldNightJunkYardSightOccluders[1];
static WorldCollisionTrigger  _gDryfieldNightJunkYardViewBoundaryTriggers[10];
static WorldCollisionTrigger  _gDryfieldNightJunkYardActionTriggers[20];
static WorldCoordRoomLights   _gDryfieldNightJunkYardRoomLights;

/// World-space capsule endpoints followed by the junk yard's five flare centres.
///
/// Entries 0..1 are one capsule; 2..6 are flare centres in signed game units.
/// Camera views select overlapping spans of this seven-point table, borrowed
/// only during drawing while the room overlay is loaded.
static SVECTOR _gDryfieldNightJunkYardGlowWorldPoints[7] = {
    { 4021, -2596, 348, 0 },
    { 4021, -2396, 71, 0 },
    { -46, -2914, 4605, 0 },
    { 0x278A, -2914, 4897, 0 },
    { 0x589B, -2917, 4106, 0 },
    { 0x4718, -4530, 1335, 0 },
    { 0x5274, -2451, 968, 0 },
};

#include "../../shared/room/room_visual_effects_trail_data.inc.c"

WorldCoordRoomLighting gDryfieldNightJunkYardRoomLighting[2] = {
    { .lights = &_gDryfieldNightJunkYardRoomLights, .ambientTable = NULL },
    { .lights = &_gDryfieldNightJunkYardRoomLights, .ambientTable = NULL },
};

WorldCollisionRoomResources gDryfieldNightJunkYardRoomCollisionResources[2] = {
    { .grid = &_gDryfieldNightJunkYardCollisionGrid, .viewBoundaryTriggers = _gDryfieldNightJunkYardViewBoundaryTriggers, .actionTriggers = _gDryfieldNightJunkYardActionTriggers, .occluders = _gDryfieldNightJunkYardSightOccluders },
    { .grid = &_gDryfieldNightJunkYardCollisionGrid, .viewBoundaryTriggers = _gDryfieldNightJunkYardViewBoundaryTriggers, .actionTriggers = _gDryfieldNightJunkYardActionTriggers, .occluders = _gDryfieldNightJunkYardSightOccluders },
};

/// Room 2's logical-view to camera/image/sprite index mapping.
///
/// Logical views 1..10 index bytes 0..9 and map to resource indices 1..10.
/// The two retained zero bytes have no established use and are outside the
/// reverse-search count. The room directory borrows this mutable array.
static u8 _gDryfieldNightJunkYardRoom2ViewMap[] = {
    1,
    8,
    3,
    9,
    10,
    6,
    7,
    2,
    4,
    5,
    0,
    0,
};

u8* gDryfieldNightJunkYardViewMaps[2] = {
    gViewIdentityMap,
    _gDryfieldNightJunkYardRoom2ViewMap,
};

enum { DRYFIELD_NIGHT_JUNK_YARD_LOGICAL_VIEW_COUNT = 10 };
ViewCount gDryfieldNightJunkYardViewCounts[2] = {
    DRYFIELD_NIGHT_JUNK_YARD_LOGICAL_VIEW_COUNT,
    DRYFIELD_NIGHT_JUNK_YARD_LOGICAL_VIEW_COUNT,
};

/// Room-bank cues and map-marker nibbles for the three warp endpoints.
enum {
    DRYFIELD_NIGHT_JUNK_YARD_GARAGE_ARRIVAL_SOUND          = SOUND_AREA(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_JUNK_YARD, 4),
    DRYFIELD_NIGHT_JUNK_YARD_GARAGE_DEPARTURE_SOUND        = SOUND_AREA(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_JUNK_YARD, 3),
    DRYFIELD_NIGHT_JUNK_YARD_TRAILER_COACH_ARRIVAL_SOUND   = SOUND_AREA(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_JUNK_YARD, 2),
    DRYFIELD_NIGHT_JUNK_YARD_TRAILER_COACH_DEPARTURE_SOUND = SOUND_AREA(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_JUNK_YARD, 1),
    DRYFIELD_NIGHT_JUNK_YARD_TRAILER_COACH_BLOCKED_SOUND   = SOUND_AREA(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_JUNK_YARD, 11),
    DRYFIELD_NIGHT_JUNK_YARD_WARP_3_ARRIVAL_SOUND          = SOUND_AREA(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_JUNK_YARD, 6),
    DRYFIELD_NIGHT_JUNK_YARD_WARP_3_DEPARTURE_SOUND        = SOUND_AREA(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_JUNK_YARD, 5),
    DRYFIELD_NIGHT_JUNK_YARD_GARAGE_WARP_MAP_FLAG          = 472,
    DRYFIELD_NIGHT_JUNK_YARD_TRAILER_COACH_WARP_MAP_FLAG   = 471,
};

DirectionWarpEntry gDryfieldNightJunkYardWarpEntries[3] = {
    {
        .player         = { .yaw = { .word = 0 }, .x = 5030, .y = 4, .z = 390 },
        .companion      = { .yaw = { .word = 0 }, .x = 5030, .y = 4, .z = 390 },
        .arrivalSound   = DRYFIELD_NIGHT_JUNK_YARD_GARAGE_ARRIVAL_SOUND,
        .departureSound = DRYFIELD_NIGHT_JUNK_YARD_GARAGE_DEPARTURE_SOUND,
        .blockedSound   = DIRECTION_WARP_SOUND_NONE,
        .initialView    = 2,
        .flags          = DIRECTION_WARP_FLAG_NONE,
        .mapFlagId      = DRYFIELD_NIGHT_JUNK_YARD_GARAGE_WARP_MAP_FLAG,
    },
    {
        .player         = { .yaw = { .word = 0 }, .x = 0x55A1, .y = 4, .z = 1639 },
        .companion      = { .yaw = { .word = 0 }, .x = 0x55A1, .y = 4, .z = 1639 },
        .arrivalSound   = DRYFIELD_NIGHT_JUNK_YARD_TRAILER_COACH_ARRIVAL_SOUND,
        .departureSound = DRYFIELD_NIGHT_JUNK_YARD_TRAILER_COACH_DEPARTURE_SOUND,
        .blockedSound   = DRYFIELD_NIGHT_JUNK_YARD_TRAILER_COACH_BLOCKED_SOUND,
        .initialView    = 5,
        .flags          = DIRECTION_WARP_FLAG_NONE,
        .mapFlagId      = DRYFIELD_NIGHT_JUNK_YARD_TRAILER_COACH_WARP_MAP_FLAG,
    },
    {
        .player         = { .yaw = { .word = 0 }, .x = 1859, .y = 4, .z = 672 },
        .companion      = { .yaw = { .word = 0 }, .x = 1859, .y = 4, .z = 672 },
        .arrivalSound   = DRYFIELD_NIGHT_JUNK_YARD_WARP_3_ARRIVAL_SOUND,
        .departureSound = DRYFIELD_NIGHT_JUNK_YARD_WARP_3_DEPARTURE_SOUND,
        .blockedSound   = DIRECTION_WARP_SOUND_NONE,
        .initialView    = 3,
        .flags          = DIRECTION_WARP_FLAG_NONE,
        .mapFlagId      = DIRECTION_WARP_MAP_FLAG_NONE,
    },
};

/// Unit-scale room-space normals for the junk-yard collision grid (4096 = 1.0).
static SVECTOR _gDryfieldNightJunkYardCollisionGridNormals[] = {
#include "assets/dryfield_night_junk_yard_collision_03BF8_normals.inc"
};

/// Collision-grid vertices in signed room-coordinate units.
static SVECTOR _gDryfieldNightJunkYardCollisionGridVerts[] = {
#include "assets/dryfield_night_junk_yard_collision_03BF8_verts.inc"
};

/// Indexed grid faces selecting junk-yard surface classes 0, 1 and 2.
static WorldCollisionGridFace _gDryfieldNightJunkYardCollisionGridFaces[] = {
#include "assets/dryfield_night_junk_yard_collision_03BF8_faces.inc"
};

/// Serialized face-index lists, each ending at WORLD_COLLISION_GRID_CELL_END.
///
/// Cell pointers select the list starts; retained words between lists are not
/// additional face entries in the preceding list.
static s16 _gDryfieldNightJunkYardCollisionGridCells[] = {
#include "assets/dryfield_night_junk_yard_collision_03BF8_cells.inc"
};

/// Supplies this grid's cell face-list addresses to its generated table.
///
/// Captures `_gDryfieldNightJunkYardCollisionGridCells`.
/// `halfwordOffset` is a constant s16-element offset in 0..229 to a list's
/// first entry, evaluated once; the list ends at WORLD_COLLISION_GRID_CELL_END.
/// Returns a borrowed s16* without modifying the pool. `cell_face_list_macro`
/// selects this binding for the table include; it is undefined afterwards.
#define DRYFIELD_NIGHT_JUNK_YARD_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gDryfieldNightJunkYardCollisionGridCells[(halfwordOffset)])
/// X-major cell directory for the collision grid's seven-by-two XZ cells.
static s16* _gDryfieldNightJunkYardCollisionGridTable[] = {
#include "assets/dryfield_night_junk_yard_collision_03BF8_table.inc"
};
#undef DRYFIELD_NIGHT_JUNK_YARD_CELL_FACE_LIST_AT_OFFSET

/// Shared collision mesh and spatial index for junk-yard rooms 1 and 2.
///
/// Covers seven by two cells of side 4000 in room-coordinate units, starting
/// at X = -2400, Z = -610. Activation binds the view coordinate; the descriptor
/// and its mutable pools must remain loaded while the grid is active.
static WorldCollisionGrid _gDryfieldNightJunkYardCollisionGrid = {
    .viewCoord   = NULL,
    .normals     = _gDryfieldNightJunkYardCollisionGridNormals,
    .vertices    = _gDryfieldNightJunkYardCollisionGridVerts,
    .faces       = _gDryfieldNightJunkYardCollisionGridFaces,
    .cellFaceIds = _gDryfieldNightJunkYardCollisionGridTable,
    .xBias       = 2400,
    .zBias       = 610,
    .cellCountX  = 7,
    .cellCountZ  = 2,
    .cellSize    = 4000,
    .faceCount   = ARRAY_SIZE(_gDryfieldNightJunkYardCollisionGridFaces),
};

ViewCamera gDryfieldNightJunkYardViewCameras[11] = {
    { { { { ONE, 0, 0 }, { 0, 0, -ONE }, { 0, ONE, 0 } }, { -0x2DE6, 0x61A8, -5750 } }, 257 },
    { { { { 965, 0, -3980 }, { -535, 4058, -129 }, { 3944, 550, 956 } }, { 1022, 2299, -989 } }, 282 },
    { { { { 783, 0, -4020 }, { -3412, 2165, -665 }, { 2125, 3476, 414 } }, { 1120, 4985, -2136 } }, 239 },
    { { { { 1009, 0, -3969 }, { -1187, 3908, -301 }, { 3788, 1224, 962 } }, { -2958, 2969, -1227 } }, 282 },
    { { { { 66, 0, 4095 }, { 1199, 3916, -19 }, { -3915, 1199, 63 } }, { -0x63AC, 2717, -2429 } }, 289 },
    { { { { 3309, 0, -2413 }, { -53, 4094, -73 }, { 2412, 91, 3308 } }, { -1910, 2342, -1845 } }, 329 },
    { { { { 1162, 0, 3927 }, { 1136, 3920, -336 }, { -3759, 1185, 1113 } }, { -0x4970, 2301, -1670 } }, 269 },
    { { { { 965, 0, -3980 }, { -535, 4058, -129 }, { 3944, 550, 956 } }, { 1022, 2299, -989 } }, 282 },
    { { { { 1009, 0, -3969 }, { -1187, 3908, -301 }, { 3788, 1224, 962 } }, { -2958, 2969, -1227 } }, 282 },
    { { { { 66, 0, 4095 }, { 1199, 3916, -19 }, { -3915, 1199, 63 } }, { -0x63AC, 2717, -2429 } }, 289 },
    { { { { 1162, 0, 3927 }, { 1136, 3920, -336 }, { -3759, 1185, 1113 } }, { -0x4970, 2301, -1670 } }, 269 },
};

/// Empty sprite list for mapped junk-yard view 1, retaining the decoded background.
///
/// The zero-count first record selects image strips; SPRITE_BATCH_END then ends
/// the list without reading the retained source-slot pointer as sprite data.
static SpriteBatch _gDryfieldNightJunkYardView1SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Authored sprite rectangles and sorting depths for mapped junk-yard view 2.
///
/// The view's batches cover all 30 elements. Positions and sizes are pixels;
/// depth is the unscaled OT sorting value. The view descriptor borrows this
/// mutable array while the room overlay is loaded.
static SpriteSource _gDryfieldNightJunkYardView2SpriteSources[] = {
    { 143, 0x3FC0, { .fields = { 32, 128 } }, -40, -104, 3066, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 64 } }, 24, -72, 6500, { .fields = { 104, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 120 } }, 56, -112, 5000, { .fields = { 16, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -72, 64, 1304, { .fields = { 88, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 72 } }, -96, -32, 1248, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 80 } }, -160, -40, 1068, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, -88, 48, 1199, { .fields = { 96, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -96, 56, 1121, { .fields = { 16, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 72 } }, -160, 48, 937, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -160, 40, 1038, { .fields = { 64, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, -120, 40, 963, { .fields = { 8, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -112, 40, 1206, { .fields = { 104, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -112, 48, 1149, { .fields = { 32, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 64 } }, -112, 56, 1011, { .fields = { 96, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 56 } }, -104, 56, 1049, { .fields = { 96, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -80, 64, 1256, { .fields = { 104, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -80, 40, 1282, { .fields = { 112, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 40, -24, 4346, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 32, -16, 4080, { .fields = { 56, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 16 } }, 24, -8, 3816, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 120, 64, 1172, { .fields = { 16, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 112, 64, 1189, { .fields = { 24, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, 120, 72, 1150, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 128, 72, 1160, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 64, -16, 3495, { .fields = { 72, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 72, -40, 3638, { .fields = { 0, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 80, -72, 3625, { .fields = { 120, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 128 } }, 88, -104, 3625, { .fields = { 56, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 96 } }, 96, 24, 3500, { .fields = { 64, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 144 } }, 96, -120, 3500, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
};

/// Sprite ranges for mapped junk-yard view 2, over its complete source array.
///
/// The zero-count first record keeps the decoded background; SPRITE_BATCH_END
/// terminates the list. Starts and counts are in source elements.
static SpriteBatch _gDryfieldNightJunkYardView2SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 3, 0, 0, { 0, 0 } },
    { 3, 14, 0, 0, { 3, 0 } },
    { 17, 3, 0, 0, { 2, 0 } },
    { 20, 4, 0, 0, { 4, 0 } },
    { 24, ARRAY_SIZE(_gDryfieldNightJunkYardView2SpriteSources) - 24, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Authored sprite rectangles and sorting depths for mapped junk-yard view 3.
///
/// The view's batches cover all 58 elements. Positions and sizes are pixels;
/// depth is the unscaled OT sorting value. The view descriptor borrows this
/// mutable array while the room overlay is loaded.
static SpriteSource _gDryfieldNightJunkYardView3SpriteSources[] = {
    { 142, 0x3FC0, { .fields = { 88, 24 } }, -104, -80, 1516, { .fields = { 96, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -104, 0, 1538, { .fields = { 64, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -56, 0, 1522, { .fields = { 56, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 56 } }, -64, -56, 1522, { .fields = { 80, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 56 } }, -104, -56, 1538, { .fields = { 88, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 40 } }, -64, -120, 1372, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -104, -120, 1388, { .fields = { 40, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 104, -64, 1558, { .fields = { 48, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 104, -56, 1577, { .fields = { 64, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 104, -48, 1578, { .fields = { 32, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 112, 56, 984, { .fields = { 80, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, 64, 1124, { .fields = { 80, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 112, 64, 1038, { .fields = { 40, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 120, 64, 976, { .fields = { 56, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, 72, 955, { .fields = { 40, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 112, 72, 951, { .fields = { 40, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 120, 72, 952, { .fields = { 32, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 128, 72, 949, { .fields = { 32, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 136, 72, 951, { .fields = { 32, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, 80, 942, { .fields = { 32, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 112, 80, 937, { .fields = { 32, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 120, 80, 934, { .fields = { 32, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 128, 80, 930, { .fields = { 32, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 136, 80, 931, { .fields = { 32, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, 88, 929, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 112, 88, 923, { .fields = { 24, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 120, 88, 920, { .fields = { 48, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 128, 88, 915, { .fields = { 24, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 136, 88, 916, { .fields = { 24, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, 88, 918, { .fields = { 24, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, 96, 916, { .fields = { 24, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 112, 96, 906, { .fields = { 24, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 120, 96, 903, { .fields = { 24, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 128, 96, 900, { .fields = { 24, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 136, 96, 903, { .fields = { 24, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, 96, 904, { .fields = { 24, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, 104, 918, { .fields = { 24, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 112, 104, 898, { .fields = { 24, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 120, 104, 888, { .fields = { 24, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 128, 104, 884, { .fields = { 24, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 136, 104, 885, { .fields = { 24, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, 104, 890, { .fields = { 24, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, 112, 963, { .fields = { 24, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 112, 112, 938, { .fields = { 24, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 120, 112, 931, { .fields = { 24, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 128, 112, 887, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 136, 112, 878, { .fields = { 24, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, 112, 886, { .fields = { 24, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -48, 24, 950, { .fields = { 96, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 64 } }, 56, 56, 1134, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 24 } }, -136, 24, 1220, { .fields = { 0, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -88, 32, 1270, { .fields = { 56, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -72, 48, 1196, { .fields = { 64, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -56, 56, 1174, { .fields = { 80, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -8, 64, 1158, { .fields = { 32, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 24, 72, 1142, { .fields = { 24, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -104, 48, 1184, { .fields = { 48, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -136, 48, 1200, { .fields = { 32, 216 } }, 128, 128, 128, 0 },
};

/// Sprite ranges for mapped junk-yard view 3, over its complete source array.
///
/// The zero-count first record keeps the decoded background; SPRITE_BATCH_END
/// terminates the list. Starts and counts are in source elements.
static SpriteBatch _gDryfieldNightJunkYardView3SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 7, 0, 0, { 1, 0 } },
    { 7, 3, 0, 0, { 2, 0 } },
    { 10, ARRAY_SIZE(_gDryfieldNightJunkYardView3SpriteSources) - 10, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Authored sprite rectangles and sorting depths for mapped junk-yard view 4.
///
/// The view's batches cover all 25 elements. Positions and sizes are pixels;
/// depth is the unscaled OT sorting value. The view descriptor borrows this
/// mutable array while the room overlay is loaded.
static SpriteSource _gDryfieldNightJunkYardView4SpriteSources[] = {
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -64, -16, 2084, { .fields = { 32, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -56, 0, 2125, { .fields = { 48, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -80, 16, 2042, { .fields = { 24, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -80, 0, 2069, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -72, -24, 2101, { .fields = { 112, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -80, -40, 2069, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -80, -120, 2069, { .fields = { 112, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 80, -88, 3836, { .fields = { 104, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 88, -120, 3836, { .fields = { 104, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 24, -80, 6000, { .fields = { 120, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 24, -112, 6000, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 40, -56, 3550, { .fields = { 16, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 8 } }, 32, -48, 3452, { .fields = { 0, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 8 } }, 24, -40, 3366, { .fields = { 120, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 24, -32, 2914, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 64, -32, 2838, { .fields = { 104, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 40, -32, 3473, { .fields = { 56, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 40, -24, 2857, { .fields = { 32, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 136 } }, 104, -120, 2500, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 80, -72, 2750, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 80, -32, 2545, { .fields = { 56, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 80 } }, 120, 40, 2500, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 56 } }, 112, -16, 2500, { .fields = { 56, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 56 } }, 112, -72, 2500, { .fields = { 56, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, -120, 2500, { .fields = { 56, 192 } }, 128, 128, 128, 0 },
};

/// Sprite ranges for mapped junk-yard view 4, over its complete source array.
///
/// The zero-count first record keeps the decoded background; SPRITE_BATCH_END
/// terminates the list. Starts and counts are in source elements.
static SpriteBatch _gDryfieldNightJunkYardView4SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 11, 0, 0, { 1, 0 } },
    { 11, 7, 0, 0, { 2, 0 } },
    { 18, ARRAY_SIZE(_gDryfieldNightJunkYardView4SpriteSources) - 18, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Authored sprite rectangles and sorting depths for mapped junk-yard view 5.
///
/// The view's batches cover all 97 elements. Positions and sizes are pixels;
/// depth is the unscaled OT sorting value. The view descriptor borrows this
/// mutable array while the room overlay is loaded.
static SpriteSource _gDryfieldNightJunkYardView5SpriteSources[] = {
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 16, -112, 8250, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 88 } }, -96, -48, 1825, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -96, -120, 1825, { .fields = { 80, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 32, -80, 4051, { .fields = { 16, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 40, -120, 4051, { .fields = { 96, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, 8, -88, 7006, { .fields = { 88, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 0, -40, 2314, { .fields = { 16, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 8, -40, 2762, { .fields = { 24, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 0, -32, 2398, { .fields = { 40, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 8, -32, 2533, { .fields = { 48, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 8, -24, 2312, { .fields = { 24, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 8, -16, 2122, { .fields = { 16, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 8, -8, 1963, { .fields = { 16, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 8, 0, 1825, { .fields = { 24, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 8, 8, 1720, { .fields = { 16, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 8, 16, 1627, { .fields = { 48, 192 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 56, 8 } }, -56, -40, 2182, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 56, 8 } }, -56, -32, 1941, { .fields = { 88, 248 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 64, 8 } }, -64, -24, 1907, { .fields = { 80, 184 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 72, 8 } }, -72, -16, 1862, { .fields = { 56, 24 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 80, 8 } }, -72, -8, 1785, { .fields = { 40, 40 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 88, 8 } }, -80, 0, 1689, { .fields = { 40, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 0, -24, 2039, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 88, 24 } }, -80, 8, 1673, { .fields = { 80, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -72, 72, 1256, { .fields = { 56, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -88, -48, 1372, { .fields = { 32, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -88, 0, 1267, { .fields = { 40, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -64, 16, 1379, { .fields = { 80, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 40 } }, -56, 40, 1314, { .fields = { 40, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 40 } }, -56, 0, 1388, { .fields = { 40, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, -160, -120, 826, { .fields = { 48, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, -160, -80, 841, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, -160, -40, 823, { .fields = { 56, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, -160, 0, 833, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, -160, 40, 816, { .fields = { 64, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, -160, 80, 831, { .fields = { 120, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, -120, -120, 1075, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, -120, -80, 1075, { .fields = { 120, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, -120, -40, 1075, { .fields = { 120, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -120, 0, 1075, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -96, -120, 1290, { .fields = { 104, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, -96, -80, 1290, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, -96, -40, 1290, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, -96, 0, 1290, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -120, 104, 569, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -88, 24, 1275, { .fields = { 40, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, -120, 40, 1075, { .fields = { 24, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -96, 40, 1253, { .fields = { 24, 224 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 8 } }, -120, 80, 999, { .fields = { 80, 64 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 8 } }, -120, 88, 968, { .fields = { 88, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -120, 96, 943, { .fields = { 120, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -120, 72, 1035, { .fields = { 16, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, 0, 1521, { .fields = { 80, 152 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 8 } }, 120, -16, 1266, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 8 } }, 96, -8, 1584, { .fields = { 96, 56 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 24 } }, 120, -8, 1200, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 8 } }, 128, 16, 1200, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 136, 24, 1050, { .fields = { 0, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 144, 40, 1075, { .fields = { 48, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 144, 48, 974, { .fields = { 96, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 152, 48, 931, { .fields = { 88, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, 136, 40, 1035, { .fields = { 96, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 96 } }, 128, 24, 1102, { .fields = { 120, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 120, 16, 1203, { .fields = { 64, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 112, 0, 1290, { .fields = { 72, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 104, 8, 1376, { .fields = { 48, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 96, 0, 1476, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 80, 8, 1618, { .fields = { 80, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 112, 0, 1290, { .fields = { 72, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 88 } }, 120, -16, 1237, { .fields = { 104, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 136 } }, 128, -16, 1151, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 128 } }, 136, -8, 1067, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 128 } }, 144, -8, 990, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 128 } }, 152, -8, 959, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, 48, -48, 2676, { .fields = { 112, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, 56, -48, 2513, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 64, -56, 2167, { .fields = { 56, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, 72, -56, 2248, { .fields = { 16, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 16 } }, 72, -48, 2192, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 72, 8 } }, 72, -32, 2202, { .fields = { 72, 176 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 80, 8 } }, 80, -24, 2196, { .fields = { 88, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, 72, -24, 2034, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 80, -16, 1882, { .fields = { 32, 224 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 72, 32 } }, 88, -16, 1887, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 24 } }, -152, 96, 575, { .fields = { 112, 192 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 24 } }, -112, 96, 575, { .fields = { 96, 216 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 24 } }, -72, 96, 575, { .fields = { 104, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, -32, 96, 575, { .fields = { 8, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, 8, 96, 600, { .fields = { 120, 40 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 24 } }, 64, 96, 575, { .fields = { 120, 40 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 24 } }, 112, 96, 500, { .fields = { 120, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 32 } }, 112, 64, 575, { .fields = { 16, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -64, -24, 2394, { .fields = { 120, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -72, -48, 2252, { .fields = { 64, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -80, -56, 2177, { .fields = { 72, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 80, 64 } }, -160, -56, 2200, { .fields = { 120, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 64 } }, -80, -72, 2583, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
};

/// Sprite ranges for mapped junk-yard view 5, over its complete source array.
///
/// The zero-count first record keeps the decoded background; SPRITE_BATCH_END
/// terminates the list. Starts and counts are in source elements.
static SpriteBatch _gDryfieldNightJunkYardView5SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 5, 0, 0, { 6, 0 } },
    { 5, 1, 0, 0, { 1, 0 } },
    { 6, 18, 0, 0, { 8, 0 } },
    { 24, 28, 0, 0, { 0, 0 } },
    { 52, 16, 0, 0, { 5, 0 } },
    { 68, 6, 0, 0, { 2, 0 } },
    { 74, 10, 0, 0, { 9, 0 } },
    { 84, 8, 0, 0, { 4, 0 } },
    { 92, 4, 0, 0, { 7, 0 } },
    { 96, ARRAY_SIZE(_gDryfieldNightJunkYardView5SpriteSources) - 96, 0, 0, { 3, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Empty sprite list for mapped junk-yard view 6, retaining the decoded background.
///
/// The zero-count first record selects image strips; SPRITE_BATCH_END then ends
/// the list without reading the retained source-slot pointer as sprite data.
static SpriteBatch _gDryfieldNightJunkYardView6SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Authored sprite rectangles and sorting depths for mapped junk-yard view 7.
///
/// The view's batches cover all 64 elements. Positions and sizes are pixels;
/// depth is the unscaled OT sorting value. The view descriptor borrows this
/// mutable array while the room overlay is loaded.
static SpriteSource _gDryfieldNightJunkYardView7SpriteSources[] = {
    { 143, 0x3FC0, { .fields = { 32, 56 } }, -56, -88, 4064, { .fields = { 80, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 8, -32, 2340, { .fields = { 72, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 16, -32, 2312, { .fields = { 80, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, 16, -120, 2402, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 144, -24, 1012, { .fields = { 40, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, 88, -16, 1347, { .fields = { 40, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 112, -16, 1388, { .fields = { 40, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 72, -8, 1363, { .fields = { 120, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 80, -8, 1312, { .fields = { 104, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 88, -8, 1246, { .fields = { 112, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 104, 0, 1148, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 120, 0, 1078, { .fields = { 112, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 104, -8, 1146, { .fields = { 40, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 136, -8, 995, { .fields = { 56, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 136, 32, 1004, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 24, -24, 2186, { .fields = { 104, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 64, 0, 1521, { .fields = { 56, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -160, -16, 1314, { .fields = { 32, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -160, -8, 1283, { .fields = { 40, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -160, 0, 1243, { .fields = { 80, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -144, 0, 1243, { .fields = { 88, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -128, -8, 1300, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -120, 16, 1234, { .fields = { 56, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -16, 8, 1223, { .fields = { 32, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -16, 24, 1098, { .fields = { 40, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 16 } }, -64, 16, 1209, { .fields = { 16, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 16 } }, -120, 16, 1177, { .fields = { 0, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -128, 32, 1056, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 104, 8 } }, -80, 32, 657, { .fields = { 80, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 40, 56, 719, { .fields = { 48, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 80, 16 } }, -136, 40, 597, { .fields = { 0, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 88, 16 } }, -56, 40, 593, { .fields = { 112, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 88, 16 } }, -144, 56, 540, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 96, 16 } }, -56, 56, 530, { .fields = { 88, 16 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 104, 16 } }, -160, 72, 491, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 120, 16 } }, -56, 72, 488, { .fields = { 64, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 112, 16 } }, -160, 88, 447, { .fields = { 72, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 120, 16 } }, -160, 104, 411, { .fields = { 40, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 128, 16 } }, -48, 88, 447, { .fields = { 40, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 136, 16 } }, -40, 104, 409, { .fields = { 40, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -160, -64, 1156, { .fields = { 56, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -160, -56, 1031, { .fields = { 0, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -160, -48, 1122, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -160, 8, 1084, { .fields = { 64, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -160, 56, 960, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 8 } }, 40, 56, 703, { .fields = { 88, 112 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 8 } }, 40, 64, 537, { .fields = { 96, 56 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 24, 16 } }, -16, 8, 1206, { .fields = { 120, 104 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 32, 8 } }, -16, 24, 1082, { .fields = { 24, 80 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 48, 16 } }, -64, 16, 1193, { .fields = { 80, 168 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 56, 16 } }, -120, 16, 1161, { .fields = { 72, 232 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 48, 8 } }, -128, 32, 1040, { .fields = { 24, 136 } }, 128, 128, 128, 0 },
    { 141, 0x4000, { .fields = { 104, 8 } }, -80, 32, 641, { .fields = { 104, 248 } }, 128, 128, 128, 0 },
    { 141, 0x4000, { .fields = { 96, 16 } }, -56, 40, 577, { .fields = { 120, 16 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 80, 16 } }, -136, 40, 581, { .fields = { 104, 200 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 96, 16 } }, -56, 56, 514, { .fields = { 8, 200 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 120, 16 } }, -56, 72, 471, { .fields = { 72, 136 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 16 } }, 48, 56, 702, { .fields = { 112, 216 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 128, 16 } }, -48, 88, 431, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 141, 0x4000, { .fields = { 136, 16 } }, -40, 104, 392, { .fields = { 72, 168 } }, 128, 128, 128, 0 },
    { 141, 0x4000, { .fields = { 88, 16 } }, -144, 56, 524, { .fields = { 120, 48 } }, 128, 128, 128, 0 },
    { 141, 0x4000, { .fields = { 104, 16 } }, -160, 72, 475, { .fields = { 120, 64 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 112, 16 } }, -160, 88, 431, { .fields = { 0, 80 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 120, 16 } }, -160, 104, 395, { .fields = { 0, 104 } }, 128, 128, 128, 0 },
};

/// Sprite ranges for mapped junk-yard view 7, over its complete source array.
///
/// The zero-count first record keeps the decoded background; SPRITE_BATCH_END
/// terminates the list. Starts and counts are in source elements.
/// Batch 5 selects elements 45..63, hidden when the encounter event has been seen.
static SpriteBatch _gDryfieldNightJunkYardView7SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 15, 0, 0, { 0, 0 } },
    { 15, 8, 0, 0, { 3, 0 } },
    { 23, 17, 0, 0, { 2, 0 } },
    { 40, 5, 0, 0, { 4, 0 } },
    { 45, ARRAY_SIZE(_gDryfieldNightJunkYardView7SpriteSources) - 45, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Authored sprite rectangles and sorting depths for mapped junk-yard view 8.
///
/// The view's batches cover all 34 elements. Positions and sizes are pixels;
/// depth is the unscaled OT sorting value. The view descriptor borrows this
/// mutable array while the room overlay is loaded.
static SpriteSource _gDryfieldNightJunkYardView8SpriteSources[] = {
    { 142, 0x3FC0, { .fields = { 32, 32 } }, -40, -8, 3066, { .fields = { 96, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 96 } }, -40, -104, 3066, { .fields = { 48, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 24, -72, 6500, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, 64, -112, 5000, { .fields = { 96, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 72, -96, 5000, { .fields = { 0, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 88 } }, 72, -80, 5000, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 56, -80, 5000, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 72 } }, -96, -32, 1248, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 80 } }, -160, -40, 1068, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, -120, 40, 963, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -160, 40, 1038, { .fields = { 80, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 72 } }, -160, 48, 937, { .fields = { 8, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -80, 40, 1282, { .fields = { 120, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -80, 64, 1256, { .fields = { 8, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -72, 64, 1304, { .fields = { 120, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -112, 40, 1206, { .fields = { 96, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -88, 48, 1199, { .fields = { 0, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -112, 48, 1149, { .fields = { 104, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -96, 56, 1121, { .fields = { 48, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -104, 56, 1049, { .fields = { 0, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -112, 56, 1011, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 40, -24, 4346, { .fields = { 88, 16 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 32, -16, 4080, { .fields = { 80, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 16 } }, 24, -8, 3816, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, 112, 64, 1189, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 120, 64, 1172, { .fields = { 24, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 120, 72, 1150, { .fields = { 56, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 128, 72, 1160, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 64, -16, 3495, { .fields = { 32, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 72, -40, 3638, { .fields = { 16, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 80, -72, 3625, { .fields = { 120, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 128 } }, 88, -104, 3625, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 144 } }, 96, -120, 3500, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 96 } }, 96, 24, 3500, { .fields = { 64, 144 } }, 128, 128, 128, 0 },
};

/// Sprite ranges for mapped junk-yard view 8, over its complete source array.
///
/// The zero-count first record keeps the decoded background; SPRITE_BATCH_END
/// terminates the list. Starts and counts are in source elements.
static SpriteBatch _gDryfieldNightJunkYardView8SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 7, 0, 0, { 0, 0 } },
    { 7, 14, 0, 0, { 3, 0 } },
    { 21, 3, 0, 0, { 2, 0 } },
    { 24, 4, 0, 0, { 4, 0 } },
    { 28, ARRAY_SIZE(_gDryfieldNightJunkYardView8SpriteSources) - 28, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Authored sprite rectangles and sorting depths for mapped junk-yard view 9.
///
/// The view's batches cover all 25 elements. Positions and sizes are pixels;
/// depth is the unscaled OT sorting value. The view descriptor borrows this
/// mutable array while the room overlay is loaded.
static SpriteSource _gDryfieldNightJunkYardView9SpriteSources[] = {
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -80, -120, 2069, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -80, -40, 2069, { .fields = { 48, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -72, -24, 2101, { .fields = { 64, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -64, -16, 2084, { .fields = { 48, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -56, 0, 2125, { .fields = { 48, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -80, 0, 2069, { .fields = { 48, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 24, -112, 6000, { .fields = { 80, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 24, -80, 6000, { .fields = { 96, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 80, -96, 3836, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 88, -120, 3836, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -80, 16, 2042, { .fields = { 40, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 40, -56, 3550, { .fields = { 32, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 8 } }, 32, -48, 3452, { .fields = { 40, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, 24, -40, 3366, { .fields = { 0, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 24, -32, 2914, { .fields = { 72, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 40, -32, 3473, { .fields = { 40, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 64, -32, 2838, { .fields = { 64, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 40, -24, 2857, { .fields = { 48, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 80, -72, 2750, { .fields = { 104, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 80, -32, 2545, { .fields = { 72, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 136 } }, 104, -120, 2500, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 80 } }, 120, 40, 2500, { .fields = { 88, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 56 } }, 112, -16, 2500, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, -120, 2500, { .fields = { 40, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 56 } }, 112, -72, 2500, { .fields = { 72, 80 } }, 128, 128, 128, 0 },
};

/// Sprite ranges for mapped junk-yard view 9, over its complete source array.
///
/// The zero-count first record keeps the decoded background; SPRITE_BATCH_END
/// terminates the list. Starts and counts are in source elements.
static SpriteBatch _gDryfieldNightJunkYardView9SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 11, 0, 0, { 1, 0 } },
    { 11, 7, 0, 0, { 2, 0 } },
    { 18, ARRAY_SIZE(_gDryfieldNightJunkYardView9SpriteSources) - 18, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Authored sprite rectangles and sorting depths for mapped junk-yard view 10.
///
/// The view's batches cover all 97 elements. Positions and sizes are pixels;
/// depth is the unscaled OT sorting value. The view descriptor borrows this
/// mutable array while the room overlay is loaded.
static SpriteSource _gDryfieldNightJunkYardView10SpriteSources[] = {
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 48, -48, 2676, { .fields = { 104, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, 56, -48, 2513, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 64, -56, 2167, { .fields = { 64, 184 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 8 } }, 72, -56, 2248, { .fields = { 80, 160 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 56, 16 } }, 72, -48, 2192, { .fields = { 72, 136 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 72, 8 } }, 72, -32, 2202, { .fields = { 56, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, 72, -24, 2034, { .fields = { 120, 128 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 80, 8 } }, 80, -24, 2196, { .fields = { 48, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 80, -16, 1882, { .fields = { 80, 224 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 72, 32 } }, 88, -16, 1887, { .fields = { 112, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, 8, 96, 600, { .fields = { 120, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, -32, 96, 575, { .fields = { 0, 152 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 24 } }, -72, 96, 575, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 24 } }, -112, 96, 575, { .fields = { 88, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, -152, 96, 575, { .fields = { 0, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 32 } }, 112, 64, 575, { .fields = { 0, 120 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 24 } }, 112, 96, 500, { .fields = { 112, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 24 } }, 64, 96, 575, { .fields = { 0, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -64, -24, 2394, { .fields = { 120, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -72, -48, 2252, { .fields = { 56, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -80, -56, 2177, { .fields = { 72, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 80, 64 } }, -160, -56, 2200, { .fields = { 120, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -96, -120, 1825, { .fields = { 80, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 88 } }, -96, -48, 1825, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 16, -112, 8250, { .fields = { 72, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 40, -120, 4051, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 32, -80, 4051, { .fields = { 24, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, 8, -88, 7006, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 0, -40, 2314, { .fields = { 32, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 8, -40, 2762, { .fields = { 24, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 0, -32, 2398, { .fields = { 64, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 8, -32, 2533, { .fields = { 48, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 8, -24, 2312, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 8, -16, 2122, { .fields = { 88, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 8, -8, 1963, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 8, 0, 1825, { .fields = { 48, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 8, 8, 1720, { .fields = { 32, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 8, 16, 1627, { .fields = { 24, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -48, -40, 2182, { .fields = { 0, 112 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 56, 8 } }, -56, -32, 1941, { .fields = { 72, 184 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 64, 8 } }, -64, -24, 1907, { .fields = { 64, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 0, -24, 2039, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 72, 8 } }, -72, -16, 1862, { .fields = { 56, 152 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 88, 24 } }, -80, 8, 1673, { .fields = { 88, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 88, 8 } }, -80, 0, 1689, { .fields = { 0, 248 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 80, 8 } }, -72, -8, 1785, { .fields = { 80, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, 0, 1521, { .fields = { 24, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 80, 8, 1618, { .fields = { 48, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, 96, -8, 1584, { .fields = { 112, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 96, 0, 1476, { .fields = { 48, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 104, 8, 1376, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 112, 0, 1290, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 120, -16, 1266, { .fields = { 48, 240 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 24 } }, 120, -8, 1200, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 120, 16, 1203, { .fields = { 64, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 128, 16, 1200, { .fields = { 8, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 96 } }, 128, 24, 1102, { .fields = { 120, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 136, 24, 1050, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, 136, 40, 1035, { .fields = { 96, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 144, 40, 1075, { .fields = { 24, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 144, 48, 974, { .fields = { 96, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 152, 48, 931, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 112, 0, 1290, { .fields = { 72, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 88 } }, 120, -16, 1237, { .fields = { 104, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 136 } }, 128, -16, 1151, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 128 } }, 136, -8, 1067, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 128 } }, 144, -8, 990, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 128 } }, 152, -8, 959, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 64 } }, -80, -72, 2583, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -72, 72, 1256, { .fields = { 24, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 40 } }, -56, 0, 1388, { .fields = { 40, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 40 } }, -56, 40, 1314, { .fields = { 48, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -64, 16, 1379, { .fields = { 80, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -88, 0, 1267, { .fields = { 8, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -88, -48, 1372, { .fields = { 48, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -88, 24, 1275, { .fields = { 24, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -96, 40, 1253, { .fields = { 40, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -96, 0, 1290, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -96, -40, 1290, { .fields = { 8, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -96, -80, 1290, { .fields = { 0, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -96, -120, 1290, { .fields = { 16, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -120, 104, 569, { .fields = { 88, 232 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 8 } }, -120, 96, 943, { .fields = { 96, 216 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 8 } }, -120, 88, 968, { .fields = { 96, 248 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 8 } }, -120, 80, 999, { .fields = { 88, 200 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 8 } }, -120, 72, 1035, { .fields = { 88, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, -120, 40, 1075, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, -120, 0, 1075, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, -120, -40, 1075, { .fields = { 56, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, -120, -80, 1075, { .fields = { 48, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, -120, -120, 1075, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, -160, -120, 826, { .fields = { 112, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, -160, -80, 841, { .fields = { 112, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, -160, -40, 823, { .fields = { 80, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, -160, 0, 833, { .fields = { 72, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, -160, 40, 816, { .fields = { 72, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, -160, 80, 831, { .fields = { 32, 80 } }, 128, 128, 128, 0 },
};

/// Mapped view 10's ten contiguous foreground ranges, covering all 97 sources.
///
/// Indices and counts select source elements. A zero-count first record keeps
/// decoded background strips, and `SPRITE_BATCH_END` follows the live ranges.
/// Visibility remains writable; authored metadata bytes retain their unproven
/// meaning. Borrow the list and its source pool while this overlay is loaded.
static SpriteBatch _gDryfieldNightJunkYardView10SpriteBatches[] = {
    { .firstSprite = 0, .spriteCount = 0, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 0, 0 } },
    { .firstSprite = 0, .spriteCount = 10, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 6, 0 } },
    { .firstSprite = 10, .spriteCount = 8, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 1, 0 } },
    { .firstSprite = 18, .spriteCount = 4, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 8, 0 } },
    { .firstSprite = 22, .spriteCount = 5, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 0, 0 } },
    { .firstSprite = 27, .spriteCount = 1, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 5, 0 } },
    { .firstSprite = 28, .spriteCount = 18, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 2, 0 } },
    { .firstSprite = 46, .spriteCount = 16, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 9, 0 } },
    { .firstSprite = 62, .spriteCount = 6, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 4, 0 } },
    { .firstSprite = 68, .spriteCount = 1, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 7, 0 } },
    { .firstSprite = 69, .spriteCount = ARRAY_SIZE(_gDryfieldNightJunkYardView10SpriteSources) - 69, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 3, 0 } },
    { .firstSprite = SPRITE_BATCH_END, .spriteCount = 0, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 0, 0 } },
};

SpriteView gDryfieldNightJunkYardSpriteViews[10] = {
    { { .empty = _gDryfieldNightJunkYardView1SpriteBatches }, _gDryfieldNightJunkYardView1SpriteBatches, NULL },
    { { .elements = _gDryfieldNightJunkYardView2SpriteSources }, _gDryfieldNightJunkYardView2SpriteBatches, NULL },
    { { .elements = _gDryfieldNightJunkYardView3SpriteSources }, _gDryfieldNightJunkYardView3SpriteBatches, NULL },
    { { .elements = _gDryfieldNightJunkYardView4SpriteSources }, _gDryfieldNightJunkYardView4SpriteBatches, NULL },
    { { .elements = _gDryfieldNightJunkYardView5SpriteSources }, _gDryfieldNightJunkYardView5SpriteBatches, NULL },
    { { .empty = _gDryfieldNightJunkYardView6SpriteBatches }, _gDryfieldNightJunkYardView6SpriteBatches, NULL },
    { { .elements = _gDryfieldNightJunkYardView7SpriteSources }, _gDryfieldNightJunkYardView7SpriteBatches, NULL },
    { { .elements = _gDryfieldNightJunkYardView8SpriteSources }, _gDryfieldNightJunkYardView8SpriteBatches, NULL },
    { { .elements = _gDryfieldNightJunkYardView9SpriteSources }, _gDryfieldNightJunkYardView9SpriteBatches, NULL },
    { { .elements = _gDryfieldNightJunkYardView10SpriteSources }, _gDryfieldNightJunkYardView10SpriteBatches, NULL },
};

/// Ten directed view boundaries shared by junk-yard rooms 1 and 2.
///
/// Source/destination IDs address logical views 2..7; room 2 applies its view
/// map afterwards. Geometry uses room units and normals use ONE per unit.
/// Setup binds and links every mutable record through the final LAST element.
static WorldCollisionTrigger _gDryfieldNightJunkYardViewBoundaryTriggers[10] = {
    { .next = NULL, .prevLink = NULL, .coord = NULL, .origin = { 3568, -1408, 2448, 0 }, .vertices = { { 467, -2432, -3264, 0 }, { -494, -2432, 3235, 0 }, { 467, 2432, -3264, 0 }, { -494, 2432, 3235, 0 } }, .normal = { 4052, 0, 599, 0 }, .facingNormal = { 0, 0, ONE, 0 }, .radius = 4087, .control = 0, .parameter0 = 2, .parameter1 = 3, .flags = WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, .hit = 0 },
    { .next = NULL, .prevLink = NULL, .coord = NULL, .origin = { 3680, -1376, 2336, 0 }, .vertices = { { -517, -2400, 3225, 0 }, { 476, -2400, -3264, 0 }, { -517, 2400, 3225, 0 }, { 476, 2400, -3264, 0 } }, .normal = { -4056, 0, -621, 0 }, .facingNormal = { 0, 0, ONE, 0 }, .radius = 4063, .control = 0, .parameter0 = 3, .parameter1 = 2, .flags = WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, .hit = 0 },
    { .next = NULL, .prevLink = NULL, .coord = NULL, .origin = { 7324, -1328, 2299, 0 }, .vertices = { { 335, -2352, -3268, 0 }, { -337, -2352, 3267, 0 }, { 335, 2352, -3268, 0 }, { -337, 2352, 3267, 0 } }, .normal = { 4074, 0, 418, 0 }, .facingNormal = { 0, 0, ONE, 0 }, .radius = 4039, .control = 0, .parameter0 = 4, .parameter1 = 2, .flags = WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, .hit = 0 },
    { .next = NULL, .prevLink = NULL, .coord = NULL, .origin = { 7483, -1296, 2174, 0 }, .vertices = { { -337, -2320, 3267, 0 }, { 335, -2320, -3268, 0 }, { -337, 2320, 3267, 0 }, { 335, 2320, -3268, 0 } }, .normal = { -4076, 0, -420, 0 }, .facingNormal = { 0, 0, ONE, 0 }, .radius = 4015, .control = 0, .parameter0 = 2, .parameter1 = 4, .flags = WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, .hit = 0 },
    { .next = NULL, .prevLink = NULL, .coord = NULL, .origin = { 0x3EDF, -1296, 3778, 0 }, .vertices = { { -14, -2320, 2006, 0 }, { 14, -2320, -2006, 0 }, { -14, 2320, 2006, 0 }, { 14, 2320, -2006, 0 } }, .normal = { -4117, 0, -29, 0 }, .facingNormal = { 0, 0, ONE, 0 }, .radius = 3061, .control = 0, .parameter0 = 7, .parameter1 = 5, .flags = WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, .hit = 0 },
    { .next = NULL, .prevLink = NULL, .coord = NULL, .origin = { 0x3E40, -1328, 3727, 0 }, .vertices = { { 14, -2352, -1990, 0 }, { -14, -2352, 1990, 0 }, { 14, 2352, -1990, 0 }, { -14, 2352, 1990, 0 } }, .normal = { 4112, 0, 28, 0 }, .facingNormal = { 0, 0, ONE, 0 }, .radius = 3072, .control = 0, .parameter0 = 5, .parameter1 = 7, .flags = WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, .hit = 0 },
    { .next = NULL, .prevLink = NULL, .coord = NULL, .origin = { 0x3400, -1344, 928, 0 }, .vertices = { { 1262, -2352, -2822, 0 }, { -1262, -2352, 2822, 0 }, { 1262, 2352, -2822, 0 }, { -1262, 2352, 2822, 0 } }, .normal = { 3740, 0, 1672, 0 }, .facingNormal = { 0, 0, ONE, 0 }, .radius = 3882, .control = 0, .parameter0 = 7, .parameter1 = 4, .flags = WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, .hit = 0 },
    { .next = NULL, .prevLink = NULL, .coord = NULL, .origin = { 0x3430, -1312, 5328, 0 }, .vertices = { { -1330, -2352, -1782, 0 }, { 1330, -2352, 1782, 0 }, { -1330, 2352, -1782, 0 }, { 1330, 2352, 1782, 0 } }, .normal = { 3290, 0, -2457, 0 }, .facingNormal = { 0, 0, ONE, 0 }, .radius = 3228, .control = 0, .parameter0 = 7, .parameter1 = 4, .flags = WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, .hit = 0 },
    { .next = NULL, .prevLink = NULL, .coord = NULL, .origin = { 0x3531, -1344, 5361, 0 }, .vertices = { { 1310, -2352, 1818, 0 }, { -1310, -2352, -1818, 0 }, { 1310, 2352, 1818, 0 }, { -1310, 2352, -1818, 0 } }, .normal = { -3324, 0, 2394, 0 }, .facingNormal = { 0, 0, ONE, 0 }, .radius = 3248, .control = 0, .parameter0 = 4, .parameter1 = 7, .flags = WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, .hit = 0 },
    { .next = NULL, .prevLink = NULL, .coord = NULL, .origin = { 0x343F, -1345, 1280, 0 }, .vertices = { { -1090, -2352, 2426, 0 }, { 1090, -2352, -2426, 0 }, { -1090, 2352, 2426, 0 }, { 1090, 2352, -2426, 0 } }, .normal = { -3741, 0, -1681, 0 }, .facingNormal = { 0, 0, ONE, 0 }, .radius = 3547, .control = 0, .parameter0 = 4, .parameter1 = 7, .flags = WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, .hit = 0 },
};

/// Authored point lights for Dryfield's nighttime junk yard, contributing in every view.
///
/// Positions and falloff radii use integer world units; RGB intensity uses `ONE` as 1.0.
/// The room light collection borrows the full array while this overlay is loaded.
/// Entries remain writable for view parenting, transform composition and attenuation queries.
static WorldCoordPointLight _gDryfieldNightJunkYardPointLights[] = {
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -340, -1140, 4782 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { ONE, ONE, ONE },
            .unknown_56 = { 0, 0 },
        },
        .inner = 1121,
        .outer = 3124,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 9395, -1941, 3919 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { 4016, 3761, ONE },
            .unknown_56 = { 0, 0 },
        },
        .inner = 921,
        .outer = 2558,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x5931, -2202, 4970 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { ONE, ONE, ONE },
            .unknown_56 = { 0, 0 },
        },
        .inner = 1702,
        .outer = 4769,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x5336, -2180, 1052 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { ONE, 3552, 2054 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 0,
        .outer = 1807,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 3402, -1820, 321 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { 3555, 3695, ONE },
            .unknown_56 = { 0, 0 },
        },
        .inner = 1162,
        .outer = 4860,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x4423, -4059, 6797 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { 1706, 2302, 2709 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 1980,
        .outer = 0x3346,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 4863, -4059, 6797 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { 1706, 2302, 2709 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 1980,
        .outer = 0x3346,
    },
};

/// Authored light collection shared by junk-yard rooms 1 and 2.
///
/// Borrows all seven mutable point lights, with no directional or cone lights.
/// The collection and its point-light array live with the loaded room overlay.
static WorldCoordRoomLights _gDryfieldNightJunkYardRoomLights = {
    .directionalLightCount = 0,
    .directionalLights     = NULL,
    .pointLightCount       = ARRAY_SIZE(_gDryfieldNightJunkYardPointLights),
    .pointLights           = _gDryfieldNightJunkYardPointLights,
    .coneLightCount        = 0,
    .coneLights            = NULL,
};

/// Packed departure/arrival warp slots and the encounter's room-action ID.
enum {
    DRYFIELD_NIGHT_JUNK_YARD_GARAGE_WARP_SLOTS           = (1 << 4) | 2,
    DRYFIELD_NIGHT_JUNK_YARD_TRAILER_COACH_WARP_SLOTS    = (2 << 4) | 1,
    DRYFIELD_NIGHT_JUNK_YARD_ROOM_ACTION_START_ENCOUNTER = 3,
};

/// Twenty interaction and automatic-event quads shared by junk-yard rooms 1 and 2.
///
/// Includes garage/trailer-coach warps, CAP interactions and room actions 1..3;
/// action 3 starts the variant-1 encounter. Positions/radii use room units and
/// normals use ONE per unit. Setup binds and links through the final LAST
/// record; contact latches and list links remain writable while loaded.
static WorldCollisionTrigger _gDryfieldNightJunkYardActionTriggers[20] = {
    { .next = NULL, .prevLink = NULL, .coord = NULL, .origin = { 5312, -48, 240, 0 }, .vertices = { { -1024, 0, -464, 0 }, { 1024, 0, -464, 0 }, { -1024, 0, 464, 0 }, { 1024, 0, 464, 0 } }, .normal = { 0, 4098, 0, 0 }, .facingNormal = { 0, 0, ONE, 0 }, .radius = 1123, .control = WORLD_COLLISION_TRIGGER_ACTION_WARP, .parameter0 = GAME_AREA_DRYFIELD_NIGHT_GARAGE, .parameter1 = DRYFIELD_NIGHT_JUNK_YARD_GARAGE_WARP_SLOTS, .flags = WORLD_COLLISION_TRIGGER_FACING_QUAD, .hit = 0 },
    { .next = NULL, .prevLink = NULL, .coord = NULL, .origin = { 0x5600, -48, 1568, 0 }, .vertices = { { -1024, 0, -464, 0 }, { 1024, 0, -464, 0 }, { -1024, 0, 464, 0 }, { 1024, 0, 464, 0 } }, .normal = { 0, 4098, 0, 0 }, .facingNormal = { 0, 0, ONE, 0 }, .radius = 1123, .control = WORLD_COLLISION_TRIGGER_ACTION_WARP, .parameter0 = GAME_AREA_DRYFIELD_NIGHT_TRAILER_COACH, .parameter1 = DRYFIELD_NIGHT_JUNK_YARD_TRAILER_COACH_WARP_SLOTS, .flags = WORLD_COLLISION_TRIGGER_FACING_QUAD, .hit = 0 },
    { .next = NULL, .prevLink = NULL, .coord = NULL, .origin = { 7536, -64, 4704, 0 }, .vertices = { { -3216, 0, -624, 0 }, { 3216, 0, -624, 0 }, { -3216, 0, 624, 0 }, { 3216, 0, 624, 0 } }, .normal = { 0, 4097, 0, 0 }, .facingNormal = { 0, 0, -ONE, 0 }, .radius = 3268, .control = WORLD_COLLISION_TRIGGER_ACTION_CAP, .parameter0 = 11, .parameter1 = 0, .flags = WORLD_COLLISION_TRIGGER_FACING_QUAD, .hit = 0 },
    { .next = NULL, .prevLink = NULL, .coord = NULL, .origin = { 432, -64, 4576, 0 }, .vertices = { { -896, 0, -1072, 0 }, { 896, 0, -1072, 0 }, { -896, 0, 1072, 0 }, { 896, 0, 1072, 0 } }, .normal = { 0, 4101, 0, 0 }, .facingNormal = { ONE, 0, 0, 0 }, .radius = 1396, .control = WORLD_COLLISION_TRIGGER_ACTION_CAP, .parameter0 = 3, .parameter1 = 0, .flags = WORLD_COLLISION_TRIGGER_FACING_QUAD, .hit = 0 },
    { .next = NULL, .prevLink = NULL, .coord = NULL, .origin = { 6623, -64, 2783, 0 }, .vertices = { { -935, 0, 3140, 0 }, { -306, 0, -3261, 0 }, { 307, 0, 3262, 0 }, { 936, 0, -3139, 0 } }, .normal = { 0, 4097, 0, 0 }, .facingNormal = { 0, 0, -ONE, 0 }, .radius = 3268, .control = WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, .parameter0 = 1, .parameter1 = 0, .flags = WORLD_COLLISION_TRIGGER_QUAD, .hit = 0 },
    { .next = NULL, .prevLink = NULL, .coord = NULL, .origin = { 0x4ABF, -64, 3551, 0 }, .vertices = { { -609, 0, 945, 0 }, { -308, 0, -1081, 0 }, { -395, 0, 1082, 0 }, { -94, 0, -944, 0 } }, .normal = { 0, 4102, 0, 0 }, .facingNormal = { 0, 0, ONE, 0 }, .radius = 1144, .control = WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, .parameter0 = 2, .parameter1 = 0, .flags = WORLD_COLLISION_TRIGGER_QUAD, .hit = 0 },
    { .next = NULL, .prevLink = NULL, .coord = NULL, .origin = { 0x4882, -64, 3200, 0 }, .vertices = { { -1024, 0, -464, 0 }, { 1024, 0, -464, 0 }, { -1024, 0, 464, 0 }, { 1024, 0, 464, 0 } }, .normal = { 0, 4098, 0, 0 }, .facingNormal = { 0, 0, ONE, 0 }, .radius = 1123, .control = WORLD_COLLISION_TRIGGER_ACTION_CAP, .parameter0 = 8, .parameter1 = WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, .flags = WORLD_COLLISION_TRIGGER_FACING_QUAD, .hit = 0 },
    { .next = NULL, .prevLink = NULL, .coord = NULL, .origin = { 0x3120, -64, 3856, 0 }, .vertices = { { -1088, 0, -288, 0 }, { 32, 0, -4768, 0 }, { 352, 0, 2528, 0 }, { 704, 0, 2528, 0 } }, .normal = { 0, 4117, 0, 0 }, .facingNormal = { 0, 0, ONE, 0 }, .radius = 4748, .control = WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, .parameter0 = DRYFIELD_NIGHT_JUNK_YARD_ROOM_ACTION_START_ENCOUNTER, .parameter1 = 0, .flags = WORLD_COLLISION_TRIGGER_QUAD, .hit = 0 },
    { .next = NULL, .prevLink = NULL, .coord = NULL, .origin = { 1569, -64, 4160, 0 }, .vertices = { { -544, 0, -1456, 0 }, { 416, 0, -1200, 0 }, { -544, 0, 1328, 0 }, { 672, 0, 1328, 0 } }, .normal = { 0, 4110, 0, 0 }, .facingNormal = { -4052, 0, 601, 0 }, .radius = 1551, .control = WORLD_COLLISION_TRIGGER_ACTION_CAP, .parameter0 = 20, .parameter1 = 0, .flags = WORLD_COLLISION_TRIGGER_FACING_QUAD, .hit = 0 },
    { .next = NULL, .prevLink = NULL, .coord = NULL, .origin = { 0x527F, -64, 4111, 0 }, .vertices = { { -1440, 0, -448, 0 }, { 1440, 0, -448, 0 }, { -1440, 0, 448, 0 }, { 1440, 0, 448, 0 } }, .normal = { 0, 4114, 0, 0 }, .facingNormal = { 0, 0, -ONE, 0 }, .radius = 1503, .control = WORLD_COLLISION_TRIGGER_ACTION_CAP, .parameter0 = 18, .parameter1 = 0, .flags = WORLD_COLLISION_TRIGGER_FACING_QUAD, .hit = 0 },
    { .next = NULL, .prevLink = NULL, .coord = NULL, .origin = { 0x5130, -64, 3008, 0 }, .vertices = { { -848, 0, -496, 0 }, { 848, 0, -496, 0 }, { -848, 0, 496, 0 }, { 848, 0, 496, 0 } }, .normal = { 0, ONE, 0, 0 }, .facingNormal = { 201, 0, 4091, 0 }, .radius = 981, .control = WORLD_COLLISION_TRIGGER_ACTION_CAP, .parameter0 = 17, .parameter1 = 0, .flags = WORLD_COLLISION_TRIGGER_FACING_QUAD, .hit = 0 },
    { .next = NULL, .prevLink = NULL, .coord = NULL, .origin = { 320, -64, 2400, 0 }, .vertices = { { -896, 0, -1072, 0 }, { 896, 0, -1072, 0 }, { -896, 0, 1072, 0 }, { 896, 0, 1072, 0 } }, .normal = { 0, 4101, 0, 0 }, .facingNormal = { ONE, 0, 0, 0 }, .radius = 1396, .control = WORLD_COLLISION_TRIGGER_ACTION_CAP, .parameter0 = 14, .parameter1 = 0, .flags = WORLD_COLLISION_TRIGGER_FACING_QUAD, .hit = 0 },
    { .next = NULL, .prevLink = NULL, .coord = NULL, .origin = { 0x4210, -64, 4096, 0 }, .vertices = { { -2608, 0, -448, 0 }, { 2608, 0, -448, 0 }, { -2608, 0, 448, 0 }, { 2608, 0, 448, 0 } }, .normal = { 0, 4107, 0, 0 }, .facingNormal = { 0, 0, -ONE, 0 }, .radius = 2635, .control = WORLD_COLLISION_TRIGGER_ACTION_CAP, .parameter0 = 24, .parameter1 = 0, .flags = WORLD_COLLISION_TRIGGER_FACING_QUAD, .hit = 0 },
    { .next = NULL, .prevLink = NULL, .coord = NULL, .origin = { 848, -64, 544, 0 }, .vertices = { { -624, 0, -688, 0 }, { 624, 0, -688, 0 }, { -624, 0, 688, 0 }, { 624, 0, 688, 0 } }, .normal = { 0, 4099, 0, 0 }, .facingNormal = { ONE, 0, 0, 0 }, .radius = 927, .control = WORLD_COLLISION_TRIGGER_ACTION_CAP, .parameter0 = 3, .parameter1 = 0, .flags = WORLD_COLLISION_TRIGGER_FACING_QUAD, .hit = 0 },
    { .next = NULL, .prevLink = NULL, .coord = NULL, .origin = { 2720, -64, 3360, 0 }, .vertices = { { -1504, 0, -464, 0 }, { 1504, 0, -464, 0 }, { -1504, 0, 464, 0 }, { 1504, 0, 464, 0 } }, .normal = { 0, 4099, 0, 0 }, .facingNormal = { 201, 0, -4091, 0 }, .radius = 1572, .control = WORLD_COLLISION_TRIGGER_ACTION_CAP, .parameter0 = 20, .parameter1 = 0, .flags = WORLD_COLLISION_TRIGGER_FACING_QUAD, .hit = 0 },
    { .next = NULL, .prevLink = NULL, .coord = NULL, .origin = { 4159, -64, 4415, 0 }, .vertices = { { 315, 0, -1542, 0 }, { 609, 0, 1452, 0 }, { -608, 0, -1451, 0 }, { -314, 0, 1543, 0 } }, .normal = { 0, 4099, 0, 0 }, .facingNormal = { 4051, 0, -601, 0 }, .radius = 1572, .control = WORLD_COLLISION_TRIGGER_ACTION_CAP, .parameter0 = 20, .parameter1 = 0, .flags = WORLD_COLLISION_TRIGGER_FACING_QUAD, .hit = 0 },
    { .next = NULL, .prevLink = NULL, .coord = NULL, .origin = { 0x3590, -64, 1888, 0 }, .vertices = { { -1232, 0, -1984, 0 }, { -80, 0, -1984, 0 }, { 80, 0, 1984, 0 }, { 1232, 0, 1984, 0 } }, .normal = { 0, 4095, 0, 0 }, .facingNormal = { -3973, 0, 995, 0 }, .radius = 2332, .control = WORLD_COLLISION_TRIGGER_ACTION_CAP, .parameter0 = 16, .parameter1 = 0, .flags = WORLD_COLLISION_TRIGGER_FACING_QUAD, .hit = 0 },
    { .next = NULL, .prevLink = NULL, .coord = NULL, .origin = { 0x3DB0, -64, 3328, 0 }, .vertices = { { -1648, 0, -384, 0 }, { 1648, 0, -512, 0 }, { -1648, 0, 512, 0 }, { 1648, 0, 384, 0 } }, .normal = { 0, 4110, 0, 0 }, .facingNormal = { 201, 0, 4091, 0 }, .radius = 1722, .control = WORLD_COLLISION_TRIGGER_ACTION_CAP, .parameter0 = 16, .parameter1 = 0, .flags = WORLD_COLLISION_TRIGGER_FACING_QUAD, .hit = 0 },
    { .next = NULL, .prevLink = NULL, .coord = NULL, .origin = { 0x35BF, -64, 4895, 0 }, .vertices = { { -957, 0, 887, 0 }, { 189, 0, -1347, 0 }, { -188, 0, 1348, 0 }, { 958, 0, -886, 0 } }, .normal = { 0, 4104, 0, 0 }, .facingNormal = { -3920, 0, -1189, 0 }, .radius = 1360, .control = WORLD_COLLISION_TRIGGER_ACTION_CAP, .parameter0 = 24, .parameter1 = 0, .flags = WORLD_COLLISION_TRIGGER_FACING_QUAD, .hit = 0 },
    { .next = NULL, .prevLink = NULL, .coord = NULL, .origin = { 0x52C0, -64, 2144, 0 }, .vertices = { { -368, 0, -976, 0 }, { 624, 0, -976, 0 }, { -624, 0, 976, 0 }, { 368, 0, 976, 0 } }, .normal = { 0, 4100, 0, 0 }, .facingNormal = { 4051, 0, -601, 0 }, .radius = 1152, .control = WORLD_COLLISION_TRIGGER_ACTION_CAP, .parameter0 = 17, .parameter1 = 0, .flags = WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, .hit = 0 },
};

/// One line-of-sight blocking quad shared by junk-yard rooms 1 and 2.
///
/// Geometry uses room units with an X-axis normal scaled by ONE. LAST includes
/// this sole mutable record in setup; flag bit 0 is retained with unproven role.
/// Keep storage loaded until the runtime list has unlinked it.
static WorldCollisionOccluder _gDryfieldNightJunkYardSightOccluders[1] = {
    { .next = NULL, .prevLink = NULL, .origin = { 0x4980, -544, 1536, 0 }, .vertices = { { 0, 1024, -1024, 0 }, { 0, -1024, -1024, 0 }, { 0, 1024, 1024, 0 }, { 0, -1024, 1024, 0 } }, .normal = { ONE, 0, 0, 0 }, .radius = 1448, .flags = 1 | WORLD_COLLISION_OCCLUDER_LAST, .unknown_3B = 0 },
};

/// Placement-entry keys used by this room's resource recipes.
enum {
    DRYFIELD_NIGHT_JUNK_YARD_ZEBRA_STALKER_RESOURCE_ENTRY = 6,
};

/// Zebra Stalker recipe for night junk-yard variant 1.
///
/// Two placements use entry 6, base-group-40 file 6 and descriptor 0.
/// The final record is `AREA_PLACEMENT_END`. Keep this room, its map-owned
/// placements and the selected actor overlays loaded during loading, spawning
/// and model-flag updates.
static AreaResource _gDryfieldNightJunkYardVariant1Resources[2] = {
    { .entryId = DRYFIELD_NIGHT_JUNK_YARD_ZEBRA_STALKER_RESOURCE_ENTRY, .fileNumber = 6, .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_40, .taskIndex = 0, .taskTable = &gActor400600ZebraStalkerTaskDesc },
    { .entryId = AREA_PLACEMENT_END },
};

/// Placement-entry keys used by variants 2, 7 and 21.
enum {
    DRYFIELD_NIGHT_JUNK_YARD_DESERT_CHASER_RESOURCE_ENTRY = 1,
    DRYFIELD_NIGHT_JUNK_YARD_MIND_SUCKLER_RESOURCE_ENTRY  = 15,
    DRYFIELD_NIGHT_JUNK_YARD_PAWN_GOLEM_RESOURCE_ENTRY    = 56,
    DRYFIELD_NIGHT_JUNK_YARD_ROOK_GOLEM_RESOURCE_ENTRY    = 23,
};

/// Desert Chaser and Mind Suckler recipe for night Junk Yard variant 2.
///
/// Placement keys 1/15 select base files 400100/301500 and descriptor 0.
/// The third record ends the table. This room owns the recipe, the map owns
/// its placements and actor overlays own the borrowed descriptors; keep all
/// loaded during loading, spawning and model-flag updates.
static AreaResource _gDryfieldNightJunkYardVariant2Resources[3] = {
    { .entryId = DRYFIELD_NIGHT_JUNK_YARD_DESERT_CHASER_RESOURCE_ENTRY, .fileNumber = 1, .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_40, .taskIndex = 0, .taskTable = &gActor00100DesertChaserTaskDesc },
    { .entryId = DRYFIELD_NIGHT_JUNK_YARD_MIND_SUCKLER_RESOURCE_ENTRY, .fileNumber = 15, .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_30, .taskIndex = 0, .taskTable = &gActor01500MindSucklerTaskDesc },
    { .entryId = AREA_PLACEMENT_END },
};

/// Zebra Stalker recipe for night Junk Yard variant 4.
///
/// Placement key 6 selects base file 400600 and descriptor 0, followed by the
/// end marker. Borrowed map placements and actor descriptors must remain
/// loaded with this recipe during loading, spawning and model-flag updates.
static AreaResource _gDryfieldNightJunkYardVariant4Resources[2] = {
    { .entryId = DRYFIELD_NIGHT_JUNK_YARD_ZEBRA_STALKER_RESOURCE_ENTRY, .fileNumber = 6, .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_40, .taskIndex = 0, .taskTable = &gActor400600ZebraStalkerTaskDesc },
    { .entryId = AREA_PLACEMENT_END },
};

/// Pawn and Beam Sword Rook GOLEM recipe for night Junk Yard variant 7.
///
/// Placement keys 56/23 select base files 105600/202300 and descriptor 0 in
/// each actor table. The third record ends the recipe. Keep room/map storage
/// and both borrowed actor tables loaded through loading, spawning and
/// model-flag updates.
static AreaResource _gDryfieldNightJunkYardVariant7Resources[3] = {
    { .entryId = DRYFIELD_NIGHT_JUNK_YARD_PAWN_GOLEM_RESOURCE_ENTRY, .fileNumber = 56, .fileGroupIndex = LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, .taskIndex = 0, .taskTable = gActor105600PawnGolemTasks },
    { .entryId = DRYFIELD_NIGHT_JUNK_YARD_ROOK_GOLEM_RESOURCE_ENTRY, .fileNumber = 23, .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_20, .taskIndex = 0, .taskTable = gActor202300RookGolemTasks },
    { .entryId = AREA_PLACEMENT_END },
};

/// Zebra Stalker and Mind Suckler recipe for night Junk Yard variant 21.
///
/// Placement keys 6/15 select base files 400600/301500 and descriptor 0.
/// The third record ends the recipe. Keep room/map storage and borrowed actor
/// descriptors loaded during loading, spawning and model-flag updates.
static AreaResource _gDryfieldNightJunkYardVariant21Resources[3] = {
    { .entryId = DRYFIELD_NIGHT_JUNK_YARD_ZEBRA_STALKER_RESOURCE_ENTRY, .fileNumber = 6, .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_40, .taskIndex = 0, .taskTable = &gActor400600ZebraStalkerTaskDesc },
    { .entryId = DRYFIELD_NIGHT_JUNK_YARD_MIND_SUCKLER_RESOURCE_ENTRY, .fileNumber = 15, .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_30, .taskIndex = 0, .taskTable = &gActor01500MindSucklerTaskDesc },
    { .entryId = AREA_PLACEMENT_END },
};

AreaVariant gDryfieldNightJunkYardAreaVariants[22] = {
    [1]  = { .placements = gMapDryfieldFullJunkYardVariant1Placements, .resources = _gDryfieldNightJunkYardVariant1Resources },
    [2]  = { .placements = gMapDryfieldFullJunkYardVariant2Placements, .resources = _gDryfieldNightJunkYardVariant2Resources },
    [4]  = { .placements = gMapDryfieldFullJunkYardVariant4Placements, .resources = _gDryfieldNightJunkYardVariant4Resources },
    [7]  = { .placements = gMapDryfieldFullJunkYardVariant7Placements, .resources = _gDryfieldNightJunkYardVariant7Resources },
    [21] = { .placements = gMapDryfieldFullJunkYardVariant21Placements, .resources = _gDryfieldNightJunkYardVariant21Resources },
};

/// Type-1 sound-script cue-2 bases in the currently loaded sound bank.
enum {
    DRYFIELD_NIGHT_JUNK_YARD_SURFACE_1_WALK_SOUND = 0x10000049,
    DRYFIELD_NIGHT_JUNK_YARD_SURFACE_1_RUN_SOUND  = 0x1000004B
};

/// Surface class 1's footstep cues; scripted jumps reuse the walking pair.
static WorldCollisionFootstepSounds _gDryfieldNightJunkYardSurface1FootstepSounds = {
    .walk         = DRYFIELD_NIGHT_JUNK_YARD_SURFACE_1_WALK_SOUND,
    .run          = DRYFIELD_NIGHT_JUNK_YARD_SURFACE_1_RUN_SOUND,
    .scriptedJump = DRYFIELD_NIGHT_JUNK_YARD_SURFACE_1_WALK_SOUND,
};

/// Blocking, impact-enabled policy without footstep cues for classes 0 and 3..7.
static WorldCollisionSurfaceProperties _gDryfieldNightJunkYardSilentSurfaceProperties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

/// Surface class 1 blocks probes, allows weapon impacts and supplies footstep cues.
static WorldCollisionSurfaceProperties _gDryfieldNightJunkYardSurface1Properties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gDryfieldNightJunkYardSurface1FootstepSounds,
};

/// Surface class 2 passes probes and ignores weapon impacts while applying pushback.
static WorldCollisionSurfaceProperties _gDryfieldNightJunkYardSurface2Properties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_PASS_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

WorldCollisionSurfaceProperties* gDryfieldNightJunkYardSurfacePropertiesByClass[8] = {
    &_gDryfieldNightJunkYardSilentSurfaceProperties,
    &_gDryfieldNightJunkYardSurface1Properties,
    &_gDryfieldNightJunkYardSurface2Properties,
    &_gDryfieldNightJunkYardSilentSurfaceProperties,
    &_gDryfieldNightJunkYardSilentSurfaceProperties,
    &_gDryfieldNightJunkYardSilentSurfaceProperties,
    &_gDryfieldNightJunkYardSilentSurfaceProperties,
    &_gDryfieldNightJunkYardSilentSurfaceProperties,
};

/// Draws the grey capsule and two large flares shared by junk yard views 5, 7 and 10.
///
/// Requires four consecutive, word-aligned world-space points, borrowed for
/// this call: `worldPoints[0..1]` are capsule endpoints and `worldPoints[2..3]`
/// are flare centres. The capsule starts at screen angle zero (down); both
/// flares use texture column zero and the room's large perspective radius.
/// Uses the current view, initialized scratch stack, ordering table and packet
/// arena; queued primitives remain in that frame's arena until GPU completion.
static inline void _dryfieldNightJunkYardDrawCapsuleAndFlares(const SVECTOR worldPoints[4])
{
    enum {
        DRYFIELD_NIGHT_JUNK_YARD_CAPSULE_DOWN_ANGLE   = 0, // 4096 units per screen-space turn; zero points down
        DRYFIELD_NIGHT_JUNK_YARD_FLARE_TEXTURE_COLUMN = 0
    };

    _glowDrawGreyCapsule(&worldPoints[0], DRYFIELD_NIGHT_JUNK_YARD_CAPSULE_RADIUS_SCALE, DRYFIELD_NIGHT_JUNK_YARD_CAPSULE_DOWN_ANGLE);
    glowDrawFlareClipped(&worldPoints[2], DRYFIELD_NIGHT_JUNK_YARD_FLARE_TEXTURE_COLUMN, DRYFIELD_NIGHT_JUNK_YARD_LARGE_FLARE_RADIUS_SCALE);
    glowDrawFlareClipped(&worldPoints[3], DRYFIELD_NIGHT_JUNK_YARD_FLARE_TEXTURE_COLUMN, DRYFIELD_NIGHT_JUNK_YARD_LARGE_FLARE_RADIUS_SCALE);
}

void dryfieldNightJunkYardDrawGlowsTask(Task* task)
{
    enum { DRYFIELD_NIGHT_JUNK_YARD_EFFECTS_INITIALIZE = 0 };

    if (task->state == DRYFIELD_NIGHT_JUNK_YARD_EFFECTS_INITIALIZE) {
        gRoomEffectFlashId      = EFFECT_DRYFIELD_NIGHT_JUNK_YARD_FLASH;
        gRoomEffectTwinTrailId  = EFFECT_DRYFIELD_NIGHT_JUNK_YARD_TWIN_TRAIL;
        gRoomEffectSparkBurstId = EFFECT_DRYFIELD_NIGHT_JUNK_YARD_SPARK_BURST;
    }
    gRoomEffectState->roomEffectMode = ROOM_EFFECT_VIEW_ENABLED;
    // The grey capsule uses the first two points; flares depend on the active view.
    switch (gGameSession->location.view) {
        case 2:
        case 8: {
            const SVECTOR* lightPoints = _gDryfieldNightJunkYardGlowWorldPoints;
            _glowDrawGreyCapsule(&lightPoints[0], DRYFIELD_NIGHT_JUNK_YARD_CAPSULE_RADIUS_SCALE, GLOW_QUARTER_TURN);
            glowDrawFlareClipped(&lightPoints[3], 0, DRYFIELD_NIGHT_JUNK_YARD_LARGE_FLARE_RADIUS_SCALE);
            glowDrawFlareClipped(&lightPoints[4], 0, DRYFIELD_NIGHT_JUNK_YARD_LARGE_FLARE_RADIUS_SCALE);
            glowDrawFlareClipped(&lightPoints[5], 2, DRYFIELD_NIGHT_JUNK_YARD_LARGE_FLARE_RADIUS_SCALE);
            glowDrawFlareClipped(&lightPoints[6], 1, DRYFIELD_NIGHT_JUNK_YARD_SMALL_FLARE_RADIUS_SCALE);
            break;
        }
        case 4:
        case 9: {
            const SVECTOR* flarePoints = &_gDryfieldNightJunkYardGlowWorldPoints[3];
            glowDrawFlareClipped(&flarePoints[0], 0, DRYFIELD_NIGHT_JUNK_YARD_LARGE_FLARE_RADIUS_SCALE);
            glowDrawFlareClipped(&flarePoints[1], 0, DRYFIELD_NIGHT_JUNK_YARD_LARGE_FLARE_RADIUS_SCALE);
            glowDrawFlareClipped(&flarePoints[3], 1, DRYFIELD_NIGHT_JUNK_YARD_SMALL_FLARE_RADIUS_SCALE);
            break;
        }
        case 5:
        case 10: {
            const SVECTOR* lightPoints = _gDryfieldNightJunkYardGlowWorldPoints;
            _dryfieldNightJunkYardDrawCapsuleAndFlares(lightPoints);
            glowDrawFlareClipped(&lightPoints[6], 1, DRYFIELD_NIGHT_JUNK_YARD_SMALL_FLARE_RADIUS_SCALE);
            break;
        }
        case 7: {
            const SVECTOR* lightPoints = _gDryfieldNightJunkYardGlowWorldPoints;
            _dryfieldNightJunkYardDrawCapsuleAndFlares(lightPoints);
            break;
        }
    }
}

#include "../../shared/effects/glow_draw_grey_capsule.inc.c"

#include "../../shared/effects/glow_draw_flare_clipped.inc.c"

#include "../../shared/room/room_visual_effects.inc.c"

#include "../../shared/room/room_visual_effects_flash_task.inc.c"

void dryfieldNightJunkYardRoomVisualEffectsFlashTask(Task* task)
{
    _roomVisualEffectsFlashTask(task);
}

#include "../../shared/room/room_visual_effects_trails.inc.c"

void dryfieldNightJunkYardRoomVisualEffectsTwinTrailTask(Task* task)
{
#include "../../shared/room/room_visual_effects_trail_task.inc.c"

    void dryfieldNightJunkYardRoomVisualEffectsSparkBurstTask(Task * task)
    {
        _roomVisualEffectsSparkBurstTask(task);
    }

#include "../../shared/room/room_visual_effects_glow.inc.c"
