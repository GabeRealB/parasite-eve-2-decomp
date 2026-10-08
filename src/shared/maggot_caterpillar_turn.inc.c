/* Part of the Maggot and Caterpillar library; see maggot_caterpillar.h. */

/// Turns the root toward its requested heading and replaces its rotation with level yaw.
///
/// Headings use 4096 units per turn; `turnRate` is a nonnegative angular step.
/// The target must be in the same one-turn domain as the matrix-derived yaw.
/// The difference narrows to s16 before choosing the shorter arc; an exact
/// half-turn takes the wrapped arc. Translation is retained, and the caller
/// must mark the coordinate dirty and recompose it after this step.
static void _maggotCaterpillarTurnStep(Task* actor)
{
    MaggotCaterpillarWork* work;
    GfxCoord*              coord;
    ActorFaceScratch*      turnScratch;
    s32                    currentYaw;
    u16                    targetYaw;
    s16                    yawDelta;
    s32                    absoluteYawDelta;
    s32                    turnRate;
    s32                    previousYaw;
    s32                    nextYaw;
    s32                    wrappedTurnRate;

    turnScratch      = SCRATCH_STACK_RESERVE_BLOCK(ActorFaceScratch);
    coord            = actor->extra.tmd->coords;
    work             = actor->work;
    currentYaw       = ratan2(coord->coord.m[0][2], coord->coord.m[2][2]) & ACTOR_TRANSFORM_ANGLE_MASK;
    targetYaw        = work->targetYaw;
    yawDelta         = targetYaw - currentYaw;
    absoluteYawDelta = yawDelta >= 0 ? yawDelta : -yawDelta;

    work->yaw = currentYaw;
    if (absoluteYawDelta < ACTOR_TRANSFORM_ANGLE_HALF_TURN) {
        turnRate = work->turnRate;
        if (turnRate >= absoluteYawDelta) {
            work->yaw = targetYaw;
        } else {
            nextYaw = work->yaw;
            if (yawDelta <= 0) {
                nextYaw -= turnRate;
            } else {
                nextYaw += turnRate;
            }
            work->yaw = nextYaw;
        }
    } else {
        turnRate = work->turnRate;
        if (yawDelta > 0 ? turnRate >= ACTOR_TRANSFORM_ANGLE_TURN - yawDelta : turnRate >= ACTOR_TRANSFORM_ANGLE_TURN + yawDelta) {
            work->yaw = work->targetYaw;
        } else {
            wrappedTurnRate = work->turnRate;
            previousYaw     = work->yaw;
            if (yawDelta > 0) {
                work->yaw = previousYaw - wrappedTurnRate;
            } else {
                work->yaw = previousYaw + wrappedTurnRate;
            }
        }
    }
    turnScratch->rot.vx = 0;
    turnScratch->rot.vy = work->yaw;
    turnScratch->rot.vz = 0;
    RotMatrix(&turnScratch->rot, &coord->coord);
    SCRATCH_STACK_RELEASE_BLOCK(ActorFaceScratch);
}
