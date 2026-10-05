#include "main/random.h"

/* Included drift-task implementation; exported callback declarations and
 * contracts are in the carriers' public headers. Five rooms instantiate it.
 * Drawer bindings are function identifiers, not expression-like macros. */

#ifndef EFFECT_SPRITE_DRIFT_TASK
#error "Bind EFFECT_SPRITE_DRIFT_TASK to the carrier's void (Task*) callback before inclusion"
#endif
#ifndef EFFECT_SPRITE_DRIFT_DRAW_BANKED
/// Binds the twelve-frame drawer used by the drift task's banked state.
///
/// Supply a function identifier accepting `(const GfxCoord*, u16, s16, s16)`:
/// composed coordinate, packed frame/palette, perspective size, and angle in
/// 4096 units per turn. It borrows the coordinate and retains no pointer.
/// The default uses per-frame palettes; three carriers bind local variants.
/// Dryfield R08 also uses this drawer with palette zero during suspension.
/// Calls evaluate each argument once. This binding is cleared after inclusion.
#define EFFECT_SPRITE_DRIFT_DRAW_BANKED _effectSpriteDrawBanked
#endif
#ifndef EFFECT_SPRITE_DRIFT_DRAW_ALTERNATE
/// Binds the ten-frame drawer used by the drift task's alternate state.
///
/// The function identifier has the same borrowed-coordinate signature and units
/// as `EFFECT_SPRITE_DRIFT_DRAW_BANKED`; nonzero palette bits select its alternate
/// palette. Pod bottom and pod access tunnel use the default, while three rooms
/// bind local variants. Arguments are evaluated once; the binding is cleared
/// after inclusion. Neither drawer binding manufactures tokens or captures locals.
#define EFFECT_SPRITE_DRIFT_DRAW_ALTERNATE _effectSpriteDrawRotated
#endif

/// Converts a drift sprite's initial direction into velocity at its selected speed.
///
/// `work` borrows the task-owned effect work. `move` initially contains a signed
/// direction; `step` supplies speed in coordinate units per running update
/// (0 disables movement, otherwise the drift initializer selects 1..255).
/// In-place Q12 normalization precedes speed scaling; the resulting signed
/// halfword components are the displacement added to the coordinate each update.
///
/// Only the three `move` components change; `step` and the SVECTOR pad remain
/// intact, and no pointer is retained. Zero directions follow `VectorNormalSS`
/// without a special case. The operation clobbers GTE data and result registers.
static __inline__ void _effectSpriteDriftInitializeVelocity(EffectWork* work)
{
    SVECTOR* velocity;

    velocity = &work->move;
    VectorNormalSS(velocity, velocity);
    gte_lddp(work->step);
    gte_ldsv(velocity);
    gte_gpf12();
    gte_stsv(velocity);
}

/// Moves the sprite by its signed-halfword velocity and invalidates the composed transform.
///
/// `coord` and `work` are borrowed from a live effect task. Velocity components
/// are coordinate units per running update; translation uses signed 32-bit
/// arithmetic. Acceleration and animation advancement remain with the caller.
static __inline__ void _effectSpriteDriftMove(EffectWork* work, GfxCoord* coord)
{
    coord->coord.t[0]  += work->move.vx;
    coord->coord.t[1]  += work->move.vy;
    coord->coord.t[2]  += work->move.vz;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
}

