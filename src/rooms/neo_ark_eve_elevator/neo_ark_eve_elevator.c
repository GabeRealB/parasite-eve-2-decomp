#include "rooms/neo_ark_eve_elevator.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/coord.h"
#include "main/fs.h"
#include "main/gameflag_ids.h"
#include "main/session.h"
#include "main/sound_ids.h"
#include "main/sound_types.h"
#include "main/task.h"
#include "main/task_types.h"

#include "mapui/map_neo_ark.h"

#include "../../shared/room/room_variants.h"

static void _neoArkEveElevatorInitializeRoom(Task* task);
static void _neoArkEveElevatorIdleRoomTask(Task* unusedTask);

/// Room-task handlers for states 0 (install receiver), 1 (idle), and 2 (release task).
static const TaskFuncTable3 _gNeoArkEveElevatorRoomTaskHandlers = {
    {
        _neoArkEveElevatorInitializeRoom,
        _neoArkEveElevatorIdleRoomTask,
        taskKill,
    },
};

static s32 _neoArkEveElevatorRejectKeyItemUse(Task* unusedTask, s32 unusedMessageId, s32 itemId, s32 unusedSecondArg);
static s32 _neoArkEveElevatorResolveTransition(Task* unusedTask, s32 unusedMessageId, RoomEventMsg* request, RoomEventMsg* reply);
static s32 _neoArkEveElevatorIgnoreRoomCommand(Task* unusedTask, s32 unusedMessageId, s32 commandId, s32 commandArgument);
static s32 _neoArkEveElevatorIgnoreRoomAction(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* request, s32 unusedSecondArg);

static WorldCollisionGrid    _gNeoArkEveElevatorCollisionGrid;
static WorldCollisionTrigger _gNeoArkEveElevatorExitTriggers[1];
static WorldCoordRoomLights  _gNeoArkEveElevatorRoomLights;

/// Room receiver for transitions, key-item uses, trigger actions and commands.
///
/// Installed for the live room task, which borrows the table and callbacks
/// until teardown. The end marker makes unsupported messages return zero.
static TaskMessageEntry _gNeoArkEveElevatorRoomMessageTable[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _neoArkEveElevatorResolveTransition },
    { ROOM_MESSAGE_USE_KEY_ITEM, _neoArkEveElevatorRejectKeyItemUse },
    { DIRECTION_MESSAGE_ROOM_ACTION, _neoArkEveElevatorIgnoreRoomAction },
    { ROOM_MESSAGE_COMMAND, _neoArkEveElevatorIgnoreRoomCommand },
    { TASK_MESSAGE_TABLE_END, NULL },
};

WorldCollisionRoomResources gNeoArkEveElevatorCollisionRooms[1] = {
    { &_gNeoArkEveElevatorCollisionGrid, NULL, _gNeoArkEveElevatorExitTriggers, NULL },
};

WorldCoordRoomLighting gNeoArkEveElevatorRoomLighting[1] = {
    { &_gNeoArkEveElevatorRoomLights, NULL },
};

u8* gNeoArkEveElevatorViewMaps[1] = {
    gViewIdentityMap,
};

ViewCount gNeoArkEveElevatorViewCounts[1] = { NEO_ARK_EVE_ELEVATOR_VIEW_COUNT };

/// Endpoint 1 opens the ride view; headings use 4096 angle units per turn.
enum {
    NEO_ARK_EVE_ELEVATOR_ARRIVAL_YAW     = 3072,
    NEO_ARK_EVE_ELEVATOR_ARRIVAL_VIEW    = 2,
    NEO_ARK_EVE_ELEVATOR_ARRIVAL_SOUND   = SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_EVE_ELEVATOR, 2),
    NEO_ARK_EVE_ELEVATOR_DEPARTURE_SOUND = SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_EVE_ELEVATOR, 1),
};

