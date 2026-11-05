#include "energyball.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include <core/pe2.h>
#include <math/gte.h>

#include <gameplay/display.h>
#include <gameplay/actor.h>
#include <gameplay/actor_render.h>
#include <gameplay/attachment_state.h>
#include <gameplay/attachments.h>
#include <gameplay/effect_tasks.h>
#include <gameplay/effects.h>
#include <gameplay/light.h>
#include <gameplay/room_effects.h>
#include <gameplay/scene.h>
#include <gameplay/world_collision.h>
#include <gameplay/world_coords.h>

#include <engine/coord.h>
#include <engine/display.h>
#include <engine/display_types.h>
#include <engine/random.h>
#include <engine/gfx.h>
#include <engine/mem.h>
#include <engine/scratch.h>
#include <engine/session.h>
#include <engine/session_types.h>
#include <engine/sound.h>
#include <engine/task.h>
#include <engine/task_types.h>
#include <engine/tmd_types.h>

#include <effects/sprite_quad/sprite_quad.h>
#include <effects/ground_glow/ground_glow.h>

PE2_EDITOR_LABEL("Cast")
/// The most balls in the air at once, and how far apart a cast's balls are, in a 4096-unit turn, fanned around the
/// caster's facing.
PE2_OPTION_DEF(MAX_BALLS, int, 3)
PE2_OPTION_DEF(BALL_SPACING, int, 0x555)
/// How far from the caster a ball spawns, and what each level adds.
PE2_OPTION_DEF(SPAWN_RADIUS, int, 0x300)
PE2_OPTION_DEF(LEVEL_SPAWN_RADIUS, int, 256)

PE2_EDITOR_LABEL("Ball")
/// The ball's brightness; its glow is drawn at a quarter of it.
PE2_OPTION_DEF(BRIGHTNESS, int, 0xC0)
/// The ball's tint on its even frames, a color word.
PE2_OPTION_DEF(TINT, int, 0x60C040)
/// How much dimmer red and blue are than green in the ball's light, glow, charge band and impact rings, as a shift: the
/// ball is green.
PE2_OPTION_DEF(RED_BLUE_SHIFT, int, 1)
/// The charge band's brightness is the size still to charge, divided by this.
PE2_OPTION_DEF(CHARGE_BAND_DIM, int, 5)
/// A ball that hits bursts until it is this many times its full size.
PE2_OPTION_DEF(BURST_SCALE, int, 2)
/// The tint of the glow under the ball, a color word.
PE2_OPTION_DEF(GROUND_GLOW_TINT, int, 0x203020)

PE2_EDITOR_LABEL("Flight")
/// The ball's speed as it launches in a random direction.
PE2_OPTION_DEF(LAUNCH_SPEED, int, 0x20)
/// Every this many frames the ball aims at the player again, a power of two, keeping this much of its speed, in 4096ths,
/// plus what each level adds.
PE2_OPTION_DEF(AIM_INTERVAL, int, 8)
PE2_OPTION_DEF(SPEED_KEPT, int, 0xA00)
PE2_OPTION_DEF(LEVEL_SPEED_KEPT, int, 0x180)
/// Speed the ball turns toward its aim by, every other frame, on each axis.
PE2_OPTION_DEF(STEERING, int, 0x10)

PE2_EDITOR_LABEL("Light")
/// The light the ball casts: its inner and outer radii, and its intensity, plus a random value's bits under the mask.
PE2_OPTION_DEF(LIGHT_INNER_RADIUS, int, 0x100)
PE2_OPTION_DEF(LIGHT_OUTER_RADIUS, int, 0x1000)
PE2_OPTION_DEF(LIGHT_INTENSITY, int, 0x800)
PE2_OPTION_DEF(LIGHT_FLICKER_MASK, int, 0x700)

PE2_EDITOR_LABEL("Impact rings")
/// The three rings a hit spawns are rolled by 0 and these, in a 4096-unit turn.
PE2_OPTION_DEF(RING_2_ROLL, int, 0x2AA)
PE2_OPTION_DEF(RING_3_ROLL, int, 0x555)
/// A ring's first radius and brightness, its width, and what it grows and loses each frame; it goes when its brightness
/// is spent.
PE2_OPTION_DEF(RING_RADIUS, int, 256)
PE2_OPTION_DEF(RING_BRIGHTNESS, int, 128)
PE2_OPTION_DEF(RING_WIDTH, int, 384)
PE2_OPTION_DEF(RING_GROWTH, int, 128)
PE2_OPTION_DEF(RING_FADE_STEP, int, 8)
PE2_EDITOR_LABEL("")

STATIC_ASSERT(IS_COLOR(PE2_OPTION(TINT)) && IS_COLOR(PE2_OPTION(GROUND_GLOW_TINT)), colors);
STATIC_ASSERT(POWER_OF_TWO(PE2_OPTION(AIM_INTERVAL)), aim_interval_is_a_mask);
STATIC_ASSERT(PE2_OPTION(CHARGE_BAND_DIM) > 0 && PE2_OPTION(RING_FADE_STEP) > 0, divisors_and_steps);

