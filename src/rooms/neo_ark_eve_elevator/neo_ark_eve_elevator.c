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
#include "main/task.h"
#include "main/task_types.h"

#include "mapui/map_neo_ark.h"

#include "../../shared/room_variants.h"

/// The room's message table, which state 0 of its event task installs.
extern TaskMessageEntry D_neo_ark_eve_elevator_8017D724[];

static void _neoArkEveElevatorInitializeRoom(Task* task);
static void _neoArkEveElevatorIdleRoomTask(Task* unusedTask);

/// The event task's three states: install the message table, idle, and kill.
static const TaskFuncTable3 D_neo_ark_eve_elevator_8017D5C4 = {
    {
        _neoArkEveElevatorInitializeRoom,
        _neoArkEveElevatorIdleRoomTask,
        taskKill,
    },
};

/// Key-item requests delivered by the inventory menu to this room's task.
enum { NEO_ARK_EVE_ELEVATOR_MESSAGE_USE_KEY_ITEM = 0x13F1 };

static s32 _neoArkEveElevatorRejectKeyItemUse(Task* unusedTask, s32 unusedMessageId, s32 itemId, s32 unusedSecondArg);
static s32 _neoArkEveElevatorResolveTransition(Task* unusedTask, s32 unusedMessageId, RoomEventMsg* request, RoomEventMsg* reply);
static s32 _neoArkEveElevatorIgnoreRoomCommand(Task* unusedTask, s32 unusedMessageId, s32 commandId, s32 commandArgument);
static s32 _neoArkEveElevatorIgnoreRoomAction(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* request, s32 unusedSecondArg);

extern WorldCollisionGrid    D_neo_ark_eve_elevator_8017DA2C[1];
extern WorldCollisionTrigger D_neo_ark_eve_elevator_8017DBC8[1];
extern WorldCoordRoomLights  D_neo_ark_eve_elevator_8017DBB0[1];

TaskMessageEntry D_neo_ark_eve_elevator_8017D724[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _neoArkEveElevatorResolveTransition },
    { NEO_ARK_EVE_ELEVATOR_MESSAGE_USE_KEY_ITEM, _neoArkEveElevatorRejectKeyItemUse },
    { DIRECTION_MESSAGE_ROOM_ACTION, _neoArkEveElevatorIgnoreRoomAction },
    { ROOM_MESSAGE_COMMAND, _neoArkEveElevatorIgnoreRoomCommand },
    { TASK_MESSAGE_TABLE_END, NULL },
};

WorldCollisionRoomResources D_neo_ark_eve_elevator_8017D74C[1] = {
    { D_neo_ark_eve_elevator_8017DA2C, NULL, D_neo_ark_eve_elevator_8017DBC8, NULL },
};

WorldCoordRoomLighting D_neo_ark_eve_elevator_8017D75C[1] = {
    { D_neo_ark_eve_elevator_8017DBB0, NULL },
};

u8* D_neo_ark_eve_elevator_8017D764[1] = {
    gViewIdentityMap,
};

ViewCount D_neo_ark_eve_elevator_8017D768[1] = { 4 };

DirectionWarpEntry D_neo_ark_eve_elevator_8017D76C[1] = {
    { { { .word = 3072 }, -960, 0, 0 }, { 0, 0, 0, 0 }, { { .word = 3072 }, -960, 0, 0 }, { 0, 0, 0, 0 }, 0x55090002, 0x55090001, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, GAME_FLAG_MAP_MARK_POWER_PLANT_2 },
};

static SVECTOR _gNeoArkEveElevatorCollision0046CNormals[14] = {
#include "assets/neo_ark_eve_elevator_collision_0046C_normals.inc"
};

static SVECTOR _gNeoArkEveElevatorCollision0046CVerts[24] = {
#include "assets/neo_ark_eve_elevator_collision_0046C_verts.inc"
};

static WorldCollisionGridFace _gNeoArkEveElevatorCollision0046CFaces[24] = {
#include "assets/neo_ark_eve_elevator_collision_0046C_faces.inc"
};

static s16 _gNeoArkEveElevatorCollision0046CCells[26] = {
#include "assets/neo_ark_eve_elevator_collision_0046C_cells.inc"
};

#define GRID_CELL(i) (&_gNeoArkEveElevatorCollision0046CCells[i])
static s16* _gNeoArkEveElevatorCollision0046CTable[1] = {
#include "assets/neo_ark_eve_elevator_collision_0046C_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_neo_ark_eve_elevator_8017DA2C[1] = {
    { NULL, _gNeoArkEveElevatorCollision0046CNormals, _gNeoArkEveElevatorCollision0046CVerts, _gNeoArkEveElevatorCollision0046CFaces, _gNeoArkEveElevatorCollision0046CTable, 2050, 1050, 1, 1, 4000, 24 },
};