DirectionWarpEntry gNeoArkEveElevatorWarpEntries[1] = {
    {
        .player         = { { .word = NEO_ARK_EVE_ELEVATOR_ARRIVAL_YAW }, -960, 0, 0 },
        .unknown_10     = { 0, 0, 0, 0 },
        .companion      = { { .word = NEO_ARK_EVE_ELEVATOR_ARRIVAL_YAW }, -960, 0, 0 },
        .unknown_24     = { 0, 0, 0, 0 },
        .arrivalSound   = NEO_ARK_EVE_ELEVATOR_ARRIVAL_SOUND,
        .departureSound = NEO_ARK_EVE_ELEVATOR_DEPARTURE_SOUND,
        .blockedSound   = DIRECTION_WARP_SOUND_NONE,
        .initialView    = NEO_ARK_EVE_ELEVATOR_ARRIVAL_VIEW,
        .flags          = DIRECTION_WARP_FLAG_NONE,
        .mapFlagId      = GAME_FLAG_MAP_MARK_POWER_PLANT_2,
    },
};

/// Room-local unit normals for the platform's walls and floor, with 4096 per unit.
static SVECTOR _gNeoArkEveElevatorCollisionGridNormals[14] = {
#include "assets/neo_ark_eve_elevator_collision_0046C_normals.inc"
};

/// Platform wall and floor vertices in signed room-coordinate units.
static SVECTOR _gNeoArkEveElevatorCollisionGridVerts[24] = {
#include "assets/neo_ark_eve_elevator_collision_0046C_verts.inc"
};

/// Indexed platform faces; class 1 is the floor and class 0 is silent geometry.
static WorldCollisionGridFace _gNeoArkEveElevatorCollisionGridFaces[24] = {
#include "assets/neo_ark_eve_elevator_collision_0046C_faces.inc"
};

/// The single cell's 24 face indices and -1 terminator, followed by a retained zero halfword.
static s16 _gNeoArkEveElevatorCollisionGridCells[26] = {
#include "assets/neo_ark_eve_elevator_collision_0046C_cells.inc"
};

/// Supplies this grid's cell face-list addresses to its generated table.
///
/// `halfwordOffset` is an integer constant measured in `s16` elements of
/// `_gNeoArkEveElevatorCollisionGridCells` and occurs once in the expansion.
/// It must start a complete `WORLD_COLLISION_GRID_CELL_END`-terminated list
/// within the pool. Returns a borrowed `s16*` valid while this overlay is loaded.
/// Set `collisionSource.cell_face_list_macro` to this identifier; keep the
/// binding through the table include, then undefine it.
#define NEO_ARK_EVE_ELEVATOR_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gNeoArkEveElevatorCollisionGridCells[(halfwordOffset)])
/// One X-major cell-list pointer covering the entire platform mesh.
static s16* _gNeoArkEveElevatorCollisionGridTable[1] = {
#include "assets/neo_ark_eve_elevator_collision_0046C_table.inc"
};
#undef NEO_ARK_EVE_ELEVATOR_CELL_FACE_LIST_AT_OFFSET

/// Writable platform collision mesh with a single 4000-unit square cell.
///
/// Room setup binds its view coordinate. Mesh indices stay within 14 normals,
/// 24 vertices and 24 faces; active queries borrow these pools until unload.
static WorldCollisionGrid _gNeoArkEveElevatorCollisionGrid = {
    .viewCoord   = NULL,
    .normals     = _gNeoArkEveElevatorCollisionGridNormals,
    .vertices    = _gNeoArkEveElevatorCollisionGridVerts,
    .faces       = _gNeoArkEveElevatorCollisionGridFaces,
    .cellFaceIds = _gNeoArkEveElevatorCollisionGridTable,
    .xBias       = 2050,
    .zBias       = 1050,
    .cellCountX  = 1,
    .cellCountZ  = 1,
    .cellSize    = 4000,
    .faceCount   = ARRAY_SIZE(_gNeoArkEveElevatorCollisionGridFaces),
};

ViewCamera gNeoArkEveElevatorViewCameras[NEO_ARK_EVE_ELEVATOR_VIEW_COUNT] = {
    { { { { ONE, 0, 0 }, { 0, 0, -ONE }, { 0, ONE, 0 } }, { 1000, 20000, 0 } }, 1371 },
    { { { { 943, 0, -3985 }, { -3917, 755, -927 }, { 735, 4025, 174 } }, { 1340, 3520, 90 } }, 207 },
    { { { { 943, 0, -3985 }, { -3917, 755, -927 }, { 735, 4025, 174 } }, { 1340, 3520, 90 } }, 207 },
    { { { { 943, 0, -3985 }, { -3917, 755, -927 }, { 735, 4025, 174 } }, { 1340, 3520, 90 } }, 207 },
};