/// The Energy Ball spells' ids over 10 (431 to 433): casting another spell retires the balls.
enum { ATTACHMENT_ID_ENERGY_BALL_FAMILY = 43 };

/// A ball's tuning at one Parasite Energy level, by the level digit of the attachment id less one, in units of its
/// size (`EffectWork::angle`), which draws it and whose half is its collision radius.
typedef struct {
    s16 fullSize; // Size the ball charges to before it launches, and bursts from
    s16 sizeStep; // Size gained or lost each frame of charging, bursting or fading; also how far it rises each frame of charging
} _LevelTuning;

/// A ball's collision, which its task allocates and frees: a sphere, linked while the ball flies with half its size as
/// its radius, and the one contact it holds; an enemy's body in it bursts the ball.
typedef struct {
    WorldCollisionBody    body;
    WorldCollisionContact contacts[1]; // Cleared each frame it holds no enemy
} _BallCollision;

enum {
    FULL_TURN               = 4096,
    GLOW_RIM_ANGLE_STEP     = FULL_TURN / 16,
    GLOW_PERSPECTIVE_SCALE  = 64,
    GLOW_TRIG_FRACTION_BITS = 12,
};

static void _drawGlowDisc(const GfxCoord* centreCoord, s16 radius, s16 brightness);
static void _drawChargeBand(const GfxCoord* coord, s16 halfHeight, s16 brightness);

enum {
    PE_LEVEL_COUNT       = 3,
    LOOP_SOUND_START     = 0,
    IMPACT_SOUND_START   = PE_LEVEL_COUNT,
    SOUND_SELECTOR_COUNT = 2 * PE_LEVEL_COUNT,
};

/// Each level's loop, playing while any ball is, then each level's hit.
static s32 _gSoundScripts[SOUND_SELECTOR_COUNT] = {
    0xE02B0002,
    0xE02E0002,
    0xE0310002,
    0xE02B0001,
    0xE02E0001,
    0xE0310001,
};

/// Levels 1 to 3.
static _LevelTuning _gLevelTuning[PE_LEVEL_COUNT] = {
    { 0x0400, 0x0040 },
    { 0x0480, 0x0048 },
    { 0x0500, 0x0050 },
};

/// Each charge band segment's random offset into the band's animation, chosen when a cast starts.
static s16 _gChargeBandFrameOffsets[EFFECT_BAND_SEGMENT_COUNT];

/// Chooses the charge band segments' offsets.
#define SEED_CHARGE_BAND_PHASES(segmentIndex, phaseRoll)                                                    \
    do {                                                                                                    \
        enum { PHASE_JITTER_MASK = 0xFF };                                                                  \
        for ((segmentIndex) = 0; (segmentIndex) < ARRAY_SIZE(_gChargeBandFrameOffsets); (segmentIndex)++) { \
            (phaseRoll)                              = RANDOM_LCG_NEXT(gRandomLcgState);                    \
            _gChargeBandFrameOffsets[(segmentIndex)] = ((u32)(phaseRoll) >> 16) & PHASE_JITTER_MASK;        \
            gRandomLcgState                          = (phaseRoll);                                         \
        }                                                                                                   \
    } while (0)

void energyballCastTask(Task* task)
{
    enum {
        CAST_INITIALIZE = 0,
        CAST_RELEASE    = 1,
    };
    EffectWork* effect;
    GfxCoord*   coord;
    s32         ballIndex;
    s32         levelIndex;
    s32         jitterRoll;

    effect      = task->spawnArg2.pointer;
    coord       = task->extra.coordBody->coord;
    effect->age = effect->age + 1;
    switch (task->state) {
        case CAST_INITIALIZE:
            effect->index = gAttachmentState.attachId % 10 - 1;
            levelIndex    = effect->index;
            effect->angle = (levelIndex * PE2_OPTION(LEVEL_SPAWN_RADIUS)) + PE2_OPTION(SPAWN_RADIUS);
            if (gEnergyBallInFlightCount < 0) {
                gEnergyBallInFlightCount = 0;
            }
            if (gEnergyBallInFlightCount == 0) {
                sndEvtRequestScriptStart(_gSoundScripts[effect->index + LOOP_SOUND_START], 0, 0);
            }
            SEED_CHARGE_BAND_PHASES(ballIndex, jitterRoll);
            for (ballIndex = 0; ballIndex < effect->index + 1; ballIndex++) {
                if (gEnergyBallInFlightCount + ballIndex >= PE2_OPTION(MAX_BALLS)) {
                    break;
                }
                effect->scale   = ballIndex * PE2_OPTION(BALL_SPACING) - effect->index * (PE2_OPTION(BALL_SPACING) / 2);
                effect->move.vx = (effect->angle * rsin(effect->scale)) >> GLOW_TRIG_FRACTION_BITS;
                effect->move.vz = (effect->angle * rcos(effect->scale)) >> GLOW_TRIG_FRACTION_BITS;
                effectSpawn((EFFECT_ENERGY_BALL | EFFECT_SPAWN_UNLIMITED), coord, ballIndex, &effect->move);
            }
            task->state = CAST_RELEASE;
            return;
        case CAST_RELEASE:
            effectKillTask(effect, task);
            return;
    }
}
#undef SEED_CHARGE_BAND_PHASES

