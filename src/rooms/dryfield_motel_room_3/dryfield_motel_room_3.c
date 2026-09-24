#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/D4.h"

#include "main/session.h"
#include "main/task.h"

#include "rooms/room_common.h"

/// The room's message table, published at `Task::msgTable` by the room task.
extern GpMsgEntry D_dryfield_motel_room_3_8017D6B4[];

/// Message-table handler for id 0x13F1: accepts the message and does nothing.
s32 func_dryfield_motel_room_3_8017D5D0(void)
{
    return 0;
}

/// Message-table handler for id 0x13EE: echoes the incoming record into the
/// reply unchanged and returns 1.
s32 func_dryfield_motel_room_3_8017D5D8(Task* task, s32 msgId, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    return 1;
}

/// Message-table handler for id 0x13F0: accepts the message and does nothing.
s32 func_dryfield_motel_room_3_8017D600(void)
{
    return 0;
}

/// Message-table handler for id 0x13EF: accepts the message and does nothing.
s32 func_dryfield_motel_room_3_8017D608(void)
{
    return 0;
}

/// First state of the room task: publishes the room's message table, claims
/// pointer slot 7 and advances to the next state.
void func_dryfield_motel_room_3_8017D610(Task* task)
{
    task->msgTable = D_dryfield_motel_room_3_8017D6B4;
    Game_SetPtrSlot(task, 7);
    task->state = (s32)(task->state + 1);
}

/// Second state of the room task: the room has nothing to do each frame.
void func_dryfield_motel_room_3_8017D654(Task* task)
{
}

/// The room task's three states.
const TaskFuncTable3 D_dryfield_motel_room_3_8017D5C4 = {
    { func_dryfield_motel_room_3_8017D610, func_dryfield_motel_room_3_8017D654, taskKill },
};

/// The room task's callback: runs the state `Task::state` selects from a
/// stack copy of `D_dryfield_motel_room_3_8017D5C4`.
void func_dryfield_motel_room_3_8017D65C(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_motel_room_3_8017D5C4;
    sp.funcs[task->state](task);
}
