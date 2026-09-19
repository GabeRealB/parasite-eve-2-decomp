#ifndef ACTORS_ACTOR_403200_VIEW_H
#define ACTORS_ACTOR_403200_VIEW_H

#include "common.h"

#include "gameplay/3CD8.h"
#include "main/gfx.h"

#include <psyq/inline_c.h>

/// `MVMVA` with `sf = 1` reading the rotation matrix and V0 -- the operation
/// `gte_RotTrans` performs -- spelled with the COP2 prefix the retail build
/// used, which `psyq/inline_c.h` omits. Same form as
/// `include/actors/actor_444000_view.h`.
#define gte_rt_real() __asm__ volatile("nop; nop; .word 0x4A480012")

/// GPF with `sf = 1`, which `psyq/inline_c.h` spells without the COP2 prefix
/// the retail build used. Same form as `include/actors/actor_444000_view.h`.
#define gte_gpf12_real() __asm__ volatile("nop; nop; .word 0x4B98003D")

/// Accumulated world rotation of `coord`: `mat` starts as the coordinate's own
/// rotation and is multiplied by each parent's in turn, renormalised at every
/// level, until the chain reaches `gGfxViewCoord` (or runs out).
static __inline__ void Actor403200_AccumulateRotation(GsCOORDINATE2* coord, MATRIX* mat)
{
    MATRIX         m;
    GsCOORDINATE2* cur;

    cur  = coord->sub;
    *mat = coord->coord;
    while (1) {
        if (cur == NULL) {
            return;
        }
        if (cur == &gGfxViewCoord) {
            return;
        }
        gte_SetRotMatrix(&cur->coord);
        MulRotMatrix(mat);
        MatrixNormal(mat, &m);
        *mat = m;
        cur  = cur->sub;
    }
}

/// World position of `coord` as seen from `gGfxViewCoord`: `out` starts as the
/// point in `coord`'s own space and is walked up the coordinate hierarchy, one
/// `gte_rt` per level, until the view coordinate is reached. A hierarchy that
/// does not end at the view coordinate leaves `out` untouched.
static __inline__ void Actor403200_LocalToView(GsCOORDINATE2* coord, SVECTOR* out)
{
    SVECTOR acc;
    VECTOR  v;
    s32     flag;

    acc.vx = out->vx;
    acc.vy = out->vy;
    acc.vz = out->vz;

    for (;;) {
        if (coord->sub == NULL) {
            return;
        }
        if (coord != &gGfxViewCoord) {
            gte_SetTransMatrix(&coord->coord);
            gte_SetRotMatrix(&coord->coord);
            gte_ldv0(&acc);
            gte_rt_real();
            gte_stlvnl(&v);
            gte_stflg(&flag);
            acc.vx = v.vx;
            acc.vy = v.vy;
            acc.vz = v.vz;
            coord  = coord->sub;
        } else {
            out->vx = acc.vx;
            out->vy = acc.vy;
            out->vz = acc.vz;
            return;
        }
    }
}

#endif
