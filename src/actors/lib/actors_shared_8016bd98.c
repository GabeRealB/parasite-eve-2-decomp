#include "common.h"

#include "main/task.h"
#include "main/tmd.h"

#include "actors/actors_shared_8016bd98.h"

MATRIX* ScaleMatrix(MATRIX* m, VECTOR* v);
MATRIX* MulMatrix(MATRIX* m0, MATRIX* m1);

void ActorsShared8016bd98(Task* arg0)
{
    ActorsShared80168d3cWork*     work;
    TmdObject*                    obj;
    GsCOORDINATE2*                coord;
    VECTOR                        scale;
    ActorsShared8016bd98Mat       m;
    ActorsShared8016bd98MatWords* ident;

    work             = (ActorsShared80168d3cWork*)arg0->idMap;
    ident            = &m.ident;
    obj              = arg0->extra;
    coord            = obj->field_8;
    work->field_430 -= 0x40;
    scale.vx         = 0x1000;
    scale.vy         = (s16)work->field_430;
    scale.vz         = 0x1000;
    coord->coord     = work->mat_0;
    m.ident.m00_m01  = 0x1000;
    m.ident.m02_m10  = 0;
    ident->m11_m12   = 0x1000;
    m.ident.m20_m21  = 0;
    ident->m22       = 0x1000;
    ScaleMatrix(&m.mat, &scale);
    MulMatrix(&coord->coord, &m.mat);
    if ((s16)++work->field_412 == 0x10) {
        Gp_SetLightMode(arg0->spawnArg2, 2);
    }
    if ((s16)work->field_412 > 0x20) {
        obj->field_C   |= 0x80;
        work->field_412 = 0;
        work->field_420++;
    }
}
