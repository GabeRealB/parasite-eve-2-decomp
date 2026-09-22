#include "common.h"

#include "actors/actors_shared_80135098.h"

#include "main/mem.h"

MATRIX* ScaleMatrix(MATRIX* m, VECTOR* v);
MATRIX* MulMatrix(MATRIX* m0, MATRIX* m1);

/// Rescales the actor's attach coordinate through a 0x30-byte block borrowed
/// from the scratchpad and released again: the work block's `field_100` matrix
/// is copied into the coordinate, an identity rotation carrying `field_12A` on
/// its Y axis supplies the scale, and `ScaleMatrix` + `MulMatrix` apply it to
/// the copy in place. `flg` is cleared so the coordinate's own work matrix is
/// rebuilt from `coord` next frame.
///
/// Shared verbatim by `actor_102400` and `actor_202400`.
void ActorsShared80135098(ActorShared80135098* arg0)
{
    void**                      scratch;
    void*                       head;
    ActorShared80135098Scratch* blk;
    ActorShared80135098Work*    work;
    GsCOORDINATE2*              coord;

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    blk      = (ActorShared80135098Scratch*)((u8*)head - 0x30);
    *scratch = blk;
    coord    = arg0->field_2C->field_8;
    work     = arg0->field_1C;

    blk->scale.vx          = 0x1000;
    blk->scale.vy          = work->field_12A;
    blk->scale.vz          = 0x1000;
    coord->coord           = work->field_100;
    blk->mat.ident.m00_m01 = 0x1000;
    blk->mat.ident.m02_m10 = 0;
    blk->mat.ident.m11_m12 = 0x1000;
    blk->mat.ident.m20_m21 = 0;
    blk->mat.ident.m22     = 0x1000;
    ScaleMatrix(&blk->mat.mat, &blk->scale);
    MulMatrix(&coord->coord, &blk->mat.mat);
    coord->flg = 0;
    *scratch   = (u8*)*scratch + 0x30;
}
