#include "rooms/shelter_r37.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/coord.h"
#include "main/session.h"
#include "main/sound_types.h"
#include "main/task.h"
#include "main/task_types.h"

#include "mapui/map_shelter.h"

static s32 _shelterR37RejectKeyItemUse(Task* task, s32 messageId, s32 itemId, s32 unusedArg);
static s32 _shelterR37ResolveRoomEvent(Task* task, s32 messageId, RoomEventMsg* request, RoomEventMsg* reply);
static s32 _shelterR37IgnoreRoomCommand(Task* task, s32 messageId, s32 commandId, s32 commandArg);
static s32 _shelterR37IgnoreRoomAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedArg);

/// The room controller's four message handlers and terminal entry.
///
/// Resolves transitions, refuses key items and ignores room actions and commands.
/// The live room task borrows the table while this overlay remains loaded.
static TaskMessageEntry _gShelterR37RoomMessages[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _shelterR37ResolveRoomEvent },
    { ROOM_MESSAGE_USE_KEY_ITEM, _shelterR37RejectKeyItemUse },
    { DIRECTION_MESSAGE_ROOM_ACTION, _shelterR37IgnoreRoomAction },
    { ROOM_MESSAGE_COMMAND, _shelterR37IgnoreRoomCommand },
    { TASK_MESSAGE_TABLE_END, NULL },
};

u8* gShelterR37ViewMaps[1] = {
    gViewIdentityMap,
};

ViewCount gShelterR37ViewCounts[1] = { ARRAY_SIZE(gShelterR37ViewCameras) };

DirectionWarpEntry gShelterR37WarpEntries[1] = {
    {
        .player         = { { .word = ONE / 4 }, 3756, 0, -197 },
        .unknown_10     = { 0, 0, 0, 0 },
        .companion      = { { .word = ONE / 4 }, 3756, 0, -197 },
        .unknown_24     = { 0, 0, 0, 0 },
        .arrivalSound   = DIRECTION_WARP_SOUND_NONE,
        .departureSound = DIRECTION_WARP_SOUND_NONE,
        .blockedSound   = DIRECTION_WARP_SOUND_NONE,
        .initialView    = 2,
        .flags          = DIRECTION_WARP_FLAG_NONE,
        .mapFlagId      = DIRECTION_WARP_MAP_FLAG_NONE,
    },
};

/// Room-space face normals, scaled by ONE for unit length.
static SVECTOR _gShelterR37RoomCollisionNormals[6] = {
#include "assets/shelter_r37_collision_00360_normals.inc"
};

/// Signed room-space vertices in whole game-coordinate units.
static SVECTOR _gShelterR37RoomCollisionVerts[22] = {
#include "assets/shelter_r37_collision_00360_verts.inc"
};

/// Twelve indexed quads using surface classes 0 and 1.
static WorldCollisionGridFace _gShelterR37RoomCollisionFaces[12] = {
#include "assets/shelter_r37_collision_00360_faces.inc"
};

/// Six cell face-index lists, each ending at -1 within its eight-halfword span.
static s16 _gShelterR37RoomCollisionCells[48] = {
#include "assets/shelter_r37_collision_00360_cells.inc"
};

/// Supplies a cell face-list address for the generated collision table.
///
/// Captures `_gShelterR37RoomCollisionCells`.
/// `halfwordOffset` is evaluated once and must be a constant in 0..47
/// selecting a list start in `s16` elements. Returns a borrowed `s16*`
/// valid while this room is loaded; lists end at `WORLD_COLLISION_GRID_CELL_END`.
/// The producer selects this binding through `cell_face_list_macro`.
#define SHELTER_R37_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gShelterR37RoomCollisionCells[(halfwordOffset)])
/// X-major cell directory; each X row contains two Z cells.
static s16* _gShelterR37RoomCollisionTable[6] = {
#include "assets/shelter_r37_collision_00360_table.inc"
};
#undef SHELTER_R37_CELL_FACE_LIST_AT_OFFSET

enum { SHELTER_R37_COLLISION_CELL_COUNT_Z = 2 };

