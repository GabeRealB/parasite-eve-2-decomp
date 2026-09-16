#include "common.h"

#include "actors/actor_104000.h"
#include "gameplay/1BC.h"
#include "main/task.h"
#include "main/tmd.h"

/// State 0 of the actor's 19-entry `GpEnemyTaskFuncTable` dispatch: the
/// leave/despawn teardown. When the work block's `field_4` latch is set it
/// raises the enemy's list-node flag, sets the display object's visibility bit
/// and clears the gate bits on four records of the 0x38-stride flag table.
///
/// Shared: the three actor slots (`actor_104000` / `204000` / `304000`) carry
/// the same body byte for byte, so one object serves every overlay that lists
/// this unit in `configs/USA/overlays.toml`.
void ActorsShared8015087c(GpEnemy* enemy, Task* task)
{
    Actor104000Work* work;
    TmdObject*       obj;

    work = (Actor104000Work*)task->idMap;
    if (work->field_4 != 0) {
        obj                 = task->extra;
        enemy->node.field_4 = 1;
        obj->field_C        = (u16)(obj->field_C | 0x80);
        work->field_36E     = (u16)(work->field_36E & 0x7FFF);
        work->field_3A6     = (u16)(work->field_3A6 & 0x7FFF);
        work->field_3DE     = (u16)(work->field_3DE & 0x7FFF);
        work->field_28E     = (u16)(work->field_28E & 0xBFFF);
    }
}
