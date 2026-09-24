#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "rooms/room_common.h"

extern u8 D_80115598;

extern s32 func_80179A04(RoomEventMsg* in, RoomEventMsg* out);

/// Message table the room task installs on itself: ids 0x13EE-0x13F2 mapped
/// to the room's handlers, closed by id 0x7FFFFFFF.
extern GpMsgEntry D_shelter_b1_storeroom_80184968[];

void func_shelter_b1_storeroom_8017D740(Task* task);
void func_shelter_b1_storeroom_8017D78C(Task* task);

/// The room task's three states, dispatched by
/// `func_shelter_b1_storeroom_8017D794`: install the message table, idle, end.
const TaskFuncTable3 D_shelter_b1_storeroom_8017D5C4 = {
    { func_shelter_b1_storeroom_8017D740, func_shelter_b1_storeroom_8017D78C, taskKill }
};

s32 func_shelter_b1_storeroom_8017D5FC(void)
{
    return 0;
}

s32 func_shelter_b1_storeroom_8017D604(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    func_80179A04(in, out);
    if (in->msgId == 0xD && GameFlag_GetNibble(0xA6) == 0) {
        if (in->field_5 != 0) {
            return 0;
        }
        Gp_SetNibbleIf(in->field_6, 2);
        Gp_RunCapCmd1(1);
        return 0;
    }
    if (in->msgId != 0xC && in->msgId != 0xA) {
        return 1;
    }
    if (GameFlag_GetNibble(0x7A) < 6) {
        return 1;
    }
    if (in->field_5 != 0) {
        return 0;
    }
    Gp_RunCapCmd1(0xE);
    return 0;
}

s32 func_shelter_b1_storeroom_8017D6E0(void)
{
    return 0;
}

s32 func_shelter_b1_storeroom_8017D6E8(void)
{
    return 0;
}

s32 func_shelter_b1_storeroom_8017D6F0(s32 arg0, s32 arg1, s32 arg2)
{
    switch (arg2) {
        case 8:
            SndEvt_EnqueueType6(0x540B0008, 0, 0);
            break;
        case 0x6A:
            SndEvt_EnqueueType6(0x540B0009, 0, 0);
            break;
    }
    return 0;
}

/// Installs the room's message table on `task`, registers the task in pointer
/// slot 7, sets `D_80115598` and advances to the idle state.
void func_shelter_b1_storeroom_8017D740(Task* task)
{
    task->msgTable = D_shelter_b1_storeroom_80184968;
    Game_SetPtrSlot(task, 7);
    task->state = (s32)(task->state + 1);
    D_80115598  = 1;
}

void func_shelter_b1_storeroom_8017D78C(Task* task)
{
}

/// Runs the handler for the task's state from the room's state table.
void func_shelter_b1_storeroom_8017D794(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_b1_storeroom_8017D5C4;
    sp.funcs[task->state](task);
}
