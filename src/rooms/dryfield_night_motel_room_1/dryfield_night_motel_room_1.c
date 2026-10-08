#include "rooms/dryfield_night_motel_room_1.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>

#include "gte.h"
#include "types.h"

#include "dryfield_night_motel_room_1_private.h"

#include "gameplay/direction.h"
#include "gameplay/message.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/task.h"
#include "main/task_types.h"

#include "rooms/room_common.h"
#include "../../shared/glow_draw.h"
#include "../../shared/room_variants.h"

/// The room's message table, published at `Task::msgTable` by the room task.
extern TaskMessageEntry D_dryfield_night_motel_room_1_8017DA2C[];

static s32 _dryfieldNightMotelRoom1RejectKeyItemUse(Task* unusedTask, s32 unusedMessageId, s32 itemId, s32 unusedSecondArg);
static s32 _dryfieldNightMotelRoom1IgnoreRoomCommand(Task* unusedTask, s32 unusedMessageId, s32 commandId, s32 commandArgument);
static s32 _dryfieldNightMotelRoom1IgnoreRoomAction(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* request, s32 unusedSecondArg);

TaskMessageEntry D_dryfield_night_motel_room_1_8017DA2C[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _roomVariantMainStreetMsg },
    { ROOM_MESSAGE_USE_KEY_ITEM, _dryfieldNightMotelRoom1RejectKeyItemUse },
    { DIRECTION_MESSAGE_ROOM_ACTION, _dryfieldNightMotelRoom1IgnoreRoomAction },
    { ROOM_MESSAGE_COMMAND, _dryfieldNightMotelRoom1IgnoreRoomCommand },
    { TASK_MESSAGE_TABLE_END, NULL },
};

static void _dryfieldNightMotelRoom1InitRoomTask(Task* task);
static void _dryfieldNightMotelRoom1IdleRoomTask(Task* unusedTask);

/// Refuses every key-item use request in Dryfield night motel room 1.
///
/// Ignores the selected collected-item ID and returns `ROOM_KEY_ITEM_USE_REFUSED`
/// for the item menu's refusal notice, without consuming an item or starting an event.
static s32 _dryfieldNightMotelRoom1RejectKeyItemUse(Task* unusedTask, s32 unusedMessageId, s32 itemId, s32 unusedSecondArg)
{
    return ROOM_KEY_ITEM_USE_REFUSED;
}

#include "../../shared/room_variants_main_street.inc.c"

/// Ignores room commands without starting an action.
///
/// Both command words are unused; the signed message result is always zero.
static s32 _dryfieldNightMotelRoom1IgnoreRoomCommand(Task* unusedTask, s32 unusedMessageId, s32 commandId, s32 commandArgument)
{
    return 0;
}

/// Ignores room actions requested by direction triggers.
///
/// The borrowed request is neither read nor retained. The sender supplies zero
/// for the second payload and ignores the result, which is always zero.
static s32 _dryfieldNightMotelRoom1IgnoreRoomAction(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* request, s32 unusedSecondArg)
{
    return 0;
}

/// Registers night motel room 1's room-message receiver and advances to idle state 1.
///
/// Runs in room-task state 0. Borrows this overlay's message table and registers
/// the live task in `GAME_TASK_SLOT_ROOM`; keep the overlay loaded while it can
/// receive messages. Registration does not retain the task or clear on teardown.
static void _dryfieldNightMotelRoom1InitRoomTask(Task* task)
{
    task->msgTable = D_dryfield_night_motel_room_1_8017DA2C;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    task->state++;
}

/// Keeps night motel room 1's room-message receiver idle in state 1.
///
/// The task and its message table stay active between synchronous messages;
/// the separate state-2 handler performs teardown. The argument is unused.
static void _dryfieldNightMotelRoom1IdleRoomTask(Task* unusedTask)
{
}

/// The room task's three states.
static const TaskFuncTable3 D_dryfield_night_motel_room_1_8017D5C4 = {
    { _dryfieldNightMotelRoom1InitRoomTask, _dryfieldNightMotelRoom1IdleRoomTask, taskKill },
};

void dryfieldNightMotelRoom1RoomTask(Task* task)
{
    TaskFuncTable3 stateHandlers = D_dryfield_night_motel_room_1_8017D5C4;
    stateHandlers.funcs[task->state](task);
}

#include "../../shared/glow_draw_flare_clipped.inc.c"
