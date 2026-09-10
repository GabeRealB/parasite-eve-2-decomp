#ifndef ACTOR_403100_ROTATION_H
#define ACTOR_403100_ROTATION_H

#include "actors/coord_to_view.h"
#include "psyq/inline_c.h"

/* Inline forms of this actor's rotation traversal helpers. */
static __inline__ s32 Actor403100_AccumulateRotation(GsCOORDINATE2* arg0, MATRIX* arg1, GsCOORDINATE2* arg2)
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

static __inline__ s32 Actor403100_LocalizeRotation(GsCOORDINATE2* arg0, MATRIX* arg1, GsCOORDINATE2* arg2)
{
    MATRIX         matrix;
    MATRIX         normal;
    MATRIX         transposed;
    GsCOORDINATE2* coord;

    coord = arg0->sub;
    if (coord == &Gfx_ViewCoord) {
        return 0;
    }
    matrix = coord->coord;
    while (1) {
        coord = coord->sub;
        if (coord == NULL) {
            return 0;
        }
        if (coord == arg2) {
            break;
        }
        gte_SetRotMatrix(&coord->coord);
        MulRotMatrix(&matrix);
        MatrixNormal(&matrix, &normal);
        matrix = normal;
    }
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
    return 1;
}

#endif
