#include "common.h"

#include "kyle/kyle_800102.h"

INCLUDE_ASM("kyle/nonmatchings/kyle_800102/kyle_800102", func_kyle_800102_80167A84);

INCLUDE_ASM("kyle/nonmatchings/kyle_800102/kyle_800102", func_kyle_800102_80167DE0);

INCLUDE_ASM("kyle/nonmatchings/kyle_800102/kyle_800102", func_kyle_800102_80168244);

void Gp_UnlinkObj(Kyle800102Obj* node);
void Task_Kill(Kyle800102Task* task);

void func_kyle_800102_80168270(Kyle800102Task* arg0)
{
    Kyle800102Work* work = arg0->field_1C;

    Gp_UnlinkObj(&work->field_0);
    Gp_UnlinkObj(&work->field_20);
    Task_Kill(arg0);
}

INCLUDE_RODATA("kyle/nonmatchings/kyle_800102/kyle_800102", D_kyle_800102_80167A70);

INCLUDE_ASM("kyle/nonmatchings/kyle_800102/kyle_800102", func_kyle_800102_801682B4);
