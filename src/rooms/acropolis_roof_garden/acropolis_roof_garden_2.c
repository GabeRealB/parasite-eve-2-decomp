#include "common.h"

#include "gameplay/268.h"
#include "gameplay/3CD8.h"
#include "main/gameflag.h"
extern s32 D_acropolis_roof_garden_80183C0C;

extern u8 D_801153F4;

INCLUDE_ASM("rooms/nonmatchings/acropolis_roof_garden/acropolis_roof_garden_2", func_acropolis_roof_garden_8017D7A0);

s32 func_acropolis_roof_garden_8017D868(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    switch (arg2) {
        case 3:
            SndEvt_EnqueueType6(0x510D0003, 0, 0);
            break;
        case 5:
            break;
        case 9:
            break;
    }
    return 0;
}

s32 func_acropolis_roof_garden_8017D8AC(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 2) {
        if ((Gp_GetCurBit2Flag(0x13) == 0) || (Gp_GetCurBit2Flag(0x13) == 1)) {
            Gp_RunCapCmd1(5);
        } else {
            Gp_StartCapSlot(2, 1, 0);
        }
    }
    if (arg2 == 4) {
        if (GameFlag_GetNibble(0x155) < 6) {
            GameFlag_SetNibble(3, 0);
            GameFlag_SetNibble(0x155, 6);
        }
        D_801153F4 = 1;
        Gp_RunCapCmd(4, 0);
        func_800E3FAC(0xA2, 7);
    }
    return 0;
}

INCLUDE_ASM("rooms/nonmatchings/acropolis_roof_garden/acropolis_roof_garden_2", func_acropolis_roof_garden_8017D970);

void func_acropolis_roof_garden_8017DA48(Task* arg0)
{
    s32 temp_v1;

    temp_v1 = arg0->state;
    switch (temp_v1) { /* irregular */
        case 0x4C:
            SndEvt_EnqueueType6(0x510D000F, 0, 0);
            break;
        case 0x64:
            SndEvt_EnqueueType6(0x510D0010, 0, 0);
            D_acropolis_roof_garden_80183C0C = 0;
            Task_Kill(arg0);
            break;
    }
    arg0->state += 1;
}

INCLUDE_ASM("rooms/nonmatchings/acropolis_roof_garden/acropolis_roof_garden_2", func_acropolis_roof_garden_8017DAD4);

INCLUDE_ASM("rooms/nonmatchings/acropolis_roof_garden/acropolis_roof_garden_2", func_acropolis_roof_garden_8017DB74);

INCLUDE_ASM("rooms/nonmatchings/acropolis_roof_garden/acropolis_roof_garden_2", func_acropolis_roof_garden_8017DBEC);

INCLUDE_ASM("rooms/nonmatchings/acropolis_roof_garden/acropolis_roof_garden_2", func_acropolis_roof_garden_8017DC74);
