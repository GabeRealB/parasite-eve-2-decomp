#include "common.h"

#include "actors/actor_451100.h"
#include "main/tmd.h"

/// Per-frame handler of the sub-model task, entry 1 of
/// `D_actor_451100_8014E6E4`, reached with the sub-model's own `TmdObject` in
/// `Task::extra` and the actor holding it as `Task::parent`. On its first tick
/// it lights the sub-model with the parent's two leading work matrices and
/// hangs its root coordinate off coordinate 8 of the parent's model; after that
/// it only marks the coordinate dirty each frame so it follows that part.
void func_actor_451100_801330B0(Task* task)
{
    char           pad[0x10];
    Task*          parent = task->parent;
    TmdObject*     obj    = task->extra;
    GsCOORDINATE2* coord  = obj->coords;
    GsCOORDINATE2* sub    = &((TmdObject*)parent->extra)->coords[8];
    MATRIX*        work   = (MATRIX*)parent->work;

    switch (task->state) {
        case 0:
            coord->flg    = 0;
            obj->lightMtx = work;
            obj->colorMtx = work + 1;
            coord->sub    = sub;
            task->state++;
            break;
        case 1:
            coord->flg = 0;
            break;
    }
}
