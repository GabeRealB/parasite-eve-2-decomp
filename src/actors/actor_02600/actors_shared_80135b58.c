#include "common.h"

#include "main/mem.h"

#include "actors/actors_shared_80135b58.h"

MATRIX* ScaleMatrix(MATRIX* m, VECTOR* v);
MATRIX* MulMatrix(MATRIX* m0, MATRIX* m1);

/// Folds a uniform 1/16 scale into the model's third coordinate node, through a
/// 0x30-byte block borrowed from the scratchpad and released again: an identity
/// rotation is splatted word-wise, `ScaleMatrix` shrinks its diagonal to 0x100,
/// and `MulMatrix` multiplies the result into `field_8[2].coord`.
void ActorsShared80135b58(ActorShared80135b58* actor)
{
    void**                      scratch;
    void*                       head;
    ActorShared80135b58Scratch* blk;
    GsCOORDINATE2*              coord;

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    blk      = (ActorShared80135b58Scratch*)((u8*)head - 0x30);
    *scratch = blk;
    coord    = actor->field_2C->field_8;

    blk->scale.vx          = 0x100;
    blk->scale.vy          = 0x100;
    blk->scale.vz          = 0x100;
    blk->mat.ident.m00_m01 = 0x1000;
    blk->mat.ident.m02_m10 = 0;
    blk->mat.ident.m11_m12 = 0x1000;
    blk->mat.ident.m20_m21 = 0;
    blk->mat.ident.m22     = 0x1000;
    ScaleMatrix(&blk->mat.mat, &blk->scale);
    MulMatrix(&coord[2].coord, &blk->mat.mat);
    *scratch = (u8*)*scratch + 0x30;
}
