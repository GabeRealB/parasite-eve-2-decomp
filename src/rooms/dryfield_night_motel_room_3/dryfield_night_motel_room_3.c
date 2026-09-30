#include "rooms/dryfield_night_motel_room_3.h"

#include "types.h"

#include "gameplay/message.h"

#include "main/gameflag.h"
#include "main/session.h"
#include "main/task.h"
#include "main/task_types.h"

/// The room's message table, published at `Task::msgTable` by the room task.
extern GpMsgEntry D_dryfield_night_motel_room_3_8017DA5C[];

s32 func_dryfield_night_motel_room_3_8017D5F4(Task*, s32, TaskMessageArg, TaskMessageArg);
s32 func_dryfield_night_motel_room_3_8017D5FC(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32 func_dryfield_night_motel_room_3_8017D684(Task*, s32, TaskMessageArg, TaskMessageArg);
s32 func_dryfield_night_motel_room_3_8017D68C(Task*, s32, TaskMessageArg, TaskMessageArg);

GpMsgEntry D_dryfield_night_motel_room_3_8017DA5C[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_dryfield_night_motel_room_3_8017D5FC },
    { 5105, func_dryfield_night_motel_room_3_8017D5F4 },
    { 5103, func_dryfield_night_motel_room_3_8017D68C },
    { 5104, func_dryfield_night_motel_room_3_8017D684 },
    { 0x7FFFFFFF, NULL },
};

static void func_dryfield_night_motel_room_3_8017D694(Task* task);
static void func_dryfield_night_motel_room_3_8017D6D8(Task* task);

/// Message-table handler for id 0x13F1: accepts the message and does nothing.
s32 func_dryfield_night_motel_room_3_8017D5F4(Task* task, s32 msgId, TaskMessageArg arg2, TaskMessageArg arg3)
{
    return 0;
}

/// Message-table handler for id 0x13EE: echoes the incoming record into the
/// reply and, for a message 2 that is not report-only (`queryOnly == 0`),
/// answers game nibble 0x61 plus one while game nibble 0x7A is below 4, and 3
/// once it has reached 4. Returns 1.
s32 func_dryfield_night_motel_room_3_8017D5FC(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    s32 val;
    s32 n;

    *out = *in;
    if (in->areaId == 2 && in->queryOnly == ROOM_EVENT_EXECUTE) {
        n = GameFlag_GetNibble(0x7A);
        if (n >= 4) {
            val = 3;
        } else {
            val = GameFlag_GetNibble(0x61) + 1;
        }
        out->room = val;
    }
    return 1;
}

/// Message-table handler for id 0x13F0: accepts the message and does nothing.
s32 func_dryfield_night_motel_room_3_8017D684(Task* task, s32 msgId, TaskMessageArg arg2, TaskMessageArg arg3)
{
    return 0;
}

/// Message-table handler for id 0x13EF: accepts the message and does nothing.
s32 func_dryfield_night_motel_room_3_8017D68C(Task* task, s32 msgId, TaskMessageArg arg2, TaskMessageArg arg3)
{
    return 0;
}

/// First state of the room task: publishes the room's message table, claims
/// pointer slot 7 and advances to the next state.
static void func_dryfield_night_motel_room_3_8017D694(Task* task)
{
    task->msgTable = D_dryfield_night_motel_room_3_8017DA5C;
    Game_SetPtrSlot(task, 7);
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
