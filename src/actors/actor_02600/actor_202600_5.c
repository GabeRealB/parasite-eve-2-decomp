#include "common.h"

#include "actors/actors_shared_8014d378.h"

#include "gameplay/1BC.h"

/// `func_800B4114` is deliberately declared locally with a signed `arg2`; see
/// the note in `include/gameplay/1BC.h`.
void func_800B4114(void* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

/* Animation id -> slot blend value table in this overlay's own data. */
extern s16 Actor02600_D08A10[];

/// Switches the work's animation id, resetting the slots to the blend value the
/// table gives for the new id; otherwise ticks every slot one frame.
void ActorsShared8014d378_Fn4D7C4(ActorShared8014d378* arg0)
{
    ActorShared8014d378Work* work;
    s32                      i;
    s32                      value;

    work = arg0->field_1C;
    i    = 1;
    if (work->field_392 != work->field_394) {
        work->field_394 = work->field_392;
        work->field_396 = 0;
        value           = Actor02600_D08A10[work->field_392];
        for (; i < 8; i++) {
            func_800B4114(work, i, work->field_392, 0, value);
        }
    } else {
        TOUCH_REG(i);
        work->field_396 += i;
        do {
            Gp_AnimTickIndex((GpAnimCtx*)work, i);
            i++;
        } while (i < 8);
    }
}
