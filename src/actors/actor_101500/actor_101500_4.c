#include "common.h"

#include "actors/actor_101500.h"
#include "gameplay/1BC.h"

void func_800B4114(Actor101500Work* arg0, s32 arg1, s16 arg2, s32 arg3, s32 arg4);

INCLUDE_ASM("actors/nonmatchings/actor_101500/actor_101500_4", func_actor_101500_801346D0);

/// Animation tick. When the pose the actor asks for differs from the one its
/// slots were last queued for, every slot is re-seeded from the per-state
/// animation id table and the frame counter is cleared; while the two agree
/// each slot is ticked and the frame counter accumulates the slot index.
void func_actor_101500_80134778(Actor101500* arg0)
{
    Actor101500Work* work;
    s32              i;
    s32              value;

    work = arg0->field_1C;
    i    = 1;
    if ((s16)work->field_352 != work->field_354) {
        work->field_354 = work->field_352;
        work->field_356 = 0;
        value           = D_actor_101500_8013BE70[(s16)work->field_352];
        for (; i < 7; i++) {
            func_800B4114(work, i, (s16)work->field_352, 0, value);
        }
    } else {
        TOUCH_REG(i);
        work->field_356 += i;
        do {
            Gp_AnimTickIndex((GpAnimCtx*)work, i);
            i++;
        } while (i < 7);
    }
}
