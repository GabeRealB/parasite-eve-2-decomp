#include "rooms/dryfield_night_motel_room_5.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>

#include "gte.h"
#include "types.h"

#include "dryfield_night_motel_room_5_private.h"

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

static s32 _roomVariantMotelBalconyMsg(Task* task, s32 messageId, const RoomEventMsg* request, RoomEventMsg* reply);

/// The room's message table, published at `Task::msgTable` by the room task.
extern TaskMessageEntry D_dryfield_night_motel_room_5_8017DA30[];

static s32  _dryfieldNightMotelRoom5RejectKeyItemUse(Task* task, s32 messageId, s32 itemId, s32 unusedArg);
static s32  _dryfieldNightMotelRoom5IgnoreRoomCommand(Task* task, s32 messageId, s32 commandId, s32 commandArg);
static s32  _dryfieldNightMotelRoom5IgnoreRoomAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedArg);
static void _dryfieldNightMotelRoom5InitRoomTask(Task* task);
static void _dryfieldNightMotelRoom5IdleRoomTask(Task* unusedTask);

TaskMessageEntry D_dryfield_night_motel_room_5_8017DA30[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _roomVariantMotelBalconyMsg },
    { ROOM_MESSAGE_USE_KEY_ITEM, _dryfieldNightMotelRoom5RejectKeyItemUse },
    { DIRECTION_MESSAGE_ROOM_ACTION, _dryfieldNightMotelRoom5IgnoreRoomAction },
    { ROOM_MESSAGE_COMMAND, _dryfieldNightMotelRoom5IgnoreRoomCommand },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// Refuses every key-item use request without consuming the selected item.
///
/// All arguments are unused. The item menu supplies the collected-item ID
/// and a zero second payload; the refusal reply selects its unavailable-item notice.
static s32 _dryfieldNightMotelRoom5RejectKeyItemUse(Task* task, s32 messageId, s32 itemId, s32 unusedArg)
{
    return ROOM_KEY_ITEM_USE_REFUSED;
}

#include "../../shared/room_variants_motel_balcony.inc.c"

/// Ignores room commands from scripts and triggers, returning zero.
///
/// The command ID and its integer argument are unused, as are the receiver
/// and message ID. No room state changes and no payload is retained.
static s32 _dryfieldNightMotelRoom5IgnoreRoomCommand(Task* task, s32 messageId, s32 commandId, s32 commandArg)
{
    return 0;
}

/// Ignores direction-triggered room actions and returns zero.
///
/// All arguments are unused. The four-byte request is borrowed only for
/// synchronous dispatch and is neither read nor retained; the second payload is zero.
static s32 _dryfieldNightMotelRoom5IgnoreRoomAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedArg)
{
    return 0;
}

/// Registers the room task and its message handlers, then enters its idle state.
///
/// Called in state 0 with a live task. The loaded room's table is borrowed
/// for subsequent synchronous messages while the registered task remains alive.
static void _dryfieldNightMotelRoom5InitRoomTask(Task* task)
{
    task->msgTable = D_dryfield_night_motel_room_5_8017DA30;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    task->state++;
}

/// Keeps room-task state 1 idle while its message handlers remain available.
///
/// Ignores the task argument and leaves its state and resources unchanged.
static void _dryfieldNightMotelRoom5IdleRoomTask(Task* unusedTask)
{
}

/// The room task's three states.
static const TaskFuncTable3 D_dryfield_night_motel_room_5_8017D5C4 = {
    { _dryfieldNightMotelRoom5InitRoomTask, _dryfieldNightMotelRoom5IdleRoomTask, taskKill },
};

/// The room task's callback: runs the state `Task::state` selects from a
/// stack copy of `D_dryfield_night_motel_room_5_8017D5C4`.
void func_dryfield_night_motel_room_5_8017D6D0(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_night_motel_room_5_8017D5C4;
    sp.funcs[task->state](task);
}

#include "../../shared/glow_draw_flare_clipped.inc.c"
