#include "common.h"
#include "main/task.h"
extern s32 D_8011572C;
extern s32 D_80115730;
extern s32 D_80115734;
extern s32 D_80115738;
extern s32 D_8011574C;
extern s32 D_80115750;
extern s32 D_80115754;
extern s32 D_80115758;

void func_neo_ark_pavilion_8017FC10(Task* arg0)
{
    if (arg0->state == 0) {
        D_80115758  = 0x601DA;
        D_8011572C  = 0x601F6;
        D_80115750  = 0x60212;
        D_80115734  = 0x60227;
        D_80115730  = 0x60232;
        D_80115754  = 0x6023D;
        D_8011574C  = 0x60176;
        D_80115738  = 0x60177;
        arg0->state = 1;
    }
}

INCLUDE_ASM("rooms/nonmatchings/neo_ark_pavilion/neo_ark_pavilion_3", func_neo_ark_pavilion_8017FCB0);
