/* Part of the Pawn and Rook GOLEM library; see golem_pawn_rook.h. */

/// Rebuilds the body root's yaw while turning toward its requested heading.
///
/// Headings use 4096 units per turn; targetYaw is 0..4095 and turnRate is a
/// nonnegative angular step per call. The shorter arc crosses zero when needed;
/// the turn-about animation always steps in the negative direction unless already
/// close enough to snap. Replaces pitch, roll and scale but preserves translation.
/// The stored yaw can cross outside 0..4095 until the next call remeasures it.
/// The enclosing frame handler owns the root's composition-dirty mark.
static void _golemPawnRookTurnTowardTarget(Task* actor)
{
    enum { GOLEM_PAWN_ROOK_TURN_ANIM_ABOUT = 3 };
    GolemPawnRookWork* work;
    GfxCoord*          root;
    SVECTOR*           rotation;
    s32                currentYaw;
    u16                targetYaw;
    s16                yawDifference;
    s32                absoluteDifference;
    s32                turnRate;
    s32                unsignedTurnRate;
    s32                wrapTurnRate;
    s32                previousYaw;
    s32                nextYaw;
    s32                wrappedTurnRate;

    rotation           = SCRATCH_STACK_RESERVE_BLOCK(SVECTOR);
    root               = actor->extra.tmd->coords;
    work               = actor->work;
    currentYaw         = ratan2(root->coord.m[0][2], root->coord.m[2][2]) & ACTOR_TRANSFORM_ANGLE_MASK;
    targetYaw          = work->targetYaw;
    yawDifference      = targetYaw - currentYaw;
    absoluteDifference = yawDifference >= 0 ? yawDifference : -yawDifference;

    work->yaw = currentYaw;
    if (absoluteDifference < ACTOR_TRANSFORM_ANGLE_HALF_TURN) {
        turnRate         = work->turnRate;
        unsignedTurnRate = (u16)work->turnRate;
        if (turnRate >= absoluteDifference) {
            work->yaw = targetYaw;
        } else {
            if (work->anim == GOLEM_PAWN_ROOK_TURN_ANIM_ABOUT) {
                nextYaw = currentYaw - unsignedTurnRate;
            } else {
                nextYaw = work->yaw;
                if (yawDifference <= 0) {
                    nextYaw -= turnRate;
                } else {
                    nextYaw += turnRate;
                }
            }
            work->yaw = nextYaw;
        }
    } else {
        wrapTurnRate = work->turnRate;
        if (yawDifference > 0 ? wrapTurnRate >= ACTOR_TRANSFORM_ANGLE_TURN - yawDifference : wrapTurnRate >= ACTOR_TRANSFORM_ANGLE_TURN + yawDifference) {
            work->yaw = work->targetYaw;
        } else if (work->anim == GOLEM_PAWN_ROOK_TURN_ANIM_ABOUT) {
            work->yaw = (u16)work->yaw - (u16)work->turnRate;
        } else {
            wrappedTurnRate = work->turnRate;
            previousYaw     = work->yaw;
            if (yawDifference > 0) {
                work->yaw = previousYaw - wrappedTurnRate;
            } else {
                work->yaw = previousYaw + wrappedTurnRate;
            }
        }
    }
    rotation->vx = 0;
    rotation->vy = work->yaw;
    rotation->vz = 0;
    RotMatrix(rotation, &root->coord);
    SCRATCH_STACK_RELEASE_BLOCK(SVECTOR);
}
