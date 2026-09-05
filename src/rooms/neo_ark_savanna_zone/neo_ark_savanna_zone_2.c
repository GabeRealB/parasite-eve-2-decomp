#include "common.h"

#include "gameplay/3CD8.h"

extern s32 D_8011572C;
extern s32 D_80115750;
extern s32 D_80115758;

void func_neo_ark_savanna_zone_8017D94C(void)
{
}

INCLUDE_ASM("rooms/nonmatchings/neo_ark_savanna_zone/neo_ark_savanna_zone_2", func_neo_ark_savanna_zone_8017D954);

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
