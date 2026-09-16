#include "common.h"

#include "actors/actor_521100.h"

/// Main-executable global with no module header yet: the remaining-enemy count.
extern s16 D_80073BA0;

/// Declared locally with a signed `arg2`, as `include/gameplay/1BC.h` explains:
/// the definition takes `u16` so that its own body matches, but this caller
/// passes a sign-extended `s16` clip id.
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

INCLUDE_ASM("actors/nonmatchings/actor_521100/actor_521100_4", func_actor_521100_80135B40);

void func_actor_521100_80135B80(GpEnemy* arg0, Task* task)
{
    Actor521100Obj2C* obj;
    Actor521100Work*  work;
    s16               mode;

    work = (Actor521100Work*)task->parent->idMap;
    obj  = (Actor521100Obj2C*)task->extra;
    if (work->field_682 != 0) {
        mode         = ((work->field_692 & 1) == 0) << 7;
        obj->field_C = mode;
        if (work->field_692 & 2) {
            obj->field_C = mode | 4;
        }
        if (work->field_694 != 0) {
            obj->field_C = 0x80;
        }
    }
}

s32 func_actor_521100_80135BEC(Actor521100* arg0)
{
    if (D_80073BA0 > 0) {
        arg0->field_1C->field_6A8 = 1;
    }
    return 0;
}

s32 func_actor_521100_80135C14(Actor521100* arg0, s32 arg1, Actor521100AnimPreset* args)
{
    Actor521100Work* work;
    s32              i;
    s32              frames;
    s16              clip;
    s16              base;

    frames = 0;
    work   = arg0->field_1C;
    base   = 0x1D;
    if (args->field_0 == 0) {
        base = 0x14;
    }
    clip            = args->field_4 + base;
    work->field_686 = clip;
    work->field_688 = clip;
    if (args->field_8 != 0) {
        frames = args->field_C;
    }
    for (i = 1; i < 0x13; i++) {
        func_800B4114((GpAnimCtx*)work, i, work->field_686, 0, frames);
    }
    return 0;
}