ViewCamera D_neo_ark_eve_elevator_8017DA50[4] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { 1000, 0x4E20, 0 } }, 1371 },
    { { { { 943, 0, -3985 }, { -3917, 755, -927 }, { 735, 4025, 174 } }, { 1340, 3520, 90 } }, 207 },
    { { { { 943, 0, -3985 }, { -3917, 755, -927 }, { 735, 4025, 174 } }, { 1340, 3520, 90 } }, 207 },
    { { { { 943, 0, -3985 }, { -3917, 755, -927 }, { 735, 4025, 174 } }, { 1340, 3520, 90 } }, 207 },
};

SpriteBatch D_neo_ark_eve_elevator_8017DAE0[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_neo_ark_eve_elevator_8017DAF0[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_neo_ark_eve_elevator_8017DB00[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_neo_ark_eve_elevator_8017DB10[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView D_neo_ark_eve_elevator_8017DB20[4] = {
    { { .empty = D_neo_ark_eve_elevator_8017DAE0 }, D_neo_ark_eve_elevator_8017DAE0, NULL },
    { { .empty = D_neo_ark_eve_elevator_8017DAF0 }, D_neo_ark_eve_elevator_8017DAF0, NULL },
    { { .empty = D_neo_ark_eve_elevator_8017DB00 }, D_neo_ark_eve_elevator_8017DB00, NULL },
    { { .empty = D_neo_ark_eve_elevator_8017DB10 }, D_neo_ark_eve_elevator_8017DB10, NULL },
};

WorldCoordPointLight D_neo_ark_eve_elevator_8017DB50[1] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -1000, -1742, 0 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1228, 2867, 2621 }, { 0, 0 } }, 1500, 2500 },
};

WorldCoordRoomLights D_neo_ark_eve_elevator_8017DBB0[1] = {
    { 0, NULL, ARRAY_SIZE(D_neo_ark_eve_elevator_8017DB50), D_neo_ark_eve_elevator_8017DB50, 0, NULL },
};

WorldCollisionTrigger D_neo_ark_eve_elevator_8017DBC8[1] = {
    { NULL, NULL, NULL, { -240, -48, 48, 0 }, { { -304, 0, -976, 0 }, { 304, 0, -976, 0 }, { -304, 0, 976, 0 }, { 304, 0, 976, 0 } }, { 0, 4109, 0, 0 }, { -4096, 0, 0, 0 }, 1021, WORLD_COLLISION_TRIGGER_ACTION_WARP, 24, 17, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionFootstepSounds D_neo_ark_eve_elevator_8017DC14 = {
    0x1000005D,
    0x1000005F,
    0x1000005D,
};

WorldCollisionSurfaceProperties D_neo_ark_eve_elevator_8017DC20[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_neo_ark_eve_elevator_8017DC28[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_neo_ark_eve_elevator_8017DC14 },
};

WorldCollisionSurfaceProperties* D_neo_ark_eve_elevator_8017DC30[8] = {
    D_neo_ark_eve_elevator_8017DC20,
    D_neo_ark_eve_elevator_8017DC28,
    D_neo_ark_eve_elevator_8017DC20,
    D_neo_ark_eve_elevator_8017DC20,
    D_neo_ark_eve_elevator_8017DC20,
    D_neo_ark_eve_elevator_8017DC20,
    D_neo_ark_eve_elevator_8017DC20,
    D_neo_ark_eve_elevator_8017DC20,
};

/// Refuses every key-item use in the EVE elevator.
///
/// `NEO_ARK_EVE_ELEVATOR_MESSAGE_USE_KEY_ITEM` supplies the inventory item ID
/// and a zero second payload word. Returning 0 makes the item menu report that
/// the selected item cannot be used here.
static s32 _neoArkEveElevatorRejectKeyItemUse(Task* unusedTask, s32 unusedMessageId, s32 itemId, s32 unusedSecondArg)
{
    enum { NEO_ARK_EVE_ELEVATOR_KEY_ITEM_REFUSED = 0 };

    return NEO_ARK_EVE_ELEVATOR_KEY_ITEM_REFUSED;
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
    task->msgTable = D_neo_ark_eve_elevator_8017D724;
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

    handlers = D_neo_ark_eve_elevator_8017D5C4;
    handlers.funcs[task->state](task);
}

void neoArkEveElevatorIdleEffectTask(Task* unusedTask)
{
}
