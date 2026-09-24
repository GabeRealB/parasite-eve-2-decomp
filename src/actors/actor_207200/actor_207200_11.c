#include "common.h"

#include "main/task.h"

#include "gameplay/1BC.h"

#include "actors/actor_207200.h"

/// `func_800B4114` is deliberately declared locally with a signed `arg2`; see
/// the note in `include/gameplay/1BC.h`.
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

/// Consumes the pending bits of the enemy's `reactionFlags`: bit 0x1 is
/// dropped on its own, bit 0x2 puts the work into reaction state 3 with its
/// frame counter cleared, and bits 0xC are dropped last, after re-reading the
/// byte.
void func_actor_207200_8014AE08(Task* arg0)
{
    GpEnemy*         enemy;
    Actor207200Work* work;
    u8               flags;

    enemy = (GpEnemy*)arg0->spawnArg2;
    work  = arg0->work;
    flags = enemy->reactionFlags;
    if (flags != 0) {
        if (flags & 1) {
            enemy->reactionFlags = flags & 0xFE;
        }
        if (enemy->reactionFlags & 2) {
            enemy->reactionFlags = enemy->reactionFlags & 0xFD;
            work->field_286      = 3;
            work->field_28A      = 0;
        }
        flags = enemy->reactionFlags;
        if (flags & 0xC) {
            enemy->reactionFlags = flags & 0xF3;
        }
    }
}

/// Per-frame dispatch on the small enemy's reaction state `field_286`: state
/// 0 runs the idle tick and state 2 does nothing. State 3 clears `field_292`
/// and turns the light blend down, resets the remembered animation id to 1
/// and the counters every fourth frame, and returns to state 0 once
/// `Gp_TickObjFlag2` reports the reaction over.
void func_actor_207200_8014AE70(Task* task)
{
    Actor207200Work* work;

    work = task->work;
    switch (work->field_286) {
        case 0:
            func_actor_207200_8014A1C4(task);
            break;
        case 2:
            break;
        case 3:
            work->field_292 = 0;
            work->field_2A6 = 0;
            work->field_28A = work->field_28A + 1;
            if (work->field_28A >= 4) {
                work->field_28E = 1;
                work->field_290 = 0;
                work->field_28A = 0;
            }
            if (Gp_TickObjFlag2((GpObj5D*)task->spawnArg2) != 0) {
                work->field_286 = 0;
            }
            break;
    }
}

/// Drives animation slots 1 and 2 from the work's animation id `field_28C`.
/// When it differs from the remembered `field_28E` it is remembered, the
/// frame counter restarts and both slots switch to it with a blend of 8;
/// otherwise the counter ticks and both slots advance.
void func_actor_207200_8014AF2C(Task* arg0)
{
    Actor207200Work* work;
    s32              i;

    work = arg0->work;
    i    = 1;
    if (work->field_28C != (s16)work->field_28E) {
        work->field_28E = work->field_28C;
        work->field_290 = 0;
        do {
            func_800B4114((GpAnimCtx*)work, i, work->field_28C, 0, 8);
            i++;
        } while (i < 3);
        return;
    }
    TOUCH_REG(i);
    work->field_290 = (u16)(work->field_290 + i);
    do {
        Gp_AnimTickIndex((GpAnimCtx*)work, i);
        i++;
    } while (i < 3);
}
