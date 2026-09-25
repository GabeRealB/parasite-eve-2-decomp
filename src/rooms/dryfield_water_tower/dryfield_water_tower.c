#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/268.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"

#include "rooms/dryfield_water_tower.h"
#include "rooms/room_common.h"

/* The room calls the dispatcher with only the task, leaving a1-a3 holding
   whatever the caller had, so the declaration must stay unprototyped. */
s32 Gp_DispatchMsg();

/// The saved view byte the scene task keeps while its CAP command runs, and
/// puts back when the answer is not 0xA.
extern u32 D_dryfield_water_tower_8018768C;

/// The cap script task the entry task spawns, the target of the scene task's
/// message 0x13EC and of the room's message 0x13F4.
extern Task* D_dryfield_water_tower_801876A0;

/// The event the room's gate `func_dryfield_water_tower_8017D674` latched:
/// the incoming message and the request, kept for the event task it spawns
/// from `D_dryfield_water_tower_80180394`, and the flag the gate sets once it
/// has done so.
extern RoomEventMsg D_dryfield_water_tower_80187694;
extern RoomEventReq D_dryfield_water_tower_801876B0;
extern u8           D_dryfield_water_tower_8018769C;
extern TaskDesc     D_dryfield_water_tower_80180394;

/// The room's message table, `(msgId, handler)` pairs ending at 0x7FFFFFFF,
/// which the entry task installs as its own `Task::msgTable`.
extern s32 D_dryfield_water_tower_801803A0[];

/// The task descriptor of the room's scene task
/// `func_dryfield_water_tower_8017D948`.
extern TaskDesc D_dryfield_water_tower_801803D8;

void func_dryfield_water_tower_8017DD6C(Task* arg0);
void func_dryfield_water_tower_8017DDD0(Task* task);

