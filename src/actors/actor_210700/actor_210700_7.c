#include "common.h"

#include "actors/actor_210700.h"

void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

/// Message-0x7D3 handler: starts an animation. A source index different from
/// the one last loaded seeds the animation context from that entry of
/// `D_actor_210700_801585C8` and forgets the current animation id; a new
/// animation id then resets slots 1..0x13 to it - through `func_800B4114`
/// when the payload's `field_8` is set, `Gp_AnimResetSlot` otherwise - ticks
/// them once and enables the per-frame tick. Always returns 0.
s32 func_actor_210700_8014A224(Task* task, s32 arg1, Actor210700Anim* msg, s32 arg3)
{
    Actor210700Work* work;
    s32              i;
    register s32     id asm("v1");
    TmdObject*       ext;

    work = (Actor210700Work*)task->work;
    id   = msg->field_0;
    ext  = (TmdObject*)task->extra;
    if (id != work->field_47C) {
        work->field_478 = -1;
        work->field_47C = id;
        func_800B3F84(&work->anim, D_actor_210700_801585C8[id], ext, work->field_334, work->slots);
    }
    if (msg->field_4 != work->field_478) {
        work->field_478 = msg->field_4;
        if (msg->field_8 != 0) {
            for (i = 1; i < 0x14; i++) {
                func_800B4114(&work->anim, i, work->field_478, 0, 6);
            }
        } else {
            for (i = 1; i < 0x14; i++) {
                Gp_AnimResetSlot(&work->anim, i, work->field_478);
            }
        }
        for (i = 1; i < 0x14; i++) {
            Gp_AnimTickIndex(&work->anim, i);
        }
        work->field_474 = 1;
    }
    return 0;
}
