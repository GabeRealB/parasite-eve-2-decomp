#include "common.h"

#include "actors/actor_401000.h"

INCLUDE_ASM("actors/nonmatchings/actor_401000/actor_401000_2", func_actor_401000_8013D958);

void func_actor_401000_8013DA78(Task* task)
{
    Actor401000Work* work;
    GpEnemy*         enemy;

    work  = (Actor401000Work*)task->idMap;
    enemy = (GpEnemy*)task->spawnArg2;
    if (work != NULL) {
        if (work->field_C1C != NULL) {
            Task_Kill(work->field_C1C);
        }
        if (work->field_C20 != NULL) {
            Task_Kill(work->field_C20);
        }
        Gp_UnlinkObj(&work->field_B50);
        Gp_UnlinkObj(&work->field_8D0);
        Gp_UnlinkObj(&work->field_A10);
        enemy->field_54 = 0;
    }
    Gp_DestroyEnemy(enemy, task);
}

INCLUDE_ASM("actors/nonmatchings/actor_401000/actor_401000_2", func_actor_401000_8013DB10);

INCLUDE_ASM("actors/nonmatchings/actor_401000/actor_401000_2", func_actor_401000_8013DB6C);

INCLUDE_ASM("actors/nonmatchings/actor_401000/actor_401000_2", func_actor_401000_8013DC14);

INCLUDE_ASM("actors/nonmatchings/actor_401000/actor_401000_2", func_actor_401000_8013DCC0);

INCLUDE_ASM("actors/nonmatchings/actor_401000/actor_401000_2", func_actor_401000_8013DD6C);

INCLUDE_ASM("actors/nonmatchings/actor_401000/actor_401000_2", func_actor_401000_8013DE24);

INCLUDE_ASM("actors/nonmatchings/actor_401000/actor_401000_2", func_actor_401000_8013DEC8);

INCLUDE_ASM("actors/nonmatchings/actor_401000/actor_401000_2", func_actor_401000_8013DF6C);
