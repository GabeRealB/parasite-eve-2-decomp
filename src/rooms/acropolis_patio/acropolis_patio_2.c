#include "common.h"

#include "main/gameflag.h"
#include "main/sound.h"
#include "main/task.h"

#include "gameplay/3CD8.h"

extern u8    D_8007216D;
extern u8    D_801153F4;
extern Task* D_acropolis_patio_80187060;

s32 func_acropolis_patio_8017DD4C(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 3) {
        SndEvt_EnqueueType6(0x51030000 | 3, 0, 0);
    }
    return 0;
}
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

void func_acropolis_patio_8017DF48(void)
{
    Game_Session->field_5 = D_8007216D = 2;
    Game_Session->field_76             = 1;
}
void func_acropolis_patio_8017DF70(u8 arg0)
{
    D_801153F4 = arg0;
}

INCLUDE_RODATA("rooms/nonmatchings/acropolis_patio/acropolis_patio_2", D_acropolis_patio_8017D5E8);
