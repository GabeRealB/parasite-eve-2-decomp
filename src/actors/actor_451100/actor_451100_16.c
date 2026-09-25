#include "common.h"

#include <psyq/libgte.h>
#include <psyq/inline_c.h>
#include "gte.h"

#include "main/gfx.h"
#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"

#include "actors/actor_451100.h"

extern u8 D_80072729;

/// Steps `coord` `amount` units along its facing (the matrix's z column,
/// normalised and scaled on the GTE), using a scratch-pad vector; skipped while
/// `D_80072729` is 1.
static __inline__ void Actor451100_MoveCoordForward(GsCOORDINATE2* coord, s16 amount)
{
    SVECTOR* head;
    SVECTOR* vec;

    if (D_80072729 == 1) {
        return;
    }
    head                       = *(SVECTOR**)G_SCRATCH_HEAD;
    vec                        = head - 1;
    *(SVECTOR**)G_SCRATCH_HEAD = vec;
    Gfx_MatrixCol2(&coord->coord, vec);
    VectorNormalSS(vec, vec);
    gte_lddp(amount);
    gte_ldsv(vec);
    gte_gpf12();
    gte_stsv(vec);
    coord->coord.t[0]          += head[-1].vx;
    coord->coord.t[1]          += vec->vy;
    coord->coord.t[2]          += vec->vz;
    coord->flg                  = 0;
    *(SVECTOR**)G_SCRATCH_HEAD += 1;
}

/// Step routine of the actor `func_actor_451100_80132BD4` dispatches, run each
/// frame and by its "start animation" opcode. States 1 and 2 restart the clip
/// (with and without the reset argument) and advance to 3; state 3 walks the
/// model 0x11 units a frame while clip 4 plays and `travel` is non-zero,
/// dropping back to clip 1 with 0xA in `animArg` when the count runs out, then
/// ticks the animation.
void func_actor_451100_80132A1C(Task* task)
{
    Actor451100Work* work;
    s16              animId;

    work = (Actor451100Work*)task->work;
    if (work->state == 1) {
        func_actor_451100_80132E34(task);
        work->state = 3;
        return;
    }
    if (work->state == 2) {
        func_actor_451100_80132DBC(task);
        work->state = 3;
        return;
    }
    if (work->state == 3) {
        // The loop-end note ends cse's first block here, so the pause check
        // loads its own 1 instead of reusing the state test's.
        do {
        } while (0);
        animId = work->animId;
        if (animId == 4 && work->travel != 0) {
            Actor451100_MoveCoordForward(((TmdObject*)task->extra)->coords, 0x11);
            work->travel = (u16)work->travel - 1;
            if (work->travel == 0) {
                work->animArg = 0xA;
                work->animId  = 1;
            }
        }
        func_actor_451100_80132D70(task);
        return;
    }
}
