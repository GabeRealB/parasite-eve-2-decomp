#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "rooms/acropolis_fountain.h"
#include "rooms/room_common.h"

extern GpMsgEntry D_acropolis_fountain_8017E764[];
extern TaskDesc   D_acropolis_fountain_8017E78C[];
extern u8         D_acropolis_fountain_80183BB0;
extern u8         D_acropolis_fountain_80183BB1;

void func_acropolis_fountain_8017D960(Task* arg0);
void func_acropolis_fountain_8017D9BC(Task* task);

/// State handlers of the room task: set-up, an empty per-frame tick and
/// `taskKill`.
const TaskFuncTable3 D_acropolis_fountain_8017D5C4 = {
    { func_acropolis_fountain_8017D960, func_acropolis_fountain_8017D9BC, taskKill },
};

/// Message gate for the fountain's hotspot: copies the incoming record to the
/// outgoing one, then edits the copy's `field_3` (the answer the caller acts
/// on) according to the message id and the room's progress nibbles. Message 3
/// before nibble 0 reaches 5 hands the record's first two bytes to
/// `D_acropolis_fountain_80183BB0`/`BB1` and spawns the room's own task,
/// consuming the message (returns 0); from nibble 0 == 5 on it only answers.
s32 func_acropolis_fountain_8017D604(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    s32 msgId;

    *out  = *in;
    msgId = in->msgId;
    if (msgId == 3) {
        if (GameFlag_GetNibble(0) < 5) {
            if (in->field_5 == 0) {
                D_acropolis_fountain_80183BB0 = in->field_2;
                D_acropolis_fountain_80183BB1 = in->field_3;
                Task_SpawnFromTable(D_acropolis_fountain_8017E78C, 0, 0, 0);
            }
            return 0;
        }
        if (in->msgId == msgId && in->field_5 == 0) {
            if (GameFlag_GetNibble(0) < 2) {
                if (GameFlag_GetNibble(0x21) < 2) {
                    out->field_3 = 1;
                } else {
                    out->field_3 = 2;
                }
            } else {
                out->field_3 = msgId;
            }
        }
    } else if (msgId == 9) {
        if (GameFlag_GetNibble(9) & 1) {
            out->field_3 = 2;
        }
        if (in->field_5 == 0 && GameFlag_GetNibble(0x13) == 0) {
            GameFlag_SetNibble(0x13, 1);
        }
    }
    return 1;
}

/// Handler for message 0x13F1 in the room task's message table: ignores the
/// message and answers 0.
s32 func_acropolis_fountain_8017D774(void)
{
    return 0;
}

s32 func_acropolis_fountain_8017D77C(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    s32 args[2];

    if (arg2 == 3) {
        Gp_RunCapCmd1(((GameFlag_GetNibble(9) & 2) == 0) ? 3 : 6);
    }
    if (arg2 == 4) {
        Gp_StartCapSlot(4, 1, 0);
        func_acropolis_fountain_8017DA1C();
        GameFlag_SetNibble(0x12, 1);
    }
    return 0;
}

s32 func_acropolis_fountain_8017D7F4(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    switch (arg2) {
        case 3:
            SndEvt_EnqueueType6(0x51080003, 0, 0);
            break;
        case 4:
            SndEvt_EnqueueType6(0x51080004, 0, 0);
            break;
        case 9:
            SndEvt_EnqueueType6(0x51080009, 0, 0);
            break;
    }
    return 0;
}

void func_acropolis_fountain_8017D868(Task* task)
{
    switch (task->state) {
        case 0:
            Gp_RunCapCmd1(1);
            task->state = task->state + 1;
            break;

        case 1:
            if (Gp_CapBusy() == 0) {
                task->state = task->state + 1;
            }
            /* fallthrough */

        case 2:
            SndEvt_EnqueueType7(0x80000000, 0);
            Mc_SaveData.at4.loc.area  = 3;
            Mc_SaveData.at4.loc.room  = 3;
            Mc_SaveData.at4.loc.warp  = D_acropolis_fountain_80183BB0;
            gDisplayState.roomVariant = 1;
            Task_Spawn(0, 0x11, 0, 0);
            GameFlag_SetNibble(0, 5);
            taskKill(task);
            break;
    }
}

void func_acropolis_fountain_8017D960(Task* arg0)
{
    arg0->msgTable = D_acropolis_fountain_8017E764;
    Game_SetPtrSlot(arg0, 7);
    if (GameFlag_GetNibble(0x12) != 0) {
        func_acropolis_fountain_8017DA1C();
    }
    arg0->state = (s32)(arg0->state + 1);
}

void func_acropolis_fountain_8017D9BC(Task* task)
{
}

/// Runs the room task's current state through a stack copy of its three-entry
/// state table.
void func_acropolis_fountain_8017D9C4(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_acropolis_fountain_8017D5C4;
    sp.funcs[task->state](task);
}
