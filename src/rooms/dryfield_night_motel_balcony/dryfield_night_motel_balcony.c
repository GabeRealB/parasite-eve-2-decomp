#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/268.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "rooms/room_common.h"
#include "rooms/dryfield_night_motel_balcony.h"

extern s8 D_8007272D;

/// A gameplay state byte; the one-shot balcony event waits while it is 1.
extern s8 D_80114C12;

/// Gameplay-resident script data the room task starts: the pair handed to
/// `func_800E8634` on the first visit, and the one handed to `func_800E8614`
/// by the one-shot event.
extern s32 D_80165060;
extern s32 D_80165798;
extern u8  D_80165720;

/// The room's message table, installed on the room task.
extern GpMsgEntry D_dryfield_night_motel_balcony_80182804[];

/// The message and request the event gate latched for the event task.
extern RoomEventMsg D_dryfield_night_motel_balcony_8018F2D4;
extern RoomEventReq D_dryfield_night_motel_balcony_8018F2E0;

/// Set by the event gate when its last call latched a request and spawned the
/// event task; every call clears it first.
extern u8 D_dryfield_night_motel_balcony_8018F2DC;

/// Descriptor of the event task the gate spawns.
extern TaskDesc D_dryfield_night_motel_balcony_801827F8;

/// The balcony's event gate. A request whose flag nibble already records the
/// event (a set nibble, or a clear one for a negative `flagId`) answers 1. One
/// whose prerequisite item has not been collected runs the request's CAP
/// command and answers 0. Otherwise the gate answers 2 and - unless the
/// message's `field_5` asks for a dry run - latches the message and the
/// request, writes the flag nibble and spawns the event task.
s32 func_dryfield_night_motel_balcony_8017D694(RoomEventReq* req, RoomEventMsg* msg)
{
    s32 flag;
    s32 id;
    s32 mode;
    s32 got;
    s32 ret;
    s32 neg;

    flag                                    = req->flagId;
    D_dryfield_night_motel_balcony_8018F2DC = 0;
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
                D_dryfield_night_motel_balcony_8018F2D4 = *msg;
                D_dryfield_night_motel_balcony_8018F2E0 = *req;
                id                                      = req->flagId;
                mode                                    = 1;
                if (id < 0) {
                    id   = -id;
                    mode = 0;
                }
                GameFlag_SetNibble(id, mode);
                Task_SpawnFromTable(&D_dryfield_night_motel_balcony_801827F8, 0, 0, 0);
                D_dryfield_night_motel_balcony_8018F2DC = 1;
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

/// The event task the gate spawns. It raises `Gp_StateF0.field_4`, runs the latched
/// request's CAP command, plays its two stage sounds in turn (either may be
/// absent) waiting for each voice to finish, then stores the latched
/// message's `msgId`, `field_2` and `field_3` as the save location's area,
/// warp and room, spawns task 0x11 and kills itself.
void func_dryfield_night_motel_balcony_8017D7F8(Task* task)
{
    switch (task->state) {
        case 0:
            Gp_StateF0.field_4 = 1;
            Gp_MsgPlayerWeapon(0);
            Gp_RunCapCmd1(D_dryfield_night_motel_balcony_8018F2E0.field_0);
            if (D_dryfield_night_motel_balcony_8018F2E0.field_8 != 0) {
                SndEvt_EnqueueType6(D_dryfield_night_motel_balcony_8018F2E0.field_8, 0, 0);
                task->state++;
            } else {
                task->state = 2;
            }
            break;
        case 1:
            if (SndVoice_HasActiveId(D_dryfield_night_motel_balcony_8018F2E0.field_8) == 0) {
                task->state++;
            }
            break;
        case 2:
            task->state++;
            break;
        case 3:
            if (D_dryfield_night_motel_balcony_8018F2E0.field_C != 0) {
                SndEvt_EnqueueType6(D_dryfield_night_motel_balcony_8018F2E0.field_C, 0, 0);
                task->state++;
            } else {
                task->state = 5;
            }
            break;
        case 4:
            if (SndVoice_HasActiveId(D_dryfield_night_motel_balcony_8018F2E0.field_C) == 0) {
                task->state++;
            }
            break;
        case 5:
            SndEvt_EnqueueType7(0x80000000, 0);
            gDisplayState.roomVariant = 1;
            Mc_SaveData.at4.loc.area  = D_dryfield_night_motel_balcony_8018F2D4.msgId;
            Mc_SaveData.at4.loc.warp  = D_dryfield_night_motel_balcony_8018F2D4.field_2;
            Mc_SaveData.at4.loc.room  = (u8)D_dryfield_night_motel_balcony_8018F2D4.field_3;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(task);
            break;
    }
}

/// The room's message handler. It copies `msg` to `out`, filling `field_3`
/// from game flags for messages 0x1C, 0xF and 0x1F, then routes messages 0x1C,
/// 0x1F and 0x1E through the event gate with each one's request; when the
/// gate fires, it updates the collected and seen item bits (and, for 0x1E, a
/// flag nibble and `D_8007272D`). Any other message answers 1; a gate result
/// of 0 is reported as 2.
s32 func_dryfield_night_motel_balcony_8017D968(Task* task, s32 msgId, RoomEventMsg* msg, RoomEventMsg* out)
{
    RoomEventReq req;
    s32          flagClear;
    s32          ret;

    *out = *msg;
    if (msg->msgId == 0x1C && msg->field_5 == 0) {
        out->field_3 = GameFlag_GetNibble(0x61) + 1;
    }
    if (msg->msgId == 0xF && msg->field_5 == 0) {
        out->field_3 = GameFlag_GetNibble(0x61) + 1;
    }
    if (msg->msgId == 0x1F && msg->field_5 == 0) {
        flagClear    = GameFlag_GetNibble(0x96) == 0;
        out->field_3 = flagClear ? 1 : 2;
    }
    if (msg->msgId == 0x1C) {
        req.field_0 = 7;
        req.field_4 = 4;
        req.field_8 = Gp_PackStageSndId(0x521D000A);
        req.field_C = Gp_PackStageSndId(0x521D0001);
        req.flagId  = 0x43;
        req.itemId  = 0x13;
        ret         = func_dryfield_night_motel_balcony_8017D694(&req, out);
        if (D_dryfield_night_motel_balcony_8018F2DC != 0) {
            Gp_ClearCollectedBit(0x10F);
            Gp_ClearCollectedBit(0x112);
            Gp_SetItemSeenBit(0x113, 1);
        }
    } else if (msg->msgId == 0x1F) {
        req.field_0 = 5;
        req.field_4 = 2;
        req.field_8 = Gp_PackStageSndId(0x521D000A);
        req.field_C = Gp_PackStageSndId(0x521D0001);
        req.flagId  = 0x44;
        req.itemId  = 0x13;
        ret         = func_dryfield_night_motel_balcony_8017D694(&req, out);
        if (D_dryfield_night_motel_balcony_8018F2DC != 0) {
            Gp_ClearCollectedBit(0x10F);
            Gp_ClearCollectedBit(0x112);
            Gp_SetItemSeenBit(0x113, 1);
        }
    } else if (msg->msgId == 0x1E) {
        req.field_0 = 6;
        req.field_4 = 3;
        req.field_8 = Gp_PackStageSndId(0x521D000A);
        req.field_C = Gp_PackStageSndId(0x521D0001);
        req.flagId  = 0x2E;
        req.itemId  = 0xF;
        ret         = func_dryfield_night_motel_balcony_8017D694(&req, out);
        if (D_dryfield_night_motel_balcony_8018F2DC != 0) {
            GameFlag_SetNibble(0x30, 1);
            D_8007272D = 3;
            func_800E3FAC(0xA2, 0xC);
        }
    } else {
        return 1;
    }
    if (ret == 0) {
        ret = 2;
    }
    return ret;
}

/// Plays stage sound 0x521D0008 or 0x521D0009 for events 8 and 9; always
/// answers 0.
s32 func_dryfield_night_motel_balcony_8017DBC8(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    switch (arg2) {
        case 0x8:
            Gp_EnqueueStageSnd6(0x521D0008, 0, 0);
            break;
        case 0x9:
            Gp_EnqueueStageSnd6(0x521D0009, 0, 0);
            break;
    }
    return 0;
}

s32 func_dryfield_night_motel_balcony_8017DC18(void)
{
    return 0;
}

s32 func_dryfield_night_motel_balcony_8017DC20(void)
{
    return 0;
}

s32 func_dryfield_night_motel_balcony_8017DC28(void)
{
    return 0;
}

/// Room task state 0: installs the room's message table, registers the task
/// in pointer slot 7 and reapplies the nine saved sprite-command states. On
/// place 2, room 2 with flag nibble 0x61 still clear, it also starts the
/// script pair, sets nibbles 0x61, 0x10E (arming state 1) and 0x155, clears
/// nibble 3 and sets `flowFlags` to 0x85. Then advances to the next state.
void func_dryfield_night_motel_balcony_8017DC30(Task* task)
{
    u8 field9;

    task->msgTable = D_dryfield_night_motel_balcony_80182804;
    Game_SetPtrSlot(task, 7);
    func_dryfield_night_motel_balcony_8017E3C8();
    field9 = gGameSession->at4.loc.place;
    if (field9 == 2 && gGameSession->at4.loc.room == field9 && GameFlag_GetNibble(0x61) == 0) {
        func_800E8634((s32)&D_80165060, 0, (s32)&D_80165798);
        GameFlag_SetNibble(0x61, 1);
        GameFlag_SetNibble(0x10E, 1);
        GameFlag_SetNibble(3, 0);
        GameFlag_SetNibble(0x155, 1);
        gGameSession->flowFlags = 0x85;
    }
    task->state = task->state + 1;
}

/// Room task state 1: once the event state is idle, `D_80114C12` is not 1 and
/// flag nibble 0x10E is 1, runs the one-shot script and moves the nibble to 2.
void func_dryfield_night_motel_balcony_8017DD0C(Task* task)
{
    if (gGameSession->eventState == 0 && D_80114C12 != 1 && GameFlag_GetNibble(0x10E) == 1) {
        func_800E8614((s32)&D_80165720, 0);
        GameFlag_SetNibble(0x10E, 2);
    }
}

/// The room task's three states: setup, the per-tick balcony event check,
/// and exit.
const TaskFuncTable3 D_dryfield_night_motel_balcony_8017D5DC = {
    func_dryfield_night_motel_balcony_8017DC30,
    func_dryfield_night_motel_balcony_8017DD0C,
    taskKill,
};

/// Runs the room task's current state from its state table, dispatching
/// through a copy of the table taken onto the stack.
void func_dryfield_night_motel_balcony_8017DD78(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_night_motel_balcony_8017D5DC;
    sp.funcs[task->state](task);
}
