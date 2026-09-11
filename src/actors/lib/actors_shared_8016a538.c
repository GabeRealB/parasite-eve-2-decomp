#include "common.h"

#include "main/task.h"
#include "main/tmd.h"

#include "actors/actors_shared_8016a538.h"

void ActorsShared8016a538(Task* arg0)
{
    ActorsShared80168d3cWork* work;
    GsCOORDINATE2*            coord;
    ActorsShared8016a538Mat   rot;
    ActorsShared8016a538Mat*  src;
    MATRIX*                   dst;
    s16                       pitch;

    work               = (ActorsShared80168d3cWork*)arg0->idMap;
    coord              = ((TmdObject*)arg0->extra)->field_8;
    src                = &rot;
    src->ident.m00_m01 = 0x1000;
    src->ident.m02_m10 = 0;
    src->ident.m11_m12 = 0x1000;
    src->ident.m20_m21 = 0;
    src->ident.m22     = 0x1000;
    pitch              = ((rsin(work->field_442 << 6) * 0x10) >> 7) - 0x400;
    RotMatrixX(pitch, &src->mat);
    func_8004BFF8(work->field_7A, &src->mat);
    dst          = &coord->coord;
    dst->m[0][0] = src->mat.m[0][0];
    dst->m[0][1] = src->mat.m[0][1];
    dst->m[0][2] = src->mat.m[0][2];
    dst->m[1][0] = src->mat.m[1][0];
    dst->m[1][1] = src->mat.m[1][1];
    dst->m[1][2] = src->mat.m[1][2];
    dst->m[2][0] = src->mat.m[2][0];
    dst->m[2][1] = src->mat.m[2][1];
    dst->m[2][2] = src->mat.m[2][2];
    if (work->field_41E == 1) {
        work->field_41E = 0;
        work->field_432 = 0;
        work->field_428 = 0;
        work->field_42A = 0;
        work->field_434 = pitch;
        work->field_422++;
    }
}
