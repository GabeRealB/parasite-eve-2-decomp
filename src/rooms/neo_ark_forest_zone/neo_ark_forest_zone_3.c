#include "common.h"
#include "gameplay/3CD8.h"
extern s32 D_8011572C;
extern s32 D_80115750;
extern s32 D_80115758;

INCLUDE_ASM("rooms/nonmatchings/neo_ark_forest_zone/neo_ark_forest_zone_3", RoomsShared8017f10cSub);

void func_neo_ark_forest_zone_8017E3C0(Task* arg0)
{
    Gp_State1C->field_A = 2;
    if (arg0->state == 0) {
        D_80115758  = 0x601D9;
        D_8011572C  = 0x601F5;
        D_80115750  = 0x60211;
        arg0->state = 1;
    }
}

INCLUDE_ASM("rooms/nonmatchings/neo_ark_forest_zone/neo_ark_forest_zone_3", func_neo_ark_forest_zone_8017E420);
