#include "rooms/neo_ark_bridge.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/rand.h>

#include "gte.h"
#include "types.h"

#include "gameplay/direction.h"
#include "gameplay/message.h"
#include "gameplay/scene_combat.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/gfx.h"
#include "main/gfxgte.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"

#include "mapui/map_neo_ark.h"

#include "overlay.h"
#include "../../shared/water_effects.h"
#include "../../shared/room_variants.h"

s32     rcos(s32);
s32     rsin(s32);
MATRIX* TransposeMatrix(MATRIX*, MATRIX*);

/// The room's message table.
extern TaskMessageEntry D_neo_ark_bridge_80181F30[];

static s32  _neoArkBridgeRejectKeyItemUse(Task* task, s32 messageId, s32 itemId, s32 unusedArg);
static s32  _neoArkBridgeResolveRoomTransition(Task* task, s32 messageId, RoomEventMsg* request, RoomEventMsg* reply);
static s32  _neoArkBridgeIgnoreRoomCommand(Task* task, s32 messageId, s32 commandId, s32 commandArg);
static s32  _neoArkBridgeIgnoreRoomAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedArg);
static void _neoArkBridgeInitRoomTask(Task* task);
static void _neoArkBridgeIdleRoomTask(Task* unusedTask);

TaskDesc D_neo_ark_bridge_80181F18 = { { { TASK_BODY_NONE, 192 } }, waterRefractionTask, { .value = 0 } };

TaskDesc D_neo_ark_bridge_80181F24 = { { { TASK_BODY_NONE, 192 } }, waterDistortBandTask, { .value = 0 } };

TaskMessageEntry D_neo_ark_bridge_80181F30[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _neoArkBridgeResolveRoomTransition },
    { ROOM_MESSAGE_USE_KEY_ITEM, _neoArkBridgeRejectKeyItemUse },
    { DIRECTION_MESSAGE_ROOM_ACTION, _neoArkBridgeIgnoreRoomAction },
    { ROOM_MESSAGE_COMMAND, _neoArkBridgeIgnoreRoomCommand },
    { TASK_MESSAGE_TABLE_END, NULL },
};

#include "../../shared/water_refraction_task.inc.c"

#include "../../shared/water_distort_band_task.inc.c"

/// Refuses every key-item use request without consuming the selected item.
///
/// All arguments are unused. The item menu supplies the collected-item ID
/// and a zero second payload; the refusal reply selects its unavailable-item notice.
static s32 _neoArkBridgeRejectKeyItemUse(Task* task, s32 messageId, s32 itemId, s32 unusedArg)
{
    return ROOM_KEY_ITEM_USE_REFUSED;
}

/// Resolves a room-transition destination from Neo Ark progress and permits departure.
///
/// Handles `ROOM_EVENT_MESSAGE_RESOLVE`; the task and message ID are unused.
/// Request and reply borrow complete eight-byte records and may be the same object.
/// The full record is copied before the stage resolver updates its room selector;
/// queries preserve the copied destination. Area and arrival selectors must be
/// valid in the destination stage. Neither pointer is retained. Always returns
/// `ROOM_VARIANT_TRANSITION_DIRECT`, even when the destination is unchanged.
static s32 _neoArkBridgeResolveRoomTransition(Task* task, s32 messageId, RoomEventMsg* request, RoomEventMsg* reply)
{
    *reply = *request;
    mapNeoArkResolveRoomVariant(request, reply);
    return ROOM_VARIANT_TRANSITION_DIRECT;
}

/// Ignores room commands from scripts and triggers, returning zero.
///
/// The command ID and its integer argument are unused, as are the receiver
/// and message ID. No room state changes and no payload is retained.
static s32 _neoArkBridgeIgnoreRoomCommand(Task* task, s32 messageId, s32 commandId, s32 commandArg)
{
    return 0;
}

/// Ignores direction-triggered room actions and returns zero.
///
/// All arguments are unused. The four-byte request is borrowed only for
/// synchronous dispatch and is neither read nor retained; the second payload is zero.
static s32 _neoArkBridgeIgnoreRoomAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedArg)
{
    return 0;
}

/// Registers the room task and starts the bridge's two ambient sound scripts.
///
/// Called in state 0 with a live task; enters the idle state after queuing both
/// sounds with no pan offset or attenuation. The loaded room's message table
/// is borrowed while the registered task remains alive.
static void _neoArkBridgeInitRoomTask(Task* task)
{
    task->msgTable = D_neo_ark_bridge_80181F30;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    sndEvtRequestScriptStart(SOUND_NEO_ARK_BRIDGE_AMBIENCE_1, 0, 0);
    sndEvtRequestScriptStart(SOUND_NEO_ARK_BRIDGE_AMBIENCE_2, 0, 0);
    task->state++;
}

/// Keeps room-task state 1 idle while its message handlers remain available.
///
/// Ignores the task argument and leaves its state and resources unchanged.
static void _neoArkBridgeIdleRoomTask(Task* unusedTask)
{
}

/// State handlers of the room's entry task, indexed by its state through
/// `neoArkBridgeRoomTask`: set-up, idle, then kill.
static const TaskFuncTable3 D_neo_ark_bridge_8017D614 = {
    { _neoArkBridgeInitRoomTask, _neoArkBridgeIdleRoomTask, taskKill }
};

void neoArkBridgeRoomTask(Task* task)
{
    TaskFuncTable3 stateHandlers = D_neo_ark_bridge_8017D614;
    stateHandlers.funcs[task->state](task);
}
