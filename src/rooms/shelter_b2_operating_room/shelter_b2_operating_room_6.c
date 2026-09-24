#include "common.h"

#include "gameplay/268.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/sound.h"
#include "main/task.h"

#include "rooms/room_common.h"
#include "rooms/shelter_b2_operating_room.h"

extern s16 D_80071076;
extern u8  D_801153F4;

/// The message and request of the exit the gate last accepted, latched for
/// the transition task.
extern RoomEventMsg D_shelter_b2_operating_room_8018421C;
extern RoomEventReq D_shelter_b2_operating_room_80184238;
/// Set to 1 once the gate has spawned the transition task, 0 otherwise.
extern u8 D_shelter_b2_operating_room_80184224;
/// Spawn descriptor of the transition task.
extern TaskDesc D_shelter_b2_operating_room_80180904;

/// Handles a request to leave through a flag-gated exit. When the flag named
/// by `req->flagId` (negated: must be clear) is already in the wanted state,
/// returns 1. Otherwise, if the item `req->itemId` has been collected (or none
/// is needed), sets the flag, records `msg` and `req`, spawns the transition
/// task and returns 2; if the item is missing, runs cap command `req->field_4`
/// and returns 0. The spawn and the cap command happen only when
/// `msg->field_5` is 0.
s32 func_shelter_b2_operating_room_8017D628(RoomEventReq* req, RoomEventMsg* msg)
{
    s32 flag;
    s32 id;
    s32 mode;
    s32 got;
    s32 ret;
    s32 neg;

    flag                                 = req->flagId;
    D_shelter_b2_operating_room_80184224 = 0;
    neg                                  = flag < 0;
    got                                  = (s16)flag;
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
                D_shelter_b2_operating_room_8018421C = *msg;
                D_shelter_b2_operating_room_80184238 = *req;
                id                                   = req->flagId;
                mode                                 = 1;
                if (id < 0) {
                    id   = -id;
                    mode = 0;
                }
                GameFlag_SetNibble(id, mode);
                Task_SpawnFromTable(&D_shelter_b2_operating_room_80180904, 0, 0, 0);
                D_shelter_b2_operating_room_80184224 = 1;
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

/// Transition task for an exit the gate accepted: runs the latched request's
/// cap command `field_0`, plays its sounds `field_8` and then `field_C` (each
/// only when non-zero), waiting for each voice to finish, then records the
/// latched message's area, warp and room in the save data's location, spawns
/// task 0x11 of bank 0 and kills itself.
void func_shelter_b2_operating_room_8017D78C(Task* task)
{
    switch (task->state) {
        case 0:
            D_801153F4 = 1;
            Gp_MsgPlayerWeapon(0);
            Gp_RunCapCmd1(D_shelter_b2_operating_room_80184238.field_0);
            if (D_shelter_b2_operating_room_80184238.field_8 != 0) {
                SndEvt_EnqueueType6(D_shelter_b2_operating_room_80184238.field_8, 0, 0);
                task->state++;
            } else {
                task->state = 2;
            }
            break;
        case 1:
            if (SndVoice_HasActiveId(D_shelter_b2_operating_room_80184238.field_8) == 0) {
                task->state++;
            }
            break;
        case 2:
            task->state++;
            break;
        case 3:
            if (D_shelter_b2_operating_room_80184238.field_C != 0) {
                SndEvt_EnqueueType6(D_shelter_b2_operating_room_80184238.field_C, 0, 0);
                task->state++;
            } else {
                task->state = 5;
            }
            break;
        case 4:
            if (SndVoice_HasActiveId(D_shelter_b2_operating_room_80184238.field_C) == 0) {
                task->state++;
            }
            break;
        case 5:
            SndEvt_EnqueueType7(0x80000000, 0);
            D_80071076               = 1;
            Mc_SaveData.at4.loc.area = D_shelter_b2_operating_room_8018421C.msgId;
            Mc_SaveData.at4.loc.warp = D_shelter_b2_operating_room_8018421C.field_2;
            Mc_SaveData.at4.loc.room = (u8)D_shelter_b2_operating_room_8018421C.field_3;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(task);
            break;
    }
}
