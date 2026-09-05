#include "common.h"
extern s32 D_dryfield_r08_80180C24;

INCLUDE_ASM("rooms/nonmatchings/dryfield_r08/dryfield_r08_2", func_dryfield_r08_8017EB68);

void func_dryfield_r08_8017F334(s32 arg0)
{
    D_dryfield_r08_80180C24 = arg0;
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_r08/dryfield_r08_2", func_dryfield_r08_8017F340);

INCLUDE_ASM("rooms/nonmatchings/dryfield_r08/dryfield_r08_2", func_dryfield_r08_8017F3B8);
