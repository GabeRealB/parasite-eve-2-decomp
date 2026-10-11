#include "rooms/acropolis_forked_road.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "acropolis_forked_road_private.h"

#include "actors/task_tables.h"

#include "gameplay/companion_load.h"
#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/evs.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
#include "gameplay/pad_input.h"
#include "gameplay/pad_script.h"
#include "gameplay/player_actor.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_combat.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/fs_types.h"
#include "main/gameflag_ids.h"
#include "main/gameflow.h"
#include "main/gamemain.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/stream.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "mapui/map_akropolis.h"

#include "rooms/room.h"

#include "rooms/room_common.h"
#include "../../shared/room_visual_effects.h"
#include "../../shared/falling_leaves.h"

static void _acropolisForkedRoadOutboundMoviePathTask(Task* task);
static void _acropolisForkedRoadReturnMoviePathTask(Task* task);
static void _acropolisForkedRoadSkipFadeOutTask(Task* task);
static void _acropolisForkedRoadSkipFadeInTask(Task* task);

static WorldCollisionGrid    _gAcropolisForkedRoadCollisionGrid;
static WorldCollisionTrigger _gAcropolisForkedRoadViewBoundaryTriggers[6];
static WorldCollisionTrigger _gAcropolisForkedRoadActionTriggers[7];
static WorldCoordRoomLights  _gAcropolisForkedRoadRoomLights;
static void                  _acropolisForkedRoadReleaseMaggotCaterpillarEntrance(void);

enum { ACROPOLIS_FORKED_ROAD_MOVIE_TASK_PRIORITY = 192 };

TaskDesc gAcropolisForkedRoadMovieTaskDescriptors[ACROPOLIS_FORKED_ROAD_MOVIE_TASK_COUNT] = {
    [ACROPOLIS_FORKED_ROAD_TASK_OUTBOUND_MOVIE] = { { { TASK_BODY_NONE, ACROPOLIS_FORKED_ROAD_MOVIE_TASK_PRIORITY } }, _acropolisForkedRoadOutboundMoviePathTask, { .value = 0 } },
    [ACROPOLIS_FORKED_ROAD_TASK_UNUSED_MOVIE]   = { { { TASK_BODY_NONE, ACROPOLIS_FORKED_ROAD_MOVIE_TASK_PRIORITY } }, NULL, { .value = 0 } },
    [ACROPOLIS_FORKED_ROAD_TASK_RETURN_MOVIE]   = { { { TASK_BODY_NONE, ACROPOLIS_FORKED_ROAD_MOVIE_TASK_PRIORITY } }, _acropolisForkedRoadReturnMoviePathTask, { .value = 0 } },
    [ACROPOLIS_FORKED_ROAD_TASK_SKIP_FADE_OUT]  = { { { TASK_BODY_NONE, ACROPOLIS_FORKED_ROAD_MOVIE_TASK_PRIORITY } }, _acropolisForkedRoadSkipFadeOutTask, { .value = 0 } },
    [ACROPOLIS_FORKED_ROAD_TASK_SKIP_FADE_IN]   = { { { TASK_BODY_NONE, ACROPOLIS_FORKED_ROAD_MOVIE_TASK_PRIORITY } }, _acropolisForkedRoadSkipFadeInTask, { .value = 0 } },
};

/// Player-root translations sampled along the observatory route's movie path.
///
/// All 300 signed-halfword XYZ samples use whole units in the root's parent
/// frame, without a terminator. Outbound indexing is `movieFrame - 1`; the
/// movie descriptor caps frames at 300, and the controller requests departure
/// on index 120 or Start. Return indexing is `59 - movieFrame` and requires
/// frames 1..59 at every access; normal exit begins below index 11. Keep the
/// overlay loaded while either task reads the table. The fourth halfword is unused.
static SVECTOR _gAcropolisForkedRoadMoviePlayerPositions[300] = {
#include "assets/acropolis_forked_road_path_039C0.inc"
};

static AnimationPackedPose _gAcropolisForkedRoadAnimation045FCBank1[6] = {
#include "assets/acropolis_forked_road_animation_045FC_bank1.inc"
};

static AnimationPackedRotation _gAcropolisForkedRoadAnimation045FCBank4[46] = {
#include "assets/acropolis_forked_road_animation_045FC_bank4.inc"
};

static AnimationRecord _gAcropolisForkedRoadAnimation045FCRecords[109] = {
#include "assets/acropolis_forked_road_animation_045FC_records.inc"
};

static u16 _gAcropolisForkedRoadAnimation045FCIndices[20] = {
#include "assets/acropolis_forked_road_animation_045FC_indices.inc"
};

static AnimationSet _gAcropolisForkedRoadAnimation045FC = {
    _gAcropolisForkedRoadAnimation045FCRecords,
    _gAcropolisForkedRoadAnimation045FCIndices,
    { NULL, _gAcropolisForkedRoadAnimation045FCBank1, NULL, NULL, _gAcropolisForkedRoadAnimation045FCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gAcropolisForkedRoadAnimation04A6CBank1[10] = {
#include "assets/acropolis_forked_road_animation_04A6C_bank1.inc"
};

static AnimationPackedRotation _gAcropolisForkedRoadAnimation04A6CBank4[95] = {
#include "assets/acropolis_forked_road_animation_04A6C_bank4.inc"
};

static AnimationRecord _gAcropolisForkedRoadAnimation04A6CRecords[139] = {
#include "assets/acropolis_forked_road_animation_04A6C_records.inc"
};

static u16 _gAcropolisForkedRoadAnimation04A6CIndices[20] = {
#include "assets/acropolis_forked_road_animation_04A6C_indices.inc"

};

static AnimationSet _gAcropolisForkedRoadAnimation04A6C = {
    _gAcropolisForkedRoadAnimation04A6CRecords,
    _gAcropolisForkedRoadAnimation04A6CIndices,
    { NULL, _gAcropolisForkedRoadAnimation04A6CBank1, NULL, NULL, _gAcropolisForkedRoadAnimation04A6CBank4, NULL, NULL, NULL },
};

/// Three set pointers copied to equipped-weapon bank entries 47..49 for the lock-release event.
///
/// Entry 47 is cleared; entries 48 and 49 borrow the two room clips. No
/// terminator: the copy request covers all three words. Keep the overlay and
/// clip pools loaded while the player bank retains these pointers.
static AnimationSet* _gAcropolisForkedRoadLockReleaseAnimationSets[3] = {
    NULL,
    &_gAcropolisForkedRoadAnimation045FC,
    &_gAcropolisForkedRoadAnimation04A6C,
};

/// Installs the lock-release event's three-pointer extension in the player's equipped-weapon bank.
///
/// The request is borrowed synchronously; the copied set pointers outlive the message.
static AnimationBankCopyRequest _gAcropolisForkedRoadLockReleaseBankCopyRequest = {
    .source    = { .sets = _gAcropolisForkedRoadLockReleaseAnimationSets },
    .wordCount = ARRAY_SIZE(_gAcropolisForkedRoadLockReleaseAnimationSets),
};

AnimationPlayRequest D_acropolis_forked_road_80182068 = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

enum {
    ACROPOLIS_FORKED_ROAD_LOCK_RELEASE_PLAYER_CLIP  = 48,
    ACROPOLIS_FORKED_ROAD_LOCK_RELEASE_BLEND_FRAMES = 20,
    ACROPOLIS_FORKED_ROAD_ENTRANCE_WAIT_TICKS       = 10,
    ACROPOLIS_FORKED_ROAD_BATTLE_WAIT_TICKS         = 3,
};

/// Blends the player to extension clip 48 with grid collision disabled for the lock-release event.
///
/// The event interpreter copies the request and resolves its bank from the
/// equipped weapon, replacing the stored index. Blend duration is twenty
/// normal-rate frames; the installed set and its data must remain loaded.
static AnimationPlayRequest _gAcropolisForkedRoadLockReleasePlayerClip48Request = {
    .source               = { .index = 1 },
    .animationId          = ACROPOLIS_FORKED_ROAD_LOCK_RELEASE_PLAYER_CLIP,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = ACROPOLIS_FORKED_ROAD_LOCK_RELEASE_BLEND_FRAMES,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

AnimationPlayRequest D_acropolis_forked_road_80182090[2] = {
    { { .index = 1 }, 49, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, 9, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE },
};

EvsCommand gAcropolisForkedRoadLockReleaseScript[8] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gAcropolisForkedRoadLockReleaseBankCopyRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gAcropolisForkedRoadLockReleasePlayerClip48Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _acropolisForkedRoadReleaseMaggotCaterpillarEntrance }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = ACROPOLIS_FORKED_ROAD_ENTRANCE_WAIT_TICKS }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = sceneEngageBattle }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = ACROPOLIS_FORKED_ROAD_BATTLE_WAIT_TICKS }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_KEEP_ROOT_OFFSET }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Fourteen wall-lamp offsets in their effect controller's coordinate frame.
///
/// Indexed by lamp 0..13, shared with `_gAcropolisForkedRoadWallLampViewMasks`.
/// XYZ uses whole coordinate units. Spawning copies the position; there is no
/// sentinel. Cells 0, 1 and 2 use indices 0..3, 4..11 and 12..13 respectively.
static SVECTOR _gAcropolisForkedRoadWallLampPositions[14] = {
    { -50, -1810, 2070, 0 },
    { -50, -1810, 430, 0 },
    { 470, -3290, -2200, 0 },
    { 80, -3290, -3070, 0 },
    { -1480, -220, 2450, 0 },
    { -540, -220, 2410, 0 },
    { 280, -220, -50, 0 },
    { 270, -220, -1130, 0 },
    { -430, -220, -1800, 0 },
    { -1400, -220, -2310, 0 },
    { -2250, -220, -2770, 0 },
    { -3310, -220, -3330, 0 },
    { 1550, -1058, -790, 0 },
    { -770, -1058, -2990, 0 },
};

/// Logical-view visibility masks for wall lamps 0..13.
///
/// Bit `view - 1` permits drawing in that 1-based logical view, before mapping
/// to a camera/image index. Room view domains are 1..10, 1..9 and 1..9; no mask
/// sets bits above view 9. Lamp 10's zero mask suppresses it in every view.
/// No terminator; keep this overlay loaded for the lamps' lifetimes.
static u16 _gAcropolisForkedRoadWallLampViewMasks[14] = {
    208,
    240,
    20,
    20,
    20,
    212,
    468,
    276,
    276,
    20,
    0,
    8,
    244,
    34,
};

#include "../../shared/room_visual_effects_trail_data.inc.c"

WorldCollisionRoomResources gAcropolisForkedRoadRoomCollisionResources[3] = {
    { &_gAcropolisForkedRoadCollisionGrid, _gAcropolisForkedRoadViewBoundaryTriggers, _gAcropolisForkedRoadActionTriggers, NULL },
    { &_gAcropolisForkedRoadCollisionGrid, _gAcropolisForkedRoadViewBoundaryTriggers, _gAcropolisForkedRoadActionTriggers, NULL },
    { &_gAcropolisForkedRoadCollisionGrid, _gAcropolisForkedRoadViewBoundaryTriggers, _gAcropolisForkedRoadActionTriggers, NULL },
};

/// Room 2's logical-view to camera/image/sprite map, indexed by view minus one.
///
/// The first nine bytes are active, with logical views 3/5 and 7/8 sharing
/// mapped indices 5 and 8. The three retained zero bytes are outside the room's
/// reverse-search count, not a terminator. Borrowed while this overlay is loaded.
static u8 _gAcropolisForkedRoadRoom2ViewMap[12] = {
    1,
    2,
    5,
    4,
    5,
    6,
    8,
    8,
    9,
    0,
    0,
    0,
};

/// Room 3's logical-view to camera/image/sprite map, indexed by view minus one.
///
/// The first nine bytes are active, with the same mapping as room 2 but separate
/// writable storage. The last three bytes are retained zeros outside the room's
/// reverse-search count. Borrowed while this overlay is loaded; no terminator.
static u8 _gAcropolisForkedRoadRoom3ViewMap[12] = {
    1,
    2,
    5,
    4,
    5,
    6,
    8,
    8,
    9,
    0,
    0,
    0,
};

u8* gAcropolisForkedRoadViewMaps[3] = {
    gViewIdentityMap,
    _gAcropolisForkedRoadRoom2ViewMap,
    _gAcropolisForkedRoadRoom3ViewMap,
};

ViewCount gAcropolisForkedRoadViewCounts[3] = { 10, 9, 9 };

WorldCoordRoomLighting gAcropolisForkedRoadRoomLightingTable[3] = {
    { &_gAcropolisForkedRoadRoomLights, NULL },
    { &_gAcropolisForkedRoadRoomLights, NULL },
    { &_gAcropolisForkedRoadRoomLights, NULL },
};

enum {
    ACROPOLIS_FORKED_ROAD_FOUNTAIN_ARRIVAL_SOUND   = 0x51090003,
    ACROPOLIS_FORKED_ROAD_FOUNTAIN_DEPARTURE_SOUND = 0x51090002,
    ACROPOLIS_FORKED_ROAD_FOUNTAIN_BLOCKED_SOUND   = 0x51090006,
    ACROPOLIS_FORKED_ROAD_FOUNTAIN_WARP_MAP_FLAG   = 0x1EF,
};

