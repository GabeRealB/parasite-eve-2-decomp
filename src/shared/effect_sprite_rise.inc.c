#include "main/random.h"

/* Included rising-sprite task; each carrier's public header declares its
 * export and task contract. Bind its function identifier before effect_sprite.h. */

#ifndef EFFECT_SPRITE_RISE_TASK
#error "Bind EFFECT_SPRITE_RISE_TASK to the carrier's void (Task*) callback before inclusion"
#endif

/// Draws the rising sprite's current cell and angle with one random palette.
///
/// Borrows live work and a composed coordinate cache. `index` is a cell 0..7,
/// `angle` a size numerator 0..4095 and `scale` a screen angle 0..4095, in
/// 4096 units per turn. Consumes one unsigned LCG step even if projection
/// rejects the quad. Requires scratch storage and capacity for one quad packet;
/// neither input pointer is retained.
static __inline__ void _effectSpriteRiseDrawRandomPalette(const GfxCoord* coord, const EffectWork* work)
{
    enum {
        EFFECT_SPRITE_RISE_PALETTE_COUNT = 6,
        EFFECT_SPRITE_RISE_PALETTE_SHIFT = 12,
        EFFECT_SPRITE_RISE_RANDOM_SHIFT  = 16
    };

    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    effectDrawSpinningBillboard(coord, work->index, work->angle, work->scale | (((gRandomLcgState >> EFFECT_SPRITE_RISE_RANDOM_SHIFT) % EFFECT_SPRITE_RISE_PALETTE_COUNT) << EFFECT_SPRITE_RISE_PALETTE_SHIFT));
}

void EFFECT_SPRITE_RISE_TASK(Task* task)
{
    enum {
        EFFECT_SPRITE_RISE_INITIALIZE       = 0,
        EFFECT_SPRITE_RISE_ACTIVE           = 1,
        EFFECT_SPRITE_RISE_RANDOM_SHIFT     = 16,
        EFFECT_SPRITE_RISE_SPEED_MASK       = 0x3F,
        EFFECT_SPRITE_RISE_MIN_SPEED        = 16,
        EFFECT_SPRITE_RISE_ANGLE_MASK       = 0xFFF,
        EFFECT_SPRITE_RISE_SIZE_MASK        = 0xFFF,
        EFFECT_SPRITE_RISE_REVERSE_Y        = 0x10000,
        EFFECT_SPRITE_RISE_UPDATES_PER_CELL = 4,
        EFFECT_SPRITE_RISE_CELL_COUNT       = 8
    };

    EffectWork* work;
    GfxCoord*   coord;

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        if (gRoomEffectState->effectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            effectKillTask(work, task);
            return;
        }
        _effectSpriteRiseDrawRandomPalette(coord, work);
        return;
    }
    work->age++;
    if (task->state == EFFECT_SPRITE_RISE_INITIALIZE) {
        // Store the fixed screen angle in scale and the size numerator in angle.
        work->move.vx   = 0;
        work->move.vz   = 0;
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        work->move.vy   = ((gRandomLcgState >> EFFECT_SPRITE_RISE_RANDOM_SHIFT) & EFFECT_SPRITE_RISE_SPEED_MASK) + EFFECT_SPRITE_RISE_MIN_SPEED;
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        work->scale     = (gRandomLcgState >> EFFECT_SPRITE_RISE_RANDOM_SHIFT) & EFFECT_SPRITE_RISE_ANGLE_MASK;
        work->angle     = task->spawnArg1.value & EFFECT_SPRITE_RISE_SIZE_MASK;
        if (task->spawnArg1.value & EFFECT_SPRITE_RISE_REVERSE_Y) {
            work->move.vy = -work->move.vy;
        }
        task->state = EFFECT_SPRITE_RISE_ACTIVE;
    }
    // Move and advance before drawing; the drawer reads the existing composed cache.
    coord->coord.t[1]  += work->move.vy;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    if (!(work->age & (EFFECT_SPRITE_RISE_UPDATES_PER_CELL - 1))) {
        work->index++;
    }
    if (work->index < EFFECT_SPRITE_RISE_CELL_COUNT) {
        _effectSpriteRiseDrawRandomPalette(coord, work);
        return;
    }
    effectKillTask(work, task);
}

#undef EFFECT_SPRITE_RISE_TASK
