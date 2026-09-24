#include "common.h"

#include "gameplay/268.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/sound.h"
#include "main/task.h"

#include "rooms/dryfield_breezeway.h"
#include "rooms/room_common.h"

/// `GpMsgEntry` (`gameplay/D4.h`), forward-declared because that header's
/// four-argument `Gp_DispatchMsg` prototype cannot be in scope here.
struct _GpMsgEntry;

/* The room calls the dispatcher with only the task, leaving a1-a3 holding
   whatever the caller had, so the declaration must stay unprototyped. */
s32 Gp_DispatchMsg();

extern s16 D_80071076;
extern u8  D_801153F4;

/// The message and request the event gate latched, and the descriptor of the
/// event task it spawns to act on them.
extern RoomEventMsg D_dryfield_breezeway_8018439C;
extern RoomEventReq D_dryfield_breezeway_801843AC;
extern TaskDesc     D_dryfield_breezeway_80181DD4;

extern TaskDesc D_dryfield_breezeway_80181E10[];
extern TaskDesc D_dryfield_breezeway_801820B0[];
extern TaskDesc D_dryfield_breezeway_80182E18;

/// This room's `GpMsgEntry` id/handler table, the one
/// `func_dryfield_breezeway_8017DDB0` parks in `Task::msgTable` so
/// `Gp_DispatchMsg` routes messages into the room at all: 0x13EE ->
/// `func_dryfield_breezeway_8017D940`, 0x13EF ->
/// `func_dryfield_breezeway_8017DBD8` (the hotspot gate, whose sub-id 1 arms
/// the room's task), 0x13F0 -> `func_dryfield_breezeway_8017DA48` (the
/// weapon/state sequencer), 0x13F1 -> `func_dryfield_breezeway_8017D90C` and
/// 0x13F2 -> `func_dryfield_breezeway_8017DBA4`, terminated by 0x7FFFFFFF.
extern struct _GpMsgEntry D_dryfield_breezeway_80181DE0[];

/// Handle of the room's key-item event task, which
/// `func_dryfield_breezeway_8017DC3C` spawns from
/// `D_dryfield_breezeway_80182E18` in its state 0 and drops again once
/// `Task_PollKill` reaps it; `func_dryfield_breezeway_8017DDB0` clears it when
/// the message task starts. `func_dryfield_breezeway_8017D90C` forwards message
/// 0x13F1 to it through `Gp_DispatchMsg`, answering 0 while there is none.
extern Task* D_dryfield_breezeway_801843A8;

/// Raised by the room's event gate `func_dryfield_breezeway_8017D638` when it
/// latched a request and spawned the event task, cleared on every other call.
extern u8 D_dryfield_breezeway_801843A4;

