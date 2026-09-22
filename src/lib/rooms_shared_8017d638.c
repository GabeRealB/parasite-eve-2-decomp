#include "common.h"
#include "gameplay/268.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/gameflag.h"
#include "main/task.h"
#include "rooms/rooms_shared_8017d638.h"
#include "main/mc.h"
#include "main/sound.h"
#include "rooms/rooms_shared_8017d74c.h"

extern s16 D_80071076;
extern u8  D_801153F4;

s32 RoomsShared8017d638(RoomEventReq* req, RoomEventMsg* msg)
{
    s32 flag;
    s32 id;
    s32 mode;
    s32 got;
    s32 ret;
    s32 neg;

    flag                    = req->flagId;
    RoomsShared8017d638Flag = 0;
    neg                     = flag < 0;
    got                     = (s16)flag;
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
                RoomsShared8017d638Msg = *msg;
                RoomsShared8017d638Req = *req;
                id                     = req->flagId;
                mode                   = 1;
                if (id < 0) {
                    id   = -id;
                    mode = 0;
                }
                GameFlag_SetNibble(id, mode);
                Task_SpawnFromTable(&RoomsShared8017d638Desc, 0, 0, 0);
                RoomsShared8017d638Flag = 1;
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

void RoomsShared8017d74c(Task* task)
{
    switch (task->state) {
        case 0:
            D_801153F4 = 1;
            Gp_MsgPlayerWeapon(0);
            Gp_RunCapCmd1(RoomsShared8017d638Req.field_0);
            if (RoomsShared8017d638Req.field_8 != 0) {
                SndEvt_EnqueueType6(RoomsShared8017d638Req.field_8, 0, 0);
                task->state++;
            } else {
                task->state = 2;
            }
            break;
        case 1:
            if (SndVoice_HasActiveId(RoomsShared8017d638Req.field_8) == 0) {
                task->state++;
            }
            break;
        case 2:
            task->state++;
            break;
        case 3:
            if (RoomsShared8017d638Req.field_C != 0) {
                SndEvt_EnqueueType6(RoomsShared8017d638Req.field_C, 0, 0);
                task->state++;
            } else {
                task->state = 5;
            }
            break;
        case 4:
            if (SndVoice_HasActiveId(RoomsShared8017d638Req.field_C) == 0) {
                task->state++;
            }
            break;
        case 5:
            SndEvt_EnqueueType7(0x80000000, 0);
            D_80071076               = 1;
            Mc_SaveData.at4.loc.area = RoomsShared8017d638Msg.msgId;
            Mc_SaveData.at4.loc.warp = RoomsShared8017d638Msg.field_2;
            Mc_SaveData.at4.loc.room = (u8)RoomsShared8017d638Msg.field_3;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(task);
            break;
    }
}