/// Moves the ball by its velocity and composes it.
static inline void _advanceProjectileCoord(GfxCoord* coord, const EffectWork* effect)
{
    coord->coord.t[0]  += effect->move.vx;
    coord->coord.t[1]  += effect->move.vy;
    coord->coord.t[2]  += effect->move.vz;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(coord);
}

void energyballProjectileTask(Task* task)
{
    enum {
        STATE_INITIALIZE        = 0,
        STATE_CHARGING          = 1,
        STATE_FLYING            = 2,
        STATE_BURSTING          = 3,
        STATE_FADING            = 4,
        FIRST_LIGHT_SLOT        = 4,
        SPELL_DAMAGE_KEY_BASE   = WORLD_COLLISION_CONTACT_ATTACK | 0x8000,
        RANDOM_DIRECTION_MASK   = 0xFFF,
        RANDOM_DIRECTION_CENTRE = 0x800,
        LIGHT_HOLD_FRAMES       = 2,
    };
    EffectWork*                    effect;
    GfxCoord*                      coord;
    _BallCollision*                collision;
    WorldCoordTransientPointLight* lightSlot;
    GfxCoord*                      lightCoord;
    WorldCoordPointLight*          pointLight;
    GfxCoord                       groundCoord;
    VECTOR                         playerOffset;
    GfxCoord*                      playerCoord;
    EffectWork*                    spawned;
    SVECTOR*                       flightVelocity;
    u16                            lightIntensity;
    s32*                           soundScripts;
    s16                            peEffectControl;
    s32                            velocityComponent;

    lightSlot       = &gWorldCoordTransientPointLights[task->spawnArg1.value + FIRST_LIGHT_SLOT];
    lightCoord      = &lightSlot->light.head.transform.coord;
    pointLight      = &lightSlot->light;
    coord           = task->extra.coordBody->coord;
    peEffectControl = gRoomEffectState->peEffectControl;
    collision       = task->work;
    effect          = task->spawnArg2.pointer;
    if (peEffectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        if (peEffectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            if (gEnergyBallInFlightCount > 0) {
                gEnergyBallInFlightCount -= 1;
                if (gEnergyBallInFlightCount == 0) {
                    sndEvtRequestScriptStop(_gSoundScripts[effect->index + LOOP_SOUND_START], SOUND_SCRIPT_STOP_KEEP_RELEASE);
                }
            }
            if (task->state != STATE_INITIALIZE) {
                worldCollisionUnlinkBody(&collision->body);
            }
            effectKillTask(effect, task);
            return;
        }
        actorRenderComposeCoord(coord);
        _spriteQuadDrawFlicker(coord, effect->age, effect->angle, effect->period);
        _drawGlowDisc(coord, effect->angle, effect->scale >> 2);
        if ((task->state < STATE_BURSTING) && (gRoomEffectState->groundTraceEnabled != 0) &&
            (worldCollisionProjectGroundCoord(coord, &groundCoord) == 1)) {
            _groundGlowDraw(&groundCoord, effect->angle);
        }
        return;
    }

    effect->age = effect->age + 1;
    switch (task->state) {
        case STATE_INITIALIZE:
            collision = memCalloc(sizeof(_BallCollision), 0);
            if (collision == NULL) {
                effect->age = 0;
                return;
            }
            task->work      = collision;
            effect->index   = (gAttachmentState.attachId % 10) - 1;
            effect->move.vx = 0;
            RANDOM_LCG_ADVANCE();
            effect->move.vy           = -_gLevelTuning[effect->index].sizeStep;
            effect->move.vz           = 0;
            effect->angle             = 0;
            effect->period            = RANDOM_LCG_DRAW() & RANDOM_DIRECTION_MASK;
            gEnergyBallInFlightCount += 1;
            effect->scale             = PE2_OPTION(BRIGHTNESS);
            effect->step              = PE2_OPTION(LAUNCH_SPEED);
            task->state               = STATE_CHARGING;
            /* fallthrough */
        case STATE_CHARGING:
            // Charge upward before arming the pair-test sphere at full size.
            if (effect->angle < _gLevelTuning[effect->index].fullSize) {
                effect->angle = effect->angle + _gLevelTuning[effect->index].sizeStep;
                _advanceProjectileCoord(coord, effect);
            } else {
                actorRenderComposeCoord(coord);
                task->work                       = collision;
                collision->body.context.contacts = collision->contacts;
                collision->body.coord            = coord;
                collision->body.key              = ((u16)(gAttachmentState.attachId / 100) - 1) * 9 +
                                      ((u16)((u16)(gAttachmentState.attachId % 100) / 10) - 1) * 3 +
                                      (u16)(gAttachmentState.attachId % 10) + SPELL_DAMAGE_KEY_BASE;
                collision->body.radius = effect->angle >> 1;
                collision->body.flags  = WORLD_COLLISION_BODY_SPHERE;
                worldCollisionLinkBody(WORLD_COLLISION_LIST_PLAYER_ATTACKS, &collision->body);
                flightVelocity               = &effect->move;
                collision->contacts[0].flags = WORLD_COLLISION_CONTACT_LAST;
                collision->body.flags       |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                task->state                  = STATE_FLYING;
                RANDOM_LCG_ADVANCE();
                effect->move.vx = RANDOM_DIRECTION_CENTRE - (RANDOM_LCG_DRAW() & RANDOM_DIRECTION_MASK);
                RANDOM_LCG_ADVANCE();
                effect->move.vy = RANDOM_DIRECTION_CENTRE - (RANDOM_LCG_DRAW() & RANDOM_DIRECTION_MASK);
                RANDOM_LCG_ADVANCE();
                effect->move.vz = RANDOM_DIRECTION_CENTRE - (RANDOM_LCG_DRAW() & RANDOM_DIRECTION_MASK);
                VectorNormalSS(flightVelocity, flightVelocity);
                gte_lddp(effect->step);
                gte_ldsv(flightVelocity);
                gte_gpf12();
                gte_stsv(flightVelocity);
                effect->pos.vx = 0;
                effect->pos.vy = -_gLevelTuning[effect->index].sizeStep;
                effect->pos.vz = 0;
            }
            lightSlot->framesLeft = LIGHT_HOLD_FRAMES;
            pointLight->inner     = PE2_OPTION(LIGHT_INNER_RADIUS);
            pointLight->outer     = PE2_OPTION(LIGHT_OUTER_RADIUS);
            RANDOM_LCG_ADVANCE();
            lightIntensity           = (RANDOM_LCG_DRAW() & PE2_OPTION(LIGHT_FLICKER_MASK)) + PE2_OPTION(LIGHT_INTENSITY);
            pointLight->head.color.g = lightIntensity;
            pointLight->head.color.r = (u16)pointLight->head.color.g >> PE2_OPTION(RED_BLUE_SHIFT);
            pointLight->head.color.b = pointLight->head.color.g >> PE2_OPTION(RED_BLUE_SHIFT);
            lightCoord->coord.t[0]   = coord->coord.t[0];
            lightCoord->coord.t[1]   = coord->coord.t[1];
            lightCoord->coord.t[2]   = coord->coord.t[2];
            lightCoord->composeStamp = GRAPHICS_COORD_DIRTY;
            _spriteQuadDrawFlicker(coord, effect->age, effect->angle, effect->period);
            _drawGlowDisc(coord, effect->angle, effect->scale >> 2);
            if ((gRoomEffectState->groundTraceEnabled != 0) && (worldCollisionProjectGroundCoord(coord, &groundCoord) == 1)) {
                _groundGlowDraw(&groundCoord, effect->angle);
            }
            // Keep the charging cylinder at its launch height while the ball rises.
            coord->workm.t[1] += _gLevelTuning[effect->index].sizeStep * effect->age;
            _drawChargeBand(coord, effect->angle,
                            (_gLevelTuning[effect->index].fullSize - effect->angle) / PE2_OPTION(CHARGE_BAND_DIM));
            coord->workm.t[1] -= _gLevelTuning[effect->index].sizeStep * effect->age;
            if ((u16)(gAttachmentState.attachId / 10) != ATTACHMENT_ID_ENERGY_BALL_FAMILY) {
                if ((gAttachmentState.effectPhase == ATTACHMENT_EFFECT_HELD) || (gRoomEffectState->peEffectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN)) {
                    if (gEnergyBallInFlightCount > 0) {
                        gEnergyBallInFlightCount -= 1;
                        if (gEnergyBallInFlightCount == 0) {
                            sndEvtRequestScriptStop(_gSoundScripts[effect->index + LOOP_SOUND_START], SOUND_SCRIPT_STOP_KEEP_RELEASE);
                        }
                    }
                    worldCollisionUnlinkBody(&collision->body);
                    effectKillTask(effect, task);
                    return;
                }
            }
            return;
        case STATE_FLYING:
            // Re-aim in the player's composed frame, then steer in parent-coordinate units.
            if ((effect->age & (PE2_OPTION(AIM_INTERVAL) - 1)) == 0) {
                playerCoord     = &(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords[1];
                playerOffset.vx = playerCoord->workm.t[0] - coord->workm.t[0];
                playerOffset.vy = playerCoord->workm.t[1] - coord->workm.t[1];
                playerOffset.vz = playerCoord->workm.t[2] - coord->workm.t[2];
                ApplyTransposeMatrixLV(&coord->workm, &playerOffset, &playerOffset);
                effect->pos.vx = playerOffset.vx;
                effect->pos.vy = playerOffset.vy;
                effect->pos.vz = playerOffset.vz;
                gte_SetRotMatrix(&coord->coord);
                gte_ldv0(&effect->pos);
                gte_rtv0();
                gte_stsv(&effect->pos);
                gte_lddp(effect->index * PE2_OPTION(LEVEL_SPEED_KEPT) + PE2_OPTION(SPEED_KEPT));
                gte_ldsv(&effect->move);
                gte_gpf12();
                gte_stsv(&effect->move);
            }
            if (effect->age & 1) {
                velocityComponent = effect->move.vx;
                effect->move.vx   = (velocityComponent < effect->pos.vx) ? velocityComponent + PE2_OPTION(STEERING) : velocityComponent - PE2_OPTION(STEERING);
                velocityComponent = effect->move.vy;
                effect->move.vy   = (velocityComponent < effect->pos.vy) ? velocityComponent + PE2_OPTION(STEERING) : velocityComponent - PE2_OPTION(STEERING);
                velocityComponent = effect->move.vz;
                effect->move.vz   = (velocityComponent < effect->pos.vz) ? velocityComponent + PE2_OPTION(STEERING) : velocityComponent - PE2_OPTION(STEERING);
            }
            _advanceProjectileCoord(coord, effect);
            lightSlot->framesLeft = LIGHT_HOLD_FRAMES;
            pointLight->inner     = PE2_OPTION(LIGHT_INNER_RADIUS);
            pointLight->outer     = PE2_OPTION(LIGHT_OUTER_RADIUS);
            RANDOM_LCG_ADVANCE();
            lightIntensity           = (RANDOM_LCG_DRAW() & PE2_OPTION(LIGHT_FLICKER_MASK)) + PE2_OPTION(LIGHT_INTENSITY);
            pointLight->head.color.g = lightIntensity;
            pointLight->head.color.r = (u16)pointLight->head.color.g >> PE2_OPTION(RED_BLUE_SHIFT);
            pointLight->head.color.b = pointLight->head.color.g >> PE2_OPTION(RED_BLUE_SHIFT);
            lightCoord->coord.t[0]   = coord->coord.t[0];
            lightCoord->coord.t[1]   = coord->coord.t[1];
            lightCoord->coord.t[2]   = coord->coord.t[2];
            lightCoord->composeStamp = GRAPHICS_COORD_DIRTY;
            _spriteQuadDrawFlicker(coord, effect->age, effect->angle, effect->period);
            _drawGlowDisc(coord, effect->angle, effect->scale >> 2);
            if (gRoomEffectState->groundTraceEnabled != 0) {
                if (worldCollisionProjectGroundCoord(coord, &groundCoord) == 1) {
                    _groundGlowDraw(&groundCoord, effect->angle);
                }
            }
            if ((u16)(gAttachmentState.attachId / 10) != ATTACHMENT_ID_ENERGY_BALL_FAMILY) {
                if ((gAttachmentState.effectPhase == ATTACHMENT_EFFECT_HELD) || (gRoomEffectState->peEffectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN)) {
                    if (gEnergyBallInFlightCount > 0) {
                        gEnergyBallInFlightCount -= 1;
                        if (gEnergyBallInFlightCount == 0) {
                            sndEvtRequestScriptStop(_gSoundScripts[effect->index + LOOP_SOUND_START], SOUND_SCRIPT_STOP_KEEP_RELEASE);
                        }
                    }
                    worldCollisionUnlinkBody(&collision->body);
                    effectKillTask(effect, task);
                    return;
                }
            }
            if (worldCollisionCountContactsByKind(collision->body.context.contacts, WORLD_COLLISION_CONTACT_ENEMY_BODY) != 0) {
                spawned = effectSpawn(EFFECT_ENERGYBALL_IMPACT_RING, coord, 0, NULL);
                if (spawned != NULL) {
                    taskReparent(task, spawned->task);
                }
                spawned = effectSpawn(EFFECT_ENERGYBALL_IMPACT_RING, coord, PE2_OPTION(RING_2_ROLL), NULL);
                if (spawned != NULL) {
                    taskReparent(task, spawned->task);
                }
                spawned = effectSpawn(EFFECT_ENERGYBALL_IMPACT_RING, coord, PE2_OPTION(RING_3_ROLL), NULL);
                if (spawned != NULL) {
                    taskReparent(task, spawned->task);
                }
                soundScripts = _gSoundScripts;
                sndEvtRequestScriptStart(soundScripts[effect->index + IMPACT_SOUND_START], 0, 0);
                worldCollisionUnlinkBody(&collision->body);
                effect->angle = _gLevelTuning[effect->index].fullSize;
                task->state   = STATE_BURSTING;
                return;
            }
            if (gRoomEffectState->battleState != ROOM_EFFECT_BATTLE_ENGAGED) {
                worldCollisionUnlinkBody(&collision->body);
                task->state = STATE_FADING;
                return;
            }
            worldCollisionClearContacts(collision->contacts);
            return;
        case STATE_BURSTING:
            // Collision is already unlinked; only size and the shared loop lifetime remain.
            actorRenderComposeCoord(coord);
            _spriteQuadDrawFlicker(coord, effect->age, effect->angle, effect->period);
            _drawGlowDisc(coord, effect->angle, effect->scale >> 2);
            _drawGlowDisc(coord, (u16)effect->angle * 2, effect->scale >> 2);
            effect->angle += _gLevelTuning[effect->index].sizeStep;
            if (((u16)(gAttachmentState.attachId / 10) != ATTACHMENT_ID_ENERGY_BALL_FAMILY) &&
                ((gAttachmentState.effectPhase == ATTACHMENT_EFFECT_HELD) || (gRoomEffectState->peEffectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN))) {
                if (gEnergyBallInFlightCount > 0) {
                    gEnergyBallInFlightCount -= 1;
                    if (gEnergyBallInFlightCount == 0) {
                        sndEvtRequestScriptStop(_gSoundScripts[effect->index + LOOP_SOUND_START], SOUND_SCRIPT_STOP_KEEP_RELEASE);
                    }
                }
                effectKillTask(effect, task);
                return;
            }
            if (_gLevelTuning[effect->index].fullSize * PE2_OPTION(BURST_SCALE) < effect->angle) {
                if (gEnergyBallInFlightCount > 0) {
                    gEnergyBallInFlightCount -= 1;
                    if (gEnergyBallInFlightCount == 0) {
                        sndEvtRequestScriptStop(_gSoundScripts[effect->index + LOOP_SOUND_START], SOUND_SCRIPT_STOP_KEEP_RELEASE);
                    }
                }
                effectKillTask(effect, task);
                return;
            }
            return;
        case STATE_FADING:
            actorRenderComposeCoord(coord);
            _spriteQuadDrawFlicker(coord, effect->age, effect->angle, effect->period);
            _drawGlowDisc(coord, effect->angle, effect->scale >> 2);
            _drawGlowDisc(coord, (u16)effect->angle * 2, effect->scale >> 2);
            effect->angle -= _gLevelTuning[effect->index].sizeStep;
            if (((u16)(gAttachmentState.attachId / 10) != ATTACHMENT_ID_ENERGY_BALL_FAMILY) &&
                ((gAttachmentState.effectPhase == ATTACHMENT_EFFECT_HELD) || (gRoomEffectState->peEffectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN))) {
                if (gEnergyBallInFlightCount > 0) {
                    gEnergyBallInFlightCount -= 1;
                    if (gEnergyBallInFlightCount == 0) {
                        sndEvtRequestScriptStop(_gSoundScripts[effect->index + LOOP_SOUND_START], SOUND_SCRIPT_STOP_KEEP_RELEASE);
                    }
                }
                effectKillTask(effect, task);
                return;
            }
            if (effect->angle < _gLevelTuning[effect->index].sizeStep) {
                if (gEnergyBallInFlightCount > 0) {
                    gEnergyBallInFlightCount -= 1;
                    if (gEnergyBallInFlightCount == 0) {
                        sndEvtRequestScriptStop(_gSoundScripts[effect->index + LOOP_SOUND_START], SOUND_SCRIPT_STOP_KEEP_RELEASE);
                    }
                }
                effectKillTask(effect, task);
                return;
            }
            return;
        default:
            return;
    }
}

/// Sets a glow quad's two fan triangles from the projected center, starting at `rimAngle`.
static inline void _setGlowFanVertices(POLY_G4* quad, const EffectCentreScratch* scratch, s32 rimAngle)
{
    quad->x0 = scratch->screenX + ((scratch->screenExtent * rsin(rimAngle)) >> GLOW_TRIG_FRACTION_BITS);
    quad->y0 = scratch->screenY + ((scratch->screenExtent * rcos(rimAngle)) >> GLOW_TRIG_FRACTION_BITS);
    quad->x1 = scratch->screenX + ((scratch->screenExtent * rsin(rimAngle + GLOW_RIM_ANGLE_STEP)) >> GLOW_TRIG_FRACTION_BITS);
    quad->y1 = scratch->screenY + ((scratch->screenExtent * rcos(rimAngle + GLOW_RIM_ANGLE_STEP)) >> GLOW_TRIG_FRACTION_BITS);
    quad->x2 = scratch->screenX;
    quad->y2 = scratch->screenY;
    quad->x3 = scratch->screenX + ((scratch->screenExtent * rsin(rimAngle + 2 * GLOW_RIM_ANGLE_STEP)) >> GLOW_TRIG_FRACTION_BITS);
    quad->y3 = scratch->screenY + ((scratch->screenExtent * rcos(rimAngle + 2 * GLOW_RIM_ANGLE_STEP)) >> GLOW_TRIG_FRACTION_BITS);
}

/// Draws the ball's additive glow, a disc fading from a lit center to a black rim, sized by `radius` in perspective.
static void _drawGlowDisc(const GfxCoord* centreCoord, s16 radius, s16 brightness)
{
    EffectCentreScratch* scratch;
    POLY_G4*             quad;
    s32                  rimAngle;

    // Project just the centre; the fan is built in screen space.
    scratch                = SCRATCH_STACK_RESERVE_BLOCK(EffectCentreScratch);
    scratch->worldPoint.vx = centreCoord->workm.t[0];
    scratch->worldPoint.vy = centreCoord->workm.t[1];
    scratch->worldPoint.vz = centreCoord->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&scratch->worldPoint);
    gte_rtps();
    gte_stsxy(&scratch->screenX);
    gte_stflg(&scratch->projectionFlags);
    if (scratch->projectionFlags >= 0) {
        gte_stszotz(&scratch->depth);
        scratch->depth++;
        scratch->screenExtent = (radius * GLOW_PERSPECTIVE_SCALE) / scratch->depth;
        // Each quad spans two triangles with only their shared centre lit.
        for (rimAngle = 0; rimAngle < FULL_TURN; rimAngle += 2 * GLOW_RIM_ANGLE_STEP) {
            quad           = gGpuPrimCursor;
            gGpuPrimCursor = quad + 1;
            setPolyG4(quad);
            setRGB0(quad, 0, 0, 0);
            setRGB1(quad, 0, 0, 0);
            setRGB2(quad, brightness >> PE2_OPTION(RED_BLUE_SHIFT), brightness, brightness >> PE2_OPTION(RED_BLUE_SHIFT));
            setRGB3(quad, 0, 0, 0);
            _setGlowFanVertices(quad, scratch, rimAngle);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(scratch->depth),
                    quad);
            gpuSetPrimitiveBlendMode(quad, GPU_BLEND_ADD, scratch->depth);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectCentreScratch);
}

