#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"

#include "actors/actor_104600.h"

/// Consumes the pending bits of the second enemy's `reactionFlags`: bit 0x1 is
/// dropped on its own, bit 0x2 puts the work into reaction state 3 with its
/// frame count cleared, and bits 0xC are dropped last, after re-reading the
/// byte.
void Actor04600_Fn03CEC(Task* arg0)
{
    GpEnemy*               enemy;
    Actor104600Enemy2Work* work;
    u8                     flags;

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

/// Per-frame dispatch on the second enemy's reaction state `field_286`: state
/// 0 runs the idle tick and state 2 does nothing. State 3 clears `field_292`
/// and turns the light blend down, resets the remembered animation id to 1 and
/// the counters every fourth frame, and returns to state 0 once
/// `Gp_TickObjFlag2` reports the reaction over.
void Actor04600_Fn03D54(Task* task)
{
    Actor104600Enemy2Work* work;

    work = task->work;
    switch (work->field_286) {
        case 0:
            Actor04600_Fn030A8(task);
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

/// Drives the second enemy's animation slots 1 and 2 from its animation id
/// `field_28C`. When it differs from the remembered `field_28E` it is
/// remembered, the frame count restarts and both slots switch to it with a
/// blend of 8; otherwise the count ticks and both slots advance.
void Actor04600_Fn03E10(Task* arg0)
{
    Actor104600Enemy2Work* work;
    s32                    i;

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
