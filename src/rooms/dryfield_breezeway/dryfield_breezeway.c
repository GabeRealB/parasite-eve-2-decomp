#include "common.h"

#include "gameplay/3CD8.h"
#include "main/gameflag.h"
#include "main/sound.h"
#include "main/task.h"
#include "gameplay/268.h"
#include "gameplay/3A34.h"

#include "rooms/dryfield_breezeway.h"
#include "rooms/room_common.h"
#include "rooms/rooms_shared_8017d638.h"

/* The room calls the dispatcher with only the task, leaving a1-a3 holding
   whatever the caller had, so the declaration must stay unprototyped. */
s32 Gp_DispatchMsg();

extern TaskDesc D_dryfield_breezeway_80181E10[];
extern TaskDesc D_dryfield_breezeway_801820B0[];
extern TaskDesc D_dryfield_breezeway_80182E18;

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
/// item 0x15) to the shared gate `RoomsShared8017d638`, returning its answer.
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
            ret         = RoomsShared8017d638(&req, out);
            if (RoomsShared8017d638Flag != 0) {
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
                Task_Kill(arg0);
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
                Task_Kill(task);
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
        Gp_DispatchMsg(Game_GetPtrSlot(4), 0x7DA, (s32)&msg, 0x7DB);
        Task_SpawnFromTable(D_dryfield_breezeway_801820B0, 0, 0, 0);
    }
    task->state++;
    D_dryfield_breezeway_801843A8 = NULL;
}

void func_dryfield_breezeway_8017DE60(void)
{
}

INCLUDE_RODATA("rooms/nonmatchings/dryfield_breezeway/dryfield_breezeway", RoomsShared8017d878Table);

INCLUDE_RODATA("rooms/nonmatchings/dryfield_breezeway/dryfield_breezeway", RoomsShared8017fc38Table);
