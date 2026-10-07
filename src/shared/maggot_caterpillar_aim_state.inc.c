/* Part of the Maggot/Caterpillar library; see maggot_caterpillar.h. */

/// Starts a pounce when the player lies close enough ahead of the actor.
///
/// The actor and player translations must share a parent frame. Distance uses
/// full-width X/Z coordinates; the bearing uses signed-halfword offsets. The
/// facing tolerance is 128 angle units, with 4096 units per turn. Reserves and
/// releases one vector on the scratch stack.
static void _maggotCaterpillarAimState(Task* actor)
{
    enum {
        MAGGOT_CATERPILLAR_AIM_FACING_TOLERANCE = 128,
    };
    MaggotCaterpillarWork* work;
    GfxCoord*              coord;
    s16                    signedTurn;
    s32                    turnMagnitude;
    s16                    wrappedTurn;
    s16                    facingError;
    s32                    playerDistance;
    s32                    playerOffsetX;
    s32                    playerOffsetZ;
    VECTOR*                delta;
    VECTOR*                scratchEnd;

    scratchEnd                   = SCRATCH_STACK_CURSOR(VECTOR);
    coord                        = actor->extra.tmd->coords;
    delta                        = scratchEnd - 1;
    SCRATCH_STACK_CURSOR(VECTOR) = delta;
    work                         = actor->work;
    work->yaw                    = ratan2((s32)coord->coord.m[0][2], (s32)coord->coord.m[2][2]) & (ONE - 1);
    scratchEnd[-1].vx            = (s32)(gPlayerStatus.coordMtx->t[0] - coord->coord.t[0]);
    delta->vy                    = 0;
    playerOffsetZ                = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
    delta->vz                    = playerOffsetZ;
    playerOffsetX                = scratchEnd[-1].vx;
    playerDistance               = SquareRoot0((playerOffsetX * playerOffsetX) + (playerOffsetZ * playerOffsetZ));
    signedTurn                   = work->yaw - (ratan2((s32)(s16)scratchEnd[-1].vx, (s32)(s16)delta->vz) & (ONE - 1));
    turnMagnitude                = __builtin_abs((s32)signedTurn);
    if (turnMagnitude < (ONE / 2)) {
        facingError = turnMagnitude;
    } else {
        if (signedTurn > 0) {
            wrappedTurn = ONE - signedTurn;
        } else {
            wrappedTurn = signedTurn + ONE;
        }
        facingError = wrappedTurn;
    }
    if ((playerDistance < MAGGOT_CATERPILLAR_POUNCE_RANGE) && (facingError < MAGGOT_CATERPILLAR_AIM_FACING_TOLERANCE)) {
        work->behaviour = MAGGOT_CATERPILLAR_BEHAVIOUR_POUNCE;
        work->step      = 0;
        work->animId    = MAGGOT_CATERPILLAR_ANIM_POUNCE;
        sceneEngageBattle(1);
    }
    SCRATCH_STACK_RELEASE_BLOCK(VECTOR);
}
