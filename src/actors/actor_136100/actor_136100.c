#include "common.h"

#include "actors/actor_136100.h"
#include "main/task.h"

extern Task* D_actor_136100_8014078C;

INCLUDE_ASM("actors/nonmatchings/actor_136100/actor_136100", func_actor_136100_80131EC4);

INCLUDE_ASM("actors/nonmatchings/actor_136100/actor_136100", func_actor_136100_80131FBC);

INCLUDE_ASM("actors/nonmatchings/actor_136100/actor_136100", func_actor_136100_801320E0);

INCLUDE_ASM("actors/nonmatchings/actor_136100/actor_136100", func_actor_136100_80132284);

INCLUDE_RODATA("actors/nonmatchings/actor_136100/actor_136100", D_actor_136100_80131E20);

INCLUDE_ASM("actors/nonmatchings/actor_136100/actor_136100", func_actor_136100_801323F8);

INCLUDE_ASM("actors/nonmatchings/actor_136100/actor_136100", func_actor_136100_80132748);

INCLUDE_ASM("actors/nonmatchings/actor_136100/actor_136100", func_actor_136100_80132BC0);

INCLUDE_ASM("actors/nonmatchings/actor_136100/actor_136100", func_actor_136100_80132E78);

INCLUDE_ASM("actors/nonmatchings/actor_136100/actor_136100", func_actor_136100_80133238);

INCLUDE_ASM("actors/nonmatchings/actor_136100/actor_136100", func_actor_136100_80133558);

INCLUDE_ASM("actors/nonmatchings/actor_136100/actor_136100", func_actor_136100_80133690);

INCLUDE_ASM("actors/nonmatchings/actor_136100/actor_136100", func_actor_136100_8013379C);

INCLUDE_ASM("actors/nonmatchings/actor_136100/actor_136100", func_actor_136100_80133904);

INCLUDE_ASM("actors/nonmatchings/actor_136100/actor_136100", func_actor_136100_80133A88);

INCLUDE_ASM("actors/nonmatchings/actor_136100/actor_136100", func_actor_136100_80133BC8);

INCLUDE_ASM("actors/nonmatchings/actor_136100/actor_136100", func_actor_136100_801344AC);

INCLUDE_ASM("actors/nonmatchings/actor_136100/actor_136100", func_actor_136100_80134588);

INCLUDE_ASM("actors/nonmatchings/actor_136100/actor_136100", func_actor_136100_8013467C);

INCLUDE_ASM("actors/nonmatchings/actor_136100/actor_136100", func_actor_136100_801346EC);

INCLUDE_ASM("actors/nonmatchings/actor_136100/actor_136100", func_actor_136100_80134720);

INCLUDE_ASM("actors/nonmatchings/actor_136100/actor_136100", func_actor_136100_801347B8);

void func_actor_136100_80134838(s16 arg0)
{
    Actor136100Work* work = (Actor136100Work*)D_actor_136100_8014078C->idMap;

    work->field_4C4 = arg0;
    work->field_4C6 = 0;
}

void func_actor_136100_80134858(s16 arg0)
{
    Actor136100Work* work = (Actor136100Work*)D_actor_136100_8014078C->idMap;

    work->field_4CC = arg0;
    work->field_4CE = 0;
}

void func_actor_136100_80134878(s16 arg0)
{
    Actor136100Work* work = (Actor136100Work*)D_actor_136100_8014078C->idMap;

    work->field_4D4 = arg0;
    work->field_4D6 = 0;
}
