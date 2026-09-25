#include "common.h"

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

extern s16 D_80071076;

/// The event the room's gate `func_dryfield_night_water_tower_8017D60C`
/// latched: the incoming message and the request, kept for the event task it
/// spawns from `D_dryfield_night_water_tower_8017E6E0`, and the flag the gate
/// sets once it has done so.
extern RoomEventMsg D_dryfield_night_water_tower_80182C50;
extern RoomEventReq D_dryfield_night_water_tower_80182C5C;
extern u8           D_dryfield_night_water_tower_80182C58;
extern TaskDesc     D_dryfield_night_water_tower_8017E6E0;

/// The room's message table, `(msgId, handler)` pairs ending at 0x7FFFFFFF,
/// which the entry task installs as its own `Task::msgTable`.
extern GpMsgEntry D_dryfield_night_water_tower_8017E6EC[];

/// The room's event gate. A request whose flag nibble is already set (or clear,
/// for a negative `flagId`) answers 1. One whose prerequisite item is missing
/// runs the request's CAP command and answers 0. Otherwise the message and
/// request are latched, the nibble is written, the event task is spawned and
/// the answer is 2. A non-zero `field_5` on the message only reports the
/// answer, with none of the side effects.
s32 func_dryfield_night_water_tower_8017D60C(RoomEventReq* req, RoomEventMsg* msg)
{
    s32 flag;
    s32 id;
    s32 mode;
    s32 got;
    s32 ret;
    s32 neg;

    flag                                  = req->flagId;
    D_dryfield_night_water_tower_80182C58 = 0;
    neg                                   = flag < 0;
    got                                   = (s16)flag;
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
                D_dryfield_night_water_tower_80182C50 = *msg;
                D_dryfield_night_water_tower_80182C5C = *req;
                id                                    = req->flagId;
                mode                                  = 1;
                if (id < 0) {
                    id   = -id;
                    mode = 0;
                }
                GameFlag_SetNibble(id, mode);
                Task_SpawnFromTable(&D_dryfield_night_water_tower_8017E6E0, 0, 0, 0);
                D_dryfield_night_water_tower_80182C58 = 1;
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

/// The event task the gate spawns: runs the latched request's CAP command,
/// plays its two sound events in turn and waits for each to finish, then
/// writes the latched message's destination into the save data and hands over
/// to task type 0x11 to load it.
void func_dryfield_night_water_tower_8017D770(Task* task)
{
    switch (task->state) {
        case 0:
            Gp_StateF0.field_4 = 1;
            Gp_MsgPlayerWeapon(0);
            Gp_RunCapCmd1(D_dryfield_night_water_tower_80182C5C.field_0);
            if (D_dryfield_night_water_tower_80182C5C.field_8 != 0) {
                SndEvt_EnqueueType6(D_dryfield_night_water_tower_80182C5C.field_8, 0, 0);
                task->state++;
            } else {
                task->state = 2;
            }
            break;
        case 1:
            if (SndVoice_HasActiveId(D_dryfield_night_water_tower_80182C5C.field_8) == 0) {
                task->state++;
            }
            break;
        case 2:
            task->state++;
            break;
        case 3:
            if (D_dryfield_night_water_tower_80182C5C.field_C != 0) {
                SndEvt_EnqueueType6(D_dryfield_night_water_tower_80182C5C.field_C, 0, 0);
                task->state++;
            } else {
                task->state = 5;
            }
            break;
        case 4:
            if (SndVoice_HasActiveId(D_dryfield_night_water_tower_80182C5C.field_C) == 0) {
                task->state++;
            }
            break;
        case 5:
            SndEvt_EnqueueType7(0x80000000, 0);
            D_80071076               = 1;
            Mc_SaveData.at4.loc.area = D_dryfield_night_water_tower_80182C50.msgId;
            Mc_SaveData.at4.loc.warp = D_dryfield_night_water_tower_80182C50.field_2;
            Mc_SaveData.at4.loc.room = (u8)D_dryfield_night_water_tower_80182C50.field_3;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(task);
            break;
    }
}

/// The room's handler for message 0x13EE, the first entry of its message table.
/// It copies the incoming record to `out` and answers by the record's first
/// halfword. For 0x13 it builds the room's event request -- flag nibble 0x34,
/// prerequisite item 0x10, CAP commands 0xA and 6 and two stage sounds -- and
/// hands it to the event gate with the incoming record; the gate's 0 (the
/// prerequisite missing) is answered as 2, and once the gate has latched the
/// event item 0x110 is marked seen. Any other record first drops nibble 0x55
/// from 2 back to 1 unless it is only a query. For 0x15 it also clears nibble
/// 0x4B when it reads 7, and answers 1 on stage 3 and otherwise only while
/// nibble 0x32 is 2. Everything else answers 1.
s32 func_dryfield_night_water_tower_8017D8E0(Task* task, s32 msgId, RoomEventMsg* msg, RoomEventMsg* out)
{
    RoomEventReq req;
    s32          ret;

    *out = *msg;
    if (msg->msgId == 0x13) {
        req.field_0 = 0xA;
        req.field_4 = 6;
        req.field_8 = Gp_PackStageSndId(0x5214000E);
        req.field_C = Gp_PackStageSndId(0x52140003);
        req.flagId  = 0x34;
        req.itemId  = 0x10;
        ret         = func_dryfield_night_water_tower_8017D60C(&req, msg);
        if (ret == 0) {
            ret = 2;
        }
        if (D_dryfield_night_water_tower_80182C58 != 0) {
            Gp_SetItemSeenBit(0x110, 1);
        }
        return ret;
    }
    if (msg->field_5 == 0 && GameFlag_GetNibble(0x55) == 2) {
        GameFlag_SetNibble(0x55, 1);
    }
    if (msg->msgId == 0x15) {
        if (msg->field_5 == 0 && GameFlag_GetNibble(0x4B) == 7) {
            GameFlag_SetNibble(0x4B, 0);
        }
        if (gGameSession->at4.loc.stage == 3) {
            return 1;
        }
        if (GameFlag_GetNibble(0x32) != 2) {
            return 0;
        }
    }
    return 1;
}

/// The room's handler for message 0x13F2: plays the stage sound for script
/// events 8 and 13 and answers 0 for every event.
s32 func_dryfield_night_water_tower_8017DA4C(s32 arg0, s32 arg1, s32 arg2)
{
    switch (arg2) {
        case 8:
            Gp_EnqueueStageSnd6(0x52140008, 0, 0);
            break;
        case 13:
            Gp_EnqueueStageSnd6(0x5214000D, 0, 0);
            break;
    }
    return 0;
}

/// The room's handler for message 0x13F1: answers 0.
s32 func_dryfield_night_water_tower_8017DA9C(void)
{
    return 0;
}

/// The room's handler for message 0x13F0: script event 7 starts CAP slot 7;
/// every event answers 0.
s32 func_dryfield_night_water_tower_8017DAA4(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 7) {
        Gp_StartCapSlot(7, 1, 3);
    }
    return 0;
}

/// The room's handler for message 0x13EF: answers 0.
s32 func_dryfield_night_water_tower_8017DAD4(void)
{
    return 0;
}

/// State 0 of the room entry task: installs the room's message table,
/// registers the task in game pointer slot 7 and advances to the idle state.
void func_dryfield_night_water_tower_8017DADC(Task* task)
{
    task->msgTable = D_dryfield_night_water_tower_8017E6EC;
    Game_SetPtrSlot(task, 7);
    task->state = (s32)(task->state + 1);
}

/// State 1 of the room entry task: idles.
void func_dryfield_night_water_tower_8017DB20(Task* task)
{
}

/// The room entry task's three states: install the room's message table,
/// idle, and `taskKill`.
const TaskFuncTable3 D_dryfield_night_water_tower_8017D5DC = {
    { func_dryfield_night_water_tower_8017DADC, func_dryfield_night_water_tower_8017DB20, taskKill },
};

/// Runs the room entry task's current state from its three-entry table, which
/// it copies onto the stack before the call.
void func_dryfield_night_water_tower_8017DB28(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_night_water_tower_8017D5DC;
    sp.funcs[task->state](task);
}
