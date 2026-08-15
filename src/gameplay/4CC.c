#include "common.h"

#include "gameplay/268.h"
#include "gameplay/4CC.h"
#include "main/task.h"

INCLUDE_ASM("gameplay/nonmatchings/4CC", func_800BC634);

INCLUDE_ASM("gameplay/nonmatchings/4CC", func_800BCC44);

INCLUDE_ASM("gameplay/nonmatchings/4CC", func_800BCEA4);

INCLUDE_ASM("gameplay/nonmatchings/4CC", func_800BD2FC);

INCLUDE_ASM("gameplay/nonmatchings/4CC", func_800BD6DC);

INCLUDE_ASM("gameplay/nonmatchings/4CC", func_800BDAA8);

INCLUDE_ASM("gameplay/nonmatchings/4CC", func_800BDC80);

INCLUDE_ASM("gameplay/nonmatchings/4CC", func_800BDDC4);

INCLUDE_ASM("gameplay/nonmatchings/4CC", func_800BDF6C);

INCLUDE_ASM("gameplay/nonmatchings/4CC", func_800BE808);

INCLUDE_ASM("gameplay/nonmatchings/4CC", func_800BEBE4);

INCLUDE_ASM("gameplay/nonmatchings/4CC", func_800BF2C8);

INCLUDE_ASM("gameplay/nonmatchings/4CC", func_800BF334);

INCLUDE_ASM("gameplay/nonmatchings/4CC", func_800BF398);

INCLUDE_ASM("gameplay/nonmatchings/4CC", func_800BF464);

INCLUDE_ASM("gameplay/nonmatchings/4CC", func_800BF4FC);

s32 func_800BF5CC(Task* arg0, s32 arg1, GpItemObj2* arg2)
{
    s32         flag;
    GpItemObj8* obj;

    obj              = arg0->spawnArg2;
    flag             = 1;
    arg0->flags      = flag;
    arg0->extraState = (s32)arg2;
    if (func_800BB470(obj->field_8) == 2) {
        arg2->field_2 = flag;
    }
    return 0;
}

INCLUDE_ASM("gameplay/nonmatchings/4CC", func_800BF624);

INCLUDE_ASM("gameplay/nonmatchings/4CC", func_800BF738);
