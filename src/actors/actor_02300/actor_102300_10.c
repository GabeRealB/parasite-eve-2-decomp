#include "common.h"

#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"

#include "actors/actor_102300.h"

extern s32 D_8011572C;

/// Per-frame state of the child task `Actor02300_Fn03C04` sets up. It mirrors
/// the enemy's model flags onto its own model and drains the enemy's
/// `field_6D8` countdown; on the frame it reaches zero it spawns a
/// `Gp_SpawnEff` effect at part 7 of the enemy's coordinate array and
/// reparents the effect's task to this child. `arg0` is the spawn context
/// every state handler takes and is unused here.
void Actor02300_Fn03C50(GpEnemy* arg0, Task* task)
{
    GpEffWork*       effect;
    Task*            parent;
    Actor102300Work* work;
    s16              count;

    parent                           = task->parent;
    work                             = (Actor102300Work*)parent->work;
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
