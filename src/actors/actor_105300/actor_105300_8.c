#include "common.h"

#include "actors/actor_105300.h"
#include "gameplay/1BC.h"

void func_800B4114(Actor05300Work* arg0, s32 arg1, s16 arg2, s32 arg3, s32 arg4);

/// Pose tick. When the pose asked for (`field_320`) differs from the one the
/// animation slots were last queued for (`field_322`), slots 1-9 are re-queued
/// with it and its entry of `D_actor_105300_80133A18`, and the frame count
/// `field_324` restarts; otherwise every slot is ticked and the count advances
/// by one.
void func_actor_105300_80133610(Actor05300* arg0)
{
    Actor05300Work* work;
    s32             i;
    s32             value;

    work = arg0->field_1C;
    i    = 1;
    if ((s16)work->field_320 != work->field_322) {
        work->field_322 = work->field_320;
        work->field_324 = 0;
        value           = D_actor_105300_80133A18[(s16)work->field_320];
        for (; i < 10; i++) {
            func_800B4114(work, i, (s16)work->field_320, 0, value);
        }
    } else {
        TOUCH_REG(i);
        work->field_324 += i;
        do {
            Gp_AnimTickIndex(&work->anim, i);
            i++;
        } while (i < 10);
    }
}
