#include "common.h"

#include "actors/actor_401300.h"

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300_3", func_actor_401300_80141758);

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300_3", func_actor_401300_801417F0);

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300_3", func_actor_401300_8014192C);

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300_3", func_actor_401300_801419B8);

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300_3", func_actor_401300_80141A60);

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300_3", func_actor_401300_80141B0C);

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300_3", func_actor_401300_80141BC8);

void func_actor_401300_80141C80(void)
{
}

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300_3", func_actor_401300_80141C88);

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300_3", func_actor_401300_80141D50);

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300_3", func_actor_401300_80141DF4);

void func_actor_401300_80141EF8(Task* task)
{
    Actor401300Work* work  = (Actor401300Work*)task->idMap;
    GpEnemy*         enemy = task->spawnArg2;

    if (enemy->field_40 != -0x3E7 && work->field_C8A == 0) {
        enemy->field_40 = -0x3E7;
    }
}
