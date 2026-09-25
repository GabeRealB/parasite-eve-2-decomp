#include "common.h"

#include "gameplay/1BC.h"

#include "actors/actor_450800.h"

/// `func_800B4114` is deliberately declared locally with a signed `arg2`; see
/// the note in `include/gameplay/1BC.h`.
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

/// Ticks the enemy's animation slots 1..0x12.
void func_actor_450800_80133400(Task* task)
{
    Actor450800SpawnWork* work;
    s32                   i;

    work = (Actor450800SpawnWork*)task->work;
    i    = 1;
    do {
        Gp_AnimTickIndex(&work->anim, i);
        i++;
    } while (i < 0x13);
}

/// Resets the enemy's animation slots 1..0x12 to clip `animId` at rate 1,
/// without a reset argument, and latches the clip into `appliedAnimId`.
void func_actor_450800_8013344C(Task* task)
{
    Actor450800SpawnWork* work;
    s32                   i;

    work = (Actor450800SpawnWork*)task->work;
    for (i = 1; i < 0x13; i++) {
        work->slots[i].rate = 1;
        Gp_AnimResetSlot(&work->anim, i, (s16)work->animId);
    }
    work->appliedAnimId = work->animId;
}

/// Starts the enemy's animation slots 1..0x12 on clip `animId`, forwarding
/// `animArg` as the reset argument, and latches the clip into `appliedAnimId`.
void func_actor_450800_801334C4(Task* task)
{
    Actor450800SpawnWork* work;
    s32                   i;

    work = (Actor450800SpawnWork*)task->work;
    i    = 1;
    do {
        func_800B4114(&work->anim, i, (s16)work->animId, 0, work->animArg);
        i++;
    } while (i < 0x13);
    work->appliedAnimId = work->animId;
}
