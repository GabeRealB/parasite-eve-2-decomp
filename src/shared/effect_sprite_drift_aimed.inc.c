#include "main/random.h"

/* Included aimed-drift task; both Shelter B3 carriers export a callback whose
 * declaration and task contract are in that package's public header. */

#ifndef EFFECT_SPRITE_DRIFT_AIMED_TASK
#error "Bind EFFECT_SPRITE_DRIFT_AIMED_TASK to the carrier's void (Task*) callback before inclusion"
#endif

/// Converts a Q12 sprite direction in place to per-update integer velocity.
///
/// `velocity` borrows a live, writable `SVECTOR`; XYZ are parent-space direction
/// components with 4096 representing one. `speed` is signed integer
/// parent-coordinate units per running update (this task supplies 0..255).
/// Each component becomes its product with `speed`, arithmetically shifted
/// right by 12 and saturated to a signed halfword. Zero speed writes zero XYZ.
/// Leaves `pad` intact and retains no pointer. Clobbers GTE IR0..3, MAC1..3,
/// the colour FIFO and FLAG.
static __inline__ void _effectSpriteAimedDriftScaleVelocity(SVECTOR* velocity, s16 speed)
{
    gte_lddp(speed);
    gte_ldsv(velocity);
    gte_gpf12();
    gte_stsv(velocity);
}

/// Converts a generated sprite direction to velocity at the selected launch speed.
///
/// Borrows writable task-owned work: `move` supplies signed parent-space XYZ
/// direction and `step` supplies 0..255 coordinate units per running update.
/// Normalizes in place to Q12 before reading the speed for integer scaling.
/// Only XYZ change; the vector's fourth halfword and other work fields stay
/// intact. A zero direction follows SDK normalization without a special case.
/// Clobbers GTE data/results and retains no pointer. A supplied velocity must
/// bypass this conversion.
static __inline__ void _effectSpriteAimedDriftInitializeVelocity(EffectWork* work)
{
    SVECTOR* direction = &work->move;

    VectorNormalSS(direction, direction);
    _effectSpriteAimedDriftScaleVelocity(direction, work->step);
}

/// Adds one running update's velocity to a sprite's parent-space translation.
///
/// `velocity` must be a readable `SVECTOR` lvalue; `coord` must point to a live,
/// writable `GfxCoord` disjoint from it. Both expressions must be side-effect-free:
/// the vector is evaluated three times and the pointer four times. Signed XYZ
/// integer displacements are extended for the 32-bit additions; velocity and
/// rotation stay intact. Invalidates composition even for zero displacement;
/// composition must run before using `workm`. The caller gates movement and
/// applies acceleration afterwards. Expands to a compound statement, captures
/// no locals and retains no pointer. Undefined after the task fragment.
#define EFFECT_SPRITE_AIMED_DRIFT_TRANSLATE(velocity, coord) \
    {                                                        \
        (coord)->coord.t[0]  += (velocity).vx;               \
        (coord)->coord.t[1]  += (velocity).vy;               \
        (coord)->coord.t[2]  += (velocity).vz;               \
        (coord)->composeStamp = GRAPHICS_COORD_DIRTY;        \
    }

