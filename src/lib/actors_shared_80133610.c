#include "common.h"

#include "actors/actors_shared_80133610.h"

#include "gameplay/1BC.h"

void func_800B4114(ActorShared80133610Work* arg0, s32 arg1, s16 arg2, s32 arg3, s32 arg4);

/// Animation tick. When the pose the actor asks for differs from the one its
/// slots were last queued for, every slot is re-seeded from the per-state
/// animation id table and the frame counter is cleared; while the two agree
/// each slot is ticked and the frame counter accumulates the slot index.
void ActorsShared80133610(ActorShared80133610* arg0)
{
    ActorShared80133610Work* work;
    s32                      i;
    s32                      value;

    work = arg0->field_1C;
    i    = 1;
    if ((s16)work->field_320 != work->field_322) {
        work->field_322 = work->field_320;
        work->field_324 = 0;
        value           = ActorsShared80133610Table[(s16)work->field_320];
        for (; i < 10; i++) {
            func_800B4114(work, i, (s16)work->field_320, 0, value);
        }
    } else {
        TOUCH_REG(i);
        work->field_324 += i;
        do {
            Gp_AnimTickIndex((GpAnimCtx*)work, i);
            i++;
        } while (i < 10);
    }
}
