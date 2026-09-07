#include "common.h"

#include "main/gfx.h"

/// Copies the 3x3 rotation of `src` into `dst`, leaving `dst`'s translation
/// row alone. Used to push a freshly built scratch matrix into a part's
/// `GsCOORDINATE2::coord` without disturbing its position.
void ActorsShared80132c4c(MATRIX* src, MATRIX* dst)
{
    dst->m[0][0] = src->m[0][0];
    dst->m[0][1] = src->m[0][1];
    dst->m[0][2] = src->m[0][2];
    dst->m[1][0] = src->m[1][0];
    dst->m[1][1] = src->m[1][1];
    dst->m[1][2] = src->m[1][2];
    dst->m[2][0] = src->m[2][0];
    dst->m[2][1] = src->m[2][1];
    dst->m[2][2] = src->m[2][2];
}
