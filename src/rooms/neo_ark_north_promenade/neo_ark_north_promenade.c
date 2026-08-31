#include "common.h"
#include "main/task.h"
extern s32 D_80115720;
extern s32 D_80115728;
extern s32 D_8011572C;
extern s32 D_8011573C;
extern s32 D_80115744;
extern s32 D_80115750;
extern s32 D_80115758;

INCLUDE_ASM("rooms/nonmatchings/neo_ark_north_promenade/neo_ark_north_promenade", func_neo_ark_north_promenade_8017D5D8);

s32 func_neo_ark_north_promenade_8017D66C(void)
{
    return 0;
}

s32 func_neo_ark_north_promenade_8017D674(void)
{
    return 0;
}

INCLUDE_ASM("rooms/nonmatchings/neo_ark_north_promenade/neo_ark_north_promenade", func_neo_ark_north_promenade_8017D67C);

void func_neo_ark_north_promenade_8017D6C0(void)
{
}

INCLUDE_RODATA("rooms/nonmatchings/neo_ark_north_promenade/neo_ark_north_promenade", D_neo_ark_north_promenade_8017D5C0);

INCLUDE_ASM("rooms/nonmatchings/neo_ark_north_promenade/neo_ark_north_promenade", func_neo_ark_north_promenade_8017D6C8);

void func_neo_ark_north_promenade_8017D720(Task* arg0)
{
    if (arg0->state == 0) {
        D_80115728  = 0x6024F;
        D_80115744  = 0x6025B;
        D_8011573C  = 0x60266;
        D_80115720  = 0x60272;
        D_80115758  = 0x601D8;
        D_8011572C  = 0x601F4;
        D_80115750  = 0x60210;
        arg0->state = 1;
    }
}

INCLUDE_ASM("rooms/nonmatchings/neo_ark_north_promenade/neo_ark_north_promenade", func_neo_ark_north_promenade_8017D7B0);

INCLUDE_ASM("rooms/nonmatchings/neo_ark_north_promenade/neo_ark_north_promenade", func_neo_ark_north_promenade_8017DA7C);
