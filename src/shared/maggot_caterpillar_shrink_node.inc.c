/* Part of the Maggot and Caterpillar library; see maggot_caterpillar.h. */

/// Shrinks the model's third coordinate node to 1/16 through an
/// `ActorScaleScratch` block taken from the scratch stack and released again:
/// an identity rotation is written word-wise, `ScaleMatrix` scales its
/// diagonal to 0x100 and `MulMatrix` multiplies it into `field_8[2].coord`.
void maggotCaterpillarShrinkNode2(Task* actor)
{
    void**             scratch;
    ActorScaleScratch* head;
    ActorScaleScratch* blk;
    GfxCoord*          coord;

    scratch                                     = SCRATCH_HEAD_ADDR;
    head                                        = SCRATCH_HEAD_AT(scratch, ActorScaleScratch);
    blk                                         = head - 1;
    SCRATCH_HEAD_AT(scratch, ActorScaleScratch) = blk;
    coord                                       = actor->extra.tmd->coords;

    blk->scale.vx                    = ONE / 16;
    blk->scale.vy                    = ONE / 16;
    blk->scale.vz                    = ONE / 16;
    blk->matrix.rotationWords.m00M01 = ONE;
    blk->matrix.rotationWords.m02M10 = 0;
    blk->matrix.rotationWords.m11M12 = ONE;
    blk->matrix.rotationWords.m20M21 = 0;
    blk->matrix.rotationWords.m22    = ONE;
    ScaleMatrix(&blk->matrix.mat, &blk->scale);
    MulMatrix(&coord[2].coord, &blk->matrix.mat);
    SCRATCH_POP_AT(scratch, ActorScaleScratch);
}
