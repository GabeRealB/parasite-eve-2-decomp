#include "common.h"

#include "actors/actor_361100.h"

#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1BC.h"

extern Task* D_actor_361100_80171BE0;

extern TaskDesc D_actor_361100_80165C58;

void func_actor_361100_8016299C(void)
{
    D_actor_361100_80171BE0 = Task_SpawnFromTable(&D_actor_361100_80165C58, 0, 0, 0);
}

INCLUDE_ASM("actors/nonmatchings/actor_361100/actor_361100_2", func_actor_361100_801629D0);

void func_actor_361100_80162A24(s32 arg0)
{
    Task_SpawnFromTable(&D_actor_361100_80165C58, 1, arg0, 0);
}

INCLUDE_ASM("actors/nonmatchings/actor_361100/actor_361100_2", func_actor_361100_80162A54);

INCLUDE_ASM("actors/nonmatchings/actor_361100/actor_361100_2", func_actor_361100_80162AEC);

void func_actor_361100_80162B0C(void)
{
    D_actor_361100_80171BE0 = 0;
}

INCLUDE_ASM("actors/nonmatchings/actor_361100/actor_361100_2", func_actor_361100_80162B18);

INCLUDE_ASM("actors/nonmatchings/actor_361100/actor_361100_2", func_actor_361100_80162CBC);

INCLUDE_ASM("actors/nonmatchings/actor_361100/actor_361100_2", func_actor_361100_80162D28);

void func_actor_361100_80162DE4(Task* arg0)
{
    Gp_EnemyTaskExit(arg0);
}

void func_actor_361100_80162E04(Task* arg0)
{
    TmdObject*       ext;
    Actor361100Work* work;

    work          = (Actor361100Work*)arg0->idMap;
    ext           = arg0->extra;
    ext->field_1C = &work->light;
    ext->field_20 = &work->color;
}

INCLUDE_ASM("actors/nonmatchings/actor_361100/actor_361100_2", func_actor_361100_80162E20);

INCLUDE_ASM("actors/nonmatchings/actor_361100/actor_361100_2", func_actor_361100_80162F58);

INCLUDE_ASM("actors/nonmatchings/actor_361100/actor_361100_2", func_actor_361100_80162FF4);

INCLUDE_ASM("actors/nonmatchings/actor_361100/actor_361100_2", func_actor_361100_801630D4);

INCLUDE_ASM("actors/nonmatchings/actor_361100/actor_361100_2", func_actor_361100_801631C4);

INCLUDE_ASM("actors/nonmatchings/actor_361100/actor_361100_2", func_actor_361100_801633A4);

INCLUDE_ASM("actors/nonmatchings/actor_361100/actor_361100_2", func_actor_361100_80163410);

void func_actor_361100_80163494(Task* arg0)
{
    Gp_EnemyTaskExit(arg0);
}

void func_actor_361100_801634B4(Task* arg0)
{
    TmdObject*       ext;
    Actor361100Work* work;

    ext           = arg0->extra;
    work          = (Actor361100Work*)arg0->idMap;
    ext->field_1C = &work->light;
    ext->field_20 = &work->color;
}

INCLUDE_ASM("actors/nonmatchings/actor_361100/actor_361100_2", func_actor_361100_801634D0);

INCLUDE_ASM("actors/nonmatchings/actor_361100/actor_361100_2", func_actor_361100_801635F4);

INCLUDE_ASM("actors/nonmatchings/actor_361100/actor_361100_2", func_actor_361100_80163670);

INCLUDE_ASM("actors/nonmatchings/actor_361100/actor_361100_2", func_actor_361100_80163750);