/// The ball's copy of the flicker drawer, sized by its 56-texel cell's span.
#define SPRITE_QUAD_SCALE (56 - 1)
/// Odd frames draw the raw cell, additive.
#define SPRITE_QUAD_CONFIGURE_FLICKER_ODD_PACKET(quad) \
    {                                                  \
        setSemiTrans((quad), 1);                       \
        setShadeTex((quad), 1);                        \
        SPRITE_QUAD_SET_FLICKER_ODD_CELL((quad));      \
    }
/// Even frames draw the tinted cell, semitransparent.
#define SPRITE_QUAD_CONFIGURE_FLICKER_EVEN_PACKET(quad)                                                            \
    {                                                                                                              \
        setRGB0((quad), COLOR_RED(PE2_OPTION(TINT)), COLOR_GREEN(PE2_OPTION(TINT)), COLOR_BLUE(PE2_OPTION(TINT))); \
        SPRITE_QUAD_SET_FLICKER_EVEN_CELL((quad));                                                                 \
        setSemiTrans((quad), 1);                                                                                   \
    }
#include <effects/sprite_quad/draw_flicker.inc.c>

/// The glow under the ball: its tint and its palette.
#define GROUND_GLOW_R    COLOR_RED(PE2_OPTION(GROUND_GLOW_TINT))
#define GROUND_GLOW_G    COLOR_GREEN(PE2_OPTION(GROUND_GLOW_TINT))
#define GROUND_GLOW_B    COLOR_BLUE(PE2_OPTION(GROUND_GLOW_TINT))
#define GROUND_GLOW_CLUT getClut(192, 266)
#include <effects/ground_glow/draw.inc.c>

