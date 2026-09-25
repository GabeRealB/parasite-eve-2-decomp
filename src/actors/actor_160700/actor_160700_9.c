#include "common.h"

#include "actors/actor_160700.h"
#include "gameplay/1BC.h"
#include "main/task.h"

/// `func_800B4114` is deliberately declared locally with a signed `arg2`; see
/// the note in `include/gameplay/1BC.h`.
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

/// Reseeds animation slots 1..0x13 with clip `animId` and argument `animArg`,
/// and records the clip as the applied one.
void func_actor_160700_8013258C(Task* task)
{
    Actor160700Work* work;
    s32              i;

    work = (Actor160700Work*)task->work;
    i    = 1;
    do {
        func_800B4114(&work->anim, i, work->animId, 0, work->animArg);
        i++;
    } while (i < 0x14);
    work->appliedAnimId = work->animId;
}
