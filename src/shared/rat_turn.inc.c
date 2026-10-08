/* Part of the Rat library; see rat.h. */

/// Turns the root toward its target heading and rebuilds a yaw-only rotation.
///
/// Requires live work/model, a target yaw in 0..4095 and a nonnegative turn
/// rate in angle units per update. The current heading is read from the local
/// matrix; the shortest arc is used and a step reaching the target snaps to it.
/// Translation is retained; pitch, roll and prior scale are replaced. The
/// caller owns invalidation/composition. Releases `ActorFaceScratch` on return.
static void _ratTurn(Task* actor)
{
    RatWork*          work;
    GfxCoord*         rootCoord;
    ActorFaceScratch* scratch;
    s32               currentYaw;
    u16               targetYaw;
    s16               yawDelta;
    s32               absYawDelta;
    s32               turnRate;
    s32               wrappedYaw;
    s32               nextYaw;
    s32               wrappedTurnRate;

    scratch     = SCRATCH_STACK_RESERVE_BLOCK(ActorFaceScratch);
    rootCoord   = actor->extra.tmd->coords;
    work        = actor->work;
    currentYaw  = ratan2(rootCoord->coord.m[0][2], rootCoord->coord.m[2][2]) & ACTOR_TRANSFORM_ANGLE_MASK;
    targetYaw   = work->targetYaw;
    yawDelta    = targetYaw - currentYaw;
    absYawDelta = yawDelta >= 0 ? yawDelta : -yawDelta;

    work->yaw = currentYaw;
    if (absYawDelta < ACTOR_TRANSFORM_ANGLE_HALF_TURN) {
        turnRate = work->turnRate;
        if (turnRate >= absYawDelta) {
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
            wrappedYaw      = work->yaw;
            if (yawDelta > 0) {
                work->yaw = wrappedYaw - wrappedTurnRate;
            } else {
                work->yaw = wrappedYaw + wrappedTurnRate;
            }
        }
    }
    // Rebuild from yaw alone, retaining translation and replacing pitch/roll/scale.
    scratch->rot.vx = 0;
    scratch->rot.vy = work->yaw;
    scratch->rot.vz = 0;
    RotMatrix(&scratch->rot, &rootCoord->coord);
    SCRATCH_STACK_RELEASE_BLOCK(ActorFaceScratch);
}
