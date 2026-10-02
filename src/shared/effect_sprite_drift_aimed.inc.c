#include "main/random.h"

/* Part of the effect sprite library; see effect_sprite.h. */

/// Room-effect task for one drifting animated sprite, drawn by
/// _effectSpriteDrawBanked or _effectSpriteDrawRotated by the sign of spawnArg1.
/// It rolls a velocity kind from bits 24..27 and scales it to bits 16..23 (0x40
/// when zero). Kind 7 turns the velocity through the parent coordinate and
/// accelerates it downward by age/10. Otherwise it rises by 2 or 1 a frame, and
/// the task frees itself after the drawer's last cell (12 or 10).
void effectSpriteDriftTaskAimed(Task* task)
{
    EffectWork* work;
    GfxCoord*   coord;
    SVECTOR*    vec;
    s32         step;
    s32         level;

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        if (task->spawnArg1.value < 0) {
            _effectSpriteDrawRotated(coord, work->index | work->pos.vx, work->scale, work->angle);
        } else {
            _effectSpriteDrawBanked(coord, work->index | work->pos.vx, work->scale, work->angle);
        }
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
            task->state  = 1;
            task->state  = task->spawnArg1.value < 0 ? 2 : 1;
            work->pos.vx = (task->spawnArg1.value >> 16) & 0x7000;
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
                    case 7:
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        work->move.vx   = 0x10 - ((gRandomLcgState >> 16) & 0x1F);
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        work->move.vy   = 0x10 - ((gRandomLcgState >> 16) & 0x1F);
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        work->move.vz   = (gRandomLcgState >> 16) & 0xFF;
                        gte_SetRotMatrix(&work->parent->coord);
                        gte_ldv0(&work->move);
                        gte_rtv0();
                        gte_stsv(&work->move);
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
            _effectSpriteDrawBanked(coord, work->index | work->pos.vx, work->scale, work->angle);
            if (work->step != 0) {
                coord->coord.t[0]  += work->move.vx;
                coord->coord.t[1]  += work->move.vy;
                coord->coord.t[2]  += work->move.vz;
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
                if (((task->spawnArg1.value >> 24) & 0xF) == 7) {
                    work->move.vy += work->age / 10;
                } else {
                    work->move.vy -= 2;
                }
            }
            if ((work->age % work->period) == 0) {
                work->index++;
                if (work->index >= 12) {
                    effectKillTask(work, task);
                }
            }
            break;
        case 2:
            _effectSpriteDrawRotated(coord, work->index | work->pos.vx, work->scale, work->angle);
            if (work->step != 0) {
                coord->coord.t[0]  += work->move.vx;
                coord->coord.t[1]  += work->move.vy;
                coord->coord.t[2]  += work->move.vz;
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
                if (((task->spawnArg1.value >> 24) & 0xF) == 7) {
                    work->move.vy += work->age / 10;
                } else {
                    work->move.vy -= 1;
                }
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
