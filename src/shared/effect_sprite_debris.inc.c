#include "main/random.h"

/* Part of the effect sprite library; see effect_sprite.h. */

/// Room-effect task for one tumbling debris piece. The first frame unpacks
/// size, frames per cell (bits 12..15) and drawer (chip, or billboard when bits
/// 28..31 are set), and rolls a velocity of kind 0..5 scaled to a speed. Each
/// frame it draws the cell, moves the coordinate, adds 6 of gravity and frees
/// itself after cell 8. While effects are paused it only draws.
void effectSpriteDebrisTask(Task* task)
{
    EffectWork* work;
    GfxCoord*   coord;
    SVECTOR*    vec;
    s32         kind;
    s32         step;
    s32         state;
    s32         level;

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        _effectSpriteDrawChip(coord, work->index, work->scale, work->angle);
        if (gRoomEffectState->effectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            effectKillTask(work, task);
        }
        return;
    }
    work->age++;
    switch (task->state) {
        case 0:
            work->scale     = task->spawnArg1.halves.low & 0xFFF;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->angle     = (gRandomLcgState >> 16) & 0xFFF;
            if (task->spawnArg1.value & 0xF000) {
                step = (task->spawnArg1.value >> 12) & 0xF;
            } else {
                step = 1;
            }
            work->period = step;
            work->age    = 0;
            state        = 1;
            if (task->spawnArg1.value & 0xF0000000) {
                state = 2;
            }
            task->state = state;
            if (((u16)work->move.vx | (u16)work->move.vy | (u16)work->move.vz) == 0) {
                if (task->spawnArg1.value & 0xFF0000) {
                    level = (task->spawnArg1.value >> 16) & 0xFF;
                } else {
                    level = 0x40;
                }
                work->step = level;
                kind       = task->spawnArg1.signedBytes[3];
                switch (kind & 0xF) {
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
            return;
        case 1:
            _effectSpriteDrawChip(coord, work->index, work->scale, work->angle);
            break;
        case 2:
            _effectSpriteDrawBillboard(coord, (u16)work->index, work->scale);
            break;
        default:
            return;
    }
    if (work->step != 0) {
        coord->coord.t[0]  += work->move.vx;
        coord->coord.t[1]  += work->move.vy;
        coord->coord.t[2]  += work->move.vz;
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        work->move.vy      += 6;
    }
    if ((work->age % work->period) == 0) {
        work->index++;
        if (work->index >= 8) {
            effectKillTask(work, task);
        }
    }
}
