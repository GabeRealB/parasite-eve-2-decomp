/* Part of the Diver library; see diver.h. */

/// Turns the joint `coord` by `yaw` about the world Y axis: accumulates its
/// rotation up to the view coordinate, turns it, expresses the result back in
/// the parent's frame and writes the 3x3 into the joint.  The working matrix
/// is one `MATRIX` taken off the scratchpad head for the duration.
void diverTurnJoint(GfxCoord* coord, s16 yaw)
{
    MATRIX*   rotation;
    GfxCoord* out;

    SCRATCH_STACK_RESERVE_BLOCK(MATRIX);
    rotation = SCRATCH_STACK_CURSOR(MATRIX);
    diverAccumulateRotation(coord, rotation, &gGfxViewCoord);
    RotMatrixY(yaw, rotation);
    out = diverLocalizeRotation(coord, rotation);
    memcpy(out->coord.m, rotation->m, sizeof(out->coord.m));
    out->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(out);
    SCRATCH_STACK_RELEASE_BLOCK(MATRIX);
}