void EFFECT_SPRITE_DRIFT_AIMED_TASK(Task* task)
{
    enum {
        EFFECT_SPRITE_AIMED_DRIFT_INITIALIZE = 0,
        EFFECT_SPRITE_AIMED_DRIFT_BANKED     = 1,
        EFFECT_SPRITE_AIMED_DRIFT_ALTERNATE  = 2,
        EFFECT_SPRITE_AIMED_DRIFT_SIZE_MASK  = 0xFFF,
        EFFECT_SPRITE_AIMED_DRIFT_ANGLE_MASK = 0xFFF,
        // Bit 15 participates in the fallback test, but not the extracted period.
        EFFECT_SPRITE_AIMED_DRIFT_PERIOD_NIBBLE_MASK   = 0xF000,
        EFFECT_SPRITE_AIMED_DRIFT_PERIOD_SHIFT         = 12,
        EFFECT_SPRITE_AIMED_DRIFT_PERIOD_MASK          = 7,
        EFFECT_SPRITE_AIMED_DRIFT_SPEED_BYTE_MASK      = 0xFF0000,
        EFFECT_SPRITE_AIMED_DRIFT_SPEED_SHIFT          = 16,
        EFFECT_SPRITE_AIMED_DRIFT_SPEED_MASK           = 0xFF,
        EFFECT_SPRITE_AIMED_DRIFT_DEFAULT_SPEED        = 0x40,
        EFFECT_SPRITE_AIMED_DRIFT_DIRECTION_SHIFT      = 24,
        EFFECT_SPRITE_AIMED_DRIFT_DIRECTION_MASK       = 0xF,
        EFFECT_SPRITE_AIMED_DRIFT_PALETTE_MASK         = 0x7000,
        EFFECT_SPRITE_AIMED_DRIFT_STILL                = 0,
        EFFECT_SPRITE_AIMED_DRIFT_RANDOM_UPWARD        = 1,
        EFFECT_SPRITE_AIMED_DRIFT_RANDOM_ALL_AXES      = 2,
        EFFECT_SPRITE_AIMED_DRIFT_RANDOM_NARROW_UPWARD = 3,
        EFFECT_SPRITE_AIMED_DRIFT_OFFSET_DIRECTION     = 5,
        EFFECT_SPRITE_AIMED_DRIFT_RANDOM_PLANAR        = 6,
        EFFECT_SPRITE_AIMED_DRIFT_PARENT_FORWARD       = 7,
        // Signed-halfword encoding of -64 before the random upward Y offset.
        EFFECT_SPRITE_AIMED_DRIFT_UPWARD_Y_BIAS            = 0xFFC0,
        EFFECT_SPRITE_AIMED_DRIFT_BANKED_FRAME_COUNT       = 12,
        EFFECT_SPRITE_AIMED_DRIFT_ALTERNATE_FRAME_COUNT    = 10,
        EFFECT_SPRITE_AIMED_DRIFT_BANKED_Y_ACCELERATION    = 2,
        EFFECT_SPRITE_AIMED_DRIFT_ALTERNATE_Y_ACCELERATION = 1,
        EFFECT_SPRITE_AIMED_DRIFT_DOWNWARD_AGE_DIVISOR     = 10
    };
    EffectWork* work;
    GfxCoord*   coord;
    s32         framesPerCell;
    s32         speed;

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        // Suspended and cancelling updates redraw before any work is released.
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
        case EFFECT_SPRITE_AIMED_DRIFT_INITIALIZE:
            // Initialize once without drawing; every sprite consumes a random spin.
            work->scale     = task->spawnArg1.value & EFFECT_SPRITE_AIMED_DRIFT_SIZE_MASK;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->angle     = (gRandomLcgState >> 16) & EFFECT_SPRITE_AIMED_DRIFT_ANGLE_MASK;
            if (task->spawnArg1.value & EFFECT_SPRITE_AIMED_DRIFT_PERIOD_NIBBLE_MASK) {
                framesPerCell = (task->spawnArg1.value >> EFFECT_SPRITE_AIMED_DRIFT_PERIOD_SHIFT) & EFFECT_SPRITE_AIMED_DRIFT_PERIOD_MASK;
            } else {
                framesPerCell = 1;
            }
            work->period = framesPerCell;
            work->age    = 0;
            task->state  = EFFECT_SPRITE_AIMED_DRIFT_BANKED;
            task->state  = task->spawnArg1.value < 0 ? EFFECT_SPRITE_AIMED_DRIFT_ALTERNATE : EFFECT_SPRITE_AIMED_DRIFT_BANKED;
            work->pos.vx = (task->spawnArg1.value >> EFFECT_SPRITE_AIMED_DRIFT_SPEED_SHIFT) & EFFECT_SPRITE_AIMED_DRIFT_PALETTE_MASK;
            if ((work->move.vx | work->move.vy | work->move.vz) == 0) {
                if (task->spawnArg1.value & EFFECT_SPRITE_AIMED_DRIFT_SPEED_BYTE_MASK) {
                    speed = (task->spawnArg1.value >> EFFECT_SPRITE_AIMED_DRIFT_SPEED_SHIFT) & EFFECT_SPRITE_AIMED_DRIFT_SPEED_MASK;
                } else {
                    speed = EFFECT_SPRITE_AIMED_DRIFT_DEFAULT_SPEED;
                }
                work->step = speed;
                switch ((task->spawnArg1.value >> EFFECT_SPRITE_AIMED_DRIFT_DIRECTION_SHIFT) & EFFECT_SPRITE_AIMED_DRIFT_DIRECTION_MASK) {
                    case EFFECT_SPRITE_AIMED_DRIFT_STILL:
                        work->step = 0;
                        break;
                    case EFFECT_SPRITE_AIMED_DRIFT_RANDOM_UPWARD:
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        work->move.vx   = 0x80 - ((gRandomLcgState >> 16) & 0xFF);
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        work->move.vy   = EFFECT_SPRITE_AIMED_DRIFT_UPWARD_Y_BIAS - ((gRandomLcgState >> 16) & 0x7F);
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        work->move.vz   = 0x80 - ((gRandomLcgState >> 16) & 0xFF);
                        break;
                    case EFFECT_SPRITE_AIMED_DRIFT_RANDOM_ALL_AXES:
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        work->move.vx   = 0x80 - ((gRandomLcgState >> 16) & 0xFF);
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        work->move.vy   = 0x80 - ((gRandomLcgState >> 16) & 0xFF);
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        work->move.vz   = 0x80 - ((gRandomLcgState >> 16) & 0xFF);
                        break;
                    case EFFECT_SPRITE_AIMED_DRIFT_RANDOM_NARROW_UPWARD:
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        work->move.vx   = 0x10 - ((gRandomLcgState >> 16) & 0x1F);
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        work->move.vy   = -((gRandomLcgState >> 16) & 0xFF);
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        work->move.vz   = 0x10 - ((gRandomLcgState >> 16) & 0x1F);
                        break;
                    case EFFECT_SPRITE_AIMED_DRIFT_OFFSET_DIRECTION:
                        // X now holds palette bits; Y and Z retain the spawn offset.
                        work->move.vx = work->pos.vx;
                        work->move.vy = work->pos.vy;
                        work->move.vz = work->pos.vz;
                        break;
                    case EFFECT_SPRITE_AIMED_DRIFT_RANDOM_PLANAR:
                        work->move.vy   = 0;
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        work->move.vx   = 0x80 - ((gRandomLcgState >> 16) & 0xFF);
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        work->move.vz   = 0x80 - ((gRandomLcgState >> 16) & 0xFF);
                        break;
                    case EFFECT_SPRITE_AIMED_DRIFT_PARENT_FORWARD:
                        // Rotate the random forward direction into the effect's parent space.
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
                _effectSpriteAimedDriftInitializeVelocity(work);
            } else {
                // A supplied velocity is already scaled; step only enables movement.
                work->step = EFFECT_SPRITE_AIMED_DRIFT_DEFAULT_SPEED;
            }
            break;
        case EFFECT_SPRITE_AIMED_DRIFT_BANKED:
            _effectSpriteDrawBanked(coord, work->index | work->pos.vx, work->scale, work->angle);
            if (work->step != 0) {
                // Draw and translate before updating the next tick's Y velocity.
                EFFECT_SPRITE_AIMED_DRIFT_TRANSLATE(work->move, coord);
                if (((task->spawnArg1.value >> EFFECT_SPRITE_AIMED_DRIFT_DIRECTION_SHIFT) & EFFECT_SPRITE_AIMED_DRIFT_DIRECTION_MASK) == EFFECT_SPRITE_AIMED_DRIFT_PARENT_FORWARD) {
                    work->move.vy += work->age / EFFECT_SPRITE_AIMED_DRIFT_DOWNWARD_AGE_DIVISOR;
                } else {
                    work->move.vy -= EFFECT_SPRITE_AIMED_DRIFT_BANKED_Y_ACCELERATION;
                }
            }
            if ((work->age % work->period) == 0) {
                work->index++;
                if (work->index >= EFFECT_SPRITE_AIMED_DRIFT_BANKED_FRAME_COUNT) {
                    effectKillTask(work, task);
                }
            }
            break;
        case EFFECT_SPRITE_AIMED_DRIFT_ALTERNATE:
            _effectSpriteDrawRotated(coord, work->index | work->pos.vx, work->scale, work->angle);
            if (work->step != 0) {
                EFFECT_SPRITE_AIMED_DRIFT_TRANSLATE(work->move, coord);
                if (((task->spawnArg1.value >> EFFECT_SPRITE_AIMED_DRIFT_DIRECTION_SHIFT) & EFFECT_SPRITE_AIMED_DRIFT_DIRECTION_MASK) == EFFECT_SPRITE_AIMED_DRIFT_PARENT_FORWARD) {
                    work->move.vy += work->age / EFFECT_SPRITE_AIMED_DRIFT_DOWNWARD_AGE_DIVISOR;
                } else {
                    work->move.vy -= EFFECT_SPRITE_AIMED_DRIFT_ALTERNATE_Y_ACCELERATION;
                }
            }
            if ((work->age % work->period) == 0) {
                work->index++;
                if (work->index >= EFFECT_SPRITE_AIMED_DRIFT_ALTERNATE_FRAME_COUNT) {
                    effectKillTask(work, task);
                }
            }
            break;
    }
}

#undef EFFECT_SPRITE_DRIFT_AIMED_TASK
#undef EFFECT_SPRITE_AIMED_DRIFT_TRANSLATE
