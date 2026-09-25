#include "common.h"

#include "main/tmd.h"

#include "actors/actor_450800.h"

/// Handler of the enemy's sub-model task, the second entry of
/// `D_actor_450800_801539DC`, which the enemy's spawn handler reparents under
/// the enemy's own task. On its first tick it lights the sub-model with the
/// enemy's `Actor450800SpawnWork` matrices and hangs its root coordinate off
/// part 7 of the enemy's model; after that it only marks the coordinate dirty
/// each frame so it follows that part.
void func_actor_450800_80133740(Task* task)
{
    char                  pad[0x10];
    Task*                 parent = task->parent;
    TmdObject*            obj    = task->extra;
    GsCOORDINATE2*        coord  = obj->coords;
    GsCOORDINATE2*        sub    = &((TmdObject*)parent->extra)->coords[7];
    Actor450800SpawnWork* work   = (Actor450800SpawnWork*)parent->work;

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
