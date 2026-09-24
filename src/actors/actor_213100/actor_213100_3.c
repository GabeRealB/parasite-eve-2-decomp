#include "common.h"
#include "actors/actor_213100.h"

#include "gameplay/1BC.h"

/// `func_800B4114` is deliberately declared locally with a signed `arg2`; see
/// `gameplay/1BC.h`.
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

/// Animation bank table the 0x7D3 handler indexes with the preset's
/// `field_0`.
extern void* D_actor_213100_801521A4[];

void func_actor_213100_8014A23C(Task* arg0)
{
    TmdObject*       ext;
    Actor213100Work* work;

    ext           = arg0->extra;
    work          = (Actor213100Work*)arg0->work;
    ext->lightMtx = &work->light;
    ext->colorMtx = &work->color;
}

/// Message-0x7D3 handler, also called directly by the spawn handler with the
/// initial preset. A changed bank index re-seeds the whole animation slot
/// array through `func_800B3F84` from the bank table and forgets the current
/// animation id. A changed animation id is then stored and installed on every
/// slot - through `func_800B4114` when the preset's `field_8` is set and the
/// slots have already been started, through `Gp_AnimResetSlot` otherwise -
/// after which every slot is ticked once and `field_43C` latches. An
/// unchanged id skips all of that. Returns 0.
s32 func_actor_213100_8014A258(Task* task, s32 arg1, Actor213100AnimPreset* msg, s32 arg3)
{
    Actor213100Work* work;
    TmdObject*       ext;
    s32              i;

    work = (Actor213100Work*)task->work;
    ext  = task->extra;
    if (msg->field_0 != work->field_43E) {
        work->field_43E = msg->field_0;
        work->field_43D = -1;
        func_800B3F84(&work->anim, D_actor_213100_801521A4[work->field_43E], ext, work->field_30C,
                      work->slots);
    }
    if (msg->field_4 != work->field_43D) {
        work->field_43D = msg->field_4;
        if (msg->field_8 != 0 && work->field_43C != 0) {
            for (i = 1; i < 0x13; i++) {
                func_800B4114(&work->anim, i, work->field_43D, 0, msg->field_C);
            }
        } else {
            for (i = 1; i < 0x13; i++) {
                Gp_AnimResetSlot(&work->anim, i, work->field_43D);
            }
        }
        for (i = 1; i < 0x13; i++) {
            Gp_AnimTickIndex(&work->anim, i);
        }
        work->field_43C = 1;
    }
    return 0;
}
