#include "gameplay/message.h"

#include "main/random.h"

/* Part of the water effects library; see water_effects.h. */

/// Normalizes a water particle's launch direction and scales it to its speed.
///
/// Borrows live effect work. `move` becomes a Q12 unit direction, then a
/// velocity in parent-coordinate units per running update using `step`.
/// Only the three signed halfwords change; the SDK handles a zero direction
/// without a caller special case. No pointer is retained; GTE state changes.
static inline void _waterDriftNormalizeVelocity(EffectWork* particleWork)
{
    SVECTOR* velocity;

    velocity = &particleWork->move;
    VectorNormalSS(velocity, velocity);
    gte_lddp(particleWork->step);
    gte_ldsv(velocity);
    gte_gpf12();
    gte_stsv(velocity);
}

/// Advances and draws one eight-cell water-spray particle through unsigned-index drawers.
///
/// `task` must be a live counted effect with owned `EffectWork` in
/// `spawnArg2.pointer`, a coordinate body, initial state 0 and cell index 0.
/// Translation and velocity use the coordinate's parent space, normally the
/// view coordinate installed by the effect spawner. Declare the selected
/// drawers before including this fragment; both receive a u16 cell index,
/// including when their declared parameter is s32.
///
/// `spawnArg1` bits 0..11 supply perspective size (0..4095); bits 12..15
/// supply running updates per cell (0 selects 1); bits 16..23 supply launch
/// speed in coordinate units per update (0 selects 64). Bits 24..27 select
/// velocity kind: 0 stationary, 1 upward burst, 2 all-axis spray, 3 narrow
/// upward jet, 5 the spawner-copied `pos` direction. Other kinds leave the
/// zero direction unchanged before SDK normalization. Any nonzero bits
/// 28..31 select the upright drawer; otherwise a random angle is retained,
/// in 4096 units per turn. A supplied nonzero `move` bypasses generation and
/// scaling, with `step` set to 64 solely to enable movement.
///
/// The first running update composes and initializes without drawing or moving.
/// Later updates draw the current cell, move by the signed halfword velocity,
/// and add 6 to its Y component, keeping the low 16 bits. Stationary particles
/// skip movement and gravity. Cells 0..7 last `period` running updates each;
/// a fresh particle retires after initialization plus 8 * `period` updates.
///
/// Suspended control values below cancellation redraw without composing,
/// initializing or aging; cancellation releases without drawing. Retirement
/// frees the work, decrements the effect count and tears down the task and
/// coordinate body. Callers must not retain pointers after retirement.
static inline void _waterDriftTaskU16(Task* task)
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
        // Signed-halfword encoding of -64 before the random upward Y offset.
        WATER_DRIFT_UPWARD_Y_BIAS      = 0xFFC0,
        WATER_DRIFT_GRAVITY_PER_UPDATE = 6,
        WATER_DRIFT_FRAME_COUNT        = 8
    };
    EffectWork* particleWork;
    GfxCoord*   particleCoord;
    s32         velocityKind;
    s32         updatesPerCell;
    s32         launchSpeed;

    particleWork  = task->spawnArg2.pointer;
    particleCoord = task->extra.coordBody->coord;
    if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        // Redraw suspended particles from cached state; cancellation skips drawing.
        if (gRoomEffectState->effectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            if (task->state < WATER_DRIFT_STATE_UPRIGHT) {
                _waterDrawSpinU16(particleCoord, (u16)particleWork->index, particleWork->scale, particleWork->angle);
            } else {
                _waterDrawTileU16(particleCoord, (u16)particleWork->index, particleWork->scale);
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
            // Decode independent spawn fields; the random angle remains fixed.
            particleWork->scale = task->spawnArg1.halves.low & WATER_DRIFT_SPAWN_SIZE_MASK;
            gRandomLcgState     = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            particleWork->angle = (gRandomLcgState >> 16) & ACTOR_TRANSFORM_ANGLE_MASK;
            if (task->spawnArg1.value & WATER_DRIFT_SPAWN_PERIOD_MASK) {
                updatesPerCell = (task->spawnArg1.value >> WATER_DRIFT_SPAWN_PERIOD_SHIFT) & WATER_DRIFT_PERIOD_MASK;
            } else {
                updatesPerCell = WATER_DRIFT_DEFAULT_PERIOD;
            }
            particleWork->period = updatesPerCell;
            particleWork->age    = 0;
            task->state          = task->spawnArg1.value & WATER_DRIFT_SPAWN_UPRIGHT_MASK ? WATER_DRIFT_STATE_UPRIGHT : WATER_DRIFT_STATE_ROTATED;
            if ((particleWork->move.vx | particleWork->move.vy | particleWork->move.vz) == 0) {
                if (task->spawnArg1.value & WATER_DRIFT_SPAWN_SPEED_MASK) {
                    launchSpeed = (task->spawnArg1.value >> WATER_DRIFT_SPAWN_SPEED_SHIFT) & WATER_DRIFT_SPEED_MASK;
                } else {
                    launchSpeed = WATER_DRIFT_DEFAULT_SPEED;
                }
                particleWork->step = launchSpeed;
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
                        // Reuse the copied spawn offset as a direction without transforming it.
                        particleWork->move.vx = particleWork->pos.vx;
                        particleWork->move.vy = particleWork->pos.vy;
                        particleWork->move.vz = particleWork->pos.vz;
                        break;
                }
                _waterDriftNormalizeVelocity(particleWork);
            } else {
                // A supplied velocity bypasses scaling; step only enables movement.
                particleWork->step = WATER_DRIFT_DEFAULT_SPEED;
            }
            return;
        case WATER_DRIFT_STATE_ROTATED:
            _waterDrawSpinU16(particleCoord, (u16)particleWork->index, particleWork->scale, particleWork->angle);
            break;
        case WATER_DRIFT_STATE_UPRIGHT:
            _waterDrawTileU16(particleCoord, (u16)particleWork->index, particleWork->scale);
            break;
        default:
            return;
    }
    // Draw first; the changed translation is composed on the next running update.
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
