/* Part of the player detection library; see player_detection.h. */

/// Returns 1 when the player is outside the stopping region for a proposed step.
///
/// The live player and `actorCoord` must have roots in the same parent frame.
/// `stopDistance` and signed `forwardStep` use game coordinate units. A
/// nonnegative step returns 1 immediately for a player bearing outside the
/// forward half-plane; a negative step does so inside that half-plane.
/// Bearings exactly a quarter turn away proceed to the distance test either way.
/// Otherwise returns whether the 3D distance from the stepped point to the
/// player is at least `stopDistance + 150`. This does not test obstacles.
///
/// Bearing offsets and the stepped point narrow to signed halfwords; the final
/// player position remains word-sized. Supply a nonzero local Z axis whose
/// squared length fits a positive signed word, and a final squared distance
/// in 0..0x7FFFFFFF. Neither root is changed or composed; GTE state is clobbered.
static s32 _playerDetectionOutOfReach(const GfxCoord* actorCoord, s16 stopDistance, s16 forwardStep)
{
    enum { PLAYER_DETECTION_REACH_MARGIN = 150 };
    SVECTOR  stepPoint;
    SVECTOR  toPlayer;
    VECTOR   remainingDelta;
    Task*    player;
    s16      playerTurn;
    SVECTOR* stepVector;
    s32      playerTurnHigh;

    // Choose the facing half-plane from the direction of the proposed step.
    player      = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    toPlayer.vx = (u16)player->extra.tmd->coords->coord.t[0] - (u16)actorCoord->coord.t[0];
    toPlayer.vy = (u16)player->extra.tmd->coords->coord.t[1] - (u16)actorCoord->coord.t[1];
    toPlayer.vz = (u16)player->extra.tmd->coords->coord.t[2] - (u16)actorCoord->coord.t[2];
    playerTurn  = _actorAngleNormalizeYaw(ratan2(toPlayer.vx, toPlayer.vz) - ratan2(-actorCoord->coord.m[2][0], actorCoord->coord.m[2][2]));
    // Keep the signed turn in the high halfword for the facing tests.
    playerTurnHigh = playerTurn << 16;
    if (forwardStep >= 0) {
        if (abs(playerTurnHigh >> 16) > ACTOR_TRANSFORM_ANGLE_TURN / 4) {
            return 1;
        }
    } else {
        if (abs(playerTurnHigh >> 16) < ACTOR_TRANSFORM_ANGLE_TURN / 4) {
            return 1;
        }
    }
    // Project the signed step along the normalized local Z axis, then measure clearance.
/// Builds a signed local-Z step point in parent-frame game units.
///
/// Captures the writable `stepVector` alias. Arguments must be side-effect-free:
/// `root` is a live coordinate, `step` a signed halfword, `point` an SVECTOR lvalue.
/// The point is written in place; arguments are evaluated repeatedly. Use only
/// as a standalone statement sequence in this function; GTE state is clobbered.
#define PLAYER_DETECTION_STEP_POINT(root, step, point) \
    gfxReadMatrixZAxis(&(root)->coord, &(point));      \
    stepVector = &(point);                             \
    VectorNormalSS(stepVector, stepVector);            \
    gte_lddp(step);                                    \
    gte_ldsv(stepVector);                              \
    gte_gpf12();                                       \
    gte_stsv(stepVector);                              \
    (point).vx += (u16)(root)->coord.t[0];             \
    (point).vy += (u16)(root)->coord.t[1];             \
    (point).vz += (u16)(root)->coord.t[2]
    PLAYER_DETECTION_STEP_POINT(actorCoord, forwardStep, stepPoint);
#undef PLAYER_DETECTION_STEP_POINT
    remainingDelta.vx = player->extra.tmd->coords->coord.t[0] - stepPoint.vx;
    remainingDelta.vy = player->extra.tmd->coords->coord.t[1] - stepPoint.vy;
    remainingDelta.vz = player->extra.tmd->coords->coord.t[2] - stepPoint.vz;
    return SquareRoot0(remainingDelta.vx * remainingDelta.vx + remainingDelta.vy * remainingDelta.vy + remainingDelta.vz * remainingDelta.vz) >= stopDistance + PLAYER_DETECTION_REACH_MARGIN;
}
