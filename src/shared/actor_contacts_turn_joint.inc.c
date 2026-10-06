/* Part of the actor contacts library; see actor_contacts.h. */

/// Turns joint `coord` by `yaw` about the world Y axis: builds its world
/// rotation in a matrix carved off the scratchpad head, applies the turn,
/// converts the result back into the parent's frame, writes the 3x3 into the
/// joint and refreshes it.
static void ActorContact_TurnJoint(GfxCoord* coord, s16 yaw)
{
    MATRIX* rotation;

    SCRATCH_STACK_RESERVE_BLOCK(MATRIX);
    rotation = SCRATCH_STACK_CURSOR(MATRIX);
    actorAccumulateRotation(coord, rotation, &gGfxViewCoord);
    RotMatrixY(yaw, rotation);
    _actorRenderLocalizeRotation(coord, rotation);
    memcpy(coord->coord.m, rotation->m, sizeof(coord->coord.m));
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(coord);
    SCRATCH_STACK_RELEASE_BLOCK(MATRIX);
}
