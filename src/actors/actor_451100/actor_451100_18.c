#include "common.h"

#include "actors/actor_451100.h"
#include "gameplay/1BC.h"
#include "main/task.h"

/// `func_800B4114` is deliberately declared locally with a signed `arg2`; see
/// the note in `include/gameplay/1BC.h`.
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

/// Ticks animation slots 1..0x12 of the task's work block.
void func_actor_451100_80132D70(Task* task)
{
    Actor451100Work* work;
    s32              i;

    work = (Actor451100Work*)task->work;
    i    = 1;
    do {
        Gp_AnimTickIndex(&work->anim, i);
        i++;
    } while (i < 0x13);
}

/// Restarts the animation without a reset argument, the step routine's state
/// 2: marks animation slots 1..0x12 as reset-pending, reseeds each from the
/// current animation id and records that id as the one now playing.
void func_actor_451100_80132DBC(Task* task)
{
    Actor451100Work* work;
    s32              i;

    work = (Actor451100Work*)task->work;
    for (i = 1; i < 0x13; i++) {
        work->slots[i].rate = 1;
        Gp_AnimResetSlot(&work->anim, i, (s16)work->animId);
    }
    work->field_47E = work->animId;
}

/// Restarts the animation with `animArg` as the reset argument, the step
/// routine's state 1, then records the animation id as the one now playing.
void func_actor_451100_80132E34(Task* task)
{
    Actor451100Work* work;
    s32              i;

    work = (Actor451100Work*)task->work;
    i    = 1;
    do {
        func_800B4114(&work->anim, i, (s16)work->animId, 0, (s16)work->animArg);
        i++;
    } while (i < 0x13);
    work->field_47E = work->animId;
}
