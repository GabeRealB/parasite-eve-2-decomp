/* Part of the Moth library; see moth.h. */

/// Saves the previous root position, then wanders or advances toward the player.
///
/// Positions and speeds are game-coordinate units in the roots' common parent
/// frame. Wandering reverses signed-halfword candidate X/Y steps outside open
/// home bands of +/-200 and +/-500; Z instead tests the signed random draw
/// against home Z +/-200, retaining that distinct behavior. Alerted flight
/// uses rowIndex 0..7 to select speed, adds 0..31 and follows the Q12 forward
/// axis. Height tends toward 1200 above the player within a +/-400 band;
/// the target height narrows to s16. Fast flapping biases Y upward.
static void _mothDrift(Task* task)
{
    enum {
        MOTH_WANDER_HORIZONTAL_HALF_EXTENT = 200,
        MOTH_WANDER_VERTICAL_HALF_EXTENT   = 500,
        MOTH_PURSUIT_HEIGHT_ABOVE_PLAYER   = 1200,
        MOTH_PURSUIT_HEIGHT_HALF_BAND      = 400,
        MOTH_FORWARD_BASIS_FRACTION_BITS   = 12,
        MOTH_FLIGHT_JITTER_MASK            = 31,
        MOTH_WANDER_POSITIVE_STEP_BIT      = 32,
        MOTH_PURSUIT_HEIGHT_STEP_MASK      = 15
    };

    MothWork* work;
    GfxCoord* rootCoord;
    u32       horizontalDraw;
    u32       heightDraw;
    u32       zDraw;
    s32       horizontalStep;
    s32       verticalStep;
    s16       wanderStep;
    s16       forwardSpeed;
    s32       currentY;
    s32       nextY;
    s16       targetY;

    work             = task->work;
    rootCoord        = task->extra.tmd->coords;
    work->prevPos.vx = rootCoord->coord.t[0];
    work->prevPos.vy = rootCoord->coord.t[1];
    work->prevPos.vz = rootCoord->coord.t[2];
    switch (work->alerted) {
        case false:
            horizontalDraw = (gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16;
            horizontalStep = horizontalDraw & MOTH_FLIGHT_JITTER_MASK;
            if (!(horizontalDraw & MOTH_WANDER_POSITIVE_STEP_BIT)) {
                horizontalStep = -horizontalStep;
            }
            wanderStep = horizontalStep;
            if ((s16)(rootCoord->coord.t[0] + wanderStep) < work->homePos.vx + MOTH_WANDER_HORIZONTAL_HALF_EXTENT &&
                work->homePos.vx - MOTH_WANDER_HORIZONTAL_HALF_EXTENT < (s16)(rootCoord->coord.t[0] + wanderStep)) {
                rootCoord->coord.t[0] += wanderStep;
            } else {
                rootCoord->coord.t[0] -= wanderStep;
            }
            verticalStep = ((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & MOTH_FLIGHT_JITTER_MASK;
            if (work->flapFast != 0) {
                verticalStep = -verticalStep;
            }
            wanderStep = verticalStep;
            if ((s16)(rootCoord->coord.t[1] + wanderStep) < work->homePos.vy + MOTH_WANDER_VERTICAL_HALF_EXTENT &&
                work->homePos.vy - MOTH_WANDER_VERTICAL_HALF_EXTENT < (s16)(rootCoord->coord.t[1] + wanderStep)) {
                rootCoord->coord.t[1] += wanderStep;
            } else {
                rootCoord->coord.t[1] -= wanderStep;
            }
            zDraw          = (gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16;
            horizontalStep = zDraw & MOTH_FLIGHT_JITTER_MASK;
            if (!(zDraw & MOTH_WANDER_POSITIVE_STEP_BIT)) {
                horizontalStep = -horizontalStep;
            }
            wanderStep = horizontalStep;
            // The retained Z reversal tests the draw, not the proposed position.
            if ((s16)zDraw < work->homePos.vz + MOTH_WANDER_HORIZONTAL_HALF_EXTENT && work->homePos.vz - MOTH_WANDER_HORIZONTAL_HALF_EXTENT < (s16)zDraw) {
                rootCoord->coord.t[2] += wanderStep;
            } else {
                rootCoord->coord.t[2] -= wanderStep;
            }
            break;
        case true:
            forwardSpeed = gMothSpeeds[((Enemy*)task->spawnArg2.pointer)->place->rowIndex] +
                           (((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & MOTH_FLIGHT_JITTER_MASK);
            rootCoord->coord.t[0] += (rootCoord->coord.m[0][2] * forwardSpeed) >> MOTH_FORWARD_BASIS_FRACTION_BITS;
            rootCoord->coord.t[2] += (rootCoord->coord.m[2][2] * forwardSpeed) >> MOTH_FORWARD_BASIS_FRACTION_BITS;
            targetY                = gPlayerStatus.coordMtx->t[1] - MOTH_PURSUIT_HEIGHT_ABOVE_PLAYER;
            heightDraw             = (gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16;
            currentY               = rootCoord->coord.t[1];
            if (currentY >= targetY + MOTH_PURSUIT_HEIGHT_HALF_BAND) {
                rootCoord->coord.t[1] = currentY - (heightDraw & MOTH_PURSUIT_HEIGHT_STEP_MASK);
            } else {
                if (targetY - MOTH_PURSUIT_HEIGHT_HALF_BAND >= currentY) {
                    nextY = currentY + (heightDraw & MOTH_PURSUIT_HEIGHT_STEP_MASK);
                } else {
                    verticalStep = heightDraw & MOTH_FLIGHT_JITTER_MASK;
                    if (work->flapFast != 0) {
                        nextY = currentY - verticalStep;
                    } else {
                        nextY = currentY + verticalStep;
                    }
                }
                rootCoord->coord.t[1] = nextY;
            }
            break;
    }
}
