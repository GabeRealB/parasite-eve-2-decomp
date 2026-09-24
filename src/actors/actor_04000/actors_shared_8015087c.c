#include "common.h"

#include "actors/actor_104000.h"
#include "gameplay/1BC.h"
#include "main/task.h"
#include "main/tmd.h"

/// State 0 of the actor's per-frame dispatch: on the frame the state is
/// entered (`field_4` latch) it raises the enemy's list-node flag and the
/// display object's 0x80 bit, and clears the gate bits of the work block's
/// four collision objects (the high bit on three, 0x4000 on `obj270`).
void Actor04000_Fn06A5C(GpEnemy* enemy, Task* task)
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
