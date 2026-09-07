#include "common.h"

#include "actors/actor_260400.h"
#include "gameplay/1BC.h"
#include "main/task.h"

INCLUDE_ASM("actors/nonmatchings/actor_260400/actor_260400_2", ActorsShared80131f9cSub1);

void func_actor_260400_8014A630(Task* task)
{
    Actor260400Work* work = (Actor260400Work*)task->idMap;

    Gp_DestroyEnemy(task->spawnArg2, task);
    Task_Kill(work->field_4F0);
}

INCLUDE_ASM("actors/nonmatchings/actor_260400/actor_260400_2", func_actor_260400_8014A66C);

INCLUDE_ASM("actors/nonmatchings/actor_260400/actor_260400_2", func_actor_260400_8014A6F8);

INCLUDE_ASM("actors/nonmatchings/actor_260400/actor_260400_2", func_actor_260400_8014A7AC);

INCLUDE_ASM("actors/nonmatchings/actor_260400/actor_260400_2", func_actor_260400_8014A7F8);

INCLUDE_ASM("actors/nonmatchings/actor_260400/actor_260400_2", func_actor_260400_8014A888);

INCLUDE_ASM("actors/nonmatchings/actor_260400/actor_260400_2", func_actor_260400_8014A908);

INCLUDE_ASM("actors/nonmatchings/actor_260400/actor_260400_2", func_actor_260400_8014A998);

INCLUDE_ASM("actors/nonmatchings/actor_260400/actor_260400_2", func_actor_260400_8014AA28);

INCLUDE_ASM("actors/nonmatchings/actor_260400/actor_260400_2", func_actor_260400_8014AAA4);

INCLUDE_ASM("actors/nonmatchings/actor_260400/actor_260400_2", func_actor_260400_8014AB50);
