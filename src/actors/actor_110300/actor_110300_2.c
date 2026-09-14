#include "common.h"
#include "gameplay/3FB8.h"
#include "main/task.h"
#include "main/tmd.h"

extern Task*        D_actor_110300_8013A0A8;
extern GpActorWork* D_actor_110300_8013A0A4;

void func_actor_110300_80131FF8(GpActorWork* arg0)
{
    GsCOORDINATE2* parent;
    GsCOORDINATE2* coord;

    parent     = D_actor_110300_8013A0A4->extra->field_8;
    coord      = arg0->extra->field_8;
    coord->flg = 0;
    coord->sub = parent + 8;
}

INCLUDE_ASM("actors/nonmatchings/actor_110300/actor_110300_2", ActorsShared80131f9cSub1);

void func_actor_110300_80132088(Task* arg0)
{
    Task_Kill(D_actor_110300_8013A0A8);
    Gp_DestroyEnemy(arg0->spawnArg2, arg0);
}

INCLUDE_ASM("actors/nonmatchings/actor_110300/actor_110300_2", func_actor_110300_801320C4);
