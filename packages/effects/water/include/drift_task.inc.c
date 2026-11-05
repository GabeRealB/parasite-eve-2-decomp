#include <gameplay/message.h>

#include <engine/random.h>

/* Part of the water effects library; see water.h. */

/// Turns a particle's launch direction (`move`) into a velocity of `step` units per update.
/// The drawers a drift calls: the spin and tile ones, or with `WATER_DRIFT_U16` defined their u16 versions.
#ifdef WATER_DRIFT_U16
#define WATER_DRIFT_DRAW_SPIN(coord, index, scale, angle) _waterDrawSpinU16((coord), (u16)(index), (scale), (angle))
#define WATER_DRIFT_DRAW_TILE(coord, index, scale)        _waterDrawTileU16((coord), (u16)(index), (scale))
#else
#define WATER_DRIFT_DRAW_SPIN(coord, index, scale, angle) _waterDrawSpin((coord), (index), (scale), (angle))
#define WATER_DRIFT_DRAW_TILE(coord, index, scale)        _waterDrawTile((coord), (index), (scale))
#endif

static inline void _waterDriftInitializeVelocity(EffectWork* particleWork)
{
    SVECTOR* velocity = &particleWork->move;

    VectorNormalSS(velocity, velocity);
    gte_lddp(particleWork->step);
    gte_ldsv(velocity);
    gte_gpf12();
    gte_stsv(velocity);
}

/// Animates one water-spray particle through eight cells, falling under gravity, then retires it. `spawnArg1`: bits
/// 0..11 size, 12..15 updates per cell (0 means 1), 16..23 speed (0 means 64), 24..27 launch kind (0 still, 1 upward
/// burst, 2 any direction, 3 narrow upward jet, 5 along `pos`), 28..31 nonzero for the upright tile rather than the
/// spinning quad. A nonzero `move` at spawn is the velocity as given.
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
                WATER_DRIFT_DRAW_SPIN(particleCoord, particleWork->index, particleWork->scale, particleWork->angle);
            } else {
                WATER_DRIFT_DRAW_TILE(particleCoord, particleWork->index, particleWork->scale);
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
            RANDOM_LCG_ADVANCE();
            particleWork->angle = RANDOM_LCG_DRAW() & ACTOR_TRANSFORM_ANGLE_MASK;
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
                        RANDOM_LCG_ADVANCE();
                        particleWork->move.vx = 0x80 - (RANDOM_LCG_DRAW() & 0xFF);
                        RANDOM_LCG_ADVANCE();
                        particleWork->move.vy = WATER_DRIFT_UPWARD_Y_BIAS - (RANDOM_LCG_DRAW() & 0x7F);
                        RANDOM_LCG_ADVANCE();
                        particleWork->move.vz = 0x80 - (RANDOM_LCG_DRAW() & 0xFF);
                        break;
                    case WATER_DRIFT_VELOCITY_ALL_AXES:
                        RANDOM_LCG_ADVANCE();
                        particleWork->move.vx = 0x80 - (RANDOM_LCG_DRAW() & 0xFF);
                        RANDOM_LCG_ADVANCE();
                        particleWork->move.vy = 0x80 - (RANDOM_LCG_DRAW() & 0xFF);
                        RANDOM_LCG_ADVANCE();
                        particleWork->move.vz = 0x80 - (RANDOM_LCG_DRAW() & 0xFF);
                        break;
                    case WATER_DRIFT_VELOCITY_NARROW_JET:
                        RANDOM_LCG_ADVANCE();
                        particleWork->move.vx = 0x10 - (RANDOM_LCG_DRAW() & 0x1F);
                        RANDOM_LCG_ADVANCE();
                        particleWork->move.vy = -(RANDOM_LCG_DRAW() & 0xFF);
                        RANDOM_LCG_ADVANCE();
                        particleWork->move.vz = 0x10 - (RANDOM_LCG_DRAW() & 0x1F);
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
            WATER_DRIFT_DRAW_SPIN(particleCoord, particleWork->index, particleWork->scale, particleWork->angle);
            break;
        case WATER_DRIFT_STATE_UPRIGHT:
            WATER_DRIFT_DRAW_TILE(particleCoord, particleWork->index, particleWork->scale);
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

#undef WATER_DRIFT_DRAW_SPIN
#undef WATER_DRIFT_DRAW_TILE
