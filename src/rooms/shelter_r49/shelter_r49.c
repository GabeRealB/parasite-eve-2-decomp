#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/session.h"
#include "main/task.h"
#include "rooms/room_common.h"

extern s32 func_80179A04(RoomEventMsg* in, RoomEventMsg* out);

extern GpMsgEntry D_shelter_r49_8017D9D8[];

extern s32 D_80133560;
extern s32 D_80133860;

void func_shelter_r49_8017D648(Task* arg0);
void func_shelter_r49_8017D6B4(Task* task);

/// The room task's states: set up, idle, then `taskKill`.
const TaskFuncTable3 D_shelter_r49_8017D5C4 = {
    { func_shelter_r49_8017D648, func_shelter_r49_8017D6B4, taskKill },
};

/// The room's handler for message 0x13F1: does nothing and returns 0.
s32 func_shelter_r49_8017D5EC(void)
{
    return 0;
}

/// The room's handler for message 0x13EE: copies the incoming record onto the
/// outgoing one, passes both to `func_80179A04` and returns 1.
s32 func_shelter_r49_8017D5F4(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    func_80179A04(in, out);
    return 1;
}

/// The room's handler for message 0x13F0: does nothing and returns 0.
s32 func_shelter_r49_8017D638(void)
{
    return 0;
}

/// The room's handler for message 0x13EF: does nothing and returns 0.
s32 func_shelter_r49_8017D640(void)
{
    return 0;
}

/// The room task's setup state: installs the room's message table, stores the
/// task in pointer slot 7 and, unless `Mc_SaveData.demoScene` is 9, calls
/// `func_800E8634`.
void func_shelter_r49_8017D648(Task* arg0)
{
    arg0->msgTable = D_shelter_r49_8017D9D8;
    Game_SetPtrSlot(arg0, 7);
    if (Mc_SaveData.demoScene != 9) {
        func_800E8634((s32)&D_80133560, 0, (s32)&D_80133860);
    }
    arg0->state = (s32)(arg0->state + 1);
}

/// The room task's idle state: does nothing. The unused local reproduces the
/// original's stack frame.
void func_shelter_r49_8017D6B4(Task* task)
{
    char pad[0x10];
}

/// The room task: copies its three-state table to the stack and runs the
/// entry the task's state selects.
void func_shelter_r49_8017D6C4(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_r49_8017D5C4;
    sp.funcs[task->state](task);
}
