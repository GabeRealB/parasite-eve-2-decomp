#include "common.h"

#include "gameplay/1A8.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"

#include "main/gameflag.h"
#include "main/mc.h"
#include "main/sound.h"
#include "main/task.h"

#include "rooms/room_common.h"

extern GpMsgEntry D_shelter_b1_elevator_hall_80182CB8[];
extern TaskDesc   D_shelter_b1_elevator_hall_80182CAC;
extern TaskDesc   D_shelter_b1_elevator_hall_80182CE8;
extern GpSaveLoc  D_shelter_b1_elevator_hall_801849F8;
extern GpStateBD8 D_shelter_b1_elevator_hall_801849F0;

extern s16 D_80071076;
extern s8  D_801153F4;

extern s32 func_80179A04(GpSaveLoc* in, GpSaveLoc* out);

void func_shelter_b1_elevator_hall_8017D620(Task* task)
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
            func_80179A04((GpSaveLoc*)&msg, (GpSaveLoc*)&msg2);
            D_80071076               = 1;
            Mc_SaveData.at4.loc.warp = msg2.field_2;
            Mc_SaveData.at4.loc.room = msg2.field_3;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(task);
            break;
    }
}

s32 func_shelter_b1_elevator_hall_8017D810(Task* task, s32 msgId, GpSaveLoc* src, GpSaveLoc* dst)
{
    *dst = *src;
    func_80179A04(src, dst);
    if (*(u16*)src == 0xF && GameFlag_GetNibble(0xA5) == 0) {
        if (src->field_5 == 0) {
            Gp_SetNibbleIf(src->field_6, 2);
            Gp_RunCapCmd1(2);
        }
        return 0;
    }
    if (*(u16*)src == 0x1A) {
        if (GameFlag_GetNibble(0xBA) == 0) {
            if (src->field_5 == 0) {
                Gp_SetNibbleIf(src->field_6, 2);
                Gp_RunCapCmd1(1);
            }
        } else {
            if (src->field_5 == 0) {
                Gp_RunCapCmd(4, 0);
                Task_SpawnFromTable(&D_shelter_b1_elevator_hall_80182CAC, 0, 0x54090008, 0);
            }
        }
        return 0;
    }
    if (*(u16*)src == 8) {
        if (src->field_5 == 0) {
            D_shelter_b1_elevator_hall_801849F8.field_2 = dst->field_0;
            D_shelter_b1_elevator_hall_801849F8.field_4 = dst->field_2;
            D_shelter_b1_elevator_hall_801849F8.field_1 = dst->field_3;
            Gp_MsgPlayerWeapon(0);
            Task_SpawnFromTable(&D_shelter_b1_elevator_hall_80182CE8, 0, 0, 0);
        }
        return 2;
    }
    return 1;
}

INCLUDE_RODATA("rooms/nonmatchings/shelter_b1_elevator_hall/shelter_b1_elevator_hall", RoomsShared8017d878Table);

void func_shelter_b1_elevator_hall_8017D99C(Task* arg0)
{
    s16 temp_v0;

    switch (arg0->state) {
        case 0:
            Gp_RunCapCmd(5, 0);
            goto advance;
        case 1:
            if (Gp_CapBusy() != 0) {
                break;
            }
            D_801153F4 = 0;
            goto advance;
        case 2:
            if (Gp_GetCapEventKey() != 0xA) {
                taskKill(arg0);
                Gp_MsgPlayerWeapon(1);
                D_801153F4 = 0;
                break;
            }
            D_801153F4          = 1;
            arg0->killCountdown = 3;
            arg0->state++;
            break;
        case 3:
            temp_v0             = (u16)arg0->killCountdown - 1;
            arg0->killCountdown = temp_v0;
            if ((temp_v0 << 0x10) != 0) {
                break;
            }
            Gp_TriggerPeIfArmed();
            goto advance;
        case 4:
            D_shelter_b1_elevator_hall_801849F0.field_0 = 0;
            D_shelter_b1_elevator_hall_801849F0.field_1 = 0;
            D_shelter_b1_elevator_hall_801849F0.field_2 = 0x1E;
            Task_Spawn(1, 0x31, 0, (s32)&D_shelter_b1_elevator_hall_801849F0);
            SndEvt_EnqueueType6(0x54090007, 0, 0);
            goto advance;
        case 5:
            if (SndVoice_HasActiveId(0x54090007) != 0) {
                break;
            }
        advance:
            arg0->state++;
            break;
        case 6:
            SndEvt_EnqueueType7(0x80000000, 0);
            D_80071076               = 1;
            Mc_SaveData.at4.loc.area = (u8)D_shelter_b1_elevator_hall_801849F8.field_2;
            Mc_SaveData.at4.loc.warp = (u8)D_shelter_b1_elevator_hall_801849F8.field_4;
            Mc_SaveData.at4.loc.room = (u8)D_shelter_b1_elevator_hall_801849F8.field_1;
            Task_Spawn(0, 0x11, 0x10, 0);
            taskKill(arg0);
            break;
    }
}

s32 func_shelter_b1_elevator_hall_8017DB54(void)
{
    return 0;
}

s32 func_shelter_b1_elevator_hall_8017DB5C(void)
{
    return 0;
}

s32 func_shelter_b1_elevator_hall_8017DB64(void)
{
    return 0;
}

s32 func_shelter_b1_elevator_hall_8017DB6C(s32 arg0, s32 arg1, s32 arg2)
{
    switch (arg2) {
        case 6:
            SndEvt_EnqueueType6(0x16, 0, 0);
            break;
        case 8:
            SndEvt_EnqueueType6(0x54090008, 0, 0);
            break;
    }
    return 0;
}

void func_shelter_b1_elevator_hall_8017DBB8(Task* arg0)
{
    arg0->msgTable = D_shelter_b1_elevator_hall_80182CB8;
    Game_SetPtrSlot(arg0, 7);
    if (GameFlag_GetNibble(0x122) == 0) {
        GameFlag_SetNibble(0x122, 1);
        func_800E3FAC(0xA2, 0x1D);
    }
    arg0->state++;
}

void func_shelter_b1_elevator_hall_8017DC20(void)
{
}
