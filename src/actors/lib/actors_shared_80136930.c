#include "common.h"

#include "main/task.h"
#include "main/tmd.h"

#include "actors/actors_shared_80136930.h"

/// Per-frame handler of the enemy actors parented to a spawner's model: it
/// mirrors the spawner's model flags onto its own every frame, and when the
/// parent's work block raises `field_6BA` it spawns one companion effect from
/// `field_66C` and hands it this actor's texture page and CLUT row. That last
/// step is `ActorsShared8013851c` inlined - the stream is processed twice
/// because `Tmd_ProcessStream` advances one of the two half-buffers per call.
void ActorsShared80136930(GpEnemy* enemy, Task* task)
{
    ActorsShared80136930Work* work;
    GpEnemy*                  spawned;
    TmdObject*                src;
    TmdObject*                dst;

    work                               = (ActorsShared80136930Work*)task->parent->idMap;
    ((TmdObject*)task->extra)->field_C = ((TmdObject*)task->parent->extra)->field_C;
    if (work->field_6BA != 0) {
        work->field_6BA = 0;
        spawned         = Gp_SpawnEnemyFromTable(work->field_66C, 2, 0, enemy);
        src             = (TmdObject*)task->extra;
        dst             = (TmdObject*)spawned->task->extra;
        dst->field_24   = src->field_24;
        dst->field_25   = src->field_25;
        if (dst->field_18 != NULL) {
            Tmd_ProcessStream(dst);
            Tmd_ProcessStream(dst);
        }
    }
}