/// The room's event gate. A request whose flag nibble is already set (or clear,
/// for a negative `flagId`) answers 1. One whose prerequisite item is missing
/// runs the request's CAP command and answers 0. Otherwise the message and
/// request are latched, the nibble is written, the event task is spawned and
/// the answer is 2. A non-zero `field_5` on the message only reports the
/// answer, with none of the side effects.
s32 func_dryfield_water_tower_8017D674(RoomEventReq* req, RoomEventMsg* msg)
{
    s32 flag;
    s32 id;
    s32 mode;
    s32 got;
    s32 ret;
    s32 neg;

    flag                            = req->flagId;
    D_dryfield_water_tower_8018769C = 0;
    neg                             = flag < 0;
    got                             = (s16)flag;
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
                D_dryfield_water_tower_80187694 = *msg;
                D_dryfield_water_tower_801876B0 = *req;
                id                              = req->flagId;
                mode                            = 1;
                if (id < 0) {
                    id   = -id;
                    mode = 0;
                }
                GameFlag_SetNibble(id, mode);
                Task_SpawnFromTable(&D_dryfield_water_tower_80180394, 0, 0, 0);
                D_dryfield_water_tower_8018769C = 1;
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
void func_dryfield_water_tower_8017D7D8(Task* task)
{
    switch (task->state) {
        case 0:
            Gp_StateF0.field_4 = 1;
            Gp_MsgPlayerWeapon(0);
            Gp_RunCapCmd1(D_dryfield_water_tower_801876B0.field_0);
            if (D_dryfield_water_tower_801876B0.field_8 != 0) {
                SndEvt_EnqueueType6(D_dryfield_water_tower_801876B0.field_8, 0, 0);
                task->state++;
            } else {
                task->state = 2;
            }
            break;
        case 1:
            if (SndVoice_HasActiveId(D_dryfield_water_tower_801876B0.field_8) == 0) {
                task->state++;
            }
            break;
        case 2:
            task->state++;
            break;
        case 3:
            if (D_dryfield_water_tower_801876B0.field_C != 0) {
                SndEvt_EnqueueType6(D_dryfield_water_tower_801876B0.field_C, 0, 0);
                task->state++;
            } else {
                task->state = 5;
            }
            break;
        case 4:
            if (SndVoice_HasActiveId(D_dryfield_water_tower_801876B0.field_C) == 0) {
                task->state++;
            }
            break;
        case 5:
            SndEvt_EnqueueType7(0x80000000, 0);
            gDisplayState.roomVariant = 1;
            Mc_SaveData.at4.loc.area  = D_dryfield_water_tower_80187694.msgId;
            Mc_SaveData.at4.loc.warp  = D_dryfield_water_tower_80187694.field_2;
            Mc_SaveData.at4.loc.room  = (u8)D_dryfield_water_tower_80187694.field_3;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(task);
            break;
    }
}

/// The room entry task's three states: install the room and spawn the cap
/// script, idle, and `taskKill`.
const TaskFuncTable3 D_dryfield_water_tower_8017D5DC = {
    { func_dryfield_water_tower_8017DD6C, func_dryfield_water_tower_8017DDD0, taskKill },
};

/// The room's scene task, spawned on script event 7. Unless nibble 0x55 has
/// reached 2 it hides the player's weapon, runs CAP command 7 and waits for it,
/// saving the view byte; a key answer of 0xA then sets nibble 0x55 to 2, sends
/// 0x13EC to the cap script and plays 0x52140009, and any other answer restores
/// the session and the view byte. With nibble 0x55 already at 2 it only runs
/// CAP command 7. Every finished path kills the task.
void func_dryfield_water_tower_8017D948(Task* arg0)
{
    switch (arg0->state) {
        case 0:
            if (GameFlag_GetNibble(0x55) < 2) {
                func_dryfield_water_tower_8017DCB4();
                Gp_MsgPlayer3F3(0);
                Gp_MsgPlayerWeapon(0);
                Gp_RunCapCmd(7, 0);
                gGameSession->eventState        = 1;
                D_dryfield_water_tower_8018768C = Mc_SaveData.at4.loc.view;
                arg0->state                     = arg0->state + 1;
                return;
            }
            Gp_RunCapCmd1(7);
            break;
        case 1:
            if (Gp_CapBusy() == 0) {
                Gp_StateF0.field_4 = 2;
                /* keeps the `lw state` behind the `sb` instead of filling its load delay */
                arg0->state = arg0->state + 1;
            }
            return;
        case 2:
            if (Gp_GetCapEventKey() == 0xA) {
                GameFlag_SetNibble(0x55, 2);
                func_dryfield_water_tower_8017DCB4();
                Gp_StateF0.field_4 = 0;
                Gp_DispatchMsg(D_dryfield_water_tower_801876A0, 0x13EC, 0, 0);
                SndEvt_EnqueueType6(0x52140009, 0, 0);
            } else {
                gGameSession->eventState = 0;
                gGameSession->hideHud    = 0;
                Gp_StateF0.field_4       = 0;
                Mc_SaveData.at4.loc.view = D_dryfield_water_tower_8018768C;
                Gp_MsgPlayerWeapon(1);
                Gp_MsgPlayer3F3(1);
            }
            break;
        default:
            return;
    }
    taskKill(arg0);
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
s32 func_dryfield_water_tower_8017DAF8(Task* task, s32 msgId, RoomEventMsg* msg, RoomEventMsg* out)
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
        ret         = func_dryfield_water_tower_8017D674(&req, msg);
        if (ret == 0) {
            ret = 2;
        }
        if (D_dryfield_water_tower_8018769C != 0) {
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
s32 func_dryfield_water_tower_8017DC64(s32 arg0, s32 arg1, s32 arg2)
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

/// Shows or hides the view's sprites from nibble 0x55: modes 0 and 1 draw
/// them, 2 and 3 skip them, and any other value leaves them alone.
void func_dryfield_water_tower_8017DCB4(void)
{
    s32 mode = GameFlag_GetNibble(0x55);

    if (mode < 0) {
        return;
    }
    if (mode < 2) {
        func_dryfield_water_tower_801802D8(1);
    } else if (mode < 4) {
        func_dryfield_water_tower_801802D8(0);
    }
}

/// The room's handler for message 0x13F1: answers 0.
s32 func_dryfield_water_tower_8017DCFC(void)
{
    return 0;
}

/// The room's handler for message 0x13F0: script event 7 spawns the scene
/// task; every event answers 0.
s32 func_dryfield_water_tower_8017DD04(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    if (arg2 == 7) {
        Task_SpawnFromTable(&D_dryfield_water_tower_801803D8, 0, 0, 0);
    }
    return 0;
}

/// The room's handler for message 0x13EF: answers 0.
s32 func_dryfield_water_tower_8017DD3C(void)
{
    return 0;
}

/// The room's handler for message 0x13F4: passes the message on to the cap
/// script task with the arguments it arrived with.
void func_dryfield_water_tower_8017DD44(void)
{
    Gp_DispatchMsg(D_dryfield_water_tower_801876A0);
}

/// State 0 of the room entry task: installs the room's message table,
/// registers the task in game pointer slot 7 and spawns the cap script.
void func_dryfield_water_tower_8017DD6C(Task* arg0)
{
    Task* temp_v0;

    arg0->msgTable = D_dryfield_water_tower_801803A0;
    Game_SetPtrSlot(arg0, 7);
    temp_v0                         = Task_SpawnFromTable(D_dryfield_water_tower_80182384, 0, 0, 0);
    arg0->state                     = (s32)(arg0->state + 1);
    D_dryfield_water_tower_801876A0 = temp_v0;
}

/// State 1 of the room entry task: idles.
void func_dryfield_water_tower_8017DDD0(Task* task)
{
}

/// Runs the room entry task's current state from its three-entry table, which
/// it copies onto the stack before the call.
void func_dryfield_water_tower_8017DDD8(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_water_tower_8017D5DC;
    sp.funcs[task->state](task);
}
