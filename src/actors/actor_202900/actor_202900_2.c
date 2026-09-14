#include "common.h"
#include "gameplay/3FB8.h"
#include "main/task.h"

extern Task*        D_actor_202900_80156E5C;
extern GpActorWork* D_actor_202900_80156E58;

void func_actor_202900_8014A088(GpActorWork* arg0)
{
    GsCOORDINATE2* parent;
    GsCOORDINATE2* coord;
    TmdObject*     extra;

    extra          = arg0->extra;
    parent         = D_actor_202900_80156E58->extra->field_8;
    coord          = extra->field_8;
    coord->flg     = 0;
    extra->field_C = 0;
    coord->sub     = parent + 4;
}

INCLUDE_ASM("actors/nonmatchings/actor_202900/actor_202900_2", ActorsShared80131f9cSub1);

void func_actor_202900_8014A158(Task* arg0)
{
    Task_Kill(D_actor_202900_80156E5C);
    Gp_DestroyEnemy(arg0->spawnArg2, arg0);
}

INCLUDE_ASM("actors/nonmatchings/actor_202900/actor_202900_2", func_actor_202900_8014A194);

INCLUDE_ASM("actors/nonmatchings/actor_202900/actor_202900_2", func_actor_202900_8014A208);

INCLUDE_ASM("actors/nonmatchings/actor_202900/actor_202900_2", func_actor_202900_8014A260);

INCLUDE_ASM("actors/nonmatchings/actor_202900/actor_202900_2", func_actor_202900_8014A304);

INCLUDE_ASM("actors/nonmatchings/actor_202900/actor_202900_2", func_actor_202900_8014A394);

INCLUDE_ASM("actors/nonmatchings/actor_202900/actor_202900_2", func_actor_202900_8014A3E0);

INCLUDE_ASM("actors/nonmatchings/actor_202900/actor_202900_2", func_actor_202900_8014A440);
