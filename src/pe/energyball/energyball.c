#include "pe/energyball.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "gameplay/display.h"
#include "gameplay/actor.h"
#include "gameplay/actor_render.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/light.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/random.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "overlay.h"
#include "../../shared/sprite_quad.h"
#include "../../shared/ground_glow.h"

/// Size and growth rate of one energy ball at one Parasite Energy level.
///
/// A ball selects its row by the level digit of the spell being cast
/// (`AttachmentState::attachId % 10 - 1`), so a higher level charges a larger
/// ball at a faster rate.
///
/// Both members are in the units of the ball's size, which the ball keeps in
/// `EffectWork::angle`: its glow sprite, ring and ground glow are drawn from
/// that size and its collision sphere takes half of it as its radius.
typedef struct {
    s16 fullSize; // Size the ball charges to from zero before it is launched; a ball that hits bursts from this size to twice it
    s16 sizeStep; // Size gained per frame while charging and bursting, and lost per frame by a ball fading out, which ends below one step; also the distance the ball rises per frame while it charges, and the upward speed it first steers toward once launched
} _EnergyballLevelTuning;
STATIC_ASSERT_SIZEOF(_EnergyballLevelTuning, 4);

/// Collision block of one energy ball: the sphere it strikes with and the one
/// contact that sphere can hold.
///
/// The ball's task allocates the block zeroed when it spawns and keeps it in
/// `Task::work`, so the task's teardown frees it. The sphere is armed only once
/// the ball is fully charged: it then follows the ball's coordinate with half
/// the ball's size as its radius and is linked on collision list 1 for pair
/// tests. Its packed key has contact category 2 and an identity counted from
/// 0x8000 by the digits of the spell being cast. An occupied contact of
/// category 3 bursts the ball. The sphere is unlinked before the ball bursts,
/// fades or is cancelled, which is harmless for a block that was never linked.
typedef struct {
    WorldCollisionBody    body;        // Sphere linked on list 1 while the ball flies; pair tests are enabled after the link
    WorldCollisionContact contacts[1]; // One-entry table `body` borrows. The entry is marked LAST; a contact that does not burst the ball is cleared the frame it is found
} _EnergyballBody;
STATIC_ASSERT_SIZEOF(_EnergyballBody, 0x38);

/// Angle units and perspective sizing for the energy ball's sixteen-triangle glow fan.
enum {
    ENERGYBALL_FULL_TURN               = 4096,
    ENERGYBALL_GLOW_RIM_ANGLE_STEP     = ENERGYBALL_FULL_TURN / 16,
    ENERGYBALL_GLOW_PERSPECTIVE_SCALE  = 64,
    ENERGYBALL_GLOW_TRIG_FRACTION_BITS = 12,
};

static void _energyballDrawGlowDisc(const GfxCoord* centreCoord, s16 radius, s16 brightness);
static void _energyballDrawChargeBand(const GfxCoord* coord, s16 halfHeight, s16 brightness);

/// The energy ball's sound-script ids. Only the first three are read, indexed by
/// the cast's level: the cast starts its entry with `sndEvtRequestScriptStart` and
/// later passes the same id to `sndEvtRequestScriptStop`.
static s32 D_energyball_8013117C[] = {
    0xE02B0002,
    0xE02E0002,
    0xE0310002,
    0xE02B0001,
    0xE02E0001,
    0xE0310001,
};

/// Per-level size tuning for the ball, one row per PE level 1-3, weakest
/// first.
static _EnergyballLevelTuning D_energyball_80131194[] = {
    { 0x0400, 0x0040 },
    { 0x0480, 0x0048 },
    { 0x0500, 0x0050 },
};

/// Sixteen 8-bit draws from `gRandomLcgState`, refilled once per cast by
/// `energyballCastTask` and consumed by the GTE pass in
/// `_energyballDrawChargeBand` as the per-vertex jitter of the ball's surface.
static s16 D_energyball_801311A0[16];

/// Seeds all sixteen charge-band phases in LCG order.
///
/// segmentIndex and phaseRoll are writable scalar locals, evaluated repeatedly.
/// Captures the shared LCG and the complete charge-band phase table; no validation.
#define ENERGYBALL_SEED_CHARGE_BAND_PHASES(segmentIndex, phaseRoll)                                                 \
    {                                                                                                               \
        enum { ENERGYBALL_PHASE_JITTER_MASK = 0xFF };                                                               \
        for ((segmentIndex) = 0; (segmentIndex) < ARRAY_SIZE(D_energyball_801311A0); (segmentIndex)++) {            \
            (phaseRoll)                           = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT; \
            D_energyball_801311A0[(segmentIndex)] = ((u32)(phaseRoll) >> 16) & ENERGYBALL_PHASE_JITTER_MASK;        \
            gRandomLcgState                       = (phaseRoll);                                                    \
        }                                                                                                           \
    }

