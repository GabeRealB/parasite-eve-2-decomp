/* Part of the Moth library; see moth.h. */

/// Rebuilds the living moth's root from its wandering or pursuing heading and pitch.
///
/// Angles use 4096 units per turn. Unalerted yaw steps randomly by 0..31;
/// alerted yaw turns toward the player's parent-frame X/Z offset by 16,
/// snapping within 16. Pitch steps randomly by 0..63 and clamps to +/-256.
/// Player offsets narrow to signed halfwords. The wrapped difference is used
/// only for its turn sign; composition is invalidated by the living update.
/// One face scratch block is released before return.
static void _mothSteer(Task* task)
{
    enum {
        MOTH_PURSUIT_TURN_STEP       = 16,
        MOTH_PITCH_LIMIT             = 256,
        MOTH_YAW_STEP_MASK           = 31,
        MOTH_YAW_POSITIVE_STEP_BIT   = 32,
        MOTH_PITCH_STEP_MASK         = 63,
        MOTH_PITCH_POSITIVE_STEP_BIT = 64
    };

    MothWork*         work;
    GfxCoord*         rootCoord;
    ActorFaceScratch* scratch;
    s32               yawDraw;
    s32               yawStep;
    s32               previousYaw;
    s32               pursuitYaw;
    s32               previousPitch;
    s32               pitchDraw;
    s32               pitchStep;
    u16               playerYaw;
    s16               yawDifference;
    s32               yawDistance;
    s16               turnDirection;
    s16               wrappedDifference;

    scratch   = SCRATCH_STACK_RESERVE_BLOCK(ActorFaceScratch);
    work      = task->work;
    rootCoord = task->extra.tmd->coords;
    switch (work->alerted) {
        case false:
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            yawDraw         = gRandomLcgState >> 16;
            yawStep         = yawDraw & MOTH_YAW_STEP_MASK;
            previousYaw     = work->yaw;
            work->yaw       = !(yawDraw & MOTH_YAW_POSITIVE_STEP_BIT) ? previousYaw - yawStep : previousYaw + yawStep;
            break;
        case true:
            scratch->delta.vx = gPlayerStatus.coordMtx->t[0] - rootCoord->coord.t[0];
            scratch->delta.vy = 0;
            scratch->delta.vz = gPlayerStatus.coordMtx->t[2] - rootCoord->coord.t[2];
            playerYaw         = ratan2((s16)scratch->delta.vx, (s16)scratch->delta.vz) & ACTOR_TRANSFORM_ANGLE_MASK;
            yawDifference     = playerYaw - (work->yaw & ACTOR_TRANSFORM_ANGLE_MASK);
            yawDistance       = yawDifference >= 0 ? yawDifference : -yawDifference;
            turnDirection     = yawDifference;
            if (yawDistance < MOTH_PURSUIT_TURN_STEP + 1) {
                work->yaw = playerYaw;
            } else {
                if (yawDistance >= ACTOR_TRANSFORM_ANGLE_HALF_TURN + 1) {
                    wrappedDifference = yawDifference - ACTOR_TRANSFORM_ANGLE_TURN;
                    if (yawDifference <= 0)
                        wrappedDifference = ACTOR_TRANSFORM_ANGLE_TURN - yawDifference;
                    turnDirection = wrappedDifference;
                }
                pursuitYaw = work->yaw;
                if (turnDirection > 0) {
                    work->yaw = pursuitYaw + MOTH_PURSUIT_TURN_STEP;
                } else {
                    work->yaw = pursuitYaw - MOTH_PURSUIT_TURN_STEP;
                }
            }
            break;
    }
    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    pitchDraw       = gRandomLcgState >> 16;
    pitchStep       = pitchDraw & MOTH_PITCH_STEP_MASK;
    previousPitch   = work->pitch;
    work->pitch     = !(pitchDraw & MOTH_PITCH_POSITIVE_STEP_BIT) ? previousPitch - pitchStep : previousPitch + pitchStep;
    if (work->pitch > MOTH_PITCH_LIMIT) {
        work->pitch = MOTH_PITCH_LIMIT;
    } else if (work->pitch < -MOTH_PITCH_LIMIT) {
        work->pitch = -MOTH_PITCH_LIMIT;
    }
    scratch->rot.vx = work->pitch;
    scratch->rot.vy = work->yaw;
    scratch->rot.vz = 0;
    RotMatrix(&scratch->rot, &rootCoord->coord);
    SCRATCH_STACK_RELEASE_BLOCK(ActorFaceScratch);
}