void EFFECT_SPRITE_DRIFT_TASK(Task* task)
{
    enum {
        EFFECT_SPRITE_DRIFT_INITIALIZE         = 0,
        EFFECT_SPRITE_DRIFT_BANKED             = 1,
        EFFECT_SPRITE_DRIFT_ALTERNATE          = 2,
        EFFECT_SPRITE_DRIFT_SIZE_MASK          = 0xFFF,
        EFFECT_SPRITE_DRIFT_SPIN_MASK          = 0xFFF,
        EFFECT_SPRITE_DRIFT_DRAWER_NIBBLE_MASK = 0xF0000000,
        // Signed-halfword encoding of -64 before the random upward Y offset.
        EFFECT_SPRITE_DRIFT_UPWARD_Y_BIAS = 0xFFC0,
        // Bit 15 participates in the fallback test but not in the extracted period.
        EFFECT_SPRITE_DRIFT_PERIOD_NIBBLE_MASK    = 0xF000,
        EFFECT_SPRITE_DRIFT_PERIOD_SHIFT          = 12,
        EFFECT_SPRITE_DRIFT_PERIOD_MASK           = 7,
        EFFECT_SPRITE_DRIFT_SPEED_MASK            = 0xFF0000,
        EFFECT_SPRITE_DRIFT_SPEED_SHIFT           = 16,
        EFFECT_SPRITE_DRIFT_DEFAULT_SPEED         = 0x40,
        EFFECT_SPRITE_DRIFT_MOVEMENT_SHIFT        = 24,
        EFFECT_SPRITE_DRIFT_MOVEMENT_MASK         = 0xF,
        EFFECT_SPRITE_DRIFT_PALETTE_MASK          = 0x7000,
        EFFECT_SPRITE_DRIFT_PALETTE_SHIFT         = 12,
        EFFECT_SPRITE_DRIFT_STILL                 = 0,
        EFFECT_SPRITE_DRIFT_RANDOM_UPWARD         = 1,
        EFFECT_SPRITE_DRIFT_RANDOM_ALL_AXES       = 2,
        EFFECT_SPRITE_DRIFT_RANDOM_NARROW_UPWARD  = 3,
        EFFECT_SPRITE_DRIFT_OFFSET_DIRECTION      = 5,
        EFFECT_SPRITE_DRIFT_RANDOM_PLANAR         = 6,
        EFFECT_SPRITE_DRIFT_AGE_ACCELERATION      = 7,
        EFFECT_SPRITE_DRIFT_BANKED_FRAME_COUNT    = 12,
        EFFECT_SPRITE_DRIFT_ALTERNATE_FRAME_COUNT = 10
    };
    EffectWork* work;
    GfxCoord*   coord;
    s32         framesPerCell;
    s32         speed;
#if EFFECT_SPRITE_DRIFT_SIGN_BANK
    s32 zeroSignReference;
#endif

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        // Every suspended or cancelling update redraws before any work is released.
#if EFFECT_SPRITE_DRIFT_PAUSED_DRAW_A_UNBANKED
        // Suspended sprites use the banked base palette even in the alternate state.
        EFFECT_SPRITE_DRIFT_DRAW_BANKED(coord, work->index, work->scale, work->angle);
#elif EFFECT_SPRITE_DRIFT_SIGN_BANK
        // Suspension reselects the drawer from the sign, rather than the running state.
        if (task->spawnArg1.value < 0) {
            EFFECT_SPRITE_DRIFT_DRAW_ALTERNATE(coord, work->index | work->pos.vx, work->scale, work->angle);
        } else {
            EFFECT_SPRITE_DRIFT_DRAW_BANKED(coord, work->index | work->pos.vx, work->scale, work->angle);
        }
#else
        if (task->state < EFFECT_SPRITE_DRIFT_ALTERNATE) {
            EFFECT_SPRITE_DRIFT_DRAW_BANKED(coord, work->index | work->pos.vx, work->scale, work->angle);
        } else {
            EFFECT_SPRITE_DRIFT_DRAW_ALTERNATE(coord, work->index | work->pos.vx, work->scale, work->angle);
        }
#endif
        if (gRoomEffectState->effectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            effectKillTask(work, task);
        }
        return;
    }
    work->age++;
    switch (task->state) {
        case EFFECT_SPRITE_DRIFT_INITIALIZE:
            // Initialization consumes one random spin sample and does not draw.
            work->scale     = task->spawnArg1.value & EFFECT_SPRITE_DRIFT_SIZE_MASK;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->angle     = (gRandomLcgState >> 16) & EFFECT_SPRITE_DRIFT_SPIN_MASK;
            if (task->spawnArg1.value & EFFECT_SPRITE_DRIFT_PERIOD_NIBBLE_MASK) {
                framesPerCell = (task->spawnArg1.value >> EFFECT_SPRITE_DRIFT_PERIOD_SHIFT) & EFFECT_SPRITE_DRIFT_PERIOD_MASK;
            } else {
                framesPerCell = 1;
            }
            work->period = framesPerCell;
            work->age    = 0;
#if EFFECT_SPRITE_DRIFT_SIGN_BANK
            // The high nibble chooses the running drawer; its sign alone chooses the palette.
            task->state       = task->spawnArg1.value & EFFECT_SPRITE_DRIFT_DRAWER_NIBBLE_MASK ? EFFECT_SPRITE_DRIFT_ALTERNATE : EFFECT_SPRITE_DRIFT_BANKED;
            zeroSignReference = 0;
            work->pos.vx      = (task->spawnArg1.value < zeroSignReference) << EFFECT_SPRITE_DRIFT_PALETTE_SHIFT;
#else
            task->state  = EFFECT_SPRITE_DRIFT_BANKED;
            task->state  = task->spawnArg1.value < 0 ? EFFECT_SPRITE_DRIFT_ALTERNATE : EFFECT_SPRITE_DRIFT_BANKED;
            work->pos.vx = (task->spawnArg1.value >> EFFECT_SPRITE_DRIFT_SPEED_SHIFT) & EFFECT_SPRITE_DRIFT_PALETTE_MASK;
#endif
            if ((work->move.vx | work->move.vy | work->move.vz) == 0) {
                if (task->spawnArg1.value & EFFECT_SPRITE_DRIFT_SPEED_MASK) {
                    speed = (task->spawnArg1.value >> EFFECT_SPRITE_DRIFT_SPEED_SHIFT) & 0xFF;
                } else {
                    speed = EFFECT_SPRITE_DRIFT_DEFAULT_SPEED;
                }
                work->step = speed;
                switch ((task->spawnArg1.value >> EFFECT_SPRITE_DRIFT_MOVEMENT_SHIFT) & EFFECT_SPRITE_DRIFT_MOVEMENT_MASK) {
                    case EFFECT_SPRITE_DRIFT_STILL:
                        work->step = 0;
                        break;
                    case EFFECT_SPRITE_DRIFT_RANDOM_UPWARD:
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        work->move.vx   = 0x80 - ((gRandomLcgState >> 16) & 0xFF);
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        work->move.vy   = EFFECT_SPRITE_DRIFT_UPWARD_Y_BIAS - ((gRandomLcgState >> 16) & 0x7F);
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        work->move.vz   = 0x80 - ((gRandomLcgState >> 16) & 0xFF);
                        break;
                    case EFFECT_SPRITE_DRIFT_RANDOM_ALL_AXES:
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        work->move.vx   = 0x80 - ((gRandomLcgState >> 16) & 0xFF);
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        work->move.vy   = 0x80 - ((gRandomLcgState >> 16) & 0xFF);
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        work->move.vz   = 0x80 - ((gRandomLcgState >> 16) & 0xFF);
                        break;
                    case EFFECT_SPRITE_DRIFT_RANDOM_NARROW_UPWARD:
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        work->move.vx   = 0x10 - ((gRandomLcgState >> 16) & 0x1F);
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        work->move.vy   = -((gRandomLcgState >> 16) & 0xFF);
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        work->move.vz   = 0x10 - ((gRandomLcgState >> 16) & 0x1F);
                        break;
                    case EFFECT_SPRITE_DRIFT_OFFSET_DIRECTION:
                        // X now contains palette bits; Y and Z retain the spawn offset.
                        work->move.vx = work->pos.vx;
                        work->move.vy = work->pos.vy;
                        work->move.vz = work->pos.vz;
                        break;
                    case EFFECT_SPRITE_DRIFT_RANDOM_PLANAR:
                        work->move.vy   = 0;
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        work->move.vx   = 0x80 - ((gRandomLcgState >> 16) & 0xFF);
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        work->move.vz   = 0x80 - ((gRandomLcgState >> 16) & 0xFF);
                        break;
                }
                _effectSpriteDriftInitializeVelocity(work);
            } else {
                work->step = EFFECT_SPRITE_DRIFT_DEFAULT_SPEED;
            }
            break;
        case EFFECT_SPRITE_DRIFT_BANKED:
            // Draw the current cell before moving, accelerating, or retiring it.
            EFFECT_SPRITE_DRIFT_DRAW_BANKED(coord, work->index | work->pos.vx, work->scale, work->angle);
            if (work->step != 0) {
                _effectSpriteDriftMove(work, coord);
#if !EFFECT_SPRITE_DRIFT_FIXED_NEGATIVE_Y_ACCELERATION
                if (((task->spawnArg1.value >> EFFECT_SPRITE_DRIFT_MOVEMENT_SHIFT) & EFFECT_SPRITE_DRIFT_MOVEMENT_MASK) == EFFECT_SPRITE_DRIFT_AGE_ACCELERATION) {
                    work->move.vy += work->age / 10;
                } else
#endif
                {
                    work->move.vy -= 2;
                }
            }
            if ((work->age % work->period) == 0) {
                work->index++;
                if (work->index >= EFFECT_SPRITE_DRIFT_BANKED_FRAME_COUNT) {
                    effectKillTask(work, task);
                }
            }
            break;
        case EFFECT_SPRITE_DRIFT_ALTERNATE:
            EFFECT_SPRITE_DRIFT_DRAW_ALTERNATE(coord, work->index | work->pos.vx, work->scale, work->angle);
            if (work->step != 0) {
                _effectSpriteDriftMove(work, coord);
#if !EFFECT_SPRITE_DRIFT_FIXED_NEGATIVE_Y_ACCELERATION
                if (((task->spawnArg1.value >> EFFECT_SPRITE_DRIFT_MOVEMENT_SHIFT) & EFFECT_SPRITE_DRIFT_MOVEMENT_MASK) == EFFECT_SPRITE_DRIFT_AGE_ACCELERATION) {
                    work->move.vy += work->age / 10;
                } else
#endif
                {
                    work->move.vy -= 1;
                }
            }
            if ((work->age % work->period) == 0) {
                work->index++;
                if (work->index >= EFFECT_SPRITE_DRIFT_ALTERNATE_FRAME_COUNT) {
                    effectKillTask(work, task);
                }
            }
            break;
    }
}

#undef EFFECT_SPRITE_DRIFT_TASK
#undef EFFECT_SPRITE_DRIFT_PAUSED_DRAW_A_UNBANKED
#undef EFFECT_SPRITE_DRIFT_SIGN_BANK
#undef EFFECT_SPRITE_DRIFT_FIXED_NEGATIVE_Y_ACCELERATION
#undef EFFECT_SPRITE_DRIFT_DRAW_BANKED
#undef EFFECT_SPRITE_DRIFT_DRAW_ALTERNATE
