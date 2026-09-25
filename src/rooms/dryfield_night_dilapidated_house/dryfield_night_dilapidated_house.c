#include "common.h"

#include "gameplay/1BC.h"
#include "gameplay/268.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "rooms/room_common.h"
#include "rooms/dryfield_night_dilapidated_house.h"

/// Cutscene script blob arguments of `func_800E8634`.
extern s32 D_dryfield_night_dilapidated_house_801868F4;
extern s32 D_dryfield_night_dilapidated_house_80187134;

/// The room task's message table: `{id, handler}` pairs terminated by
/// `0x7FFFFFFF`.
extern GpMsgEntry D_dryfield_night_dilapidated_house_8017E700[];

/// The message and request the event gate latched for the event task.
extern RoomEventMsg D_dryfield_night_dilapidated_house_8018A104;
extern RoomEventReq D_dryfield_night_dilapidated_house_8018A110;

/// Set by the event gate when its last call latched a request and spawned the
/// event task; every call clears it first.
extern u8 D_dryfield_night_dilapidated_house_8018A10C;

/// Descriptor of the event task the gate spawns.
extern TaskDesc D_dryfield_night_dilapidated_house_8017E6F4;

/// The room's event gate. A request whose flag nibble already records the
/// event (a set nibble, or a clear one for a negative `flagId`) answers 1. One
/// whose prerequisite item has not been collected runs the request's CAP
/// command and answers 0. Otherwise the gate answers 2 and - unless the
/// message's `field_5` asks for a dry run - latches the message and the
/// request, writes the flag nibble and spawns the event task.
s32 func_dryfield_night_dilapidated_house_8017D600(RoomEventReq* req, RoomEventMsg* msg)
{
    s32 flag;
    s32 id;
    s32 mode;
    s32 got;
    s32 ret;
    s32 neg;

    flag                                        = req->flagId;
    D_dryfield_night_dilapidated_house_8018A10C = 0;
    neg                                         = flag < 0;
    got                                         = (s16)flag;
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
                D_dryfield_night_dilapidated_house_8018A104 = *msg;
                D_dryfield_night_dilapidated_house_8018A110 = *req;
                id                                          = req->flagId;
                mode                                        = 1;
                if (id < 0) {
                    id   = -id;
                    mode = 0;
                }
                GameFlag_SetNibble(id, mode);
                Task_SpawnFromTable(&D_dryfield_night_dilapidated_house_8017E6F4, 0, 0, 0);
                D_dryfield_night_dilapidated_house_8018A10C = 1;
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
void func_dryfield_night_dilapidated_house_8017D764(Task* task)
{
    switch (task->state) {
        case 0:
            Gp_StateF0.field_4 = 1;
            Gp_MsgPlayerWeapon(0);
            Gp_RunCapCmd1(D_dryfield_night_dilapidated_house_8018A110.field_0);
            if (D_dryfield_night_dilapidated_house_8018A110.field_8 != 0) {
                SndEvt_EnqueueType6(D_dryfield_night_dilapidated_house_8018A110.field_8, 0, 0);
                task->state++;
            } else {
                task->state = 2;
            }
            break;
        case 1:
            if (SndVoice_HasActiveId(D_dryfield_night_dilapidated_house_8018A110.field_8) == 0) {
                task->state++;
            }
            break;
        case 2:
            task->state++;
            break;
        case 3:
            if (D_dryfield_night_dilapidated_house_8018A110.field_C != 0) {
                SndEvt_EnqueueType6(D_dryfield_night_dilapidated_house_8018A110.field_C, 0, 0);
                task->state++;
            } else {
                task->state = 5;
            }
            break;
        case 4:
            if (SndVoice_HasActiveId(D_dryfield_night_dilapidated_house_8018A110.field_C) == 0) {
                task->state++;
            }
            break;
        case 5:
            SndEvt_EnqueueType7(0x80000000, 0);
            gDisplayState.roomVariant = 1;
            Mc_SaveData.at4.loc.area  = D_dryfield_night_dilapidated_house_8018A104.msgId;
            Mc_SaveData.at4.loc.warp  = D_dryfield_night_dilapidated_house_8018A104.field_2;
            Mc_SaveData.at4.loc.room  = (u8)D_dryfield_night_dilapidated_house_8018A104.field_3;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(task);
            break;
    }
}

s32 func_dryfield_night_dilapidated_house_8017D8D4(void)
{
    return 0;
}

/// Handler for the room's `0x13EE` message, the warp destination the gameplay
/// side posts as `Gp_WarpLoc`: copies the incoming payload through to `out` and,
/// when the destination id is 5, offers the event gate a request that plays
/// the room's pair of stage sounds under flag nibble 0x3F. Returns 1 for a
/// destination it does not own.
s32 func_dryfield_night_dilapidated_house_8017D8DC(Task* task, s32 msgId, RoomEventMsg* in,
                                                   RoomEventMsg* out)
{
    RoomEventReq req;

    *out = *in;
    if (in->msgId == 5) {
        req.field_0 = 0xC;
        req.field_4 = 0xC;
        req.field_8 = 0x53090005;
        req.field_C = 0x53090001;
        req.flagId  = 0x3F;
        req.itemId  = 0;
        return func_dryfield_night_dilapidated_house_8017D600(&req, in);
    }
    return 1;
}

s32 func_dryfield_night_dilapidated_house_8017D960(void)
{
    return 0;
}

s32 func_dryfield_night_dilapidated_house_8017D968(void)
{
    return 0;
}

/// Room task state 0: installs the room's message table, registers the task
/// in pointer slot 7 and advances. On the first visit (flag nibble 0x92 still
/// clear) it starts the cutscene script pair when pointer slot 0xA is filled,
/// then sets nibble 0x92 to 1 and nibble 0x7A to 3 and calls
/// `func_800E3FAC(0xA2, 0x11)`.
void func_dryfield_night_dilapidated_house_8017D970(Task* arg0)
{
    arg0->msgTable = D_dryfield_night_dilapidated_house_8017E700;
    Game_SetPtrSlot(arg0, 7);
    arg0->state = (s32)(arg0->state + 1);
    if (GameFlag_GetNibble(0x92) == 0) {
        if (gameGetPtrSlot(0xA) != 0) {
            func_800E8634((s32)&D_dryfield_night_dilapidated_house_801868F4, 0,
                          (s32)&D_dryfield_night_dilapidated_house_80187134);
        }
        GameFlag_SetNibble(0x92, 1);
        GameFlag_SetNibble(0x7A, 3);
        func_800E3FAC(0xA2, 0x11);
    }
}

/// The room task's idle state, entry 1 of its three-state table: does nothing.
/// The 0x10-byte local is never used, but the original reserved the frame.
void func_dryfield_night_dilapidated_house_8017DA08(Task* task)
{
    char pad[0x10];
}

/// The room task's three states: setup, idle, and exit.
const TaskFuncTable3 D_dryfield_night_dilapidated_house_8017D5DC = {
    {
        func_dryfield_night_dilapidated_house_8017D970,
        func_dryfield_night_dilapidated_house_8017DA08,
        taskKill,
    },
};

/// Runs the room task's current state, through a copy of its state table
/// taken onto the stack.
void func_dryfield_night_dilapidated_house_8017DA18(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_night_dilapidated_house_8017D5DC;
    sp.funcs[task->state](task);
}

/// Cutscene script callback: queues the replacement of overlay 0x82.
void func_dryfield_night_dilapidated_house_8017DA70(void)
{
    CdCmd_EnqueueReplaceOverlay82();
}

/// Cutscene script callback: queues overlay 0x81.
void func_dryfield_night_dilapidated_house_8017DA90(void)
{
    CdCmd_EnqueueOverlay81();
}

/// Cutscene script callback: restores the stream random state.
void func_dryfield_night_dilapidated_house_8017DAB0(void)
{
    Gp_RestoreStreamRng();
}

/// Cutscene script callback: clears the queued CD command and restarts the CD
/// queue.
void func_dryfield_night_dilapidated_house_8017DAD0(void)
{
    CdCmd_CancelReplaceAndActivate();
}

/// Cutscene script callback: spawns the first task of the room's two-entry
/// descriptor table, the one that starts the streamed sequence.
void func_dryfield_night_dilapidated_house_8017DAF0(void)
{
    Task_SpawnFromTable(&D_dryfield_night_dilapidated_house_801872B4, 0, 0, 0);
}
