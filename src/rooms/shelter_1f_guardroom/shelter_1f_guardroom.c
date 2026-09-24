#include "common.h"

#include "gameplay/1A8.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/stream.h"
#include "main/task.h"

extern GpCapCmd** Gp_CapCmds;

/// The room's message table, installed on its event task in state 0.
extern GpMsgEntry D_shelter_1f_guardroom_8017DA30[];
extern TaskDesc   D_shelter_1f_guardroom_8017DA60;
extern TaskDesc   D_shelter_1f_guardroom_8017DA6C;
extern Task*      D_shelter_1f_guardroom_8017E014;

extern void func_80179B14(GpSaveLoc* src, GpSaveLoc* dst);

void func_shelter_1f_guardroom_8017D824(Task* arg0);
void func_shelter_1f_guardroom_8017D878(Task* task);
void func_shelter_1f_guardroom_8017D9CC(s32 arg0);

/// The event task's three states: set-up, idle, and kill.
const TaskFuncTable3 D_shelter_1f_guardroom_8017D5C4 = {
    {
        func_shelter_1f_guardroom_8017D824,
        func_shelter_1f_guardroom_8017D878,
        taskKill,
    },
};

/// Cutscene task spawned from the 0x13F0 handler: runs cap command 2, waits for
/// it, and when the cap event key reads 0xB hides the HUD and runs the task
/// described at `D_shelter_1f_guardroom_8017DA6C` until it is killed. It then
/// restores the HUD, calls `func_shelter_1f_guardroom_8017D9CC(1)`, sets game
/// nibble 0xB2 to 1 and hands the weapon back. Any other key ends it at once.
void func_shelter_1f_guardroom_8017D5E8(Task* task)
{
    s32 poll;

    switch (task->state) {
        case 0:
            Gp_RunCapCmd1(2);
            goto next;
        case 1:
            if (Gp_CapBusy() == 0) {
                goto next;
            }
            break;
        case 2:
            Gp_CapCmds[2]->field_4 = 1;
            if (Gp_GetCapEventKey() != 0xB) {
                taskKill(task);
                Gp_MsgPlayerWeapon(1);
                break;
            }
            gGameSession->hideHud           = 1;
            D_shelter_1f_guardroom_8017E014 = Task_SpawnFromTable(&D_shelter_1f_guardroom_8017DA6C, 0, 0, 0);
            task->state++;
            break;
        case 3:
            if (Task_PollKill(D_shelter_1f_guardroom_8017E014, &poll) == 0) {
                break;
            }
        next:
            task->state++;
            break;
        case 4:
            gGameSession->hideHud = 0;
            func_shelter_1f_guardroom_8017D9CC(1);
            GameFlag_SetNibble(0xB2, 1);
            Gp_MsgPlayerWeapon(1);
            taskKill(task);
            break;
    }
}

/// The room's handler for message 0x13F1: does nothing and returns 0.
s32 func_shelter_1f_guardroom_8017D73C(void)
{
    return 0;
}

/// The room's handler for message 0x13EE: copies the incoming `GpSaveLoc` onto
/// the outgoing one, passes both to `func_80179B14`, and returns 1.
s32 func_shelter_1f_guardroom_8017D744(s32 arg0, s32 arg1, GpSaveLoc* in, GpSaveLoc* out)
{
    *out = *in;
    func_80179B14(in, out);
    return 1;
}

/// The room's handler for message 0x13F0: when its third argument is 2 and game
/// nibble 0xB2 is still clear, takes the weapon away and spawns the cutscene task
/// `func_shelter_1f_guardroom_8017D5E8`; once the nibble is set it runs cap
/// command 3 instead. Returns 0.
s32 func_shelter_1f_guardroom_8017D788(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 2) {
        if (GameFlag_GetNibble(0xB2) == 0) {
            Gp_MsgPlayerWeapon(0);
            Task_SpawnFromTable(&D_shelter_1f_guardroom_8017DA60, 0, 0, 0);
        } else {
            Gp_RunCapCmd1(3);
        }
    }
    return 0;
}

/// The room's handler for message 0x13EF: does nothing and returns 0.
s32 func_shelter_1f_guardroom_8017D7E8(void)
{
    return 0;
}

/// The room's handler for message 0x13F2: when its third argument is 3, queues
/// sound event 0x55060003. Returns 0.
s32 func_shelter_1f_guardroom_8017D7F0(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 3) {
        SndEvt_EnqueueType6(0x55060003, 0, 0);
    }
    return 0;
}

/// State 0 of the room's event task: installs the room's message table,
/// publishes the task in pointer slot 7, passes game nibble 0xB2 to
/// `func_shelter_1f_guardroom_8017D9CC` and advances to state 1.
void func_shelter_1f_guardroom_8017D824(Task* arg0)
{
    arg0->msgTable = D_shelter_1f_guardroom_8017DA30;
    Game_SetPtrSlot(arg0, 7);
    func_shelter_1f_guardroom_8017D9CC(GameFlag_GetNibble(0xB2) & 0xFF);
    arg0->state = (s32)(arg0->state + 1);
}

/// State 1 of the room's event task: does nothing, so the task idles here.
void func_shelter_1f_guardroom_8017D878(Task* task)
{
}

/// The room's event task: copies its state table onto the stack and calls the
/// entry for the current state.
void func_shelter_1f_guardroom_8017D880(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_1f_guardroom_8017D5C4;
    sp.funcs[task->state](task);
}

/// Task the cutscene task spawns: calls `func_shelter_1f_guardroom_8017D9CC(0)`,
/// raises the CD queue's `field_1EA`, enqueues CD command 0x61 for the stream
/// slot of the session's current location, waits for the queue's `field_1FA`,
/// then for the CD to go idle, and requests its own kill.
void func_shelter_1f_guardroom_8017D8D8(Task* arg0)
{
    u8          slotParam[4];
    CdCmdQueue* queue;

    queue = &CdCmd_Queue;
    switch (arg0->state) {
        case 0:
            func_shelter_1f_guardroom_8017D9CC(0);
            queue->field_1EA = 1;
            slotParam[0]     = Stream_FindSlot(gGameSession->at4.raw.data, 0, 0);
            CdCmd_Enqueue(0x61, 0, slotParam);
            arg0->state++;
            break;
        case 1:
            if (queue->field_1FA != 0) {
                arg0->state = 2;
            }
            break;
        case 2:
            if (CdCmd_IsIdle() & 0xFFFF) {
                Task_RequestKill(arg0, 0);
            }
            break;
    }
}

/// Sets `field_4` of the second sprite command in entry 2 of the current
/// area's sprite table: 1 when the low byte of `arg0` is zero, 0 otherwise.
void func_shelter_1f_guardroom_8017D9CC(s32 arg0)
{
    GpAreaKey* sess = &gGameSession->at4.loc;
    GpSprtCmd* cmd;

    cmd = Gp_SprtTables[sess->stage - 1][0].field_0[sess->area - 1][2].field_4;
    if ((arg0 & 0xFF) == 0) {
        cmd[1].field_4 = 1;
    } else {
        cmd[1].field_4 = 0;
    }
}

void func_shelter_1f_guardroom_8017DA28(void)
{
}
