#include "common.h"

#include "main/gameflag.h"
#include "main/mc.h"
#include "main/sound.h"
#include "main/task.h"

#include "gameplay/1A8.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"

extern u8         D_8007216C;
extern u8         D_801153F4;
extern s16        D_80114D08;
extern s16        D_80071076;
extern u8         D_80115680;
extern u8         D_80115690;
extern u8         D_shelter_b4_upper_sewer_80186318;
extern u8         D_shelter_b4_upper_sewer_80188D2C;
extern TaskDesc   D_shelter_b4_upper_sewer_80186300;
extern GpMsgEntry D_shelter_b4_upper_sewer_801862D0[];
extern s16        D_shelter_b4_upper_sewer_80186438;
extern TaskDesc   D_shelter_b4_upper_sewer_8018643C[];
extern void       func_shelter_b4_upper_sewer_8017E59C(s32);
/// Save location filled from the outgoing location just before a table task is
/// spawned: `field_2` / `field_4` / `field_1` take its `field_0` / `field_2` /
/// `field_3`.
extern GpSaveLoc D_shelter_b4_upper_sewer_80188D24;
/// Spawn argument for the task `func_shelter_b4_upper_sewer_8017D80C` starts
/// with `Task_Spawn(1, 0x31, ...)`.
extern GpStateBD8 D_shelter_b4_upper_sewer_80188D1C;

extern s32 func_80179A04(GpSaveLoc* in, GpSaveLoc* out);
INCLUDE_RODATA("rooms/nonmatchings/shelter_b4_upper_sewer/shelter_b4_upper_sewer", D_shelter_b4_upper_sewer_8017D5C4);

void func_shelter_b4_upper_sewer_8017D660(Task* task)
{
    switch (task->state) {
        case 0:
            func_shelter_b4_upper_sewer_8017E59C(0);
            gGameSession->eventState = 1;
            task->state++;
            break;
        case 1:
            gGameSession->hideHud = 1;
            Gp_RunCapCmd(1, 0);
            D_80115690 = 1;
            D_80115680 = 5;
            task->state++;
            break;
        case 2:
            if (Gp_CapBusy() == 0) {
                task->state++;
            }
            break;
        case 3:
            if (Gp_GetCapEventKey() == 0xC) {
                taskKill(task);
                D_8007216C = D_shelter_b4_upper_sewer_80188D2C;
                Gp_MsgPlayerWeapon(1);
                Gp_MsgPlayer3F3(1);
                Gp_MsgAllyWeapon(1);
                Gp_MsgAlly3F3(1);
                gGameSession->eventState = 0;
                gGameSession->hideHud    = 0;
                D_801153F4               = 0;
                D_80114D08               = 0xA;
                break;
            }
            func_800E8614((s32)&D_shelter_b4_upper_sewer_80186318, 0);
            GameFlag_SetNibble(0xB8, 1);
            GameFlag_SetNibble(0x1BD, 0);
            task->state++;
            break;
        case 4:
            if (gGameSession->eventState == 0) {
                D_80114D08 = 0xA;
                D_801153F4 = 0;
                taskKill(task);
            }
            break;
    }
}

void func_shelter_b4_upper_sewer_8017D80C(Task* arg0)
{
    switch (arg0->state) {
        case 0:
            D_801153F4 = 1;
            Gp_MsgPlayerWeapon(0);
            Gp_RunCapCmd(arg0->spawnArg1, 0);
            arg0->state++;
            break;
        case 1:
            if (Gp_CapBusy() == 0) {
                arg0->state++;
            }
            break;
        case 2:
            if (Gp_GetCapEventKey() != 0xA) {
                taskKill(arg0);
                Gp_MsgPlayerWeapon(1);
                D_801153F4 = 0;
                D_80114D08 = 0xA;
                break;
            }
            D_801153F4 = 1;
            Gp_TriggerPeIfArmed();
            D_shelter_b4_upper_sewer_80188D1C.field_0 = 0;
            D_shelter_b4_upper_sewer_80188D1C.field_1 = 0;
            D_shelter_b4_upper_sewer_80188D1C.field_2 = 0x1E;
            Task_Spawn(1, 0x31, 0, (s32)&D_shelter_b4_upper_sewer_80188D1C);
            arg0->killCountdown = 0x1E;
            arg0->state++;
            break;
        case 3:
            if (--arg0->killCountdown == 0) {
                SndEvt_EnqueueType6(0x542C0003, 0, 0);
                arg0->state++;
            }
            break;
        case 4:
            if (SndVoice_HasActiveId(0x542C0003) == 0) {
                arg0->state++;
            }
            break;
        case 5:
            D_80071076               = 1;
            Mc_SaveData.at4.loc.area = D_shelter_b4_upper_sewer_80188D24.field_2;
            Mc_SaveData.at4.loc.warp = D_shelter_b4_upper_sewer_80188D24.field_4;
            Mc_SaveData.at4.loc.room = D_shelter_b4_upper_sewer_80188D24.field_1;
            Task_Spawn(0, 0x11, 0x10, 0);
            taskKill(arg0);
            break;
    }
}

