#include "common.h"

#include "main/task.h"

#include <psyq/rand.h>

extern u8 D_80071068;

/// 5-way task dispatch table at package header + 4 (header.s).
extern TaskFuncTable5 D_80093804;
/// Stores the result of rand() after each title dispatcher tick.
extern s32 D_80094CA4;

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

void func_8009472C(Task* arg0)
{
    TaskFuncTable5 sp;

    sp         = D_80093804;
    D_80094CA4 = rand();
    sp.funcs[arg0->field_30](arg0);
}

void func_800947A8(Task* arg0)
{
    Task_CallExit(arg0);
}

INCLUDE_ASM("title/nonmatchings/title", func_800947C8);

INCLUDE_ASM("title/nonmatchings/title", func_80094A08);

INCLUDE_ASM("title/nonmatchings/title", func_80094B90);
