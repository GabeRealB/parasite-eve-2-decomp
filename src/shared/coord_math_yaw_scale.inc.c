/* Part of the coord math library; see coord_math.h. */

/// Rebuilds `coord`'s rotation as its current yaw alone, uniformly scaled by
/// `scale`, working in a block borrowed from the scratch stack, and marks the
/// coordinate dirty.
void coordSetYawScale(GfxCoord* coord, s16 scale)
{
    void**                scratch;
    ActorScaleRotScratch* head;
    ActorScaleRotScratch* blk;
    s16                   ang;
    u16                   m22;

    scratch                                        = SCRATCH_HEAD_ADDR;
    head                                           = SCRATCH_HEAD_AT(scratch, ActorScaleRotScratch);
    blk                                            = head - 1;
    SCRATCH_HEAD_AT(scratch, ActorScaleRotScratch) = blk;

    ang      = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    blk->yaw = ang;
    gfxRotMatrixY(&blk->rotation, ang, 1);
    blk->scale.vz = scale;
    blk->scale.vy = scale;
    blk->scale.vx = scale;
    ScaleMatrix(&blk->rotation, &blk->scale);

    coord->coord.m[0][0] = (u16)(head - 1)->rotation.m[0][0];
    coord->coord.m[0][1] = (u16)blk->rotation.m[0][1];
    coord->coord.m[0][2] = (u16)blk->rotation.m[0][2];
    coord->coord.m[1][0] = (u16)blk->rotation.m[1][0];
    coord->coord.m[1][1] = (u16)blk->rotation.m[1][1];
    coord->coord.m[1][2] = (u16)blk->rotation.m[1][2];
    coord->coord.m[2][0] = (u16)blk->rotation.m[2][0];
    coord->coord.m[2][1] = (u16)blk->rotation.m[2][1];
    m22                  = (u16)blk->rotation.m[2][2];
    SCRATCH_POP_AT(scratch, ActorScaleRotScratch);
    coord->composeStamp  = GRAPHICS_COORD_DIRTY;
    coord->coord.m[2][2] = m22;
}
