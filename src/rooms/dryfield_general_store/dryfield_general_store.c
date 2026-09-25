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

/// The two caption flags the store's cutscene task raises: `Gp_StateF0.field_4` while
/// the script owns the screen and `D_80115690` when CAP command 0xF showed the
/// letterbox. `Mc_SaveData.at4.loc.view` is the area-record id the same task publishes, and
/// `D_80071076` the "chapter advanced" halfword it sets on the way out.
extern u8 D_80115690;
extern u8 D_80115598;

/// The event message and request the gate latched for the event task, and the
/// flag saying one was latched this call.
extern RoomEventMsg D_dryfield_general_store_80185700;
extern RoomEventReq D_dryfield_general_store_8018570C;
extern u8           D_dryfield_general_store_80185708;

/// Descriptor of the event task `func_dryfield_general_store_8017D764`.
extern TaskDesc D_dryfield_general_store_8017E158;

extern TaskDesc D_dryfield_general_store_8017E164;

/// The stage byte `Mc_SaveData.at4.loc.view` held when the cutscene began, saved by
/// `func_dryfield_general_store_8017DAC0`'s first state and restored into
/// `Mc_SaveData.at4.loc.view` when the cutscene is cut short.
extern u8 D_dryfield_general_store_801856F8;

/// The two script arguments, latched from the message that armed the cutscene
/// task `D_dryfield_general_store_8017E164`; the task itself reads them back to
/// place its actors.
extern u8 D_dryfield_general_store_80185709;
extern u8 D_dryfield_general_store_8018570A;

/// The 4-byte record `func_dryfield_general_store_8017DAC0` hands the helper
/// task 0x31 when the script's CAP event key asks for it.
extern GpFadeWork D_dryfield_general_store_801856FC;

extern GpMsgEntry D_dryfield_general_store_8017E188[];
extern s32        D_dryfield_general_store_8017E1B8;
extern TaskDesc   D_dryfield_general_store_8017E4C0;
extern s32        D_dryfield_general_store_8017E55C;
extern s32        D_dryfield_general_store_8017E560;
extern s32        D_dryfield_general_store_8017E564;
extern s32        D_dryfield_general_store_8017E568;

void func_dryfield_general_store_8017DEAC(Task* arg0);
void func_dryfield_general_store_8017DF4C(Task* task);

/// The room's event gate, through which the clock arm of
/// `func_dryfield_general_store_8017D8D4` passes its warp. Returns 1 when game-flag
/// nibble `req->flagId` already reads set (clear, for a negative id).
/// Otherwise, when `req->itemId` has been collected or is 0, it returns 2 and -
/// unless `msg->field_5` asks for a dry run - latches `msg` and `req`, sets
/// the nibble and spawns the event task. When the item is missing it returns
/// 0 and, outside a dry run, runs cap command `req->field_4`.
s32 func_dryfield_general_store_8017D600(RoomEventReq* req, RoomEventMsg* msg)
{
    s32 flag;
    s32 id;
    s32 mode;
    s32 got;
    s32 ret;
    s32 neg;

    flag                              = req->flagId;
    D_dryfield_general_store_80185708 = 0;
    neg                               = flag < 0;
    got                               = (s16)flag;
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
                D_dryfield_general_store_80185700 = *msg;
                D_dryfield_general_store_8018570C = *req;
                id                                = req->flagId;
                mode                              = 1;
                if (id < 0) {
                    id   = -id;
                    mode = 0;
                }
                GameFlag_SetNibble(id, mode);
                Task_SpawnFromTable(&D_dryfield_general_store_8017E158, 0, 0, 0);
                D_dryfield_general_store_80185708 = 1;
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
void func_dryfield_general_store_8017D764(Task* task)
{
    switch (task->state) {
        case 0:
            Gp_StateF0.field_4 = 1;
            Gp_MsgPlayerWeapon(0);
            Gp_RunCapCmd1(D_dryfield_general_store_8018570C.field_0);
            if (D_dryfield_general_store_8018570C.field_8 != 0) {
                SndEvt_EnqueueType6(D_dryfield_general_store_8018570C.field_8, 0, 0);
                task->state++;
            } else {
                task->state = 2;
            }
            break;
        case 1:
            if (SndVoice_HasActiveId(D_dryfield_general_store_8018570C.field_8) == 0) {
                task->state++;
            }
            break;
        case 2:
            task->state++;
            break;
        case 3:
            if (D_dryfield_general_store_8018570C.field_C != 0) {
                SndEvt_EnqueueType6(D_dryfield_general_store_8018570C.field_C, 0, 0);
                task->state++;
            } else {
                task->state = 5;
            }
            break;
        case 4:
            if (SndVoice_HasActiveId(D_dryfield_general_store_8018570C.field_C) == 0) {
                task->state++;
            }
            break;
        case 5:
            SndEvt_EnqueueType7(0x80000000, 0);
            gDisplayState.roomVariant = 1;
            Mc_SaveData.at4.loc.area  = D_dryfield_general_store_80185700.msgId;
            Mc_SaveData.at4.loc.warp  = D_dryfield_general_store_80185700.field_2;
            Mc_SaveData.at4.loc.room  = (u8)D_dryfield_general_store_80185700.field_3;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(task);
            break;
    }
}

/// Handler for the store's two event ids. Both answer with a furniture-style
/// "which variant" byte in `out->field_3`, and a non-zero `field_5` asks what
/// would happen without the side effects.
///
/// Message 1 is the grandfather clock: with nibble 0x63 clear the reply is the
/// id itself, otherwise 4, or 2 + nibble 0x61 while nibble 0x7A is still below
/// 4. The final arm offers the gate a request that plays the two stage sounds
/// 0x5203000C / 0x52030003 under flag nibble 0x3B.
///
/// Message 0x26 is the shop till: with nibble 0xC9 set the reply is 2, or 1
/// while nibble 0x53 is clear, plus 2 more while nibble 0x51 is clear;
/// otherwise 5, or 6 while nibble 0x51 is clear. The arm that is not asking
/// latches `field_2` / `field_3` for the spawned task and answers 2, or runs
/// CAP command 0xE when nibble 0x62 is set. Anything else answers 1.
s32 func_dryfield_general_store_8017D8D4(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
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
        return func_dryfield_general_store_8017D600(&req, in);
    }
    if (in->msgId != 0x26) {
        return 1;
    }
    if (in->field_5 != 0) {
        return 2;
    }
    if (GameFlag_GetNibble(0x62) == 0) {
        Task_SpawnFromTable(&D_dryfield_general_store_8017E164, 1, 0, 0);
        D_dryfield_general_store_80185709 = in->field_2;
        D_dryfield_general_store_8018570A = in->field_3;
    } else {
        Gp_RunCapCmd1(0xE);
    }
    return 2;
}

