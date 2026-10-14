#include "rooms/shelter_1f_heliport_s4.h"

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
#include "main/task.h"
#include "main/task_types.h"

#include "mapui/map_shelter.h"

#include "../../shared/room/room_variants.h"

static s32 _shelter1fHeliportS4RejectKeyItemUse(Task* task, s32 messageId, s32 itemId, s32 unusedArg);
static s32 _shelter1fHeliportS4ResolveRoomTransition(Task* task, s32 messageId, RoomEventMsg* request, RoomEventMsg* reply);
static s32 _shelter1fHeliportS4IgnoreRoomCommand(Task* task, s32 messageId, s32 commandId, s32 commandArg);
static s32 _shelter1fHeliportS4IgnoreRoomAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedArg);

/// Room receiver: resolve transitions, refuse key items, and ignore actions and commands.
///
/// The final end marker makes other message IDs return zero. Dispatch borrows
/// this table and its callbacks while the room task and overlay remain live.
static TaskMessageEntry _gShelter1fHeliportS4RoomMessageTable[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _shelter1fHeliportS4ResolveRoomTransition },
    { ROOM_MESSAGE_USE_KEY_ITEM, _shelter1fHeliportS4RejectKeyItemUse },
    { DIRECTION_MESSAGE_ROOM_ACTION, _shelter1fHeliportS4IgnoreRoomAction },
    { ROOM_MESSAGE_COMMAND, _shelter1fHeliportS4IgnoreRoomCommand },
    { TASK_MESSAGE_TABLE_END, NULL },
};

u8* gShelter1fHeliportS4RoomViewMaps[1] = {
    gViewIdentityMap,
};

ViewCount gShelter1fHeliportS4RoomViewCounts[1] = { SHELTER_1F_HELIPORT_S4_VIEW_COUNT };

DirectionWarpEntry gShelter1fHeliportS4WarpEntries[1] = {
    { { { .word = 0 }, 8000, 0, 1632 }, { 0, 0, 0, 0 }, { { .word = 0 }, 8000, 0, 1632 }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
};

/// Room-space unit face normals, with `ONE` per unit.
static SVECTOR _gShelter1fHeliportS4CollisionGridNormals[7] = {
#include "assets/shelter_1f_heliport_s4_collision_00408_normals.inc"
};

/// Collision-mesh vertices in signed room-space game coordinates.
static SVECTOR _gShelter1fHeliportS4CollisionGridVerts[34] = {
#include "assets/shelter_1f_heliport_s4_collision_00408_verts.inc"
};

/// Indexed collision quads, all using surface class 0.
static WorldCollisionGridFace _gShelter1fHeliportS4CollisionGridFaces[15] = {
#include "assets/shelter_1f_heliport_s4_collision_00408_faces.inc"
};

/// Nine -1-terminated face-index lists and their retained intervening zero halfwords.
static s16 _gShelter1fHeliportS4CollisionGridCells[56] = {
#include "assets/shelter_1f_heliport_s4_collision_00408_cells.inc"
};

/// Supplies this grid's cell face-list addresses to its generated table.
///
/// Captures `_gShelter1fHeliportS4CollisionGridCells`.
/// `halfwordOffset` is a constant s16-element offset in 0..55,
/// evaluated once and selecting a list's first entry. Lists end at
/// `WORLD_COLLISION_GRID_CELL_END`. Returns a borrowed `s16*`; no pool writes.
/// `cell_face_list_macro` selects this binding for the table include.
/// The binding is undefined immediately after that include.
#define SHELTER_1F_HELIPORT_S4_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gShelter1fHeliportS4CollisionGridCells[(halfwordOffset)])
/// X-major pointers to the nine lists in the 3x3 collision grid.
static s16* _gShelter1fHeliportS4CollisionGridTable[9] = {
#include "assets/shelter_1f_heliport_s4_collision_00408_table.inc"
};
#undef SHELTER_1F_HELIPORT_S4_CELL_FACE_LIST_AT_OFFSET

WorldCollisionGrid gShelter1fHeliportS4CollisionGrid = {
    .viewCoord   = NULL,
    .normals     = _gShelter1fHeliportS4CollisionGridNormals,
    .vertices    = _gShelter1fHeliportS4CollisionGridVerts,
    .faces       = _gShelter1fHeliportS4CollisionGridFaces,
    .cellFaceIds = _gShelter1fHeliportS4CollisionGridTable,
    .xBias       = 0,
    .zBias       = 2000,
    .cellCountX  = 3,
    .cellCountZ  = 3,
    .cellSize    = 4000,
    .faceCount   = ARRAY_SIZE(_gShelter1fHeliportS4CollisionGridFaces),
};

ViewCamera gShelter1fHeliportS4ViewCameras[SHELTER_1F_HELIPORT_S4_VIEW_COUNT] = {
    { { { { ONE, 0, 0 }, { 0, 0, -ONE }, { 0, ONE, 0 } }, { -4500, 0x3CE8, -3400 } }, 235 },
    { { { { 600, 0, -4051 }, { -345, 4081, -51 }, { 4037, 348, 598 } }, { -1395, 1503, -3025 } }, 225 },
    { { { { 577, 0, 4055 }, { 345, 4081, -49 }, { -4040, 349, 575 } }, { -8585, 1503, -2595 } }, 225 },
    { { { { 3910, 0, 1218 }, { -32, 4094, 105 }, { -1217, -110, 3909 } }, { -2572, 1124, -1708 } }, 269 },
    { { { { -4049, 0, 618 }, { -54, 4079, -359 }, { -615, -363, -4033 } }, { -8235, 928, -4313 } }, 289 },
};

/// View 1's zero-sprite image-background batch followed by its end marker.
static SpriteBatch _gShelter1fHeliportS4View1SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 2's zero-sprite image-background batch followed by its end marker.
static SpriteBatch _gShelter1fHeliportS4View2SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 3's zero-sprite image-background batch followed by its end marker.
static SpriteBatch _gShelter1fHeliportS4View3SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 4's zero-sprite image-background batch followed by its end marker.
static SpriteBatch _gShelter1fHeliportS4View4SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 5's zero-sprite image-background batch followed by its end marker.
static SpriteBatch _gShelter1fHeliportS4View5SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView gShelter1fHeliportS4SpriteViews[SHELTER_1F_HELIPORT_S4_VIEW_COUNT] = {
    { { .empty = _gShelter1fHeliportS4View1SpriteBatches }, _gShelter1fHeliportS4View1SpriteBatches, NULL },
    { { .empty = _gShelter1fHeliportS4View2SpriteBatches }, _gShelter1fHeliportS4View2SpriteBatches, NULL },
    { { .empty = _gShelter1fHeliportS4View3SpriteBatches }, _gShelter1fHeliportS4View3SpriteBatches, NULL },
    { { .empty = _gShelter1fHeliportS4View4SpriteBatches }, _gShelter1fHeliportS4View4SpriteBatches, NULL },
    { { .empty = _gShelter1fHeliportS4View5SpriteBatches }, _gShelter1fHeliportS4View5SpriteBatches, NULL },
};

/// Four all-view directional lights; translation supplies direction and RGB uses 12 fractional bits.
///
/// Room lighting binds their parent and overwrites composition and attenuation.
static WorldCoordLight _gShelter1fHeliportS4DirectionalLights[4] = {
    { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, -1000, 1000 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1028, 1028, 1028 }, { 0, 0 } },
    { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, -1000, -1000 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3076, 3076, 3076 }, { 0, 0 } },
    { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 1000, -1000, 0 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1028, 1028, 1028 }, { 0, 0 } },
    { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -1000, -1000, 0 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 409, 409, 409 }, { 0, 0 } },
};

