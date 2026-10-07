#include "rooms/dryfield_night_water_tower.h"

#include "types.h"

#include "dryfield_night_water_tower_private.h"

#include "gameplay/captions.h"
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

/// The event the room's gate `roomEventGate`
/// latched: the incoming message and the request, kept for the event task it
/// spawns from `gRoomEventTaskDesc`, and the flag the gate
/// sets once it has done so.
extern RoomEventMsg gRoomEventMsg;
extern RoomEventReq gRoomEventReq;

RoomEventMsg gRoomEventMsg = { 0 };

RoomEventActiveBytes gRoomEventActive = { 0, { 1, 238, 253 } };

RoomEventReq gRoomEventReq = { 0 };

static void func_dryfield_night_water_tower_8017DADC(Task* task);
static void func_dryfield_night_water_tower_8017DB20(Task* task);

#include "../../shared/room_event_gate.inc.c"

#include "../../shared/room_event_task.inc.c"

#include "../../shared/water_tower_event_msg.inc.c"

#include "../../shared/water_tower_sound_msg.inc.c"

/// The room's handler for message 0x13F1: answers 0.
s32 func_dryfield_night_water_tower_8017DA9C(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
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

/// The room's handler for message 0x13EF: answers 0.
s32 func_dryfield_night_water_tower_8017DAD4(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

/// State 0 of the room entry task: installs the room's message table,
/// registers the task in game pointer slot 7 and advances to the idle state.
static void func_dryfield_night_water_tower_8017DADC(Task* task)
{
    task->msgTable = D_dryfield_night_water_tower_8017E6EC;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    task->state = (s32)(task->state + 1);
}

/// State 1 of the room entry task: idles.
static void func_dryfield_night_water_tower_8017DB20(Task* task)
{
}

/// The room entry task's three states: install the room's message table,
/// idle, and `taskKill`.
static const TaskFuncTable3 D_dryfield_night_water_tower_8017D5DC = {
    { func_dryfield_night_water_tower_8017DADC, func_dryfield_night_water_tower_8017DB20, taskKill },
};

/// Runs the room entry task's current state from its three-entry table, which
/// it copies onto the stack before the call.
void func_dryfield_night_water_tower_8017DB28(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_night_water_tower_8017D5DC;
    sp.funcs[task->state](task);
}
