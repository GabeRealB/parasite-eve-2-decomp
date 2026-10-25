#include "rooms/shelter_b1_elevator_hall.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "shelter_b1_elevator_hall_private.h"

#include "actors/actor_00300.h"
#include "actors/actor_02100.h"
#include "actors/actor_02300.h"
#include "actors/actor_05600.h"
#include "actors/actor_400600.h"

#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/area.h"
#include "gameplay/areaplace.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag_ids.h"
#include "main/gamemain.h"
#include "main/gfx.h"
#include "main/gfx_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/sound_ids.h"
#include "main/sound_types.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "overlay.h"

#include "rooms/room.h"

#include "rooms/room_common.h"

#include "../../shared/room/room_visual_effects.h"
#include "../../shared/effects/glow_draw.h"
#include "../../shared/room/shelter_elevator.h"

/// Per-index shifts applied to the flash brightness, one per colour channel.

// Indexed views below share one contiguous table.
static WorldCollisionGrid     _gShelterB1ElevatorHallCollisionGrid;
static WorldCollisionOccluder _gShelterB1ElevatorHallOccluders[1];
static WorldCollisionTrigger  _gShelterB1ElevatorHallViewBoundaryTriggers[10];
static WorldCollisionTrigger  _gShelterB1ElevatorHallActionTriggers[6];
static WorldCoordRoomLights   _gShelterB1ElevatorHallRoomLights;

enum {
    SHELTER_B1_ELEVATOR_HALL_GRID_CELLS_X      = 7,
    SHELTER_B1_ELEVATOR_HALL_GRID_CELLS_Z      = 2,
    SHELTER_B1_ELEVATOR_HALL_GRID_CELL_SIZE    = 4000,
    SHELTER_B1_ELEVATOR_HALL_SURFACE1_WALK_CUE = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x59,
    SHELTER_B1_ELEVATOR_HALL_SURFACE1_RUN_CUE  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x5B,
    SHELTER_B1_ELEVATOR_HALL_SURFACE2_WALK_CUE = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x5D,
    SHELTER_B1_ELEVATOR_HALL_SURFACE2_RUN_CUE  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x5F,
};

/// One-based endpoint IDs used by the warp table and the action triggers.
enum {
    SHELTER_B1_ELEVATOR_HALL_WARP_MINE_PASSAGE  = 1,
    SHELTER_B1_ELEVATOR_HALL_WARP_MAIN_CORRIDOR = 2,
    SHELTER_B1_ELEVATOR_HALL_WARP_ELEVATOR      = 3,
    SHELTER_B1_ELEVATOR_HALL_WARP_SOUTH_WALKWAY = 4,
};

/// Destination endpoint IDs and the command shared by the two CAP hotspots.
enum {
    SHELTER_B1_ELEVATOR_HALL_DESTINATION_FIRST_WARP = 1,
    SHELTER_B1_ELEVATOR_HALL_MINE_PASSAGE_ARRIVAL   = 2,
    SHELTER_B1_ELEVATOR_HALL_HOTSPOT_CAP_COMMAND    = 6,
};

/// Sound requests selected by each endpoint's arrival, departure or refusal.
enum {
    SHELTER_B1_ELEVATOR_HALL_CORRIDOR_DEPARTURE_SOUND = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_ELEVATOR_HALL, 1),
    SHELTER_B1_ELEVATOR_HALL_CORRIDOR_ARRIVAL_SOUND   = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_ELEVATOR_HALL, 2),
    SHELTER_B1_ELEVATOR_HALL_CORRIDOR_BLOCKED_SOUND   = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_ELEVATOR_HALL, 3),
    SHELTER_B1_ELEVATOR_HALL_WALKWAY_DEPARTURE_SOUND  = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_ELEVATOR_HALL, 4),
    SHELTER_B1_ELEVATOR_HALL_WALKWAY_ARRIVAL_SOUND    = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_ELEVATOR_HALL, 5),
    SHELTER_B1_ELEVATOR_HALL_MINE_ARRIVAL_SOUND       = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_ELEVATOR_HALL, 6),
    SHELTER_B1_ELEVATOR_HALL_ELEVATOR_ARRIVAL_SOUND   = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_ELEVATOR_HALL, 9),
};

/// Keys matching placement rows to their file and body-task resource entries.
enum {
    SHELTER_B1_ELEVATOR_HALL_RESOURCE_BRAIN_STINGER = 3,
    SHELTER_B1_ELEVATOR_HALL_RESOURCE_ZEBRA_STALKER = 6,
    SHELTER_B1_ELEVATOR_HALL_RESOURCE_WATCHER       = 21,
    SHELTER_B1_ELEVATOR_HALL_RESOURCE_ROOK_GOLEM    = 23,
    SHELTER_B1_ELEVATOR_HALL_RESOURCE_PAWN_GOLEM    = 56,
};

/// Populated indices in the saved placement-variant directory.
enum {
    SHELTER_B1_ELEVATOR_HALL_VARIANT_ZEBRA_STALKERS = 2,
    SHELTER_B1_ELEVATOR_HALL_VARIANT_BRAIN_STINGERS = 3,
    SHELTER_B1_ELEVATOR_HALL_VARIANT_WATCHERS       = 4,
    SHELTER_B1_ELEVATOR_HALL_VARIANT_GOLEMS         = 11,
};

TaskDesc gShelterB1ElevatorHallElevatorTaskDesc = {
    .header   = { .fields = { TASK_BODY_NONE, TASK_PRIORITY_SCENE } },
    .callback = shelterElevatorTask,
    .data     = { .value = 0 },
};

TaskMessageEntry gShelterB1ElevatorHallRoomMessages[6] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, shelterB1ElevatorHallResolveRoomTransition },
    { ROOM_MESSAGE_USE_KEY_ITEM, shelterB1ElevatorHallRejectKeyItemUse },
    { DIRECTION_MESSAGE_ROOM_ACTION, shelterB1ElevatorHallIgnoreActionMessage },
    { ROOM_MESSAGE_COMMAND, shelterB1ElevatorHallIgnoreCommandMessage },
    { ROOM_MESSAGE_SOUND, shelterB1ElevatorHallPlaySoundCueMessage },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc gShelterB1ElevatorHallMineTransitTaskDesc = {
    .header   = { .fields = { TASK_BODY_NONE, TASK_PRIORITY_SCENE } },
    .callback = shelterB1ElevatorHallMineTransitTask,
    .data     = { .value = 0 },
};

/// World-space endpoint pairs for the hall's additive capsule glows.
///
/// Each row supplies two signed game-coordinate positions to one capsule.
/// View selection borrows subsets while this room overlay remains loaded.
static SVECTOR _gShelterB1ElevatorHallGlowEndpointPairs[14][2] = {
    { { -8833, -620, -1755, 0 }, { -8166, -620, -1755, 0 } },
    { { -4828, -620, -1755, 0 }, { -4165, -620, -1755, 0 } },
    { { -828, -620, -1755, 0 }, { -165, -620, -1755, 0 } },
    { { 2165, -620, -1755, 0 }, { 2833, -620, -1755, 0 } },
    { { 6165, -620, -1755, 0 }, { 6833, -620, -1755, 0 } },
    { { 9165, -620, -1755, 0 }, { 9833, -620, -1755, 0 } },
    { { -828, -620, 1759, 0 }, { -165, -620, 1759, 0 } },
    { { 2165, -620, 1759, 0 }, { 2833, -620, 1759, 0 } },
    { { 4726, -620, 2658, 0 }, { 4726, -620, 3269, 0 } },
    { { 10749, -620, 2564, 0 }, { 10749, -620, 3176, 0 } },
    { { -4125, -2399, 1652, 0 }, { -3753, -2399, 1652, 0 } },
    { { 10537, -2376, -252, 0 }, { 10537, -2376, -734, 0 } },
    { { 10575, -2526, 3841, 0 }, { 10575, -2526, 4301, 0 } },
    { { -10963, -998, 1157, 0 }, { -10695, -998, 1157, 0 } },
};