/// The store's cutscene task, the one the shop-till arm of
/// `func_dryfield_general_store_8017D8D4` spawns. It runs as a state script:
/// states 0 and 2 arm the cutscene and states 1 / 3 are the idle steps that
/// wait for CAP command 0xF to finish.
///
/// State 0 silences the player's weapon messages and latches the save's stage
/// byte into `D_dryfield_general_store_801856F8` before forcing that byte to
/// 0x10, the stage the cutscene belongs to. State 2 queues stage sound
/// 0x5203000D, hands CAP command 0xF the screen and raises `Gp_StateF0.field_4` /
/// `D_80115690` with it.
///
/// State 4 is the exit test. CAP event key 0xB means the script asked for the
/// helper task 0x31, which it spawns with a zeroed `GpFadeWork` record whose
/// `field_2` selects variant 8; any other key cuts the cutscene short instead -
/// captions off, stage sound 0x5203000E, the latched stage byte back into
/// `Mc_SaveData.at4.loc.view` and the player's weapon messages re-enabled.
///
/// State 5 is the commit: it queues sound event 0x80000000, points the save's
/// location at area 0x26 with the two latched script arguments as its warp
/// point and room, raises `D_80071076` and spawns helper task 0x11.
///
/// Every arm that is finished with the task, state 5's and the cut-short arm of
/// state 4's, leaves through the shared `taskKill` below the switch.
void func_dryfield_general_store_8017DAC0(Task* arg0)
{
    switch (arg0->state) {
        case 0:
            Gp_MsgPlayerWeapon(0);
            Gp_MsgPlayer3F3(0);
            D_dryfield_general_store_801856F8 = Mc_SaveData.at4.loc.view;
            Mc_SaveData.at4.loc.view          = 0x10;
            arg0->state                      += 1;
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
                D_dryfield_general_store_801856FC.field_0 = 0;
                D_dryfield_general_store_801856FC.field_1 = 0;
                D_dryfield_general_store_801856FC.field_2 = 8;
                Task_Spawn(1, 0x31, 0, (s32)&D_dryfield_general_store_801856FC);
                arg0->state += 1;
                return;
            }
            Gp_StateF0.field_4 = 0;
            Gp_EnqueueStageSnd6(0x5203000E, 0, 0);
            Mc_SaveData.at4.loc.view = D_dryfield_general_store_801856F8;
            Gp_MsgPlayerWeapon(1);
            Gp_MsgPlayer3F3(1);
            break;
        case 5:
            SndEvt_EnqueueType7(0x80000000, 0);
            Mc_SaveData.at4.loc.area  = 0x26;
            Mc_SaveData.at4.loc.warp  = D_dryfield_general_store_80185709;
            Mc_SaveData.at4.loc.room  = D_dryfield_general_store_8018570A;
            gDisplayState.roomVariant = 1;
            Task_Spawn(0, 0x11, 0, 0);
            break;
        default:
            return;
    }
    taskKill(arg0);
}

