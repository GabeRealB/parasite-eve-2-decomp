#include "common.h"

#include "gameplay/268.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/sound.h"
#include "main/task.h"
#include "rooms/dryfield_main_street.h"

extern s16 D_80071076;
extern u8  D_801153F4;

/// Descriptor of the event task the event gate spawns.
extern TaskDesc D_dryfield_main_street_80180E88;

/// The message and request the event gate latched for its event task.
extern RoomEventMsg D_dryfield_main_street_80185624;
extern RoomEventReq D_dryfield_main_street_80185644;

/// Set by the event gate when its last call latched a request and spawned the
/// event task; every call clears it first.
extern u8 D_dryfield_main_street_8018562C;

/// The room's event gate. A request whose flag nibble is already set (or clear,
/// for a negative `flagId`) answers 1. One whose prerequisite item is missing
/// runs the request's CAP command and answers 0. Otherwise the message and
/// request are latched, the nibble is written, the event task is spawned and
/// the answer is 2. A non-zero `field_5` on the message only reports the
/// answer, with none of the side effects.

s32 func_dryfield_main_street_8017D798(RoomEventReq* req, RoomEventMsg* msg)
{
    s32 flag;
    s32 id;
    s32 mode;
    s32 got;
    s32 ret;
    s32 neg;

    flag                            = req->flagId;
    D_dryfield_main_street_8018562C = 0;
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
                D_dryfield_main_street_80185624 = *msg;
                D_dryfield_main_street_80185644 = *req;
                id                              = req->flagId;
                mode                            = 1;
                if (id < 0) {
                    id   = -id;
                    mode = 0;
                }
                GameFlag_SetNibble(id, mode);
                Task_SpawnFromTable(&D_dryfield_main_street_80180E88, 0, 0, 0);
                D_dryfield_main_street_8018562C = 1;
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
void func_dryfield_main_street_8017D8FC(Task* task)
{
    switch (task->state) {
        case 0:
            D_801153F4 = 1;
            Gp_MsgPlayerWeapon(0);
            Gp_RunCapCmd1(D_dryfield_main_street_80185644.field_0);
            if (D_dryfield_main_street_80185644.field_8 != 0) {
                SndEvt_EnqueueType6(D_dryfield_main_street_80185644.field_8, 0, 0);
                task->state++;
            } else {
                task->state = 2;
            }
            break;
        case 1:
            if (SndVoice_HasActiveId(D_dryfield_main_street_80185644.field_8) == 0) {
                task->state++;
            }
            break;
        case 2:
            task->state++;
            break;
        case 3:
            if (D_dryfield_main_street_80185644.field_C != 0) {
                SndEvt_EnqueueType6(D_dryfield_main_street_80185644.field_C, 0, 0);
                task->state++;
            } else {
                task->state = 5;
            }
            break;
        case 4:
            if (SndVoice_HasActiveId(D_dryfield_main_street_80185644.field_C) == 0) {
                task->state++;
            }
            break;
        case 5:
            SndEvt_EnqueueType7(0x80000000, 0);
            D_80071076               = 1;
            Mc_SaveData.at4.loc.area = D_dryfield_main_street_80185624.msgId;
            Mc_SaveData.at4.loc.warp = D_dryfield_main_street_80185624.field_2;
            Mc_SaveData.at4.loc.room = (u8)D_dryfield_main_street_80185624.field_3;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(task);
            break;
    }
}
