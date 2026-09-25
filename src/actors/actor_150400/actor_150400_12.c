#include "common.h"

#include "main/task.h"
#include "main/tmd.h"

#include "actors/actor_150400.h"

/// Per-frame callback of the actor's sub-model task, which the spawn handler
/// parents under the actor's own task. On the first frame it draws the
/// sub-model under the actor's `light` / `color` matrices and parents its root
/// coordinate to part 7 of the actor's model, then advances to state 1; from
/// then on it only marks the coordinate for recomputation each frame.
void func_actor_150400_801328BC(Task* task)
{
    char             pad[0x10];
    Task*            parent = task->parent;
    TmdObject*       obj    = task->extra;
    GsCOORDINATE2*   coord  = obj->coords;
    GsCOORDINATE2*   sub    = &((TmdObject*)parent->extra)->coords[7];
    Actor150400Work* work   = (Actor150400Work*)parent->work;

    switch (task->state) {
        case 0:
            coord->flg    = 0;
            obj->lightMtx = &work->light;
            obj->colorMtx = &work->color;
            coord->sub    = sub;
            task->state++;
            break;
        case 1:
            coord->flg = 0;
            break;
    }
}
