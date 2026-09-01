#include "common.h"

#include "main/gameflag.h"
#include "main/task.h"

#include "gameplay/3CD8.h"

extern u8    D_801153F4;
extern Task* D_acropolis_patio_80187060;

INCLUDE_ASM("rooms/nonmatchings/acropolis_patio/acropolis_patio_2", func_acropolis_patio_8017DD4C);

void func_acropolis_patio_8017DD80(Task* task)
{
    s32 state;

    state = task->state;
    switch (state) {
        case 0:
            Gp_RunCapCmd1(6);
            task->state = task->state + 1;
            return;
        case 1:
            task->state = 2;
            return;
        case 2:
            if (Gp_GetCapEventKey() == 1) {
                GameFlag_SetNibble(0x15, 1);
            }
            Task_Kill(task);
            return;
    }
}

INCLUDE_ASM("rooms/nonmatchings/acropolis_patio/acropolis_patio_2", func_acropolis_patio_8017DE2C);

void func_acropolis_patio_8017DF38(s32 arg0)
{
    D_acropolis_patio_80187060->state = arg0;
}

INCLUDE_ASM("rooms/nonmatchings/acropolis_patio/acropolis_patio_2", func_acropolis_patio_8017DF48);

void func_acropolis_patio_8017DF70(u8 arg0)
{
    D_801153F4 = arg0;
}
