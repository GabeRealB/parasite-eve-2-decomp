#include "common.h"

#include "actors/actor_101500.h"
#include "gameplay/1BC.h"

/// Countdown pose. Requests pose 9 and clears the move state each frame; when
/// `field_362` runs out it switches to pose 7 and reloads the countdown from a
/// `Gp_LcgState` draw into `D_actor_101500_8013BDE8`.
void func_actor_101500_801346D0(Actor101500* actor)
{
    Actor101500Work* work = actor->field_1C;
    u32              rnd;
    u16              val;
    u16*             tbl;

    work->field_352 = 9;
    work->field_34C = 0;
    work->field_360 = 0;
    work->field_366 = 0;
    if (--work->field_362 == 0) {
        tbl             = D_actor_101500_8013BDE8;
        work->field_358 = 1;
        work->field_35A = 2;
        work->field_352 = 7;
        work->field_35C = 0;
        rnd             = Gp_LcgState * 5 + 0x71357911;
        Gp_LcgState     = rnd;
        val             = tbl[(rnd >> 16) & 0xF];
        work->field_36E = 0;
        work->field_34C = 0x400F0002;
        work->field_380 = 15;
        work->field_362 = val;
    }
}

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
