#include "common.h"

#include "gameplay/1BC.h"

#include "actors/actor_403900.h"

/// Per-animation-id value `func_actor_403900_80137EF0` hands `func_800B4114`
/// as its fifth argument when it reseeds animation slots 1..0x12.
extern s16 D_actor_403900_801383B0[];

/// 1BC.h keeps this out of scope on purpose: callers hand it a sign-extended
/// animation id, which a `u16` prototype would zero-extend.
void func_800B4114(void* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

/// Reseeds animation slots 1..0x12 when the actor's animation id changes,
/// handing each slot the blend weight the id selects from
/// `D_actor_403900_801383B0`; while the id is unchanged it instead ticks every
/// slot one frame and walks the id's frame counter up.
void func_actor_403900_80137EF0(Actor403900* arg0)
{
    Actor403900Work* work;
    s32              i;
    s32              value;

    work = arg0->field_1C;
    i    = 1;
    if (work->field_6C0 != work->field_6C2) {
        work->field_6C2 = work->field_6C0;
        work->field_6C4 = 0;
        value           = D_actor_403900_801383B0[work->field_6C0];
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