/// Initializes this overlay's three halo tint rows and retained trailing halfword.
///
/// Rows 0..2 give RGB right shifts of brightness (0 full, 1 half, 2 quarter).
/// Requires `ROOM_VISUAL_EFFECTS_HALO_STORAGE_TYPE` = `RoomFxHaloStorage` and an empty
/// `ROOM_VISUAL_EFFECTS_HALO_STORAGE_BOUND`. The trailing u16 is preserved
/// verbatim; its role is unproven and it is not a fourth tint row.
/// Bind around `room_visual_effects_halo_data.inc.c`, then undefine.
/// Expands to a constant braced initializer, with no arguments or side effects.
#define ROOM_VISUAL_EFFECTS_HALO_STORAGE_INITIALIZER { \
    .entries = {                                       \
        { .rShift = 0, .gShift = 1, .bShift = 2 },     \
        { .rShift = 2, .gShift = 1, .bShift = 0 },     \
        { .rShift = 0, .gShift = 2, .bShift = 1 },     \
    },                                                 \
    .alignmentFill = 0x90EE,                           \
}
/// Selects the halo tint storage record, including its retained trailing halfword.
///
/// Expands to the `RoomFxHaloStorage` type token; requires an empty
/// `ROOM_VISUAL_EFFECTS_HALO_STORAGE_BOUND` and a
/// `ROOM_VISUAL_EFFECTS_HALO_STORAGE_INITIALIZER` for `entries` and `alignmentFill`.
/// Only the three RGB-shift rows are read, at indices 0..2; the halfword's
/// role is unproven. Include `room_visual_effects.h` first, bind around
/// `room_visual_effects_halo_data.inc.c`, then undefine.
#define ROOM_VISUAL_EFFECTS_HALO_STORAGE_TYPE RoomFxHaloStorage
/// Declares one halo storage record, retaining the halfword after its tint rows.
///
/// This empty suffix requires `ROOM_VISUAL_EFFECTS_HALO_STORAGE_TYPE` to be
/// `RoomFxHaloStorage`. Bind around `room_visual_effects_halo_data.inc.c`.
#define ROOM_VISUAL_EFFECTS_HALO_STORAGE_BOUND
#include "../../shared/room/room_visual_effects_halo_data.inc.c"

static void _glowDrawCapsule(const SVECTOR worldPoints[2], s32 radiusScale, s32 packedColor);

/// Returns this overlay's three read-only halo tint rows for spawn indices 0..2.
static inline const RoomFxShade* _roomVisualEffectsGetHaloShades(void)
{
    return _gRoomVisualEffectsHaloShades.entries;
}
#undef ROOM_VISUAL_EFFECTS_HALO_STORAGE_INITIALIZER
#undef ROOM_VISUAL_EFFECTS_HALO_STORAGE_TYPE
#undef ROOM_VISUAL_EFFECTS_HALO_STORAGE_BOUND

#include "../../shared/room/room_visual_effects_trail_data.inc.c"

WorldCoordRoomLighting gShelterB1ElevatorHallRoomLighting[1] = {
    { &_gShelterB1ElevatorHallRoomLights, NULL },
};

WorldCollisionRoomResources gShelterB1ElevatorHallRoomCollisionResources[1] = {
    { .grid                 = &_gShelterB1ElevatorHallCollisionGrid,
      .viewBoundaryTriggers = _gShelterB1ElevatorHallViewBoundaryTriggers,
      .actionTriggers       = _gShelterB1ElevatorHallActionTriggers,
      .occluders            = _gShelterB1ElevatorHallOccluders },
};

u8* gShelterB1ElevatorHallRoomViewMaps[1] = {
    gViewIdentityMap,
};

ViewCount gShelterB1ElevatorHallRoomViewCounts[1] = { ARRAY_SIZE(gShelterB1ElevatorHallViewCameras) };

DirectionWarpEntry gShelterB1ElevatorHallWarpEntries[4] = {
    [SHELTER_B1_ELEVATOR_HALL_WARP_MINE_PASSAGE - 1] = {
        .player         = { { .word = ACTOR_TRANSFORM_ANGLE_TURN / 4 }, -10700, 0, 520 },
        .companion      = { { .word = ACTOR_TRANSFORM_ANGLE_TURN / 4 }, -10700, 0, 520 },
        .arrivalSound   = SHELTER_B1_ELEVATOR_HALL_MINE_ARRIVAL_SOUND,
        .departureSound = SOUND_SHELTER_B1_ELEV_HALL_MINE_TRANSIT,
        .blockedSound   = DIRECTION_WARP_SOUND_NONE,
        .initialView    = 2,
        .flags          = DIRECTION_WARP_FLAG_FADE_DEPARTURE,
        .mapFlagId      = DIRECTION_WARP_MAP_FLAG_NONE,
    },
    [SHELTER_B1_ELEVATOR_HALL_WARP_MAIN_CORRIDOR - 1] = {
        .player         = { { .word = ACTOR_TRANSFORM_ANGLE_HALF_TURN }, -3864, 0, 1073 },
        .companion      = { { .word = ACTOR_TRANSFORM_ANGLE_HALF_TURN }, -3864, 0, 1073 },
        .arrivalSound   = SHELTER_B1_ELEVATOR_HALL_CORRIDOR_ARRIVAL_SOUND,
        .departureSound = SHELTER_B1_ELEVATOR_HALL_CORRIDOR_DEPARTURE_SOUND,
        .blockedSound   = SHELTER_B1_ELEVATOR_HALL_CORRIDOR_BLOCKED_SOUND,
        .initialView    = 3,
        .flags          = DIRECTION_WARP_FLAG_NONE,
        .mapFlagId      = GAME_FLAG_MAP_MARK_SHELTER_1CD,
    },
    [SHELTER_B1_ELEVATOR_HALL_WARP_ELEVATOR - 1] = {
        .player         = { { .word = ACTOR_TRANSFORM_ANGLE_TURN * 3 / 4 }, 9950, 0, -290 },
        .companion      = { { .word = ACTOR_TRANSFORM_ANGLE_TURN * 3 / 4 }, 9950, 0, -290 },
        .arrivalSound   = SHELTER_B1_ELEVATOR_HALL_ELEVATOR_ARRIVAL_SOUND,
        .departureSound = SOUND_SHELTER_B1_ELEVATOR_RIDE,
        .blockedSound   = DIRECTION_WARP_SOUND_NONE,
        .initialView    = 8,
        .flags          = DIRECTION_WARP_FLAG_NONE,
        .mapFlagId      = GAME_FLAG_MAP_MARK_SHELTER_1BB,
    },
    [SHELTER_B1_ELEVATOR_HALL_WARP_SOUTH_WALKWAY - 1] = {
        .player         = { { .word = ACTOR_TRANSFORM_ANGLE_TURN * 3 / 4 }, 9950, 0, 3740 },
        .companion      = { { .word = ACTOR_TRANSFORM_ANGLE_TURN * 3 / 4 }, 9950, 0, 3740 },
        .arrivalSound   = SHELTER_B1_ELEVATOR_HALL_WALKWAY_ARRIVAL_SOUND,
        .departureSound = SHELTER_B1_ELEVATOR_HALL_WALKWAY_DEPARTURE_SOUND,
        .blockedSound   = DIRECTION_WARP_SOUND_NONE,
        .initialView    = 7,
        .flags          = DIRECTION_WARP_FLAG_NONE,
        .mapFlagId      = DIRECTION_WARP_MAP_FLAG_NONE,
    },
};

