#include "common.h"

#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/3A34.h"

#include "actors/actor_342400.h"
#include "actors/actors_shared_8016bd98.h"

MATRIX* ScaleMatrix(MATRIX* m, VECTOR* v);
MATRIX* MulMatrix(MATRIX* m0, MATRIX* m1);

/// Death shrink without the effect of `func_actor_342400_80166180`:
/// restores the saved root matrix, scales it on Y by `field_430` (0x40
/// smaller each frame), sets light mode 2 on frame 16, and after frame 32
/// hides the model, clears the frame counter and advances the state.
void func_actor_342400_8016BD98(Task* arg0)
{
    Actor342400Work*              work;
    TmdObject*                    obj;
    GsCOORDINATE2*                coord;
    VECTOR                        scale;
    ActorsShared8016bd98Mat       m;
    ActorsShared8016bd98MatWords* ident;

    work             = (Actor342400Work*)arg0->work;
    ident            = &m.ident;
    obj              = arg0->extra;
    coord            = obj->coords;
    work->field_430 -= 0x40;
    scale.vx         = 0x1000;
    scale.vy         = (s16)work->field_430;
    scale.vz         = 0x1000;
    coord->coord     = work->savedRootMtx;
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
        obj->flags     |= 0x80;
        work->field_412 = 0;
        work->field_420++;
    }
}
