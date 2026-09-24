#include "common.h"

#include "gameplay/268.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/sound.h"
#include "main/task.h"
#include "rooms/room_common.h"
#include "rooms/shelter_b2_north_maintenance_walkway.h"

extern s16 D_80071076;
extern u8  D_801153F4;
extern u8  D_80115690;

/// Spawn payload of the task 0x31 the event task may start.
extern GpStateBD8 D_shelter_b2_north_maintenance_walkway_801863A0;

/// The message and request the event gate latched for the event task.
extern RoomEventMsg D_shelter_b2_north_maintenance_walkway_801863B8;
extern RoomEventReq D_shelter_b2_north_maintenance_walkway_801863D4;

/// Descriptor of the event task the gate spawns.
extern TaskDesc D_shelter_b2_north_maintenance_walkway_80183B54;

/// The event task the walkway's message handler spawns for its own event. It
/// runs the latched event's CAP command and waits for it to finish, starting
/// task 0x31 when the event asks for it; then plays the event's stage sound
/// (if any) and waits for the voice to end. Finally it commits the latched
/// message's area, warp and room as the save location, respawns the player
/// task as type 0x11 and ends.
void func_shelter_b2_north_maintenance_walkway_8017D61C(Task* arg0)
{
    switch (arg0->state) {
        case 0:
            D_801153F4 = 1;
            Gp_MsgPlayerWeapon(0);
            Gp_RunCapCmd(D_shelter_b2_north_maintenance_walkway_801863C4.field_0, 0);
            D_80115690 = 1;
            arg0->state++;
            break;
        case 1:
            if (Gp_CapBusy() == 0) {
                if (D_shelter_b2_north_maintenance_walkway_801863C4.field_A != 0) {
                    D_shelter_b2_north_maintenance_walkway_801863A0.field_0 = 0;
                    D_shelter_b2_north_maintenance_walkway_801863A0.field_1 = 0;
                    D_shelter_b2_north_maintenance_walkway_801863A0.field_2 = 0x1E;
                    Task_Spawn(1, 0x31, 0, (s32)&D_shelter_b2_north_maintenance_walkway_801863A0);
                }
                arg0->state++;
            }
            break;
        case 2:
            if (D_shelter_b2_north_maintenance_walkway_801863C4.field_4 != 0) {
                Gp_EnqueueStageSnd6(D_shelter_b2_north_maintenance_walkway_801863C4.field_4, 0, 0);
                arg0->state++;
            } else {
                arg0->state = 4;
            }
            break;
        case 3:
            if (SndVoice_HasActiveId(Gp_PackStageSndId(D_shelter_b2_north_maintenance_walkway_801863C4.field_4)) == 0) {
                arg0->state++;
            }
            break;
        case 4:
            SndEvt_EnqueueType7(0x80000000, 0);
            D_80071076               = 1;
            Mc_SaveData.at4.loc.area = D_shelter_b2_north_maintenance_walkway_801863A8.msgId;
            Mc_SaveData.at4.loc.warp = D_shelter_b2_north_maintenance_walkway_801863A8.field_2;
            Mc_SaveData.at4.loc.room = (u8)D_shelter_b2_north_maintenance_walkway_801863A8.field_3;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(arg0);
            break;
    }
}

s32 func_shelter_b2_north_maintenance_walkway_8017D7B4(RoomEventReq* req, RoomEventMsg* msg)
{
    s32 flag;
    s32 id;
    s32 mode;
    s32 got;
    s32 ret;
    s32 neg;

    flag                                            = req->flagId;
    D_shelter_b2_north_maintenance_walkway_801863C0 = 0;
    neg                                             = flag < 0;
    got                                             = (s16)flag;
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
                D_shelter_b2_north_maintenance_walkway_801863B8 = *msg;
                D_shelter_b2_north_maintenance_walkway_801863D4 = *req;
                id                                              = req->flagId;
                mode                                            = 1;
                if (id < 0) {
                    id   = -id;
                    mode = 0;
                }
                GameFlag_SetNibble(id, mode);
                Task_SpawnFromTable(&D_shelter_b2_north_maintenance_walkway_80183B54, 0, 0, 0);
                D_shelter_b2_north_maintenance_walkway_801863C0 = 1;
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

/// The event task the gate spawns. It runs the latched request's CAP command,
/// plays its first and then its second sound event (either may be 0), waiting
/// for each voice to finish, then commits the latched message's area, warp and
/// room as the save location, respawns the player task as type 0x11 and ends.
void func_shelter_b2_north_maintenance_walkway_8017D918(Task* task)
{
    switch (task->state) {
        case 0:
            D_801153F4 = 1;
            Gp_MsgPlayerWeapon(0);
            Gp_RunCapCmd1(D_shelter_b2_north_maintenance_walkway_801863D4.field_0);
            if (D_shelter_b2_north_maintenance_walkway_801863D4.field_8 != 0) {
                SndEvt_EnqueueType6(D_shelter_b2_north_maintenance_walkway_801863D4.field_8, 0, 0);
                task->state++;
            } else {
                task->state = 2;
            }
            break;
        case 1:
            if (SndVoice_HasActiveId(D_shelter_b2_north_maintenance_walkway_801863D4.field_8) == 0) {
                task->state++;
            }
            break;
        case 2:
            task->state++;
            break;
        case 3:
            if (D_shelter_b2_north_maintenance_walkway_801863D4.field_C != 0) {
                SndEvt_EnqueueType6(D_shelter_b2_north_maintenance_walkway_801863D4.field_C, 0, 0);
                task->state++;
            } else {
                task->state = 5;
            }
            break;
        case 4:
            if (SndVoice_HasActiveId(D_shelter_b2_north_maintenance_walkway_801863D4.field_C) == 0) {
                task->state++;
            }
            break;
        case 5:
            SndEvt_EnqueueType7(0x80000000, 0);
            D_80071076               = 1;
            Mc_SaveData.at4.loc.area = D_shelter_b2_north_maintenance_walkway_801863B8.msgId;
            Mc_SaveData.at4.loc.warp = D_shelter_b2_north_maintenance_walkway_801863B8.field_2;
            Mc_SaveData.at4.loc.room = (u8)D_shelter_b2_north_maintenance_walkway_801863B8.field_3;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(task);
            break;
    }
}
