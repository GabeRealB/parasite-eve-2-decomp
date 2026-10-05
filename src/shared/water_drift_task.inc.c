#include "gameplay/message.h"

#include "main/random.h"

/* Part of the water effects library; see water_effects.h. */

/// Converts a water particle's direction into velocity at its selected speed.
///
/// Borrows the live effect work. `move` is normalized in place to Q12, then
/// multiplied by `step` in coordinate units per running update. Only its three
/// signed halfword components change; no pointer is retained. Zero directions
/// follow the SDK normalization without a special case. GTE state is overwritten.
static inline void _waterDriftInitializeVelocity(EffectWork* particleWork)
{
    SVECTOR* velocity;

    velocity = &particleWork->move;
    VectorNormalSS(velocity, velocity);
    gte_lddp(particleWork->step);
    gte_ldsv(velocity);
    gte_gpf12();
    gte_stsv(velocity);
}

/// Advances and draws one eight-frame water-spray particle.
///
/// `task` must be a live counted effect with an owned `EffectWork` in
/// `spawnArg2.pointer`, one coordinate body, initial state 0 and frame index 0.
/// The coordinate's translation and velocity are in its parent's space;
/// the effect spawner parents it to the view coordinate.
///
/// `spawnArg1` bits 0..11 give the perspective size scale (0..4095), bits
/// 12..15 the running updates per animation cell (0 selects 1), bits 16..23
/// the speed in coordinate units per update (0 selects 64), and bits 24..27
/// the velocity kind (0 stationary, 1 upward burst, 2 all-axis spray, 3 narrow
/// upward jet, 5 direction from `pos`). Other kinds leave the zero direction
/// unchanged before normalization. Any nonzero bits 28..31 select the upright
/// tile; otherwise the quad retains a random angle in 4096 units per turn.
/// An already nonzero `move` is used unchanged, ignoring the speed and kind.
/// `step` then holds 64 as the movement enable value, rather than that velocity's speed.
///
/// The first running update composes and initializes without drawing or moving.
/// Later updates draw the current cell, move by signed halfword velocity, then
/// add 6 to its Y component, retaining the low 16 bits. A stationary particle
/// skips both movement and gravity. Cells 0..7 each last `period` running
/// updates; a fresh particle retires after another 8 * `period` updates.
///
/// Non-running control values below the cancellation threshold redraw without
/// composing, initializing or aging. Cancellation retires without drawing.
/// Retirement releases the work, decrements the effect count and tears down
/// the task and its coordinate body; callers must not retain released pointers.
static inline void _waterDriftTask(Task* task)
{
    enum {
        WATER_DRIFT_STATE_NEW              = 0,
        WATER_DRIFT_STATE_ROTATED          = 1,
        WATER_DRIFT_STATE_UPRIGHT          = 2,
        WATER_DRIFT_SPAWN_SIZE_MASK        = 0xFFF,
        WATER_DRIFT_SPAWN_PERIOD_MASK      = 0xF000,
        WATER_DRIFT_SPAWN_PERIOD_SHIFT     = 12,
        WATER_DRIFT_PERIOD_MASK            = 0xF,
        WATER_DRIFT_SPAWN_SPEED_MASK       = 0xFF0000,
        WATER_DRIFT_SPAWN_SPEED_SHIFT      = 16,
        WATER_DRIFT_SPEED_MASK             = 0xFF,
        WATER_DRIFT_SPAWN_UPRIGHT_MASK     = 0xF0000000,
        WATER_DRIFT_VELOCITY_KIND_MASK     = 0xF,
        WATER_DRIFT_VELOCITY_STATIONARY    = 0,
        WATER_DRIFT_VELOCITY_UPWARD_BURST  = 1,
        WATER_DRIFT_VELOCITY_ALL_AXES      = 2,
        WATER_DRIFT_VELOCITY_NARROW_JET    = 3,
        WATER_DRIFT_VELOCITY_POS_DIRECTION = 5,
        WATER_DRIFT_DEFAULT_SPEED          = 0x40,
        WATER_DRIFT_DEFAULT_PERIOD         = 1,
        // Signed-halfword encoding of -64, before the random upward Y offset.
        WATER_DRIFT_UPWARD_Y_BIAS      = 0xFFC0,
        WATER_DRIFT_GRAVITY_PER_UPDATE = 6,
        WATER_DRIFT_FRAME_COUNT        = 8
    };
    EffectWork* particleWork;
    GfxCoord*   particleCoord;
    s32         velocityKind;
    s32         framesPerCell;
    s32         speed;

    particleWork  = task->spawnArg2.pointer;
    particleCoord = task->extra.coordBody->coord;
    if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        // Frozen particles redraw their retained state; cancellation skips drawing.
        if (gRoomEffectState->effectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            if (task->state < WATER_DRIFT_STATE_UPRIGHT) {
                _waterDrawSpin(particleCoord, particleWork->index, particleWork->scale, particleWork->angle);
            } else {
                _waterDrawTile(particleCoord, particleWork->index, particleWork->scale);
            }
            return;
        }
        effectKillTask(particleWork, task);
        return;
    }
    actorRenderComposeCoord(particleCoord);
    particleWork->age++;
    switch (task->state) {
        case WATER_DRIFT_STATE_NEW:
            // Decode independent spawn fields; the angle remains fixed throughout the animation.
            particleWork->scale = task->spawnArg1.halves.low & WATER_DRIFT_SPAWN_SIZE_MASK;
            gRandomLcgState     = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            particleWork->angle = (gRandomLcgState >> 16) & ACTOR_TRANSFORM_ANGLE_MASK;
            if (task->spawnArg1.value & WATER_DRIFT_SPAWN_PERIOD_MASK) {
                framesPerCell = (task->spawnArg1.value >> WATER_DRIFT_SPAWN_PERIOD_SHIFT) & WATER_DRIFT_PERIOD_MASK;
            } else {
                framesPerCell = WATER_DRIFT_DEFAULT_PERIOD;
            }
            particleWork->period = framesPerCell;
            particleWork->age    = 0;
            task->state          = task->spawnArg1.value & WATER_DRIFT_SPAWN_UPRIGHT_MASK ? WATER_DRIFT_STATE_UPRIGHT : WATER_DRIFT_STATE_ROTATED;
            if ((particleWork->move.vx | particleWork->move.vy | particleWork->move.vz) == 0) {
                if (task->spawnArg1.value & WATER_DRIFT_SPAWN_SPEED_MASK) {
                    speed = (task->spawnArg1.value >> WATER_DRIFT_SPAWN_SPEED_SHIFT) & WATER_DRIFT_SPEED_MASK;
                } else {
                    speed = WATER_DRIFT_DEFAULT_SPEED;
                }
                particleWork->step = speed;
                velocityKind       = task->spawnArg1.signedBytes[3];
                switch (velocityKind & WATER_DRIFT_VELOCITY_KIND_MASK) {
                    case WATER_DRIFT_VELOCITY_STATIONARY:
                        particleWork->step = 0;
                        break;
                    case WATER_DRIFT_VELOCITY_UPWARD_BURST:
                        gRandomLcgState       = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        particleWork->move.vx = 0x80 - ((gRandomLcgState >> 16) & 0xFF);
                        gRandomLcgState       = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        particleWork->move.vy = WATER_DRIFT_UPWARD_Y_BIAS - ((gRandomLcgState >> 16) & 0x7F);
                        gRandomLcgState       = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        particleWork->move.vz = 0x80 - ((gRandomLcgState >> 16) & 0xFF);
                        break;
                    case WATER_DRIFT_VELOCITY_ALL_AXES:
                        gRandomLcgState       = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        particleWork->move.vx = 0x80 - ((gRandomLcgState >> 16) & 0xFF);
                        gRandomLcgState       = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        particleWork->move.vy = 0x80 - ((gRandomLcgState >> 16) & 0xFF);
                        gRandomLcgState       = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        particleWork->move.vz = 0x80 - ((gRandomLcgState >> 16) & 0xFF);
                        break;
                    case WATER_DRIFT_VELOCITY_NARROW_JET:
                        gRandomLcgState       = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        particleWork->move.vx = 0x10 - ((gRandomLcgState >> 16) & 0x1F);
                        gRandomLcgState       = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        particleWork->move.vy = -((gRandomLcgState >> 16) & 0xFF);
                        gRandomLcgState       = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        particleWork->move.vz = 0x10 - ((gRandomLcgState >> 16) & 0x1F);
                        break;
                    case WATER_DRIFT_VELOCITY_POS_DIRECTION:
                        // Reuse the spawner's copied offset as a direction, without transforming it.
                        particleWork->move.vx = particleWork->pos.vx;
                        particleWork->move.vy = particleWork->pos.vy;
                        particleWork->move.vz = particleWork->pos.vz;
                        break;
                }
                _waterDriftInitializeVelocity(particleWork);
            } else {
                // A supplied velocity bypasses generation and scaling; step only enables motion.
                particleWork->step = WATER_DRIFT_DEFAULT_SPEED;
            }
            return;
        case WATER_DRIFT_STATE_ROTATED:
            _waterDrawSpin(particleCoord, particleWork->index, particleWork->scale, particleWork->angle);
            break;
        case WATER_DRIFT_STATE_UPRIGHT:
            _waterDrawTile(particleCoord, particleWork->index, particleWork->scale);
            break;
        default:
            return;
    }
    // Draw before stepping, so the dirty translation is composed on the next running update.
    if (particleWork->step != 0) {
        particleCoord->coord.t[0]  += particleWork->move.vx;
        particleCoord->coord.t[1]  += particleWork->move.vy;
        particleCoord->coord.t[2]  += particleWork->move.vz;
        particleCoord->composeStamp = GRAPHICS_COORD_DIRTY;
        particleWork->move.vy      += WATER_DRIFT_GRAVITY_PER_UPDATE;
    }
    if ((particleWork->age % particleWork->period) == 0) {
        particleWork->index++;
        if (particleWork->index >= WATER_DRIFT_FRAME_COUNT) {
            effectKillTask(particleWork, task);
        }
    }
}
