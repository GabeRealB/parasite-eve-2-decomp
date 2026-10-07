/* Part of the Sucklerceph library; see sucklerceph.h. */

/// Turns the crawling Sucklerceph toward the player by up to 32 angle units.
///
/// Requires live root/work storage and the player's root in the same parent
/// frame. Bearing uses the low signed halfwords of the X/Z offset, with 4096
/// angle units per turn. Deltas of at most 32 snap to the bearing; larger
/// deltas step the stored signed-halfword heading, retaining its wrap behavior.
/// Replaces pitch, roll and scale with the resulting yaw while keeping translation.
/// Releases one `ActorFaceScratch` block; the caller invalidates composition.
static void _sucklercephTurnToPlayer(Task* task)
{
    enum { SUCKLERCEPH_TURN_STEP = 32 };

    SucklercephWork*  work;
    GfxCoord*         rootCoord;
    ActorFaceScratch* turnScratch;
    s16               normalizedHeading;
    s16               nextHeading;
    s32               playerHeading;
    s16               headingDelta;
    s32               absoluteDelta;
    s16               turnDelta;
    s16               wrappedDelta;
    s32               heading;

    rootCoord             = task->extra.tmd->coords;
    work                  = task->work;
    turnScratch           = SCRATCH_STACK_RESERVE_BLOCK(ActorFaceScratch);
    turnScratch->delta.vx = gPlayerStatus.coordMtx->t[0] - rootCoord->coord.t[0];
    turnScratch->delta.vy = 0;
    turnScratch->delta.vz = gPlayerStatus.coordMtx->t[2] - rootCoord->coord.t[2];
    playerHeading         = ratan2((s16)turnScratch->delta.vx, (s16)turnScratch->delta.vz) & ACTOR_TRANSFORM_ANGLE_MASK;
    normalizedHeading     = work->heading & ACTOR_TRANSFORM_ANGLE_MASK;
    headingDelta          = playerHeading - normalizedHeading;
    absoluteDelta         = headingDelta >= 0 ? headingDelta : -headingDelta;
    turnDelta             = headingDelta;
    if (absoluteDelta < SUCKLERCEPH_TURN_STEP + 1) {
        work->heading = playerHeading;
    } else {
        if (absoluteDelta >= ACTOR_TRANSFORM_ANGLE_HALF_TURN + 1) {
            // Retain the asymmetric negative-delta wrap: turn selection uses its sign.
            wrappedDelta = headingDelta - ACTOR_TRANSFORM_ANGLE_TURN;
            if (headingDelta <= 0) {
                wrappedDelta = ACTOR_TRANSFORM_ANGLE_TURN - headingDelta;
            }
            turnDelta = wrappedDelta;
        }
        heading = work->heading;
        if (turnDelta <= 0) {
            nextHeading = heading - SUCKLERCEPH_TURN_STEP;
        } else {
            nextHeading = heading + SUCKLERCEPH_TURN_STEP;
        }
        work->heading = nextHeading;
    }
    turnScratch->rot.vx = 0;
    turnScratch->rot.vy = work->heading;
    turnScratch->rot.vz = 0;
    RotMatrix(&turnScratch->rot, &rootCoord->coord);
    SCRATCH_STACK_RELEASE_BLOCK(ActorFaceScratch);
}
