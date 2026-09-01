#include "common.h"

#include "actors/actor_350700.h"

#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1BC.h"

INCLUDE_ASM("actors/nonmatchings/actor_350700/actor_350700", func_actor_350700_80161E88);

INCLUDE_ASM("actors/nonmatchings/actor_350700/actor_350700", func_actor_350700_80162070);

INCLUDE_ASM("actors/nonmatchings/actor_350700/actor_350700", func_actor_350700_801621B4);

INCLUDE_RODATA("actors/nonmatchings/actor_350700/actor_350700", D_actor_350700_80161E20);

INCLUDE_ASM("actors/nonmatchings/actor_350700/actor_350700", func_actor_350700_80162398);

INCLUDE_ASM("actors/nonmatchings/actor_350700/actor_350700", func_actor_350700_80162404);

void func_actor_350700_80162494(Task* arg0)
{
    Gp_EnemyTaskExit(arg0);
}

void func_actor_350700_801624B4(Task* arg0)
{
    TmdObject*       ext;
    Actor350700Work* work;

    ext           = arg0->extra;
    work          = (Actor350700Work*)arg0->idMap;
    ext->field_1C = &work->light;
    ext->field_20 = &work->color;
}

void func_actor_350700_801624D0(void)
{
}

INCLUDE_ASM("actors/nonmatchings/actor_350700/actor_350700", func_actor_350700_801624D8);

INCLUDE_ASM("actors/nonmatchings/actor_350700/actor_350700", func_actor_350700_80162540);

INCLUDE_ASM("actors/nonmatchings/actor_350700/actor_350700", func_actor_350700_8016261C);

INCLUDE_ASM("actors/nonmatchings/actor_350700/actor_350700", func_actor_350700_80162764);

INCLUDE_ASM("actors/nonmatchings/actor_350700/actor_350700", func_actor_350700_80162860);

INCLUDE_ASM("actors/nonmatchings/actor_350700/actor_350700", func_actor_350700_80162998);

INCLUDE_ASM("actors/nonmatchings/actor_350700/actor_350700", func_actor_350700_80162A14);

INCLUDE_ASM("actors/nonmatchings/actor_350700/actor_350700", func_actor_350700_80162AF4);

INCLUDE_ASM("actors/nonmatchings/actor_350700/actor_350700", func_actor_350700_80162B30);

INCLUDE_ASM("actors/nonmatchings/actor_350700/actor_350700", func_actor_350700_80162D5C);

INCLUDE_ASM("actors/nonmatchings/actor_350700/actor_350700", func_actor_350700_80162F7C);

INCLUDE_ASM("actors/nonmatchings/actor_350700/actor_350700", func_actor_350700_801630C0);

INCLUDE_ASM("actors/nonmatchings/actor_350700/actor_350700", func_actor_350700_80163274);

INCLUDE_ASM("actors/nonmatchings/actor_350700/actor_350700", func_actor_350700_801632CC);

void func_actor_350700_80163348(void)
{
}

INCLUDE_ASM("actors/nonmatchings/actor_350700/actor_350700", func_actor_350700_80163350);

void func_actor_350700_801633BC(Task* arg0)
{
    Gp_EnemyTaskExit(arg0);
}
