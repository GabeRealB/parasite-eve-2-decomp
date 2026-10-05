#include "main/random.h"

/* Part of the Dryfield main street library; see main_street.h. */

/// Effect 0x601B2: on its first frame takes size, frame period and speed from
/// the spawn argument and a random spin and backward/sideways bearing. Each
/// frame draws the current sheet cell, drifts, and steps the cell every period
/// frames, releasing after cell 9.
void mainStreetPuffTask(Task* task)
{
    EffectWork* work  = task->spawnArg2.pointer;
    GfxCoord*   coord = task->extra.coordBody->coord;
    s32         vz;
    s16         f2a;
    u32         rng2;
    u32         rng3;

    work->age++;
    if (task->state == 0) {
        work->scale     = task->spawnArg1.value & 0xFFF;
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        work->angle     = (gRandomLcgState >> 16) & 0xFFF;

        if (task->spawnArg1.value & 0xF000) {
            work->period = (task->spawnArg1.value >> 12) & 0x7;
        } else {
            work->period = 1;
        }

        work->age   = 0;
        task->state = 1;

        if (task->spawnArg1.value & 0xFF0000) {
            f2a = (task->spawnArg1.value >> 16) & 0xFF;
        } else {
            f2a = 0x40;
        }

        work->step      = f2a;
        work->move.vy   = 0;
        rng2            = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        gRandomLcgState = rng2;
        work->move.vx   = -(((u32)rng2 >> 16) & 0x7F);
        rng3            = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        gRandomLcgState = rng3;
        vz              = 0x80 - (((u32)rng3 >> 16) & 0xFF);
        work->move.vz   = vz;
        VectorNormalSS(&work->move, &work->move);

        gte_lddp(work->step);
        gte_ldsv(&work->move);
        gte_gpf12();
        gte_stsv(&work->move);
    }

    _mainStreetDrawPuff(coord, work->index, work->scale, work->angle);

    coord->coord.t[0]  += work->move.vx;
    coord->coord.t[1]  += work->move.vy;
    coord->coord.t[2]  += work->move.vz;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;

    if ((work->age % work->period) == 0) {
        work->index++;
        if (work->index >= 0xA) {
            effectKillTask(work, task);
        }
    }
}
