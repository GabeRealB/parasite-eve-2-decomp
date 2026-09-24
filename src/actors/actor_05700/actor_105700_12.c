#include "common.h"

#include "actors/actor_105700.h"
#include "gameplay/1BC.h"
#include "main/task.h"
#include "main/tmd.h"

/// Spawn state of the model child hung off the actor: parents the child's
/// root coordinate to part 7 of the actor's model, points the child's model
/// at the actor's light and colour matrices and advances to state 1.
void Actor05700_Fn0509C(GpEnemy* arg0, Task* task)
{
    Task*            parent;
    TmdObject*       obj;
    Actor105700Work* work;
    GsCOORDINATE2*   coord;
    GsCOORDINATE2*   parentCoords;

    parent       = task->parent;
    obj          = (TmdObject*)task->extra;
    parentCoords = ((TmdObject*)parent->extra)->coords;
    coord        = obj->coords;
    work         = (Actor105700Work*)parent->work;

    coord->flg    = 0;
    coord->sub    = &parentCoords[7];
    obj->lightMtx = &work->field_45C;
    obj->flags    = 0;
    obj->colorMtx = &work->field_43C;
    task->state   = 1;
}

/// Per-frame state of the model child: mirrors the actor's model flags onto
/// its own, and when the actor raises `field_6BA` spawns entry 2 of the
/// actor's spawn table and hands it this child's texture page and CLUT row,
/// reprocessing its model stream for both half-buffers.
void Actor05700_Fn050E0(GpEnemy* enemy, Task* task)
{
    Actor105700Work* work;
    GpEnemy*         spawned;
    TmdObject*       src;
    TmdObject*       dst;

    work                             = (Actor105700Work*)task->parent->work;
    ((TmdObject*)task->extra)->flags = ((TmdObject*)task->parent->extra)->flags;
    if (work->field_6BA != 0) {
        work->field_6BA = 0;
        spawned         = Gp_SpawnEnemyFromTable(work->field_66C, 2, 0, enemy);
        src             = (TmdObject*)task->extra;
        dst             = (TmdObject*)spawned->task->extra;
        dst->tpage      = src->tpage;
        dst->clut       = src->clut;
        if (dst->buffer != NULL) {
            tmdProcessStream(dst);
            tmdProcessStream(dst);
        }
    }
}
