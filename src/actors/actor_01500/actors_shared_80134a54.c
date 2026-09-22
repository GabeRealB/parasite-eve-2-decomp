#include "common.h"

#include "actors/actors_shared_80134a54.h"
#include "actors/actors_shared_80135b58.h"

#include "main/mem.h"

MATRIX* ScaleMatrix(MATRIX* m, VECTOR* v);
MATRIX* MulMatrix(MATRIX* m0, MATRIX* m1);

/// Rescales the actor's attach coordinate through 0x30 bytes borrowed from the
/// scratchpad and released again: an identity rotation carrying `field_368` on
/// its Y axis supplies the scale, the work block's `field_32C` matrix is copied
/// into the coordinate first, and `ScaleMatrix` + `MulMatrix` apply the scale
/// to that copy in place. `field_368` is wound down by 0x50 a frame once it
/// passes 0x200. `flg` is cleared so the coordinate's own work matrix is
/// rebuilt from `coord` next frame.
///
/// Shared verbatim by `actor_101500`, `actor_201500` and `actor_301500`.
void ActorsShared80134a54(ActorShared80134a54* arg0)
{
    GsCOORDINATE2*              coord;
    MATRIX*                     head;
    ActorShared80135b58Scratch* scratch;
    ActorShared80134a54Work*    work;

    head                    = *(MATRIX**)G_SCRATCH_HEAD;
    work                    = arg0->field_1C;
    scratch                 = (ActorShared80135b58Scratch*)((u8*)head - 0x30);
    *(void**)G_SCRATCH_HEAD = scratch;
    coord                   = arg0->field_2C->field_8;
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