/// The room task's three-state table, run from a stack copy by
/// `func_dryfield_general_store_8017DF5C`: the entry state
/// `func_dryfield_general_store_8017DEAC`, the idle state
/// `func_dryfield_general_store_8017DF4C`, then `taskKill`.
const TaskFuncTable3 D_dryfield_general_store_8017D5F4 = {
    { func_dryfield_general_store_8017DEAC, func_dryfield_general_store_8017DF4C, taskKill },
};

/// A task that runs cap command `spawnArg2` and waits for it to finish; if the
/// cap then reports an event key of 0xA or above, it toggles game-flag nibble
/// `spawnArg1` between 0 and 1. The task then kills itself.
void func_dryfield_general_store_8017DC78(Task* task)
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

s32 func_dryfield_general_store_8017DD58(s32 arg0, s32 arg1, s32 arg2)
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
        Task_SpawnFromTable(&D_dryfield_general_store_8017E164, 0, 0x53, 9);
    }
    return 0;
}

/// Message handler that plays stage sound 0x52030007 on action 7. Always
/// returns 0.
s32 func_dryfield_general_store_8017DDC0(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    if (arg2 == 7) {
        Gp_EnqueueStageSnd6(0x52030000 | 7, 0, 0);
    }
    return 0;
}

/// Message handler that takes no action and reports the message as not
/// handled.
s32 func_dryfield_general_store_8017DDF4(void)
{
    return 0;
}

/// Message handler on the slot-4 table that owns the store's story flag 0x5E:
/// message 1 spawns the cutscene task once the flag is still clear, message 2
/// arms the cutscene object and then both paths advance the flag.
s32 func_dryfield_general_store_8017DDFC(Task* task, s32 msgId, RoomEventMsg* arg2)
{
    switch (arg2->field_2) {
        case 1:
            if (GameFlag_GetNibble(0x5E) == 0) {
                Task_SpawnFromTable(&D_dryfield_general_store_8017E4C0, 0, 0, 0);
                GameFlag_SetNibble(0x5E, 1);
            }
            break;
        case 2:
            if (Gp_StateF0.field_0 != 1 && GameFlag_GetNibble(0x5E) == 1) {
                func_800E8614((s32)&D_dryfield_general_store_8017E568, 1);
            }
            GameFlag_SetNibble(0x5E, 2);
            break;
    }
    return 0;
}

void func_dryfield_general_store_8017DEAC(Task* arg0)
{
    arg0->msgTable = D_dryfield_general_store_8017E188;
    Game_SetPtrSlot(arg0, 7);
    if (GameFlag_GetNibble(0x5E) == 0) {
        Gp_DispatchMsg(gameGetPtrSlot(4), 0x7DA, (s32)&D_dryfield_general_store_8017E1B8, 0x7DB);
    } else if (GameFlag_GetNibble(0x5E) == 1) {
        GameFlag_SetNibble(0x5E, 2);
    }
    arg0->state = arg0->state + 1;
    D_80115598  = 1;
}

/// Idle state of the room task: does nothing, though it reserves a stack
/// frame.
void func_dryfield_general_store_8017DF4C(Task* task)
{
    char pad[0x10];
}

/// The room task: runs the state `D_dryfield_general_store_8017D5F4` names for
/// `task->state`, through a stack copy of the table.
void func_dryfield_general_store_8017DF5C(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_general_store_8017D5F4;
    sp.funcs[task->state](task);
}

void func_dryfield_general_store_8017DFB4(Task* arg0)
{
    s16 temp_v0;

    switch (arg0->state) {
        case 0:
            Gp_DispatchMsg(gameGetPtrSlot(4), 0x7DA, (s32)&D_dryfield_general_store_8017E55C, 0x7DB);
            SndEvt_EnqueueType6(0x5203000F, 0, 0);
            arg0->killCountdown = 0x5A;
            arg0->state++;
            return;
        case 1:
            temp_v0             = (u16)arg0->killCountdown - 1;
            arg0->killCountdown = temp_v0;
            if (temp_v0 < 0) {
                taskKill(arg0);
            }
            return;
    }
}

void func_dryfield_general_store_8017E064(Task* arg0)
{
    s16 temp_v0;

    switch (arg0->state) {
        case 0:
            Gp_MsgPlayerWeapon(0);
            Gp_DispatchMsg(gameGetPtrSlot(4), 0x7DA, (s32)&D_dryfield_general_store_8017E560, 0x7DB);
            arg0->killCountdown = 0x5A;
            arg0->state++;
            return;
        case 1:
            temp_v0             = (u16)arg0->killCountdown - 1;
            arg0->killCountdown = temp_v0;
            if (temp_v0 < 0) {
                Gp_DispatchMsg(gameGetPtrSlot(4), 0x7DA, (s32)&D_dryfield_general_store_8017E564, 0x7DB);
                Gp_MsgPlayerWeapon(1);
                taskKill(arg0);
            }
            return;
    }
}

/// Arms `Gp_StateF0` with `arg0`.
void func_dryfield_general_store_8017E130(s32 arg0)
{
    Gp_ArmStateF0(arg0);
}

void func_dryfield_general_store_8017E150(void)
{
}