WorldCollisionGrid gShelterR37RoomCollision = {
    .viewCoord   = NULL,
    .normals     = _gShelterR37RoomCollisionNormals,
    .vertices    = _gShelterR37RoomCollisionVerts,
    .faces       = _gShelterR37RoomCollisionFaces,
    .cellFaceIds = _gShelterR37RoomCollisionTable,
    .xBias       = 4250,
    .zBias       = 3500,
    .cellCountX  = ARRAY_SIZE(_gShelterR37RoomCollisionTable) / SHELTER_R37_COLLISION_CELL_COUNT_Z,
    .cellCountZ  = SHELTER_R37_COLLISION_CELL_COUNT_Z,
    .cellSize    = 4000,
    .faceCount   = ARRAY_SIZE(_gShelterR37RoomCollisionFaces),
};

ViewCamera gShelterR37ViewCameras[3] = {
    { { { { ONE, 0, 0 }, { 0, 0, -ONE }, { 0, ONE, 0 } }, { 0, 0x38D6, 0 } }, 257 },
    { { { { 888, 0, -3998 }, { -122, 4094, -27 }, { 3996, 125, 888 } }, { 2810, 910, 870 } }, 257 },
    { { { { 902, 0, 3995 }, { 3827, 1174, -864 }, { -1145, 3924, 258 } }, { 1110, 5890, 370 } }, 207 },
};

/// View 1's empty batch list; the first zero count enables decoded image strips.
static SpriteBatch _gShelterR37View1SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 2's empty batch list; the first zero count enables decoded image strips.
static SpriteBatch _gShelterR37View2SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 3's empty batch list; the first zero count enables decoded image strips.
static SpriteBatch _gShelterR37View3SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView gShelterR37SpriteViews[3] = {
    { { .empty = _gShelterR37View1SpriteBatches }, _gShelterR37View1SpriteBatches, NULL },
    { { .empty = _gShelterR37View2SpriteBatches }, _gShelterR37View2SpriteBatches, NULL },
    { { .empty = _gShelterR37View3SpriteBatches }, _gShelterR37View3SpriteBatches, NULL },
};

/// Four all-view directional lights with 12-fractional-bit RGB intensities.
///
/// Local translations supply direction vectors; composition and attenuation
/// are runtime state, so these room-owned records remain writable.
static WorldCoordLight _gShelterR37DirectionalLights[4] = {
    { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, -1000, 1000 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1028, 1028, 1028 }, { 0, 0 } },
    { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, -1000, -1000 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3076, 3076, 3076 }, { 0, 0 } },
    { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 1000, -1000, 0 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1028, 1028, 1028 }, { 0, 0 } },
    { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -1000, -1000, 0 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 409, 409, 409 }, { 0, 0 } },
};

/// Five all-view point lights with positions and falloff radii in world units.
///
/// RGB uses 12 fractional bits. Runtime composition and attenuation write
/// these records; every inner radius is less than its outer radius.
static WorldCoordPointLight _gShelterR37PointLights[5] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -4500, -2620, 0 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2048, 2048, 2048 }, { 0, 0 } }, 1339, 2360 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 3200, -2159, -922 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, ONE, ONE }, { 0, 0 } }, 2220, 3442 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -766, -2000, -4618 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, ONE, ONE }, { 0, 0 } }, 1319, 2059 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -1240, -5082, 2801 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, ONE, ONE }, { 0, 0 } }, 7000, 7001 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 3966, -2581, 2000 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, ONE, ONE }, { 0, 0 } }, 1800, 3001 },
};

WorldCoordRoomLights gShelterR37RoomLights = {
    .directionalLightCount = ARRAY_SIZE(_gShelterR37DirectionalLights),
    .directionalLights     = _gShelterR37DirectionalLights,
    .pointLightCount       = ARRAY_SIZE(_gShelterR37PointLights),
    .pointLights           = _gShelterR37PointLights,
    .coneLightCount        = 0,
    .coneLights            = NULL,
};

