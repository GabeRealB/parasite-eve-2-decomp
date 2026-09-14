#include "common.h"

#include "actors/actor_403000.h"
#include "gameplay/1BC.h"
#include "main/task.h"

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_80132348);

INCLUDE_RODATA("actors/nonmatchings/actor_403000/actor_403000", D_actor_403000_80131E20);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_801324EC);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_801327B0);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_80132AE0);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_801330D4);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_801332E8);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_80133444);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_801336B4);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_801337E0);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_80133AF8);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_80133FC0);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_80134204);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_801343B8);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_80134910);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_80134E00);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_80134F44);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_80135F08);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_8013603C);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_801365D0);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_80136B14);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_80136D68);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_80137084);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_801377C8);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_801384E8);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_801386E8);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_80138DB0);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_801399A0);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_80139AE0);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_8013A08C);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_8013A678);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_8013ACBC);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_8013B238);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_8013B74C);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_8013BDE0);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_8013C050);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_8013C2D4);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_8013C864);

INCLUDE_RODATA("actors/nonmatchings/actor_403000/actor_403000", ActorsShared80135df4Table);

void func_actor_403000_8013D260(void)
{
}

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_8013D268);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_8013D324);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_8013D364);

s32 func_actor_403000_8013D464(Task* task, s32 arg1, Actor403000Msg* msg)
{
    Actor403000Work* work = (Actor403000Work*)task->idMap;

    work->field_AC6 = msg->field_4;
    work->field_0   = 1;
    work->field_2   = -1;
    return 0;
}

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_8013D48C);

void func_actor_403000_8013D4F4(Task* task)
{
    Actor403000Work* work  = (Actor403000Work*)task->idMap;
    GpEnemy*         enemy = (GpEnemy*)task->spawnArg2;

    if (work != NULL) {
        Gp_UnlinkObj(&work->objB50.obj);
        Gp_UnlinkObj(&work->objBE8.obj);
        Gp_UnlinkObj(&work->objC80.obj);
        Gp_UnlinkObj(&work->objD18.obj);
        enemy->field_54 = 0;
    }
    Gp_DestroyEnemy(enemy, task);
}

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_8013D564);
