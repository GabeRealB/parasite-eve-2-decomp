#include "common.h"

#include "main/mem.h"

#include "actors/actors_shared_80136574.h"

MATRIX* ScaleMatrix(MATRIX* m, VECTOR* v);
MATRIX* MulMatrix(MATRIX* m0, MATRIX* m1);

/// Multiplies `arg1` into the coordinate `arg0->field_2C` points at, scaled by
/// `arg2`: through a 0x30-byte block borrowed from the scratchpad and released
/// again, an identity rotation is splatted word-wise and `ScaleMatrix` scales
/// it. `arg3` selects the axis layout - non-zero scales all three axes by
/// `arg2`, zero leaves X and Z at 0x1000 and scales only Y.
///
/// The scratchpad head is deliberately written twice, from two separate
/// computations of `head - 0x30`. CSE cannot substitute a value that holds no
/// register, so the store keeps the block-local `$v1` while `blk` - which
/// crosses both calls - is copied into `$s0` by `reload_cse_regs`. Folding the
/// two into one variable allocates `blk`'s register for the store as well and
/// loses the copy, the delay-slot fill and the frame layout.
void ActorsShared80136574(ActorShared80136574* arg0, MATRIX* arg1, s16 arg2, s32 arg3)
{
    void*                       head;
    ActorShared80136574Scratch* blk;
    GsCOORDINATE2*              coord;

    head                    = *(void**)G_SCRATCH_HEAD;
    *(void**)G_SCRATCH_HEAD = (u8*)head - 0x30;
    blk                     = (ActorShared80136574Scratch*)((u8*)head - 0x30);
    coord                   = arg0->field_2C->field_8;

    if (arg3 == 0) {
        blk->scale.vx = 0x1000;
        blk->scale.vy = arg2;
        blk->scale.vz = 0x1000;
    } else {
        blk->scale.vx = arg2;
        blk->scale.vy = arg2;
        blk->scale.vz = arg2;
    }

    coord->coord = *arg1;

    blk->mat.ident.m00_m01 = 0x1000;
    blk->mat.ident.m02_m10 = 0;
    blk->mat.ident.m11_m12 = 0x1000;
    blk->mat.ident.m20_m21 = 0;
    blk->mat.ident.m22     = 0x1000;

    ScaleMatrix(&blk->mat.mat, &blk->scale);
    MulMatrix(&coord->coord, &blk->mat.mat);
    coord->flg              = 0;
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x30;
}