DirectionWarpEntry gAcropolisForkedRoadWarpEntries[5] = {
    { { { .word = 1920 }, -3791, 1, 833 }, { 0, 0, 0, 0 }, { { .word = 1920 }, -3791, 1, 833 }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
    { { { .word = 3072 }, -261, 1, 1205 }, { 0, 0, 0, 0 }, { { .word = 3072 }, -261, 1, 1205 }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 8, DIRECTION_WARP_FLAG_NONE, GAME_FLAG_MAP_MARK_SECURITY_ROOM },
    { { { .word = 0 }, -5107, 1, -4030 }, { 0, 0, 0, 0 }, { { .word = 0 }, -5107, 1, -4030 }, { 0, 0, 0, 0 }, ACROPOLIS_FORKED_ROAD_FOUNTAIN_ARRIVAL_SOUND, ACROPOLIS_FORKED_ROAD_FOUNTAIN_DEPARTURE_SOUND, ACROPOLIS_FORKED_ROAD_FOUNTAIN_BLOCKED_SOUND, 4, DIRECTION_WARP_FLAG_NONE, ACROPOLIS_FORKED_ROAD_FOUNTAIN_WARP_MAP_FLAG },
    { { { .word = 1792 }, -4094, -470, 1325 }, { 0, 0, 0, 0 }, { { .word = 1792 }, -3764, -600, 797 }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_SCRIPTED_PLAYER, DIRECTION_WARP_MAP_FLAG_NONE },
    { { { .word = 3072 }, -261, 1, 1205 }, { 0, 0, 0, 0 }, { { .word = 3072 }, -261, 1, 1205 }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 3, DIRECTION_WARP_FLAG_NONE, GAME_FLAG_MAP_MARK_SECURITY_ROOM },
};

/// Room-space face normals of `_gAcropolisForkedRoadCollisionGrid`, with `ONE` per unit.
static SVECTOR _gAcropolisForkedRoadCollisionGridNormals[18] = {
#include "assets/acropolis_forked_road_collision_05630_normals.inc"
};

/// Signed room-coordinate vertex pool of `_gAcropolisForkedRoadCollisionGrid`.
static SVECTOR _gAcropolisForkedRoadCollisionGridVerts[118] = {
#include "assets/acropolis_forked_road_collision_05630_verts.inc"
};

/// Indexed quads and surface classes of `_gAcropolisForkedRoadCollisionGrid`.
static WorldCollisionGridFace _gAcropolisForkedRoadCollisionGridFaces[38] = {
#include "assets/acropolis_forked_road_collision_05630_faces.inc"
};

/// Packed face-index lists of `_gAcropolisForkedRoadCollisionGrid`, ending at -1.
static s16 _gAcropolisForkedRoadCollisionGridCells[244] = {
#include "assets/acropolis_forked_road_collision_05630_cells.inc"
};

enum {
    ACROPOLIS_FORKED_ROAD_GRID_CELLS_X = 6,
    ACROPOLIS_FORKED_ROAD_GRID_CELLS_Z = 4,
};

// Asset-include binding: i is an in-bounds halfword index into the cell-list pool.
#define GRID_CELL(i) (&_gAcropolisForkedRoadCollisionGridCells[i])
/// X-major directory of `_gAcropolisForkedRoadCollisionGrid`, indexed by x * 4 + z.
static s16* _gAcropolisForkedRoadCollisionGridTable[ACROPOLIS_FORKED_ROAD_GRID_CELLS_X * ACROPOLIS_FORKED_ROAD_GRID_CELLS_Z] = {
#include "assets/acropolis_forked_road_collision_05630_table.inc"
};
#undef GRID_CELL

/// Collision mesh shared by all three forked-road rooms.
///
/// Covers six X cells by four Z cells, each 4000 room-coordinate units wide,
/// with origin (-12350, -6900) in XZ. Face indices, normals and vertices refer
/// to the private asset pools above. Room setup binds the mutable view parent;
/// keep this overlay loaded while the grid and its pools are active.
static WorldCollisionGrid _gAcropolisForkedRoadCollisionGrid = {
    .viewCoord   = NULL,
    .normals     = _gAcropolisForkedRoadCollisionGridNormals,
    .vertices    = _gAcropolisForkedRoadCollisionGridVerts,
    .faces       = _gAcropolisForkedRoadCollisionGridFaces,
    .cellFaceIds = _gAcropolisForkedRoadCollisionGridTable,
    .xBias       = 12350,
    .zBias       = 6900,
    .cellCountX  = ACROPOLIS_FORKED_ROAD_GRID_CELLS_X,
    .cellCountZ  = ACROPOLIS_FORKED_ROAD_GRID_CELLS_Z,
    .cellSize    = 4000,
    .faceCount   = ARRAY_SIZE(_gAcropolisForkedRoadCollisionGridFaces),
};

/// Six directed view-change quads shared by the three forked-road rooms.
///
/// Pairs select logical views 2/3 and 2/4; parameter bytes are source/destination
/// views. Geometry uses whole room units and normals use `ONE`. Setup binds the
/// view coordinate, links and enables each record, including the final LAST
/// entry. Keep this mutable storage loaded until the boundary list is unlinked.
static WorldCollisionTrigger _gAcropolisForkedRoadViewBoundaryTriggers[6] = {
    { NULL, NULL, NULL, { -1905, 0, -353, 0 }, { { -509, 3168, 2239, 0 }, { 510, 3168, -2238, 0 }, { -509, -3168, 2239, 0 }, { 510, -3168, -2238, 0 } }, { 3996, 0, 909, 0 }, { 0, 0, ONE, 0 }, 3907, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -1712, 0, -149, 0 }, { { 526, 3168, -2286, 0 }, { -525, 3168, 2287, 0 }, { 526, -3168, -2286, 0 }, { -525, -3168, 2287, 0 } }, { -4002, 0, -920, 0 }, { 0, 0, ONE, 0 }, 3941, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -6711, 0, -2121, 0 }, { { 1938, 3168, 482, 0 }, { -1937, 3168, -481, 0 }, { 1938, -3168, 482, 0 }, { -1937, -3168, -481, 0 } }, { 989, 0, -3983, 0 }, { 0, 0, ONE, 0 }, 3736, 0, 4, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -6668, 0, -2514, 0 }, { { -1920, 3168, -512, 0 }, { 1920, 3168, 513, 0 }, { -1920, -3168, -512, 0 }, { 1920, -3168, 513, 0 } }, { -1058, 0, 3959, 0 }, { 0, 0, ONE, 0 }, 3736, 0, 2, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -3905, 0, -3010, 0 }, { { -943, 3168, 1067, 0 }, { 943, 3168, -1066, 0 }, { -943, -3168, 1067, 0 }, { 943, -3168, -1066, 0 } }, { 3088, 0, 2731, 0 }, { 0, 0, ONE, 0 }, 3472, 0, 2, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -3575, 0, -2822, 0 }, { { 1290, 3168, -1366, 0 }, { -1290, 3168, 1367, 0 }, { 1290, -3168, -1366, 0 }, { -1290, -3168, 1367, 0 } }, { -2990, 0, -2822, 0 }, { 0, 0, ONE, 0 }, 3683, 0, 4, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

// Action-1 control bit 8 selects descent; its parameter0 packs surface class and steps.
enum {
    ACROPOLIS_FORKED_ROAD_STAIR_DESCEND       = 0x100,
    ACROPOLIS_FORKED_ROAD_STAIR_SURFACE_CLASS = 4,
    ACROPOLIS_FORKED_ROAD_ASCENT_STEPS        = 4,
    ACROPOLIS_FORKED_ROAD_DESCENT_STEPS       = 2,
    ACROPOLIS_FORKED_ROAD_LOCK_RELEASE_ACTION = 1,
};

/// Seven interaction and automatic-action quads shared by all forked-road rooms.
///
/// Includes fountain/observatory/square warps, two stair climbs, CAP command 4
/// and room action 1 for the lock-release event. Warp parameter1 packs the
/// departure slot in its high nibble and arrival slot in its low nibble.
/// Stair parameters pack surface class/step count and yaw in 16-angle-unit steps.
/// Geometry uses room units and normals use `ONE`. Setup links and enables all
/// records through LAST; keep the mutable array loaded until the list is unlinked.
static WorldCollisionTrigger _gAcropolisForkedRoadActionTriggers[7] = {
    { NULL, NULL, NULL, { -5024, -64, -4192, 0 }, { { -896, 0, -768, 0 }, { 896, 0, -768, 0 }, { -896, 0, 768, 0 }, { 896, 0, 768, 0 } }, { 0, 4100, 0, 0 }, { 0, 0, ONE, 0 }, 1180, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_ACROPOLIS_FOUNTAIN, (3 << 4) | 2, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -192, -96, 1440, 0 }, { { -768, 0, -736, 0 }, { 768, 0, -736, 0 }, { -768, 0, 736, 0 }, { 768, 0, 736, 0 } }, { 0, 4106, 0, 0 }, { -ONE, 0, 0, 0 }, 1063, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_ACROPOLIS_OBSERVATORY, (2 << 4) | 4, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -3905, -128, 1039, 0 }, { { -433, 0, -633, 0 }, { 721, 0, -218, 0 }, { -720, 0, 219, 0 }, { 434, 0, 634, 0 } }, { 0, 4114, 0, 0 }, { 1189, 0, -3920, 0 }, 768, WORLD_COLLISION_TRIGGER_ACTION_FACING, (ACROPOLIS_FORKED_ROAD_STAIR_SURFACE_CLASS << 4) | ACROPOLIS_FORKED_ROAD_ASCENT_STEPS, 244, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1049, -96, -712, 0 }, { { -1285, 0, 644, 0 }, { -1287, 0, -1365, 0 }, { 232, 0, 613, 0 }, { 231, 0, -468, 0 } }, { 0, 4097, 0, 0 }, { 0, 0, ONE, 0 }, 1872, WORLD_COLLISION_TRIGGER_ACTION_CAP_WEAPON, 4, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -3922, -656, 1150, 0 }, { { -1191, -1520, -451, 0 }, { 1192, -1520, 452, 0 }, { -1191, 1520, -451, 0 }, { 1192, 1520, 452, 0 } }, { 1451, 0, -3834, 0 }, { 1380, 0, -3857, 0 }, 1982, WORLD_COLLISION_TRIGGER_ACTION_WARP | WORLD_COLLISION_TRIGGER_AUTOMATIC, GAME_AREA_ACROPOLIS_SQUARE, (1 << 4) | 6, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -4113, -512, 1439, 0 }, { { -983, 0, -519, 0 }, { 1087, 0, 270, 0 }, { -1087, 0, -270, 0 }, { 983, 0, 519, 0 } }, { 0, 4098, 0, 0 }, { -1568, 0, 3784, 0 }, 1115, WORLD_COLLISION_TRIGGER_ACTION_FACING | ACROPOLIS_FORKED_ROAD_STAIR_DESCEND | WORLD_COLLISION_TRIGGER_AUTOMATIC, (ACROPOLIS_FORKED_ROAD_STAIR_SURFACE_CLASS << 4) | ACROPOLIS_FORKED_ROAD_DESCENT_STEPS, 112, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -2336, -96, -512, 0 }, { { -256, 0, -2624, 0 }, { 1344, 0, -1600, 0 }, { -1344, 0, 1600, 0 }, { 256, 0, 2624, 0 } }, { 0, 4099, 0, 0 }, { 0, 0, ONE, 0 }, 2635, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, ACROPOLIS_FORKED_ROAD_LOCK_RELEASE_ACTION, 0, WORLD_COLLISION_TRIGGER_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Actor loading and spawn recipes for forked-road placement variant 2.
