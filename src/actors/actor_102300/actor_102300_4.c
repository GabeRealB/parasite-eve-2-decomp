#include "common.h"

#include "actors/actors_shared_80135a24.h"
#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"
#include "main/task.h"
#include "main/tmd.h"

/// The enemy's three state handlers - spawn/setup, per-frame tick and
/// teardown - dispatched through by state.
extern GpEnemyTaskFuncTable3 D_actor_102300_80131E8C;

extern s32 D_8011572C;

INCLUDE_RODATA("actors/nonmatchings/actor_102300/actor_102300_4", D_actor_102300_80131E8C);

/// Per-frame tick that keeps this task's model object in step with its
/// spawner's and drains the spawner work block's `field_6D8` dwell counter.
/// `field_C` of the task's model object is refreshed from the parent's every
/// tick. The counter is decremented only while it is still positive; on the
/// tick it reaches zero the task spawns effect `D_8011572C` on part 7 of the
/// spawner's model coordinates and reparents itself onto the spawned effect
/// task, so it dies with the effect instead of with the spawner. `arg0` is the
/// enemy context every state handler takes and is unused here.
void func_actor_102300_80135A70(void* arg0, Task* task)
{
    GpEffWork*                effect;
    Task*                     parent;
    ActorsShared80135a24Work* work;
    s16                       count;

    parent                             = task->parent;
    work                               = (ActorsShared80135a24Work*)parent->idMap;
    ((TmdObject*)task->extra)->field_C = (u16)((TmdObject*)parent->extra)->field_C;
    if (work->field_6D8 > 0) {
        count           = (u16)work->field_6D8 - 1;
        work->field_6D8 = count;
        if (count == 0) {
            effect = Gp_SpawnEff(D_8011572C | 0x80000000,
                                 &((TmdObject*)task->parent->extra)->field_8[7], 0, NULL);
            if (effect != NULL) {
                Task_Reparent(task, effect->field_0);
            }
        }
    }
}

void func_actor_102300_80135B08(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_102300_80131E8C;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}
