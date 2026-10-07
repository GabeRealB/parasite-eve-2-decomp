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

s32 func_dryfield_night_motel_room_5_8017D5F0(Task*, s32, s32, s32);
s32 func_dryfield_night_motel_room_5_8017D674(Task*, s32, s32, s32);
s32 func_dryfield_night_motel_room_5_8017D67C(Task*, s32, s32, s32);

TaskMessageEntry D_dryfield_night_motel_room_5_8017DA30[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _roomVariantMotelBalconyMsg },
    { 5105, func_dryfield_night_motel_room_5_8017D5F0 },
    { DIRECTION_MESSAGE_ROOM_ACTION, func_dryfield_night_motel_room_5_8017D67C },
    { ROOM_MESSAGE_COMMAND, func_dryfield_night_motel_room_5_8017D674 },
    { TASK_MESSAGE_TABLE_END, NULL },
};

static void func_dryfield_night_motel_room_5_8017D684(Task* task);
static void func_dryfield_night_motel_room_5_8017D6C8(Task* task);

/// Message-table handler for id 0x13F1: accepts the message and does nothing.
s32 func_dryfield_night_motel_room_5_8017D5F0(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

#include "../../shared/room_variants_motel_balcony.inc.c"

/// Message-table handler for id 0x13F0: accepts the message and does nothing.
s32 func_dryfield_night_motel_room_5_8017D674(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

/// Message-table handler for id 0x13EF: accepts the message and does nothing.
s32 func_dryfield_night_motel_room_5_8017D67C(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

/// First state of the room task: publishes the room's message table, claims
/// pointer slot 7 and advances to the next state.
static void func_dryfield_night_motel_room_5_8017D684(Task* task)
{
    task->msgTable = D_dryfield_night_motel_room_5_8017DA30;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    task->state = (s32)(task->state + 1);
}

/// Second state of the room task: the room has nothing to do each frame.
static void func_dryfield_night_motel_room_5_8017D6C8(Task* task)
{
}

/// The room task's three states.
static const TaskFuncTable3 D_dryfield_night_motel_room_5_8017D5C4 = {
    { func_dryfield_night_motel_room_5_8017D684, func_dryfield_night_motel_room_5_8017D6C8, taskKill },
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
