#include "gameplay/message.h"

#include "main/random.h"

/* Part of the water effects library; see water_effects.h. */

/// Initializes a water-spray particle's parent-space velocity from its launch direction.
///
/// Borrows a live, writable `particleWork`. Its signed `move` components
/// supply a direction in the particle coordinate's parent space; `step`
/// supplies speed in parent-coordinate units per running update (0..255,
/// with 0 stationary). The direction is normalized in place to Q12, then
/// multiplied by the signed speed read after normalization with a 12-bit
/// arithmetic right shift. The stored velocity is an integer displacement,
/// not Q12; SDK approximation and rounding can change its magnitude.
///
/// Only `move.vx`, `move.vy` and `move.vz` are written. `move.pad`, `step`
/// and the rest of the work are preserved. Zero speed or direction still
/// reaches the SDK normalizer. No pointer is retained or storage released.
/// GTE state is clobbered, including its colour FIFO.
static inline void _waterInitializeLaunchVelocity(EffectWork* particleWork)
{
    SVECTOR* launchVelocity = &particleWork->move;

    VectorNormalSS(launchVelocity, launchVelocity);
    gte_lddp(particleWork->step);
    gte_ldsv(launchVelocity);
    gte_gpf12();
    gte_stsv(launchVelocity);
}

/* WATER_DRIFT_UNCOMPOSED_TASK must name the carrier's externally linked
 * void (Task*) callback, declared in its public room header. The carrier
 * supplies the unsigned-index water drawers and undefines this binding
 * after including the body. This task draws the cached coordinate matrix
 * without composing the local translation it updates.
 */
void WATER_DRIFT_UNCOMPOSED_TASK(Task* task)
{
    enum {
        WATER_DRIFT_STATE_NEW              = 0,
        WATER_DRIFT_STATE_ROTATED          = 1,
        WATER_DRIFT_STATE_UPRIGHT          = 2,
        WATER_DRIFT_SPAWN_SIZE_MASK        = 0xFFF,
        WATER_DRIFT_SPAWN_PERIOD_MASK      = 0xF000,
        WATER_DRIFT_SPAWN_PERIOD_SHIFT     = 12,
        WATER_DRIFT_PERIOD_MASK            = 0xF,
        WATER_DRIFT_DEFAULT_PERIOD         = 1,
        WATER_DRIFT_SPAWN_SPEED_MASK       = 0xFF0000,
        WATER_DRIFT_SPAWN_SPEED_SHIFT      = 16,
        WATER_DRIFT_SPEED_MASK             = 0xFF,
        WATER_DRIFT_DEFAULT_SPEED          = 0x40,
        WATER_DRIFT_SPAWN_UPRIGHT_MASK     = 0xF0000000,
        WATER_DRIFT_VELOCITY_KIND_MASK     = 0xF,
        WATER_DRIFT_VELOCITY_STATIONARY    = 0,
        WATER_DRIFT_VELOCITY_UPWARD_BURST  = 1,
        WATER_DRIFT_VELOCITY_ALL_AXES      = 2,
        WATER_DRIFT_VELOCITY_NARROW_JET    = 3,
        WATER_DRIFT_VELOCITY_POS_DIRECTION = 5,
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
                _waterDrawSpinU16(particleCoord, particleWork->index, particleWork->scale, particleWork->angle);
            } else {
                _waterDrawTileU16(particleCoord, particleWork->index, particleWork->scale);
            }
            return;
        }
        effectKillTask(particleWork, task);
        return;
    }
    particleWork->age++;
    switch (task->state) {
        case WATER_DRIFT_STATE_NEW:
            // Decode independent spawn fields; retain the random angle for every cell.
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
                        // The copied spawn offset supplies a direction in the coordinate's parent space.
                        particleWork->move.vx = particleWork->pos.vx;
                        particleWork->move.vy = particleWork->pos.vy;
                        particleWork->move.vz = particleWork->pos.vz;
                        break;
                }
                _waterInitializeLaunchVelocity(particleWork);
            } else {
                // A supplied velocity bypasses scaling; step only enables motion.
                particleWork->step = WATER_DRIFT_DEFAULT_SPEED;
            }
            return;
        case WATER_DRIFT_STATE_ROTATED:
            _waterDrawSpinU16(particleCoord, particleWork->index, particleWork->scale, particleWork->angle);
            break;
        case WATER_DRIFT_STATE_UPRIGHT:
            _waterDrawTileU16(particleCoord, particleWork->index, particleWork->scale);
            break;
        default:
            return;
    }
    // Local motion dirties the cache; this task never rebuilds the matrix used by its drawers.
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
