#include "main/random.h"

/* Part of the Dryfield main street library; see main_street.h. */

/// Initializes a puff's local displacement at the requested speed.
///
/// `work` is live writable effect work; `speed` is 1..255 coordinate units per
/// tick. Advances the shared LCG twice to select nonpositive X and signed Z,
/// with zero Y. Normalizes that bearing to Q12, then scales it through the GTE
/// to integer displacement components; no pointer is retained.
static inline void _mainStreetInitializePuffDrift(EffectWork* work, s16 speed)
{
    enum {
        MAIN_STREET_PUFF_DIRECTION_X_MASK     = 0x7F,
        MAIN_STREET_PUFF_DIRECTION_Z_MASK     = 0xFF,
        MAIN_STREET_PUFF_DIRECTION_Z_MIDPOINT = 0x80,
    };
    u32 directionXState;
    u32 directionZState;

    work->step      = speed;
    work->move.vy   = 0;
    directionXState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    gRandomLcgState = directionXState;
    work->move.vx   = -((directionXState >> 16) & MAIN_STREET_PUFF_DIRECTION_X_MASK);
    directionZState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    gRandomLcgState = directionZState;
    work->move.vz   = MAIN_STREET_PUFF_DIRECTION_Z_MIDPOINT - ((directionZState >> 16) & MAIN_STREET_PUFF_DIRECTION_Z_MASK);
    VectorNormalSS(&work->move, &work->move);

    gte_lddp(work->step);
    gte_ldsv(&work->move);
    gte_gpf12();
    gte_stsv(&work->move);
}

void MAIN_STREET_PUFF_TASK(Task* task)
{
    enum {
        MAIN_STREET_PUFF_STATE_INIT          = 0,
        MAIN_STREET_PUFF_STATE_DRIFT         = 1,
        MAIN_STREET_PUFF_FRAME_COUNT         = 10,
        MAIN_STREET_PUFF_SIZE_MASK           = 0xFFF,
        MAIN_STREET_PUFF_ANGLE_MASK          = ONE - 1,
        MAIN_STREET_PUFF_PERIOD_PRESENT_MASK = 0xF000,
        MAIN_STREET_PUFF_PERIOD_SHIFT        = 12,
        MAIN_STREET_PUFF_PERIOD_MASK         = 0x7,
        MAIN_STREET_PUFF_DEFAULT_PERIOD      = 1,
        MAIN_STREET_PUFF_SPEED_PRESENT_MASK  = 0xFF0000,
        MAIN_STREET_PUFF_SPEED_SHIFT         = 16,
        MAIN_STREET_PUFF_SPEED_MASK          = 0xFF,
        MAIN_STREET_PUFF_DEFAULT_SPEED       = 64,
    };
    EffectWork* work  = task->spawnArg2.pointer;
    GfxCoord*   coord = task->extra.coordBody->coord;
    s16         speed;

    work->age++;
    if (task->state == MAIN_STREET_PUFF_STATE_INIT) {
        // Decode the packed spawn values and choose the fixed screen rotation.
        work->scale     = task->spawnArg1.value & MAIN_STREET_PUFF_SIZE_MASK;
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        work->angle     = (gRandomLcgState >> 16) & MAIN_STREET_PUFF_ANGLE_MASK;

        // Bit 15 affects presence but is excluded from the decoded period.
        if (task->spawnArg1.value & MAIN_STREET_PUFF_PERIOD_PRESENT_MASK) {
            work->period = (task->spawnArg1.value >> MAIN_STREET_PUFF_PERIOD_SHIFT) & MAIN_STREET_PUFF_PERIOD_MASK;
        } else {
            work->period = MAIN_STREET_PUFF_DEFAULT_PERIOD;
        }

        work->age   = 0;
        task->state = MAIN_STREET_PUFF_STATE_DRIFT;

        if (task->spawnArg1.value & MAIN_STREET_PUFF_SPEED_PRESENT_MASK) {
            speed = (task->spawnArg1.value >> MAIN_STREET_PUFF_SPEED_SHIFT) & MAIN_STREET_PUFF_SPEED_MASK;
        } else {
            speed = MAIN_STREET_PUFF_DEFAULT_SPEED;
        }

        _mainStreetInitializePuffDrift(work, speed);
    }

    // Draw the composed position before applying the next tick's displacement.
    _mainStreetDrawPuff(coord, work->index, work->scale, work->angle);

    coord->coord.t[0]  += work->move.vx;
    coord->coord.t[1]  += work->move.vy;
    coord->coord.t[2]  += work->move.vz;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;

    // Age zero advances cell zero immediately; later cells last a full period.
    if ((work->age % work->period) == 0) {
        work->index++;
        if (work->index >= MAIN_STREET_PUFF_FRAME_COUNT) {
            effectKillTask(work, task);
        }
    }
}
