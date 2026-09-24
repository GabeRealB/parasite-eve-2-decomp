#include "common.h"

#include "gameplay/D4.h"
#include "main/session.h"
#include "main/task.h"
#include "rooms/room_common.h"

extern s32 func_80179A04(RoomEventMsg* in, RoomEventMsg* out);

/// The room's message table, handed to its event task in state 0.
extern GpMsgEntry D_shelter_r37_8017D6D0[];

/// The room's handler for message 0x13F1: does nothing and returns 0.
s32 func_shelter_r37_8017D5D0(void)
{
    return 0;
}

/// The room's handler for message 0x13EE: copies the incoming record onto the
/// outgoing one, passes both to `func_80179A04` and returns 1.
s32 func_shelter_r37_8017D5D8(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    func_80179A04(in, out);
    return 1;
}

/// The room's handler for message 0x13F0: does nothing and returns 0.
s32 func_shelter_r37_8017D61C(void)
{
    return 0;
}

/// The room's handler for message 0x13EF: does nothing and returns 0.
s32 func_shelter_r37_8017D624(void)
{
    return 0;
}

/// State 0 of the room's event task: installs the room's message table,
/// publishes the task in pointer slot 7 and advances to state 1.
void func_shelter_r37_8017D62C(Task* task)
{
    task->msgTable = D_shelter_r37_8017D6D0;
    Game_SetPtrSlot(task, 7);
    task->state = (s32)(task->state + 1);
}

/// State 1 of the room's event task: does nothing, so the task idles here.
void func_shelter_r37_8017D670(Task* task)
{
}

/// The event task's three states: install the message table, idle, and kill.
const TaskFuncTable3 D_shelter_r37_8017D5C4 = {
    {
        func_shelter_r37_8017D62C,
        func_shelter_r37_8017D670,
        taskKill,
    },
};

/// The room's event task: runs the handler for its current state, through a
/// stack copy of the state table.
void func_shelter_r37_8017D678(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_r37_8017D5C4;
    sp.funcs[task->state](task);
}
