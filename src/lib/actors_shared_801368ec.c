#include "common.h"
#include "main/task.h"
#include "main/tmd.h"
#include "actors/actors_shared_801368ec.h"
#include "actors/actors_shared_80136930.h"

/// Parents this actor's model to part 7 of its spawner's model and points the
/// model at the spawner's light and colour matrices, then advances the task to
/// state 1. `arg0` is the enemy context every state handler takes and is unused
/// here. Identical to `ActorsShared80135b64` but for the part index.
void ActorsShared801368ec(void* arg0, Task* task)
{
    Task*                     parent;
    TmdObject*                obj;
    ActorsShared801368ecWork* work;
    GsCOORDINATE2*            coord;
    GsCOORDINATE2*            parentCoords;

    parent       = task->parent;
    obj          = (TmdObject*)task->extra;
    parentCoords = ((TmdObject*)parent->extra)->coords;
    coord        = obj->coords;
    work         = (ActorsShared801368ecWork*)parent->work;

    coord->flg    = 0;
    coord->sub    = &parentCoords[7];
    obj->lightMtx = &work->field_45C;
    obj->flags    = 0;
    obj->colorMtx = &work->field_43C;
    task->state   = 1;
}

/// Per-frame handler of the enemy actors parented to a spawner's model: it
/// mirrors the spawner's model flags onto its own every frame, and when the
/// parent's work block raises `field_6BA` it spawns one companion effect from
/// `field_66C` and hands it this actor's texture page and CLUT row. That last
/// step is `ActorsShared8013851c` inlined - the stream is processed twice
/// because `tmdProcessStream` advances one of the two half-buffers per call.
void ActorsShared80136930(GpEnemy* enemy, Task* task)
{
    ActorsShared80136930Work* work;
    GpEnemy*                  spawned;
    TmdObject*                src;
    TmdObject*                dst;

    work                             = (ActorsShared80136930Work*)task->parent->work;
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
