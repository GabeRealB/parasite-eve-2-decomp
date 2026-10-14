#include "main/random.h"

/* Included debris task; each carrier's public header declares its export and
 * task contract. Bind its function identifier before effect_sprite.h. */

#ifndef EFFECT_SPRITE_DEBRIS_TASK
#error "Bind EFFECT_SPRITE_DEBRIS_TASK to the carrier's void (Task*) callback before inclusion"
#endif

/// Converts a debris sprite's launch direction to its per-update velocity.
///
/// `work` must be a live, writable effect work block. Its signed `move`
/// components supply a direction in the coordinate's parent space; `step`
/// supplies speed in coordinate units per running update (0 for stationary
/// debris, otherwise 1..255). Normalize in place to Q12, then read the speed
/// and scale back to signed halfword integer displacements.
///
/// Zero directions follow `VectorNormalSS` without a special case. Only the
/// three `move` components change; `step` and the vector's fourth halfword
/// remain intact. No pointer is retained. Clobbers GTE data/result registers.
static __inline__ void _effectSpriteDebrisInitializeVelocity(EffectWork* work)
{
    SVECTOR* velocity = &work->move;

    VectorNormalSS(velocity, velocity);
    gte_lddp(work->step);
    gte_ldsv(velocity);
    gte_gpf12();
    gte_stsv(velocity);
}

/// Advances a debris sprite's translation and velocity by one running update.
///
/// `work` and `coord` must be live, writable objects borrowed from the effect
/// task. Signed halfwords in `work->move` are sign-extended for additions to
/// the 32-bit parent-space translation. Then add 6 coordinate units per update
/// to Y velocity, narrowing back to a signed halfword for the next update.
/// Acceleration is along positive parent-space Y; no parent rotation is applied.
///
/// The caller skips this operation when `work->step` is zero. Otherwise the
/// composition stamp is cleared even for zero velocity, so the cached transform
/// must be rebuilt before use. Neither pointer is retained.
static __inline__ void _effectSpriteDebrisMove(EffectWork* work, GfxCoord* coord)
{
    enum { EFFECT_SPRITE_DEBRIS_GRAVITY = 6 }; // Coordinate units per running update squared

    coord->coord.t[0]  += work->move.vx;
    coord->coord.t[1]  += work->move.vy;
    coord->coord.t[2]  += work->move.vz;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    work->move.vy      += EFFECT_SPRITE_DEBRIS_GRAVITY;
}

