/* Part of the striker enemy library; see striker_enemy.h. */

/// Turns the joint `coord` by `yaw` about the world Y axis: accumulates its
/// rotation up to the view coordinate, turns it, expresses the result back in
/// the parent's frame and writes the 3x3 into the joint.  The working matrix
/// is one `MATRIX` taken off the scratchpad head for the duration.
void strikerTurnJoint(GfxCoord* coord, s16 yaw)
{
    MATRIX*   rotation;
    GfxCoord* out;

    SCRATCH_STACK_RESERVE_BLOCK(MATRIX);
    rotation = SCRATCH_STACK_CURSOR(MATRIX);
    strikerAccumulateRotation(coord, rotation, &gGfxViewCoord);
    RotMatrixY(yaw, rotation);
    out = strikerLocalizeRotation(coord, rotation);
    memcpy(out->coord.m, rotation->m, sizeof(out->coord.m));
    out->composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(out);
    SCRATCH_STACK_RELEASE_BLOCK(MATRIX);
}