/// The room's event gate, called by `func_dryfield_breezeway_8017D940` with the
/// request it builds on the stack. A set flag nibble (or a clear one, for a
/// negative `flagId`) means the event has already happened and the answer is
/// 1; a missing collected-bit prerequisite runs the request's `field_4` CAP
/// command and answers 0; otherwise the request and message are latched into
/// `D_dryfield_breezeway_801843AC` / `D_dryfield_breezeway_8018439C`, the flag
/// nibble is written, the event task is spawned and
/// `D_dryfield_breezeway_801843A4` is raised, for 2. A non-zero `field_5` on
/// the message asks what would happen and suppresses all of those effects.
s32 func_dryfield_breezeway_8017D638(RoomEventReq* req, RoomEventMsg* msg)
{
    s32 flag;
    s32 id;
    s32 mode;
    s32 got;
    s32 ret;
    s32 neg;

    flag                          = req->flagId;
    D_dryfield_breezeway_801843A4 = 0;
    neg                           = flag < 0;
    got                           = (s16)flag;
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
                D_dryfield_breezeway_8018439C = *msg;
                D_dryfield_breezeway_801843AC = *req;
                id                            = req->flagId;
                mode                          = 1;
                if (id < 0) {
                    id   = -id;
                    mode = 0;
                }
                GameFlag_SetNibble(id, mode);
                Task_SpawnFromTable(&D_dryfield_breezeway_80181DD4, 0, 0, 0);
                D_dryfield_breezeway_801843A4 = 1;
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

/// The event task `D_dryfield_breezeway_80181DD4` describes, spawned by the gate
/// above once it has latched a request: it runs the request's CAP command,
/// plays and waits out its two sounds (`field_8`, then `field_C`, either
/// skipped when zero), then writes the latched message's destination into the
/// save's location and spawns the room-change task, killing itself.
void func_dryfield_breezeway_8017D79C(Task* task)
{
    switch (task->state) {
        case 0:
            D_801153F4 = 1;
            Gp_MsgPlayerWeapon(0);
            Gp_RunCapCmd1(D_dryfield_breezeway_801843AC.field_0);
            if (D_dryfield_breezeway_801843AC.field_8 != 0) {
                SndEvt_EnqueueType6(D_dryfield_breezeway_801843AC.field_8, 0, 0);
                task->state++;
            } else {
                task->state = 2;
            }
            break;
        case 1:
            if (SndVoice_HasActiveId(D_dryfield_breezeway_801843AC.field_8) == 0) {
                task->state++;
            }
            break;
        case 2:
            task->state++;
            break;
        case 3:
            if (D_dryfield_breezeway_801843AC.field_C != 0) {
                SndEvt_EnqueueType6(D_dryfield_breezeway_801843AC.field_C, 0, 0);
                task->state++;
            } else {
                task->state = 5;
            }
            break;
        case 4:
            if (SndVoice_HasActiveId(D_dryfield_breezeway_801843AC.field_C) == 0) {
                task->state++;
            }
            break;
        case 5:
            SndEvt_EnqueueType7(0x80000000, 0);
            D_80071076               = 1;
            Mc_SaveData.at4.loc.area = D_dryfield_breezeway_8018439C.msgId;
            Mc_SaveData.at4.loc.warp = D_dryfield_breezeway_8018439C.field_2;
            Mc_SaveData.at4.loc.room = (u8)D_dryfield_breezeway_8018439C.field_3;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(task);
            break;
    }
}

s32 func_dryfield_breezeway_8017D90C(void)
{
    s32 ret;

    if (D_dryfield_breezeway_801843A8 == NULL) {
        ret = 0;
    } else {
        ret = Gp_DispatchMsg(D_dryfield_breezeway_801843A8);
    }
    return ret;
}

/// `GpMsgEntry` handler for message 0x13EE, the room's own progress gate. It
/// answers message 0x17 by writing 1 or 2 into the outgoing record's `field_3`
/// from the room's progress nibble 0x47, and - when the message id still reads
/// 0x17 on a second look - hands the room's event request (flag nibble 0x37,
/// item 0x15) to the room's event gate `func_dryfield_breezeway_8017D638`,
/// returning its answer.
/// A gate that latched the request is followed by the room's own follow-up:
/// progress nibble 0x56 set to 4 and effect 0xA2. Everything else answers 1.
s32 func_dryfield_breezeway_8017D940(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    RoomEventReq req;
    s32          ret;

    *out = *in;
    if (in->msgId == 0x17) {
        if (in->field_5 == 0) {
            if (GameFlag_GetNibble(0x47) == 0) {
                out->field_3 = 1;
            } else {
                out->field_3 = 2;
            }
        }
        if (in->msgId == 0x17) {
            req.field_0 = 4;
            req.field_4 = 2;
            req.field_8 = 0x52160006;
            req.field_C = 0x52160003;
            req.flagId  = 0x37;
            req.itemId  = 0x15;
            ret         = func_dryfield_breezeway_8017D638(&req, out);
            if (D_dryfield_breezeway_801843A4 != 0) {
                GameFlag_SetNibble(0x56, 4);
                func_800E3FAC(0xA2, 0x38);
            }
            return ret;
        }
    }
    return 1;
}

s32 func_dryfield_breezeway_8017DA48(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    switch (arg2) {
        case 1:
            if (Gp_StateF0.field_0 == 1) {
                Gp_RunCapCmd1(5);
            } else {
                if (GameFlag_GetNibble(0x56) != 4) {
                    if (Gp_HasCollectedBit(0x115) != 0) {
                        GameFlag_SetNibble(0x56, 3);
                    } else if (GameFlag_GetNibble(0xFE) != 0) {
                        if (Gp_HasCollectedBit(0x11B) == 0) {
                            if (GameFlag_GetNibble(0x56) != 6) {
                                GameFlag_SetNibble(0x56, 5);
                            }
                        } else {
                            GameFlag_SetNibble(0x56, 2);
                        }
                    }
                }
                Gp_MsgPlayerWeapon(0);
                Task_SpawnFromTable(D_dryfield_breezeway_80181E10, 1, arg2, 0);
            }
            break;
        case 3:
            if (GameFlag_GetNibble(0x56) >= 2) {
                if (Gp_StateF0.field_0 != 1) {
                    if (Gp_GetCurBit2Flag(6) == 1) {
                        Task_SpawnFromTable(D_dryfield_breezeway_80181E10, 0, 0, 0);
                        GameFlag_SetNibble(0xFE, 1);
                    }
                } else {
                    Gp_RunCapCmd1(5);
                }
            }
            break;
    }
    return 0;
}

s32 func_dryfield_breezeway_8017DBA4(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    if (arg2 == 7) {
        SndEvt_EnqueueType6(0x52160000 | 7, 0, 0);
    }
    return 0;
}

/// `GpMsgEntry` handler for message 0x13EF, the room's hotspot gate: sub-id 1
/// arms the room's own task the first time it is seen, latching nibble 0x5D so
/// a repeat visit does nothing. Only the incoming record is read - the handler
/// answers 0 and never edits the outgoing copy.
s32 func_dryfield_breezeway_8017DBD8(Task* task, s32 msgId, RoomEventMsg* in, RoomEventMsg* out)
{
    if (GameFlag_GetNibble(0x5D) == 0 && in->field_2 == 1) {
        GameFlag_SetNibble(0x5D, 1);
        Task_SpawnFromTable(D_dryfield_breezeway_801820B0, 1, 0, 0);
    }
    return 0;
}

/// The breezeway's room task, spawned from the room data table. State 0 arms
/// the room: it silences the two weapon displays and spawns the secondary task
/// `D_dryfield_breezeway_80182E18` describes, keeping the handle so state 1 can
/// reap it. State 1 polls that child and, once it is gone, drops the handle and
/// kills the room task with it.
void func_dryfield_breezeway_8017DC3C(Task* arg0)
{
    s32 sp10;
    s32 temp_v1;

    temp_v1 = arg0->state;
    switch (temp_v1) {
        case 0:
            Gp_MsgPlayerWeapon(0);
            Gp_MsgPlayer3F3(0);
            D_dryfield_breezeway_801843A8 = Task_SpawnFromTable(&D_dryfield_breezeway_80182E18, 0, 0, 0);
            arg0->state                  += 1;
            return;
        case 1:
            if (Task_PollKill(D_dryfield_breezeway_801843A8, &sp10) != 0) {
                D_dryfield_breezeway_801843A8 = NULL;
                taskKill(arg0);
            }
            return;
    }
}

void func_dryfield_breezeway_8017DCE4(Task* task)
{
    switch (task->state) {
        case 0:
            Gp_RunCapCmd(task->spawnArg1, 0);
            task->state++;
            break;
        case 1:
            if (Gp_CapBusy() == 0) {
                if (GameFlag_GetNibble(0x56) != 4) {
                    if (Gp_GetCapEventKey() == 0xB) {
                        GameFlag_SetNibble(0x56, 2);
                    }
                    if (GameFlag_GetNibble(0x56) == 5) {
                        GameFlag_SetNibble(0x56, 6);
                    }
                }
                Gp_MsgPlayerWeapon(1);
                taskKill(task);
            }
            break;
    }
}

void func_dryfield_breezeway_8017DDB0(Task* task)
{
    DbwMsg7DA msg;

    task->msgTable = D_dryfield_breezeway_80181DE0;
    Game_SetPtrSlot(task, 7);
    if (GameFlag_GetNibble(0x5D) == 0) {
        msg.field_0 = gGameSession->at4.loc.stage;
        msg.field_1 = gGameSession->at4.loc.area;
        msg.field_2 = 0;
        Gp_DispatchMsg(gameGetPtrSlot(4), 0x7DA, (s32)&msg, 0x7DB);
        Task_SpawnFromTable(D_dryfield_breezeway_801820B0, 0, 0, 0);
    }
    task->state++;
    D_dryfield_breezeway_801843A8 = NULL;
}

void func_dryfield_breezeway_8017DE60(Task* task)
{
}

/// State handlers of the room's message task, indexed by its state through
/// `func_dryfield_breezeway_8017DE68`: publish the message table, idle, then
/// kill.
const TaskFuncTable3 D_dryfield_breezeway_8017D5DC = {
    { func_dryfield_breezeway_8017DDB0, func_dryfield_breezeway_8017DE60, taskKill }
};

/// Runs the room's message task through its three states: publishing the
/// room's message table (`func_dryfield_breezeway_8017DDB0`), idling
/// (`func_dryfield_breezeway_8017DE60`) and `taskKill`. The table is copied
/// onto the stack first, so the call goes through a local copy rather than the
/// rodata.
void func_dryfield_breezeway_8017DE68(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_breezeway_8017D5DC;
    sp.funcs[task->state](task);
}