/// Projects a charge band segment's four corners and picks its animation cell.
#define PROJECT_BAND_SEGMENT(scratch, segmentIndex, textureFrame)                                                                          \
    {                                                                                                                                      \
        enum { BAND_FRAME_COUNT = 6 };                                                                                                     \
        s32 nextSegmentIndex;                                                                                                              \
                                                                                                                                           \
        gte_ldv0(&(scratch)->topRing[(segmentIndex)]);                                                                                     \
        gte_rtps();                                                                                                                        \
        (textureFrame) = (_gChargeBandFrameOffsets[(segmentIndex)] + gDisplayState.animFrame) % BAND_FRAME_COUNT;                          \
        gte_stsxy(&(scratch)->sxy0);                                                                                                       \
        nextSegmentIndex = ((segmentIndex) + 1) & (EFFECT_BAND_SEGMENT_COUNT - 1);                                                         \
        gte_ldv3(&(scratch)->topRing[nextSegmentIndex], &(scratch)->bottomRing[(segmentIndex)], &(scratch)->bottomRing[nextSegmentIndex]); \
        gte_rtpt();                                                                                                                        \
        gte_stsxy3(&(scratch)->sxy1, &(scratch)->sxy2, &(scratch)->sxy3);                                                                  \
        gte_stflg(&(scratch)->projectionFlags);                                                                                            \
    }

