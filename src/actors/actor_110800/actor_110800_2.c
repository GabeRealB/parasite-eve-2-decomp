#include "common.h"
#include "gameplay/3FB8.h"
#include "main/task.h"
#include "main/tmd.h"

extern Task*        D_actor_110800_80139F18;
extern GpActorWork* D_actor_110800_80139F14;

void func_actor_110800_801322FC(GpActorWork* arg0)
{
    GsCOORDINATE2* parent;
    GsCOORDINATE2* coord;

    parent            = D_actor_110800_80139F14->extra->field_8;
    coord             = arg0->extra->field_8;
    coord->flg        = 0;
    coord->coord.t[0] = -50;
    coord->sub        = parent + 8;
}

void func_actor_110800_8013232C(Task* arg0)
{
    Task_Kill(D_actor_110800_80139F18);
    Gp_DestroyEnemy(arg0->spawnArg2, arg0);
}

INCLUDE_ASM("actors/nonmatchings/actor_110800/actor_110800_2", func_actor_110800_80132368);
