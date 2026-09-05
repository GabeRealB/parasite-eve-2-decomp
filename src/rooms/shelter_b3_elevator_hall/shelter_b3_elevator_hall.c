#include "common.h"

#include "gameplay/3CD8.h"

#include "main/gameflag.h"
#include "main/mc.h"
#include "main/sound.h"
#include "main/task.h"

extern s16      D_80071076;
extern u8       D_801153F4;
extern TaskDesc D_shelter_b3_elevator_hall_80182A2C[];

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_elevator_hall/shelter_b3_elevator_hall", func_shelter_b3_elevator_hall_8017D62C);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_elevator_hall/shelter_b3_elevator_hall", func_shelter_b3_elevator_hall_8017D790);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_elevator_hall/shelter_b3_elevator_hall", func_shelter_b3_elevator_hall_8017D900);

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
        Task_Kill(task);
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
        Mc_SaveData.field_6 = 0x1A;
        Mc_SaveData.field_8 = 1;
        Mc_SaveData.field_5 = 1;
    } else {
        Gp_MsgPlayerWeapon(1);
        D_801153F4 = 0;
        Task_Kill(task);
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
    Task_Kill(task);
}

s32 func_shelter_b3_elevator_hall_8017DC78(void)
{
    return 0;
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_elevator_hall/shelter_b3_elevator_hall", func_shelter_b3_elevator_hall_8017DC80);

s32 func_shelter_b3_elevator_hall_8017DD88(void)
{
    return 0;
}

s32 func_shelter_b3_elevator_hall_8017DD90(void)
{
    return 0;
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_elevator_hall/shelter_b3_elevator_hall", func_shelter_b3_elevator_hall_8017DD98);
