#include "common.h"

#include "gameplay/268.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "rooms/room_common.h"

extern s16 D_80071076;
extern u8  D_8007216C;
extern u8  D_80115598;
extern u8  D_80115690;

/// The event message and request the gate latched for the event task, and the
/// flag saying one was latched this call.
extern RoomEventMsg D_dryfield_night_general_store_801858BC;
extern RoomEventReq D_dryfield_night_general_store_801858C8;
extern u8           D_dryfield_night_general_store_801858C4;

/// Descriptor of the event task `func_dryfield_night_general_store_8017D794`.
extern TaskDesc D_dryfield_night_general_store_8017E78C;

/// The room's two spawnable tasks: entry 0 the CAP-command task
/// `func_dryfield_night_general_store_8017DCA8`, entry 1 the cutscene task
/// `func_dryfield_night_general_store_8017DAF0`.
extern TaskDesc D_dryfield_night_general_store_8017E798[];

/// The room's message table, installed by the room task's entry state.
extern GpMsgEntry D_dryfield_night_general_store_8017E7BC[];

/// The save's stage byte as it was when the cutscene began, restored into
/// `D_8007216C` when the cutscene is cut short.
extern u8 D_dryfield_night_general_store_801858B4;

/// The record handed to helper task 0x31 when the cutscene asks for it.
extern GpFadeWork D_dryfield_night_general_store_801858B8;

/// The warp point and room the cutscene task commits to, latched from the
/// message that spawned it.
extern u8 D_dryfield_night_general_store_801858C5;
extern u8 D_dryfield_night_general_store_801858C6;

void func_dryfield_night_general_store_8017DE34(Task* arg0);
void func_dryfield_night_general_store_8017DE80(Task* task);

/// The room's event gate. Returns 1 when game-flag nibble `req->flagId`
/// already reads set (clear, for a negative id). Otherwise, when
/// `req->itemId` has been collected or is 0, it returns 2 and - unless
/// `msg->field_5` asks for a dry run - latches `msg` and `req`, sets the
/// nibble and spawns the event task. When the item is missing it returns 0
/// and, outside a dry run, runs cap command `req->field_4`.
s32 func_dryfield_night_general_store_8017D630(RoomEventReq* req, RoomEventMsg* msg)
{
    s32 flag;
    s32 id;
    s32 mode;
    s32 got;
    s32 ret;
    s32 neg;

    flag                                    = req->flagId;
    D_dryfield_night_general_store_801858C4 = 0;
    neg                                     = flag < 0;
    got                                     = (s16)flag;
    if (neg) {
        flag = -flag;
        got  = GameFlag_GetNibble(flag) == 0;
    } else {
        got = GameFlag_GetNibble(got);
    }
    ret = 1;
    if (got == 0) {
        if (Gp_HasCollectedBit(req->itemId) != 0 || req->itemId == 0) {
            ret = 2;
            if (msg->field_5 == 0) {
                D_dryfield_night_general_store_801858BC = *msg;
                D_dryfield_night_general_store_801858C8 = *req;
                id                                      = req->flagId;
                mode                                    = 1;
                if (id < 0) {
                    id   = -id;
                    mode = 0;
                }
                GameFlag_SetNibble(id, mode);
                Task_SpawnFromTable(&D_dryfield_night_general_store_8017E78C, 0, 0, 0);
                D_dryfield_night_general_store_801858C4 = 1;
                return 2;
            }
            return ret;
        }
        ret = 0;
        if (msg->field_5 == 0) {
            Gp_RunCapCmd1(req->field_4);
            Gp_SetNibbleIf(msg->field_6, 2);
            ret = 0;
        }
        return ret;
    }
    return ret;
}

/// The event task the gate spawns: runs the latched request's cap command,
/// plays its two sound ids in turn, each waited out, then warps to the area,
/// warp point and room the latched message names.
void func_dryfield_night_general_store_8017D794(Task* task)
{
    switch (task->state) {
        case 0:
            Gp_StateF0.field_4 = 1;
            Gp_MsgPlayerWeapon(0);
            Gp_RunCapCmd1(D_dryfield_night_general_store_801858C8.field_0);
            if (D_dryfield_night_general_store_801858C8.field_8 != 0) {
                SndEvt_EnqueueType6(D_dryfield_night_general_store_801858C8.field_8, 0, 0);
                task->state++;
            } else {
                task->state = 2;
            }
            break;
        case 1:
            if (SndVoice_HasActiveId(D_dryfield_night_general_store_801858C8.field_8) == 0) {
                task->state++;
            }
            break;
        case 2:
            task->state++;
            break;
        case 3:
            if (D_dryfield_night_general_store_801858C8.field_C != 0) {
                SndEvt_EnqueueType6(D_dryfield_night_general_store_801858C8.field_C, 0, 0);
                task->state++;
            } else {
                task->state = 5;
            }
            break;
        case 4:
            if (SndVoice_HasActiveId(D_dryfield_night_general_store_801858C8.field_C) == 0) {
                task->state++;
            }
            break;
        case 5:
            SndEvt_EnqueueType7(0x80000000, 0);
            D_80071076               = 1;
            Mc_SaveData.at4.loc.area = D_dryfield_night_general_store_801858BC.msgId;
            Mc_SaveData.at4.loc.warp = D_dryfield_night_general_store_801858BC.field_2;
            Mc_SaveData.at4.loc.room = (u8)D_dryfield_night_general_store_801858BC.field_3;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(task);
            break;
    }
}