/// Room-space collision normals, with 4096 units per unit length.
static SVECTOR _gShelterB1ElevatorHallCollisionGridNormals[8] = {
#include "assets/shelter_b1_elevator_hall_collision_05E54_normals.inc"
};

/// Collision vertex pool in signed room-coordinate units.
static SVECTOR _gShelterB1ElevatorHallCollisionGridVerts[58] = {
#include "assets/shelter_b1_elevator_hall_collision_05E54_verts.inc"
};

/// Indexed collision quads carrying the hall's surface classes 0..2.
static WorldCollisionGridFace _gShelterB1ElevatorHallCollisionGridFaces[32] = {
#include "assets/shelter_b1_elevator_hall_collision_05E54_faces.inc"
};

/// Packed cell face-index lists, each ending in WORLD_COLLISION_GRID_CELL_END.
static s16 _gShelterB1ElevatorHallCollisionGridCells[170] = {
#include "assets/shelter_b1_elevator_hall_collision_05E54_cells.inc"
};

/// Supplies this grid's cell face-list addresses to its generated table.
///
/// Captures `_gShelterB1ElevatorHallCollisionGridCells`.
/// `halfwordOffset` is a constant s16-element offset in 0..169,
/// evaluated once and selecting a list's first entry. Lists end at
/// `WORLD_COLLISION_GRID_CELL_END`. Returns a borrowed `s16*`; no pool writes.
/// `cell_face_list_macro` selects this binding for the table include.
/// The binding is undefined immediately after that include.
#define SHELTER_B1_ELEVATOR_HALL_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gShelterB1ElevatorHallCollisionGridCells[(halfwordOffset)])
/// X-major directory of the collision grid's 7 by 2 cell lists.
static s16* _gShelterB1ElevatorHallCollisionGridTable[SHELTER_B1_ELEVATOR_HALL_GRID_CELLS_X * SHELTER_B1_ELEVATOR_HALL_GRID_CELLS_Z] = {
#include "assets/shelter_b1_elevator_hall_collision_05E54_table.inc"
};
#undef SHELTER_B1_ELEVATOR_HALL_CELL_FACE_LIST_AT_OFFSET

/// The hall's collision mesh and square-cell index in room coordinates.
///
/// Activation borrows the current view coordinate; all mesh storage remains
/// in this overlay until the active grid is replaced before unloading.
static WorldCollisionGrid _gShelterB1ElevatorHallCollisionGrid = {
    .viewCoord   = NULL,
    .normals     = _gShelterB1ElevatorHallCollisionGridNormals,
    .vertices    = _gShelterB1ElevatorHallCollisionGridVerts,
    .faces       = _gShelterB1ElevatorHallCollisionGridFaces,
    .cellFaceIds = _gShelterB1ElevatorHallCollisionGridTable,
    .xBias       = 13222,
    .zBias       = 1641,
    .cellCountX  = SHELTER_B1_ELEVATOR_HALL_GRID_CELLS_X,
    .cellCountZ  = SHELTER_B1_ELEVATOR_HALL_GRID_CELLS_Z,
    .cellSize    = SHELTER_B1_ELEVATOR_HALL_GRID_CELL_SIZE,
    .faceCount   = ARRAY_SIZE(_gShelterB1ElevatorHallCollisionGridFaces),
};

ViewCamera gShelterB1ElevatorHallViewCameras[9] = {
    { { { { ONE, 0, 0 }, { 0, 0, -ONE }, { 0, ONE, 0 } }, { 20, 0x7530, -1740 } }, 329 },
    { { { { 1125, 0, 3938 }, { 1740, 3674, -497 }, { -3532, 1810, 1009 } }, { 5854, 2660, 842 } }, 230 },
    { { { { 1121, 0, 3939 }, { 444, 4069, -126 }, { -3914, 462, 1114 } }, { 561, 1452, 741 } }, 230 },
    { { { { 1056, 0, 3957 }, { 339, 4080, -90 }, { -3942, 350, 1052 } }, { -2895, 1369, 969 } }, 230 },
    { { { { 897, 0, -3996 }, { -262, 4087, -58 }, { 3987, 269, 895 } }, { 2381, 1413, 650 } }, 230 },
    { { { { -1383, 0, -3855 }, { -354, 4078, 127 }, { 3839, 376, -1377 } }, { -8739, 1597, -288 } }, 257 },
    { { { { 3754, 0, -1637 }, { -52, 4093, -121 }, { 1636, 132, 3752 } }, { -5306, 1248, 1319 } }, 230 },
    { { { { -3762, 0, -1618 }, { -138, 4080, 322 }, { 1613, 351, -3748 } }, { -5568, 1552, -4937 } }, 207 },
    { { { { 4070, 0, 459 }, { 337, 2784, -2984 }, { -312, 3003, 2767 } }, { 0x29F8, 1700, -374 } }, 257 },
};

