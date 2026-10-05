#include "main/random.h"

/* Part of the falling leaves library; see falling_leaves.h. */

/// Initializes a leaf's tumble rates and translation per tick.
///
/// `work` must be a live, writable `EffectWork`. `period` receives the X
/// tumble rate (-240..256), and `step` the Z rate (-112..128), both in
/// 4096-units-per-turn angles and multiples of 16. Each `move` component
/// receives -15..16 parent-coordinate units per tick.
/// Consumes exactly five shared random draws, ordered X/Z tumble then X/Y/Z
/// translation. Other work fields, including the vector's fourth halfword,
/// are left intact; the borrowed work pointer is not retained.
static inline void _leafSeedMotion(EffectWork* work)
{
    enum {
        LEAF_INITIAL_X_TUMBLE_BIAS = 256,
        LEAF_INITIAL_X_TUMBLE_MASK = 0x1F0,
        LEAF_INITIAL_Z_TUMBLE_BIAS = 128,
        LEAF_INITIAL_Z_TUMBLE_MASK = 0xF0,
        LEAF_INITIAL_MOVE_BIAS     = 16,
        LEAF_INITIAL_MOVE_MASK     = 0x1F,
    };

    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    work->period    = LEAF_INITIAL_X_TUMBLE_BIAS - ((gRandomLcgState >> 16) & LEAF_INITIAL_X_TUMBLE_MASK);
    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    work->step      = LEAF_INITIAL_Z_TUMBLE_BIAS - ((gRandomLcgState >> 16) & LEAF_INITIAL_Z_TUMBLE_MASK);
    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    work->move.vx   = LEAF_INITIAL_MOVE_BIAS - ((gRandomLcgState >> 16) & LEAF_INITIAL_MOVE_MASK);
    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    work->move.vy   = LEAF_INITIAL_MOVE_BIAS - ((gRandomLcgState >> 16) & LEAF_INITIAL_MOVE_MASK);
    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    work->move.vz   = LEAF_INITIAL_MOVE_BIAS - ((gRandomLcgState >> 16) & LEAF_INITIAL_MOVE_MASK);
}

/// Advances one tumbling leaf through its fall, stationary hold and fade.
///
/// Requires a counted effect task with a coordinate body and a cleared,
/// primary-heap `EffectWork` in `spawnArg2.pointer`, as `Gp_SpawnEff` supplies.
/// `move` holds coordinate units per tick; `period` and `step` are X/Z tumble
/// increments in 4096 units per turn. The square has half-size 32.
/// Stops motion after parent-space Y becomes positive, retaining the final
/// translation and rotation. `angle` counts the opaque hold up to 128 in steps
/// of 16, then stores the fading brightness, drawn from 112 down to 16.
/// Releases the counted work and task after the fade, ending both lifetimes.
static inline void _leafFallTask(Task* task)
{
    enum {
        LEAF_STATE_INIT           = 0,
        LEAF_STATE_FALL           = 1,
        LEAF_STATE_HOLD           = 2,
        LEAF_STATE_FADE           = 3,
        LEAF_HALF_SIZE            = 32,
        LEAF_FALL_SPEED_THRESHOLD = 29,
        LEAF_HOLD_LIMIT           = 128,
        LEAF_BRIGHTNESS_STEP      = 16,
    };
    EffectWork* work;
    GfxCoord*   coord;
    s32         fallSpeed;
    s32         driftX;
    s32         driftZ;

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    // Draw the cached entry pose; motion below marks composition dirty for the next tick.
    actorRenderComposeCoord(coord);
    work->age++;
    switch (task->state) {
        case LEAF_STATE_INIT:
            work->scale = LEAF_HALF_SIZE;
            _leafSeedMotion(work);
            task->state = LEAF_STATE_FALL;
            /* fallthrough */
        case LEAF_STATE_FALL:
            coord->coord.t[0] += work->move.vx;
            coord->coord.t[1] += work->move.vy;
            coord->coord.t[2] += work->move.vz;
            gfxRotMatrixX(&coord->coord, work->period, GRAPHICS_ROTATION_COMPOSE);
            gfxRotMatrixZ(&coord->coord, work->step, GRAPHICS_ROTATION_COMPOSE);
            coord->composeStamp = GRAPHICS_COORD_DIRTY;

            // Downward speed settles into 28/29; horizontal drift damps and reseeds.
            fallSpeed = work->move.vy;
            if (fallSpeed >= LEAF_FALL_SPEED_THRESHOLD) {
                fallSpeed = fallSpeed - 1;
            } else {
                fallSpeed = fallSpeed + 1;
            }
            work->move.vy = fallSpeed;

            driftX = work->move.vx;
            if (driftX == 0) {
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->move.vx  += (2 - (u16)((gRandomLcgState >> 16) % 5U)) * 8;
            } else {
                if (driftX > 0) {
                    driftX = driftX - 1;
                } else {
                    driftX = driftX + 1;
                }
                work->move.vx = driftX;
            }

            driftZ = work->move.vz;
            if (driftZ == 0) {
                work->move.vz  += work->step % 32;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->move.vz  += (2 - (u16)((gRandomLcgState >> 16) % 5U)) * 8;
            } else {
                if (driftZ > 0) {
                    driftZ = driftZ - 1;
                } else {
                    driftZ = driftZ + 1;
                }
                work->move.vz = driftZ;
            }

            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->period   += (1 - (u16)((gRandomLcgState >> 16) % 3U)) * 0x10;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->step     += (1 - (u16)((gRandomLcgState >> 16) % 3U)) * 8;

            if (coord->coord.t[1] > 0) {
                task->state = LEAF_STATE_HOLD;
            }
            _leafDraw(coord, work->scale, LEAF_BRIGHTNESS_RAW_TEXTURE);
            break;
        case LEAF_STATE_HOLD:
            // Eight hold ticks raise the counter; the ninth advances to fading.
            if (work->angle < LEAF_HOLD_LIMIT) {
                work->angle += LEAF_BRIGHTNESS_STEP;
            } else {
                task->state = LEAF_STATE_FADE;
            }
            _leafDraw(coord, work->scale, LEAF_BRIGHTNESS_RAW_TEXTURE);
            break;
        case LEAF_STATE_FADE:
            if (work->angle >= LEAF_BRIGHTNESS_STEP + 1) {
                work->angle -= LEAF_BRIGHTNESS_STEP;
                _leafDraw(coord, work->scale, work->angle);
            } else {
                effectKillTask(work, task);
            }
            break;
    }
}
