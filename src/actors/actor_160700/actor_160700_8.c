#include "common.h"

#include "actors/actor_160700.h"
#include "gameplay/1BC.h"
#include "main/task.h"

/// Ticks animation slots 1..0x13.
void func_actor_160700_801324C8(Task* task)
{
    Actor160700Work* work;
    s32              i;

    work = (Actor160700Work*)task->work;
    i    = 1;
    do {
        Gp_AnimTickIndex(&work->anim, i);
        i++;
    } while (i < 0x14);
}

/// Resets animation slots 1..0x13 to clip `animId` and records it as the
/// applied clip.
void func_actor_160700_80132514(Task* task)
{
    Actor160700Work* work;
    s32              i;

    work = (Actor160700Work*)task->work;
    i    = 1;
    do {
        work->slots[i].rate = 1;
        Gp_AnimResetSlot(&work->anim, i, work->animId);
        i++;
    } while (i < 0x14);
    work->appliedAnimId = work->animId;
}
