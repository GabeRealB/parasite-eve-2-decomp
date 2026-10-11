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
#include "../../shared/room_variants.h"
#include "../../shared/water_tower.h"

extern RoomEventActiveBytes gRoomEventActive;

/// The event the room's gate `_roomEventGate`
/// latched: the incoming message and the request, kept for the event task it
/// spawns from `gRoomEventTaskDesc`, and the flag the gate
/// sets once it has done so.
extern RoomEventReq gRoomEventReq;

/// Snapshot of the accepted transition for the deferred room event.
///
/// The gate copies all eight bytes before spawning; another accepted start
/// replaces them. The task commits area, arrival and the room byte on reload.
/// Keep this room loaded and the snapshot unchanged until that task finishes.
static RoomEventMsg _gRoomEventMsg = { 0 };

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

s32 dryfieldNightWaterTowerCommandMessage(Task* unusedTask, s32 unusedMessageId, s32 commandId, s32 unusedSecondArg)
{
    enum {
        DRYFIELD_NIGHT_WATER_TOWER_COMMAND_MECHANISM = 7,
        DRYFIELD_NIGHT_WATER_TOWER_CAP_MECHANISM_KEY = 3,
    };

    if (commandId == DRYFIELD_NIGHT_WATER_TOWER_COMMAND_MECHANISM) {
        capStartSequenceSlot(DRYFIELD_NIGHT_WATER_TOWER_COMMAND_MECHANISM, CAP_PLAYBACK_DISPLAY_TRANSITION,
                             DRYFIELD_NIGHT_WATER_TOWER_CAP_MECHANISM_KEY);
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
    task->msgTable = gDryfieldNightWaterTowerRoomMessages;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    task->state = task->state + 1;
}

/// Keeps the registered room task alive in state 1 to receive messages.
static void _dryfieldNightWaterTowerIdleRoomTask(Task* unusedTask)
{
}

/// Room-task handlers indexed by state 0..2.
///
/// 0 initializes the receiver, 1 waits for messages, 2 tears down.
/// Dispatch copies all three pointers to the stack; there is no bounds check
/// or terminator. Keep this overlay loaded until the task exits.
static const TaskFuncTable3 _gDryfieldNightWaterTowerRoomStates = {
    { _dryfieldNightWaterTowerInitializeRoomTask, _dryfieldNightWaterTowerIdleRoomTask, taskKill },
};

void dryfieldNightWaterTowerRoomTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = _gDryfieldNightWaterTowerRoomStates;
    stateHandlers.funcs[task->state](task);
}
