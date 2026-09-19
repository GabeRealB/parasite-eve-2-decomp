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

    work = (Actor104000Work*)task->work;
    if (work->field_4 != 0) {
        obj                = task->extra;
        enemy->node.flags  = 1;
        obj->flags         = (u16)(obj->flags | 0x80);
        work->obj350.flags = (u16)(work->obj350.flags & 0x7FFF);
        work->obj388.flags = (u16)(work->obj388.flags & 0x7FFF);
        work->obj3C0.flags = (u16)(work->obj3C0.flags & 0x7FFF);
        work->obj270.flags = (u16)(work->obj270.flags & 0xBFFF);
    }
}
