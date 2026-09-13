#include "common.h"

#include "actors/actor_402200.h"

INCLUDE_ASM("actors/nonmatchings/actor_402200/actor_402200_2", func_actor_402200_80137D78);

INCLUDE_ASM("actors/nonmatchings/actor_402200/actor_402200_2", func_actor_402200_80137E48);

/// 1BC.h keeps this out of scope on purpose: callers hand it a sign-extended
/// animation id, which a `u16` prototype would zero-extend.
void func_800B4114(void* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

/// Reseeds animation slots 1..0x12 when the actor's animation id changes,
/// handing each slot the blend weight the id selects from
/// `D_actor_402200_801383AC`; while the id is unchanged it instead ticks every
/// slot one frame and walks the id's frame counter up.
void func_actor_402200_80137EEC(Actor402200* arg0)
{
    Actor402200Work* work;
    s32              i;
    s32              value;

    work = arg0->field_1C;
    i    = 1;
    if (work->field_6C0 != work->field_6C2) {
        work->field_6C2 = work->field_6C0;
        work->field_6C4 = 0;
        value           = D_actor_402200_801383AC[work->field_6C0];
        for (; i < 0x13; i++) {
            func_800B4114(work, i, work->field_6C0, 0, value);
        }
    } else {
        TOUCH_REG(i);
        work->field_6C4 += i;
        do {
            Gp_AnimTickIndex((GpAnimCtx*)work, i);
            i++;
        } while (i < 0x13);
    }
}
