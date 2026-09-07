#include "common.h"

#include "actors/actor_136100.h"
#include "main/task.h"

extern Task* D_actor_136100_8014078C;

INCLUDE_ASM("actors/nonmatchings/actor_136100/actor_136100_2", func_actor_136100_801347B8);

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
