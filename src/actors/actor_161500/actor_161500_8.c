#include "common.h"

#include "actors/actor_161500.h"
#include "gameplay/1BC.h"
#include "main/task.h"

/// `func_800B4114` is deliberately declared locally with a signed `arg2`; see
/// the note in `include/gameplay/1BC.h`.
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

/// Reseeds animation slots 1..0x13 with `animId`, passing `animArg` through,
/// and records that id as the one applied.
void func_actor_161500_801329C4(Task* task)
{
    Actor161500Work* work;
    s32              i;

    work = (Actor161500Work*)task->work;
    i    = 1;
    do {
        func_800B4114(&work->anim, i, work->animId, 0, work->animArg);
        i++;
    } while (i < 0x14);
    work->appliedAnimId = work->animId;
}
