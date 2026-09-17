#ifndef ACTOR_461800_MOVE_H
#define ACTOR_461800_MOVE_H

#include "main/gfx.h"
#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"
#include <psyq/inline_c.h>

extern u8 D_80072729;

/// Steps the task's model `amount` units along its facing (the coordinate
/// matrix's z column, normalised and scaled on the GTE), using a scratch-pad
/// vector; skipped while `D_80072729` is 1.
static __inline__ void Actor461800_MoveForward(Task* task, s16 amount)
{
    GsCOORDINATE2* coord;
    SVECTOR*       head;
    SVECTOR*       vec;

    coord = ((TmdObject*)task->extra)->field_8;
    if (D_80072729 != 1) {
        head                       = *(SVECTOR**)G_SCRATCH_HEAD;
        vec                        = head - 1;
        *(SVECTOR**)G_SCRATCH_HEAD = vec;
        Gfx_MatrixCol2(&coord->coord, vec);
        VectorNormalSS(vec, vec);
        gte_lddp(amount);
        gte_ldsv(vec);
        __asm__ volatile("nop; nop; .word 0x4B98003D");
        gte_stsv(vec);
        coord->coord.t[0]          += head[-1].vx;
        coord->coord.t[1]          += vec->vy;
        coord->coord.t[2]          += vec->vz;
        coord->flg                  = 0;
        *(SVECTOR**)G_SCRATCH_HEAD += 1;
    }
}

#endif
