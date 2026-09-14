#include "common.h"

#include "actors/actor_401800.h"

INCLUDE_ASM("actors/nonmatchings/actor_401800/actor_401800_2", func_actor_401800_8013DF80);

void func_actor_401800_8013E0A0(Task* task)
{
    Actor401800Work* work;
    GpEnemy*         enemy;

    work  = (Actor401800Work*)task->idMap;
    enemy = (GpEnemy*)task->spawnArg2;
    if (work != NULL) {
        if (work->field_C14 != NULL) {
            Task_Kill(work->field_C14);
        }
        if (work->field_C18 != NULL) {
            Task_Kill(work->field_C18);
        }
        Gp_UnlinkObj(&work->field_B48);
        Gp_UnlinkObj(&work->field_8C8);
        Gp_UnlinkObj(&work->field_A08);
        enemy->field_54 = 0;
    }
    Gp_DestroyEnemy(enemy, task);
}

INCLUDE_ASM("actors/nonmatchings/actor_401800/actor_401800_2", func_actor_401800_8013E138);

INCLUDE_ASM("actors/nonmatchings/actor_401800/actor_401800_2", func_actor_401800_8013E194);

INCLUDE_ASM("actors/nonmatchings/actor_401800/actor_401800_2", func_actor_401800_8013E23C);

INCLUDE_ASM("actors/nonmatchings/actor_401800/actor_401800_2", func_actor_401800_8013E2E8);

INCLUDE_ASM("actors/nonmatchings/actor_401800/actor_401800_2", func_actor_401800_8013E394);

INCLUDE_ASM("actors/nonmatchings/actor_401800/actor_401800_2", func_actor_401800_8013E44C);

INCLUDE_ASM("actors/nonmatchings/actor_401800/actor_401800_2", func_actor_401800_8013E4F0);

INCLUDE_ASM("actors/nonmatchings/actor_401800/actor_401800_2", func_actor_401800_8013E5A4);
