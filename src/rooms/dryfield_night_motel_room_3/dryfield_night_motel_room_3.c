#include "common.h"

#include "gameplay/D4.h"

#include "main/gameflag.h"
#include "main/session.h"
#include "main/task.h"

#include "rooms/room_common.h"

/// The room's message table, published at `Task::msgTable` by the room task.
extern GpMsgEntry D_dryfield_night_motel_room_3_8017DA5C[];

/// Message-table handler for id 0x13F1: accepts the message and does nothing.
s32 func_dryfield_night_motel_room_3_8017D5F4(void)
{
    return 0;
}

/// Message-table handler for id 0x13EE: echoes the incoming record into the
/// reply and, for a message 2 that is not report-only (`field_5 == 0`),
/// answers game nibble 0x61 plus one while nibble 0x7A is below 4, and 3 once
/// it has reached 4. Returns 1.
s32 func_dryfield_night_motel_room_3_8017D5FC(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    s32 val;
    s32 n;

    *out = *in;
    if (in->msgId == 2 && in->field_5 == 0) {
        n = GameFlag_GetNibble(0x7A);
        if (n < 4) {
            val = 3;
            TOUCH_REG(val);
            val = GameFlag_GetNibble(0x61) + 1;
        } else {
            val = 3;
        }
        out->field_3 = val;
    }
    return 1;
}

/// Message-table handler for id 0x13F0: accepts the message and does nothing.
s32 func_dryfield_night_motel_room_3_8017D684(void)
{
    return 0;
}

/// Message-table handler for id 0x13EF: accepts the message and does nothing.
s32 func_dryfield_night_motel_room_3_8017D68C(void)
{
    return 0;
}

/// First state of the room task: publishes the room's message table, claims
/// pointer slot 7 and advances to the next state.
void func_dryfield_night_motel_room_3_8017D694(Task* task)
{
    task->msgTable = D_dryfield_night_motel_room_3_8017DA5C;
    Game_SetPtrSlot(task, 7);
    task->state = (s32)(task->state + 1);
}

/// Second state of the room task, run every frame: does nothing.
void func_dryfield_night_motel_room_3_8017D6D8(Task* task)
{
}

/// The room task's three states.
const TaskFuncTable3 D_dryfield_night_motel_room_3_8017D5C4 = {
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
