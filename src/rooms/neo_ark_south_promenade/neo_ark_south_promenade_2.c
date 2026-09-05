#include "common.h"

#include "main/task.h"

extern s32 D_8011572C;
extern s32 D_80115750;
extern s32 D_80115758;

void func_neo_ark_south_promenade_8017D670(void)
{
}

INCLUDE_ASM("rooms/nonmatchings/neo_ark_south_promenade/neo_ark_south_promenade_2", func_neo_ark_south_promenade_8017D678);

void func_neo_ark_south_promenade_8017D6D0(Task* arg0)
{
    if (arg0->state == 0) {
        D_80115758  = 0x601DE;
        D_8011572C  = 0x601FA;
        D_80115750  = 0x60216;
        arg0->state = 1;
    }
}
