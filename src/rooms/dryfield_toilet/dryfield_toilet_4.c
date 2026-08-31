#include "common.h"
#include "main/task.h"
extern s32 D_80115730;
extern s32 D_80115734;
extern s32 D_80115754;

INCLUDE_ASM("rooms/nonmatchings/dryfield_toilet/dryfield_toilet_4", func_dryfield_toilet_8017DCF0);

INCLUDE_ASM("rooms/nonmatchings/dryfield_toilet/dryfield_toilet_4", func_dryfield_toilet_8017DEF4);

void func_dryfield_toilet_8017E64C(Task* arg0)
{
    if (arg0->state == 0) {
        D_80115734  = 0x6028A;
        D_80115730  = 0x6028B;
        D_80115754  = 0x6028C;
        arg0->state = 1;
    }
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_toilet/dryfield_toilet_4", func_dryfield_toilet_8017E69C);

INCLUDE_ASM("rooms/nonmatchings/dryfield_toilet/dryfield_toilet_4", func_dryfield_toilet_8017EBF4);
