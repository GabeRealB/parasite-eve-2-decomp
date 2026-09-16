#include "common.h"
#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"
#include "gameplay/1BC.h"
extern GpEnemyTaskFuncTable3 D_actor_204600_80149E24;
INCLUDE_ASM("actors/nonmatchings/actor_204600/actor_204600_2", func_actor_204600_8014BD74);

void func_actor_204600_8014C2C4(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_204600_80149E24;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}

INCLUDE_ASM("actors/nonmatchings/actor_204600/actor_204600_2", func_actor_204600_8014C320);

INCLUDE_ASM("actors/nonmatchings/actor_204600/actor_204600_2", func_actor_204600_8014C438);