WorldCollisionTrigger gShelterR37ViewBoundaryTriggers[4] = {
    { NULL, NULL, NULL, { 192, -3504, 1359, 0 }, { { 1344, -4304, 3152, 0 }, { -1344, -4304, -3152, 0 }, { 1344, 4304, 3152, 0 }, { -1344, 4304, -3152, 0 } }, { -3775, 0, 1609, 0 }, { 0, 0, ONE, 0 }, 5490, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -176, -3488, 846, 0 }, { { -1072, -4304, -2544, 0 }, { 1072, -4304, 2544, 0 }, { -1072, 4304, -2544, 0 }, { 1072, 4304, 2544, 0 } }, { 3779, 0, -1593, 0 }, { 0, 0, ONE, 0 }, 5094, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x4560, -3552, -5312, 0 }, { { -480, -4304, 6912, 0 }, { 480, -4304, -6912, 0 }, { -480, 4304, 6912, 0 }, { 480, 4304, -6912, 0 } }, { -4088, 0, -284, 0 }, { 0, 0, ONE, 0 }, 8143, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x4520, -3520, -5568, 0 }, { { 432, -4304, -6784, 0 }, { -432, -4304, 6784, 0 }, { 432, 4304, -6784, 0 }, { -432, 4304, 6784, 0 } }, { 4093, 0, 260, 0 }, { 0, 0, ONE, 0 }, 8030, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Cue-2 bases in whichever type-1 sound bank is loaded; cue 1 adds one.
enum {
    SHELTER_R37_CLASS5_FOOTSTEP_WALK_CUE_BASE = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x49,
    SHELTER_R37_CLASS5_FOOTSTEP_RUN_CUE_BASE  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x4B,
    SHELTER_R37_CLASS2_FOOTSTEP_WALK_CUE_BASE = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x3D,
    SHELTER_R37_CLASS2_FOOTSTEP_RUN_CUE_BASE  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x3F,
    SHELTER_R37_CLASS3_FOOTSTEP_WALK_CUE_BASE = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x51,
    SHELTER_R37_CLASS3_FOOTSTEP_RUN_CUE_BASE  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x53,
};

/// Class-5 footstep pairs; walking and scripted jumps share the same base.
///
/// Companion cues add 100. The surface borrows this record while loaded;
/// the audible material and bank contents are unproven.
static WorldCollisionFootstepSounds _gShelterR37Class5FootstepSounds = {
    .walk         = SHELTER_R37_CLASS5_FOOTSTEP_WALK_CUE_BASE,
    .run          = SHELTER_R37_CLASS5_FOOTSTEP_RUN_CUE_BASE,
    .scriptedJump = SHELTER_R37_CLASS5_FOOTSTEP_WALK_CUE_BASE,
};

/// Class-2 footstep pairs; walking and scripted jumps share the same base.
///
/// Companion cues add 100. The surface borrows this record while loaded;
/// the audible material and bank contents are unproven.
static WorldCollisionFootstepSounds _gShelterR37Class2FootstepSounds = {
    .walk         = SHELTER_R37_CLASS2_FOOTSTEP_WALK_CUE_BASE,
    .run          = SHELTER_R37_CLASS2_FOOTSTEP_RUN_CUE_BASE,
    .scriptedJump = SHELTER_R37_CLASS2_FOOTSTEP_WALK_CUE_BASE,
};

/// Class-3 footstep pairs; walking and scripted jumps share the same base.
///
/// Companion cues add 100. The surface borrows this record while loaded;
/// the audible material and bank contents are unproven.
static WorldCollisionFootstepSounds _gShelterR37Class3FootstepSounds = {
    .walk         = SHELTER_R37_CLASS3_FOOTSTEP_WALK_CUE_BASE,
    .run          = SHELTER_R37_CLASS3_FOOTSTEP_RUN_CUE_BASE,
    .scriptedJump = SHELTER_R37_CLASS3_FOOTSTEP_WALK_CUE_BASE,
};

/// Blocking collision policy without footstep cues for classes 0, 4, 6 and 7.
static WorldCollisionSurfaceProperties _gShelterR37DefaultSurface = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