///
/// Selects entry 19 / file 19 in the base-30 family.
/// Each live entry spawns descriptor slot 0; `AREA_PLACEMENT_END` ends the list.
/// Keep the table and borrowed descriptors loaded through loading and spawning.
static AreaResource _gAcropolisForkedRoadVariant2Resources[2] = {
    { 19, 19, AREA_RESOURCE_FILE_GROUP_BASE_30, 0, { 0, 0 }, &gActor301900GrinningStrangerTaskDesc },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Actor loading and spawn recipes for forked-road placement variant 3.
///
/// Selects entry 10 / file 10 in base 40 and entry 7 / file 7 in base 30.
/// Each live entry spawns descriptor slot 0; `AREA_PLACEMENT_END` ends the list.
/// Keep the table and borrowed descriptors loaded through loading and spawning.
static AreaResource _gAcropolisForkedRoadVariant3Resources[3] = {
    { 10, 10, AREA_RESOURCE_FILE_GROUP_BASE_40, 0, { 0, 0 }, &gActor401000StrangerTaskDesc },
    { 7, 7, AREA_RESOURCE_FILE_GROUP_BASE_30, 0, { 0, 0 }, &gActor300700RatTaskDesc },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Actor loading and spawn recipes for forked-road placement variant 4.
///
/// Selects entry 55 / file 55 in the base-10 family.
/// Each live entry spawns descriptor slot 0; `AREA_PLACEMENT_END` ends the list.
/// Keep the table and borrowed descriptors loaded through loading and spawning.
static AreaResource _gAcropolisForkedRoadVariant4Resources[2] = {
    { 55, 55, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, gActor105500MaggotCaterpillarTasks },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Actor loading and spawn recipes for forked-road placement variant 7.
///
/// Selects entry 20 / file 20 in base 10 and entry 7 / file 7 in base 20.
/// Each live entry spawns descriptor slot 0; `AREA_PLACEMENT_END` ends the list.
/// Keep the table and borrowed descriptors loaded through loading and spawning.
static AreaResource _gAcropolisForkedRoadVariant7Resources[3] = {
    { 20, 20, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, gActor102000PawnGolemTasks },
    { 7, 7, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, &gActor200700RatTaskDesc },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Actor loading and spawn recipes for forked-road placement variant 8.
///
/// Selects entry 26 / file 26 in the base-10 family.
/// Each live entry spawns descriptor slot 0; `AREA_PLACEMENT_END` ends the list.
/// Keep the table and borrowed descriptors loaded through loading and spawning.
static AreaResource _gAcropolisForkedRoadVariant8Resources[2] = {
    { 26, 26, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &gActor02600MaggotCaterpillarBodyTask },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Actor loading and spawn recipes for forked-road placement variant 9.
///
/// Selects entry 18 / file 18 in the base-40 family.
/// Each live entry spawns descriptor slot 0; `AREA_PLACEMENT_END` ends the list.
/// Keep the table and borrowed descriptors loaded through loading and spawning.
static AreaResource _gAcropolisForkedRoadVariant9Resources[2] = {
    { 18, 18, AREA_RESOURCE_FILE_GROUP_BASE_40, 0, { 0, 0 }, &gActor401800OddStrangerTaskDesc },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Actor loading and spawn recipes for forked-road placement variant 10.
///
/// Selects entries 7 and 8, both using file 7 in the base-10 family.
/// Each live entry spawns descriptor slot 0; `AREA_PLACEMENT_END` ends the list.
/// Keep the table and borrowed descriptors loaded through loading and spawning.
static AreaResource _gAcropolisForkedRoadVariant10Resources[3] = {
    { 7, 7, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &gActor00700RatTaskDesc },
    { 8, 7, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &gActor00700MothTaskDesc },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Actor loading and spawn recipes for forked-road placement variant 11.
///
/// Selects entry 24 / file 24 in the base-10 family.
/// Each live entry spawns descriptor slot 0; `AREA_PLACEMENT_END` ends the list.
/// Keep the table and borrowed descriptors loaded through loading and spawning.
static AreaResource _gAcropolisForkedRoadVariant11Resources[2] = {
    { 24, 24, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, gActor102400Tasks },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Actor loading and spawn recipes for forked-road placement variant 12.
///
/// Selects entry 25 / file 25 in the base-10 family.
/// Each live entry spawns descriptor slot 0; `AREA_PLACEMENT_END` ends the list.
/// Keep the table and borrowed descriptors loaded through loading and spawning.
static AreaResource _gAcropolisForkedRoadVariant12Resources[2] = {
    { 25, 25, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, gActor102500Tasks },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Actor loading and spawn recipes for forked-road placement variant 13.
///
/// Selects entry 26 / file 26 in the base-10 family.
/// Each live entry spawns descriptor slot 0; `AREA_PLACEMENT_END` ends the list.
/// Keep the table and borrowed descriptors loaded through loading and spawning.
static AreaResource _gAcropolisForkedRoadVariant13Resources[2] = {
    { 26, 26, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &gActor02600MaggotCaterpillarBodyTask },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Actor loading and spawn recipes for forked-road placement variant 14.
///
/// Selects entry 37 / file 37 in the base-10 family.
/// Each live entry spawns descriptor slot 0; `AREA_PLACEMENT_END` ends the list.
/// Keep the table and borrowed descriptors loaded through loading and spawning.
static AreaResource _gAcropolisForkedRoadVariant14Resources[2] = {
    { 37, 37, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &gActor103700BatTaskDesc },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Actor loading and spawn recipes for forked-road placement variant 15.
///
/// Selects entry 15 / file 15 in the base-10 family.
/// Each live entry spawns descriptor slot 0; `AREA_PLACEMENT_END` ends the list.
/// Keep the table and borrowed descriptors loaded through loading and spawning.
static AreaResource _gAcropolisForkedRoadVariant15Resources[2] = {
    { 15, 15, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &gActor01500MindSucklerTaskDesc },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Actor loading and spawn recipes for forked-road placement variant 16.
///
/// Selects entry 38 / file 38 in the base-10 family.
/// Each live entry spawns descriptor slot 0; `AREA_PLACEMENT_END` ends the list.
/// Keep the table and borrowed descriptors loaded through loading and spawning.
static AreaResource _gAcropolisForkedRoadVariant16Resources[2] = {
    { 38, 38, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &gActor103800BlackBeetleTaskDesc },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Actor loading and spawn recipes for forked-road placement variant 20.
///
/// Selects entry 16 / file 16 in the base-10 family.
/// Each live entry spawns descriptor slot 0; `AREA_PLACEMENT_END` ends the list.
/// Keep the table and borrowed descriptors loaded through loading and spawning.
static AreaResource _gAcropolisForkedRoadVariant20Resources[2] = {
    { 16, 16, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &gActor101600ScavengerTaskDesc },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Actor loading and spawn recipes for forked-road placement variant 21.
///
/// Selects entries/files 46 and 47 in the base-10 family.
/// Each live entry spawns descriptor slot 0; `AREA_PLACEMENT_END` ends the list.
/// Keep the table and borrowed descriptors loaded through loading and spawning.
static AreaResource _gAcropolisForkedRoadVariant21Resources[3] = {
    { 46, 46, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &gActor104600SucklercephTaskDesc },
    { 47, 47, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &gActor104600SkullStalkerTaskDesc },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Actor loading and spawn recipes for forked-road placement variant 22.
///
/// Selects entries/files 70 and 71 in base 10, and 72 and 73 in base 20.
/// Each live entry spawns descriptor slot 0; `AREA_PLACEMENT_END` ends the list.
/// Keep the table and borrowed descriptors loaded through loading and spawning.
/// All four resources are loaded; the placement table selects only entries 70 and 72.
static AreaResource _gAcropolisForkedRoadVariant22Resources[5] = {
    { 70, 70, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, gActor107000SlouchTaskDescs },
    { 71, 71, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, gActor107000SucklercephTaskDescs },
    { 72, 72, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, &gActor207200CreepingStrangerTaskDesc },
    { 73, 73, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, &gActor207200SkullStalkerTaskDesc },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaVariant gAcropolisForkedRoadAreaVariants[24] = {
    { NULL, NULL },
    { NULL, NULL },
    { gMapAkropolisForkedRoadVariant2Placements, _gAcropolisForkedRoadVariant2Resources },
    { gMapAkropolisForkedRoadVariant3Placements, _gAcropolisForkedRoadVariant3Resources },
    { gMapAkropolisForkedRoadVariant4Placements, _gAcropolisForkedRoadVariant4Resources },
    { NULL, NULL },
    { NULL, NULL },
    { gMapAkropolisForkedRoadVariant7Placements, _gAcropolisForkedRoadVariant7Resources },
    { gMapAkropolisForkedRoadVariant8Placements, _gAcropolisForkedRoadVariant8Resources },
    { gMapAkropolisForkedRoadVariant9Placements, _gAcropolisForkedRoadVariant9Resources },
    { gMapAkropolisForkedRoadVariant10Placements, _gAcropolisForkedRoadVariant10Resources },
    { gMapAkropolisForkedRoadVariant11Placements, _gAcropolisForkedRoadVariant11Resources },
    { gMapAkropolisForkedRoadVariant12Placements, _gAcropolisForkedRoadVariant12Resources },
    { gMapAkropolisForkedRoadVariant13Placements, _gAcropolisForkedRoadVariant13Resources },
    { gMapAkropolisForkedRoadVariant14Placements, _gAcropolisForkedRoadVariant14Resources },
    { gMapAkropolisForkedRoadVariant15Placements, _gAcropolisForkedRoadVariant15Resources },
    { gMapAkropolisForkedRoadVariant16Placements, _gAcropolisForkedRoadVariant16Resources },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { gMapAkropolisForkedRoadVariant20Placements, _gAcropolisForkedRoadVariant20Resources },
    { gMapAkropolisForkedRoadVariant21Placements, _gAcropolisForkedRoadVariant21Resources },
    { gMapAkropolisForkedRoadVariant22Placements, _gAcropolisForkedRoadVariant22Resources },
    { NULL, NULL },
};

// Encoded 8-bit texture pages, palette rows and neutral RGB for the room sprites.
enum {
    ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_896              = getTPage(1, GPU_BLEND_AVERAGE, 896, 0),
    ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960              = getTPage(1, GPU_BLEND_AVERAGE, 960, 0),
    ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT           = getClut(0, 255),
    ACROPOLIS_FORKED_ROAD_SPRITE_SEMI_TRANSPARENT_CLUT = getClut(0, 256),
    ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR         = 128,
};

/// Decoded-image background with no cached sprites for mapped view 1.
///
/// The zero-count header is followed by `SPRITE_BATCH_END`; no source elements are read.
/// Ranges use source-element indices; retain the mutable list while this overlay
/// is loaded. The final record ends the list at `SPRITE_BATCH_END`.
static SpriteBatch _gAcropolisForkedRoadView1SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Screen-space source sprites for mapped view 2, in batch order.
///
/// Positions are pixels from the view centre and dimensions are pixel extents; depth is
/// an unscaled OT sorting value. Batches index elements, with no source sentinel.
/// Keep this writable array loaded while cached packets are allocated or drawn.
static SpriteSource _gAcropolisForkedRoadView2SpriteSources[85] = {
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 8, 16 } }, 152, -32, 1212, { .fields = { 104, 240 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 16, 16 } }, 104, -32, 1237, { .fields = { 112, 240 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_896, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 16, 24 } }, 104, -56, 1237, { .fields = { 112, 216 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 8, 24 } }, 112, -80, 1237, { .fields = { 48, 136 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 16, 16 } }, 120, -72, 1222, { .fields = { 72, 240 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_896, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 16, 24 } }, 120, -56, 1222, { .fields = { 96, 64 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_896, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 24, 16 } }, 136, -72, 1212, { .fields = { 80, 200 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 24, 24 } }, 136, -56, 1212, { .fields = { 56, 136 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 40, 40 } }, 120, -24, 1350, { .fields = { 80, 80 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 40, 40 } }, 120, 16, 1375, { .fields = { 80, 120 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_896, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 32, 8 } }, 128, 56, 1375, { .fields = { 40, 144 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 16, 40 } }, 104, 16, 1400, { .fields = { 72, 0 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 16, 40 } }, 104, -24, 1375, { .fields = { 72, 200 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 8, 40 } }, 96, -24, 1400, { .fields = { 120, 80 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 8, 40 } }, 96, 16, 1400, { .fields = { 80, 40 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_896, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 8, 16 } }, 88, 40, 1425, { .fields = { 96, 88 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_896, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 8, 8 } }, 104, -40, 1312, { .fields = { 96, 152 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 16, 32 } }, 112, -64, 1312, { .fields = { 64, 72 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 16, 32 } }, 128, -64, 1375, { .fields = { 32, 0 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 16, 16 } }, 128, -32, 1375, { .fields = { 88, 240 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 8, 8 } }, -128, 40, 400, { .fields = { 0, 192 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 40, 40 } }, -160, 80, 400, { .fields = { 88, 200 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 40, 40 } }, -120, 80, 400, { .fields = { 88, 0 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_896, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 40, 32 } }, -160, 48, 400, { .fields = { 112, 64 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 40, 32 } }, -120, 48, 400, { .fields = { 0, 32 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_896, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 40, 16 } }, -120, 32, 400, { .fields = { 56, 48 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_896, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 8, 16 } }, -80, 48, 400, { .fields = { 72, 144 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 8, 8 } }, -8, 56, 450, { .fields = { 40, 32 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 8, 8 } }, 0, 80, 450, { .fields = { 8, 96 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_896, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 8, 8 } }, -104, 104, 450, { .fields = { 104, 184 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 40, 40 } }, -40, 80, 450, { .fields = { 48, 160 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_896, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 16, 16 } }, 0, 104, 450, { .fields = { 64, 128 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_896, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 16, 16 } }, 0, 64, 450, { .fields = { 88, 16 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 40, 40 } }, -80, 80, 450, { .fields = { 88, 40 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_896, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 32, 16 } }, -112, 80, 450, { .fields = { 72, 0 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_896, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 24, 16 } }, -104, 64, 450, { .fields = { 104, 240 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_896, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 40, 16 } }, -80, 64, 450, { .fields = { 64, 240 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_896, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 24, 16 } }, -64, 48, 450, { .fields = { 80, 136 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_896, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 24, 16 } }, -40, 48, 450, { .fields = { 80, 104 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_896, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 40, 16 } }, -40, 64, 450, { .fields = { 56, 32 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_896, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 16, 24 } }, -96, 96, 450, { .fields = { 96, 160 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_896, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 24, 16 } }, 120, 104, 425, { .fields = { 80, 184 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_896, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 32, 32 } }, 88, 88, 425, { .fields = { 104, 0 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_896, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 24, 24 } }, 64, 96, 425, { .fields = { 88, 216 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_896, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 24, 16 } }, 40, 104, 425, { .fields = { 80, 120 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 32, 24 } }, 8, 96, 425, { .fields = { 0, 232 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 40, 40 } }, -136, 80, 455, { .fields = { 88, 160 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 40, 24 } }, -136, 56, 455, { .fields = { 32, 232 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_896, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 8, 16 } }, -144, 64, 455, { .fields = { 72, 80 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 8, 40 } }, -144, 80, 455, { .fields = { 120, 120 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_896, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 8, 8 } }, -96, 64, 500, { .fields = { 104, 88 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_896, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 32, 8 } }, -160, 48, 700, { .fields = { 40, 152 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_896, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 32, 16 } }, -160, 56, 650, { .fields = { 56, 216 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_896, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 32, 16 } }, -160, 72, 600, { .fields = { 56, 16 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_896, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 32, 32 } }, -160, 88, 500, { .fields = { 104, 128 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_896, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 32, 16 } }, -128, 56, 650, { .fields = { 40, 80 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_896, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 32, 16 } }, -128, 72, 600, { .fields = { 40, 0 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_896, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 32, 32 } }, -128, 88, 500, { .fields = { 96, 32 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_896, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 32, 16 } }, -96, 72, 600, { .fields = { 48, 184 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_896, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 32, 32 } }, -96, 88, 500, { .fields = { 104, 96 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 32, 32 } }, -64, 88, 500, { .fields = { 48, 104 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 16, 8 } }, -64, 80, 600, { .fields = { 24, 64 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_896, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 24, 24 } }, -32, 96, 500, { .fields = { 104, 192 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_896, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 16, 16 } }, -8, 104, 500, { .fields = { 80, 80 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 24, 8 } }, 8, 112, 500, { .fields = { 48, 32 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 24, 32 } }, -160, 72, 1050, { .fields = { 48, 200 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 24, 32 } }, -160, 40, 1050, { .fields = { 48, 0 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_896, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 24, 32 } }, -160, 8, 1050, { .fields = { 112, 160 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 24, 32 } }, -160, -24, 1050, { .fields = { 24, 200 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 24, 32 } }, -160, -56, 1050, { .fields = { 8, 0 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 24, 32 } }, -160, -88, 1050, { .fields = { 0, 200 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_896, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 16, 16 } }, -160, -104, 1050, { .fields = { 80, 152 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 8, 8 } }, -48, 0, 2750, { .fields = { 16, 96 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 40, 32 } }, -160, 24, 2475, { .fields = { 8, 104 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 40, 32 } }, -120, 24, 2475, { .fields = { 24, 72 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 40, 32 } }, -80, 24, 2475, { .fields = { 40, 40 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 40, 32 } }, -40, 24, 2475, { .fields = { 8, 168 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_896, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 40, 16 } }, -160, 8, 2500, { .fields = { 40, 96 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_896, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 40, 16 } }, -120, 8, 2500, { .fields = { 56, 64 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_896, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 40, 16 } }, -80, 8, 2500, { .fields = { 40, 200 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_896, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 40, 16 } }, -40, 8, 2500, { .fields = { 40, 112 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_896, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 40, 8 } }, -40, 0, 2750, { .fields = { 40, 160 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 40, 32 } }, 0, 24, 2475, { .fields = { 8, 136 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_896, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 40, 16 } }, 0, 8, 2500, { .fields = { 56, 168 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_896, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 40, 8 } }, 0, 0, 2750, { .fields = { 48, 232 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
};

/// Decoded-image background and ten sprite ranges for mapped view 2.
///
/// The ranges partition all 85 source elements; the zero-count header and end record select no sprites.
/// Ranges use source-element indices; retain the mutable list while this overlay
/// is loaded. The final record ends the list at `SPRITE_BATCH_END`.
static SpriteBatch _gAcropolisForkedRoadView2SpriteBatches[12] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 8, 0, 0, { 6, 0 } },
    { 8, 8, 0, 0, { 1, 0 } },
    { 16, 4, 0, 0, { 8, 0 } },
    { 20, 7, 0, 0, { 0, 0 } },
    { 27, 14, 0, 0, { 5, 0 } },
    { 41, 5, 0, 0, { 2, 0 } },
    { 46, 4, 0, 0, { 9, 0 } },
    { 50, 15, 0, 0, { 4, 0 } },
    { 65, 7, 0, 0, { 7, 0 } },
    { 72, 13, 0, 0, { 3, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Screen-space source sprites shared by mapped views 3 and 12.
///
/// Positions are pixels from the view centre and dimensions are pixel extents; depth is
/// an unscaled OT sorting value. Batches index elements, with no source sentinel.
/// Keep this writable array loaded while cached packets are allocated or drawn.
static SpriteSource _gAcropolisForkedRoadViews3And12SpriteSources[6] = {
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 24, 40 } }, -160, 64, 937, { .fields = { 104, 0 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 24, 40 } }, -160, 24, 937, { .fields = { 104, 40 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 24, 40 } }, -160, -16, 937, { .fields = { 104, 80 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 8, 40 } }, -160, -56, 937, { .fields = { 120, 120 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 8, 32 } }, -160, -88, 937, { .fields = { 120, 160 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 16, 8 } }, -152, -24, 937, { .fields = { 112, 192 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
};

/// Decoded-image background and one sprite range shared by mapped views 3 and 12.
///
/// The range selects all six shared source elements, after a zero-count header.
/// Ranges use source-element indices; retain the mutable list while this overlay
/// is loaded. The final record ends the list at `SPRITE_BATCH_END`.
static SpriteBatch _gAcropolisForkedRoadViews3And12SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gAcropolisForkedRoadViews3And12SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Decoded-image background with no cached sprites for mapped view 4.
///
/// The zero-count header is followed by `SPRITE_BATCH_END`; no source elements are read.
/// Ranges use source-element indices; retain the mutable list while this overlay
/// is loaded. The final record ends the list at `SPRITE_BATCH_END`.
static SpriteBatch _gAcropolisForkedRoadView4SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Screen-space source sprites for mapped view 5, in batch order.
///
/// Positions are pixels from the view centre and dimensions are pixel extents; depth is
/// an unscaled OT sorting value. Batches index elements, with no source sentinel.
/// Keep this writable array loaded while cached packets are allocated or drawn.
static SpriteSource _gAcropolisForkedRoadView5SpriteSources[6] = {
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 24, 40 } }, -160, 64, 937, { .fields = { 104, 0 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 24, 40 } }, -160, 24, 937, { .fields = { 104, 40 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 24, 40 } }, -160, -16, 937, { .fields = { 104, 80 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 8, 40 } }, -160, -56, 937, { .fields = { 120, 120 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 8, 32 } }, -160, -88, 937, { .fields = { 120, 160 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 16, 8 } }, -152, -24, 937, { .fields = { 112, 192 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
};

/// Decoded-image background and one sprite range for mapped view 5.
///
/// The range selects all six source elements, after a zero-count header.
/// Ranges use source-element indices; retain the mutable list while this overlay
/// is loaded. The final record ends the list at `SPRITE_BATCH_END`.
static SpriteBatch _gAcropolisForkedRoadView5SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gAcropolisForkedRoadView5SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Decoded-image background with no cached sprites for mapped view 6.
///
/// The zero-count header is followed by `SPRITE_BATCH_END`; no source elements are read.
/// Ranges use source-element indices; retain the mutable list while this overlay
/// is loaded. The final record ends the list at `SPRITE_BATCH_END`.
static SpriteBatch _gAcropolisForkedRoadView6SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_acropolis_forked_road_80183AD8[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Opaque and semitransparent source sprites shared by mapped views 7 and 8.
///
/// Positions are pixels from the view centre and dimensions are pixel extents; depth is
/// an unscaled OT sorting value. Batches index elements, with no source sentinel.
/// Keep this writable array loaded while cached packets are allocated or drawn.
static SpriteSource _gAcropolisForkedRoadViews7And8SpriteSources[58] = {
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 8, 8 } }, 56, -96, 750, { .fields = { 40, 248 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 16, 16 } }, -56, -104, 750, { .fields = { 48, 96 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 32, 32 } }, -72, -88, 750, { .fields = { 96, 96 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 32, 32 } }, -40, -88, 750, { .fields = { 96, 0 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 32, 32 } }, -8, -88, 750, { .fields = { 96, 32 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 32, 32 } }, 24, -88, 750, { .fields = { 96, 192 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 16, 32 } }, 56, -88, 750, { .fields = { 112, 128 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 32, 16 } }, -72, -56, 750, { .fields = { 32, 80 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 32, 16 } }, -40, -56, 750, { .fields = { 32, 64 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 32, 16 } }, -8, -56, 750, { .fields = { 32, 48 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 32, 16 } }, 24, -56, 750, { .fields = { 32, 32 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 16, 16 } }, 56, -56, 750, { .fields = { 56, 216 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 32, 32 } }, -40, -120, 750, { .fields = { 64, 0 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 32, 32 } }, -8, -120, 750, { .fields = { 64, 32 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 32, 32 } }, 24, -120, 750, { .fields = { 64, 64 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 16, 16 } }, -48, -40, 1125, { .fields = { 80, 240 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 16, 16 } }, 32, -40, 1125, { .fields = { 48, 16 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 24, 24 } }, -48, -64, 1125, { .fields = { 56, 128 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 24, 24 } }, 24, -64, 1125, { .fields = { 48, 192 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 16, 16 } }, -24, -80, 1125, { .fields = { 48, 112 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 16, 16 } }, 8, -80, 1125, { .fields = { 48, 0 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 16, 16 } }, -8, -80, 1125, { .fields = { 64, 240 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 16, 8 } }, -40, -72, 1125, { .fields = { 48, 248 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 16, 8 } }, -24, -64, 1125, { .fields = { 48, 184 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 16, 8 } }, 24, -72, 1125, { .fields = { 24, 104 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 16, 8 } }, 8, -64, 1125, { .fields = { 32, 192 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_SEMI_TRANSPARENT_CLUT, { .fields = { 8, 8 } }, -40, -88, 1178, { .fields = { 40, 184 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_SEMI_TRANSPARENT_CLUT, { .fields = { 8, 8 } }, 32, -88, 1178, { .fields = { 40, 160 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_SEMI_TRANSPARENT_CLUT, { .fields = { 8, 8 } }, -32, -80, 1185, { .fields = { 40, 168 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_SEMI_TRANSPARENT_CLUT, { .fields = { 8, 8 } }, -24, -80, 1197, { .fields = { 64, 232 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_SEMI_TRANSPARENT_CLUT, { .fields = { 8, 8 } }, 16, -80, 1197, { .fields = { 48, 160 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_SEMI_TRANSPARENT_CLUT, { .fields = { 8, 8 } }, 24, -80, 1195, { .fields = { 40, 176 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_SEMI_TRANSPARENT_CLUT, { .fields = { 8, 8 } }, -32, -72, 1195, { .fields = { 40, 200 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_SEMI_TRANSPARENT_CLUT, { .fields = { 8, 8 } }, -24, -72, 1200, { .fields = { 40, 240 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_SEMI_TRANSPARENT_CLUT, { .fields = { 8, 8 } }, 16, -72, 1200, { .fields = { 40, 232 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_SEMI_TRANSPARENT_CLUT, { .fields = { 8, 8 } }, 24, -72, 1195, { .fields = { 40, 208 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_SEMI_TRANSPARENT_CLUT, { .fields = { 8, 8 } }, -32, -64, 1200, { .fields = { 32, 120 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_SEMI_TRANSPARENT_CLUT, { .fields = { 8, 8 } }, 24, -64, 1200, { .fields = { 32, 112 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_SEMI_TRANSPARENT_CLUT, { .fields = { 16, 16 } }, 40, -64, 1175, { .fields = { 48, 168 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_SEMI_TRANSPARENT_CLUT, { .fields = { 8, 16 } }, 32, -64, 1195, { .fields = { 56, 152 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_SEMI_TRANSPARENT_CLUT, { .fields = { 16, 16 } }, -48, -64, 1175, { .fields = { 48, 232 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_SEMI_TRANSPARENT_CLUT, { .fields = { 16, 16 } }, -48, -80, 1175, { .fields = { 40, 128 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_SEMI_TRANSPARENT_CLUT, { .fields = { 16, 16 } }, -32, -96, 1181, { .fields = { 40, 144 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_SEMI_TRANSPARENT_CLUT, { .fields = { 16, 16 } }, -16, -96, 1187, { .fields = { 40, 216 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_SEMI_TRANSPARENT_CLUT, { .fields = { 16, 16 } }, 16, -96, 1181, { .fields = { 32, 0 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_SEMI_TRANSPARENT_CLUT, { .fields = { 16, 16 } }, 0, -96, 1187, { .fields = { 32, 16 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_SEMI_TRANSPARENT_CLUT, { .fields = { 8, 16 } }, 40, -80, 1175, { .fields = { 40, 96 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_SEMI_TRANSPARENT_CLUT, { .fields = { 8, 16 } }, 32, -80, 1195, { .fields = { 40, 112 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_SEMI_TRANSPARENT_CLUT, { .fields = { 16, 8 } }, -16, -80, 1200, { .fields = { 64, 152 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_SEMI_TRANSPARENT_CLUT, { .fields = { 16, 8 } }, 0, -80, 1200, { .fields = { 24, 96 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_SEMI_TRANSPARENT_CLUT, { .fields = { 32, 32 } }, -64, -80, 757, { .fields = { 64, 160 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_SEMI_TRANSPARENT_CLUT, { .fields = { 32, 32 } }, -32, -80, 756, { .fields = { 64, 96 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_SEMI_TRANSPARENT_CLUT, { .fields = { 32, 32 } }, 0, -80, 757, { .fields = { 80, 128 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_SEMI_TRANSPARENT_CLUT, { .fields = { 32, 32 } }, 32, -80, 757, { .fields = { 96, 224 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_SEMI_TRANSPARENT_CLUT, { .fields = { 24, 24 } }, -56, -104, 750, { .fields = { 72, 216 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_SEMI_TRANSPARENT_CLUT, { .fields = { 32, 32 } }, -32, -112, 762, { .fields = { 96, 64 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_SEMI_TRANSPARENT_CLUT, { .fields = { 32, 32 } }, 0, -112, 762, { .fields = { 96, 160 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_SEMI_TRANSPARENT_CLUT, { .fields = { 24, 24 } }, 32, -104, 751, { .fields = { 72, 192 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, SPRITE_SOURCE_SEMI_TRANSPARENT },
};

/// Decoded-image background and four sprite ranges shared by mapped views 7 and 8.
///
/// The ranges partition all 58 shared source elements, after a zero-count header.
/// Ranges use source-element indices; retain the mutable list while this overlay
/// is loaded. The final record ends the list at `SPRITE_BATCH_END`.
static SpriteBatch _gAcropolisForkedRoadViews7And8SpriteBatches[6] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 15, 0, 0, { 3, 0 } },
    { 15, 11, 0, 0, { 0, 0 } },
    { 26, 24, 0, 0, { 2, 0 } },
    { 50, 8, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Decoded-image background with no cached sprites for mapped view 9.
///
/// The zero-count header is followed by `SPRITE_BATCH_END`; no source elements are read.
/// Ranges use source-element indices; retain the mutable list while this overlay
/// is loaded. The final record ends the list at `SPRITE_BATCH_END`.
static SpriteBatch _gAcropolisForkedRoadView9SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Decoded-image background with no cached sprites for mapped view 10.
///
/// The zero-count header is followed by `SPRITE_BATCH_END`; no source elements are read.
/// Ranges use source-element indices; retain the mutable list while this overlay
/// is loaded. The final record ends the list at `SPRITE_BATCH_END`.
static SpriteBatch _gAcropolisForkedRoadView10SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Screen-space source sprites for mapped view 11, in batch order.
///
/// Retained resource slot; none of the authored room maps selects index 11.
/// Positions are pixels from the view centre and dimensions are pixel extents; depth is
/// an unscaled OT sorting value. Batches index elements, with no source sentinel.
/// Keep this writable array loaded while cached packets are allocated or drawn.
static SpriteSource _gAcropolisForkedRoadView11SpriteSources[62] = {
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 16, 40 } }, -56, 80, 1075, { .fields = { 112, 160 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 16, 40 } }, -40, 80, 1062, { .fields = { 112, 120 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 24, 40 } }, -24, 80, 1050, { .fields = { 104, 80 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 24, 32 } }, 0, 88, 1037, { .fields = { 88, 0 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 32, 32 } }, 24, 88, 1025, { .fields = { 72, 80 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 32, 24 } }, 56, 96, 1012, { .fields = { 32, 160 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 32, 24 } }, 88, 96, 1000, { .fields = { 32, 208 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 40, 16 } }, 120, 104, 987, { .fields = { 8, 88 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_896, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 32, 8 } }, 128, 72, 1037, { .fields = { 120, 56 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 32, 16 } }, 128, 80, 1025, { .fields = { 16, 0 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 32, 24 } }, 128, 96, 1000, { .fields = { 24, 24 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_896, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 32, 8 } }, 96, 64, 1075, { .fields = { 120, 48 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_896, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 32, 8 } }, 96, 72, 1062, { .fields = { 120, 64 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 32, 16 } }, 96, 80, 1050, { .fields = { 0, 200 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 32, 24 } }, 96, 96, 1025, { .fields = { 40, 184 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_896, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 32, 16 } }, 64, 96, 1037, { .fields = { 120, 32 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 32, 16 } }, 64, 80, 1062, { .fields = { 8, 184 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_896, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 32, 8 } }, 64, 72, 1075, { .fields = { 120, 80 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_896, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 32, 8 } }, 64, 64, 1087, { .fields = { 120, 72 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 32, 8 } }, 32, 64, 1100, { .fields = { 24, 248 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 32, 8 } }, 32, 72, 1087, { .fields = { 56, 24 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 32, 16 } }, 32, 80, 1075, { .fields = { 0, 216 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_896, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 32, 16 } }, 32, 96, 1050, { .fields = { 120, 16 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 16, 8 } }, 32, 56, 1112, { .fields = { 56, 104 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 16, 8 } }, 16, 56, 1125, { .fields = { 40, 128 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 32, 8 } }, 0, 64, 1112, { .fields = { 0, 176 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_896, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 32, 8 } }, 0, 72, 1100, { .fields = { 112, 104 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 32, 16 } }, 0, 80, 1087, { .fields = { 0, 160 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_896, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 24, 8 } }, 8, 96, 1062, { .fields = { 120, 8 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_896, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 32, 8 } }, -32, 64, 1125, { .fields = { 112, 112 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_896, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 32, 8 } }, -32, 72, 1112, { .fields = { 112, 0 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 16, 8 } }, -48, 64, 1137, { .fields = { 32, 16 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 32, 16 } }, -32, 80, 1100, { .fields = { 0, 232 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 24, 8 } }, -56, 72, 1125, { .fields = { 0, 248 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 24, 16 } }, -56, 80, 1112, { .fields = { 24, 72 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 8, 8 } }, 112, 40, 925, { .fields = { 48, 72 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 32, 40 } }, 128, 48, 950, { .fields = { 96, 40 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 24, 16 } }, 136, 32, 950, { .fields = { 32, 232 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 16, 40 } }, 112, 48, 937, { .fields = { 112, 0 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 8, 16 } }, 104, 48, 925, { .fields = { 32, 128 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 32, 24 } }, 128, 40, 975, { .fields = { 64, 160 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 24, 24 } }, 128, 64, 975, { .fields = { 72, 184 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 32, 24 } }, 96, 64, 975, { .fields = { 64, 208 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 16, 16 } }, 104, 88, 975, { .fields = { 96, 184 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 32, 32 } }, 96, 32, 975, { .fields = { 96, 200 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 24, 16 } }, 72, 48, 975, { .fields = { 16, 144 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 32, 24 } }, 64, 64, 975, { .fields = { 56, 232 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 8, 8 } }, 64, 32, 1375, { .fields = { 96, 112 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 16, 32 } }, 48, 64, 1312, { .fields = { 96, 152 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 16, 32 } }, 48, 32, 1312, { .fields = { 96, 120 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 40, 24 } }, 8, 8, 1250, { .fields = { 48, 0 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 40, 24 } }, 8, -16, 1250, { .fields = { 56, 32 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 24, 24 } }, 24, -40, 1250, { .fields = { 72, 136 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 24, 16 } }, 24, -56, 1250, { .fields = { 8, 128 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 40, 24 } }, 48, 8, 1312, { .fields = { 56, 112 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 40, 24 } }, 48, -16, 1312, { .fields = { 56, 56 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 40, 24 } }, 48, -40, 1312, { .fields = { 88, 232 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 40, 24 } }, 48, -64, 1312, { .fields = { 16, 104 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 32, 24 } }, 88, -40, 1375, { .fields = { 24, 48 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 32, 24 } }, 88, -16, 1375, { .fields = { 40, 136 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 24, 24 } }, 88, 8, 1375, { .fields = { 48, 80 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
    { ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960, ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT, { .fields = { 16, 8 } }, 96, -48, 1375, { .fields = { 96, 32 } }, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR, 0 },
};

/// Decoded-image background and five sprite ranges for mapped view 11.
///
/// Retained resource slot; none of the authored room maps selects index 11.
/// The ranges partition all 62 source elements, after a zero-count header.
/// Ranges use source-element indices; retain the mutable list while this overlay
/// is loaded. The final record ends the list at `SPRITE_BATCH_END`.
static SpriteBatch _gAcropolisForkedRoadView11SpriteBatches[7] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 8, 0, 0, { 0, 0 } },
    { 8, 27, 0, 0, { 3, 0 } },
    { 35, 5, 0, 0, { 2, 0 } },
    { 40, 7, 0, 0, { 4, 0 } },
    { 47, 15, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_acropolis_forked_road_801844D0[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView gAcropolisForkedRoadSpriteViews[12] = {
    { { .empty = _gAcropolisForkedRoadView1SpriteBatches }, _gAcropolisForkedRoadView1SpriteBatches, NULL },
    { { .elements = _gAcropolisForkedRoadView2SpriteSources }, _gAcropolisForkedRoadView2SpriteBatches, NULL },
    { { .elements = _gAcropolisForkedRoadViews3And12SpriteSources }, _gAcropolisForkedRoadViews3And12SpriteBatches, NULL },
    { { .empty = _gAcropolisForkedRoadView4SpriteBatches }, _gAcropolisForkedRoadView4SpriteBatches, NULL },
    { { .elements = _gAcropolisForkedRoadView5SpriteSources }, _gAcropolisForkedRoadView5SpriteBatches, NULL },
    { { .empty = _gAcropolisForkedRoadView6SpriteBatches }, _gAcropolisForkedRoadView6SpriteBatches, NULL },
    { { .elements = _gAcropolisForkedRoadViews7And8SpriteSources }, _gAcropolisForkedRoadViews7And8SpriteBatches, NULL },
    { { .elements = _gAcropolisForkedRoadViews7And8SpriteSources }, _gAcropolisForkedRoadViews7And8SpriteBatches, NULL },
    { { .empty = _gAcropolisForkedRoadView9SpriteBatches }, _gAcropolisForkedRoadView9SpriteBatches, NULL },
    { { .empty = _gAcropolisForkedRoadView10SpriteBatches }, _gAcropolisForkedRoadView10SpriteBatches, NULL },
    { { .elements = _gAcropolisForkedRoadView11SpriteSources }, _gAcropolisForkedRoadView11SpriteBatches, NULL },
    { { .elements = _gAcropolisForkedRoadViews3And12SpriteSources }, _gAcropolisForkedRoadViews3And12SpriteBatches, NULL },
};

/// Point lights for model shading in every view of the forked road.
///
/// Shared by the three room entries. Positions and inner/outer falloff radii
/// use integer world units; RGB intensities have 12 fractional bits (`ONE` is 1.0).
/// Runtime updates the transform parents, composition caches and attenuation.
/// The room-light collection borrows this array while the overlay is loaded.
static WorldCoordPointLight _gAcropolisForkedRoadPointLights[] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -5613, -698, -1760 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2703, 2621, 2621 }, { 0, 0 } }, 109, 4700 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -8121, -599, -1760 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2703, 2621, 2621 }, { 0, 0 } }, 701, 3078 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -6800, -720, -1259 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2785, 2703, 2621 }, { 0, 0 } }, 759, 3078 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 1515, -1950, 1250 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3031, 3194, 3358 }, { 0, 0 } }, 10, 6500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -2069, -1843, 122 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2262, 2259, 2205 }, { 0, 0 } }, 476, 2458 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -3075, -681, 256 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2703, 2621, 2621 }, { 0, 0 } }, 538, 2689 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -3810, -80, 660 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2703, 2621, 2621 }, { 0, 0 } }, 100, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -5010, -80, -2723 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2949, 2949, 2867 }, { 0, 0 } }, 200, 4200 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -5010, -421, -2648 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2949, 2949, 2867 }, { 0, 0 } }, 602, 4200 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -5010, -80, -3010 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3031, 3031, 2949 }, { 0, 0 } }, 200, 4500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -390, -3810, -2720 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2949, 2949, 2867 }, { 0, 0 } }, 100, 8500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -390, -3810, -2720 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2949, 2949, 2867 }, { 0, 0 } }, 100, 8500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -390, -3810, -2720 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2785, 2785, 2703 }, { 0, 0 } }, 100, 5000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -260, -2410, 1040 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1310, 1392, 1474 }, { 0, 0 } }, 150, 2800 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -260, -2410, 1040 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1310, 1392, 1474 }, { 0, 0 } }, 150, 2800 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -260, -2410, 1040 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1310, 1392, 1474 }, { 0, 0 } }, 150, 2800 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -909, -1343, -2901 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2905, 2885, 2862 }, { 0, 0 } }, 785, 5481 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 67, -1140, -1049 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3075, 3055, 3047 }, { 0, 0 } }, 618, 2345 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -2991, -841, -2090 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2621, 2539, 2539 }, { 0, 0 } }, 300, 5493 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -1130, -1886, 1241 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2887, 2856, 2841 }, { 0, 0 } }, 894, 3098 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -1040, -80, 1860 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2703, 2621, 2621 }, { 0, 0 } }, 100, 3500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -1040, -80, 1860 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2539, 2457, 2457 }, { 0, 0 } }, 100, 4200 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 5690, -4590, 1250 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3031, 3194, 3358 }, { 0, 0 } }, 10, 6500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 3700, -3420, 1250 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3031, 3194, 3358 }, { 0, 0 } }, 10, 6500 },
};

/// Point-light collection shared by the forked road's three room entries.
///
/// Borrows all 24 mutable point lights; directional and cone arrays are absent.
/// Lighting updates their view parents, composed transforms and attenuation.
/// Keep this overlay loaded while the collection is borrowed.
static WorldCoordRoomLights _gAcropolisForkedRoadRoomLights = {
    .directionalLightCount = 0,
    .directionalLights     = NULL,
    .pointLightCount       = ARRAY_SIZE(_gAcropolisForkedRoadPointLights),
    .pointLights           = _gAcropolisForkedRoadPointLights,
    .coneLightCount        = 0,
    .coneLights            = NULL,
};

ViewCamera gAcropolisForkedRoadViewCameras[12] = {
    { { { { ONE, 0, 0 }, { 0, 0, -ONE }, { 0, ONE, 0 } }, { 2130, 0x61A8, 630 } }, 541 },
    { { { { 1721, 0, 3716 }, { 310, 4081, -143 }, { -3703, 342, 1715 } }, { -849, 1600, 2900 } }, 235 },
    { { { { 1174, 0, -3924 }, { -167, 4092, -50 }, { 3920, 174, 1173 } }, { 5150, 1500, 1530 } }, 235 },
    { { { { -3906, 0, 1229 }, { 110, 4079, 349 }, { -1225, 366, -3891 } }, { 3990, 1330, -439 } }, 257 },
    { { { { 1174, 0, -3924 }, { -167, 4092, -50 }, { 3920, 174, 1173 } }, { 5150, 1500, 1530 } }, 235 },
    { { { { -662, 0, 4041 }, { 2151, 3467, 352 }, { -3421, 2180, -561 } }, { -3590, 3140, 350 } }, 257 },
    { { { { -3, 0, -4095 }, { 199, 4091, 0 }, { 4091, -199, -3 } }, { 2900, 1240, -1259 } }, 235 },
    { { { { -3, 0, -4095 }, { 199, 4091, 0 }, { 4091, -199, -3 } }, { 2900, 1240, -1259 } }, 235 },
    { { { { 587, 0, -4053 }, { -2658, 3092, -385 }, { 3060, 2686, 443 } }, { 1140, 2057, 1292 } }, 235 },
    { { { { -1274, 0, -3892 }, { -708, 4027, 232 }, { 3827, 746, -1253 } }, { 8190, 1950, 430 } }, 297 },
    { { { { -2176, 0, -3469 }, { 153, 4091, -96 }, { 3466, -181, -2174 } }, { 4930, 1460, 430 } }, 297 },
    { { { { 1174, 0, -3924 }, { -167, 4092, -50 }, { 3920, 174, 1173 } }, { 5150, 1500, 1530 } }, 235 },
};

enum {
    ACROPOLIS_FORKED_ROAD_RETURN_VIBRATION_WAIT_FRAMES = 2,
    ACROPOLIS_FORKED_ROAD_RETURN_VIBRATION_LOOP_COUNT  = 16,
};

/// Six-step on/off-motor vibration script for the return movie path.
///
/// Reads segments 0 and 1 of the return ride's segment table: a two-frame motor
/// hold, a two-frame WAIT, then a one-frame hold. LOOP/JUMP repeat from step 0
/// until the counter reaches zero; step 5 stops the lane. The variable motor
/// lane stays stopped; segment intensities and its initial STOP operand are ignored.
/// Commands are borrowed
/// until the adopted vibration task is torn down; keep this overlay loaded.
static PadScriptCmd _gAcropolisForkedRoadReturnMovieVibrationCommands[6] = {
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 1) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_WAIT, ACROPOLIS_FORKED_ROAD_RETURN_VIBRATION_WAIT_FRAMES), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 1), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_LOOP, ACROPOLIS_FORKED_ROAD_RETURN_VIBRATION_LOOP_COUNT), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_JUMP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0) }
};

/// Return-movie binary-motor holds: segment 0 lasts two frames, segment 1 one.
///
/// The return commands select both entries. Their intensity bytes are ignored
/// by the binary lane; the variable lane stays stopped. Borrowed until teardown.
static const PadScriptVibrationSegment _gAcropolisForkedRoadReturnMovieVibrationSegments[2] SECTION(".data") = {
    { .startIntensity = 0, .endIntensity = 0, .durationFrames = 2, .field_3 = 0 },
    { .startIntensity = 60, .endIntensity = 60, .durationFrames = 1, .field_3 = 0 },
};

enum {
    ACROPOLIS_FORKED_ROAD_OUTBOUND_VIBRATION_WAIT_FRAMES = 2,
    ACROPOLIS_FORKED_ROAD_OUTBOUND_VIBRATION_LOOP_COUNT  = 38,
};

/// Outbound-movie binary-motor pulses with a two-frame pause and loop counter 38.
///
/// Steps 0/2 play segments 0/1; step 4 jumps to step 0 while the loop counter
/// remains nonzero, otherwise step 5 stops. The variable lane stays stopped.
/// Commands and segments remain borrowed until the adopted script task is killed.
static const PadScriptCmd _gAcropolisForkedRoadOutboundMovieVibrationCommands[6] SECTION(".data") = {
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 1) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_WAIT, ACROPOLIS_FORKED_ROAD_OUTBOUND_VIBRATION_WAIT_FRAMES), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 1), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_LOOP, ACROPOLIS_FORKED_ROAD_OUTBOUND_VIBRATION_LOOP_COUNT), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_JUMP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0) }
};

/// Outbound-movie binary-motor holds: segment 0 lasts two frames, segment 1 one.
///
/// Both entries are selected by the outbound commands. Intensity bytes are
/// ignored by the binary lane; the variable lane stays stopped. Borrowed until teardown.
static const PadScriptVibrationSegment _gAcropolisForkedRoadOutboundMovieVibrationSegments[2] SECTION(".data") = {
    { .startIntensity = 0, .endIntensity = 0, .durationFrames = 2, .field_3 = 0 },
    { .startIntensity = 60, .endIntensity = 60, .durationFrames = 1, .field_3 = 0 },
};

enum {
    ACROPOLIS_FORKED_ROAD_SURFACE_CLASS4_WALK_CUE = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x11,
    ACROPOLIS_FORKED_ROAD_SURFACE_CLASS4_RUN_CUE  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x13,
};

/// Paired footstep cue bases for forked-road surface class 4.
///
/// Walking and scripted jumping use type-1 entry 17; running uses entry 19.
/// The second animation cue uses the base and the first adds one; companions
/// add 100 after cue selection. Playback requires the loaded room's type-1 bank;
/// the surface record borrows these bases while this overlay is loaded.
static WorldCollisionFootstepSounds _gAcropolisForkedRoadSurfaceClass4FootstepSounds = {
    .walk         = ACROPOLIS_FORKED_ROAD_SURFACE_CLASS4_WALK_CUE,
    .run          = ACROPOLIS_FORKED_ROAD_SURFACE_CLASS4_RUN_CUE,
    .scriptedJump = ACROPOLIS_FORKED_ROAD_SURFACE_CLASS4_WALK_CUE,
};

/// Blocking, pushback-enabled policies for surface classes 0, 1, 5, 6 and 7.
///
/// Allows weapon impacts and supplies no footstep cues. Borrowed only while
/// the forked-road overlay is loaded; the uninterpreted first byte stays zero.
static WorldCollisionSurfaceProperties _gAcropolisForkedRoadDefaultSurfaceProperties = {
    0,
    WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    NULL,
};

/// Surface class 2's blocking and pushback policies, without impacts or footsteps.
///
/// Borrowed while the forked-road overlay is loaded; the first byte's role is unproven.
static WorldCollisionSurfaceProperties _gAcropolisForkedRoadSurfaceClass2Properties = {
    0,
    WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS,
    WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    NULL,
};

/// Surface class 3's blocking and pushback policies, without impacts or footsteps.
///
/// Borrowed while the forked-road overlay is loaded; the first byte's role is unproven.
static WorldCollisionSurfaceProperties _gAcropolisForkedRoadSurfaceClass3Properties = {
    0,
    WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS,
    WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    NULL,
};

/// Surface class 4's blocking and pushback policies, with impacts and footsteps.
///
/// Borrows its cue bases while this overlay is loaded; the first byte's role is unproven.
static WorldCollisionSurfaceProperties _gAcropolisForkedRoadSurfaceClass4Properties = {
    0,
    WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    &_gAcropolisForkedRoadSurfaceClass4FootstepSounds,
};

WorldCollisionSurfaceProperties* gAcropolisForkedRoadSurfacePropertiesByClass[8] = {
    &_gAcropolisForkedRoadDefaultSurfaceProperties,
    &_gAcropolisForkedRoadDefaultSurfaceProperties,
    &_gAcropolisForkedRoadSurfaceClass2Properties,
    &_gAcropolisForkedRoadSurfaceClass3Properties,
    &_gAcropolisForkedRoadSurfaceClass4Properties,
    &_gAcropolisForkedRoadDefaultSurfaceProperties,
    &_gAcropolisForkedRoadDefaultSurfaceProperties,
    &_gAcropolisForkedRoadDefaultSurfaceProperties,
};

/// Copies a frame-indexed movie-path sample into the borrowed player root.
///
/// Requires a live `RoomMoviePathWork` and index 0..299 in the room's
/// `_gAcropolisForkedRoadMoviePlayerPositions` path. Both arguments are evaluated three
/// times, once per XYZ store, and must be stable and side-effect-free. Positions
/// use whole room units; rotation and coordinate cache stamps are untouched.
/// Expands to a braced statement block; use inside a braced scope.
#define ACROPOLIS_FORKED_ROAD_APPLY_MOVIE_PATH_POSITION(movieWork, pathIndex)                     \
    {                                                                                             \
        (movieWork)->playerMtx->t[0] = _gAcropolisForkedRoadMoviePlayerPositions[(pathIndex)].vx; \
        (movieWork)->playerMtx->t[1] = _gAcropolisForkedRoadMoviePlayerPositions[(pathIndex)].vy; \
        (movieWork)->playerMtx->t[2] = _gAcropolisForkedRoadMoviePlayerPositions[(pathIndex)].vz; \
    }

/// Moves the player along the outbound movie path, then reloads the observatory.
///
/// Starts bodyless in state 0 with a live player and loaded room/stream resources.
/// Owns a zeroed `RoomMoviePathWork`; allocation failure kills the controller.
/// Places the player before walking to the path head, hides the HUD and actors,
/// then queues the current view's movie. Once ready, adopts the vibration task
/// and writes XYZ from path index `movieFrame - 1` in whole room units. The
/// frame must be 1..300 on every path access; reaching index 120 or pressing
/// Start schedules departure on the next tick. Reload hands off to observatory
/// room 1, warp 4. Task teardown releases the work and adopted script task.
static void _acropolisForkedRoadOutboundMoviePathTask(Task* task)
{
    enum {
        ACROPOLIS_FORKED_ROAD_OUTBOUND_INITIALIZE          = 0,
        ACROPOLIS_FORKED_ROAD_OUTBOUND_APPROACH            = 1,
        ACROPOLIS_FORKED_ROAD_OUTBOUND_WAIT_APPROACH       = 2,
        ACROPOLIS_FORKED_ROAD_OUTBOUND_WAIT_MOVIE          = 3,
        ACROPOLIS_FORKED_ROAD_OUTBOUND_FOLLOW_PATH         = 4,
        ACROPOLIS_FORKED_ROAD_OUTBOUND_DEPART              = 5,
        ACROPOLIS_FORKED_ROAD_OUTBOUND_PATH_EXIT_INDEX     = 120,
        ACROPOLIS_FORKED_ROAD_OUTBOUND_YAW                 = ACTOR_TRANSFORM_ANGLE_TURN / 4,
        ACROPOLIS_FORKED_ROAD_OUTBOUND_APPROACH_X_DISTANCE = 1620,
        ACROPOLIS_FORKED_ROAD_OUTBOUND_DESTINATION_WARP    = 4,
        ACROPOLIS_FORKED_ROAD_OUTBOUND_DESTINATION_ROOM    = 1
    };
    ActorTransform     entryPose;
    ActorTransform     approachTarget;
    u8                 streamArgs[sizeof(gCdCmdQueue.entries[0].args.bytes)];
    RoomMoviePathWork* movieWork;
    CdCmdQueue*        cdQueue;

    cdQueue   = &gCdCmdQueue;
    movieWork = task->work;
    switch (task->state) {
        case ACROPOLIS_FORKED_ROAD_OUTBOUND_INITIALIZE:
            task->work = memCalloc(sizeof(RoomMoviePathWork), 0);
            if (task->work == NULL) {
                taskKill(task);
                break;
            }
            cdQueue->movieFrame = 1;
            padInputChangeSuppression(PAD_INPUT_SUPPRESSION_SET_AND_HOLD, PAD_INPUT_SUPPRESS_ACTIONS_AND_MENU);
            gSceneCombatState.actorControl               = SCENE_COMBAT_ACTORS_HIDDEN;
            ((RoomMoviePathWork*)task->work)->playerMtx  = gPlayerStatus.coordMtx;
            ((RoomMoviePathWork*)task->work)->playerTask = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
            taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_CAP_CONTROL), CAP_CONTROL_MESSAGE_HIDE_HUD, 0, 0);
            entryPose.rot.vy = ACROPOLIS_FORKED_ROAD_OUTBOUND_YAW;
            entryPose.rot.vx = 0;
            entryPose.rot.vz = 0;
            entryPose.pos.vx = _gAcropolisForkedRoadMoviePlayerPositions[0].vx - ACROPOLIS_FORKED_ROAD_OUTBOUND_APPROACH_X_DISTANCE;
            entryPose.pos.vy = _gAcropolisForkedRoadMoviePlayerPositions[0].vy;
            entryPose.pos.vz = _gAcropolisForkedRoadMoviePlayerPositions[0].vz;
            TASK_MESSAGE_DISPATCH_POINTER(((RoomMoviePathWork*)task->work)->playerTask, GAME_ACTOR_MESSAGE_PLACE, &entryPose, 0);
            task->state = task->state + 1;
            break;

        case ACROPOLIS_FORKED_ROAD_OUTBOUND_APPROACH:
            approachTarget.rot.vy = ACROPOLIS_FORKED_ROAD_OUTBOUND_YAW;
            approachTarget.pos.vx = _gAcropolisForkedRoadMoviePlayerPositions[0].vx;
            approachTarget.pos.vy = _gAcropolisForkedRoadMoviePlayerPositions[0].vy;
            approachTarget.pos.vz = _gAcropolisForkedRoadMoviePlayerPositions[0].vz;
            TASK_MESSAGE_DISPATCH_POINTER(((RoomMoviePathWork*)task->work)->playerTask, GAME_ACTOR_MESSAGE_MOVE_TO, &approachTarget, 0);
            task->state = task->state + 1;
            break;

        case ACROPOLIS_FORKED_ROAD_OUTBOUND_WAIT_APPROACH:
            if (taskMessageDispatch(movieWork->playerTask, GAME_ACTOR_MESSAGE_IS_SCRIPTED_MOTION_PENDING, 0, 0) == 0) {
                // Only byte zero is interpreted by this opcode; preserve the unwritten tail and low-RAM key reads.
                streamArgs[0] = streamFindMovieSlot(&gGameSession->location.loc, 0, 0);
                cdCmdEnqueue(CD_COMMAND_PLAY_STREAM, 0, streamArgs);
                task->state = task->state + 1;
            }
            break;

        case ACROPOLIS_FORKED_ROAD_OUTBOUND_WAIT_MOVIE:
            if (cdQueue->movieReady != 0) {
                movieWork->padScriptTask      = padScriptSpawn(_gAcropolisForkedRoadOutboundMovieVibrationCommands,
                                                               _gAcropolisForkedRoadOutboundMovieVibrationSegments);
                gGameSession->padScriptFlags |= GAME_SESSION_PAD_SCRIPT_DURING_BATTLE_FREEZE;
                taskReparent(task, movieWork->padScriptTask);
                task->state = task->state + 1;
            }
            break;

        case ACROPOLIS_FORKED_ROAD_OUTBOUND_FOLLOW_PATH:
            // Follow encoded frames in the player root's parent-coordinate space.
            ACROPOLIS_FORKED_ROAD_APPLY_MOVIE_PATH_POSITION(movieWork, cdQueue->movieFrame - 1);
            if ((padIsStartPressed() != 0) || ((cdQueue->movieFrame - 1) >= ACROPOLIS_FORKED_ROAD_OUTBOUND_PATH_EXIT_INDEX)) {
                task->state = task->state + 1;
            }
            break;

        case ACROPOLIS_FORKED_ROAD_OUTBOUND_DEPART:
            gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_RUNNING;
            padInputChangeSuppression(PAD_INPUT_SUPPRESSION_CLEAR_ALIAS, PAD_INPUT_SUPPRESS_ACTIONS_AND_MENU);
            sndEvtRequestScriptStop(SOUND_BANK_TYPE_ALL_NON_AMBIENT, SOUND_SCRIPT_STOP_NO_FADE);
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.stage = GAME_STAGE_ACROPOLIS;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.area  = GAME_AREA_ACROPOLIS_OBSERVATORY;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.warp  = ACROPOLIS_FORKED_ROAD_OUTBOUND_DESTINATION_WARP;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room  = ACROPOLIS_FORKED_ROAD_OUTBOUND_DESTINATION_ROOM;
            gDisplayState.spriteVariant                                 = 1;
            taskSpawn(GAME_FLOW_RELOAD_TASK_BANK, GAME_FLOW_RELOAD_TASK_SLOT, GAME_FLOW_RELOAD_CAPTURE_FRAME, 0);
            gGameSession->padScriptFlags &= (0xFF ^ GAME_SESSION_PAD_SCRIPT_DURING_BATTLE_FREEZE);
            taskKill(task);
            break;
    }
}

/// Moves the player backwards along the arrival movie path and restores local play.
///
/// Starts bodyless in state 0 after arrival warp 2 has selected the view movie;
/// this controller waits for readiness and does not queue playback itself.
/// Owns a zeroed `RoomMoviePathWork`, borrows the live player/root matrix and
/// adopts its vibration task. Allocation failure kills the controller.
/// Path accesses require `movieFrame` in 1..59 and use index `59 - movieFrame`
/// in whole room units. The first two active path ticks blank the display.
/// Start requests a fade task polled to suspension before placing the exit pose;
/// otherwise indices below 11 start a walk to the exit. After movement ends,
/// restores the HUD, input, actors and logical view 5, then tears down its work
/// and adopted children. Fade and vibration spawns retain unchecked failures.
static void _acropolisForkedRoadReturnMoviePathTask(Task* task)
{
    enum {
        ACROPOLIS_FORKED_ROAD_RETURN_INITIALIZE          = 0,
        ACROPOLIS_FORKED_ROAD_RETURN_WAIT_MOVIE          = 1,
        ACROPOLIS_FORKED_ROAD_RETURN_FOLLOW_PATH         = 2,
        ACROPOLIS_FORKED_ROAD_RETURN_WAIT_EXIT           = 3,
        ACROPOLIS_FORKED_ROAD_RETURN_FRAME_ORIGIN        = 59,
        ACROPOLIS_FORKED_ROAD_RETURN_WALK_EXIT_INDEX     = 11,
        ACROPOLIS_FORKED_ROAD_RETURN_REVEAL_TICK         = 3,
        ACROPOLIS_FORKED_ROAD_RETURN_YAW                 = ACTOR_TRANSFORM_ANGLE_TURN * 3 / 4,
        ACROPOLIS_FORKED_ROAD_RETURN_LOGICAL_VIEW        = 5,
        ACROPOLIS_FORKED_ROAD_PRIMARY_CHARACTER_ID       = 1,
        ACROPOLIS_FORKED_ROAD_ALTERNATE_WEAPON_BANK_BASE = 0x22,
        ACROPOLIS_FORKED_ROAD_RETURN_IDLE_ANIMATION      = 1
    };
    AnimationPlayRequest animationRequest;
    ActorTransform       exitPose;
    s32                  fadeResult;
    RoomMoviePathWork*   movieWork;
    CdCmdQueue*          cdQueue;
    s32                  weaponId;

    cdQueue   = &gCdCmdQueue;
    movieWork = task->work;
    switch (task->state) {
        case ACROPOLIS_FORKED_ROAD_RETURN_INITIALIZE:
            task->work = memCalloc(sizeof(RoomMoviePathWork), 0);
            if (task->work == NULL) {
                taskKill(task);
                break;
            }
            ((RoomMoviePathWork*)task->work)->playerTask = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
            ((RoomMoviePathWork*)task->work)->playerMtx  = gPlayerStatus.coordMtx;
            taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_CAP_CONTROL), CAP_CONTROL_MESSAGE_HIDE_HUD, 0, 0);
            weaponId                              = gPlayerStatus.weapon;
            animationRequest.source.index         = (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId == ACROPOLIS_FORKED_ROAD_PRIMARY_CHARACTER_ID) ? weaponId + 1 : weaponId + ACROPOLIS_FORKED_ROAD_ALTERNATE_WEAPON_BANK_BASE;
            animationRequest.animationId          = ACROPOLIS_FORKED_ROAD_RETURN_IDLE_ANIMATION;
            animationRequest.blend                = ANIMATION_BLEND_RESET;
            animationRequest.blendFrames          = 0;
            animationRequest.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
            TASK_MESSAGE_DISPATCH_POINTER(((RoomMoviePathWork*)task->work)->playerTask, ANIMATION_MESSAGE_PLAY, &animationRequest, 0);
            padInputChangeSuppression(PAD_INPUT_SUPPRESSION_SET_AND_HOLD, PAD_INPUT_SUPPRESS_ACTIONS_AND_MENU);
            gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_HIDDEN;
            task->state                    = task->state + 1;
            break;

        case ACROPOLIS_FORKED_ROAD_RETURN_WAIT_MOVIE:
            if (cdQueue->movieReady != 0) {
                ACROPOLIS_FORKED_ROAD_APPLY_MOVIE_PATH_POSITION(movieWork, ACROPOLIS_FORKED_ROAD_RETURN_FRAME_ORIGIN - cdQueue->movieFrame);
                movieWork->padScriptTask      = padScriptSpawn(_gAcropolisForkedRoadReturnMovieVibrationCommands,
                                                               _gAcropolisForkedRoadReturnMovieVibrationSegments);
                gGameSession->padScriptFlags |= GAME_SESSION_PAD_SCRIPT_DURING_BATTLE_FREEZE;
                taskReparent(task, movieWork->padScriptTask);
                SetDispMask(0);
                task->killCountdown = 0;
                task->state         = task->state + 1;
            }
            break;

        case ACROPOLIS_FORKED_ROAD_RETURN_FOLLOW_PATH:
            // Hide the initial discontinuity for two ticks before revealing the reverse path.
            task->killCountdown = task->killCountdown + 1;
            if (task->killCountdown >= ACROPOLIS_FORKED_ROAD_RETURN_REVEAL_TICK) {
                SetDispMask(1);
            }
            ACROPOLIS_FORKED_ROAD_APPLY_MOVIE_PATH_POSITION(movieWork, ACROPOLIS_FORKED_ROAD_RETURN_FRAME_ORIGIN - cdQueue->movieFrame);
            // A completed skip fade places the final pose; normal completion walks there.
            if (movieWork->skipFadeStarted != 0) {
                if (taskPollKill(movieWork->skipFadeTask, &fadeResult) != 0) {
                    exitPose.pos.vx = -0x190;
                    exitPose.pos.vy = 1;
                    exitPose.pos.vz = _gAcropolisForkedRoadMoviePlayerPositions[ACROPOLIS_FORKED_ROAD_RETURN_FRAME_ORIGIN - cdQueue->movieFrame].vz;
                    exitPose.rot.vz = 0;
                    exitPose.rot.vx = 0;
                    exitPose.rot.vy = ACROPOLIS_FORKED_ROAD_RETURN_YAW;
                    TASK_MESSAGE_DISPATCH_POINTER(((RoomMoviePathWork*)task->work)->playerTask, GAME_ACTOR_MESSAGE_PLACE, &exitPose, 0);
                    taskSpawnFromTable(gAcropolisForkedRoadMovieTaskDescriptors, ACROPOLIS_FORKED_ROAD_TASK_SKIP_FADE_IN, 0, 0);
                    task->state = task->state + 1;
                    break;
                }
            } else if (padIsStartPressed() != 0) {
                movieWork->skipFadeTask    = taskSpawnFromTable(gAcropolisForkedRoadMovieTaskDescriptors, ACROPOLIS_FORKED_ROAD_TASK_SKIP_FADE_OUT, 0, 0);
                movieWork->skipFadeStarted = 1;
            }
            if ((ACROPOLIS_FORKED_ROAD_RETURN_FRAME_ORIGIN - cdQueue->movieFrame) < ACROPOLIS_FORKED_ROAD_RETURN_WALK_EXIT_INDEX) {
                exitPose.pos.vx = -0x190;
                exitPose.pos.vy = 1;
                exitPose.pos.vz = _gAcropolisForkedRoadMoviePlayerPositions[ACROPOLIS_FORKED_ROAD_RETURN_FRAME_ORIGIN - cdQueue->movieFrame].vz;
                TASK_MESSAGE_DISPATCH_POINTER(((RoomMoviePathWork*)task->work)->playerTask, GAME_ACTOR_MESSAGE_MOVE_TO, &exitPose, 0);
                task->state = task->state + 1;
            }
            break;

        case ACROPOLIS_FORKED_ROAD_RETURN_WAIT_EXIT:
            if (taskMessageDispatch(movieWork->playerTask, GAME_ACTOR_MESSAGE_IS_SCRIPTED_MOTION_PENDING, 0, 0) == 0) {
                taskMessageDispatch(movieWork->playerTask, GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = viewFindLogicalIndex(ACROPOLIS_FORKED_ROAD_RETURN_LOGICAL_VIEW);
                taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_CAP_CONTROL), CAP_CONTROL_MESSAGE_SHOW_HUD, 0, 0);
                padInputChangeSuppression(PAD_INPUT_SUPPRESSION_CLEAR_ALIAS, PAD_INPUT_SUPPRESS_ACTIONS_AND_MENU);
                gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_RUNNING;
                gGameSession->padScriptFlags  &= (0xFF ^ GAME_SESSION_PAD_SCRIPT_DURING_BATTLE_FREEZE);
                taskKill(task);
            }
            break;
    }
}

#undef ACROPOLIS_FORKED_ROAD_APPLY_MOVIE_PATH_POSITION

/// Darkens the return ride before its skip caller replaces the player's pose.
///
/// Requires a bodyless task with `killCountdown` initially zero. Queues eight
/// equal-channel subtractive overlays, intensities 0..224 in steps of 32,
/// before suspending with result zero at progress 256. The caller must collect
/// it with `taskPollKill`; suspension retains the task and its resources.
static void _acropolisForkedRoadSkipFadeOutTask(Task* task)
{
    enum { ACROPOLIS_FORKED_ROAD_SKIP_FADE_STEP = 32,
           ACROPOLIS_FORKED_ROAD_SKIP_FADE_SPAN = 256 };
    u8  fadeLevel;
    s16 nextProgress;

    fadeLevel = (u8)task->killCountdown;
    fadeDrawOverlay(fadeLevel, fadeLevel, fadeLevel, GPU_BLEND_SUBTRACT);
    nextProgress        = (u16)task->killCountdown + ACROPOLIS_FORKED_ROAD_SKIP_FADE_STEP;
    task->killCountdown = nextProgress;
    if (nextProgress >= ACROPOLIS_FORKED_ROAD_SKIP_FADE_SPAN) {
        taskRequestKill(task, 0);
    }
}

/// Reveals the return ride's final pose after its skip fade has hidden the scene.
///
/// Requires a bodyless task with `killCountdown` initially zero. Draws eight
/// subtractive overlays, from brightness 255 down to 31 in steps of 32, then
/// kills the task. The signed 16-bit counter is callback-owned fade progress.
static void _acropolisForkedRoadSkipFadeInTask(Task* task)
{
    enum { SKIP_FADE_LEVEL_STEP = 32,
           SKIP_FADE_LEVEL_SPAN = 256 };
    u8  fadeLevel;
    s16 nextProgress;

    fadeLevel = ~(u8)task->killCountdown;
    fadeDrawOverlay(fadeLevel, fadeLevel, fadeLevel, GPU_BLEND_SUBTRACT);
    nextProgress        = (u16)task->killCountdown + SKIP_FADE_LEVEL_STEP;
    task->killCountdown = nextProgress;
    if (nextProgress >= SKIP_FADE_LEVEL_SPAN) {
        taskKill(task);
    }
}

/// Releases the room's scripted Maggot/Caterpillar entrances.
static void _acropolisForkedRoadReleaseMaggotCaterpillarEntrance(void)
{
    gSceneCombatState.maggotCaterpillarEntranceReady = true;
}

void acropolisForkedRoadInitializeRoomEffectsTask(Task* task)
{
    enum {
        ACROPOLIS_FORKED_ROAD_ROOM_EFFECTS_INITIALIZE = 0,
        ACROPOLIS_FORKED_ROAD_CELL_0_SMALL_END        = 2,
        ACROPOLIS_FORKED_ROAD_CELL_0_LARGE_END        = 4,
        ACROPOLIS_FORKED_ROAD_CELL_1_END              = 12,
        ACROPOLIS_FORKED_ROAD_LAMP_SCALE_512          = 512 << 16,
        ACROPOLIS_FORKED_ROAD_LAMP_SCALE_768          = 768 << 16,
        ACROPOLIS_FORKED_ROAD_LAMP_CELL_1             = 1 << 8,
        ACROPOLIS_FORKED_ROAD_LAMP_CELL_2             = 2 << 8
    };
    GfxCoord* effectCoord;
    s32       lampIndex;

    effectCoord = task->extra.coordBody->coord;
    if (task->state == ACROPOLIS_FORKED_ROAD_ROOM_EFFECTS_INITIALIZE) {
        for (lampIndex = 0; lampIndex < ACROPOLIS_FORKED_ROAD_CELL_0_SMALL_END; lampIndex++) {
            effectSpawn(EFFECT_ACROPOLIS_FORKED_ROAD_WALL_LAMP, effectCoord, lampIndex + ACROPOLIS_FORKED_ROAD_LAMP_SCALE_512, &_gAcropolisForkedRoadWallLampPositions[lampIndex]);
        }
        for (lampIndex = ACROPOLIS_FORKED_ROAD_CELL_0_SMALL_END; lampIndex < ACROPOLIS_FORKED_ROAD_CELL_0_LARGE_END; lampIndex++) {
            effectSpawn(EFFECT_ACROPOLIS_FORKED_ROAD_WALL_LAMP, effectCoord, lampIndex + ACROPOLIS_FORKED_ROAD_LAMP_SCALE_768, &_gAcropolisForkedRoadWallLampPositions[lampIndex]);
        }
        for (lampIndex = ACROPOLIS_FORKED_ROAD_CELL_0_LARGE_END; lampIndex < ACROPOLIS_FORKED_ROAD_CELL_1_END; lampIndex++) {
            effectSpawn(EFFECT_ACROPOLIS_FORKED_ROAD_WALL_LAMP, effectCoord, lampIndex + (ACROPOLIS_FORKED_ROAD_LAMP_SCALE_512 | ACROPOLIS_FORKED_ROAD_LAMP_CELL_1), &_gAcropolisForkedRoadWallLampPositions[lampIndex]);
        }
        for (lampIndex = ACROPOLIS_FORKED_ROAD_CELL_1_END; lampIndex < (s32)ARRAY_SIZE(_gAcropolisForkedRoadWallLampPositions); lampIndex++) {
            effectSpawn(EFFECT_ACROPOLIS_FORKED_ROAD_WALL_LAMP, effectCoord, lampIndex + ACROPOLIS_FORKED_ROAD_LAMP_CELL_2, &_gAcropolisForkedRoadWallLampPositions[lampIndex]);
        }
        gRoomEffectFlashId      = EFFECT_ACROPOLIS_FORKED_ROAD_FLASH;
        gRoomEffectTwinTrailId  = EFFECT_ACROPOLIS_FORKED_ROAD_TWIN_TRAIL;
        gRoomEffectSparkBurstId = EFFECT_ACROPOLIS_FORKED_ROAD_SPARK_BURST;
        task->state             = task->state + 1;
    }
}

/// Positions the wall-lamp glow quad as a screen-aligned square.
///
/// Requires an initialized `projection->screenPos` centre and nonnegative
/// `projection->halfExtent` half-width/half-height in pixels. Their sums and
/// differences must fit signed 32 bits. Corner indices 0..3 are top-left,
/// top-right, bottom-left and bottom-right. Each edge narrows to signed 16 bits
/// without clipping. Borrows both objects for this call and writes only X/Y.
static inline void _acropolisForkedRoadSetWallLampBounds(POLY_FT4* quad, const RoomGlowSpriteScratch* projection)
{
    s16 left;
    s16 right;
    s16 top;
    s16 bottom;

    left     = projection->screenPos.vx - projection->halfExtent;
    quad->x2 = left;
    quad->x0 = left;
    right    = projection->screenPos.vx + projection->halfExtent;
    quad->x3 = right;
    quad->x1 = right;
    top      = projection->screenPos.vy - projection->halfExtent;
    quad->y1 = top;
    quad->y0 = top;
    bottom   = projection->screenPos.vy + projection->halfExtent;
    quad->y3 = bottom;
    quad->y2 = bottom;
}

void acropolisForkedRoadWallLampTask(Task* task)
{
    enum {
        WALL_LAMP_INITIALIZE     = 0,
        WALL_LAMP_INDEX_MASK     = 0xF,
        WALL_LAMP_CELL_SHIFT     = 8,
        WALL_LAMP_CELL_MASK      = 3,
        WALL_LAMP_CELL_COUNT     = 3,
        WALL_LAMP_SCALE_SHIFT    = 16,
        WALL_LAMP_SCALE_MASK     = 0xFFF,
        WALL_LAMP_DEFAULT_SCALE  = 640,
        WALL_LAMP_MIN_DRAW_DEPTH = 17,
        WALL_LAMP_FLICKER_STEP   = 16,
        WALL_LAMP_TEXTURE_PAGE   = getTPage(0, GPU_BLEND_ADD, 704, 0),
        WALL_LAMP_CLUT_X_STRIDE  = 16,
        WALL_LAMP_CLUT_Y         = 270,
        WALL_LAMP_CELL_WIDTH     = 40,
        WALL_LAMP_LAST_TEXEL     = WALL_LAMP_CELL_WIDTH - 1,
    };
    RoomGlowSpriteScratch* projection;
    EffectWork*            work;
    GfxCoord*              coord;
    POLY_FT4*              quad;
    s32                    greyLevel;
    s32                    flickerLevel;

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    if (gRoomEffectState->effectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN &&
        ((_gAcropolisForkedRoadWallLampViewMasks[task->spawnArg1.value & WALL_LAMP_INDEX_MASK] >> (gGameSession->location.loc.view - 1)) & 1)) {
        actorRenderComposeCoord(coord);
        projection = SCRATCH_STACK_RESERVE_BLOCK(RoomGlowSpriteScratch);
        if (task->state == WALL_LAMP_INITIALIZE) {
            u8 restingLevels[WALL_LAMP_CELL_COUNT] = { 0x50, 0x30, 0x10 };

            // Decode once, retaining only the lamp's view-mask index in the spawn argument.
            if (task->spawnArg1.value & (WALL_LAMP_SCALE_MASK << WALL_LAMP_SCALE_SHIFT)) {
                work->scale = (task->spawnArg1.value >> WALL_LAMP_SCALE_SHIFT) & WALL_LAMP_SCALE_MASK;
            } else {
                work->scale = WALL_LAMP_DEFAULT_SCALE;
            }
            work->angle           = (task->spawnArg1.value >> WALL_LAMP_CELL_SHIFT) & WALL_LAMP_CELL_MASK;
            task->spawnArg1.value = task->spawnArg1.value & WALL_LAMP_INDEX_MASK;
            work->period          = restingLevels[work->angle];
            task->state++;
        }
        // Rejected projections still consume one frame-arena packet.
        projection->worldPos.vx = coord->workm.t[0];
        projection->worldPos.vy = coord->workm.t[1];
        projection->worldPos.vz = coord->workm.t[2];
        gte_SetTransMatrix(&GsWSMATRIX);
        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(&projection->worldPos);
        gte_rtps();
        quad           = gGpuPrimCursor;
        gGpuPrimCursor = quad + 1;
        setPolyFT4(quad);
        gte_stsxy(&projection->screenPos);
        gte_stszotz(&projection->otz);
        if (projection->otz >= WALL_LAMP_MIN_DRAW_DEPTH) {
            flickerLevel = ((u8)gDisplayState.animFrame & 1) * WALL_LAMP_FLICKER_STEP;
            greyLevel    = (u8)work->period + flickerLevel;
            quad->tpage  = WALL_LAMP_TEXTURE_PAGE;
            quad->r0     = greyLevel;
            quad->g0     = greyLevel;
            quad->b0     = greyLevel;
            setSemiTrans(quad, true);
            setClut(quad, work->angle * WALL_LAMP_CLUT_X_STRIDE, WALL_LAMP_CLUT_Y);
            quad->u0 = work->angle * WALL_LAMP_CELL_WIDTH;
            quad->v0 = 0;
            quad->u1 = work->angle * WALL_LAMP_CELL_WIDTH + WALL_LAMP_LAST_TEXEL;
            quad->v1 = 0;
            quad->u2 = work->angle * WALL_LAMP_CELL_WIDTH;
            quad->v2 = WALL_LAMP_LAST_TEXEL;
            quad->u3 = work->angle * WALL_LAMP_CELL_WIDTH + WALL_LAMP_LAST_TEXEL;
            quad->v3 = WALL_LAMP_LAST_TEXEL;

            projection->halfExtent = (work->scale * WALL_LAMP_LAST_TEXEL) / projection->otz;
            _acropolisForkedRoadSetWallLampBounds(quad, projection);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)projection->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)), quad);
        }
        SCRATCH_STACK_RELEASE_BLOCK(RoomGlowSpriteScratch);
    }
}

#include "../../shared/falling_leaves_task.inc.c"

void acropolisForkedRoadLeafFallTask(Task* task)
{
    _leafFallTask(task);
}

#include "../../shared/falling_leaves_draw.inc.c"

#include "../../shared/room_visual_effects.inc.c"

#include "../../shared/room_visual_effects_flash_task.inc.c"

void acropolisForkedRoadRoomVisualEffectsFlashTask(Task* task)
{
    _roomVisualEffectsFlashTask(task);
}

#include "../../shared/room_visual_effects_trails.inc.c"

void acropolisForkedRoadRoomVisualEffectsTwinTrailTask(Task* task)
{
#include "../../shared/room_visual_effects_trail_task.inc.c"
}

#include "../../shared/room_visual_effects_sparks.inc.c"

void acropolisForkedRoadRoomVisualEffectsSparkBurstTask(Task* task)
{
    _roomVisualEffectsSparkBurstTask(task);
}

#include "../../shared/room_visual_effects_glow.inc.c"