/// View 1's zero-sprite image-background batch, followed by its end marker.
static SpriteBatch _gNeoArkEveElevatorView1SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 2's zero-sprite image-background batch, followed by its end marker.
static SpriteBatch _gNeoArkEveElevatorView2SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 3's zero-sprite image-background batch, followed by its end marker.
static SpriteBatch _gNeoArkEveElevatorView3SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 4's zero-sprite image-background batch, followed by its end marker.
static SpriteBatch _gNeoArkEveElevatorView4SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView gNeoArkEveElevatorSpriteViews[NEO_ARK_EVE_ELEVATOR_VIEW_COUNT] = {
    { { .empty = _gNeoArkEveElevatorView1SpriteBatches }, _gNeoArkEveElevatorView1SpriteBatches, NULL },
    { { .empty = _gNeoArkEveElevatorView2SpriteBatches }, _gNeoArkEveElevatorView2SpriteBatches, NULL },
    { { .empty = _gNeoArkEveElevatorView3SpriteBatches }, _gNeoArkEveElevatorView3SpriteBatches, NULL },
    { { .empty = _gNeoArkEveElevatorView4SpriteBatches }, _gNeoArkEveElevatorView4SpriteBatches, NULL },
};

/// One all-view point light at (-1000, -1742, 0), with 1500/2500-unit falloff radii.
///
/// RGB intensity has 12 fractional bits. Composition and shading update the
/// cached transform and attenuation, so the room-owned storage stays writable.
static WorldCoordPointLight _gNeoArkEveElevatorPointLights[1] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -1000, -1742, 0 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1228, 2867, 2621 }, { 0, 0 } }, 1500, 2500 },
};

/// The elevator's point-light collection, with no directional or cone lights.
static WorldCoordRoomLights _gNeoArkEveElevatorRoomLights = {
    .directionalLightCount = 0,
    .directionalLights     = NULL,
    .pointLightCount       = ARRAY_SIZE(_gNeoArkEveElevatorPointLights),
    .pointLights           = _gNeoArkEveElevatorPointLights,
    .coneLightCount        = 0,
    .coneLights            = NULL,
};

/// Depart through elevator endpoint 1 and arrive at Shelter B6 corridor endpoint 1.
enum { NEO_ARK_EVE_ELEVATOR_EXIT_WARP_SELECTORS = (1 << 4) | 1 };

