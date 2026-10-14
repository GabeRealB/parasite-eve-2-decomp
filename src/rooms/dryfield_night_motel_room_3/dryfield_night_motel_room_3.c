#include "rooms/dryfield_night_motel_room_3.h"

#include "types.h"

#include "gameplay/direction.h"
#include "gameplay/message.h"

#include "main/gameflag.h"
#include "main/session.h"
#include "main/task.h"
#include "main/task_types.h"
#include "../../shared/room/room_variants.h"

static s32 _dryfieldNightMotelRoom3RejectKeyItemUse(Task* unusedTask, s32 unusedMessageId, s32 itemId, s32 unusedSecondArg);
static s32 _dryfieldNightMotelRoom3IgnoreRoomCommand(Task* unusedTask, s32 unusedMessageId, s32 commandId, s32 commandArgument);
static s32 _dryfieldNightMotelRoom3IgnoreRoomAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedArg);

/// Night motel room 3's transition, key-item, direction-action and command handlers.
///
/// Borrowed by the live room task; keep this overlay loaded while it receives
/// messages. The final end marker makes unsupported IDs return zero.
static const TaskMessageEntry _gDryfieldNightMotelRoom3RoomMessages[5] SECTION(".data") = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _roomVariantMainStreetMsg },
    { ROOM_MESSAGE_USE_KEY_ITEM, _dryfieldNightMotelRoom3RejectKeyItemUse },
    { DIRECTION_MESSAGE_ROOM_ACTION, _dryfieldNightMotelRoom3IgnoreRoomAction },
    { ROOM_MESSAGE_COMMAND, _dryfieldNightMotelRoom3IgnoreRoomCommand },
    { TASK_MESSAGE_TABLE_END, NULL },
};

static void _dryfieldNightMotelRoom3InitRoomTask(Task* task);
static void _dryfieldNightMotelRoom3IdleRoomTask(Task* unusedTask);

/// Refuses every key-item use request in Dryfield night motel room 3.
///
/// Ignores the selected collected-item ID and returns `ROOM_KEY_ITEM_USE_REFUSED`
/// for the item menu's refusal notice, without consuming an item or starting an event.
static s32 _dryfieldNightMotelRoom3RejectKeyItemUse(Task* unusedTask, s32 unusedMessageId, s32 itemId, s32 unusedSecondArg)
{
    return ROOM_KEY_ITEM_USE_REFUSED;
}

#include "../../shared/room/room_variants_main_street.inc.c"

/// Ignores room commands without starting an action.
///
/// Both command words are unused; the signed message result is always zero.
static s32 _dryfieldNightMotelRoom3IgnoreRoomCommand(Task* unusedTask, s32 unusedMessageId, s32 commandId, s32 commandArgument)
{
    return 0;
}

/// Ignores direction-triggered room actions and returns zero.
///
/// All arguments are unused. The four-byte request is borrowed only for
/// synchronous dispatch and is neither read nor retained; the second payload is zero.
static s32 _dryfieldNightMotelRoom3IgnoreRoomAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedArg)
{
    return 0;
}

/// Registers night motel room 3's room-message receiver and advances to idle state 1.
///
/// Runs in room-task state 0. Borrows this overlay's message table and registers
/// the live task in `GAME_TASK_SLOT_ROOM`; keep the overlay loaded while it can
/// receive messages. Registration does not retain the task or clear on teardown.
static void _dryfieldNightMotelRoom3InitRoomTask(Task* task)
{
    task->msgTable = _gDryfieldNightMotelRoom3RoomMessages;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    task->state++;
}

/// Keeps night motel room 3's room-message receiver idle in state 1.
///
/// The task and its message table stay active between synchronous messages;
/// the separate state-2 handler performs teardown. The argument is unused.
static void _dryfieldNightMotelRoom3IdleRoomTask(Task* unusedTask)
{
}

/// Room-task states 0 initialize, 1 remain available for messages, and 2 release the task.
///
/// Copied whole before unchecked state dispatch in 0..2; all three callback
/// addresses are borrowed from loaded code. There is no terminator.
static const TaskFuncTable3 _gDryfieldNightMotelRoom3RoomTaskStates = {
    { _dryfieldNightMotelRoom3InitRoomTask, _dryfieldNightMotelRoom3IdleRoomTask, taskKill },
};

void dryfieldNightMotelRoom3RoomTask(Task* task)
{
    TaskFuncTable3 stateHandlers = _gDryfieldNightMotelRoom3RoomTaskStates;
    stateHandlers.funcs[task->state](task);
}