/// Background-only batch list for mapped view 1; the terminal record selects no sprites.
static SpriteBatch _gShelterB1ElevatorHallView1SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Depth-sorted foreground rectangles for mapped view 2, in view-centred pixels.
static SpriteSource _gShelterB1ElevatorHallView2SpriteSources[47] = {
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 40 } }, -96, 8, 1138, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 32 } }, -96, -64, 1018, { .fields = { 48, 144 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 56 } }, -96, -120, 3430, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 56 } }, -128, -120, 984, { .fields = { 96, 56 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 32 } }, -128, -64, 1000, { .fields = { 40, 32 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 40 } }, -120, -32, 3744, { .fields = { 72, 40 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 40 } }, -120, 8, 3708, { .fields = { 72, 192 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 40 } }, -72, -120, 0, { .fields = { 120, 152 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 40 } }, -72, -80, 0, { .fields = { 120, 112 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, -72, -24, 1106, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, -72, -40, 0, { .fields = { 64, 64 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, -72, -8, 1123, { .fields = { 72, 232 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 32 } }, -72, 16, 1170, { .fields = { 64, 208 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 16 } }, -96, -32, 1033, { .fields = { 40, 232 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, -88, -16, 1126, { .fields = { 80, 232 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, -96, -16, 1113, { .fields = { 96, 232 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 40, 40 } }, -160, 8, 1164, { .fields = { 80, 112 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 40, 16 } }, -160, -32, 1042, { .fields = { 16, 80 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 40, 24 } }, -160, -16, 1170, { .fields = { 24, 120 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 16 } }, -160, -80, 952, { .fields = { 24, 64 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 32 } }, -160, -64, 1048, { .fields = { 64, 80 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 40 } }, -160, -120, 968, { .fields = { 88, 152 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, -72, 32, 0x30D4, { .fields = { 72, 32 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, -64, 32, 0x30D4, { .fields = { 80, 184 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 8 } }, -96, 32, 0, { .fields = { 32, 192 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 8 } }, -88, 24, 1145, { .fields = { 40, 200 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 8 } }, -88, 16, 1121, { .fields = { 40, 184 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 8 } }, -88, 8, 1102, { .fields = { 48, 248 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, -96, 8, 1101, { .fields = { 104, 232 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 8 } }, -96, -16, 0, { .fields = { 40, 176 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 8 } }, -80, -16, 1123, { .fields = { 24, 8 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, -64, -8, 1121, { .fields = { 112, 232 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, -72, -8, 1121, { .fields = { 120, 232 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 8 } }, -96, -8, 1119, { .fields = { 24, 16 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 8 } }, -96, 0, 1088, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, -64, 16, 1121, { .fields = { 72, 176 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, -72, 16, 1123, { .fields = { 96, 40 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 256), { .fields = { 24, 32 } }, -128, -72, 1053, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 256), { .fields = { 24, 24 } }, -128, 24, 1141, { .fields = { 40, 208 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 256), { .fields = { 8, 32 } }, -112, -40, 1064, { .fields = { 80, 152 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 256), { .fields = { 16, 32 } }, -128, -40, 1031, { .fields = { 56, 176 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 256), { .fields = { 8, 32 } }, -112, -8, 1136, { .fields = { 72, 144 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 256), { .fields = { 16, 32 } }, -128, -8, 1167, { .fields = { 64, 112 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 256), { .fields = { 8, 32 } }, -104, -40, 1055, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 256), { .fields = { 8, 32 } }, -104, -8, 1139, { .fields = { 56, 64 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 256), { .fields = { 32, 24 } }, -160, -40, 1128, { .fields = { 32, 96 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 256), { .fields = { 32, 40 } }, -160, -80, 1112, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
};

/// Background selector and three foreground source ranges for mapped view 2.
static SpriteBatch _gShelterB1ElevatorHallView2SpriteBatches[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 22, 0, 0, { 2, 0 } },
    { 22, 15, 0, 0, { 1, 0 } },
    { 37, ARRAY_SIZE(_gShelterB1ElevatorHallView2SpriteSources) - 37, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Background-only batch list for mapped view 3; the terminal record selects no sprites.
static SpriteBatch _gShelterB1ElevatorHallView3SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Background-only batch list for mapped view 4; the terminal record selects no sprites.
static SpriteBatch _gShelterB1ElevatorHallView4SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Depth-sorted foreground rectangles for mapped view 5, in view-centred pixels.
static SpriteSource _gShelterB1ElevatorHallView5SpriteSources[9] = {
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 48 } }, -88, -48, 1375, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 48 } }, -88, 0, 1375, { .fields = { 88, 96 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 48 } }, -56, 0, 1650, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 48 } }, -56, -48, 1625, { .fields = { 104, 144 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 48 } }, -32, 0, 1730, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 48 } }, -32, -48, 1674, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 48 } }, -16, -48, 1863, { .fields = { 120, 96 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 40 } }, -16, 0, 1887, { .fields = { 104, 48 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, -8, 16, 1892, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
};

/// Background selector and the complete foreground source range for mapped view 5.
static SpriteBatch _gShelterB1ElevatorHallView5SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gShelterB1ElevatorHallView5SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Mapped view 5 clip in draw-buffer pixels, restored at unscaled sorting depth 1870.
static SpriteDrawArea _gShelterB1ElevatorHallView5DrawAreas[2] = {
    { { 150, 64, 84, 92 }, 1870 },
    { { 0, 0, 0, 0 }, SPRITE_DRAW_AREA_END },
};

/// Background-only batch list for mapped view 6; the terminal record selects no sprites.
static SpriteBatch _gShelterB1ElevatorHallView6SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Background-only batch list for mapped view 7; the terminal record selects no sprites.
static SpriteBatch _gShelterB1ElevatorHallView7SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Depth-sorted foreground rectangles for mapped view 8, in view-centred pixels.
static SpriteSource _gShelterB1ElevatorHallView8SpriteSources[27] = {
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 64 } }, 120, 8, 0, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 40 } }, 120, -104, 0, { .fields = { 104, 40 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, 128, -96, 0, { .fields = { 96, 104 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 40, 16 } }, 120, -120, 650, { .fields = { 64, 152 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 8 } }, 128, -104, 669, { .fields = { 72, 168 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 16 } }, 136, -96, 647, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 8 } }, 136, -80, 649, { .fields = { 80, 176 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 8 } }, 128, -72, 655, { .fields = { 72, 184 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 56 } }, 152, -64, 460, { .fields = { 120, 64 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 48 } }, 152, -8, 494, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 40 } }, 152, 40, 624, { .fields = { 112, 184 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, 152, 80, 517, { .fields = { 96, 128 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 32 } }, 144, 72, 608, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 40 } }, 144, 32, 676, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 48 } }, 144, -16, 499, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 48 } }, 144, -64, 492, { .fields = { 120, 120 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 40 } }, 136, -64, 645, { .fields = { 104, 120 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 48 } }, 136, 56, 679, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 48 } }, 128, 56, 700, { .fields = { 120, 168 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 40 } }, 128, 16, 768, { .fields = { 104, 200 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 40 } }, 128, -24, 681, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 40 } }, 128, -64, 673, { .fields = { 120, 216 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 32 } }, 120, -64, 0, { .fields = { 96, 40 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 40 } }, 120, -32, 716, { .fields = { 104, 80 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 32 } }, 120, 72, 712, { .fields = { 96, 72 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 40 } }, 136, 16, 696, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 40 } }, 136, -24, 516, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
};

/// Background selector and the complete foreground source range for mapped view 8.
static SpriteBatch _gShelterB1ElevatorHallView8SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gShelterB1ElevatorHallView8SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Background-only batch list for mapped view 9; the terminal record selects no sprites.
static SpriteBatch _gShelterB1ElevatorHallView9SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView gShelterB1ElevatorHallSpriteViews[9] = {
    { { .empty = _gShelterB1ElevatorHallView1SpriteBatches }, _gShelterB1ElevatorHallView1SpriteBatches, NULL },
    { { .elements = _gShelterB1ElevatorHallView2SpriteSources }, _gShelterB1ElevatorHallView2SpriteBatches, NULL },
    { { .empty = _gShelterB1ElevatorHallView3SpriteBatches }, _gShelterB1ElevatorHallView3SpriteBatches, NULL },
    { { .empty = _gShelterB1ElevatorHallView4SpriteBatches }, _gShelterB1ElevatorHallView4SpriteBatches, NULL },
    { { .elements = _gShelterB1ElevatorHallView5SpriteSources }, _gShelterB1ElevatorHallView5SpriteBatches, _gShelterB1ElevatorHallView5DrawAreas },
    { { .empty = _gShelterB1ElevatorHallView6SpriteBatches }, _gShelterB1ElevatorHallView6SpriteBatches, NULL },
    { { .empty = _gShelterB1ElevatorHallView7SpriteBatches }, _gShelterB1ElevatorHallView7SpriteBatches, NULL },
    { { .elements = _gShelterB1ElevatorHallView8SpriteSources }, _gShelterB1ElevatorHallView8SpriteBatches, NULL },
    { { .empty = _gShelterB1ElevatorHallView9SpriteBatches }, _gShelterB1ElevatorHallView9SpriteBatches, NULL },
};

/// Fourteen mutable world-space point lights for actor shading in every hall view.
///
/// RGB intensities use twelve fractional bits; inner and outer falloff radii
/// use world-coordinate units. Coordinate setup and shading overwrite parent,
/// composition and attenuation state while the room overlay is loaded.
static WorldCoordPointLight _gShelterB1ElevatorHallPointLights[14] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -494, -601, -1312 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 2457, 2457 }, { 0, 0 } }, 900, 1800 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 1443, -601, -1312 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 2457, 2457 }, { 0, 0 } }, 900, 1800 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 6522, -601, -211 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 2457, 2457 }, { 0, 0 } }, 900, 1800 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 9439, -601, -491 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 2457, 2457 }, { 0, 0 } }, 900, 1800 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 9174, -601, 1918 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 2457, 2457 }, { 0, 0 } }, 900, 1800 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 5099, -601, 2541 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 2457, 2457 }, { 0, 0 } }, 900, 1800 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 1370, -601, 1216 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 2457, 2457 }, { 0, 0 } }, 900, 1800 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -103, -601, 1216 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 2457, 2457 }, { 0, 0 } }, 900, 1800 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -0x2827, -2031, 94 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 2457, 2457 }, { 0, 0 } }, 900, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -4197, -5422, 94 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2867, 2867, 2867 }, { 0, 0 } }, 2000, 7641 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 2390, -5242, 94 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3532, 3583, 3528 }, { 0, 0 } }, 2000, 6000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 8692, -5422, 1815 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2867, 2867, 2867 }, { 0, 0 } }, 2000, 7341 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -8238, -601, -1312 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 2457, 2457 }, { 0, 0 } }, 900, 1800 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -4076, -601, -1312 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 2457, 2457 }, { 0, 0 } }, 900, 1800 },
};