void energyballCastTask(Task* task)
{
    enum {
        ENERGYBALL_CAST_INITIALIZE     = 0,
        ENERGYBALL_CAST_RELEASE        = 1,
        ENERGYBALL_MAX_ACTIVE_BALLS    = 3,
        ENERGYBALL_SPAWN_RADIUS_BASE   = 0x300,
        ENERGYBALL_SPAWN_RADIUS_STEP   = 256,
        ENERGYBALL_SPAWN_BEARING_STEP  = 0x555,
        ENERGYBALL_SPAWN_CENTRING_STEP = 0x2AA,
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
        case ENERGYBALL_CAST_INITIALIZE:
            effect->index = Gp_StateC08.attachId % 10 - 1;
            levelIndex    = effect->index;
            effect->angle = (levelIndex * ENERGYBALL_SPAWN_RADIUS_STEP) + ENERGYBALL_SPAWN_RADIUS_BASE;
            if (gEnergyBallInFlightCount < 0) {
                gEnergyBallInFlightCount = 0;
            }
            if (gEnergyBallInFlightCount == 0) {
                sndEvtRequestScriptStart(D_energyball_8013117C[effect->index], 0, 0);
            }
            ENERGYBALL_SEED_CHARGE_BAND_PHASES(ballIndex, jitterRoll);
            for (ballIndex = 0; ballIndex < effect->index + 1; ballIndex++) {
                if (gEnergyBallInFlightCount + ballIndex >= ENERGYBALL_MAX_ACTIVE_BALLS) {
                    break;
                }
                effect->scale   = ballIndex * ENERGYBALL_SPAWN_BEARING_STEP - effect->index * ENERGYBALL_SPAWN_CENTRING_STEP;
                effect->move.vx = (effect->angle * rsin(effect->scale)) >> ENERGYBALL_GLOW_TRIG_FRACTION_BITS;
                effect->move.vz = (effect->angle * rcos(effect->scale)) >> ENERGYBALL_GLOW_TRIG_FRACTION_BITS;
                effectSpawn((EFFECT_ENERGY_BALL | EFFECT_SPAWN_UNLIMITED), coord, ballIndex, &effect->move);
            }
            task->state = ENERGYBALL_CAST_RELEASE;
            return;
        case ENERGYBALL_CAST_RELEASE:
            effectKillTask(effect, task);
            return;
    }
}
#undef ENERGYBALL_SEED_CHARGE_BAND_PHASES

