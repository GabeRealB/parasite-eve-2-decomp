#include "common.h"

#include "gameplay/268.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/sound.h"
#include "main/task.h"
#include "rooms/room_common.h"
#include "rooms/shelter_b1_main_corridor.h"

/// The event the gate last accepted: the message that triggered it, whose
/// `msgId`, `field_2` and `field_3` name the area, warp and room the event
/// task finally loads, and the request whose CAP command and sounds it runs.
extern RoomEventMsg D_shelter_b1_main_corridor_80185D2C;
extern RoomEventReq D_shelter_b1_main_corridor_80185D48;
/// Set once the gate has latched an event and spawned its task.
extern u8 D_shelter_b1_main_corridor_80185D34;
/// Spawn descriptor of the event task, `func_shelter_b1_main_corridor_8017D784`.
extern TaskDesc D_shelter_b1_main_corridor_8018308C;

extern s16 D_80071076;
extern u8  D_801153F4;

/// The corridor's event gate: given a request and the incoming message,
/// answers whether the event fires. A nibble already in its fired state (set,
/// or clear for a negative `flagId`) answers 1. A missing collected-bit
/// prerequisite answers 0 and runs the request's CAP command. Otherwise the
/// message and request are latched, the nibble is written and the event task
/// is spawned, for 2. A non-zero `field_5` on the message only asks for the
/// answer and suppresses every side effect.
s32 func_shelter_b1_main_corridor_8017D620(RoomEventReq* req, RoomEventMsg* msg)
{
    s32 flag;
    s32 id;
    s32 mode;
    s32 got;
    s32 ret;
    s32 neg;

    flag                                = req->flagId;
    D_shelter_b1_main_corridor_80185D34 = 0;
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
                D_shelter_b1_main_corridor_80185D2C = *msg;
                D_shelter_b1_main_corridor_80185D48 = *req;
                id                                  = req->flagId;
                mode                                = 1;
                if (id < 0) {
                    id   = -id;
                    mode = 0;
                }
                GameFlag_SetNibble(id, mode);
                Task_SpawnFromTable(&D_shelter_b1_main_corridor_8018308C, 0, 0, 0);
                D_shelter_b1_main_corridor_80185D34 = 1;
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

/// Event task the gate spawns: runs the latched request's CAP command, plays
/// its two sounds in turn (each optional) and waits for each to finish, then
/// queues sound event 0x80000000, records the latched message's area, warp and
/// room in the save data's location, spawns task 0x11 and kills itself.
void func_shelter_b1_main_corridor_8017D784(Task* task)
{
    switch (task->state) {
        case 0:
            D_801153F4 = 1;
            Gp_MsgPlayerWeapon(0);
            Gp_RunCapCmd1(D_shelter_b1_main_corridor_80185D48.field_0);
            if (D_shelter_b1_main_corridor_80185D48.field_8 != 0) {
                SndEvt_EnqueueType6(D_shelter_b1_main_corridor_80185D48.field_8, 0, 0);
                task->state++;
            } else {
                task->state = 2;
            }
            break;
        case 1:
            if (SndVoice_HasActiveId(D_shelter_b1_main_corridor_80185D48.field_8) == 0) {
                task->state++;
            }
            break;
        case 2:
            task->state++;
            break;
        case 3:
            if (D_shelter_b1_main_corridor_80185D48.field_C != 0) {
                SndEvt_EnqueueType6(D_shelter_b1_main_corridor_80185D48.field_C, 0, 0);
                task->state++;
            } else {
                task->state = 5;
            }
            break;
        case 4:
            if (SndVoice_HasActiveId(D_shelter_b1_main_corridor_80185D48.field_C) == 0) {
                task->state++;
            }
            break;
        case 5:
            SndEvt_EnqueueType7(0x80000000, 0);
            D_80071076               = 1;
            Mc_SaveData.at4.loc.area = D_shelter_b1_main_corridor_80185D2C.msgId;
            Mc_SaveData.at4.loc.warp = D_shelter_b1_main_corridor_80185D2C.field_2;
            Mc_SaveData.at4.loc.room = (u8)D_shelter_b1_main_corridor_80185D2C.field_3;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(task);
            break;
    }
}