/// The hall's point-light collection, with no directional or cone lights.
static WorldCoordRoomLights _gShelterB1ElevatorHallRoomLights = {
    .directionalLightCount = 0,
    .directionalLights     = NULL,
    .pointLightCount       = ARRAY_SIZE(_gShelterB1ElevatorHallPointLights),
    .pointLights           = _gShelterB1ElevatorHallPointLights,
    .coneLightCount        = 0,
    .coneLights            = NULL,
};

/// Ten directed camera boundaries linking views 2/3/4/5/8/7 along the hall.
///
/// Origins, vertex offsets and radii use room-coordinate units; normals use
/// 4096 per unit. Room setup binds the coordinate and mutates list state and
/// hit latches. The final boundary is live and carries the array-end flag.
static WorldCollisionTrigger _gShelterB1ElevatorHallViewBoundaryTriggers[10] = {
    { NULL, NULL, NULL, { -8070, -1296, 92, 0 }, { { -364, -2112, -2398, 0 }, { 345, -2112, 2382, 0 }, { -364, 2112, -2398, 0 }, { 345, 2112, 2382, 0 } }, { 4066, 0, -604, 0 }, { 0, 0, ONE, 0 }, 3207, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -7952, -1360, 16, 0 }, { { 337, -2112, 2391, 0 }, { -340, -2112, -2393, 0 }, { 337, 2112, 2391, 0 }, { -340, 2112, -2393, 0 } }, { -4071, 0, 575, 0 }, { 0, 0, ONE, 0 }, 3207, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -2752, -1441, 0, 0 }, { { 332, -2112, 2386, 0 }, { -345, -2112, -2396, 0 }, { 332, 2112, 2386, 0 }, { -345, 2112, -2396, 0 } }, { -4069, 0, 575, 0 }, { 0, 0, ONE, 0 }, 3207, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -3040, -1441, 0, 0 }, { { -356, -2112, -2392, 0 }, { 353, -2112, 2389, 0 }, { -356, 2112, -2392, 0 }, { 353, 2112, 2389, 0 } }, { 4067, 0, -604, 0 }, { 0, 0, ONE, 0 }, 3207, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 7824, -1313, 1792, 0 }, { { 3280, -2112, 194, 0 }, { -3309, -2112, -233, 0 }, { 3280, 2112, 194, 0 }, { -3309, 2112, -233, 0 } }, { -266, 0, 4095, 0 }, { 0, 0, ONE, 0 }, 3916, 0, 7, 8, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 7944, -1248, 1894, 0 }, { { -3173, -2112, -227, 0 }, { 3172, -2112, 227, 0 }, { -3173, 2112, -227, 0 }, { 3172, 2112, 227, 0 } }, { 292, 0, -4087, 0 }, { 0, 0, ONE, 0 }, 3814, 0, 8, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 32, -1248, 0, 0 }, { { 0, -2112, -2416, 0 }, { 0, -2112, 2416, 0 }, { 0, 2112, -2416, 0 }, { 0, 2112, 2416, 0 } }, { 4110, 0, 0, 0 }, { 0, 0, ONE, 0 }, 3207, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 256, -1248, 0, 0 }, { { -16, -2112, 2416, 0 }, { 16, -2112, -2416, 0 }, { -16, 2112, 2416, 0 }, { 16, 2112, -2416, 0 } }, { -4111, 0, -28, 0 }, { 0, 0, ONE, 0 }, 3207, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 5009, -1408, 0, 0 }, { { -13, -2112, -2417, 0 }, { 12, -2112, 2416, 0 }, { -13, 2112, -2417, 0 }, { 12, 2112, 2416, 0 } }, { 4111, 0, -22, 0 }, { 0, 0, ONE, 0 }, 3207, 0, 8, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 5184, -1248, 0, 0 }, { { -16, -2112, 2416, 0 }, { 16, -2112, -2416, 0 }, { -16, 2112, 2416, 0 }, { 16, 2112, -2416, 0 } }, { -4111, 0, -28, 0 }, { 0, 0, ONE, 0 }, 3207, 0, 5, 8, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Four facing-gated departures followed by two hotspots for CAP command 6.