void EFFECT_SPRITE_DEBRIS_TASK(Task* task)
{
    enum {
        EFFECT_SPRITE_DEBRIS_INITIALIZE           = 0,
        EFFECT_SPRITE_DEBRIS_CHIP                 = 1,
        EFFECT_SPRITE_DEBRIS_BILLBOARD            = 2,
        EFFECT_SPRITE_DEBRIS_SIZE_MASK            = 0xFFF,
        EFFECT_SPRITE_DEBRIS_ANGLE_MASK           = 0xFFF,
        EFFECT_SPRITE_DEBRIS_PERIOD_NIBBLE_MASK   = 0xF000,
        EFFECT_SPRITE_DEBRIS_PERIOD_SHIFT         = 12,
        EFFECT_SPRITE_DEBRIS_PERIOD_MASK          = 0xF,
        EFFECT_SPRITE_DEBRIS_SPEED_BYTE_MASK      = 0xFF0000,
        EFFECT_SPRITE_DEBRIS_SPEED_SHIFT          = 16,
        EFFECT_SPRITE_DEBRIS_SPEED_MASK           = 0xFF,
        EFFECT_SPRITE_DEBRIS_DEFAULT_SPEED        = 0x40,
        EFFECT_SPRITE_DEBRIS_DRAWER_NIBBLE_MASK   = 0xF0000000,
        EFFECT_SPRITE_DEBRIS_MOVEMENT_BYTE        = 3,
        EFFECT_SPRITE_DEBRIS_MOVEMENT_MASK        = 0xF,
        EFFECT_SPRITE_DEBRIS_STILL                = 0,
        EFFECT_SPRITE_DEBRIS_RANDOM_UPWARD        = 1,
        EFFECT_SPRITE_DEBRIS_RANDOM_ALL_AXES      = 2,
        EFFECT_SPRITE_DEBRIS_RANDOM_NARROW_UPWARD = 3,
        EFFECT_SPRITE_DEBRIS_OFFSET_DIRECTION     = 5,
        // Signed-halfword encoding of -64 before the random upward Y offset.
        EFFECT_SPRITE_DEBRIS_UPWARD_Y_BIAS = 0xFFC0,
        EFFECT_SPRITE_DEBRIS_FRAME_COUNT   = 8
    };
    EffectWork* work;
    GfxCoord*   coord;
    s32         movementKind;
    s32         framesPerCell;
    s32         drawState;
    s32         speed;

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        // Suspension and cancellation both redraw as a chip, regardless of state.
        _effectSpriteDrawChip(coord, work->index, work->scale, work->angle);
        if (gRoomEffectState->effectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            effectKillTask(work, task);
        }
        return;
    }
    work->age++;
    switch (task->state) {
        case EFFECT_SPRITE_DEBRIS_INITIALIZE:
            // Initialization consumes a running update without drawing or moving.
            work->scale     = task->spawnArg1.halves.low & EFFECT_SPRITE_DEBRIS_SIZE_MASK;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->angle     = (gRandomLcgState >> 16) & EFFECT_SPRITE_DEBRIS_ANGLE_MASK;
            if (task->spawnArg1.value & EFFECT_SPRITE_DEBRIS_PERIOD_NIBBLE_MASK) {
                framesPerCell = (task->spawnArg1.value >> EFFECT_SPRITE_DEBRIS_PERIOD_SHIFT) & EFFECT_SPRITE_DEBRIS_PERIOD_MASK;
            } else {
                framesPerCell = 1;
            }
            work->period = framesPerCell;
            work->age    = 0;
            drawState    = EFFECT_SPRITE_DEBRIS_CHIP;
            if (task->spawnArg1.value & EFFECT_SPRITE_DEBRIS_DRAWER_NIBBLE_MASK) {
                drawState = EFFECT_SPRITE_DEBRIS_BILLBOARD;
            }
            task->state = drawState;
            if (((u16)work->move.vx | (u16)work->move.vy | (u16)work->move.vz) == 0) {
                // A caller-supplied nonzero velocity bypasses direction generation.
                if (task->spawnArg1.value & EFFECT_SPRITE_DEBRIS_SPEED_BYTE_MASK) {
                    speed = (task->spawnArg1.value >> EFFECT_SPRITE_DEBRIS_SPEED_SHIFT) & EFFECT_SPRITE_DEBRIS_SPEED_MASK;
                } else {
                    speed = EFFECT_SPRITE_DEBRIS_DEFAULT_SPEED;
                }
                work->step   = speed;
                movementKind = task->spawnArg1.signedBytes[EFFECT_SPRITE_DEBRIS_MOVEMENT_BYTE];
                switch (movementKind & EFFECT_SPRITE_DEBRIS_MOVEMENT_MASK) {
                    case EFFECT_SPRITE_DEBRIS_STILL:
                        work->step = 0;
                        break;
                    case EFFECT_SPRITE_DEBRIS_RANDOM_UPWARD:
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        work->move.vx   = 0x80 - ((gRandomLcgState >> 16) & 0xFF);
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        work->move.vy   = EFFECT_SPRITE_DEBRIS_UPWARD_Y_BIAS - ((gRandomLcgState >> 16) & 0x7F);
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        work->move.vz   = 0x80 - ((gRandomLcgState >> 16) & 0xFF);
                        break;
                    case EFFECT_SPRITE_DEBRIS_RANDOM_ALL_AXES:
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        work->move.vx   = 0x80 - ((gRandomLcgState >> 16) & 0xFF);
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        work->move.vy   = 0x80 - ((gRandomLcgState >> 16) & 0xFF);
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        work->move.vz   = 0x80 - ((gRandomLcgState >> 16) & 0xFF);
                        break;
                    case EFFECT_SPRITE_DEBRIS_RANDOM_NARROW_UPWARD:
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        work->move.vx   = 0x10 - ((gRandomLcgState >> 16) & 0x1F);
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        work->move.vy   = -((gRandomLcgState >> 16) & 0xFF);
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        work->move.vz   = 0x10 - ((gRandomLcgState >> 16) & 0x1F);
                        break;
                    case EFFECT_SPRITE_DEBRIS_OFFSET_DIRECTION:
                        work->move.vx = work->pos.vx;
                        work->move.vy = work->pos.vy;
                        work->move.vz = work->pos.vz;
                        break;
                }
                _effectSpriteDebrisInitializeVelocity(work);
            } else {
                work->step = EFFECT_SPRITE_DEBRIS_DEFAULT_SPEED;
            }
            return;
        case EFFECT_SPRITE_DEBRIS_CHIP:
            _effectSpriteDrawChip(coord, work->index, work->scale, work->angle);
            break;
        case EFFECT_SPRITE_DEBRIS_BILLBOARD:
            _effectSpriteDrawBillboard(coord, (u16)work->index, work->scale);
            break;
        default:
            return;
    }
    // Draw at the current location; movement precedes acceleration and cell advance.
    if (work->step != 0) {
        _effectSpriteDebrisMove(work, coord);
    }
    if ((work->age % work->period) == 0) {
        work->index++;
        if (work->index >= EFFECT_SPRITE_DEBRIS_FRAME_COUNT) {
            effectKillTask(work, task);
        }
    }
}

#undef EFFECT_SPRITE_DEBRIS_TASK
