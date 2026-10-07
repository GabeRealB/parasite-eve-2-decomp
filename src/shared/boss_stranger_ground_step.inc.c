/* Part of the Boss Stranger library; see boss_stranger.h. */

/// Adjusts an initialized halfword move component by the sign of a 16.16 fraction.
///
/// `moveComponent` must be a modifiable halfword lvalue; it is read and written
/// only when the fraction is nonzero. `fixedDelta` is a signed 32-bit 16.16
/// value and is evaluated up to twice. Both arguments must have no side effects.
/// The block captures no identifiers and preserves the negative extra unit.
/// Expands to a block; place the statement within braces in control-flow bodies.
#define BOSS_STRANGER_GROUND_ADJUST_FRACTION(moveComponent, fixedDelta) \
    {                                                                   \
        enum { BOSS_STRANGER_GROUND_FRACTION_MASK = 0xFFFF };           \
        if (((fixedDelta) & BOSS_STRANGER_GROUND_FRACTION_MASK) != 0) { \
            if ((fixedDelta) > 0) {                                     \
                (moveComponent)++;                                      \
            } else {                                                    \
                (moveComponent)--;                                      \
            }                                                           \
        }                                                               \
    }

/// Applies the walker's collision correction and vertical fall in game-coordinate units.
///
/// Takes each signed 16.16 delta's integer half and, for a nonzero fractional
/// half, adds one in the delta's sign. Positive fractions round up; negative
/// fractions step one beyond floor (-0.5 becomes -2). XZ replace the scratch
/// move components, but a resolved, unlocked Y adds onto the uninitialized
/// scratch halfword. No-hit clears XYZ. `lockHeight` suppresses Y and the fall.
///
/// Adds 16 to unlocked Y, saves the whole `moveDelta` including untouched pad,
/// then applies XZ and banded Y: values above 32 apply +8, below -32 apply -32,
/// absolute values below 32 apply themselves, and exactly +/-32 apply nothing.
/// Sets `offOrigin` from the resulting local XZ translation. Does not dirty
/// the composition cache; the enclosing tick does so.
/// Requires a live coordinate, the resolver's contact contract, and initialized
/// scratch storage for the block and nested resolver. Retains no pointer.
static void _bossStrangerApplyGroundStep(BossStrangerWalker* walker)
{
    enum {
        BOSS_STRANGER_GROUND_FALL_STEP       = 16, // Positive Y points down
        BOSS_STRANGER_GROUND_VERTICAL_BAND   = 32,
        BOSS_STRANGER_GROUND_POSITIVE_Y_STEP = 8
    };
    BossStrangerGroundStepScratch* scratchEnd;
    BossStrangerGroundStepScratch* scratch;
    s32                            fixedX;
    s32                            fixedY;
    s32                            fixedZ;
    s32                            integerX;
    s32                            integerY;
    s32                            integerZ;
    s32                            verticalStep;

    scratchEnd = SCRATCH_STACK_CURSOR(BossStrangerGroundStepScratch);
    scratch    = SCRATCH_STACK_RESERVE_BLOCK(BossStrangerGroundStepScratch);
    // Convert the resolver correction without clearing the reserved move block.
    if (worldCollisionResolvePushback(walker->recs, &scratch->delta, walker->recCount, NULL) != WORLD_COLLISION_PUSHBACK_NO_GRID_HIT) {
        integerX         = scratchEnd[-1].delta.fixed.vx.halves.integer;
        integerZ         = scratch->delta.fixed.vz.halves.integer;
        scratch->move.vx = integerX;
        scratch->move.vz = integerZ;
        fixedX           = scratchEnd[-1].delta.fixed.vx.word;
        BOSS_STRANGER_GROUND_ADJUST_FRACTION(scratch->move.vx, fixedX);
        fixedZ = scratch->delta.fixed.vz.word;
        BOSS_STRANGER_GROUND_ADJUST_FRACTION(scratch->move.vz, fixedZ);
        if (walker->lockHeight == 0) {
            integerY         = scratch->delta.fixed.vy.halves.integer;
            fixedY           = scratch->delta.fixed.vy.word;
            scratch->move.vy = scratch->move.vy + integerY;
            BOSS_STRANGER_GROUND_ADJUST_FRACTION(scratch->move.vy, fixedY);
        } else {
            scratch->move.vy = 0;
        }
    } else {
        scratch->move.vx = 0;
        scratch->move.vy = 0;
        scratch->move.vz = 0;
    }
    if (walker->lockHeight == 0) {
        scratch->move.vy += BOSS_STRANGER_GROUND_FALL_STEP;
    }
    // Preserve the full move record, then apply the retained vertical bands.
    walker->moveDelta          = scratch->move;
    walker->coord->coord.t[0] += scratch->move.vx;
    if (scratch->move.vy > BOSS_STRANGER_GROUND_VERTICAL_BAND) {
        walker->coord->coord.t[1] += BOSS_STRANGER_GROUND_POSITIVE_Y_STEP;
    }
    if (scratch->move.vy < -BOSS_STRANGER_GROUND_VERTICAL_BAND) {
        walker->coord->coord.t[1] -= BOSS_STRANGER_GROUND_VERTICAL_BAND;
    }
    verticalStep = scratch->move.vy;
    if (ABS(verticalStep) < BOSS_STRANGER_GROUND_VERTICAL_BAND) {
        walker->coord->coord.t[1] += verticalStep;
    }
    walker->coord->coord.t[2] += scratch->move.vz;
    if (walker->coord->coord.t[0] != 0 || walker->coord->coord.t[2] != 0) {
        walker->offOrigin = 1;
    } else {
        walker->offOrigin = 0;
    }
    SCRATCH_STACK_RELEASE_BLOCK(BossStrangerGroundStepScratch);
}

#undef BOSS_STRANGER_GROUND_ADJUST_FRACTION
