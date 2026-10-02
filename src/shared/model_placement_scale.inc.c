/* Part of the model placement library; see model_placement.h. */

/// Sets the model's root coordinate to `arg1` scaled by `arg2`, and marks it
/// for recomputation. The scale is built in a 0x30-byte block borrowed from
/// the scratchpad: an identity rotation is written word-wise and
/// `ScaleMatrix` scales it, on all three axes when `arg3` is non-zero and on
/// Y alone when it is zero.
///
/// The scratchpad head is written twice, from two separate computations of
/// `head - 0x30`. CSE cannot substitute a value that holds no register, so
/// the store keeps the block-local `$v1` while `blk` - which crosses both
/// calls - is copied into `$s0` by `reload_cse_regs`. Folding the two into one
/// variable allocates `blk`'s register for the store as well and loses the
/// copy, the delay-slot fill and the frame layout.
void modelPlacementSetScaled(Task* arg0, MATRIX* arg1, s16 arg2, s32 arg3)
{
    ActorScaleScratch* head;
    ActorScaleScratch* blk;
    GfxCoord*          coord;

    head                                    = SCRATCH_STACK_CURSOR(ActorScaleScratch);
    SCRATCH_STACK_CURSOR(ActorScaleScratch) = head - 1;
    blk                                     = head - 1;
    coord                                   = arg0->extra.tmd->coords;

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

    blk->mat.rotationWords.m00M01 = ONE;
    blk->mat.rotationWords.m02M10 = 0;
    blk->mat.rotationWords.m11M12 = ONE;
    blk->mat.rotationWords.m20M21 = 0;
    blk->mat.rotationWords.m22    = ONE;

    ScaleMatrix(&blk->mat.mat, &blk->scale);
    MulMatrix(&coord->coord, &blk->mat.mat);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    SCRATCH_STACK_RELEASE_BLOCK(ActorScaleScratch);
}
