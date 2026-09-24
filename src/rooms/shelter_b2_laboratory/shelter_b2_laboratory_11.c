#include "common.h"

#include "gameplay/268.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/sound.h"
#include "main/task.h"
#include "rooms/room_common.h"

extern s16 D_80071076;
extern u8  D_801153F4;

/// Copies of the message and request that started the pending transition,
/// read back by `func_shelter_b2_laboratory_8017FBA8`.
extern RoomEventMsg D_shelter_b2_laboratory_801864AC;
extern RoomEventReq D_shelter_b2_laboratory_8018652C;

/// Set when `func_shelter_b2_laboratory_8017FA44` started a transition,
/// cleared on every other call.
extern u8 D_shelter_b2_laboratory_801864B4;

/// Task table spawned to run the transition.
extern TaskDesc D_shelter_b2_laboratory_80182A2C;

/// Handles a request to leave through a flag-gated exit. When the flag named
/// by `req->flagId` (negated: must be clear) is already in the wanted state,
/// returns 1. Otherwise, if the item `req->itemId` has been collected (or none
/// is needed), sets the flag, records `msg` and `req`, spawns the transition
/// task and returns 2; if the item is missing, runs cap command `req->field_4`
/// and returns 0. The spawn and the cap command happen only when
/// `msg->field_5` is 0.
s32 func_shelter_b2_laboratory_8017FA44(RoomEventReq* req, RoomEventMsg* msg)
{
    s32 flag;
    s32 id;
    s32 mode;
    s32 got;
    s32 ret;
    s32 neg;

    flag                             = req->flagId;
    D_shelter_b2_laboratory_801864B4 = 0;
    neg                              = flag < 0;
    got                              = (s16)flag;
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
                D_shelter_b2_laboratory_801864AC = *msg;
                D_shelter_b2_laboratory_8018652C = *req;
                id                               = req->flagId;
                mode                             = 1;
                if (id < 0) {
                    id   = -id;
                    mode = 0;
                }
                GameFlag_SetNibble(id, mode);
                Task_SpawnFromTable(&D_shelter_b2_laboratory_80182A2C, 0, 0, 0);
                D_shelter_b2_laboratory_801864B4 = 1;
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

/// Transition task: runs cap command `field_0` of the recorded request, plays
/// its two voice events `field_8` and `field_C` in turn (each optional) and
/// waits for them, then stops the sound, points the save's location at the
/// recorded message's destination and spawns the area-change task.
void func_shelter_b2_laboratory_8017FBA8(Task* task)
{
    switch (task->state) {
        case 0:
            D_801153F4 = 1;
            Gp_MsgPlayerWeapon(0);
            Gp_RunCapCmd1(D_shelter_b2_laboratory_8018652C.field_0);
            if (D_shelter_b2_laboratory_8018652C.field_8 != 0) {
                SndEvt_EnqueueType6(D_shelter_b2_laboratory_8018652C.field_8, 0, 0);
                task->state++;
            } else {
                task->state = 2;
            }
            break;
        case 1:
            if (SndVoice_HasActiveId(D_shelter_b2_laboratory_8018652C.field_8) == 0) {
                task->state++;
            }
            break;
        case 2:
            task->state++;
            break;
        case 3:
            if (D_shelter_b2_laboratory_8018652C.field_C != 0) {
                SndEvt_EnqueueType6(D_shelter_b2_laboratory_8018652C.field_C, 0, 0);
                task->state++;
            } else {
                task->state = 5;
            }
            break;
        case 4:
            if (SndVoice_HasActiveId(D_shelter_b2_laboratory_8018652C.field_C) == 0) {
                task->state++;
            }
            break;
        case 5:
            SndEvt_EnqueueType7(0x80000000, 0);
            D_80071076               = 1;
            Mc_SaveData.at4.loc.area = D_shelter_b2_laboratory_801864AC.msgId;
            Mc_SaveData.at4.loc.warp = D_shelter_b2_laboratory_801864AC.field_2;
            Mc_SaveData.at4.loc.room = (u8)D_shelter_b2_laboratory_801864AC.field_3;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(task);
            break;
    }
}
