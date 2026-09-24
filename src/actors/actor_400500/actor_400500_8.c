#include "common.h"
#include "psyq/libgte.h"

#include "actors/actor_400500.h"

/// Copies the 3x3 rotation of `src` into `dst`, leaving `dst`'s translation
/// alone. The actor builds a part's rotation in a scratch matrix and pushes
/// it into the part's `GsCOORDINATE2::coord` with this, so the part keeps its
/// position.
void func_actor_400500_8013DE2C(MATRIX* src, MATRIX* dst)
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