/// Applies one velocity step in the parent frame and refreshes the composed coordinate.
///
/// Borrows live writable coord and read-only effect; additions must fit s32.
/// Parent coordinates must stay live through composition. Clobbers GTE state.
static inline void _energyballAdvanceProjectileCoord(GfxCoord* coord, const EffectWork* effect)
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
        ENERGYBALL_LIGHT_INTENSITY_BASE    = 0x800,
        ENERGYBALL_IMPACT_MIDDLE_BEARING   = 0x2AA,
        ENERGYBALL_IMPACT_LAST_BEARING     = 0x555,
        ENERGYBALL_STATE_INITIALIZE        = 0,
        ENERGYBALL_STATE_CHARGING          = 1,
        ENERGYBALL_STATE_FLYING            = 2,
        ENERGYBALL_STATE_BURSTING          = 3,
        ENERGYBALL_STATE_FADING            = 4,
        ENERGYBALL_FIRST_LIGHT_SLOT        = 4,
        ENERGYBALL_SPELL_DAMAGE_KEY_BASE   = WORLD_COLLISION_CONTACT_ATTACK | 0x8000,
        ENERGYBALL_RANDOM_DIRECTION_MASK   = 0xFFF,
        ENERGYBALL_RANDOM_DIRECTION_CENTRE = 0x800,
        ENERGYBALL_BRIGHTNESS              = 0xC0,
        ENERGYBALL_INITIAL_SPEED           = 0x20,
        ENERGYBALL_AIM_PERIOD              = 8,
        ENERGYBALL_STEERING_STEP           = 0x10,
        ENERGYBALL_LIGHT_HOLD_FRAMES       = 2,
        ENERGYBALL_LIGHT_INNER_RADIUS      = 0x100,
        ENERGYBALL_LIGHT_OUTER_RADIUS      = 0x1000,
        ENERGYBALL_LIGHT_JITTER_MASK       = 0x700,
        ENERGYBALL_VELOCITY_SCALE_STEP     = 0x180,
        ENERGYBALL_VELOCITY_SCALE_BASE     = 0xA00,
        ENERGYBALL_IMPACT_SOUND_OFFSET     = 3,
    };
    EffectWork*                    effect;
    GfxCoord*                      coord;
    _EnergyballBody*               collision;
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

    lightSlot       = &gWorldCoordTransientPointLights[task->spawnArg1.value + ENERGYBALL_FIRST_LIGHT_SLOT];
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
                    sndEvtRequestScriptStop(D_energyball_8013117C[effect->index], SOUND_SCRIPT_STOP_KEEP_RELEASE);
                }
            }
            if (task->state != ENERGYBALL_STATE_INITIALIZE) {
                worldCollisionUnlinkBody(&collision->body);
            }
            effectKillTask(effect, task);
            return;
        }
        actorRenderComposeCoord(coord);
        _spriteQuadDrawFlicker(coord, effect->age, effect->angle, effect->period);
        _energyballDrawGlowDisc(coord, effect->angle, effect->scale >> 2);
        if ((task->state < ENERGYBALL_STATE_BURSTING) && (gRoomEffectState->groundTraceEnabled != 0) &&
            (worldCollisionProjectGroundCoord(coord, &groundCoord) == 1)) {
            _groundGlowDraw(&groundCoord, effect->angle);
        }
        return;
    }

    effect->age = effect->age + 1;
    switch (task->state) {
        case ENERGYBALL_STATE_INITIALIZE:
            collision = memCalloc(sizeof(_EnergyballBody), 0);
            if (collision == NULL) {
                effect->age = 0;
                return;
            }
            task->work                = collision;
            effect->index             = (Gp_StateC08.attachId % 10) - 1;
            effect->move.vx           = 0;
            gRandomLcgState           = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            effect->move.vy           = -D_energyball_80131194[effect->index].sizeStep;
            effect->move.vz           = 0;
            effect->angle             = 0;
            effect->period            = (gRandomLcgState >> 16) & ENERGYBALL_RANDOM_DIRECTION_MASK;
            gEnergyBallInFlightCount += 1;
            effect->scale             = ENERGYBALL_BRIGHTNESS;
            effect->step              = ENERGYBALL_INITIAL_SPEED;
            task->state               = ENERGYBALL_STATE_CHARGING;
            /* fallthrough */
        case ENERGYBALL_STATE_CHARGING:
            // Charge upward before arming the pair-test sphere at full size.
            if (effect->angle < D_energyball_80131194[effect->index].fullSize) {
                effect->angle = effect->angle + D_energyball_80131194[effect->index].sizeStep;
                _energyballAdvanceProjectileCoord(coord, effect);
            } else {
                actorRenderComposeCoord(coord);
                task->work                       = collision;
                collision->body.context.contacts = collision->contacts;
                collision->body.coord            = coord;
                collision->body.key              = ((u16)(Gp_StateC08.attachId / 100) - 1) * 9 +
                                      ((u16)((u16)(Gp_StateC08.attachId % 100) / 10) - 1) * 3 +
                                      (u16)(Gp_StateC08.attachId % 10) + ENERGYBALL_SPELL_DAMAGE_KEY_BASE;
                collision->body.radius = effect->angle >> 1;
                collision->body.flags  = WORLD_COLLISION_BODY_SPHERE;
                worldCollisionLinkBody(WORLD_COLLISION_LIST_PLAYER_ATTACKS, &collision->body);
                flightVelocity               = &effect->move;
                collision->contacts[0].flags = WORLD_COLLISION_CONTACT_LAST;
                collision->body.flags       |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                task->state                  = ENERGYBALL_STATE_FLYING;
                gRandomLcgState              = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                effect->move.vx              = ENERGYBALL_RANDOM_DIRECTION_CENTRE - ((gRandomLcgState >> 16) & ENERGYBALL_RANDOM_DIRECTION_MASK);
                gRandomLcgState              = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                effect->move.vy              = ENERGYBALL_RANDOM_DIRECTION_CENTRE - ((gRandomLcgState >> 16) & ENERGYBALL_RANDOM_DIRECTION_MASK);
                gRandomLcgState              = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                effect->move.vz              = ENERGYBALL_RANDOM_DIRECTION_CENTRE - ((gRandomLcgState >> 16) & ENERGYBALL_RANDOM_DIRECTION_MASK);
                VectorNormalSS(flightVelocity, flightVelocity);
                gte_lddp(effect->step);
                gte_ldsv(flightVelocity);
                gte_gpf12();
                gte_stsv(flightVelocity);
                effect->pos.vx = 0;
                effect->pos.vy = -D_energyball_80131194[effect->index].sizeStep;
                effect->pos.vz = 0;
            }
            lightSlot->framesLeft    = ENERGYBALL_LIGHT_HOLD_FRAMES;
            pointLight->inner        = ENERGYBALL_LIGHT_INNER_RADIUS;
            pointLight->outer        = ENERGYBALL_LIGHT_OUTER_RADIUS;
            gRandomLcgState          = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            lightIntensity           = ((gRandomLcgState >> 16) & ENERGYBALL_LIGHT_JITTER_MASK) + ENERGYBALL_LIGHT_INTENSITY_BASE;
            pointLight->head.color.g = lightIntensity;
            pointLight->head.color.r = (u16)pointLight->head.color.g >> 1;
            pointLight->head.color.b = pointLight->head.color.g >> 1;
            lightCoord->coord.t[0]   = coord->coord.t[0];
            lightCoord->coord.t[1]   = coord->coord.t[1];
            lightCoord->coord.t[2]   = coord->coord.t[2];
            lightCoord->composeStamp = GRAPHICS_COORD_DIRTY;
            _spriteQuadDrawFlicker(coord, effect->age, effect->angle, effect->period);
            _energyballDrawGlowDisc(coord, effect->angle, effect->scale >> 2);
            if ((gRoomEffectState->groundTraceEnabled != 0) && (worldCollisionProjectGroundCoord(coord, &groundCoord) == 1)) {
                _groundGlowDraw(&groundCoord, effect->angle);
            }
            // Keep the charging cylinder at its launch height while the ball rises.
            coord->workm.t[1] += D_energyball_80131194[effect->index].sizeStep * effect->age;
            _energyballDrawChargeBand(coord, effect->angle,
                                      (D_energyball_80131194[effect->index].fullSize - effect->angle) / 5);
            coord->workm.t[1] -= D_energyball_80131194[effect->index].sizeStep * effect->age;
            if ((u16)(Gp_StateC08.attachId / 10) != ATTACHMENT_ID_ENERGY_BALL_FAMILY) {
                if ((Gp_StateC08.effectPhase == ATTACHMENT_EFFECT_HELD) || (gRoomEffectState->peEffectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN)) {
                    if (gEnergyBallInFlightCount > 0) {
                        gEnergyBallInFlightCount -= 1;
                        if (gEnergyBallInFlightCount == 0) {
                            sndEvtRequestScriptStop(D_energyball_8013117C[effect->index], SOUND_SCRIPT_STOP_KEEP_RELEASE);
                        }
                    }
                    worldCollisionUnlinkBody(&collision->body);
                    effectKillTask(effect, task);
                    return;
                }
            }
            return;
        case ENERGYBALL_STATE_FLYING:
            // Re-aim in the player's composed frame, then steer in parent-coordinate units.
            if ((effect->age & (ENERGYBALL_AIM_PERIOD - 1)) == 0) {
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
                gte_lddp(effect->index * ENERGYBALL_VELOCITY_SCALE_STEP + ENERGYBALL_VELOCITY_SCALE_BASE);
                gte_ldsv(&effect->move);
                gte_gpf12();
                gte_stsv(&effect->move);
            }
            if (effect->age & 1) {
                velocityComponent = effect->move.vx;
                effect->move.vx   = (velocityComponent < effect->pos.vx) ? velocityComponent + ENERGYBALL_STEERING_STEP : velocityComponent - ENERGYBALL_STEERING_STEP;
                velocityComponent = effect->move.vy;
                effect->move.vy   = (velocityComponent < effect->pos.vy) ? velocityComponent + ENERGYBALL_STEERING_STEP : velocityComponent - ENERGYBALL_STEERING_STEP;
                velocityComponent = effect->move.vz;
                effect->move.vz   = (velocityComponent < effect->pos.vz) ? velocityComponent + ENERGYBALL_STEERING_STEP : velocityComponent - ENERGYBALL_STEERING_STEP;
            }
            _energyballAdvanceProjectileCoord(coord, effect);
            lightSlot->framesLeft    = ENERGYBALL_LIGHT_HOLD_FRAMES;
            pointLight->inner        = ENERGYBALL_LIGHT_INNER_RADIUS;
            pointLight->outer        = ENERGYBALL_LIGHT_OUTER_RADIUS;
            gRandomLcgState          = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            lightIntensity           = ((gRandomLcgState >> 16) & ENERGYBALL_LIGHT_JITTER_MASK) + ENERGYBALL_LIGHT_INTENSITY_BASE;
            pointLight->head.color.g = lightIntensity;
            pointLight->head.color.r = (u16)pointLight->head.color.g >> 1;
            pointLight->head.color.b = pointLight->head.color.g >> 1;
            lightCoord->coord.t[0]   = coord->coord.t[0];
            lightCoord->coord.t[1]   = coord->coord.t[1];
            lightCoord->coord.t[2]   = coord->coord.t[2];
            lightCoord->composeStamp = GRAPHICS_COORD_DIRTY;
            _spriteQuadDrawFlicker(coord, effect->age, effect->angle, effect->period);
            _energyballDrawGlowDisc(coord, effect->angle, effect->scale >> 2);
            if (gRoomEffectState->groundTraceEnabled != 0) {
                if (worldCollisionProjectGroundCoord(coord, &groundCoord) == 1) {
                    _groundGlowDraw(&groundCoord, effect->angle);
                }
            }
            if ((u16)(Gp_StateC08.attachId / 10) != ATTACHMENT_ID_ENERGY_BALL_FAMILY) {
                if ((Gp_StateC08.effectPhase == ATTACHMENT_EFFECT_HELD) || (gRoomEffectState->peEffectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN)) {
                    if (gEnergyBallInFlightCount > 0) {
                        gEnergyBallInFlightCount -= 1;
                        if (gEnergyBallInFlightCount == 0) {
                            sndEvtRequestScriptStop(D_energyball_8013117C[effect->index], SOUND_SCRIPT_STOP_KEEP_RELEASE);
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
                spawned = effectSpawn(EFFECT_ENERGYBALL_IMPACT_RING, coord, ENERGYBALL_IMPACT_MIDDLE_BEARING, NULL);
                if (spawned != NULL) {
                    taskReparent(task, spawned->task);
                }
                spawned = effectSpawn(EFFECT_ENERGYBALL_IMPACT_RING, coord, ENERGYBALL_IMPACT_LAST_BEARING, NULL);
                if (spawned != NULL) {
                    taskReparent(task, spawned->task);
                }
                soundScripts = D_energyball_8013117C;
                sndEvtRequestScriptStart(soundScripts[effect->index + ENERGYBALL_IMPACT_SOUND_OFFSET], 0, 0);
                worldCollisionUnlinkBody(&collision->body);
                effect->angle = D_energyball_80131194[effect->index].fullSize;
                task->state   = ENERGYBALL_STATE_BURSTING;
                return;
            }
            if (gRoomEffectState->battleState != ROOM_EFFECT_BATTLE_ENGAGED) {
                worldCollisionUnlinkBody(&collision->body);
                task->state = ENERGYBALL_STATE_FADING;
                return;
            }
            worldCollisionClearContacts(collision->contacts);
            return;
        case ENERGYBALL_STATE_BURSTING:
            // Collision is already unlinked; only size and the shared loop lifetime remain.
            actorRenderComposeCoord(coord);
            _spriteQuadDrawFlicker(coord, effect->age, effect->angle, effect->period);
            _energyballDrawGlowDisc(coord, effect->angle, effect->scale >> 2);
            _energyballDrawGlowDisc(coord, (u16)effect->angle * 2, effect->scale >> 2);
            effect->angle += D_energyball_80131194[effect->index].sizeStep;
            if (((u16)(Gp_StateC08.attachId / 10) != ATTACHMENT_ID_ENERGY_BALL_FAMILY) &&
                ((Gp_StateC08.effectPhase == ATTACHMENT_EFFECT_HELD) || (gRoomEffectState->peEffectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN))) {
                if (gEnergyBallInFlightCount > 0) {
                    gEnergyBallInFlightCount -= 1;
                    if (gEnergyBallInFlightCount == 0) {
                        sndEvtRequestScriptStop(D_energyball_8013117C[effect->index], SOUND_SCRIPT_STOP_KEEP_RELEASE);
                    }
                }
                effectKillTask(effect, task);
                return;
            }
            if (D_energyball_80131194[effect->index].fullSize * 2 < effect->angle) {
                if (gEnergyBallInFlightCount > 0) {
                    gEnergyBallInFlightCount -= 1;
                    if (gEnergyBallInFlightCount == 0) {
                        sndEvtRequestScriptStop(D_energyball_8013117C[effect->index], SOUND_SCRIPT_STOP_KEEP_RELEASE);
                    }
                }
                effectKillTask(effect, task);
                return;
            }
            return;
        case ENERGYBALL_STATE_FADING:
            actorRenderComposeCoord(coord);
            _spriteQuadDrawFlicker(coord, effect->age, effect->angle, effect->period);
            _energyballDrawGlowDisc(coord, effect->angle, effect->scale >> 2);
            _energyballDrawGlowDisc(coord, (u16)effect->angle * 2, effect->scale >> 2);
            effect->angle -= D_energyball_80131194[effect->index].sizeStep;
            if (((u16)(Gp_StateC08.attachId / 10) != ATTACHMENT_ID_ENERGY_BALL_FAMILY) &&
                ((Gp_StateC08.effectPhase == ATTACHMENT_EFFECT_HELD) || (gRoomEffectState->peEffectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN))) {
                if (gEnergyBallInFlightCount > 0) {
                    gEnergyBallInFlightCount -= 1;
                    if (gEnergyBallInFlightCount == 0) {
                        sndEvtRequestScriptStop(D_energyball_8013117C[effect->index], SOUND_SCRIPT_STOP_KEEP_RELEASE);
                    }
                }
                effectKillTask(effect, task);
                return;
            }
            if (effect->angle < D_energyball_80131194[effect->index].sizeStep) {
                if (gEnergyBallInFlightCount > 0) {
                    gEnergyBallInFlightCount -= 1;
                    if (gEnergyBallInFlightCount == 0) {
                        sndEvtRequestScriptStop(D_energyball_8013117C[effect->index], SOUND_SCRIPT_STOP_KEEP_RELEASE);
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

/// Places the two fan triangles of one glow quad around an already projected centre.
///
/// `scratch` supplies pixel coordinates and a signed pixel radius. `rimAngle`
/// uses 4096 units per turn; the three rim vertices are one sixteenth-turn
/// apart, with the centre at vertex 2. Writes only the quad's XY fields, which
/// narrow to signed 16 bits. Q12 products and coordinate sums must fit s32.
/// Borrows both objects and retains no pointers.
static inline void _energyballSetGlowFanVertices(POLY_G4* quad, const EffectCentreScratch* scratch, s32 rimAngle)
{
    quad->x0 = scratch->screenX + ((scratch->screenExtent * rsin(rimAngle)) >> ENERGYBALL_GLOW_TRIG_FRACTION_BITS);
    quad->y0 = scratch->screenY + ((scratch->screenExtent * rcos(rimAngle)) >> ENERGYBALL_GLOW_TRIG_FRACTION_BITS);
    quad->x1 = scratch->screenX + ((scratch->screenExtent * rsin(rimAngle + ENERGYBALL_GLOW_RIM_ANGLE_STEP)) >> ENERGYBALL_GLOW_TRIG_FRACTION_BITS);
    quad->y1 = scratch->screenY + ((scratch->screenExtent * rcos(rimAngle + ENERGYBALL_GLOW_RIM_ANGLE_STEP)) >> ENERGYBALL_GLOW_TRIG_FRACTION_BITS);
    quad->x2 = scratch->screenX;
    quad->y2 = scratch->screenY;
    quad->x3 = scratch->screenX + ((scratch->screenExtent * rsin(rimAngle + 2 * ENERGYBALL_GLOW_RIM_ANGLE_STEP)) >> ENERGYBALL_GLOW_TRIG_FRACTION_BITS);
    quad->y3 = scratch->screenY + ((scratch->screenExtent * rcos(rimAngle + 2 * ENERGYBALL_GLOW_RIM_ANGLE_STEP)) >> ENERGYBALL_GLOW_TRIG_FRACTION_BITS);
}

/// Draws the energy ball's additive green disc, fading from its centre to a black rim.
///
/// `centreCoord` supplies a composed translation in `GsWSMATRIX`'s input
/// space; rotation is unused and the position narrows to signed 16 bits.
/// Signed `radius` scales to radius * 64 / (SZ3 / 4 + 1) pixels. Centre RGB
/// channels take the low bytes of (brightness >> 1, brightness, brightness >> 1).
/// A negative GTE FLAG rejects the whole disc. Otherwise eight Gouraud quads
/// cover sixteen fan triangles and sort by the biased depth.
/// Reserves/releases one scratch block and appends eight `POLY_G4`/`DR_TPAGE`
/// pairs to the unchecked frame arena. Inputs must stay clear of that storage;
/// packets live through GPU drawing. Leaves additive blend mode active.
static void _energyballDrawGlowDisc(const GfxCoord* centreCoord, s16 radius, s16 brightness)
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
        scratch->screenExtent = (radius * ENERGYBALL_GLOW_PERSPECTIVE_SCALE) / scratch->depth;
        // Each quad spans two triangles with only their shared centre lit.
        for (rimAngle = 0; rimAngle < ENERGYBALL_FULL_TURN; rimAngle += 2 * ENERGYBALL_GLOW_RIM_ANGLE_STEP) {
            quad           = gGpuPrimCursor;
            gGpuPrimCursor = quad + 1;
            setPolyG4(quad);
            setRGB0(quad, 0, 0, 0);
            setRGB1(quad, 0, 0, 0);
            setRGB2(quad, brightness >> 1, brightness, brightness >> 1);
            setRGB3(quad, 0, 0, 0);
            _energyballSetGlowFanVertices(quad, scratch, rimAngle);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)scratch->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    quad);
            gpuSetPrimitiveBlendMode(quad, GPU_BLEND_ADD, scratch->depth);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectCentreScratch);
}

#define SPRITE_QUAD_SCALE 55
#define SPRITE_QUAD_ODD_LOOK(p) \
    setSemiTrans(p, 1);         \
    setShadeTex(p, 1);          \
    SPRITE_QUAD_CORE_CELL(p)
#define SPRITE_QUAD_EVEN_LOOK(p)  \
    setRGB0(p, 0x40, 0xC0, 0x60); \
    SPRITE_QUAD_RIM_CELL(p);      \
    setSemiTrans(p, 1)
#include "../../shared/sprite_quad_draw_flicker.inc.c"

#define GROUND_GLOW_R    0x20
#define GROUND_GLOW_G    0x30
#define GROUND_GLOW_B    0x20
#define GROUND_GLOW_CLUT 0x428C
#include "../../shared/ground_glow_draw.inc.c"

/// Projects one band quad and selects its six-frame texture cell.
///
/// Borrows complete live scratch storage with both rings initialized and
/// segmentIndex in 0..15. The caller has installed the projection matrices.
/// Saves corner zero before RTPT advances the screen FIFO; only the final
/// RTPT FLAG is retained, and the last corner's SZ3 remains for the caller.
/// Texture phase lookup stays between RTPS and saving the first corner.
/// All input arguments are side-effect-free locals, used repeatedly; textureFrame
/// is a writable s16 lvalue. Captures display animation frame and the package phase table.
/// The scoped next-segment temporary is private to the expansion.
#define ENERGYBALL_PROJECT_BAND_SEGMENT(scratch, segmentIndex, textureFrame)                                                               \
    {                                                                                                                                      \
        enum { ENERGYBALL_BAND_FRAME_COUNT = 6 };                                                                                          \
        s32 nextSegmentIndex;                                                                                                              \
                                                                                                                                           \
        gte_ldv0(&(scratch)->topRing[(segmentIndex)]);                                                                                     \
        gte_rtps();                                                                                                                        \
        (textureFrame) = (u32)(D_energyball_801311A0[(segmentIndex)] + gDisplayState.animFrame) % ENERGYBALL_BAND_FRAME_COUNT;             \
        gte_stsxy(&(scratch)->sxy0);                                                                                                       \
        nextSegmentIndex = ((segmentIndex) + 1) & (EFFECT_BAND_SEGMENT_COUNT - 1);                                                         \
        gte_ldv3(&(scratch)->topRing[nextSegmentIndex], &(scratch)->bottomRing[(segmentIndex)], &(scratch)->bottomRing[nextSegmentIndex]); \
        gte_rtpt();                                                                                                                        \
        gte_stsxy3(&(scratch)->sxy1, &(scratch)->sxy2, &(scratch)->sxy3);                                                                  \
        gte_stflg(&(scratch)->projectionFlags);                                                                                            \
    }

/// Draws the charging ball's textured cylinder between two local-XZ rims.
///
/// Borrows a composed `coord`; the rims have fixed radius 384 coordinate
/// units, at local Y = -2 * `halfHeight` and zero. The caller supplies size
/// 0..1280 and brightness 0..256; RGB takes the low bytes of
/// (brightness / 2, brightness, brightness / 2). Angles use 4096 per turn.
/// Rotated/transformed vertices narrow to signed halfwords. Six 40-texel
/// frames use per-segment jitter plus the display frame. The final RTPT FLAG
/// rejects a segment; sorting uses its last corner's SZ3 / 4 + 1.
/// Reserves/releases one complete scratch block and appends at most sixteen
/// additive modulated `POLY_FT4` packets; caller inputs must stay clear of
/// that storage and the unchecked primitive arena. Packets live through drawing.
static void _energyballDrawChargeBand(const GfxCoord* coord, s16 halfHeight, s16 brightness)
{
    enum {
        ENERGYBALL_BAND_ANGLE_STEP         = 256,
        ENERGYBALL_BAND_TRIG_FRACTION_BITS = 12,
        ENERGYBALL_BAND_CELL_WIDTH         = 40,
        ENERGYBALL_BAND_TOP_V              = 0x60,
        ENERGYBALL_BAND_UV_SPAN            = 39,
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
        rimAngle                          = segmentIndex * ENERGYBALL_BAND_ANGLE_STEP;
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
        ENERGYBALL_PROJECT_BAND_SEGMENT(scratch, segmentIndex, textureFrame);
        if (scratch->projectionFlags >= 0) {
            gte_stszotz(&scratch->otz);
            scratch->otz++;
            quad           = gGpuPrimCursor;
            gGpuPrimCursor = quad + 1;
            setPolyFT4(quad);
            quad->tpage = getTPage(0, GPU_BLEND_ADD, 640, 0);
            quad->clut  = getClut(16, 267);
            textureU    = textureFrame * ENERGYBALL_BAND_CELL_WIDTH;
            setRGB0(quad, brightness >> 1, brightness, brightness >> 1);
            setUV4(quad, textureU, ENERGYBALL_BAND_TOP_V, textureU + ENERGYBALL_BAND_UV_SPAN, ENERGYBALL_BAND_TOP_V, textureU, ENERGYBALL_BAND_TOP_V + ENERGYBALL_BAND_UV_SPAN, textureU + ENERGYBALL_BAND_UV_SPAN, ENERGYBALL_BAND_TOP_V + ENERGYBALL_BAND_UV_SPAN);
            setSemiTrans(quad, true);
            quad->x0 = scratch->sxy0.vx;
            quad->y0 = scratch->sxy0.vy;
            quad->x1 = scratch->sxy1.vx;
            quad->y1 = scratch->sxy1.vy;
            quad->x2 = scratch->sxy2.vx;
            quad->y2 = scratch->sxy2.vy;
            quad->x3 = scratch->sxy3.vx;
            quad->y3 = scratch->sxy3.vy;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)scratch->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    quad);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectBandScratch);
}
#undef ENERGYBALL_PROJECT_BAND_SEGMENT

void energyballImpactRingTask(Task* task)
{
    enum {
        ENERGYBALL_IMPACT_NEW                = 0,
        ENERGYBALL_IMPACT_ACTIVE             = 1,
        ENERGYBALL_IMPACT_INITIAL_BRIGHTNESS = 128,
        ENERGYBALL_IMPACT_INITIAL_RADIUS     = 256,
        ENERGYBALL_IMPACT_BAND_WIDTH         = 384,
        ENERGYBALL_IMPACT_RADIUS_STEP        = 128,
        ENERGYBALL_IMPACT_BRIGHTNESS_STEP    = 8,
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

    if (task->state == ENERGYBALL_IMPACT_NEW) {
        gfxRotMatrixZ(&coord->coord, task->spawnArg1.value & (ENERGYBALL_FULL_TURN - 1), GRAPHICS_ROTATION_COMPOSE);
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        work->scale         = ENERGYBALL_IMPACT_INITIAL_BRIGHTNESS;
        work->angle         = ENERGYBALL_IMPACT_INITIAL_RADIUS;
        task->state         = ENERGYBALL_IMPACT_ACTIVE;
    }

    actorRenderComposeCoord(coord);
    rgb[0] = work->scale >> 1;
    rgb[1] = work->scale;
    rgb[2] = work->scale >> 1;
    effectDrawInnerGlowBand(coord, work->angle, ENERGYBALL_IMPACT_BAND_WIDTH, rgb);

    // Draw the current band before expanding and fading it for the next update.
    nextInnerRadius  = (u16)work->angle;
    nextBrightness   = (u16)work->scale;
    nextInnerRadius += ENERGYBALL_IMPACT_RADIUS_STEP;
    nextBrightness  -= ENERGYBALL_IMPACT_BRIGHTNESS_STEP;
    work->scale      = nextBrightness;
    work->angle      = nextInnerRadius;
    if ((s16)nextBrightness <= ENERGYBALL_IMPACT_BRIGHTNESS_STEP) {
        effectKillTask(work, task);
    }
}