/// Facing-gated exit quad requesting the Shelter B6 corridor, including the final-list marker.
///
/// Room setup binds and links this writable record. The room transition
/// receiver permits departure once CD dispatch is idle; the trigger persists
/// until the action list is cleared or the room is replaced.
static WorldCollisionTrigger _gNeoArkEveElevatorExitTriggers[1] = {
    { NULL, NULL, NULL, { -240, -48, 48, 0 }, { { -304, 0, -976, 0 }, { 304, 0, -976, 0 }, { -304, 0, 976, 0 }, { 304, 0, 976, 0 } }, { 0, 4109, 0, 0 }, { -ONE, 0, 0, 0 }, 1021, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_SHELTER_B6_CORRIDOR, NEO_ARK_EVE_ELEVATOR_EXIT_WARP_SELECTORS, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Cue-2 bases in the loaded type-1 bank; cue 1 selects the following entry.
enum {
    NEO_ARK_EVE_ELEVATOR_FOOTSTEP_WALK = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x5D,
    NEO_ARK_EVE_ELEVATOR_FOOTSTEP_RUN  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x5F,
};

/// Platform-floor walk/run cue pairs, with the walk pair also stored for scripted jumps.
static WorldCollisionFootstepSounds _gNeoArkEveElevatorFootstepSounds = {
    .walk         = NEO_ARK_EVE_ELEVATOR_FOOTSTEP_WALK,
    .run          = NEO_ARK_EVE_ELEVATOR_FOOTSTEP_RUN,
    .scriptedJump = NEO_ARK_EVE_ELEVATOR_FOOTSTEP_WALK,
};

/// Blocking, impact-enabled policy with pushback and no footsteps, for classes 0 and 2..7.
static WorldCollisionSurfaceProperties _gNeoArkEveElevatorSilentSurface = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

/// Platform floor's class-1 collision policy and paired footstep cues.
static WorldCollisionSurfaceProperties _gNeoArkEveElevatorFloorSurface = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gNeoArkEveElevatorFootstepSounds,
};

WorldCollisionSurfaceProperties* gNeoArkEveElevatorSurfaceProperties[8] = {
    &_gNeoArkEveElevatorSilentSurface,
    &_gNeoArkEveElevatorFloorSurface,
    &_gNeoArkEveElevatorSilentSurface,
    &_gNeoArkEveElevatorSilentSurface,
    &_gNeoArkEveElevatorSilentSurface,
    &_gNeoArkEveElevatorSilentSurface,
    &_gNeoArkEveElevatorSilentSurface,
    &_gNeoArkEveElevatorSilentSurface,
};

/// Refuses every key-item use in the EVE elevator.
///
/// `ROOM_MESSAGE_USE_KEY_ITEM` supplies the inventory item ID
/// and a zero second payload word. Returning 0 makes the item menu report that
/// the selected item cannot be used here.
static s32 _neoArkEveElevatorRejectKeyItemUse(Task* unusedTask, s32 unusedMessageId, s32 itemId, s32 unusedSecondArg)
{
    return ROOM_KEY_ITEM_USE_REFUSED;
}

/// Resolves a transition and gates the Shelter B6 corridor departure on CD dispatch readiness.
///
/// Copies the complete request to the reply, which may alias it, before resolving
/// its room. Returns 1 for other areas or an idle CD queue; otherwise returns 0.
/// Only an execute request in that latter case attempts CAP event 1 with actors paused.
/// An empty ring outside normal dispatch also takes that handled path.
/// Both records are borrowed during dispatch and must provide a complete `RoomEventMsg`.
static s32 _neoArkEveElevatorResolveTransition(Task* unusedTask, s32 unusedMessageId, RoomEventMsg* request, RoomEventMsg* reply)
{
    enum { NEO_ARK_EVE_ELEVATOR_CAP_CORRIDOR_DEPARTURE = 1 };

    *reply = *request;
    mapNeoArkResolveRoomVariant(request, reply);
    if (request->areaId != GAME_AREA_SHELTER_B6_CORRIDOR) {
        return ROOM_VARIANT_TRANSITION_DIRECT;
    }
    if (cdCmdIsIdle() != 0) {
        return ROOM_VARIANT_TRANSITION_DIRECT;
    }
    if (request->queryOnly != ROOM_EVENT_EXECUTE) {
        return ROOM_VARIANT_TRANSITION_REFUSED;
    }
    capSpawnEventIfIdle(NEO_ARK_EVE_ELEVATOR_CAP_CORRIDOR_DEPARTURE, CAP_EVENT_PAUSE_ACTORS);
    return ROOM_VARIANT_TRANSITION_REFUSED;
}

/// Ignores room commands sent to the EVE elevator and returns 0.
///
/// `ROOM_MESSAGE_COMMAND` supplies a command ID and an integer argument;
/// neither payload word is used by this room.
static s32 _neoArkEveElevatorIgnoreRoomCommand(Task* unusedTask, s32 unusedMessageId, s32 commandId, s32 commandArgument)
{
    return 0;
}

/// Ignores trigger action requests sent to the EVE elevator and returns 0.
///
/// `DIRECTION_MESSAGE_ROOM_ACTION` supplies a borrowed request and a zero
/// second payload word. The request is neither read nor retained.
static s32 _neoArkEveElevatorIgnoreRoomAction(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* request, s32 unusedSecondArg)
{
    return 0;
}

/// Installs the elevator's room-message receiver and advances to the idle state.
///
/// The session's room slot borrows the live task; teardown does not clear it.
static void _neoArkEveElevatorInitializeRoom(Task* task)
{
    task->msgTable = _gNeoArkEveElevatorRoomMessageTable;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    task->state = task->state + 1;
}

/// Keeps the room task in its idle state while its installed table receives messages.
static void _neoArkEveElevatorIdleRoomTask(Task* unusedTask)
{
}

void neoArkEveElevatorRoomTask(Task* task)
{
    TaskFuncTable3 handlers;

    handlers = _gNeoArkEveElevatorRoomTaskHandlers;
    handlers.funcs[task->state](task);
}

void neoArkEveElevatorIdleEffectTask(Task* unusedTask)
{
}
