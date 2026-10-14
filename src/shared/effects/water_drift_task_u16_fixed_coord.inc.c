#include "gameplay/message.h"

#include "main/random.h"

/* Part of the water effects library; see water_effects.h. */

#ifndef WATER_SPRAY_TASK
#error "Define WATER_SPRAY_TASK to the package's declared void (Task*) callback"
#endif

/// Initializes a water-spray particle's velocity from its launch direction and speed.
///
/// `particleWork` borrows live, writable task-owned work. `move` contains a
/// signed direction and `step` the launch speed in parent-coordinate units per
/// running update (0 stationary, otherwise 1..255). The direction's squared
/// length must fit the SDK's signed 32-bit accumulation.
///
/// In-place Q12 normalization precedes speed multiplication with a 12-bit
/// arithmetic right shift. The resulting signed halfwords in `move` are integer
/// displacements in the same parent space; rounding can change their magnitude.
/// Zero directions still pass through the SDK normalizer.
///
/// Only the three components of `move` change; its pad, `step` and the remaining
/// work are preserved. No pointer is retained. GTE state is overwritten.
static inline void _waterSprayInitializeVelocity(EffectWork* particleWork)
{
    SVECTOR* velocity = &particleWork->move;

    VectorNormalSS(velocity, velocity);
    // Reload speed and normalized direction after the SDK call clobbers GTE inputs.
    gte_lddp(particleWork->step);
    gte_ldsv(velocity);
    gte_gpf12();
    gte_stsv(velocity);
}

