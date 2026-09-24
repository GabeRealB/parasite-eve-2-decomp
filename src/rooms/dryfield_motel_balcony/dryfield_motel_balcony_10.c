#include "common.h"

#include "gameplay/268.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/sound.h"
#include "main/task.h"
#include "rooms/room_common.h"
#include "rooms/dryfield_motel_balcony.h"

extern s16 D_80071076;
extern u8  D_801153F4;

/// The message and request the event gate latched for the event task.
extern RoomEventMsg D_dryfield_motel_balcony_80186724;
extern RoomEventReq D_dryfield_motel_balcony_80186730;

/// Descriptor of the event task the gate spawns.
extern TaskDesc D_dryfield_motel_balcony_80182270;

s32 func_dryfield_motel_balcony_8017D5E8(RoomEventReq* req, RoomEventMsg* msg)
{
    s32 flag;
    s32 id;
    s32 mode;
    s32 got;
    s32 ret;
    s32 neg;

    flag                              = req->flagId;
    D_dryfield_motel_balcony_8018672C = 0;
    neg                               = flag < 0;
    got                               = (s16)flag;
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
                D_dryfield_motel_balcony_80186724 = *msg;
                D_dryfield_motel_balcony_80186730 = *req;
                id                                = req->flagId;
                mode                              = 1;
                if (id < 0) {
                    id   = -id;
                    mode = 0;
                }
                GameFlag_SetNibble(id, mode);
                Task_SpawnFromTable(&D_dryfield_motel_balcony_80182270, 0, 0, 0);
                D_dryfield_motel_balcony_8018672C = 1;
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

/// The event task the gate spawns. It raises `D_801153F4`, runs the latched
/// request's CAP command, plays its two stage sounds in turn (either may be
/// absent) waiting for each voice to finish, then stores the latched
/// message's `msgId`, `field_2` and `field_3` as the save location's area,
/// warp and room, spawns task 0x11 and kills itself.
void func_dryfield_motel_balcony_8017D74C(Task* task)
{
    switch (task->state) {
        case 0:
            D_801153F4 = 1;
            Gp_MsgPlayerWeapon(0);
            Gp_RunCapCmd1(D_dryfield_motel_balcony_80186730.field_0);
            if (D_dryfield_motel_balcony_80186730.field_8 != 0) {
                SndEvt_EnqueueType6(D_dryfield_motel_balcony_80186730.field_8, 0, 0);
                task->state++;
            } else {
                task->state = 2;
            }
            break;
        case 1:
            if (SndVoice_HasActiveId(D_dryfield_motel_balcony_80186730.field_8) == 0) {
                task->state++;
            }
            break;
        case 2:
            task->state++;
            break;
        case 3:
            if (D_dryfield_motel_balcony_80186730.field_C != 0) {
                SndEvt_EnqueueType6(D_dryfield_motel_balcony_80186730.field_C, 0, 0);
                task->state++;
            } else {
                task->state = 5;
            }
            break;
        case 4:
            if (SndVoice_HasActiveId(D_dryfield_motel_balcony_80186730.field_C) == 0) {
                task->state++;
            }
            break;
        case 5:
            SndEvt_EnqueueType7(0x80000000, 0);
            D_80071076               = 1;
            Mc_SaveData.at4.loc.area = D_dryfield_motel_balcony_80186724.msgId;
            Mc_SaveData.at4.loc.warp = D_dryfield_motel_balcony_80186724.field_2;
            Mc_SaveData.at4.loc.room = (u8)D_dryfield_motel_balcony_80186724.field_3;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(task);
            break;
    }
}