/// Class-5 blocking policy with footstep cues, weapon impacts and pushback.
static WorldCollisionSurfaceProperties _gShelterR37Class5Surface = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gShelterR37Class5FootstepSounds,
};

/// Class-2 blocking policy with footstep cues, weapon impacts and pushback.
static WorldCollisionSurfaceProperties _gShelterR37Class2Surface = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gShelterR37Class2FootstepSounds,
};

/// Class-3 blocking policy with footstep cues, weapon impacts and pushback.
static WorldCollisionSurfaceProperties _gShelterR37Class3Surface = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gShelterR37Class3FootstepSounds,
};

/// Class-1 policy passing probes and ignoring weapon impacts, with pushback and no footsteps.
static WorldCollisionSurfaceProperties _gShelterR37Class1Surface = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_PASS_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

WorldCollisionSurfaceProperties* gShelterR37SurfacePropertiesByClass[8] = {
    &_gShelterR37DefaultSurface,
    &_gShelterR37Class1Surface,
    &_gShelterR37Class2Surface,
    &_gShelterR37Class3Surface,
    &_gShelterR37DefaultSurface,
    &_gShelterR37Class5Surface,
    &_gShelterR37DefaultSurface,
    &_gShelterR37DefaultSurface,
};

static void _shelterR37InitializeRoom(Task* task);
static void _shelterR37Idle(Task* unusedTask);

/// Refuses key-item use and selects the inventory's cannot-use notice.
///
/// All callback arguments are unused; the collected item remains in inventory.
static s32 _shelterR37RejectKeyItemUse(Task* task, s32 messageId, s32 itemId, s32 unusedArg)
{
    return ROOM_KEY_ITEM_USE_REFUSED;
}

/// Accepts a Mine/Shelter transition and resolves its destination room variant.
///
/// Copies the complete eight-byte request into the reply before resolving it.
/// Queries preserve the requested room; execution uses current game progress.
/// Both records must be live and may alias. Neither pointer is retained, and
/// task and messageId are unused. Returns 1 to permit the ordinary transition.
static s32 _shelterR37ResolveRoomEvent(Task* task, s32 messageId, RoomEventMsg* request, RoomEventMsg* reply)
{
    enum { SHELTER_R37_ROOM_EVENT_ACCEPTED = 1 };

    *reply = *request;
    mapShelterRoomVariantResolve(request, reply);
    return SHELTER_R37_ROOM_EVENT_ACCEPTED;
}

/// Ignores room commands from scripts and triggers, returning zero.
///
/// Neither command word nor any other callback argument is read or retained.
static s32 _shelterR37IgnoreRoomCommand(Task* task, s32 messageId, s32 commandId, s32 commandArg)
{
    return 0;
}

/// Ignores room-action requests from direction triggers, returning zero.
///
/// The borrowed request and all other arguments are neither read nor retained.
static s32 _shelterR37IgnoreRoomAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedArg)
{
    return 0;
}

/// Installs and publishes the room controller for message dispatch.
///
/// Requires entry state 0 and the loaded room message table. Registers this
/// live task in `GAME_TASK_SLOT_ROOM`, then advances to idle state 1. Allocates
/// no work; the task and table must remain live while room messages are sent.
static void _shelterR37InitializeRoom(Task* task)
{
    task->msgTable = _gShelterR37RoomMessages;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    task->state += 1;
}

/// Keeps the initialized room controller available for messages without per-frame work.
///
/// State 1 retains its state and resources until external teardown. The
/// `unusedTask` parameter is required by the state-handler interface.
static void _shelterR37Idle(Task* unusedTask)
{
}

/// Room-task state dispatch for indices 0 initialize, 1 idle and 2 release.
static const TaskFuncTable3 _gShelterR37RoomTaskStates = {
    {
        _shelterR37InitializeRoom,
        _shelterR37Idle,
        taskKill,
    },
};

void shelterR37RoomTask(Task* task)
{
    TaskFuncTable3 states;

    states = _gShelterR37RoomTaskStates;
    states.funcs[task->state](task);
}
