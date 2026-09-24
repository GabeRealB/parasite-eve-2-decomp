#include "common.h"

#include <psyq/inline_c.h>
#include "gte.h"

#include "actors/actor_403200_view.h"
#include "gameplay/gameplay.h"
#include "main/gfx.h"
#include "main/mem.h"

/// Psy-Q `RotMatrixY` (it sits right after `RotMatrixX`).
void func_8004BFF8(s16 angle, MATRIX* matrix);

/// Turn the world-space rotation in `mat` back into one relative to `coord`'s
/// parent: accumulate the chain above the parent up to the view coordinate,
/// transpose it (the inverse of a rotation) and pre-multiply. Nothing to do
/// when the parent already is the view coordinate. Returns `coord`, which the
/// caller stores through.
static __inline__ GsCOORDINATE2* _actor403200Localize(GsCOORDINATE2* coord, MATRIX* mat)
{
    MATRIX         matrix;
    MATRIX         normal;
    MATRIX         transposed;
    GsCOORDINATE2* cur;
    GsCOORDINATE2* view;

    cur = coord->sub;
    if (cur != &gGfxViewCoord) {
        view   = &gGfxViewCoord;
        matrix = cur->coord;
        while (1) {
            cur = cur->sub;
            if (cur == NULL) {
                break;
            }
            if (cur == view) {
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
                MulRotMatrix(mat);
                break;
            }
            gte_SetRotMatrix(&cur->coord);
            MulRotMatrix(&matrix);
            MatrixNormal(&matrix, &normal);
            matrix = normal;
        }
    }
    return coord;
}

/// Set `coord`'s rotation to its view-space orientation turned by `yaw`,
/// expressed back in its parent's frame, and refresh the coordinate. The work
/// matrix is borrowed from the scratchpad stack.
void func_actor_403200_801321C4(GsCOORDINATE2* coord, s16 yaw)
{
    MATRIX*        rotation;
    GsCOORDINATE2* out;

    *(MATRIX**)G_SCRATCH_HEAD -= 1;
    rotation                   = *(MATRIX**)G_SCRATCH_HEAD;
    Actor403200_AccumulateRotation(coord, rotation);
    func_8004BFF8(yaw, rotation);
    out = _actor403200Localize(coord, rotation);
    __builtin_memcpy(out->coord.m, rotation->m, sizeof(out->coord.m));
    out->flg = 0;
    Gp_UpdateCoord(out);
    *(MATRIX**)G_SCRATCH_HEAD += 1;
}