void WATER_SPRAY_TASK(Task* task)
{
    enum {
        WATER_SPRAY_STATE_NEW              = 0,
        WATER_SPRAY_STATE_ROTATED          = 1,
        WATER_SPRAY_STATE_UPRIGHT          = 2,
        WATER_SPRAY_SPAWN_SIZE_MASK        = 0xFFF,
        WATER_SPRAY_SPAWN_PERIOD_MASK      = 0xF000,
        WATER_SPRAY_SPAWN_PERIOD_SHIFT     = 12,
        WATER_SPRAY_PERIOD_MASK            = 0xF,
        WATER_SPRAY_SPAWN_SPEED_MASK       = 0xFF0000,
        WATER_SPRAY_SPAWN_SPEED_SHIFT      = 16,
        WATER_SPRAY_SPEED_MASK             = 0xFF,
        WATER_SPRAY_SPAWN_UPRIGHT_MASK     = 0xF0000000,
        WATER_SPRAY_VELOCITY_KIND_MASK     = 0xF,
        WATER_SPRAY_VELOCITY_STATIONARY    = 0,
        WATER_SPRAY_VELOCITY_UPWARD_BURST  = 1,
        WATER_SPRAY_VELOCITY_ALL_AXES      = 2,
        WATER_SPRAY_VELOCITY_NARROW_JET    = 3,
        WATER_SPRAY_VELOCITY_POS_DIRECTION = 5,
        WATER_SPRAY_DEFAULT_SPEED          = 0x40,
        WATER_SPRAY_DEFAULT_PERIOD         = 1,
        // Signed-halfword encoding of -64 before the random upward Y offset.
        WATER_SPRAY_UPWARD_Y_BIAS      = 0xFFC0,
        WATER_SPRAY_GRAVITY_PER_UPDATE = 6,
        WATER_SPRAY_CELL_COUNT         = 8
    };
    EffectWork* particleWork;
    GfxCoord*   particleCoord;
    s32         velocityKind;
    s32         updatesPerCell;
    s32         launchSpeed;

    particleWork  = task->spawnArg2.pointer;
    particleCoord = task->extra.coordBody->coord;
    if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        // Redraw cached state even on the update that cancels the particle.
        if (task->state < WATER_SPRAY_STATE_UPRIGHT) {
            _waterDrawSpinU16(particleCoord, (u16)particleWork->index, particleWork->scale, particleWork->angle);
        } else {
            _waterDrawTileU16(particleCoord, (u16)particleWork->index, particleWork->scale);
        }
        if (gRoomEffectState->effectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            effectKillTask(particleWork, task);
        }
        return;
    }
    particleWork->age++;
    switch (task->state) {
        case WATER_SPRAY_STATE_NEW:
            // Decode independent spawn fields; the random angle remains fixed.
            particleWork->scale = task->spawnArg1.halves.low & WATER_SPRAY_SPAWN_SIZE_MASK;
            gRandomLcgState     = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            particleWork->angle = (gRandomLcgState >> 16) & ACTOR_TRANSFORM_ANGLE_MASK;
            if (task->spawnArg1.value & WATER_SPRAY_SPAWN_PERIOD_MASK) {
                updatesPerCell = (task->spawnArg1.value >> WATER_SPRAY_SPAWN_PERIOD_SHIFT) & WATER_SPRAY_PERIOD_MASK;
            } else {
                updatesPerCell = WATER_SPRAY_DEFAULT_PERIOD;
            }
            particleWork->period = updatesPerCell;
            particleWork->age    = 0;
            task->state          = task->spawnArg1.value & WATER_SPRAY_SPAWN_UPRIGHT_MASK ? WATER_SPRAY_STATE_UPRIGHT : WATER_SPRAY_STATE_ROTATED;
            if ((particleWork->move.vx | particleWork->move.vy | particleWork->move.vz) == 0) {
                if (task->spawnArg1.value & WATER_SPRAY_SPAWN_SPEED_MASK) {
                    launchSpeed = (task->spawnArg1.value >> WATER_SPRAY_SPAWN_SPEED_SHIFT) & WATER_SPRAY_SPEED_MASK;
                } else {
                    launchSpeed = WATER_SPRAY_DEFAULT_SPEED;
                }
                particleWork->step = launchSpeed;
                velocityKind       = task->spawnArg1.signedBytes[3];
                switch (velocityKind & WATER_SPRAY_VELOCITY_KIND_MASK) {
                    case WATER_SPRAY_VELOCITY_STATIONARY:
                        particleWork->step = 0;
                        break;
                    case WATER_SPRAY_VELOCITY_UPWARD_BURST:
                        gRandomLcgState       = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        particleWork->move.vx = 0x80 - ((gRandomLcgState >> 16) & 0xFF);
                        gRandomLcgState       = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        particleWork->move.vy = WATER_SPRAY_UPWARD_Y_BIAS - ((gRandomLcgState >> 16) & 0x7F);
                        gRandomLcgState       = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        particleWork->move.vz = 0x80 - ((gRandomLcgState >> 16) & 0xFF);
                        break;
                    case WATER_SPRAY_VELOCITY_ALL_AXES:
                        gRandomLcgState       = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        particleWork->move.vx = 0x80 - ((gRandomLcgState >> 16) & 0xFF);
                        gRandomLcgState       = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        particleWork->move.vy = 0x80 - ((gRandomLcgState >> 16) & 0xFF);
                        gRandomLcgState       = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        particleWork->move.vz = 0x80 - ((gRandomLcgState >> 16) & 0xFF);
                        break;
                    case WATER_SPRAY_VELOCITY_NARROW_JET:
                        gRandomLcgState       = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        particleWork->move.vx = 0x10 - ((gRandomLcgState >> 16) & 0x1F);
                        gRandomLcgState       = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        particleWork->move.vy = -((gRandomLcgState >> 16) & 0xFF);
                        gRandomLcgState       = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        particleWork->move.vz = 0x10 - ((gRandomLcgState >> 16) & 0x1F);
                        break;
                    case WATER_SPRAY_VELOCITY_POS_DIRECTION:
                        // Use the copied spawn offset as a direction without transforming it.
                        particleWork->move.vx = particleWork->pos.vx;
                        particleWork->move.vy = particleWork->pos.vy;
                        particleWork->move.vz = particleWork->pos.vz;
                        break;
                }
                _waterSprayInitializeVelocity(particleWork);
            } else {
                // A supplied velocity bypasses scaling; step only enables movement.
                particleWork->step = WATER_SPRAY_DEFAULT_SPEED;
            }
            return;
        case WATER_SPRAY_STATE_ROTATED:
            _waterDrawSpinU16(particleCoord, (u16)particleWork->index, particleWork->scale, particleWork->angle);
            break;
        case WATER_SPRAY_STATE_UPRIGHT:
            _waterDrawTileU16(particleCoord, (u16)particleWork->index, particleWork->scale);
            break;
        default:
            return;
    }
    // Update local translation without recomposing the cached matrix used to draw.
    if (particleWork->step != 0) {
        particleCoord->coord.t[0]  += particleWork->move.vx;
        particleCoord->coord.t[1]  += particleWork->move.vy;
        particleCoord->coord.t[2]  += particleWork->move.vz;
        particleCoord->composeStamp = GRAPHICS_COORD_DIRTY;
        particleWork->move.vy      += WATER_SPRAY_GRAVITY_PER_UPDATE;
    }
    if ((particleWork->age % particleWork->period) == 0) {
        particleWork->index++;
        if (particleWork->index >= WATER_SPRAY_CELL_COUNT) {
            effectKillTask(particleWork, task);
        }
    }
}
