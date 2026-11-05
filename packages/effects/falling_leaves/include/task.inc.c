#include <engine/random.h>

/* Part of the falling leaves library; see falling_leaves.h. */

/// Picks a leaf's random tumble rates (`period` about X, `step` about Z) and its movement per tick.
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

    RANDOM_LCG_ADVANCE();
    work->period = LEAF_INITIAL_X_TUMBLE_BIAS - (RANDOM_LCG_DRAW() & LEAF_INITIAL_X_TUMBLE_MASK);
    RANDOM_LCG_ADVANCE();
    work->step = LEAF_INITIAL_Z_TUMBLE_BIAS - (RANDOM_LCG_DRAW() & LEAF_INITIAL_Z_TUMBLE_MASK);
    RANDOM_LCG_ADVANCE();
    work->move.vx = LEAF_INITIAL_MOVE_BIAS - (RANDOM_LCG_DRAW() & LEAF_INITIAL_MOVE_MASK);
    RANDOM_LCG_ADVANCE();
    work->move.vy = LEAF_INITIAL_MOVE_BIAS - (RANDOM_LCG_DRAW() & LEAF_INITIAL_MOVE_MASK);
    RANDOM_LCG_ADVANCE();
    work->move.vz = LEAF_INITIAL_MOVE_BIAS - (RANDOM_LCG_DRAW() & LEAF_INITIAL_MOVE_MASK);
}

/// Tumbles a leaf down with a wandering drift until it falls past Y 0, holds it there, then fades it out and
/// ends; `angle` counts the hold and then holds the fading brightness.
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
                RANDOM_LCG_ADVANCE();
                work->move.vx += (2 - (u16)(RANDOM_LCG_DRAW() % 5U)) * 8;
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
                work->move.vz += work->step % 32;
                RANDOM_LCG_ADVANCE();
                work->move.vz += (2 - (u16)(RANDOM_LCG_DRAW() % 5U)) * 8;
            } else {
                if (driftZ > 0) {
                    driftZ = driftZ - 1;
                } else {
                    driftZ = driftZ + 1;
                }
                work->move.vz = driftZ;
            }

            RANDOM_LCG_ADVANCE();
            work->period += (1 - (u16)(RANDOM_LCG_DRAW() % 3U)) * 0x10;
            RANDOM_LCG_ADVANCE();
            work->step += (1 - (u16)(RANDOM_LCG_DRAW() % 3U)) * 8;

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
