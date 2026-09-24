#include "common.h"

#include <psyq/inline_c.h>
#include "gte.h"

#include "actors/actor_401300.h"
#include "gameplay/gameplay.h"
#include "main/gfx.h"
#include "main/mem.h"

void func_8004BFF8(s16 angle, MATRIX* matrix);

/// Builds `joint`'s absolute rotation in `out`: its own rotation, then each
/// ancestor pre-multiplied in turn (renormalised after every step) up to but
/// not including `stop`. Returns whether the walk reached `stop` rather than
/// the end of the chain.
static __inline__ s32 Actor401300_AccumulateRotation(GsCOORDINATE2* joint, MATRIX* out, GsCOORDINATE2* stop)
{
    MATRIX         matrix;
    GsCOORDINATE2* coord;

    coord = joint->sub;
    *out  = joint->coord;
    while (1) {
        if (coord == NULL) {
            return 0;
        }
        if (coord == stop) {
            return 1;
        }
        gte_SetRotMatrix(&coord->coord);
        MulRotMatrix(out);
        MatrixNormal(out, &matrix);
        *out  = matrix;
        coord = coord->sub;
    }
}

/// Turns the world-space rotation in `rotation` back into one relative to
/// `joint`'s parent: accumulates the chain above the parent up to the view
/// coordinate, transposes it and pre-multiplies. Nothing is done when the
/// parent is the view coordinate itself. Returns `joint`; the caller stores
/// through the returned pointer, which the matched code needs.
static __inline__ GsCOORDINATE2* Actor401300_LocalizeRotation(GsCOORDINATE2* joint, MATRIX* rotation)
{
    MATRIX         matrix;
    MATRIX         normal;
    MATRIX         transposed;
    GsCOORDINATE2* coord;
    GsCOORDINATE2* view;

    coord = joint->sub;
    if (coord != &gGfxViewCoord) {
        view   = &gGfxViewCoord;
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
                MulRotMatrix(rotation);
                break;
            }
            gte_SetRotMatrix(&coord->coord);
            MulRotMatrix(&matrix);
            MatrixNormal(&matrix, &normal);
            matrix = normal;
        }
    }
    return joint;
}

/// Turns joint `coord` by `yaw` about the world Y axis: builds its world
/// rotation in a matrix carved off the scratchpad head, applies the turn,
/// converts the result back into the parent's frame, writes the 3x3 into the
/// joint and refreshes it. The actor turns two joints of its chain with it, by
/// two thirds and one half of the same clamped angle.
void func_actor_401300_801320A4(GsCOORDINATE2* coord, s16 yaw)
{
    MATRIX*        rotation;
    GsCOORDINATE2* out;

    *(MATRIX**)G_SCRATCH_HEAD -= 1;
    rotation                   = *(MATRIX**)G_SCRATCH_HEAD;
    Actor401300_AccumulateRotation(coord, rotation, &gGfxViewCoord);
    func_8004BFF8(yaw, rotation);
    out = Actor401300_LocalizeRotation(coord, rotation);
    __builtin_memcpy(out->coord.m, rotation->m, sizeof(out->coord.m));
    out->flg = 0;
    Gp_UpdateCoord(out);
    *(MATRIX**)G_SCRATCH_HEAD += 1;
}
