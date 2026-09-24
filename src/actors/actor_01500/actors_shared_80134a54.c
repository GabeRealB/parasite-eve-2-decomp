#include "common.h"

#include "actors/actor_101500.h"
#include "actors/actors_shared_80135b58.h"
#include "main/mem.h"

MATRIX* ScaleMatrix(MATRIX* m, VECTOR* v);
MATRIX* MulMatrix(MATRIX* m0, MATRIX* m1);

/// Death shrink: restores the root coordinate from the matrix `field_32C`
/// saved when the death sequence began and squashes it along Y by
/// `field_368`, which winds down by 0x50 a frame until it reaches 0x200. The
/// scale is applied through an identity rotation carved off the scratchpad,
/// `ScaleMatrix` and `MulMatrix`, and `flg` is cleared so the coordinate's work
/// matrix is rebuilt.
void Actor01500_Fn02C34(Actor101500* arg0)
{
    GsCOORDINATE2*              coord;
    MATRIX*                     head;
    ActorShared80135b58Scratch* scratch;
    Actor101500Work*            work;

    head                    = *(MATRIX**)G_SCRATCH_HEAD;
    work                    = arg0->field_1C;
    scratch                 = (ActorShared80135b58Scratch*)((u8*)head - 0x30);
    *(void**)G_SCRATCH_HEAD = scratch;
    coord                   = arg0->field_2C->coords;
    if (work->field_368 >= 0x201) {
        work->field_368 = (u16)work->field_368 - 0x50;
    }
    scratch->scale.vx          = 0x1000;
    scratch->scale.vy          = (s32)work->field_368;
    scratch->scale.vz          = 0x1000;
    coord->coord               = work->field_32C;
    scratch->mat.ident.m00_m01 = 0x1000;
    scratch->mat.ident.m02_m10 = 0;
    scratch->mat.ident.m11_m12 = 0x1000;
    scratch->mat.ident.m20_m21 = 0;
    scratch->mat.ident.m22     = 0x1000;
    ScaleMatrix(&scratch->mat.mat, &scratch->scale);
    MulMatrix(&coord->coord, &scratch->mat.mat);
    coord->flg             = 0;
    *(u8**)G_SCRATCH_HEAD += 0x30;
}