/// Draws the charge band, an additive textured cylinder from the ball down `halfHeight * 2`, its segments animating
/// at their own offsets; a segment failing to project is dropped.
static void _drawChargeBand(const GfxCoord* coord, s16 halfHeight, s16 brightness)
{
    enum {
        BAND_ANGLE_STEP         = 256,
        BAND_TRIG_FRACTION_BITS = 12,
        BAND_CELL_WIDTH         = 40,
        BAND_TOP_V              = 0x60,
        BAND_UV_SPAN            = 39,
    };
    EffectBandScratch* scratch;
    SVECTOR*           bottomVertex;
    POLY_FT4*          quad;
    s32                segmentIndex;
    s32                rimAngle;
    s32                textureU;
    s16                textureFrame;

    scratch = SCRATCH_STACK_RESERVE_BLOCK(EffectBandScratch);
    gte_SetTransMatrix(&GsWSMATRIX);
    // Build both rims in local XZ, then transform their narrowed vertices.
    for (segmentIndex = 0; segmentIndex < EFFECT_BAND_SEGMENT_COUNT; segmentIndex++) {
        rimAngle                          = segmentIndex * BAND_ANGLE_STEP;
        scratch->topRing[segmentIndex].vx = (u32)(rsin(rimAngle) * 3) >> 5;
        scratch->topRing[segmentIndex].vy = -(halfHeight * 2);
        scratch->topRing[segmentIndex].vz = (u32)(rcos(rimAngle) * 3) >> 5;
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&scratch->topRing[segmentIndex]);
        gte_rtv0();
        gte_stsv(&scratch->topRing[segmentIndex]);
        scratch->topRing[segmentIndex].vx   += coord->workm.t[0];
        scratch->topRing[segmentIndex].vy   += coord->workm.t[1];
        scratch->topRing[segmentIndex].vz   += coord->workm.t[2];
        scratch->bottomRing[segmentIndex].vx = (u32)(rsin(rimAngle) * 3) >> 5;
        // Address the lower rim through the complete scratch block's byte view.
        bottomVertex     = (SVECTOR*)((u8*)scratch + segmentIndex * sizeof(SVECTOR) + sizeof(scratch->topRing));
        bottomVertex->vy = 0;
        bottomVertex->vz = (u32)(rcos(rimAngle) * 3) >> 5;
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&scratch->bottomRing[segmentIndex]);
        gte_rtv0();
        gte_stsv(&scratch->bottomRing[segmentIndex]);
        scratch->bottomRing[segmentIndex].vx += coord->workm.t[0];
        bottomVertex->vy                     += coord->workm.t[1];
        bottomVertex->vz                     += coord->workm.t[2];
    }
    // Project sixteen wrapped segments; only accepted quads consume packets.
    gte_SetRotMatrix(&GsWSMATRIX);
    for (segmentIndex = 0; segmentIndex < EFFECT_BAND_SEGMENT_COUNT; segmentIndex++) {
        PROJECT_BAND_SEGMENT(scratch, segmentIndex, textureFrame);
        if (scratch->projectionFlags >= 0) {
            gte_stszotz(&scratch->otz);
            scratch->otz++;
            quad           = gGpuPrimCursor;
            gGpuPrimCursor = quad + 1;
            setPolyFT4(quad);
            quad->tpage = getTPage(0, GPU_BLEND_ADD, 640, 0);
            quad->clut  = getClut(16, 267);
            textureU    = textureFrame * BAND_CELL_WIDTH;
            setRGB0(quad, brightness >> PE2_OPTION(RED_BLUE_SHIFT), brightness, brightness >> PE2_OPTION(RED_BLUE_SHIFT));
            setUV4(quad, textureU, BAND_TOP_V, textureU + BAND_UV_SPAN, BAND_TOP_V, textureU, BAND_TOP_V + BAND_UV_SPAN, textureU + BAND_UV_SPAN, BAND_TOP_V + BAND_UV_SPAN);
            setSemiTrans(quad, true);
            quad->x0 = scratch->sxy0.vx;
            quad->y0 = scratch->sxy0.vy;
            quad->x1 = scratch->sxy1.vx;
            quad->y1 = scratch->sxy1.vy;
            quad->x2 = scratch->sxy2.vx;
            quad->y2 = scratch->sxy2.vy;
            quad->x3 = scratch->sxy3.vx;
            quad->y3 = scratch->sxy3.vy;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(scratch->otz),
                    quad);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectBandScratch);
}
#undef PROJECT_BAND_SEGMENT