/// Handler for the room's event ids 1 and 0x26. Both write a reply byte into
/// `out->field_3` unless `in->field_5` asks for a dry run.
///
/// For id 1 the reply is the id itself while flag nibble 0x63 is clear;
/// otherwise 4, or 2 + nibble 0x61 while nibble 0x7A is below 4. It then
/// offers the event gate a request that plays stage sounds 0x5203000C and
/// 0x52030003 under flag nibble 0x3B, and returns the gate's answer.
///
/// For id 0x26 the reply is 1 (2 with nibble 0x53 set), plus 2 while nibble
/// 0x51 is clear, when nibble 0xC9 is set; otherwise 5, or 6 while nibble 0x51
/// is clear. Outside a dry run it spawns the cutscene task with `field_2` /
/// `field_3` latched as its destination, or runs CAP command 0xE once nibble
/// 0x62 is set, and answers 2. Any other id answers 1.
s32 func_dryfield_night_general_store_8017D904(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    RoomEventReq req;
    u16          msgId;
    s32          v;

    *out  = *in;
    msgId = in->msgId;
    if (msgId == 1 && in->field_5 == 0) {
        if (GameFlag_GetNibble(0x63) == 0) {
            out->field_3 = msgId;
        } else {
            if (GameFlag_GetNibble(0x7A) < 4) {
                v = 4;
                TOUCH_REG(v);
                v = GameFlag_GetNibble(0x61) + 2;
            } else {
                v = 4;
            }
            out->field_3 = v;
        }
    }
    if (in->msgId == 0x26 && in->field_5 == 0) {
        if (GameFlag_GetNibble(0xC9) != 0) {
            if (GameFlag_GetNibble(0x53) == 0) {
                out->field_3 = 1;
            } else {
                out->field_3 = 2;
            }
            if (GameFlag_GetNibble(0x51) == 0) {
                out->field_3 = out->field_3 + 2;
            }
        } else if (GameFlag_GetNibble(0x51) == 0) {
            out->field_3 = 6;
        } else {
            out->field_3 = 5;
        }
    }
    if (in->msgId == 1) {
        req.field_0 = 0xD;
        req.field_4 = 0xD;
        req.field_8 = Gp_PackStageSndId(0x5203000C);
        req.field_C = Gp_PackStageSndId(0x52030003);
        req.flagId  = 0x3B;
        req.itemId  = 0;
        return func_dryfield_night_general_store_8017D630(&req, in);
    }
    if (in->msgId != 0x26) {
        return 1;
    }
    if (in->field_5 != 0) {
        return 2;
    }
    if (GameFlag_GetNibble(0x62) == 0) {
        Task_SpawnFromTable(D_dryfield_night_general_store_8017E798, 1, 0, 0);
        D_dryfield_night_general_store_801858C5 = in->field_2;
        D_dryfield_night_general_store_801858C6 = in->field_3;
    } else {
        Gp_RunCapCmd1(0xE);
    }
    return 2;
}

/// The room's cutscene task, a six-state script. State 0 silences the
/// player's weapon messages, saves the stage byte `Mc_SaveData.at4.loc.view`
/// and forces it to 0x10; states 1 and 3 each let one frame pass. State 2
/// queues stage sound 0x5203000D, runs CAP command 0xF and raises
/// `Gp_StateF0.field_4` / `D_80115690`.
///
/// State 4 checks the CAP event key: 0xB spawns helper task 0x31 with a
/// zeroed record whose `field_2` is 8 and moves on; any other key ends the
/// cutscene - `Gp_StateF0.field_4` cleared, stage sound 0x5203000E, the saved stage
/// byte written to `D_8007216C` and the weapon messages re-enabled. State 5
/// queues sound event 0x80000000, points the save's location at area 0x26
/// with the latched warp point and room, raises `D_80071076` and spawns
/// helper task 0x11. Both finishing arms kill the task.
void func_dryfield_night_general_store_8017DAF0(Task* arg0)
{
    switch (arg0->state) {
        case 0:
            Gp_MsgPlayerWeapon(0);
            Gp_MsgPlayer3F3(0);
            D_dryfield_night_general_store_801858B4 = Mc_SaveData.at4.loc.view;
            Mc_SaveData.at4.loc.view                = 0x10;
            arg0->state                            += 1;
            return;
        case 1:
        case 3:
            arg0->state += 1;
            return;
        case 2:
            Gp_EnqueueStageSnd6(0x5203000D, 0, 0);
            Gp_StateF0.field_4 = 1;
            Gp_RunCapCmd1(0xF);
            D_80115690   = 1;
            arg0->state += 1;
            return;
        case 4:
            if (Gp_GetCapEventKey() == 0xB) {
                D_dryfield_night_general_store_801858B8.field_0 = 0;
                D_dryfield_night_general_store_801858B8.field_1 = 0;
                D_dryfield_night_general_store_801858B8.field_2 = 8;
                Task_Spawn(1, 0x31, 0, (s32)&D_dryfield_night_general_store_801858B8);
                arg0->state += 1;
                return;
            }
            Gp_StateF0.field_4 = 0;
            Gp_EnqueueStageSnd6(0x5203000E, 0, 0);
            D_8007216C = D_dryfield_night_general_store_801858B4;
            Gp_MsgPlayerWeapon(1);
            Gp_MsgPlayer3F3(1);
            break;
        case 5:
            SndEvt_EnqueueType7(0x80000000, 0);
            Mc_SaveData.at4.loc.area = 0x26;
            Mc_SaveData.at4.loc.warp = D_dryfield_night_general_store_801858C5;
            Mc_SaveData.at4.loc.room = D_dryfield_night_general_store_801858C6;
            D_80071076               = 1;
            Task_Spawn(0, 0x11, 0, 0);
            break;
        default:
            return;
    }
    taskKill(arg0);
}

