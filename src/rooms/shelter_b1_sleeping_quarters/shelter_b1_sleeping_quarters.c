#include "common.h"

#include "gameplay/268.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"
#include "rooms/room_common.h"

/// Spawn argument of a model task whose visibility follows a 2-bit game flag;
/// `flagId` selects the flag.
typedef struct {
    u8 unk0[8];
    u8 flagId;
} _SpawnArg;

extern s32      func_80179A04(RoomEventMsg* in, RoomEventMsg* out);
extern TaskDesc D_shelter_b1_sleeping_quarters_80180540;

/// The room's message table, which its cap scripts index.
extern GpMsgEntry D_shelter_b1_sleeping_quarters_80180518[];

/// Hides the task's model while the 2-bit game flag its spawn argument names
/// reads 2, and shows it otherwise.
void func_shelter_b1_sleeping_quarters_8017D608(Task* task)
{
    TmdObject* obj = task->extra;

    if (Gp_GetCurBit2Flag(((_SpawnArg*)task->spawnArg2)->flagId) == 2) {
        obj->flags |= 0x80;
    } else {
        obj->flags &= ~0x80;
    }
}

s32 func_shelter_b1_sleeping_quarters_8017D668(void)
{
    return 0;
}

s32 func_shelter_b1_sleeping_quarters_8017D670(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    func_80179A04(in, out);
    if (in->msgId != 0xC) {
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

s32 func_shelter_b1_sleeping_quarters_8017D6FC(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 8) {
        Gp_MsgPlayerWeapon(0);
        Task_SpawnFromTable(&D_shelter_b1_sleeping_quarters_80180540, 0, 8, 0);
    }
    if (arg2 == 3) {
        Gp_SpawnIfCapIdle(GameFlag_GetNibble(0x16F) == 0 ? 3 : 0xF, 0);
    }
    return 0;
}

s32 func_shelter_b1_sleeping_quarters_8017D770(void)
{
    return 0;
}

void func_shelter_b1_sleeping_quarters_8017D778(Task* task)
{
    switch (task->state) {
        case 0:
            Gp_CapFile = 0;
            Gp_LoadCapFile(1);
            func_800E6D4C(0x2C0, 0);
            Gp_RunCapCmd(task->spawnArg1, 1);
            task->state++;
            break;
        case 1:
            if (Gp_CapBusy() == 0) {
                task->state++;
            }
            break;
        case 2:
            Gp_MsgPlayerWeapon(1);
            Gp_ResetCap();
            taskKill(task);
            break;
    }
}

/// First state of the room task: installs the room's message table, publishes
/// the task in pointer slot 7 and advances to the next state.
void func_shelter_b1_sleeping_quarters_8017D83C(Task* task)
{
    task->msgTable = D_shelter_b1_sleeping_quarters_80180518;
    Game_SetPtrSlot(task, 7);
    task->state = (s32)(task->state + 1);
}

/// Idle state of the room task.
void func_shelter_b1_sleeping_quarters_8017D880(Task* task)
{
}

/// The room task's three states, run from a stack copy by
/// `func_shelter_b1_sleeping_quarters_8017D888`: the entry tick, the idle
/// state, then `taskKill`.
const TaskFuncTable3 D_shelter_b1_sleeping_quarters_8017D5C4 = {
    { func_shelter_b1_sleeping_quarters_8017D83C, func_shelter_b1_sleeping_quarters_8017D880, taskKill },
};

/// Runs the room task's current state from a stack copy of its state table.
void func_shelter_b1_sleeping_quarters_8017D888(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_b1_sleeping_quarters_8017D5C4;
    sp.funcs[task->state](task);
}