///
/// Warp parameters pack the current endpoint in the high nibble and the
/// destination arrival ID in the low nibble. Room setup binds coordinates and
/// links mutable records; the sixth, live hotspot carries the array-end flag.
static WorldCollisionTrigger _gShelterB1ElevatorHallActionTriggers[6] = {
    { NULL, NULL, NULL, { -0x2A80, -48, 320, 0 }, { { -448, 0, -544, 0 }, { 448, 0, -544, 0 }, { -448, 0, 544, 0 }, { 448, 0, 544, 0 } }, { 0, 4098, 0, 0 }, { 0, 0, -ONE, 0 }, 704, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_MINE_SECRET_PASSAGE, (SHELTER_B1_ELEVATOR_HALL_WARP_MINE_PASSAGE << 4) | SHELTER_B1_ELEVATOR_HALL_MINE_PASSAGE_ARRIVAL, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -4064, -48, 1072, 0 }, { { -896, 0, -400, 0 }, { 896, 0, -400, 0 }, { -896, 0, 400, 0 }, { 896, 0, 400, 0 } }, { 0, 4105, 0, 0 }, { 0, 0, -ONE, 0 }, 981, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_SHELTER_B1_MAIN_CORRIDOR, (SHELTER_B1_ELEVATOR_HALL_WARP_MAIN_CORRIDOR << 4) | SHELTER_B1_ELEVATOR_HALL_DESTINATION_FIRST_WARP, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x2790, -48, -288, 0 }, { { -480, 0, -560, 0 }, { 480, 0, -560, 0 }, { -480, 0, 560, 0 }, { 480, 0, 560, 0 } }, { 0, ONE, 0, 0 }, { -ONE, 0, 0, 0 }, 735, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_SHELTER_B2_ELEVATOR, (SHELTER_B1_ELEVATOR_HALL_WARP_ELEVATOR << 4) | SHELTER_B1_ELEVATOR_HALL_DESTINATION_FIRST_WARP, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x2780, -48, 4064, 0 }, { { -480, 0, -560, 0 }, { 480, 0, -560, 0 }, { -480, 0, 560, 0 }, { 480, 0, 560, 0 } }, { 0, ONE, 0, 0 }, { -ONE, 0, 0, 0 }, 735, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_SHELTER_B1_SOUTH_MAINTENANCE_WALKWAY, (SHELTER_B1_ELEVATOR_HALL_WARP_SOUTH_WALKWAY << 4) | SHELTER_B1_ELEVATOR_HALL_DESTINATION_FIRST_WARP, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 6224, -64, 4128, 0 }, { { -1136, 0, -544, 0 }, { 1136, 0, -544, 0 }, { -1136, 0, 544, 0 }, { 1136, 0, 544, 0 } }, { 0, 4113, 0, 0 }, { 0, 0, -ONE, 0 }, 1254, WORLD_COLLISION_TRIGGER_ACTION_CAP, SHELTER_B1_ELEVATOR_HALL_HOTSPOT_CAP_COMMAND, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 7488, -64, 4256, 0 }, { { -592, 0, -544, 0 }, { 592, 0, -544, 0 }, { -592, 0, 544, 0 }, { 592, 0, 544, 0 } }, { 0, 4101, 0, 0 }, { ONE, 0, 0, 0 }, 801, WORLD_COLLISION_TRIGGER_ACTION_CAP, SHELTER_B1_ELEVATOR_HALL_HOTSPOT_CAP_COMMAND, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Single room-space sight-blocking quad, linked and enabled during room setup.
///
/// Positions and radius use room-coordinate units, with 4096 per unit normal.
/// The live record also ends the array. Its retained low flag bit is unproven.
static WorldCollisionOccluder _gShelterB1ElevatorHallOccluders[1] = {
    { NULL, NULL, { 2768, -1360, 3664, 0 }, { { -1968, -2384, 1936, 0 }, { 1968, -2384, -1936, 0 }, { -1968, 2384, 1936, 0 }, { 1968, 2384, -1936, 0 } }, { -2879, 0, -2926, 0 }, 3647, 1 | WORLD_COLLISION_OCCLUDER_LAST, 0 },
};

/// Zebra Stalker base file 400600 and its body recipe for encounter variant 2.
///
/// Placement entry 6 selects descriptor 0. The second record ends the resource
/// list; the hall and actor_400600 overlays must remain loaded while used.
static AreaResource _gShelterB1ElevatorHallZebraStalkerResources[2] = {
    { .entryId        = SHELTER_B1_ELEVATOR_HALL_RESOURCE_ZEBRA_STALKER,
      .fileNumber     = 6,
      .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_40,
      .taskIndex      = 0,
      .taskTable      = &gActor400600ZebraStalkerTaskDesc },
    { .entryId = AREA_PLACEMENT_END },
};

/// Brain Stinger base file 100300 and its body recipe for encounter variant 3.
///
/// Placement entry 3 selects descriptor 0; the second record ends the list.
/// Keep the hall and actor_100300 overlays loaded while these records are used.
static AreaResource _gShelterB1ElevatorHallBrainStingerResources[2] = {
    { .entryId        = SHELTER_B1_ELEVATOR_HALL_RESOURCE_BRAIN_STINGER,
      .fileNumber     = 3,
      .fileGroupIndex = LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR,
      .taskIndex      = 0,
      .taskTable      = gActor100300Tasks },
    { .entryId = AREA_PLACEMENT_END },
};

/// Watcher base file 102100 and its body recipe for encounter variant 4.
///
/// Placement entry 21 selects descriptor 0; the second record ends the list.
/// Keep the hall and actor_102100 overlays loaded while these records are used.
static AreaResource _gShelterB1ElevatorHallWatcherResources[2] = {
    { .entryId        = SHELTER_B1_ELEVATOR_HALL_RESOURCE_WATCHER,
      .fileNumber     = 21,
      .fileGroupIndex = LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR,
      .taskIndex      = 0,
      .taskTable      = &gActor102100WatcherTaskDesc },
    { .entryId = AREA_PLACEMENT_END },
};

/// Rook and Pawn GOLEM files 102300/205600 and body recipes for variant 11.
///
/// Entries 23/56 select descriptor 0 in their respective actor packages;
/// the third record ends the list. Both actor overlays and the hall must
/// remain loaded while these records are used.
static AreaResource _gShelterB1ElevatorHallGolemResources[3] = {
    { .entryId        = SHELTER_B1_ELEVATOR_HALL_RESOURCE_ROOK_GOLEM,
      .fileNumber     = 23,
      .fileGroupIndex = LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR,
      .taskIndex      = 0,
      .taskTable      = gActor102300RookGolemTasks },
    { .entryId        = SHELTER_B1_ELEVATOR_HALL_RESOURCE_PAWN_GOLEM,
      .fileNumber     = 56,
      .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_20,
      .taskIndex      = 0,
      .taskTable      = gActor205600PawnGolemTasks },
    { .entryId = AREA_PLACEMENT_END },
};

/// Three floor-level Zebra Stalkers for variant 2, followed by the end record.
///
/// Each starts with plain entrance mode 0 and uses base-file textures with
/// CLUT row offset 2. Positions use world units; yaw uses 4096 per turn.
/// Actors borrow these placements until teardown, within the hall overlay's life.
static AreaPlacement _gShelterB1ElevatorHallZebraStalkerPlacements[4] = {
    { .entryId = SHELTER_B1_ELEVATOR_HALL_RESOURCE_ZEBRA_STALKER, .x = -700, .yaw = ACTOR_TRANSFORM_ANGLE_TURN / 4, .clutRowOffset = 2 },
    { .entryId = SHELTER_B1_ELEVATOR_HALL_RESOURCE_ZEBRA_STALKER, .x = 6050, .z = 3250, .yaw = 1700, .clutRowOffset = 2 },
    { .entryId = SHELTER_B1_ELEVATOR_HALL_RESOURCE_ZEBRA_STALKER, .x = -2150, .z = -500, .yaw = 3700, .clutRowOffset = 2 },
    { .entryId = AREA_PLACEMENT_END },
};

/// Two Brain Stingers on patrol routes 0 and 1 for encounter variant 3.
///
/// Uses base-file textures with CLUT row offset 2. Positions use world units;
/// yaw uses 4096 per turn. The third record ends the list, and actors borrow
/// these placements until teardown within the hall overlay's life.
static AreaPlacement _gShelterB1ElevatorHallBrainStingerPlacements[3] = {
    { .entryId = SHELTER_B1_ELEVATOR_HALL_RESOURCE_BRAIN_STINGER, .mode = 0, .x = -5500, .yaw = ACTOR_TRANSFORM_ANGLE_TURN * 3 / 4, .clutRowOffset = 2 },
    { .entryId = SHELTER_B1_ELEVATOR_HALL_RESOURCE_BRAIN_STINGER, .mode = 1, .x = -2500, .yaw = ACTOR_TRANSFORM_ANGLE_TURN / 4, .clutRowOffset = 2 },
    { .entryId = AREA_PLACEMENT_END },
};

