#include "rooms/dryfield_night_water_tower.h"

#include "types.h"

#include "dryfield_night_water_tower_private.h"

#include "gameplay/captions.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/gameflag.h"
#include "gameplay/sound.h"
#include "gameplay/items.h"
#include "gameplay/message.h"
#include "gameplay/scene_combat.h"

#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"

#include "rooms/room_common.h"
// The flag symbol is four bytes; the gate writes the first.
#define ROOM_EVENT_ACTIVE gRoomEventActive.eventStarted
#include "../../shared/room_events.h"

extern RoomEventActiveBytes gRoomEventActive;

/// The event the room's gate `_roomEventGate`
/// latched: the incoming message and the request, kept for the event task it
/// spawns from `gRoomEventTaskDesc`, and the flag the gate
/// sets once it has done so.
extern RoomEventMsg gRoomEventMsg;
extern RoomEventReq gRoomEventReq;

RoomEventMsg gRoomEventMsg = { 0 };

RoomEventActiveBytes gRoomEventActive = { 0, { 1, 238, 253 } };

RoomEventReq gRoomEventReq = { 0 };

static void _dryfieldNightWaterTowerInitializeRoomTask(Task* task);
static void _dryfieldNightWaterTowerIdleRoomTask(Task* unusedTask);

#include "../../shared/room_event_gate.inc.c"

#include "../../shared/room_event_task.inc.c"

#include "../../shared/water_tower_event_msg.inc.c"

#include "../../shared/water_tower_sound_msg.inc.c"

s32 dryfieldNightWaterTowerRejectKeyItemUse(Task* unusedTask, s32 unusedMessageId, s32 itemId, s32 unusedSecondArg)
{
    return ROOM_KEY_ITEM_USE_REFUSED;
}

/// The room's handler for message 0x13F0: script event 7 starts CAP slot 7;
/// every event answers 0.
s32 func_dryfield_night_water_tower_8017DAA4(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    if (arg2 == 7) {
        capStartSequenceSlot(7, 1, 3);
    }
    return 0;
}

s32 dryfieldNightWaterTowerIgnoreActionMessage(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* request, s32 unusedSecondArg)
{
    return 0;
}

/// Registers the night water-tower room task and enables its message handlers.
///
/// Called in state 0 with a live task; advances to the idle state 1. The room
/// overlay and its message table must remain loaded for the task's lifetime.
static void _dryfieldNightWaterTowerInitializeRoomTask(Task* task)
{
    task->msgTable = D_dryfield_night_water_tower_8017E6EC;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    task->state = task->state + 1;
}

/// Keeps the registered room task alive in state 1 to receive messages.
static void _dryfieldNightWaterTowerIdleRoomTask(Task* unusedTask)
{
}

/// The room entry task's three states: install the room's message table,
/// idle, and `taskKill`.
static const TaskFuncTable3 D_dryfield_night_water_tower_8017D5DC = {
    { _dryfieldNightWaterTowerInitializeRoomTask, _dryfieldNightWaterTowerIdleRoomTask, taskKill },
};

void dryfieldNightWaterTowerRoomTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = D_dryfield_night_water_tower_8017D5DC;
    stateHandlers.funcs[task->state](task);
}