/// Five all-view point lights with full-strength and cutoff radii in world game units.
///
/// Room lighting binds their parent and overwrites composition and attenuation.
static WorldCoordPointLight _gShelter1fHeliportS4PointLights[5] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -4500, -2620, 0 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2048, 2048, 2048 }, { 0, 0 } }, 1339, 2360 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 3200, -2159, -922 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, ONE, ONE }, { 0, 0 } }, 2220, 3442 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -766, -2000, -4618 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, ONE, ONE }, { 0, 0 } }, 1319, 2059 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -1240, -5082, 2801 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, ONE, ONE }, { 0, 0 } }, 7000, 7001 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 3966, -2581, 2000 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, ONE, ONE }, { 0, 0 } }, 1800, 3001 },
};

WorldCoordRoomLights gShelter1fHeliportS4RoomLights = {
    .directionalLightCount = ARRAY_SIZE(_gShelter1fHeliportS4DirectionalLights),
    .directionalLights     = _gShelter1fHeliportS4DirectionalLights,
    .pointLightCount       = ARRAY_SIZE(_gShelter1fHeliportS4PointLights),
    .pointLights           = _gShelter1fHeliportS4PointLights,
    .coneLightCount        = 0,
    .coneLights            = NULL,
};

WorldCollisionTrigger gShelter1fHeliportS4ViewBoundaryTriggers[6] = {
    { NULL, NULL, NULL, { 5008, -1296, 3536, 0 }, { { 588, -2112, -2769, 0 }, { -611, -2112, 2745, 0 }, { 588, 2112, -2769, 0 }, { -611, 2112, 2745, 0 } }, { 4004, 0, 870, 0 }, { 0, 0, ONE, 0 }, 3519, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 5153, -1360, 3536, 0 }, { { -624, -2112, 2765, 0 }, { 610, -2112, -2781, 0 }, { -624, 2112, 2765, 0 }, { 610, 2112, -2781, 0 } }, { -4013, 0, -893, 0 }, { 0, 0, ONE, 0 }, 3537, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 7968, -1312, 848, 0 }, { { 1504, -2112, 64, 0 }, { -1504, -2112, -64, 0 }, { 1504, 2112, 64, 0 }, { -1504, 2112, -64, 0 } }, { -175, 0, 4092, 0 }, { 0, 0, ONE, 0 }, 2585, 0, 2, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 8080, -1280, 960, 0 }, { { -1632, -2112, -64, 0 }, { 1632, -2112, 64, 0 }, { -1632, 2112, -64, 0 }, { 1632, 2112, 64, 0 } }, { 160, 0, -4095, 0 }, { 0, 0, ONE, 0 }, 2660, 0, 5, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1455, -1377, 5314, 0 }, { { -2320, -2112, -880, 0 }, { 2320, -2112, 880, 0 }, { -2320, 2112, -880, 0 }, { 2320, 2112, 880, 0 } }, { 1459, 0, -3847, 0 }, { 0, 0, ONE, 0 }, 3258, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1537, -1440, 5217, 0 }, { { 2352, -2112, 928, 0 }, { -2352, -2112, -928, 0 }, { 2352, 2112, 928, 0 }, { -2352, 2112, -928, 0 } }, { -1509, 0, 3823, 0 }, { 0, 0, ONE, 0 }, 3288, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Three retained words with the layout of footstep cue bases; their role is unproven.
