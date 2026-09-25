#include "common.h"
#include <psyq/libgte.h>
#include <psyq/inline_c.h>
#include "gte.h"

#include "actors/actor_160700.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"

extern u8 D_80072729;

/// Steps `coord` `amount` units along its local Z axis, unless movement is
/// frozen. The direction is staged on the scratchpad stack.
static __inline__ void Actor160700_MoveForward(GsCOORDINATE2* coord, s16 amount)
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

/// The actor's animation step. State 1 reseeds the slots with `animArg` and
/// state 2 resets them, each then moving on to state 3; state 3 walks the
/// root coordinate 12 units forward per frame while clip 4 still has `travel`
/// left, switching to clip 1 when it runs out, and ticks the slots.
void func_actor_160700_80132184(Task* task)
{
    Actor160700Work* work;
    s16              animId;

    work = (Actor160700Work*)task->work;
    if (work->state == 1) {
        func_actor_160700_8013258C(task);
        work->state = 3;
        return;
    }
    if (work->state == 2) {
        func_actor_160700_80132514(task);
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
            Actor160700_MoveForward(((TmdObject*)task->extra)->coords, 0xC);
            work->travel = (u16)work->travel - 1;
            if (work->travel == 0) {
                work->animArg = 0xA;
                work->animId  = 1;
            }
        }
        func_actor_160700_801324C8(task);
        return;
    }
}
