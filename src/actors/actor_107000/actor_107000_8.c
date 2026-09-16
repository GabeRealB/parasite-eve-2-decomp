#include "common.h"

#include "actors/actor_107000.h"
#include "gameplay/1BC.h"
#include "main/task.h"

/// The enemy's four main-body handlers, dispatched through by state.
extern GpEnemyTaskFuncTable4 D_actor_107000_80131E5C;

INCLUDE_ASM("actors/nonmatchings/actor_107000/actor_107000_8", func_actor_107000_801378D8);

void func_actor_107000_80137C8C(Task* arg0)
{
    GpEnemyTaskFuncTable4 sp;

    sp = D_actor_107000_80131E5C;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}
