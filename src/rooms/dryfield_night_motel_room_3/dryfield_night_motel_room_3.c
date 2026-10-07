#include "rooms/dryfield_night_motel_room_3.h"

#include "types.h"

#include "gameplay/direction.h"
#include "gameplay/message.h"

#include "main/gameflag.h"
#include "main/session.h"
#include "main/task.h"
#include "main/task_types.h"
#include "../../shared/room_variants.h"

/// The room's message table, published at `Task::msgTable` by the room task.
extern TaskMessageEntry D_dryfield_night_motel_room_3_8017DA5C[];

static s32 _dryfieldNightMotelRoom3RejectKeyItemUse(Task* unusedTask, s32 unusedMessageId, s32 itemId, s32 unusedSecondArg);
static s32 _dryfieldNightMotelRoom3IgnoreRoomCommand(Task* unusedTask, s32 unusedMessageId, s32 commandId, s32 commandArgument);
s32        func_dryfield_night_motel_room_3_8017D68C(Task*, s32, s32, s32);

TaskMessageEntry D_dryfield_night_motel_room_3_8017DA5C[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _roomVariantMainStreetMsg },
    { ROOM_MESSAGE_USE_KEY_ITEM, _dryfieldNightMotelRoom3RejectKeyItemUse },
    { DIRECTION_MESSAGE_ROOM_ACTION, func_dryfield_night_motel_room_3_8017D68C },
    { ROOM_MESSAGE_COMMAND, _dryfieldNightMotelRoom3IgnoreRoomCommand },
    { TASK_MESSAGE_TABLE_END, NULL },
};

static void func_dryfield_night_motel_room_3_8017D694(Task* task);
static void func_dryfield_night_motel_room_3_8017D6D8(Task* task);

/// Refuses every key-item use request in Dryfield night motel room 3.
///
/// Ignores the selected collected-item ID and returns `ROOM_KEY_ITEM_USE_REFUSED`
/// for the item menu's refusal notice, without consuming an item or starting an event.
static s32 _dryfieldNightMotelRoom3RejectKeyItemUse(Task* unusedTask, s32 unusedMessageId, s32 itemId, s32 unusedSecondArg)
{
    return ROOM_KEY_ITEM_USE_REFUSED;
}

#include "../../shared/room_variants_main_street.inc.c"

/// Ignores room commands without starting an action.
///
/// Both command words are unused; the signed message result is always zero.
static s32 _dryfieldNightMotelRoom3IgnoreRoomCommand(Task* unusedTask, s32 unusedMessageId, s32 commandId, s32 commandArgument)
{
    return 0;
}

/// Message-table handler for id 0x13EF: accepts the message and does nothing.
s32 func_dryfield_night_motel_room_3_8017D68C(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

/// First state of the room task: publishes the room's message table, claims
/// pointer slot 7 and advances to the next state.
static void func_dryfield_night_motel_room_3_8017D694(Task* task)
{
    task->msgTable = D_dryfield_night_motel_room_3_8017DA5C;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    task->state = (s32)(task->state + 1);
}

/// Second state of the room task, run every frame: does nothing.
static void func_dryfield_night_motel_room_3_8017D6D8(Task* task)
{
}

/// The room task's three states.
static const TaskFuncTable3 D_dryfield_night_motel_room_3_8017D5C4 = {
    { func_dryfield_night_motel_room_3_8017D694, func_dryfield_night_motel_room_3_8017D6D8, taskKill },
};

/// The room task's callback: runs the state `Task::state` selects from a
/// stack copy of `D_dryfield_night_motel_room_3_8017D5C4`.
void func_dryfield_night_motel_room_3_8017D6E0(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_night_motel_room_3_8017D5C4;
    sp.funcs[task->state](task);
}
