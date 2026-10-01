/* Part of the Maggot and Caterpillar library; see maggot_caterpillar.h. */

/// Shrinks the model's third coordinate node to 1/16 through a 0x30-byte
/// block taken from the scratchpad and released again: an identity rotation
/// is written word-wise, `ScaleMatrix` scales its diagonal to 0x100 and
/// `MulMatrix` multiplies it into `field_8[2].coord`.
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

    blk->scale.vx         = 0x100;
    blk->scale.vy         = 0x100;
    blk->scale.vz         = 0x100;
    blk->mat.ident.m00M01 = ONE;
    blk->mat.ident.m02M10 = 0;
    blk->mat.ident.m11M12 = ONE;
    blk->mat.ident.m20M21 = 0;
    blk->mat.ident.m22    = ONE;
    ScaleMatrix(&blk->mat.mat, &blk->scale);
    MulMatrix(&coord[2].coord, &blk->mat.mat);
    SCRATCH_POP_AT(scratch, ActorScaleScratch);
}
