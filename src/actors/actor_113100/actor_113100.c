#include "common.h"

#include "actors/actor_113100.h"

#include "main/task.h"

#include "gameplay/1BC.h"

INCLUDE_ASM("actors/nonmatchings/actor_113100/actor_113100", func_actor_113100_80131E58);

INCLUDE_ASM("actors/nonmatchings/actor_113100/actor_113100", func_actor_113100_80132104);

INCLUDE_ASM("actors/nonmatchings/actor_113100/actor_113100", func_actor_113100_801324DC);

INCLUDE_ASM("actors/nonmatchings/actor_113100/actor_113100", func_actor_113100_8013264C);

INCLUDE_ASM("actors/nonmatchings/actor_113100/actor_113100", func_actor_113100_80132790);

INCLUDE_ASM("actors/nonmatchings/actor_113100/actor_113100", func_actor_113100_801328EC);

INCLUDE_RODATA("actors/nonmatchings/actor_113100/actor_113100", D_actor_113100_80131E20);

INCLUDE_ASM("actors/nonmatchings/actor_113100/actor_113100", func_actor_113100_80132AD8);

INCLUDE_ASM("actors/nonmatchings/actor_113100/actor_113100", func_actor_113100_80132B30);

INCLUDE_ASM("actors/nonmatchings/actor_113100/actor_113100", func_actor_113100_80132BDC);

INCLUDE_ASM("actors/nonmatchings/actor_113100/actor_113100", func_actor_113100_80132C9C);

INCLUDE_ASM("actors/nonmatchings/actor_113100/actor_113100", func_actor_113100_80132CF4);

INCLUDE_ASM("actors/nonmatchings/actor_113100/actor_113100", func_actor_113100_80132E00);

INCLUDE_ASM("actors/nonmatchings/actor_113100/actor_113100", func_actor_113100_80132E98);

void func_actor_113100_80132EF0(Task* arg0)
{
    Gp_UnlinkObj(&((Actor113100Work*)arg0->idMap)->obj);
    Gp_EnemyTaskExit(arg0);
}
