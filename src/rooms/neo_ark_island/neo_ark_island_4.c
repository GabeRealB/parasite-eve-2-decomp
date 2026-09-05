#include "common.h"
#include "main/task.h"
extern s32 D_8011572C;
extern s32 D_80115738;
extern s32 D_8011574C;
extern s32 D_80115750;
extern s32 D_80115758;

void func_neo_ark_island_8017FB2C(Task* arg0)
{
    if (arg0->state == 0) {
        D_80115758  = 0x601DB;
        D_8011572C  = 0x601F7;
        D_80115750  = 0x60213;
        D_8011574C  = 0x60178;
        D_80115738  = 0x60179;
        arg0->state = 1;
    }
}