s32 func_shelter_b4_upper_sewer_8017D9BC(void)
{
    return 0;
}

s32 func_shelter_b4_upper_sewer_8017D9C4(Task* task, s32 msgId, GpSaveLoc* src, GpSaveLoc* dst)
{
    *dst = *src;
    func_80179A04(src, dst);
    if (*(u16*)src == 0x2D) {
        if (src->field_5 == 0) {
            D_shelter_b4_upper_sewer_80188D24.field_2 = dst->field_0;
            D_shelter_b4_upper_sewer_80188D24.field_4 = dst->field_2;
            D_shelter_b4_upper_sewer_80188D24.field_1 = dst->field_3;
            Task_SpawnFromTable(&D_shelter_b4_upper_sewer_80186300, 1, 7, 0);
        }
        return 0;
    }
    if (*(u16*)src == 0x2E) {
        if (src->field_5 == 0) {
            D_shelter_b4_upper_sewer_80188D24.field_2 = dst->field_0;
            D_shelter_b4_upper_sewer_80188D24.field_4 = dst->field_2;
            D_shelter_b4_upper_sewer_80188D24.field_1 = dst->field_3;
            Task_SpawnFromTable(&D_shelter_b4_upper_sewer_80186300, 1, 8, 0);
        }
        return 0;
    }
    return 1;
}

s32 func_shelter_b4_upper_sewer_8017DAB0(Task* task, s32 msgId, s32 arg2)
{
    u8 temp_a1;

    if (arg2 == 1) {
        if (GameFlag_GetNibble(0xB8) == 0) {
            Gp_MsgPlayer3F3(0);
            Gp_MsgAlly3F3(0);
            Gp_MsgPlayerWeapon(0);
            Gp_MsgAllyWeapon(0);
            D_801153F4                        = 2;
            temp_a1                           = Mc_SaveData.at4.loc.view;
            Mc_SaveData.at4.loc.view          = 0xD;
            D_shelter_b4_upper_sewer_80188D2C = temp_a1;
            Task_SpawnFromTable(&D_shelter_b4_upper_sewer_80186300, 0, 0, 0);
        } else {
            Gp_RunCapCmd1(6);
        }
    }
    return 0;
}

s32 func_shelter_b4_upper_sewer_8017DB50(void)
{
    return 0;
}

s32 func_shelter_b4_upper_sewer_8017DB58(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 4) {
        func_shelter_b4_upper_sewer_8017E59C(1);
        SndEvt_EnqueueType6(0x542C0004, 0, 0);
    }
    return 0;
}

void func_shelter_b4_upper_sewer_8017DB94(void)
{
    D_8007216C = D_shelter_b4_upper_sewer_80188D2C;
}

void func_shelter_b4_upper_sewer_8017DBA8(Task* task)
{
    task->msgTable = D_shelter_b4_upper_sewer_801862D0;
    Game_SetPtrSlot(task, 7);
    if (GameFlag_GetNibble(0xB7) != 0) {
        D_shelter_b4_upper_sewer_80186438 = -0x708;
        Task_SpawnFromTable(D_shelter_b4_upper_sewer_8018643C, 0, 0, 0);
    } else {
        D_shelter_b4_upper_sewer_80186438 = 0;
    }
    task->state = (s32)(task->state + 1);
}

void func_shelter_b4_upper_sewer_8017DC28(void)
{
}
