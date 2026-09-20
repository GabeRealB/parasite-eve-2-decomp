#include "common.h"

#include <psyq/inline_c.h>
#include <psyq/abs.h>

#include "actors/actor_401000.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/wipsys.h"

/// `gpf 12`; the `inline_c.h` macro of that name assembles to a different word.
#define gte_gpf12_real() __asm__ volatile("nop; nop; .word 0x4B98003D")

/// `Actor401300_MoveForward`: the plain forward-step helper, the shape the
/// state-8 body `func_actor_401000_801388F4` inlines. The `Nonzero` variant
/// below is the same block with an `amount != 0` test and a `gteVec` copy
/// wrapped around it.
static __inline__ void Actor401000_MoveForward(GsCOORDINATE2* coord, s16 amount)
{
    SVECTOR* head;
    SVECTOR* vec;

    if (D_80072729 != 1) {
        head                       = *(SVECTOR**)G_SCRATCH_HEAD;
        vec                        = head - 1;
        *(SVECTOR**)G_SCRATCH_HEAD = vec;
        Gfx_MatrixCol2(&coord->coord, vec);
        VectorNormalSS(vec, vec);
        gte_lddp(amount);
        gte_ldsv(vec);
        gte_gpf12_real();
        gte_stsv(vec);
        coord->coord.t[0]          += head[-1].vx;
        coord->coord.t[1]          += vec->vy;
        coord->coord.t[2]          += vec->vz;
        coord->flg                  = 0;
        *(SVECTOR**)G_SCRATCH_HEAD += 1;
    }
}

/// `Actor401300_MoveForwardNonzero` and `Actor00100_MoveForwardNonzero`, down
/// to the `head[-1].vx` read-back; the step lands in `coord` directly rather
/// than being reported back through the caller's local.
static __inline__ void Actor401000_MoveForwardNonzero(GsCOORDINATE2* coord, s16 amount)
{
    SVECTOR* head;
    SVECTOR* vec;
    SVECTOR* gteVec;

    if (D_80072729 != 1) {
        head                       = *(SVECTOR**)G_SCRATCH_HEAD;
        vec                        = head - 1;
        *(SVECTOR**)G_SCRATCH_HEAD = vec;
        gteVec                     = vec;
        if (amount != 0) {
            SOFT_TOUCH_REG(vec);
            Gfx_MatrixCol2(&coord->coord, vec);
            VectorNormalSS(vec, vec);
            gte_lddp(amount);
            gte_ldsv(gteVec);
            gte_gpf12_real();
            gte_stsv(gteVec);
            coord->coord.t[0] += head[-1].vx;
            coord->coord.t[1] += vec->vy;
            coord->coord.t[2] += vec->vz;
            coord->flg         = 0;
        }
        *(SVECTOR**)G_SCRATCH_HEAD += 1;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_401000/actor_401000", func_actor_401000_801323EC);

INCLUDE_ASM("actors/nonmatchings/actor_401000/actor_401000", func_actor_401000_80132590);

INCLUDE_ASM("actors/nonmatchings/actor_401000/actor_401000", func_actor_401000_80132824);

INCLUDE_ASM("actors/nonmatchings/actor_401000/actor_401000", func_actor_401000_80132A84);

INCLUDE_ASM("actors/nonmatchings/actor_401000/actor_401000", func_actor_401000_80132BB0);

// Animation driver is linked from actors_shared_80133eb8.c.
