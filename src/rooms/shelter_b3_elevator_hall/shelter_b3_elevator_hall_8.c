#include "common.h"
#include "gameplay/268.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/gameflag.h"
#include "main/task.h"
#include "main/mc.h"
#include "main/sound.h"
#include "rooms/room_common.h"

#include "rooms/shelter_b3_elevator_hall.h"

extern s16 D_80071076;
extern u8  D_801153F4;

/// The event request latched by the gate, and the message that carried it.
extern RoomEventMsg D_shelter_b3_elevator_hall_80184A00;
extern RoomEventReq D_shelter_b3_elevator_hall_80184A0C;
/// Set when the gate latched a request and spawned the task that runs it.
extern u8 D_shelter_b3_elevator_hall_80184A08;
/// Descriptor of the task that runs a latched request.
extern TaskDesc D_shelter_b3_elevator_hall_80182A20;

/// Decides whether the event `req` describes fires for message `msg`. A set
/// flag nibble (a clear one for a negative `flagId`) means it already has, and
/// the answer is 1. Without the prerequisite collected item the request's CAP
/// command runs and the answer is 0. Otherwise the request and message are
/// latched, the flag nibble is written, the task that runs the request is
/// spawned, and the answer is 2. A non-zero `field_5` on the message only asks
/// for the answer and changes nothing.
s32 func_shelter_b3_elevator_hall_8017D62C(RoomEventReq* req, RoomEventMsg* msg)
{
    s32 flag;
    s32 id;
    s32 mode;
    s32 got;
    s32 ret;
    s32 neg;

    flag                                = req->flagId;
    D_shelter_b3_elevator_hall_80184A08 = 0;
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
                D_shelter_b3_elevator_hall_80184A00 = *msg;
                D_shelter_b3_elevator_hall_80184A0C = *req;
                id                                  = req->flagId;
                mode                                = 1;
                if (id < 0) {
                    id   = -id;
                    mode = 0;
                }
                GameFlag_SetNibble(id, mode);
                Task_SpawnFromTable(&D_shelter_b3_elevator_hall_80182A20, 0, 0, 0);
                D_shelter_b3_elevator_hall_80184A08 = 1;
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

/// Runs a latched event request: plays its CAP command and its two voice
/// cues in turn, waiting for each to finish, then queues a type-7 sound event
/// and loads the area, warp and room the latched message names.
void func_shelter_b3_elevator_hall_8017D790(Task* task)
{
    switch (task->state) {
        case 0:
            D_801153F4 = 1;
            Gp_MsgPlayerWeapon(0);
            Gp_RunCapCmd1(D_shelter_b3_elevator_hall_80184A0C.field_0);
            if (D_shelter_b3_elevator_hall_80184A0C.field_8 != 0) {
                SndEvt_EnqueueType6(D_shelter_b3_elevator_hall_80184A0C.field_8, 0, 0);
                task->state++;
            } else {
                task->state = 2;
            }
            break;
        case 1:
            if (SndVoice_HasActiveId(D_shelter_b3_elevator_hall_80184A0C.field_8) == 0) {
                task->state++;
            }
            break;
        case 2:
            task->state++;
            break;
        case 3:
            if (D_shelter_b3_elevator_hall_80184A0C.field_C != 0) {
                SndEvt_EnqueueType6(D_shelter_b3_elevator_hall_80184A0C.field_C, 0, 0);
                task->state++;
            } else {
                task->state = 5;
            }
            break;
        case 4:
            if (SndVoice_HasActiveId(D_shelter_b3_elevator_hall_80184A0C.field_C) == 0) {
                task->state++;
            }
            break;
        case 5:
            SndEvt_EnqueueType7(0x80000000, 0);
            D_80071076               = 1;
            Mc_SaveData.at4.loc.area = D_shelter_b3_elevator_hall_80184A00.msgId;
            Mc_SaveData.at4.loc.warp = D_shelter_b3_elevator_hall_80184A00.field_2;
            Mc_SaveData.at4.loc.room = (u8)D_shelter_b3_elevator_hall_80184A00.field_3;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(task);
            break;
    }
}
