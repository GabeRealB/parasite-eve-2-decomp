#include "common.h"

#include "actors/actor_535700.h"
#include "main/task.h"
#include "main/tmd.h"

/// Task body of the second enemy's sub-model, which the enemy's spawn handler
/// makes a child of the enemy's task. On its first tick it lights the
/// sub-model with the enemy's `Actor535700SpawnWork` matrices and hangs its
/// root coordinate off part 7 of the enemy's model; after that it only marks
/// the coordinate dirty each frame so it follows that part.
void func_actor_535700_801333FC(Task* task)
{
    char                  pad[0x10];
    Task*                 parent = task->parent;
    TmdObject*            obj    = task->extra;
    GsCOORDINATE2*        coord  = obj->coords;
    GsCOORDINATE2*        sub    = &((TmdObject*)parent->extra)->coords[7];
    Actor535700SpawnWork* work   = (Actor535700SpawnWork*)parent->work;

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
