#include "common.h"

#include "main/task.h"

extern u8 D_80071068;

INCLUDE_ASM("title/nonmatchings/title", func_8009389C);

INCLUDE_ASM("title/nonmatchings/title", func_800939C4);

INCLUDE_ASM("title/nonmatchings/title", func_80093ABC);

INCLUDE_ASM("title/nonmatchings/title", func_8009407C);

void func_8009470C(Task* arg0)
{
    s32* p = &arg0->field_30;

    D_80071068 = 1;
    (*p)++;
}

INCLUDE_ASM("title/nonmatchings/title", func_8009472C);

void func_800947A8(Task* arg0)
{
    Task_CallExit(arg0);
}

INCLUDE_ASM("title/nonmatchings/title", func_800947C8);

INCLUDE_ASM("title/nonmatchings/title", func_80094A08);

INCLUDE_ASM("title/nonmatchings/title", func_80094B90);
