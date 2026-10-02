#include "main/random.h"

/* Part of the effect sprite library; see effect_sprite.h.
 *
 * A room whose gameplay table names its own task defines
 * EFFECT_SPRITE_DRIFT_TASK to that name; EFFECT_SPRITE_DRIFT_DRAW_A / _B name the room's own
 * drawers when it does not use effectSpriteDrawBanked / effectSpriteDrawRotated.
 * Shelter R48 enables EFFECT_SPRITE_DRIFT_SIGN_BANK; see its definition for
 * the spawn-argument and suspended-draw contract.
 * EFFECT_SPRITE_DRIFT_STEADY_RISE 1 rises at a
 * constant rate for every kind (Shelter R48). */

#ifndef EFFECT_SPRITE_DRIFT_TASK
#define EFFECT_SPRITE_DRIFT_TASK effectSpriteDriftTask
#endif
#ifndef EFFECT_SPRITE_DRIFT_DRAW_A
#define EFFECT_SPRITE_DRIFT_DRAW_A effectSpriteDrawBanked
#endif
#ifndef EFFECT_SPRITE_DRIFT_DRAW_B
#define EFFECT_SPRITE_DRIFT_DRAW_B effectSpriteDrawRotated
#endif

/// Per-frame handler for one animated sprite effect, drawn by
/// `effectSpriteDrawBanked` (state 1) or
/// `effectSpriteDrawRotated` (state 2). By default its first frame unpacks
/// `spawnArg1`: the low 12 bits are the sprite size, bits 12..14 the frames per
/// animation cell (1 when zero), bits 28..30 are kept as the drawer's clut
/// selector, and the sign bit picks the second drawer. When the work block
/// arrives without a velocity, bits 24..27 choose how one is rolled from
/// `gRandomLcgState` (0 leaves it still) and it is scaled to a speed from bits
/// 16..23 (0x40 when zero). Each later frame draws the current cell, moves the
/// coordinate by the velocity and bends its Y component, then frees the effect
/// after the drawer's last cell (12 or 10). While the player is in an event it
/// only draws, and frees once the event state reaches 4.
void EFFECT_SPRITE_DRIFT_TASK(Task* task)
{
    EffectWork* work;
    GfxCoord*   coord;
    SVECTOR*    vec;
    s32         step;
    s32         level;
#if EFFECT_SPRITE_DRIFT_SIGN_BANK
    s32 zero;
#endif

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
#if EFFECT_SPRITE_DRIFT_PAUSED_DRAW_A_UNBANKED
        // Suspended sprites use drawer A's base palette even in drawer B's state.
        EFFECT_SPRITE_DRIFT_DRAW_A(coord, work->index, work->scale, work->angle);
#elif EFFECT_SPRITE_DRIFT_SIGN_BANK
        // Suspension reselects the drawer from the sign, rather than the running state.
        if (task->spawnArg1.value < 0) {
            EFFECT_SPRITE_DRIFT_DRAW_B(coord, work->index | work->pos.vx, work->scale, work->angle);
        } else {
            EFFECT_SPRITE_DRIFT_DRAW_A(coord, work->index | work->pos.vx, work->scale, work->angle);
        }
#else
        if (task->state < 2) {
            EFFECT_SPRITE_DRIFT_DRAW_A(coord, work->index | work->pos.vx, work->scale, work->angle);
        } else {
            EFFECT_SPRITE_DRIFT_DRAW_B(coord, work->index | work->pos.vx, work->scale, work->angle);
        }
#endif
        if (gRoomEffectState->effectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            effectKillTask(work, task);
        }
        return;
    }
    work->age++;
    switch (task->state) {
        case 0:
            work->scale     = task->spawnArg1.value & 0xFFF;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->angle     = (gRandomLcgState >> 16) & 0xFFF;
            if (task->spawnArg1.value & 0xF000) {
                step = (task->spawnArg1.value >> 12) & 7;
            } else {
                step = 1;
            }
            work->period = step;
            work->age    = 0;
#if EFFECT_SPRITE_DRIFT_SIGN_BANK
            // The high nibble chooses the running drawer; its sign alone chooses the palette.
            task->state  = task->spawnArg1.value & 0xF0000000 ? 2 : 1;
            zero         = 0;
            work->pos.vx = (task->spawnArg1.value < zero) << 12;
#else
            task->state  = 1;
            task->state  = task->spawnArg1.value < 0 ? 2 : 1;
            work->pos.vx = (task->spawnArg1.value >> 16) & 0x7000;
#endif
            if ((work->move.vx | work->move.vy | work->move.vz) == 0) {
                if (task->spawnArg1.value & 0xFF0000) {
                    level = (task->spawnArg1.value >> 16) & 0xFF;
                } else {
                    level = 0x40;
                }
                work->step = level;
                switch ((task->spawnArg1.value >> 24) & 0xF) {
                    case 0:
                        work->step = 0;
                        break;
                    case 1:
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        work->move.vx   = 0x80 - ((gRandomLcgState >> 16) & 0xFF);
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        work->move.vy   = 0xFFC0 - ((gRandomLcgState >> 16) & 0x7F);
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        work->move.vz   = 0x80 - ((gRandomLcgState >> 16) & 0xFF);
                        break;
                    case 2:
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        work->move.vx   = 0x80 - ((gRandomLcgState >> 16) & 0xFF);
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        work->move.vy   = 0x80 - ((gRandomLcgState >> 16) & 0xFF);
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        work->move.vz   = 0x80 - ((gRandomLcgState >> 16) & 0xFF);
                        break;
                    case 3:
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        work->move.vx   = 0x10 - ((gRandomLcgState >> 16) & 0x1F);
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        work->move.vy   = -((gRandomLcgState >> 16) & 0xFF);
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        work->move.vz   = 0x10 - ((gRandomLcgState >> 16) & 0x1F);
                        break;
                    case 5:
                        work->move.vx = work->pos.vx;
                        work->move.vy = work->pos.vy;
                        work->move.vz = work->pos.vz;
                        break;
                    case 6:
                        work->move.vy   = 0;
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        work->move.vx   = 0x80 - ((gRandomLcgState >> 16) & 0xFF);
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        work->move.vz   = 0x80 - ((gRandomLcgState >> 16) & 0xFF);
                        break;
                }
                vec = &work->move;
                VectorNormalSS(vec, vec);
                gte_lddp(work->step);
                gte_ldsv(vec);
                gte_gpf12();
                gte_stsv(vec);
            } else {
                work->step = 0x40;
            }
            break;
        case 1:
            EFFECT_SPRITE_DRIFT_DRAW_A(coord, work->index | work->pos.vx, work->scale, work->angle);
            if (work->step != 0) {
                coord->coord.t[0]  += work->move.vx;
                coord->coord.t[1]  += work->move.vy;
                coord->coord.t[2]  += work->move.vz;
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
#if EFFECT_SPRITE_DRIFT_STEADY_RISE
                work->move.vy -= 2;
#else
                if (((task->spawnArg1.value >> 24) & 0xF) == 7) {
                    work->move.vy += work->age / 10;
                } else {
                    work->move.vy -= 2;
                }
#endif
            }
            if ((work->age % work->period) == 0) {
                work->index++;
                if (work->index >= 12) {
                    effectKillTask(work, task);
                }
            }
            break;
        case 2:
            EFFECT_SPRITE_DRIFT_DRAW_B(coord, work->index | work->pos.vx, work->scale, work->angle);
            if (work->step != 0) {
                coord->coord.t[0]  += work->move.vx;
                coord->coord.t[1]  += work->move.vy;
                coord->coord.t[2]  += work->move.vz;
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
#if EFFECT_SPRITE_DRIFT_STEADY_RISE
                work->move.vy -= 1;
#else
                if (((task->spawnArg1.value >> 24) & 0xF) == 7) {
                    work->move.vy += work->age / 10;
                } else {
                    work->move.vy -= 1;
                }
#endif
            }
            if ((work->age % work->period) == 0) {
                work->index++;
                if (work->index >= 10) {
                    effectKillTask(work, task);
                }
            }
            break;
    }
}

#undef EFFECT_SPRITE_DRIFT_TASK
#undef EFFECT_SPRITE_DRIFT_PAUSED_DRAW_A_UNBANKED
#undef EFFECT_SPRITE_DRIFT_SIGN_BANK
#undef EFFECT_SPRITE_DRIFT_STEADY_RISE
#undef EFFECT_SPRITE_DRIFT_DRAW_A
#undef EFFECT_SPRITE_DRIFT_DRAW_B
