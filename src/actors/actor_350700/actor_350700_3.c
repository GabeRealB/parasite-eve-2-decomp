#include "common.h"
#include "actors/actor_350700.h"
#include "main/task.h"
#include "main/tmd.h"
#include "gameplay/1BC.h"

/// Global freeze byte in the main executable; the state dispatchers run
/// nothing while it is non-zero.
extern u8 D_801153F4;

/// Spawn, tick and exit handlers of the parent actor.
extern TaskFuncTable3 D_actor_350700_80161E5C;

void func_actor_350700_80163348(void)
{
}

/// Per-frame dispatcher of the parent actor: runs its spawn, tick or exit
/// state from `D_actor_350700_80161E5C`, skipping the frame while the global
/// freeze byte is set.
void func_actor_350700_80163350(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_350700_80161E5C;
    if (D_801153F4 == 0) {
        sp.funcs[task->state](task);
    }
}

void func_actor_350700_801633BC(Task* arg0)
{
    Gp_EnemyTaskExit(arg0);
}
