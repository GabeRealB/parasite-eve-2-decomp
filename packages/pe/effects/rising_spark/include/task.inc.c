/* Part of the rising spark library; see rising_spark.h. */

/// Moves a spark by `parentDeltaY` along its parent's Y axis and composes it.
static inline void _risingSparkMoveCoord(GfxCoord* coord, s32 parentDeltaY)
{
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    coord->coord.t[1]  += parentDeltaY;
    actorRenderComposeCoord(coord);
}

/// Moves a spark 4 units along Y each tick and plays its billboard animation at a random fixed rotation, then
/// ends; the low 12 bits of `spawnArg1` set its size, which `angle` holds.
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
            work->move.vy = RISING_SPARK_RISE_PER_TICK;
            work->move.vx = 0;
            work->move.vz = 0;
            task->state   = RISING_SPARK_STATE_ANIMATE;
            RANDOM_LCG_ADVANCE();
            work->scale  = RANDOM_LCG_DRAW() & RISING_SPARK_ROTATION_MASK;
            spawnSize    = task->spawnArg1.halves.low;
            work->period = RISING_SPARK_PALETTE_INDEX << RISING_SPARK_PALETTE_SHIFT;
            work->angle  = spawnSize & RISING_SPARK_SIZE_MASK;
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