/// Ten Watchers at world Y -2000 for variant 4, followed by the end record.
///
/// Target-range row 0 selects 3000 units; the two X=3000 placements select
/// row 2's 4000 units. Base-file textures use CLUT row offset 2. Yaw uses
/// 4096 per turn, and actors borrow the records until the hall is torn down.
static AreaPlacement _gShelterB1ElevatorHallWatcherPlacements[11] = {
    { .entryId = SHELTER_B1_ELEVATOR_HALL_RESOURCE_WATCHER, .y = -2000, .z = 1500, .yaw = ACTOR_TRANSFORM_ANGLE_HALF_TURN, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = SHELTER_B1_ELEVATOR_HALL_RESOURCE_WATCHER, .x = 3000, .y = -2000, .z = 1500, .yaw = ACTOR_TRANSFORM_ANGLE_HALF_TURN, .clutRowOffset = 2, .rowIndex = 2 },
    { .entryId = SHELTER_B1_ELEVATOR_HALL_RESOURCE_WATCHER, .y = -2000, .z = -1500, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = SHELTER_B1_ELEVATOR_HALL_RESOURCE_WATCHER, .x = 3000, .y = -2000, .z = -1500, .clutRowOffset = 2, .rowIndex = 2 },
    { .entryId = SHELTER_B1_ELEVATOR_HALL_RESOURCE_WATCHER, .x = 8000, .y = -2000, .z = -1500, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = SHELTER_B1_ELEVATOR_HALL_RESOURCE_WATCHER, .x = 7000, .y = -2000, .z = -1500, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = SHELTER_B1_ELEVATOR_HALL_RESOURCE_WATCHER, .x = 6000, .y = -2000, .z = -1500, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = SHELTER_B1_ELEVATOR_HALL_RESOURCE_WATCHER, .x = 8000, .y = -2000, .z = 4900, .yaw = ACTOR_TRANSFORM_ANGLE_HALF_TURN, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = SHELTER_B1_ELEVATOR_HALL_RESOURCE_WATCHER, .x = 7000, .y = -2000, .z = 4900, .yaw = ACTOR_TRANSFORM_ANGLE_HALF_TURN, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = SHELTER_B1_ELEVATOR_HALL_RESOURCE_WATCHER, .x = 6000, .y = -2000, .z = 4900, .yaw = ACTOR_TRANSFORM_ANGLE_HALF_TURN, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

/// Rook/Pawn GOLEM pair for variant 11, with patrol spans of 2000/7000 units.
///
/// Mode 1 starts each patrol. Base-file textures use page/CLUT offsets 0/2
/// for the Rook and 3/5 for the Pawn. World positions and yaw (4096 per turn)
/// seed the actors. The third record ends the list; borrow until hall teardown.
static AreaPlacement _gShelterB1ElevatorHallGolemPlacements[3] = {
    { .entryId = SHELTER_B1_ELEVATOR_HALL_RESOURCE_ROOK_GOLEM, .variant = 2, .mode = 1, .x = 9000, .z = 1500, .clutRowOffset = 2 },
    { .entryId = SHELTER_B1_ELEVATOR_HALL_RESOURCE_PAWN_GOLEM, .variant = 7, .mode = 1, .x = 5000, .yaw = ACTOR_TRANSFORM_ANGLE_TURN * 3 / 4, .texturePageOffset = 3, .clutRowOffset = 5 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaVariant gShelterB1ElevatorHallAreaVariants[12] = {
    [SHELTER_B1_ELEVATOR_HALL_VARIANT_ZEBRA_STALKERS] = { .placements = _gShelterB1ElevatorHallZebraStalkerPlacements, .resources = _gShelterB1ElevatorHallZebraStalkerResources },
    [SHELTER_B1_ELEVATOR_HALL_VARIANT_BRAIN_STINGERS] = { .placements = _gShelterB1ElevatorHallBrainStingerPlacements, .resources = _gShelterB1ElevatorHallBrainStingerResources },
    [SHELTER_B1_ELEVATOR_HALL_VARIANT_WATCHERS]       = { .placements = _gShelterB1ElevatorHallWatcherPlacements, .resources = _gShelterB1ElevatorHallWatcherResources },
    [SHELTER_B1_ELEVATOR_HALL_VARIANT_GOLEMS]         = { .placements = _gShelterB1ElevatorHallGolemPlacements, .resources = _gShelterB1ElevatorHallGolemResources },
};

/// Surface-class-1 cue-2 bases in the loaded type-1 bank; cue 1 uses the next entry.
static WorldCollisionFootstepSounds _gShelterB1ElevatorHallSurface1Footsteps = {
    .walk         = SHELTER_B1_ELEVATOR_HALL_SURFACE1_WALK_CUE,
    .run          = SHELTER_B1_ELEVATOR_HALL_SURFACE1_RUN_CUE,
    .scriptedJump = SHELTER_B1_ELEVATOR_HALL_SURFACE1_WALK_CUE,
};

/// Surface-class-2 cue-2 bases in the loaded type-1 bank; cue 1 uses the next entry.
static WorldCollisionFootstepSounds _gShelterB1ElevatorHallSurface2Footsteps = {
    .walk         = SHELTER_B1_ELEVATOR_HALL_SURFACE2_WALK_CUE,
    .run          = SHELTER_B1_ELEVATOR_HALL_SURFACE2_RUN_CUE,
    .scriptedJump = SHELTER_B1_ELEVATOR_HALL_SURFACE2_WALK_CUE,
};

/// Blocking, pushback-enabled surfaces without footstep cues (classes 0 and 3..7).
static WorldCollisionSurfaceProperties _gShelterB1ElevatorHallSilentSurfaceProperties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

/// Blocking floor surface class 1, with pushback and its walk/run cue pairs.
static WorldCollisionSurfaceProperties _gShelterB1ElevatorHallSurface1Properties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gShelterB1ElevatorHallSurface1Footsteps,
};

/// Blocking floor surface class 2, with pushback and its distinct walk/run cue pairs.
static WorldCollisionSurfaceProperties _gShelterB1ElevatorHallSurface2Properties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gShelterB1ElevatorHallSurface2Footsteps,
};

WorldCollisionSurfaceProperties* gShelterB1ElevatorHallSurfacePropertiesByClass[8] = {
    &_gShelterB1ElevatorHallSilentSurfaceProperties,
    &_gShelterB1ElevatorHallSurface1Properties,
    &_gShelterB1ElevatorHallSurface2Properties,
    &_gShelterB1ElevatorHallSilentSurfaceProperties,
    &_gShelterB1ElevatorHallSilentSurfaceProperties,
    &_gShelterB1ElevatorHallSilentSurfaceProperties,
    &_gShelterB1ElevatorHallSilentSurfaceProperties,
    &_gShelterB1ElevatorHallSilentSurfaceProperties,
};

RoomFadeStorage gShelterB1ElevatorHallMineTransitFade;

/// Installs the hall's seven effect IDs for actors and nested effects.
///
/// Each ID packs task bank 6 and the type of a handler in this room overlay.
/// Call after the room effect controller resets its spawn slots. The IDs are
/// valid while this hall's overlay remains loaded.
static inline void _shelterB1ElevatorHallInstallEffectIds(void)
{
    gRoomEffectMoteId         = EFFECT_SHELTER_B1_ELEVATOR_HALL_MOTE;
    gRoomEffectHaloId         = EFFECT_SHELTER_B1_ELEVATOR_HALL_HALO;
    gRoomEffectOrangeBurstId  = EFFECT_SHELTER_B1_ELEVATOR_HALL_ORANGE_BURST;
    gRoomEffectSparkEmitterId = EFFECT_SHELTER_B1_ELEVATOR_HALL_SPARK_EMITTER;
    gRoomEffectFlashId        = EFFECT_SHELTER_B1_ELEVATOR_HALL_FLASH;
    gRoomEffectTwinTrailId    = EFFECT_SHELTER_B1_ELEVATOR_HALL_TWIN_TRAIL;
    gRoomEffectSparkBurstId   = EFFECT_SHELTER_B1_ELEVATOR_HALL_SPARK_BURST;
}

void shelterB1ElevatorHallDrawGlowsTask(Task* task)
{
    enum { GLOWS_INITIALIZE,
           GLOWS_DRAW,
           GLOWS_STANDARD_RADIUS_SCALE = 0x180,
           GLOWS_WARM_RADIUS_SCALE     = 0x200,
           GLOWS_GREY_RGB              = 0x444,
           GLOWS_WARM_RGB              = 0x421,
           GLOWS_CYAN_RGB              = 0x044 }; // RGB nibbles become channel levels in steps of 16.

    if (task->state == GLOWS_INITIALIZE) {
        _shelterB1ElevatorHallInstallEffectIds();
        task->state = GLOWS_DRAW;
    }

    // Select endpoint pairs visible in this view; each draw reads two consecutive points.
    switch (viewGetMappedIndex() & 0xFF) {
        case 2:
            _glowDrawCapsule(_gShelterB1ElevatorHallGlowEndpointPairs[0], GLOWS_STANDARD_RADIUS_SCALE, GLOWS_GREY_RGB);
            _glowDrawCapsule(_gShelterB1ElevatorHallGlowEndpointPairs[13], GLOWS_STANDARD_RADIUS_SCALE, GLOWS_CYAN_RGB);
            break;
        case 3: {
            const SVECTOR(*glowPairs)[2];
            glowPairs = &_gShelterB1ElevatorHallGlowEndpointPairs[1];
            _glowDrawCapsule(glowPairs[0], GLOWS_STANDARD_RADIUS_SCALE, GLOWS_GREY_RGB);
            _glowDrawCapsule(glowPairs[9], GLOWS_WARM_RADIUS_SCALE, GLOWS_WARM_RGB);
            break;
        }
        case 4: {
            const SVECTOR(*glowPairs)[2];
            glowPairs = &_gShelterB1ElevatorHallGlowEndpointPairs[2];
            _glowDrawCapsule(glowPairs[0], GLOWS_STANDARD_RADIUS_SCALE, GLOWS_GREY_RGB);
            _glowDrawCapsule(glowPairs[4], GLOWS_STANDARD_RADIUS_SCALE, GLOWS_GREY_RGB);
            _glowDrawCapsule(glowPairs[8], GLOWS_WARM_RADIUS_SCALE, GLOWS_WARM_RGB);
            break;
        }
        case 5: {
            const SVECTOR(*glowPairs)[2];
            glowPairs = &_gShelterB1ElevatorHallGlowEndpointPairs[3];
            _glowDrawCapsule(glowPairs[0], GLOWS_STANDARD_RADIUS_SCALE, GLOWS_GREY_RGB);
            _glowDrawCapsule(glowPairs[3], GLOWS_STANDARD_RADIUS_SCALE, GLOWS_GREY_RGB);
            _glowDrawCapsule(glowPairs[4], GLOWS_STANDARD_RADIUS_SCALE, GLOWS_GREY_RGB);
            _glowDrawCapsule(glowPairs[6], GLOWS_STANDARD_RADIUS_SCALE, GLOWS_GREY_RGB);
            _glowDrawCapsule(glowPairs[8], GLOWS_WARM_RADIUS_SCALE, GLOWS_WARM_RGB);
            break;
        }
        case 7: {
            const SVECTOR(*glowPairs)[2];
            glowPairs = &_gShelterB1ElevatorHallGlowEndpointPairs[9];
            _glowDrawCapsule(glowPairs[0], GLOWS_STANDARD_RADIUS_SCALE, GLOWS_GREY_RGB);
            _glowDrawCapsule(glowPairs[3], GLOWS_WARM_RADIUS_SCALE, GLOWS_WARM_RGB);
            break;
        }
        case 8: {
            const SVECTOR(*glowPairs)[2];
            glowPairs = &_gShelterB1ElevatorHallGlowEndpointPairs[4];
            _glowDrawCapsule(glowPairs[0], GLOWS_STANDARD_RADIUS_SCALE, GLOWS_GREY_RGB);
            _glowDrawCapsule(glowPairs[1], GLOWS_STANDARD_RADIUS_SCALE, GLOWS_GREY_RGB);
            _glowDrawCapsule(glowPairs[7], GLOWS_WARM_RADIUS_SCALE, GLOWS_WARM_RGB);
            break;
        }
        case 9:
            _glowDrawCapsule(_gShelterB1ElevatorHallGlowEndpointPairs[13], GLOWS_STANDARD_RADIUS_SCALE, GLOWS_CYAN_RGB);
            break;
    }
}

#include "../../shared/effects/glow_draw_capsule.inc.c"

#include "../../shared/room/room_visual_effects.inc.c"

void shelterB1ElevatorHallRoomVisualEffectsMoteTask(Task* task)
{
    _roomVisualEffectsMoteTask(task);
}

#include "../../shared/room/room_visual_effects_halo.inc.c"

void shelterB1ElevatorHallRoomVisualEffectsHaloTask(Task* task)
{
    _roomVisualEffectsHaloTask(task);
}

void shelterB1ElevatorHallRoomVisualEffectsHaloOrangeBurstTask(Task* task)
{
    _roomVisualEffectsHaloOrangeBurstTask(task);
}

#include "../../shared/room/room_visual_effects_glow_quad.inc.c"
#include "../../shared/room/room_visual_effects_flash.inc.c"

void shelterB1ElevatorHallRoomVisualEffectsSparkEmitterTask(Task* task)
{
    _roomVisualEffectsSparkEmitterTask(task);
}

#include "../../shared/room/room_visual_effects_flash_task.inc.c"

void shelterB1ElevatorHallRoomVisualEffectsFlashTask(Task* task)
{
    _roomVisualEffectsFlashTask(task);
}

#include "../../shared/room/room_visual_effects_trails.inc.c"

void shelterB1ElevatorHallRoomVisualEffectsTwinTrailTask(Task* task)
{
#include "../../shared/room/room_visual_effects_trail_task.inc.c"

    void shelterB1ElevatorHallRoomVisualEffectsSparkBurstTask(Task * task)
    {
        _roomVisualEffectsSparkBurstTask(task);
    }

#include "../../shared/room/room_visual_effects_glow.inc.c"
