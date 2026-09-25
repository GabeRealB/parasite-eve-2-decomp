#include "common.h"

#include "main/task.h"
#include "main/tmd.h"

#include "actors/actor_102300.h"

/// Spawn state of the child task driven by `Actor02300_Fn03BA8`: parents the
/// child's root coordinate to part 7 of the enemy's model, points the child's
/// model at the enemy's light and colour matrices and seeds the enemy's
/// `field_6D8` countdown the child's tick drains, then advances to state 1.
/// `arg0` is the spawn context every state handler takes and is unused here.
void Actor02300_Fn03C04(GpEnemy* arg0, Task* task)
{
    Task*            parent;
    TmdObject*       obj;
    Actor102300Work* work;
    GsCOORDINATE2*   coord;
    GsCOORDINATE2*   parentCoords;

    parent       = task->parent;
    obj          = (TmdObject*)task->extra;
    parentCoords = ((TmdObject*)parent->extra)->coords;
    coord        = obj->coords;
    work         = (Actor102300Work*)parent->work;

    coord->flg      = 0;
    coord->sub      = &parentCoords[7];
    obj->lightMtx   = &work->field_45C;
    obj->colorMtx   = &work->field_43C;
    obj->flags      = 0;
    task->state     = 1;
    work->field_6D8 = 0xA;
}
