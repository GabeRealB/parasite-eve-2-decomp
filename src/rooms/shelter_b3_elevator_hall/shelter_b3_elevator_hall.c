#include "common.h"

#include "gameplay/3CD8.h"

#include "main/gameflag.h"
#include "main/mc.h"
#include "main/sound.h"
#include "main/task.h"
#include "rooms/room_common.h"

#include "rooms/shelter_b3_elevator_hall.h"

extern s16      D_80071076;
extern u8       D_801153F4;
extern TaskDesc D_shelter_b3_elevator_hall_80182A2C[];
extern TaskDesc D_shelter_b3_elevator_hall_80182A68[];

extern s32 func_80179A04(RoomEventMsg* in, RoomEventMsg* out);

/// Destination choice: waits for the CAP prompt to close, then maps the
/// chosen event key 0xB, 0xC or 0xD to area 9 warp 3, area 0x1B warp 2 or
/// area 0x2A warp 3; any other key hands control back and ends the task.
/// After the voice cue in the spawn argument finishes, it resolves the room
/// through `func_80179A04` and starts the load.
void func_shelter_b3_elevator_hall_8017D900(Task* task)
{
    RoomEventMsg msg;
    RoomEventMsg msg2;

    switch (task->state) {
        case 0:
            Gp_MsgPlayerWeapon(0);
            D_801153F4 = 1;
            goto next;
        case 1:
            if (Gp_CapBusy() == 0) {
                D_801153F4 = 0;
                goto next;
            }
            break;
        case 2:
            D_801153F4 = 1;
            switch (Gp_GetCapEventKey()) {
                case 0xB:
                    Mc_SaveData.at4.loc.area = 9;
                    Mc_SaveData.at4.loc.warp = 3;
                    break;
                case 0xC:
                    Mc_SaveData.at4.loc.area = 0x1B;
                    Mc_SaveData.at4.loc.warp = 2;
                    break;
                case 0xD:
                    Mc_SaveData.at4.loc.area = 0x2A;
                    Mc_SaveData.at4.loc.warp = 3;
                    break;
                default:
                    Gp_MsgPlayerWeapon(1);
                    D_801153F4 = 0;
                    taskKill(task);
                    break;
            }
            goto next;
        case 3:
            if (SndVoice_HasActiveId(task->spawnArg1) != 0) {
                break;
            }
        next:
            task->state++;
            break;
        case 4:
            SndEvt_EnqueueType7(0x80000000, 0);
            msg.field_3 = 1;
            msg.field_5 = 0;
            msg.msgId   = Mc_SaveData.at4.loc.area;
            msg.field_2 = Mc_SaveData.at4.loc.warp;
            msg2        = msg;
            func_80179A04(&msg, &msg2);
            D_80071076               = 1;
            Mc_SaveData.at4.loc.warp = msg2.field_2;
            Mc_SaveData.at4.loc.room = msg2.field_3;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(task);
            break;
    }
}

INCLUDE_RODATA("rooms/nonmatchings/shelter_b3_elevator_hall/shelter_b3_elevator_hall", D_shelter_b3_elevator_hall_8017D5F0);

void func_shelter_b3_elevator_hall_8017DAF0(Task* arg0)
{
    Task* task;

    task = arg0;
    switch (task->state) {
        case 0:
            goto L_case0;
        case 1:
            goto L_case1;
        case 2:
            goto L_case2;
        case 3:
            goto L_case3;
        case 4:
            goto L_case4;
        case 5:
            goto L_case5;
        case 6:
            goto L_case6;
    }
    return;

L_case0:
    Gp_MsgPlayerWeapon(0);
    D_801153F4 = 1;
    goto advance;

L_case1:
    if (Gp_CapBusy() != 0) {
        return;
    }
    goto advance;

L_case2:
    if (GameFlag_GetNibble(0xCF) != 0) {
        Gp_RunCapCmd(4, 0);
        Task_SpawnFromTable(D_shelter_b3_elevator_hall_80182A2C, 0, 0x542A0001, 0);
        taskKill(task);
        SCHED_BARRIER();
    } else {
        Gp_RunCapCmd1(3);
    }
    goto advance;

L_case3:
    if (Gp_CapBusy() != 0) {
        return;
    }
    goto advance;

L_case4:
    if (Gp_GetCapEventKey() == 0x15) {
        Mc_SaveData.at4.loc.area = 0x1A;
        Mc_SaveData.at4.loc.warp = 1;
        Mc_SaveData.at4.loc.room = 1;
    } else {
        Gp_MsgPlayerWeapon(1);
        D_801153F4 = 0;
        taskKill(task);
    }
    goto advance;

L_case5:
    if (SndVoice_HasActiveId(0x542A0001) != 0) {
        return;
    }
advance:
    task->state = task->state + 1;
    return;

L_case6:
    SndEvt_EnqueueType7(0x80000000, 0);
    D_80071076 = 1;
    Task_Spawn(0, 0x11, 0, 0);
    taskKill(task);
}

s32 func_shelter_b3_elevator_hall_8017DC78(void)
{
    return 0;
}

s32 func_shelter_b3_elevator_hall_8017DC80(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    RoomEventReq req;

    *out = *in;
    func_80179A04(in, out);
    if (in->msgId == 0x29) {
        req.field_0 = 1;
        req.field_4 = 1;
        req.field_8 = 0x542A0005;
        req.field_C = 0x542A0003;
        req.flagId  = 0xA7;
        req.itemId  = 0;
        return func_shelter_b3_elevator_hall_8017D62C(&req, out);
    }
    if (in->msgId != 0x1A) {
        return 1;
    }
    if (in->field_5 == 0) {
        if (GameFlag_GetNibble(0xBA) == 0) {
            Gp_RunCapCmd1(2);
            GameFlag_SetNibble(0xBA, 1);
        }
        Task_SpawnFromTable(D_shelter_b3_elevator_hall_80182A68, 0, 0x542A0001, 0);
    }
    return 0;
}

s32 func_shelter_b3_elevator_hall_8017DD88(void)
{
    return 0;
}

s32 func_shelter_b3_elevator_hall_8017DD90(void)
{
    return 0;
}

s32 func_shelter_b3_elevator_hall_8017DD98(Task* task, s32 msgId, s32 arg2)
{
    if (arg2 == 1) {
        SndEvt_EnqueueType6(0x542A0000 | 1, 0, 0);
    }
    return 0;
}