/// The room task's three-state table, run from a stack copy by
/// `func_dryfield_night_general_store_8017DE88`: the entry state
/// `func_dryfield_night_general_store_8017DE34`, the idle state
/// `func_dryfield_night_general_store_8017DE80`, then `taskKill`.
const TaskFuncTable3 D_dryfield_night_general_store_8017D5F4 = {
    { func_dryfield_night_general_store_8017DE34, func_dryfield_night_general_store_8017DE80, taskKill },
};

/// A task that runs cap command `spawnArg2` and waits for it to finish; if the
/// cap then reports an event key of 0xA or above, it toggles game-flag nibble
/// `spawnArg1` between 0 and 1. The task then kills itself.
void func_dryfield_night_general_store_8017DCA8(Task* task)
{
    s32 flag;
    s32 cmd;

    flag = task->spawnArg1;
    cmd  = (s32)task->spawnArg2;
    switch (task->state) {
        case 0:
            Gp_RunCapCmd1(cmd);
            goto advance;
        case 1:
            if (Gp_CapBusy() != 0) {
                break;
            }
            goto advance;
        case 2:
            if (Gp_GetCapEventKey() >= 0xA) {
                GameFlag_SetNibble(flag, GameFlag_GetNibble(flag) == 0);
            }
        advance:
            task->state = task->state + 1;
            break;
        case 3:
            taskKill(task);
            break;
    }
}

/// Message handler for actions 0x18 and 9. Action 0x18 hands
/// `Gp_SpawnIfCapIdle` 0x18 when pointer slot 0xA holds a task and 0x19
/// otherwise; action 9 spawns the room's CAP-command task to run CAP command 9
/// and toggle flag nibble 0x53. Always returns 0.
s32 func_dryfield_night_general_store_8017DD88(s32 arg0, s32 arg1, s32 arg2)
{
    s32   arg;
    void* slot;

    if (arg2 == 0x18) {
        slot = gameGetPtrSlot(0xA);
        arg  = 0x19;
        if (slot != 0) {
            arg = 0x18;
        }
        Gp_SpawnIfCapIdle(arg, 0);
    }
    if (arg2 == 9) {
        Task_SpawnFromTable(D_dryfield_night_general_store_8017E798, 0, 0x53, 9);
    }
    return 0;
}

/// Message handler that plays stage sound 0x52030007 on action 7. Always
/// returns 0.
s32 func_dryfield_night_general_store_8017DDF0(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    if (arg2 == 7) {
        Gp_EnqueueStageSnd6(0x52030000 | 7, 0, 0);
    }
    return 0;
}

/// Message handler that takes no action and reports the message as not
/// handled.
s32 func_dryfield_night_general_store_8017DE24(void)
{
    return 0;
}

/// Message handler that takes no action and reports the message as not
/// handled.
s32 func_dryfield_night_general_store_8017DE2C(void)
{
    return 0;
}

/// Entry state of the room task: installs the room's message table, publishes
/// the task in pointer slot 7, advances to the idle state and raises
/// `D_80115598`.
void func_dryfield_night_general_store_8017DE34(Task* arg0)
{
    arg0->msgTable = D_dryfield_night_general_store_8017E7BC;
    Game_SetPtrSlot(arg0, 7);
    arg0->state = (s32)(arg0->state + 1);
    D_80115598  = 1;
}

/// Idle state of the room task: does nothing.
void func_dryfield_night_general_store_8017DE80(Task* task)
{
}

/// The room task: runs the state `D_dryfield_night_general_store_8017D5F4`
/// names for `task->state`, through a stack copy of the table.
void func_dryfield_night_general_store_8017DE88(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_night_general_store_8017D5F4;
    sp.funcs[task->state](task);
}
