#include "common.h"

#include "main/task.h"
#include "main/tmd.h"

#include "actors/actors_shared_80135a24.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"

extern s32 D_8011572C;

/// Per-frame tick of the child task `ActorsShared80135a24` spawns. It mirrors
/// the spawner's model `field_C` onto its own model and drains the spawner's
/// dwell counter `field_6D8`, which that spawn handler seeded to 0xA; on the
/// frame the counter reaches zero it spawns a `Gp_SpawnEff` effect at part 7
/// of the spawner's coordinate array and reparents the spawned effect task to
/// itself. `arg0` is the enemy context every state handler takes and is unused
/// here.
void ActorsShared8014da70(void* arg0, Task* task)
{
    GpEffWork*                effect;
    Task*                     parent;
    ActorsShared80135a24Work* work;
    s16                       count;

    parent                           = task->parent;
    work                             = (ActorsShared80135a24Work*)parent->work;
    ((TmdObject*)task->extra)->flags = (u16)((TmdObject*)parent->extra)->flags;
    if (work->field_6D8 > 0) {
        count           = (u16)work->field_6D8 - 1;
        work->field_6D8 = count;
        if (count == 0) {
            effect = Gp_SpawnEff(D_8011572C | 0x80000000,
                                 &((TmdObject*)task->parent->extra)->coords[7], 0, NULL);
            if (effect != NULL) {
                Task_Reparent(task, effect->task);
            }
        }
    }
}
