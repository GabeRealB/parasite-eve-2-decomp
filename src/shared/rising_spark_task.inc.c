/* Part of the rising spark library; see rising_spark.h. */

/// Moves the spark along its coordinate parent's Y axis and refreshes its composed transform.
///
/// Borrows the live coordinate for this call. `risePerTick` is a signed
/// displacement in parent-coordinate units; the translation sum must fit s32.
static inline void _risingSparkMoveCoord(GfxCoord* coord, s16 risePerTick)
{
    s32 nextY;

    nextY               = coord->coord.t[1] + risePerTick;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    coord->coord.t[1]   = nextY;
    actorRenderComposeCoord(coord);
}

/// Advances the short-lived spark billboard shared by Healing and Life Drain.
///
/// Requires a counted effect task with a coordinate body and an owned,
/// cleared `EffectWork` in `spawnArg2.pointer`, as `Gp_SpawnEff` supplies.
/// The coordinate's parent chain must remain live until teardown.
/// `spawnArg1.value` bits 0..11 select the sizing numerator (0..4095) passed
/// to `effectDrawSpinningBillboard`; higher bits are ignored. `angle` holds
/// that size, `scale` holds a fixed random screen rotation (4096 units per
/// turn), and `period` holds palette index 1 encoded in bits 12..15.
///
/// The initialization tick advances `age` to one without moving or drawing.
/// Active ticks add four parent-coordinate units to local Y, advance `index`
/// on even ages and draw on odd ages. From cleared work, only texture frames
/// 1..7 are drawn, at ages 3..15; age 16 moves once more and releases the work
/// and task when the index reaches eight. Parent-task teardown may end them
/// earlier. The task performs no room-control pause or cancellation check.
static inline void _risingSparkTask(Task* task)
{
    enum {
        RISING_SPARK_STATE_INITIALIZE = 0,
        RISING_SPARK_STATE_ANIMATE    = 1,
        RISING_SPARK_RISE_PER_TICK    = 4,
        RISING_SPARK_ROTATION_MASK    = 0xFFF,
        RISING_SPARK_SIZE_MASK        = 0xFFF,
        RISING_SPARK_PALETTE_SHIFT    = 12,
        RISING_SPARK_PALETTE_INDEX    = 1,
        RISING_SPARK_FRAME_COUNT      = 8,
    };
    EffectWork* work;
    GfxCoord*   coord;
    u16         spawnSize;

    work      = task->spawnArg2.pointer;
    coord     = task->extra.coordBody->coord;
    work->age = work->age + 1;
    switch (task->state) {
        case RISING_SPARK_STATE_INITIALIZE:
            work->move.vy   = RISING_SPARK_RISE_PER_TICK;
            work->move.vx   = 0;
            work->move.vz   = 0;
            task->state     = RISING_SPARK_STATE_ANIMATE;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->scale     = (gRandomLcgState >> 16) & RISING_SPARK_ROTATION_MASK;
            spawnSize       = task->spawnArg1.halves.low;
            work->period    = RISING_SPARK_PALETTE_INDEX << RISING_SPARK_PALETTE_SHIFT;
            work->angle     = spawnSize & RISING_SPARK_SIZE_MASK;
            return;
        case RISING_SPARK_STATE_ANIMATE:
            _risingSparkMoveCoord(coord, work->move.vy);
            // Animation steps and draws use opposite age parities.
            if (!(work->age & 1)) {
                work->index = work->index + 1;
            }
            if (work->index < RISING_SPARK_FRAME_COUNT) {
                if (work->age & 1) {
                    effectDrawSpinningBillboard(coord, work->index, work->angle,
                                                work->scale | work->period);
                    return;
                }
            } else {
                effectKillTask(work, task);
                return;
            }
            break;
    }
}
