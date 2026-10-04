#include "main/random.h"

/* Part of the falling leaves library; see falling_leaves.h. */

/// One falling leaf. The first tick seeds a size of 0x20, random tumble rates
/// (`period` / `step`) and a random drift in `move`. While falling it moves and
/// tumbles the coordinate, eases each drift axis back towards zero (re-rolling
/// a multiple of 8 when it gets there) and jitters the tumble. Once it passes
/// the ground plane (y > 0, since y grows downwards) it lies there opaque for
/// eight ticks while `angle` counts up to 0x80, then fades out by 0x10 a tick
/// and releases its work block.
static inline void leafFallTask(Task* task)
{
    EffectWork* work;
    GfxCoord*   coord;
    s32         vy;
    s32         vx;
    s32         vz;

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    actorRenderComposeCoord(coord);
    work->age++;
    switch (task->state) {
        case 0:
            work->scale     = 0x20;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->period    = 0x100 - ((gRandomLcgState >> 16) & 0x1F0);
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->step      = 0x80 - ((gRandomLcgState >> 16) & 0xF0);
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->move.vx   = 0x10 - ((gRandomLcgState >> 16) & 0x1F);
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->move.vy   = 0x10 - ((gRandomLcgState >> 16) & 0x1F);
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->move.vz   = 0x10 - ((gRandomLcgState >> 16) & 0x1F);
            task->state     = 1;
            /* fallthrough */
        case 1:
            coord->coord.t[0] += work->move.vx;
            coord->coord.t[1] += work->move.vy;
            coord->coord.t[2] += work->move.vz;
            gfxRotMatrixX(&coord->coord, work->period, GRAPHICS_ROTATION_COMPOSE);
            gfxRotMatrixZ(&coord->coord, work->step, GRAPHICS_ROTATION_COMPOSE);
            coord->composeStamp = GRAPHICS_COORD_DIRTY;

            vy = work->move.vy;
            if (vy >= 0x1D) {
                vy = vy - 1;
            } else {
                vy = vy + 1;
            }
            work->move.vy = vy;

            vx = work->move.vx;
            if (vx == 0) {
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->move.vx  += (2 - (u16)((gRandomLcgState >> 16) % 5U)) * 8;
            } else {
                if (vx > 0) {
                    vx = vx - 1;
                } else {
                    vx = vx + 1;
                }
                work->move.vx = vx;
            }

            vz = work->move.vz;
            if (vz == 0) {
                work->move.vz  += work->step % 32;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->move.vz  += (2 - (u16)((gRandomLcgState >> 16) % 5U)) * 8;
            } else {
                if (vz > 0) {
                    vz = vz - 1;
                } else {
                    vz = vz + 1;
                }
                work->move.vz = vz;
            }

            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->period   += (1 - (u16)((gRandomLcgState >> 16) % 3U)) * 0x10;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->step     += (1 - (u16)((gRandomLcgState >> 16) % 3U)) * 8;

            if (coord->coord.t[1] > 0) {
                task->state = 2;
            }
            leafDraw(coord, work->scale, 0);
            break;
        case 2:
            if (work->angle < 0x80) {
                work->angle += 0x10;
            } else {
                task->state = 3;
            }
            leafDraw(coord, work->scale, 0);
            break;
        case 3:
            if (work->angle >= 0x11) {
                work->angle -= 0x10;
                leafDraw(coord, work->scale, work->angle);
            } else {
                effectKillTask(work, task);
            }
            break;
    }
}
