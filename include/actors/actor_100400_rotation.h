#ifndef ACTOR_100400_ROTATION_H
#define ACTOR_100400_ROTATION_H

#include "common.h"

#include "main/gfx.h"

#include "gameplay/3CD8.h"
#include "psyq/inline_c.h"

/* Inline forms of this actor's rotation traversal helpers. They are the
   `ActorsShared80132808` pair with one extra step: every ancestor rotation is
   copied out and renormalised before it is fed to the GTE, instead of being
   loaded straight from the coordinate. */

/// Accumulate `arg0`'s parent chain into `arg1`: seed it with the node's own
/// rotation, then pre-multiply by each (renormalised) ancestor up to but not
/// including `arg2`, renormalising after every step. Returns whether the walk
/// stopped on `arg2` rather than running off the end of the chain.
static __inline__ s32 Actor00400_AccumulateRotation(GsCOORDINATE2* arg0, MATRIX* arg1, GsCOORDINATE2* arg2)
{
    MATRIX         normal;
    MATRIX         matrix;
    GsCOORDINATE2* coord;

    coord = arg0->sub;
    *arg1 = arg0->coord;
    while (1) {
        if (coord == NULL) {
            return 0;
        }
        if (coord == arg2) {
            return 1;
        }
        matrix = coord->coord;
        MatrixNormal(&matrix, &matrix);
        gte_SetRotMatrix(&matrix);
        MulRotMatrix(arg1);
        MatrixNormal(arg1, &normal);
        *arg1 = normal;
        coord = coord->sub;
    }
}

/// Undo the parent chain again, turning the world-space rotation in `arg1`
/// back into one relative to `arg0`'s parent: accumulate the chain *above* the
/// parent, transpose it (the 3x3 inverse of a rotation) and pre-multiply.
/// Nothing to do when the parent is already the view coordinate.
///
/// Returns `arg0` so the caller stores through the returned pointer; the copy
/// GCC emits where the exits merge is what gives the store base its own
/// pseudo. Three details here are matching requirements rather than style:
/// the early `return arg0;` on the end-of-chain exit (it is what lifts `arg0`
/// past the scratch pointers in global-alloc's priority order, so it keeps
/// `$s3`), and the `mp` / `lp` pointer variables, whose declarations must
/// precede `view` so their pseudos out-rank it when the two tie.
static __inline__ GsCOORDINATE2* Actor00400_LocalizeRotation(GsCOORDINATE2* arg0, MATRIX* arg1)
{
    MATRIX         matrix;
    MATRIX         local;
    MATRIX         normal;
    MATRIX         transposed;
    MATRIX*        mp;
    MATRIX*        lp;
    GsCOORDINATE2* coord;
    GsCOORDINATE2* view;

    coord = arg0->sub;
    if (coord != &gGfxViewCoord) {
        mp     = &matrix;
        view   = &gGfxViewCoord;
        lp     = &local;
        matrix = coord->coord;
        while (1) {
            coord = coord->sub;
            if (coord == NULL) {
                return arg0;
            }
            if (coord == view) {
                __asm__ volatile(
                    "lhu $12, 0(%0);"
                    "lhu $13, 6(%0);"
                    "lhu $14, 12(%0);"
                    "sh $12, 0(%1);"
                    "sh $13, 2(%1);"
                    "sh $14, 4(%1);"
                    "lhu $12, 2(%0);"
                    "lhu $13, 8(%0);"
                    "lhu $14, 14(%0);"
                    "sh $12, 6(%1);"
                    "sh $13, 8(%1);"
                    "sh $14, 10(%1);"
                    "lhu $12, 4(%0);"
                    "lhu $13, 10(%0);"
                    "lhu $14, 16(%0);"
                    "sh $12, 12(%1);"
                    "sh $13, 14(%1);"
                    "sh $14, 16(%1);"
                    : : "r"(mp), "r"(&transposed) : "$12", "$13", "$14", "memory");
                gte_SetRotMatrix(&transposed);
                MulRotMatrix(arg1);
                break;
            }
            local = coord->coord;
            MatrixNormal(&local, &local);
            gte_SetRotMatrix(lp);
            MulRotMatrix(&matrix);
            MatrixNormal(&matrix, &normal);
            matrix = normal;
        }
    }
    return arg0;
}

#endif
