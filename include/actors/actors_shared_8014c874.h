#ifndef ACTORS_SHARED_8014C874_H
#define ACTORS_SHARED_8014C874_H

#include "common.h"

#include <psyq/inline_c.h>
#include <psyq/libgte.h>

#include "gameplay/1BC.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/task.h"

extern u8 D_80072729;

/// Work block the actors sharing this body hang off the task's `Task::idMap`
/// slot (0x1C). Only the prefix this body reaches is described.
///
/// `state` selects the start path: 1 and 2 run the two reseed bodies and
/// advance to 3, the step state. `animId` 4 is the walk clip, which `travel`
/// counts down; `animArg` is the reset argument stored when the walk ends.
typedef struct ActorsShared8014c874Work {
    /* 0x000 */ byte pad_0[0x4B4];
    /* 0x4B4 */ s16  state;
    /* 0x4B6 */ byte pad_4B6[0x2];
    /* 0x4B8 */ s16  animId;
    /* 0x4BA */ byte pad_4BA[0x30];
    /* 0x4EA */ s16  travel;
    /* 0x4EC */ s16  animArg;
} ActorsShared8014c874Work;
STATIC_ASSERT_SIZEOF(ActorsShared8014c874Work, 0x4EE);

/// Steps `coord` `amount` units along its local Z axis unless movement is
/// frozen. Same body as `Actor521100_MoveForward`, in the early-return form
/// `ActorsShared8014c874` inlines.
static __inline__ void ActorsShared8014c874_MoveForward(GsCOORDINATE2* coord, s16 amount)
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
    __asm__ volatile("nop; nop; .word 0x4B98003D");
    gte_stsv(vec);
    coord->coord.t[0]          += head[-1].vx;
    coord->coord.t[1]          += vec->vy;
    coord->coord.t[2]          += vec->vz;
    coord->flg                  = 0;
    *(SVECTOR**)G_SCRATCH_HEAD += 1;
}

/// The actor's step body. States 1 and 2 run their reseed body and advance to
/// state 3; state 3 walks the attach coordinate 12 units per frame while the
/// walk clip has `travel` left, dropping back to clip 1 when it runs out, then
/// ticks the slots.
void ActorsShared8014c874(Task* task);

#endif
