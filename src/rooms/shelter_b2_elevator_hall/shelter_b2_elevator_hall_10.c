#include "common.h"

#include "gameplay/268.h"
#include "gameplay/3CD8.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/sound.h"
#include "main/task.h"
#include "rooms/shelter_b2_elevator_hall.h"

extern s16 D_80071076;
extern u8  D_801153F4;

s32 func_shelter_b2_elevator_hall_8017D610(RoomEventReq* req, RoomEventMsg* msg)
{
    s32 flag;
    s32 id;
    s32 mode;
    s32 got;
    s32 ret;
    s32 neg;

    flag                                = req->flagId;
    D_shelter_b2_elevator_hall_80184D84 = 0;
    neg                                 = flag < 0;
    got                                 = (s16)flag;
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
                D_shelter_b2_elevator_hall_80184D7C = *msg;
                D_shelter_b2_elevator_hall_80184D88 = *req;
                id                                  = req->flagId;
                mode                                = 1;
                if (id < 0) {
                    id   = -id;
                    mode = 0;
                }
                GameFlag_SetNibble(id, mode);
                Task_SpawnFromTable(&D_shelter_b2_elevator_hall_80183790, 0, 0, 0);
                D_shelter_b2_elevator_hall_80184D84 = 1;
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

void func_shelter_b2_elevator_hall_8017D774(Task* task)
{
    switch (task->state) {
        case 0:
            D_801153F4 = 1;
            Gp_MsgPlayerWeapon(0);
            Gp_RunCapCmd1(D_shelter_b2_elevator_hall_80184D88.field_0);
            if (D_shelter_b2_elevator_hall_80184D88.field_8 != 0) {
                SndEvt_EnqueueType6(D_shelter_b2_elevator_hall_80184D88.field_8, 0, 0);
                task->state++;
            } else {
                task->state = 2;
            }
            break;
        case 1:
            if (SndVoice_HasActiveId(D_shelter_b2_elevator_hall_80184D88.field_8) == 0) {
                task->state++;
            }
            break;
        case 2:
            task->state++;
            break;
        case 3:
            if (D_shelter_b2_elevator_hall_80184D88.field_C != 0) {
                SndEvt_EnqueueType6(D_shelter_b2_elevator_hall_80184D88.field_C, 0, 0);
                task->state++;
            } else {
                task->state = 5;
            }
            break;
        case 4:
            if (SndVoice_HasActiveId(D_shelter_b2_elevator_hall_80184D88.field_C) == 0) {
                task->state++;
            }
            break;
        case 5:
            SndEvt_EnqueueType7(0x80000000, 0);
            D_80071076               = 1;
            Mc_SaveData.at4.loc.area = D_shelter_b2_elevator_hall_80184D7C.msgId;
            Mc_SaveData.at4.loc.warp = D_shelter_b2_elevator_hall_80184D7C.field_2;
            Mc_SaveData.at4.loc.room = (u8)D_shelter_b2_elevator_hall_80184D7C.field_3;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(task);
            break;
    }
}
