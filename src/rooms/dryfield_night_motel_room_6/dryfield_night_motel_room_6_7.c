#include "common.h"

#include "main/gameflag.h"
#include "main/mc.h"
#include "main/sound.h"
#include "main/task.h"

#include "gameplay/3CD8.h"
#include "gameplay/D4.h"

extern s16 D_80071076;

extern TaskDesc D_dryfield_night_motel_room_6_80182EE0;

extern GpAreaApplyRec D_dryfield_night_motel_room_6_80186270[];
extern GpAreaApplyRec D_dryfield_night_motel_room_6_801862B0[];

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_motel_room_6/dryfield_night_motel_room_6_7", func_dryfield_night_motel_room_6_8018175C);

void func_dryfield_night_motel_room_6_8018189C(Task* arg0)
{
    Task* task;

    task = arg0;
    switch (task->state) {
        case 0:
            goto L_case0;
        case 1:
            goto advance;
        case 2:
            goto L_case2;
        case 3:
            goto L_case3;
        case 4:
            goto L_case4;
        case 5:
            goto L_case5;
    }
    return;

L_case0:
    Gp_MsgPlayerWeapon(0);
    Gp_RunCapCmd1(0x10);
    goto advance;

L_case2:
    if (Gp_GetCapEventKey() == 0xB) {
        Task_Kill(task);
        Gp_MsgPlayerWeapon(1);
    }
    goto advance;

L_case3:
    GameFlag_SetNibble(0x70, 2);
    goto advance;

L_case4:
    if (Gp_CapBusy() != 0) {
        return;
    }
advance:
    task->state = task->state + 1;
    return;

L_case5:
    Gp_FillPlayerHpMp();
    SndEvt_EnqueueType7(0x80000000, 0);
    Gp_ApplyAreaRecs(D_dryfield_night_motel_room_6_80186270);
    if (GameFlag_GetNibble(0xCE) != 0) {
        Gp_ApplyAreaRecs(D_dryfield_night_motel_room_6_801862B0);
    }
    GameFlag_SetNibble(0x59, 1);
    GameFlag_SetNibble(0x5A, 2);
    GameFlag_SetNibble(0x30, 0);
    Mc_SaveData.field_6 = 8;
    Mc_SaveData.field_8 = 1;
    Mc_SaveData.field_5 = 1;
    D_80071076          = 1;
    Task_Spawn(0, 0x11, 0, 0);
    Task_Kill(task);
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_motel_room_6/dryfield_night_motel_room_6_7", func_dryfield_night_motel_room_6_80181A0C);

s32 func_dryfield_night_motel_room_6_80181A9C(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 6) {
        SOFT_TOUCH_REG(arg2);
        if (GameFlag_GetNibble(0x61) != 0) {
            Gp_RunCapCmd1(0x14);
        } else if (GameFlag_GetNibble(0x6C) <= 0) {
            Gp_RunCapCmd1(arg2);
        } else if (GameFlag_GetNibble(0x70) < 2) {
            Task_SpawnFromTable(&D_dryfield_night_motel_room_6_80182EE0, 0, 0x11, 0);
        } else {
            Gp_RunCapCmd1(arg2);
        }
    }
    if (arg2 == 0xD) {
        if (GameFlag_GetNibble(0x61) != 0) {
            Gp_RunCapCmd1(0x13);
        } else {
            Gp_RunCapCmd1(0xD);
        }
    }
    if (arg2 == 0xB) {
        if (GameFlag_GetNibble(0x61) != 0) {
            Gp_RunCapCmd1(0x15);
        } else {
            Gp_RunCapCmd1(0xB);
        }
    }
    return 0;
}

s32 func_dryfield_night_motel_room_6_80181B74(void)
{
    return 0;
}
