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
#include "rooms/room.h"
#include "rooms/room_common.h"

extern s16 D_80071076;
extern u8  D_80115690;

/// The pair of cutscene blocks the walkway's scene hands to `func_800E8634`.
extern s32 D_80165354;
extern s32 D_80165834;

extern void func_8016268C(void);
extern s32  func_80179A04(RoomEventMsg* in, RoomEventMsg* out);

/// Descriptors of the two event tasks: the one the walkway's handler spawns
/// for its own event, and the one the event gate spawns.
extern TaskDesc D_shelter_b2_north_maintenance_walkway_80183B48;
extern TaskDesc D_shelter_b2_north_maintenance_walkway_80183B54;

/// The walkway's message table, installed as the room task's `msgTable`.
extern GpMsgEntry D_shelter_b2_north_maintenance_walkway_80183B60[];

/// Area records applied once the walkway's scene has started.
extern GpAreaApplyRec D_shelter_b2_north_maintenance_walkway_80186380[];

/// Spawn payload of the task 0x31 the event task may start.
extern GpStateBD8 D_shelter_b2_north_maintenance_walkway_801863A0;

/// The message and the event the walkway's handler latched for its event task,
/// and the flag saying its last call did so.
extern RoomEventMsg     D_shelter_b2_north_maintenance_walkway_801863A8;
extern u8               D_shelter_b2_north_maintenance_walkway_801863B0;
extern RoomLatchedEvent D_shelter_b2_north_maintenance_walkway_801863C4;

/// The message and request the event gate latched for the event task.
extern RoomEventMsg D_shelter_b2_north_maintenance_walkway_801863B8;
extern RoomEventReq D_shelter_b2_north_maintenance_walkway_801863D4;

/// Set by the walkway's event gate when its last call latched a request and
/// spawned the event task; every call clears it first.
extern u8 D_shelter_b2_north_maintenance_walkway_801863C0;

/// The event task the walkway's message handler spawns for its own event. It
/// runs the latched event's CAP command and waits for it to finish, starting
/// task 0x31 when the event asks for it; then plays the event's stage sound
/// (if any) and waits for the voice to end. Finally it commits the latched
/// message's area, warp and room as the save location, respawns the player
/// task as type 0x11 and ends.
void func_shelter_b2_north_maintenance_walkway_8017D61C(Task* arg0)
{
    switch (arg0->state) {
        case 0:
            Gp_StateF0.field_4 = 1;
            Gp_MsgPlayerWeapon(0);
            Gp_RunCapCmd(D_shelter_b2_north_maintenance_walkway_801863C4.capCmd, 0);
            D_80115690 = 1;
            arg0->state++;
            break;
        case 1:
            if (Gp_CapBusy() == 0) {
                if (D_shelter_b2_north_maintenance_walkway_801863C4.fade != 0) {
                    D_shelter_b2_north_maintenance_walkway_801863A0.field_0 = 0;
                    D_shelter_b2_north_maintenance_walkway_801863A0.field_1 = 0;
                    D_shelter_b2_north_maintenance_walkway_801863A0.field_2 = 0x1E;
                    Task_Spawn(1, 0x31, 0, (s32)&D_shelter_b2_north_maintenance_walkway_801863A0);
                }
                arg0->state++;
            }
            break;
        case 2:
            if (D_shelter_b2_north_maintenance_walkway_801863C4.stageSnd != 0) {
                Gp_EnqueueStageSnd6(D_shelter_b2_north_maintenance_walkway_801863C4.stageSnd, 0, 0);
                arg0->state++;
            } else {
                arg0->state = 4;
            }
            break;
        case 3:
            if (SndVoice_HasActiveId(Gp_PackStageSndId(D_shelter_b2_north_maintenance_walkway_801863C4.stageSnd)) == 0) {
                arg0->state++;
            }
            break;
        case 4:
            SndEvt_EnqueueType7(0x80000000, 0);
            D_80071076               = 1;
            Mc_SaveData.at4.loc.area = D_shelter_b2_north_maintenance_walkway_801863A8.msgId;
            Mc_SaveData.at4.loc.warp = D_shelter_b2_north_maintenance_walkway_801863A8.field_2;
            Mc_SaveData.at4.loc.room = (u8)D_shelter_b2_north_maintenance_walkway_801863A8.field_3;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(arg0);
            break;
    }
}