///
/// No reader or surface-property reference is established.
static s32 D_shelter_1f_heliport_s4_8017E04C[3] = {
    0x10000011,
    0x10000013,
    0x10000011,
};

/// Blocking, impact-enabled, pushback-producing collision policy without footstep cues.
static WorldCollisionSurfaceProperties _gShelter1fHeliportS4SilentSurfaceProperties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

WorldCollisionSurfaceProperties* gShelter1fHeliportS4SurfacePropertiesByClass[8] = {
    &_gShelter1fHeliportS4SilentSurfaceProperties,
    &_gShelter1fHeliportS4SilentSurfaceProperties,
    &_gShelter1fHeliportS4SilentSurfaceProperties,
    &_gShelter1fHeliportS4SilentSurfaceProperties,
    &_gShelter1fHeliportS4SilentSurfaceProperties,
    &_gShelter1fHeliportS4SilentSurfaceProperties,
    &_gShelter1fHeliportS4SilentSurfaceProperties,
    &_gShelter1fHeliportS4SilentSurfaceProperties,
};

static void _shelter1fHeliportS4InitRoomTask(Task* task);
static void _shelter1fHeliportS4IdleRoomTask(Task* task);

/// Refuses every key-item use request without consuming the selected item.
///
/// All arguments are unused. The item menu supplies the collected-item ID
/// and a zero second payload; the refusal reply selects its unavailable-item notice.
static s32 _shelter1fHeliportS4RejectKeyItemUse(Task* task, s32 messageId, s32 itemId, s32 unusedArg)
{
    return ROOM_KEY_ITEM_USE_REFUSED;
}

/// Resolves a room-transition destination from Mine/Shelter progress and permits departure.
///
/// Handles `ROOM_EVENT_MESSAGE_RESOLVE`; the task and message ID are unused.
/// Request and reply borrow complete eight-byte records and may be the same object.
/// The full record is copied before the stage resolver updates its room selector;
/// queries preserve the copied destination. Area and arrival selectors must be
/// valid in the destination stage. Neither pointer is retained. Always returns
/// `ROOM_VARIANT_TRANSITION_DIRECT`, even when the destination is unchanged.
static s32 _shelter1fHeliportS4ResolveRoomTransition(Task* task, s32 messageId, RoomEventMsg* request, RoomEventMsg* reply)
{
    *reply = *request;
    mapShelterRoomVariantResolve(request, reply);
    return ROOM_VARIANT_TRANSITION_DIRECT;
}

/// Ignores room commands from scripts and triggers, returning zero.
///
/// Neither command word nor any other callback argument is read or retained.
static s32 _shelter1fHeliportS4IgnoreRoomCommand(Task* task, s32 messageId, s32 commandId, s32 commandArg)
{
    return 0;
}

/// Ignores room-action requests from direction triggers, returning zero.
///
/// The borrowed request and all other arguments are neither read nor retained.
static s32 _shelter1fHeliportS4IgnoreRoomAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedArg)
{
    return 0;
}

/// Registers the heliport's room-message receiver and advances to idle state 1.
///
/// Runs in state 0 of the live room task. Its message table remains borrowed
/// from this overlay while `GAME_TASK_SLOT_ROOM` exposes the task to senders.
static void _shelter1fHeliportS4InitRoomTask(Task* task)
{
    task->msgTable = _gShelter1fHeliportS4RoomMessageTable;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    task->state++;
}

/// Keeps the initialized room-message receiver idle in state 1.
static void _shelter1fHeliportS4IdleRoomTask(Task* task)
{
}

/// Room-task state handlers: initialize (0), idle (1), and release (2).
///
/// Copied by value before indexing with the live task's state in 0..2.
static const TaskFuncTable3 _gShelter1fHeliportS4RoomTaskStates = {
    {
        _shelter1fHeliportS4InitRoomTask,
        _shelter1fHeliportS4IdleRoomTask,
        taskKill,
    },
};

void shelter1fHeliportS4RoomTask(Task* task)
{
    TaskFuncTable3 handlers;

    // Snapshot all three state handlers before dispatching the current state.
    handlers = _gShelter1fHeliportS4RoomTaskStates;
    handlers.funcs[task->state](task);
}
