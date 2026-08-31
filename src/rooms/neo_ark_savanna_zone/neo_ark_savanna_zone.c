#include "common.h"
#include "gameplay/3CD8.h"
extern s32 D_8011572C;
extern s32 D_80115750;
extern s32 D_80115758;

INCLUDE_RODATA("rooms/nonmatchings/neo_ark_savanna_zone/neo_ark_savanna_zone", D_neo_ark_savanna_zone_8017D5C0);

INCLUDE_ASM("rooms/nonmatchings/neo_ark_savanna_zone/neo_ark_savanna_zone", func_neo_ark_savanna_zone_8017D5E4);

INCLUDE_ASM("rooms/nonmatchings/neo_ark_savanna_zone/neo_ark_savanna_zone", func_neo_ark_savanna_zone_8017D77C);

s32 func_neo_ark_savanna_zone_8017D8F0(void)
{
    return 0;
}

s32 func_neo_ark_savanna_zone_8017D8F8(void)
{
    return 0;
}

INCLUDE_ASM("rooms/nonmatchings/neo_ark_savanna_zone/neo_ark_savanna_zone", func_neo_ark_savanna_zone_8017D900);

INCLUDE_ASM("rooms/nonmatchings/neo_ark_savanna_zone/neo_ark_savanna_zone", func_neo_ark_savanna_zone_8017D908);

void func_neo_ark_savanna_zone_8017D94C(void)
{
}

INCLUDE_ASM("rooms/nonmatchings/neo_ark_savanna_zone/neo_ark_savanna_zone", func_neo_ark_savanna_zone_8017D954);

void func_neo_ark_savanna_zone_8017D9AC(Task* arg0)
{
    if (arg0->state == 0) {
        D_80115758          = 0x601DD;
        D_8011572C          = 0x601F9;
        D_80115750          = 0x60215;
        Gp_State1C->field_A = 2;
        arg0->state         = 1;
    }
}

INCLUDE_ASM("rooms/nonmatchings/neo_ark_savanna_zone/neo_ark_savanna_zone", func_neo_ark_savanna_zone_8017DA0C);
