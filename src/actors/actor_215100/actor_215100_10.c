#include "common.h"

#include "main/task.h"

#include "gameplay/1BC.h"

#include "actors/actor_215100.h"

/// Ticks animation slots 1..0x13.
void func_actor_215100_8014CBB8(Task* task)
{
    Actor215100Work* work;
    s32              i;

    work = (Actor215100Work*)task->work;
    i    = 1;
    do {
        Gp_AnimTickIndex(&work->anim, i);
        i++;
    } while (i < 0x14);
}

/// Resets animation slots 1..0x13 to clip `animId` and records it as the
/// applied clip.
void func_actor_215100_8014CC04(Task* task)
{
    Actor215100Work* work;
    s32              i;

    work = (Actor215100Work*)task->work;
    i    = 1;
    do {
        work->slots[i].rate = 1;
        Gp_AnimResetSlot(&work->anim, i, work->animId);
        i++;
    } while (i < 0x14);
    work->appliedAnimId = work->animId;
}
