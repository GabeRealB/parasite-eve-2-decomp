#include "common.h"

#include "actors/actor_323000.h"

void func_actor_323000_80164C58(GpEnemy* enemy, Task* task)
{
    Actor323000Work* work;
    TmdObject*       obj;
    SVECTOR          unused; // never referenced; only reserves the frame slot the ROM has

    work = (Actor323000Work*)task->work;
    if (work->field_4 != 0) {
        obj               = (TmdObject*)task->extra;
        enemy->node.flags = 1;
        obj->flags        = 0;
        Tmd_AllocBuffers(obj);
        work->field_832 = 0x10;
        work->field_82E = 0xD;
        work->field_828 = 2;
        work->field_83E = 0;
        work->field_840 = 0;
        work->field_6   = 0;
        func_actor_323000_80163A30(task);
    } else {
        func_actor_323000_80163A30(task);
    }
}
