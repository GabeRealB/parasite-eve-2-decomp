#ifndef ACTORS_SHARED_80132808_H
#define ACTORS_SHARED_80132808_H

#include "common.h"

#include "gameplay/3CD8.h"
#include "psyq/inline_c.h"

/// Accumulate `arg0`'s parent chain into `arg1`: seed it with the node's own
/// rotation, then pre-multiply by each ancestor up to (but not including)
/// `arg2`, renormalising after every step. Returns whether the walk stopped on
/// `arg2` rather than running off the end of the chain.
static __inline__ s32 ActorsShared80132808_Accumulate(GsCOORDINATE2* arg0, MATRIX* arg1, GsCOORDINATE2* arg2)
{
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
        gte_SetRotMatrix(&coord->coord);
        MulRotMatrix(arg1);
        MatrixNormal(arg1, &matrix);
        *arg1 = matrix;
        coord = coord->sub;
    }
}

/// Undo the parent chain again, turning the world-space rotation in `arg1`
/// back into one relative to `arg0`'s parent: accumulate the chain *above* the
/// parent, transpose it (the 3x3 inverse of a rotation) and pre-multiply.
/// Nothing to do when the parent is already the view coordinate.
///
/// Returns `arg0` so the caller stores through the returned pointer. That is a
/// matching requirement, not decoration: the copy GCC emits where the exits
/// merge is what gives the store base its own pseudo, and keeping the helper
/// to a *single* exit keeps `arg0`'s reference count low enough that
/// global-alloc ranks it last and leaves it in `$s4`.
static __inline__ GsCOORDINATE2* ActorsShared80132808_Localize(GsCOORDINATE2* arg0, MATRIX* arg1)
{
    MATRIX         matrix;
    MATRIX         normal;
    MATRIX         transposed;
    GsCOORDINATE2* coord;
    GsCOORDINATE2* view;

    coord = arg0->sub;
    if (coord != &Gfx_ViewCoord) {
        view   = &Gfx_ViewCoord;
        matrix = coord->coord;
        while (1) {
            coord = coord->sub;
            if (coord == NULL) {
                break;
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
                    : : "r"(&matrix), "r"(&transposed) : "$12", "$13", "$14", "memory");
                gte_SetRotMatrix(&transposed);
                MulRotMatrix(arg1);
                break;
            }
            gte_SetRotMatrix(&coord->coord);
            MulRotMatrix(&matrix);
            MatrixNormal(&matrix, &normal);
            matrix = normal;
        }
    }
    return arg0;
}

/// Psy-Q `RotMatrixY` (it sits right after `RotMatrixX`).
void func_8004BFF8(s16 angle, MATRIX* matrix);

/// Re-aim one joint by `yaw` about Y in world space: build the joint's absolute
/// rotation from its parent chain, turn it, then express the result back in the
/// parent's frame and write the 3x3 into the joint. The working matrix is one
/// 0x20-byte frame carved off the scratchpad head. Shared by fifteen actor
/// overlays.
void ActorsShared80132808(GsCOORDINATE2* coord, s16 yaw);

#endif