/// The walkway's event gate. A request whose flag nibble already records the
/// event (a set nibble, or a clear one for a negative `flagId`) answers 1. One
/// whose prerequisite item has not been collected runs the request's CAP
/// command and answers 0. Otherwise the gate answers 2 and - unless the
/// message's `field_5` asks for a dry run - latches the message and the
/// request, writes the flag nibble and spawns the event task.
s32 func_shelter_b2_north_maintenance_walkway_8017D7B4(RoomEventReq* req, RoomEventMsg* msg)
{
    s32 flag;
    s32 id;
    s32 mode;
    s32 got;
    s32 ret;
    s32 neg;

    flag                                            = req->flagId;
    D_shelter_b2_north_maintenance_walkway_801863C0 = 0;
    neg                                             = flag < 0;
    got                                             = (s16)flag;
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
                D_shelter_b2_north_maintenance_walkway_801863B8 = *msg;
                D_shelter_b2_north_maintenance_walkway_801863D4 = *req;
                id                                              = req->flagId;
                mode                                            = 1;
                if (id < 0) {
                    id   = -id;
                    mode = 0;
                }
                GameFlag_SetNibble(id, mode);
                Task_SpawnFromTable(&D_shelter_b2_north_maintenance_walkway_80183B54, 0, 0, 0);
                D_shelter_b2_north_maintenance_walkway_801863C0 = 1;
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

/// The event task the gate spawns. It runs the latched request's CAP command,
/// plays its first and then its second sound event (either may be 0), waiting
/// for each voice to finish, then commits the latched message's area, warp and
/// room as the save location, respawns the player task as type 0x11 and ends.
void func_shelter_b2_north_maintenance_walkway_8017D918(Task* task)
{
    switch (task->state) {
        case 0:
            Gp_StateF0.field_4 = 1;
            Gp_MsgPlayerWeapon(0);
            Gp_RunCapCmd1(D_shelter_b2_north_maintenance_walkway_801863D4.field_0);
            if (D_shelter_b2_north_maintenance_walkway_801863D4.field_8 != 0) {
                SndEvt_EnqueueType6(D_shelter_b2_north_maintenance_walkway_801863D4.field_8, 0, 0);
                task->state++;
            } else {
                task->state = 2;
            }
            break;
        case 1:
            if (SndVoice_HasActiveId(D_shelter_b2_north_maintenance_walkway_801863D4.field_8) == 0) {
                task->state++;
            }
            break;
        case 2:
            task->state++;
            break;
        case 3:
            if (D_shelter_b2_north_maintenance_walkway_801863D4.field_C != 0) {
                SndEvt_EnqueueType6(D_shelter_b2_north_maintenance_walkway_801863D4.field_C, 0, 0);
                task->state++;
            } else {
                task->state = 5;
            }
            break;
        case 4:
            if (SndVoice_HasActiveId(D_shelter_b2_north_maintenance_walkway_801863D4.field_C) == 0) {
                task->state++;
            }
            break;
        case 5:
            SndEvt_EnqueueType7(0x80000000, 0);
            D_80071076               = 1;
            Mc_SaveData.at4.loc.area = D_shelter_b2_north_maintenance_walkway_801863B8.msgId;
            Mc_SaveData.at4.loc.warp = D_shelter_b2_north_maintenance_walkway_801863B8.field_2;
            Mc_SaveData.at4.loc.room = (u8)D_shelter_b2_north_maintenance_walkway_801863B8.field_3;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(task);
            break;
    }
}

/// Starts `event` for the outgoing message `dst` unless its flag says it has
/// already happened (answering 1). Otherwise answers 2, and - unless
/// `dst->field_5` asks for a dry run - latches the message and the event,
/// sets the flag and spawns the room's event task.
static __inline__ s32 _walkwayStartEvent(RoomEventMsg* dst, RoomLatchedEvent* event)
{
    D_shelter_b2_north_maintenance_walkway_801863B0 = 0;
    if (GameFlag_GetNibble(event->flagId) == 0 || event->flagId == 0) {
        if (dst->field_5 == 0) {
            D_shelter_b2_north_maintenance_walkway_801863A8 = *dst;
            D_shelter_b2_north_maintenance_walkway_801863C4 = *event;
            if (event->flagId != 0) {
                GameFlag_SetNibble(event->flagId, 1);
            }
            Task_SpawnFromTable(&D_shelter_b2_north_maintenance_walkway_80183B48, 0, 0, 0);
            D_shelter_b2_north_maintenance_walkway_801863B0 = 1;
        }
        return 2;
    }
    return 1;
}

/// Message handler: copies the incoming message to `out` and forwards both to
/// `func_80179A04`. Message 0x1D goes through the room's event gate on flag
/// 0xA8 with item 0x22 as prerequisite, answering 2 where the gate answers 0
/// and marking item 0x122 seen when the gate started the event. Message 0x20
/// starts the room's own event on flag 0x137; any other message answers 1.
s32 func_shelter_b2_north_maintenance_walkway_8017DA88(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    RoomEventReq     req;
    RoomLatchedEvent event;
    s32              result;

    *out = *in;
    func_80179A04(in, out);
    if (in->msgId == 0x1D) {
        req.field_0 = 3;
        req.field_4 = 1;
        req.field_8 = 0x541E0003;
        req.field_C = 0x541E0001;
        req.flagId  = 0xA8;
        req.itemId  = 0x22;
        result      = func_shelter_b2_north_maintenance_walkway_8017D7B4(&req, out);
        if (result == 0) {
            result = 2;
        }
        if (D_shelter_b2_north_maintenance_walkway_801863C0 != 0) {
            Gp_SetItemSeenBit(0x122, 1);
        }
        return result;
    }
    if (in->msgId != 0x20) {
        return 1;
    }
    event.capCmd   = 4;
    event.stageSnd = 0x541E0004;
    event.flagId   = 0x137;
    event.fade     = 0;
    return _walkwayStartEvent(out, &event);
}

s32 func_shelter_b2_north_maintenance_walkway_8017DC44(void)
{
    return 0;
}

s32 func_shelter_b2_north_maintenance_walkway_8017DC4C(void)
{
    return 0;
}

/// Room message handler. On the visit whose sub-id (`field_2`) is 1, agrees with
/// the session's own sub-id and has not yet latched nibble 0x84, it starts the
/// cutscene pair, runs `func_800E3FAC(0xA2, 0x20)`, latches the nibble and
/// applies the room's area records. The outgoing record is never written.
s32 func_shelter_b2_north_maintenance_walkway_8017DC54(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    u8 subId = in->field_2;

    if (subId == 1 && GameFlag_GetNibble(0x84) == 0 && gGameSession->at4.loc.place == subId) {
        func_800E8634((s32)&D_80165354, 0, (s32)&D_80165834);
        func_800E3FAC(0xA2, 0x20);
        GameFlag_SetNibble(0x84, 1);
        Gp_ApplyAreaRecs(D_shelter_b2_north_maintenance_walkway_80186380);
    }
    return 0;
}

s32 func_shelter_b2_north_maintenance_walkway_8017DCE4(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 7) {
        SndEvt_EnqueueType6(0x541E0000 | 7, 0, 0);
    }
    return 0;
}

void func_shelter_b2_north_maintenance_walkway_8017DD18(Task* task)
{
    task->msgTable = D_shelter_b2_north_maintenance_walkway_80183B60;
    Game_SetPtrSlot(task, 7);
    if (gGameSession->at4.loc.place == 1) {
        func_8016268C();
    }
    task->state = task->state + 1;
}

/// The room task's idle state: does nothing, though it still reserves a
/// 0x10-byte frame.
void func_shelter_b2_north_maintenance_walkway_8017DD80(Task* task)
{
    char pad[0x10];
}

/// The room task's three states: setup, idle and exit.
const TaskFuncTable3 D_shelter_b2_north_maintenance_walkway_8017D5F4 = {
    { func_shelter_b2_north_maintenance_walkway_8017DD18, func_shelter_b2_north_maintenance_walkway_8017DD80, taskKill },
};

/// Runs the room task's current state from its state table, dispatching
/// through a copy of the table taken onto the stack.
void func_shelter_b2_north_maintenance_walkway_8017DD90(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_b2_north_maintenance_walkway_8017D5F4;
    sp.funcs[task->state](task);
}