void energyballImpactRingTask(Task* task)
{
    enum {
        IMPACT_NEW    = 0,
        IMPACT_ACTIVE = 1,
    };
    EffectWork* work;
    GfxCoord*   coord;
    s16         peEffectControl;
    u8          rgb[3];
    s32         nextBrightness;
    s32         nextInnerRadius;

    work            = task->spawnArg2.pointer;
    peEffectControl = gRoomEffectState->peEffectControl;
    coord           = task->extra.coordBody->coord;
    if (peEffectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        return;
    }

    if (task->state == IMPACT_NEW) {
        gfxRotMatrixZ(&coord->coord, task->spawnArg1.value & (FULL_TURN - 1), GRAPHICS_ROTATION_COMPOSE);
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        work->scale         = PE2_OPTION(RING_BRIGHTNESS);
        work->angle         = PE2_OPTION(RING_RADIUS);
        task->state         = IMPACT_ACTIVE;
    }

    actorRenderComposeCoord(coord);
    rgb[0] = work->scale >> PE2_OPTION(RED_BLUE_SHIFT);
    rgb[1] = work->scale;
    rgb[2] = work->scale >> PE2_OPTION(RED_BLUE_SHIFT);
    effectDrawInnerGlowBand(coord, work->angle, PE2_OPTION(RING_WIDTH), rgb);

    // Draw the current band before expanding and fading it for the next update.
    nextInnerRadius  = (u16)work->angle;
    nextBrightness   = (u16)work->scale;
    nextInnerRadius += PE2_OPTION(RING_GROWTH);
    nextBrightness  -= PE2_OPTION(RING_FADE_STEP);
    work->scale      = nextBrightness;
    work->angle      = nextInnerRadius;
    if ((s16)nextBrightness <= PE2_OPTION(RING_FADE_STEP)) {
        effectKillTask(work, task);
    }
}
