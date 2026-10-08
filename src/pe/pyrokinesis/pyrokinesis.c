#include "pe/pyrokinesis.h"

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
#include "gameplay/pad_script.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/random.h"
#include "main/gfx.h"
#include "main/gfx_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"
#include "../../shared/glow_draw.h"
#include "../../shared/pyro_flame.h"
/// Signed effect-age argument whose low bit selects one of the two flame cells.
#define SPRITE_QUAD_FRAME_T s16
#include "../../shared/sprite_quad.h"
#include "../../shared/jet_cone.h"

/// Collision block of the travelling pyrokinesis flame, allocated zeroed on the
/// cast's first running frame and kept at `Task::work`, whose teardown frees it.
///
/// Both bodies are spheres centred on the origin of the coordinate the flame
/// travels on, and both borrow the one contact entry. The damage sphere is what
/// the flame burns with: it takes pair tests only, and its key is the damage id
/// of the spell being cast - contact category 2 with bit 0x8000 plus that spell
/// and level's row number. A pair contact of category 3 on it bursts the flame
/// into its closing rings and unlinks it. The grid sphere is what stops the
/// flame: collision list 7 is walked only by the room-grid pass, and a contact
/// with a class-0 room surface (`WORLD_COLLISION_CONTACT_GRID` alone) unlinks
/// that sphere and leaves the flame shrinking where it stands, the damage
/// sphere shrinking with it. Running out of range unlinks both. Unlinking a
/// body that is no longer linked does nothing, which the cast relies on when it
/// ends from a state that has already dropped one of them.
typedef struct {
    WorldCollisionBody    damageBody;  // Sphere linked on list 1; pair tests are enabled after the link. Its radius follows the flame's: 0x500 on the launch frame, the level's size while it travels, 0x40 less each frame once it is stopped
    WorldCollisionBody    gridBody;    // Sphere linked on list 7 with a zero key and one eighth of the launch radius; grid tests are enabled after the link, together with `WORLD_COLLISION_BODY_CLIP_TO_GRID_CONTACT`
    WorldCollisionContact contacts[1]; // One-entry table both bodies borrow. The entry is marked LAST; an occupied contact that neither bursts nor stops the flame is cleared the frame it is found
} _PyrokinesisWork;
STATIC_ASSERT_SIZEOF(_PyrokinesisWork, 0x58);

static void _pyrokinesisDrawGroundGlow(const GfxCoord* groundCoord, s32 halfSize);

/// The `sndEvtRequestScriptStart` id of the ignition roar, three per PE level,
/// indexed by `EffectWork.index * 3 + Task::spawnArg1` (level by cast variant).
static s32 D_pyrokinesis_80131DD8[] = {
    0xE00B0002,
    0xE00B0002,
    0xE00B0002,
    0xE00E0002,
    0xE00E0002,
    0xE00E0002,
    0xE0110002,
    0xE0110003,
    0xE0110004,
};

/// Per-flame jitter of the cone, one 8-bit LCG roll each, re-rolled as a block
/// when the cast starts.
static s16 D_pyrokinesis_80131DFC[16] = { 0 };

/// Copies the player's nine Q12 coefficients without changing translation.
///
/// Borrows word-aligned matrices, with `destination` writable. Transfers all
/// 18 coefficient bytes through `GfxRotationWords`, including any scale in
/// the player's basis; preserves the alignment halfword and translation.
/// A whole word-view assignment would also overwrite the alignment halfword.
/// The caller invalidates and composes the coordinate afterward. Retains no
/// pointers and changes no GTE state.
static inline void _pyrokinesisCopyCastRotation(MATRIX* destination, const MATRIX* source)
{
    GfxRotationWords*       destinationRotation = (GfxRotationWords*)destination;
    const GfxRotationWords* sourceRotation      = (const GfxRotationWords*)source;

    destinationRotation->m00M01 = sourceRotation->m00M01;
    destinationRotation->m02M10 = sourceRotation->m02M10;
    destinationRotation->m11M12 = sourceRotation->m11M12;
    destinationRotation->m20M21 = sourceRotation->m20M21;
    destinationRotation->m22    = sourceRotation->m22;
}

/// Applies one velocity step in the parent frame and refreshes the composed coordinate.
///
/// `effect->move` supplies signed 16-bit XYZ distances per tick in the parent
/// coordinate's units; no rotation or fixed-point scaling occurs here. Borrows
/// writable `coord` and read-only `effect`; each translation sum must fit s32.
/// Marks the composition dirty before refreshing it. The parent chain stays
/// live through composition, which clobbers GTE state. Retains no pointers.
static inline void _pyrokinesisAdvanceCastCoord(GfxCoord* coord, const EffectWork* effect)
{
    coord->coord.t[0]  += effect->move.vx;
    coord->coord.t[1]  += effect->move.vy;
    coord->coord.t[2]  += effect->move.vz;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(coord);
}

void pyrokinesisCastTask(Task* task)
{
    // Packed shift expressions retain the target's unsigned halfword loads.
    enum {
        PYROKINESIS_SPEED_PER_LEVEL         = 64,
        PYROKINESIS_SPEED_BASE              = 0x1C0,
        PYROKINESIS_PHASE_JITTER_MASK       = 0xFF,
        PYROKINESIS_LIGHT_RADIUS_BASE       = 0x200,
        PYROKINESIS_LIGHT_HOLD_FRAMES       = 4,
        PYROKINESIS_LIGHT_INTENSITY_BASE    = 0x800,
        PYROKINESIS_LIGHT_JITTER_MASK       = 0x700,
        PYROKINESIS_TRAVEL_EXTENT_PER_FRAME = 6,
        PYROKINESIS_MOTOR_START_INTENSITY   = 0xFF,
        PYROKINESIS_MOTOR_END_INTENSITY     = 8,
        PYROKINESIS_STATE_INITIALIZE        = 0,
        PYROKINESIS_STATE_TRAVELLING        = 1,
        PYROKINESIS_STATE_SHRINKING         = 2,
        PYROKINESIS_STATE_BURSTING          = 3,
        PYROKINESIS_STATE_BURST_THEN_FADE   = 4,
        PYROKINESIS_SPELL_DAMAGE_KEY_BASE   = WORLD_COLLISION_CONTACT_ATTACK | 0x8000,
        PYROKINESIS_INITIAL_BRIGHTNESS      = 0xC0,
        PYROKINESIS_LAUNCH_RADIUS           = 0x500,
        PYROKINESIS_ANGLE_MASK              = 0xFFF,
        PYROKINESIS_RADIUS_STEP             = 0x40,
        PYROKINESIS_TRAVEL_RADIUS_BASE      = 0x380,
        PYROKINESIS_BURST_RADIUS_BASE       = 0x580,
        PYROKINESIS_IMPACT_BEARING_LIMIT    = 0x556,
        PYROKINESIS_IMPACT_BEARING_STEP     = 0x2AA,
        PYROKINESIS_TRAIL_AGE_LIMIT         = 0x1E,
        PYROKINESIS_TRAVEL_AGE_LIMIT        = 0x1F,
        PYROKINESIS_MIN_PUFF_RADIUS         = 0x81,
        PYROKINESIS_SHRINK_END_RADIUS       = 0x80,
        PYROKINESIS_FADE_END_BRIGHTNESS     = 9,
        PYROKINESIS_BRIGHTNESS_STEP         = 8,
    };
    EffectWork*                    effect;
    GfxCoord*                      coord;
    _PyrokinesisWork*              collision;
    ModelObjectCoordBody*          coordBody;
    GfxCoord*                      playerCoord;
    WorldCoordTransientPointLight* lightSlot;
    GfxCoord*                      lightCoord;
    WorldCoordPointLight*          pointLight;
    EffectWork*                    spawned;
    GfxCoord                       groundCoord;
    u8                             rgb[3];
    s32                            rimIndex;
    s32                            pan;
    s16                            peEffectControl;
    s32                            age;
    s32                            radius;
    s32                            nextState;
    s16                            lightIntensity;

    collision   = task->work;
    effect      = task->spawnArg2.pointer;
    coordBody   = task->extra.coordBody;
    coord       = coordBody->coord;
    effect->age = effect->age + 1;
    lightSlot   = gWorldCoordTransientPointLights;
    lightCoord  = &lightSlot->light.head.transform.coord;
    pointLight  = &lightSlot->light;
    switch (task->state) {
        case PYROKINESIS_STATE_INITIALIZE:
            if ((Gp_StateC08.effectPhase == ATTACHMENT_EFFECT_HELD) || ((peEffectControl = gRoomEffectState->peEffectControl), peEffectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN)) {
                effectKillTask(effect, task);
                return;
            }
            if (peEffectControl != ROOM_EFFECT_CONTROL_RUNNING) {
                effect->age = effect->age - 1;
                return;
            }
            collision = memCalloc(sizeof(_PyrokinesisWork), 0);
            if (collision == NULL) {
                effect->age = 0;
                return;
            }
            playerCoord = (gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords;
            // Borrow only the player's rotation; launch in that forward frame.
            _pyrokinesisCopyCastRotation(&coord->coord, &playerCoord->coord);
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(coord);
            effect->move.vx = 0;
            effect->move.vy = 0;
            effect->move.vz = (Gp_StateC08.attachId % 10) * PYROKINESIS_SPEED_PER_LEVEL + PYROKINESIS_SPEED_BASE;
            gte_SetRotMatrix(&playerCoord->coord);
            gte_ldv0(&effect->move);
            gte_rtv0();
            gte_stsv(&effect->move);
            for (rimIndex = 0; rimIndex < ARRAY_SIZE(D_pyrokinesis_80131DFC); rimIndex++) {
                gRandomLcgState                  = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                D_pyrokinesis_80131DFC[rimIndex] = (gRandomLcgState >> 16) & PYROKINESIS_PHASE_JITTER_MASK;
            }
            effect->scale   = PYROKINESIS_INITIAL_BRIGHTNESS;
            effect->angle   = PYROKINESIS_LAUNCH_RADIUS;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            effect->period  = (gRandomLcgState >> 16) & PYROKINESIS_ANGLE_MASK;
            effect->index   = (Gp_StateC08.attachId % 10) - 1;
            pan             = (s8)worldCoordGetOriginAudioPan(coord);
            sndEvtRequestScriptStart(D_pyrokinesis_80131DD8[effect->index * 3 + task->spawnArg1.value], pan,
                                     (s8)worldCoordGetOriginAudioDepth(coord));
            padScriptSpawnVariableMotorRamp((s16)(effect->index * 2 + 8), PYROKINESIS_MOTOR_START_INTENSITY, PYROKINESIS_MOTOR_END_INTENSITY);
            // Choose the sound with the requested variant before normalizing cone behavior.
            if (effect->index == 1) {
                task->spawnArg1.value = 1;
            } else if (task->spawnArg1.value == 1) {
                task->spawnArg1.value = 0;
            }
            task->work                             = collision;
            collision->damageBody.coord            = coord;
            collision->damageBody.context.contacts = collision->contacts;
            collision->damageBody.key              = ((u16)(Gp_StateC08.attachId / 100) - 1) * 9 +
                                        ((u16)((u16)(Gp_StateC08.attachId % 100) / 10) - 1) * 3 +
                                        (u16)(Gp_StateC08.attachId % 10) + PYROKINESIS_SPELL_DAMAGE_KEY_BASE;
            collision->damageBody.radius = effect->angle;
            collision->damageBody.flags  = WORLD_COLLISION_BODY_SPHERE;
            worldCollisionLinkBody(WORLD_COLLISION_LIST_PLAYER_ATTACKS, &collision->damageBody);
            collision->contacts[0].flags         = WORLD_COLLISION_CONTACT_LAST;
            collision->gridBody.coord            = coord;
            collision->gridBody.context.contacts = collision->contacts;
            collision->gridBody.key              = 0;
            collision->damageBody.flags         |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            collision->gridBody.radius           = (s16)((u16)effect->angle << 16 >> 19);
            collision->gridBody.flags            = WORLD_COLLISION_BODY_SPHERE;
            worldCollisionLinkBody(WORLD_COLLISION_LIST_GRID_ONLY, &collision->gridBody);
            collision->gridBody.flags = (collision->gridBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED)) | (WORLD_COLLISION_BODY_CLIP_TO_GRID_CONTACT | WORLD_COLLISION_BODY_GRID_ENABLED);
            effectSpawn(EFFECT_PYROKINESIS_LAUNCH_CONE, coord, 0, NULL);
            rgb[0] = 0xFF;
            rgb[1] = 0x7F;
            rgb[2] = 0x3F;
            effectDrawScreenTint(rgb, GPU_BLEND_ADD);
            task->state = PYROKINESIS_STATE_TRAVELLING;
            spriteQuadDraw(coord, effect->age, effect->angle, effect->period);
            glowDrawFlameDisc(coord, effect->angle, (s16)((u16)effect->scale << 16 >> 17));
            if (worldCollisionCountContactsByKind(collision->damageBody.context.contacts, WORLD_COLLISION_CONTACT_ENEMY_BODY) != 0) {
                worldCollisionUnlinkBody(&collision->damageBody);
                radius        = (effect->index << 9) + PYROKINESIS_TRAVEL_RADIUS_BASE;
                effect->angle = radius;
                for (rimIndex = 0; rimIndex < PYROKINESIS_IMPACT_BEARING_LIMIT; rimIndex += PYROKINESIS_IMPACT_BEARING_STEP) {
                    spawned = effectSpawn(EFFECT_PYROKINESIS_FLAME_RING, coord, rimIndex, NULL);
                    if (spawned != NULL) {
                        taskReparent(task, spawned->task);
                    }
                }
                nextState = PYROKINESIS_STATE_BURSTING;
                if (task->spawnArg1.value == 2) {
                    nextState = PYROKINESIS_STATE_BURST_THEN_FADE;
                }
                task->state = nextState;
                return;
            }
            if (worldCollisionFindContactIndex(collision->gridBody.context.contacts, WORLD_COLLISION_CONTACT_GRID) != 0) {
                worldCollisionUnlinkBody(&collision->gridBody);
                task->state = PYROKINESIS_STATE_SHRINKING;
                return;
            }
            worldCollisionClearContacts(collision->contacts);
            return;
        case PYROKINESIS_STATE_TRAVELLING:
            if ((Gp_StateC08.effectPhase == ATTACHMENT_EFFECT_HELD) || ((peEffectControl = gRoomEffectState->peEffectControl), peEffectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN)) {
                worldCollisionUnlinkBody(&collision->damageBody);
                worldCollisionUnlinkBody(&collision->gridBody);
                effectKillTask(effect, task);
                return;
            }
            if (peEffectControl != ROOM_EFFECT_CONTROL_RUNNING) {
                effect->age = effect->age - 1;
                return;
            }
            radius                       = (effect->index << 9) + PYROKINESIS_TRAVEL_RADIUS_BASE;
            effect->angle                = radius;
            collision->damageBody.radius = radius;
            _pyrokinesisAdvanceCastCoord(coord, effect);
            spriteQuadDraw(coord, effect->age, effect->angle, effect->period);
            glowDrawFlameDisc(coord, effect->angle, (s16)((u16)effect->scale << 16 >> 17));
            if (task->spawnArg1.value != 0) {
                _jetConeDraw(coord, effect->age, effect->angle, 0);
                _jetConeDraw(coord, effect->age, effect->angle, 1);
            }
            if (effect->age < PYROKINESIS_TRAIL_AGE_LIMIT) {
                spawned = effectSpawn(EFFECT_PYROKINESIS_FLAME_PUFF, coord, 0, NULL);
                if (spawned != NULL) {
                    taskReparent(task, spawned->task);
                }
            }
            if (gRoomEffectState->groundTraceEnabled != 0) {
                if (worldCollisionProjectGroundCoord(coord, &groundCoord) == 1) {
                    _pyrokinesisDrawGroundGlow(&groundCoord, effect->angle);
                }
            }
            lightSlot->framesLeft    = PYROKINESIS_LIGHT_HOLD_FRAMES;
            pointLight->inner        = (effect->index << 9) + PYROKINESIS_LIGHT_RADIUS_BASE;
            pointLight->outer        = pointLight->inner * 16;
            gRandomLcgState          = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            lightIntensity           = ((gRandomLcgState >> 16) & PYROKINESIS_LIGHT_JITTER_MASK) + PYROKINESIS_LIGHT_INTENSITY_BASE;
            pointLight->head.color.r = lightIntensity;
            pointLight->head.color.g = (u16)pointLight->head.color.r >> 1;
            pointLight->head.color.b = pointLight->head.color.r >> 2;
            lightCoord->coord.t[0]   = coord->coord.t[0];
            lightCoord->coord.t[1]   = coord->coord.t[1];
            lightCoord->coord.t[2]   = coord->coord.t[2];
            lightCoord->composeStamp = GRAPHICS_COORD_DIRTY;
            if (worldCollisionCountContactsByKind(collision->damageBody.context.contacts, WORLD_COLLISION_CONTACT_ENEMY_BODY) != 0) {
                worldCollisionUnlinkBody(&collision->damageBody);
                for (rimIndex = 0; rimIndex < PYROKINESIS_IMPACT_BEARING_LIMIT; rimIndex += PYROKINESIS_IMPACT_BEARING_STEP) {
                    spawned = effectSpawn(EFFECT_PYROKINESIS_FLAME_RING, coord, rimIndex, NULL);
                    if (spawned != NULL) {
                        taskReparent(task, spawned->task);
                    }
                }
                nextState = PYROKINESIS_STATE_BURSTING;
                if (task->spawnArg1.value == 2) {
                    nextState = PYROKINESIS_STATE_BURST_THEN_FADE;
                }
                task->state = nextState;
                return;
            }
            if (worldCollisionFindContactIndex(collision->gridBody.context.contacts, WORLD_COLLISION_CONTACT_GRID) != 0) {
                worldCollisionUnlinkBody(&collision->gridBody);
                task->state = PYROKINESIS_STATE_SHRINKING;
                return;
            }
            age = effect->age;
            if (age * PYROKINESIS_TRAVEL_EXTENT_PER_FRAME > Gp_AttachParams[ATTACHMENT_INDEX_PYROKINESIS][effect->index].area.extent) {
                worldCollisionUnlinkBody(&collision->damageBody);
                worldCollisionUnlinkBody(&collision->gridBody);
                task->state = PYROKINESIS_STATE_SHRINKING;
                return;
            }
            if (age < PYROKINESIS_TRAVEL_AGE_LIMIT) {
                worldCollisionClearContacts(collision->contacts);
                return;
            }
            worldCollisionUnlinkBody(&collision->damageBody);
            worldCollisionUnlinkBody(&collision->gridBody);
            effectKillTask(effect, task);
            return;
        case PYROKINESIS_STATE_SHRINKING:
            // The grid probe is gone; the remaining damage sphere shrinks in place.
            if ((Gp_StateC08.effectPhase == ATTACHMENT_EFFECT_HELD) || ((peEffectControl = gRoomEffectState->peEffectControl), peEffectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN)) {
                worldCollisionUnlinkBody(&collision->damageBody);
                effectKillTask(effect, task);
                return;
            }
            if (peEffectControl != ROOM_EFFECT_CONTROL_RUNNING) {
                effect->age = effect->age - 1;
                return;
            }
            actorRenderComposeCoord(coord);
            radius                       = (u16)effect->angle - PYROKINESIS_RADIUS_STEP;
            effect->angle                = radius;
            collision->damageBody.radius = radius;
            spriteQuadDraw(coord, effect->age, effect->angle, effect->period);
            glowDrawFlameDisc(coord, effect->angle, (s16)((u16)effect->scale << 16 >> 17));
            if (effect->angle >= PYROKINESIS_MIN_PUFF_RADIUS) {
                spawned = effectSpawn(EFFECT_PYROKINESIS_FLAME_PUFF, coord, 0, NULL);
                if (spawned != NULL) {
                    taskReparent(task, spawned->task);
                }
            }
            if (worldCollisionCountContactsByKind(collision->damageBody.context.contacts, WORLD_COLLISION_CONTACT_ENEMY_BODY) != 0) {
                worldCollisionUnlinkBody(&collision->damageBody);
                for (rimIndex = 0; rimIndex < PYROKINESIS_IMPACT_BEARING_LIMIT; rimIndex += PYROKINESIS_IMPACT_BEARING_STEP) {
                    spawned = effectSpawn(EFFECT_PYROKINESIS_FLAME_RING, coord, rimIndex, NULL);
                    if (spawned != NULL) {
                        taskReparent(task, spawned->task);
                    }
                }
                nextState = PYROKINESIS_STATE_BURSTING;
                if (task->spawnArg1.value == 2) {
                    nextState = PYROKINESIS_STATE_BURST_THEN_FADE;
                }
                task->state = nextState;
                return;
            }
            if (effect->angle < PYROKINESIS_SHRINK_END_RADIUS) {
                worldCollisionUnlinkBody(&collision->damageBody);
                effectKillTask(effect, task);
                return;
            }
            worldCollisionClearContacts(collision->contacts);
            return;
        case PYROKINESIS_STATE_BURSTING:
            if ((Gp_StateC08.effectPhase == ATTACHMENT_EFFECT_HELD) || ((peEffectControl = gRoomEffectState->peEffectControl), peEffectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN)) {
                worldCollisionUnlinkBody(&collision->gridBody);
                effectKillTask(effect, task);
                return;
            }
            if (peEffectControl != ROOM_EFFECT_CONTROL_RUNNING) {
                effect->age = effect->age - 1;
                return;
            }
            actorRenderComposeCoord(coord);
            spriteQuadDraw(coord, effect->age, effect->angle, effect->period);
            glowDrawFlameDisc(coord, effect->angle, (s16)((u16)effect->scale << 16 >> 17));
            glowDrawFlameDisc(coord, (s16)((u16)effect->angle * 2),
                              (s16)((u16)effect->scale << 16 >> 17));
            effect->angle = effect->angle + PYROKINESIS_RADIUS_STEP;
            if (effect->angle > ((effect->index << 9) + PYROKINESIS_BURST_RADIUS_BASE)) {
                worldCollisionUnlinkBody(&collision->gridBody);
                effectKillTask(effect, task);
                return;
            }
            return;
        case PYROKINESIS_STATE_BURST_THEN_FADE:
            // Radius keeps expanding after its limit while this variant fades intensity.
            if ((Gp_StateC08.effectPhase == ATTACHMENT_EFFECT_HELD) || ((peEffectControl = gRoomEffectState->peEffectControl), peEffectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN)) {
                worldCollisionUnlinkBody(&collision->gridBody);
                effectKillTask(effect, task);
                return;
            }
            if (peEffectControl != ROOM_EFFECT_CONTROL_RUNNING) {
                effect->age = effect->age - 1;
                return;
            }
            actorRenderComposeCoord(coord);
            spriteQuadDraw(coord, effect->age, effect->angle, effect->period);
            glowDrawFlameDisc(coord, effect->angle, (s16)((u16)effect->scale << 16 >> 17));
            glowDrawFlameDisc(coord, (s16)((u16)effect->angle * 2),
                              (s16)((u16)effect->scale << 16 >> 17));
            effect->angle = effect->angle + PYROKINESIS_RADIUS_STEP;
            if (effect->angle > ((effect->index << 9) + PYROKINESIS_BURST_RADIUS_BASE)) {
                if (effect->scale >= PYROKINESIS_FADE_END_BRIGHTNESS) {
                    effect->scale = effect->scale - PYROKINESIS_BRIGHTNESS_STEP;
                    return;
                }
                worldCollisionUnlinkBody(&collision->gridBody);
                effectKillTask(effect, task);
            }
            return;
    }
}

void pyrokinesisVolleyTask(Task* task)
{
    enum {
        PYROKINESIS_VOLLEY_FIRST      = 0,
        PYROKINESIS_VOLLEY_SECOND     = 1,
        PYROKINESIS_VOLLEY_THIRD      = 2,
        PYROKINESIS_VOLLEY_RELEASE    = 3,
        PYROKINESIS_VOLLEY_SECOND_AGE = 8,
        PYROKINESIS_VOLLEY_THIRD_AGE  = 16,
    };
    EffectWork* effect;
    GfxCoord*   coord;
    s16         battleState;
    s16         peEffectControl;

    effect = task->spawnArg2.pointer;
    coord  = task->extra.coordBody->coord;
    if (Gp_StateC08.effectPhase != ATTACHMENT_EFFECT_HELD) {
        battleState = gRoomEffectState->battleState;
        if (battleState == ROOM_EFFECT_BATTLE_ENGAGED) {
            peEffectControl = gRoomEffectState->peEffectControl;
            if (peEffectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
                if (peEffectControl != ROOM_EFFECT_CONTROL_RUNNING) {
                    return;
                }
                effect->age = effect->age + 1;
                actorRenderComposeCoord(coord);
                switch (task->state) {
                    case PYROKINESIS_VOLLEY_FIRST:
                        effectSpawn((EFFECT_PYROKINESIS_CAST | EFFECT_SPAWN_UNLIMITED), coord, 0, 0);
                        task->state = PYROKINESIS_VOLLEY_SECOND;
                        return;
                    case PYROKINESIS_VOLLEY_SECOND:
                        if (effect->age == PYROKINESIS_VOLLEY_SECOND_AGE) {
                            effectSpawn((EFFECT_PYROKINESIS_CAST | EFFECT_SPAWN_UNLIMITED), coord, 1, 0);
                            task->state = PYROKINESIS_VOLLEY_THIRD;
                        }
                        return;
                    case PYROKINESIS_VOLLEY_THIRD:
                        if (effect->age == PYROKINESIS_VOLLEY_THIRD_AGE) {
                            effectSpawn(EFFECT_PYROKINESIS_CAST | EFFECT_SPAWN_UNLIMITED, coord, 2, 0);
                            task->state = PYROKINESIS_VOLLEY_RELEASE;
                        }
                        return;
                    case PYROKINESIS_VOLLEY_RELEASE:
                        break;
                    default:
                        return;
                }
            }
        }
    }
    effectKillTask(effect, task);
}

#include "../../shared/glow_draw_flame_band.inc.c"

#include "../../shared/glow_draw_flame_star.inc.c"

/// Projects the ground glow's four staged corners, retaining the final GTE FLAG.
///
/// Borrows a live `EffectQuadScratch` with all four vertices initialized;
/// the caller has set the translation matrix. Saves corner 0 before RTPT replaces the screen FIFO, then leaves
/// corner 3's depth in SZ3 for the caller to capture before another transform.
/// Only RTPT's FLAG is saved; the first corner's RTPS FLAG is discarded.
static inline void _pyrokinesisProjectGroundGlow(EffectQuadScratch* quadScratch)
{
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&quadScratch->vertices[0]);
    gte_rtps();
    gte_stsxy(&quadScratch->screenCorners[0]);
    gte_ldv3(&quadScratch->vertices[1], &quadScratch->vertices[2], &quadScratch->vertices[3]);
    gte_rtpt();
    gte_stsxy3(&quadScratch->screenCorners[1], &quadScratch->screenCorners[2], &quadScratch->screenCorners[3]);
    gte_stflg(&quadScratch->projectionFlags);
}

/// Queues the alternating additive ground glow beneath a travelling flame.
///
/// Borrows a ground-hit coordinate with a composed translation. `halfSize`
/// is the square's half-side in coordinate units before view-frame rotation;
/// staged corners and translated positions narrow to signed 16 bits. The two
/// 32-by-32 texture cells alternate with the display animation frame. Requires
/// initialized GTE projection, a word-aligned scratch stack with room for one
/// `EffectQuadScratch`, and arena space for one `POLY_FT4`. A negative final
/// GTE FLAG rejects the quad; scratch storage is released on either path.
static void _pyrokinesisDrawGroundGlow(const GfxCoord* groundCoord, s32 halfSize)
{
    enum {
        PYROKINESIS_GROUND_GLOW_TEXTURE_DEPTH_4BIT = 0,
        PYROKINESIS_GROUND_GLOW_CELL_SIZE          = 32,
        PYROKINESIS_GROUND_GLOW_LEFT_U             = 192,
        PYROKINESIS_GROUND_GLOW_TOP_V              = 56,
        PYROKINESIS_GROUND_GLOW_UV_SPAN            = PYROKINESIS_GROUND_GLOW_CELL_SIZE - 1,
    };

    EffectQuadScratch* quadScratch;
    s32                cornerIndex;
    POLY_FT4*          quad;
    s32                textureU;

    quadScratch = SCRATCH_STACK_RESERVE_BLOCK(EffectQuadScratch);
    gte_SetTransMatrix(&GsWSMATRIX);
    // Rotate the ground-plane offsets, then centre them on the composed hit.
    for (cornerIndex = 0; cornerIndex < ARRAY_SIZE(D_80111E38); cornerIndex++) {
        quadScratch->vertices[cornerIndex].vx = (u16)D_80111E38[cornerIndex].axis0Sign * halfSize;
        quadScratch->vertices[cornerIndex].vy = 0;
        quadScratch->vertices[cornerIndex].vz = (u16)D_80111E38[cornerIndex].axis1Sign * halfSize;
        gte_SetRotMatrix(&gGfxViewCoord.workm);
        gte_ldv0(&quadScratch->vertices[cornerIndex]);
        gte_rtv0();
        gte_stsv(&quadScratch->vertices[cornerIndex]);
        quadScratch->vertices[cornerIndex].vx += groundCoord->workm.t[0];
        quadScratch->vertices[cornerIndex].vy += groundCoord->workm.t[1];
        quadScratch->vertices[cornerIndex].vz += groundCoord->workm.t[2];
    }

    _pyrokinesisProjectGroundGlow(quadScratch);
    if (quadScratch->projectionFlags >= 0) {
        gte_stszotz(&quadScratch->depth);
        quadScratch->depth++;
        quad           = gGpuPrimCursor;
        gGpuPrimCursor = quad + 1;
        setPolyFT4(quad);
        setSemiTrans(quad, 1);
        setRGB0(quad, 0x30, 0x20, 0x20);
        quad->tpage = getTPage(PYROKINESIS_GROUND_GLOW_TEXTURE_DEPTH_4BIT, GPU_BLEND_ADD, 512, 0);
        quad->clut  = getClut(192, 266);
        textureU    = (gDisplayState.animFrame & 1) * PYROKINESIS_GROUND_GLOW_CELL_SIZE + PYROKINESIS_GROUND_GLOW_LEFT_U;
        quad->v0    = PYROKINESIS_GROUND_GLOW_TOP_V;
        quad->u0    = textureU;
        textureU    = (gDisplayState.animFrame & 1) * PYROKINESIS_GROUND_GLOW_CELL_SIZE + PYROKINESIS_GROUND_GLOW_LEFT_U + PYROKINESIS_GROUND_GLOW_UV_SPAN;
        quad->v1    = PYROKINESIS_GROUND_GLOW_TOP_V;
        quad->u1    = textureU;
        textureU    = (gDisplayState.animFrame & 1) * PYROKINESIS_GROUND_GLOW_CELL_SIZE + PYROKINESIS_GROUND_GLOW_LEFT_U;
        quad->v2    = PYROKINESIS_GROUND_GLOW_TOP_V + PYROKINESIS_GROUND_GLOW_UV_SPAN;
        quad->u2    = textureU;
        textureU    = (gDisplayState.animFrame & 1) * PYROKINESIS_GROUND_GLOW_CELL_SIZE + PYROKINESIS_GROUND_GLOW_LEFT_U + PYROKINESIS_GROUND_GLOW_UV_SPAN;
        quad->v3    = PYROKINESIS_GROUND_GLOW_TOP_V + PYROKINESIS_GROUND_GLOW_UV_SPAN;
        quad->u3    = textureU;
        quad->x0    = quadScratch->screenCorners[0].vx;
        quad->y0    = quadScratch->screenCorners[0].vy;
        quad->x1    = quadScratch->screenCorners[1].vx;
        quad->y1    = quadScratch->screenCorners[1].vy;
        quad->x2    = quadScratch->screenCorners[2].vx;
        quad->y2    = quadScratch->screenCorners[2].vy;
        quad->x3    = quadScratch->screenCorners[3].vx;
        quad->y3    = quadScratch->screenCorners[3].vy;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)quadScratch->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                quad);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectQuadScratch);
}

/// Packed additive flame texture page: 4-bit indexed texels at VRAM X=576 words, Y=0 scanlines.
#define SPRITE_QUAD_TEXTURE_PAGE getTPage(0, GPU_BLEND_ADD, 576, 0)
/// Flame-strip palette: VRAM X=192 words, Y=266 scanlines, the rim look in `SPRITE_QUAD_RIM_CELL`.
#define SPRITE_QUAD_CLUT getClut(192, 266)
/// Texel width and horizontal stride of the two flame cells selected by age parity.
#define SPRITE_QUAD_CELL_WIDTH 56
#define SPRITE_QUAD_CELL_MASK  1
/// First flame-cell U coordinate in texels relative to the selected texture page.
///
/// The effect age's low bit selects U = 0x70..0xA7 or 0xA8..0xDF.
/// This integer constant configures the next drawer inclusion, which undefines it.
#define SPRITE_QUAD_U_BASE 0x70
/// Inclusive top texel row of both flame cells, relative to their texture page.
///
/// Signed integer constant for the next drawer inclusion; see `sprite_quad.h`.
#define SPRITE_QUAD_TOP_V 0xC8
#define SPRITE_QUAD_V1    0xFF
/// Perspective-sizing multiplier for the pyrokinesis flame sprite.
///
/// Uses the cell's inclusive 55-texel UV span in `size * SPRITE_QUAD_SCALE / depth`.
#define SPRITE_QUAD_SCALE (SPRITE_QUAD_CELL_WIDTH - 1)
#include "../../shared/sprite_quad_draw.inc.c"

void pyrokinesisFlamePuffTask(Task* task)
{
    enum {
        PYROKINESIS_FLAME_PUFF_INITIALIZE,
        PYROKINESIS_FLAME_PUFF_ACTIVE,
        PYROKINESIS_FLAME_PUFF_RISE_SPEED_COUNT = 32,
        PYROKINESIS_FLAME_PUFF_SIZE_FACTOR      = 768,
    };

    EffectWork* work;
    GfxCoord*   coord;
    s16         peEffectControl;
    s16         textureFrame;
    s32         nextY;

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    if (Gp_StateC08.effectPhase != ATTACHMENT_EFFECT_HELD) {
        peEffectControl = gRoomEffectState->peEffectControl;
        if (peEffectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            if (peEffectControl != ROOM_EFFECT_CONTROL_RUNNING) {
                return;
            }
            work->age++;
            if (task->state == PYROKINESIS_FLAME_PUFF_INITIALIZE) {
                // Choose a fixed rise velocity and screen rotation once per puff.
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->move.vy   = -((gRandomLcgState >> 16) & (PYROKINESIS_FLAME_PUFF_RISE_SPEED_COUNT - 1));
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->scale     = (gRandomLcgState >> 16) & (PYRO_FLAME_FULL_TURN - 1);
                task->state     = PYROKINESIS_FLAME_PUFF_ACTIVE;
            }
            nextY               = coord->coord.t[1] + work->move.vy;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            coord->coord.t[1]   = nextY;
            actorRenderComposeCoord(coord);
            // Each cell lasts two running ticks, but only its odd tick is drawn.
            if (!(work->age & 1)) {
                work->index++;
            }
            textureFrame = work->index;
            if (textureFrame < PYRO_FLAME_FRAME_COUNT) {
                if (work->age & 1) {
                    _pyroFlameDrawSprite(coord, textureFrame, PYROKINESIS_FLAME_PUFF_SIZE_FACTOR, work->scale);
                }
                return;
            }
        }
    }
    effectKillTask(work, task);
}

#include "../../shared/pyro_flame_draw_sprite.inc.c"

void pyrokinesisFlameRingTask(Task* task)
{
    enum {
        PYROKINESIS_FLAME_RING_INITIALIZE,
        PYROKINESIS_FLAME_RING_ACTIVE,
        PYROKINESIS_FLAME_RING_INITIAL_INTENSITY = 128,
        PYROKINESIS_FLAME_RING_INITIAL_RADIUS    = 256,
        PYROKINESIS_FLAME_RING_WIDTH             = 256,
        PYROKINESIS_FLAME_RING_RADIUS_STEP       = 128,
        PYROKINESIS_FLAME_RING_INTENSITY_STEP    = 8,
        PYROKINESIS_FLAME_RING_MIN_INTENSITY     = 9,
    };

    EffectWork* work;
    GfxCoord*   coord;
    s16         peEffectControl;

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    if (Gp_StateC08.effectPhase != ATTACHMENT_EFFECT_HELD) {
        peEffectControl = gRoomEffectState->peEffectControl;
        if (peEffectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            if (peEffectControl != ROOM_EFFECT_CONTROL_RUNNING) {
                return;
            }
            if (task->state == PYROKINESIS_FLAME_RING_INITIALIZE) {
                // Tilt the local XZ ring by the spawn angle once; expansion keeps that plane.
                gfxRotMatrixZ(&coord->coord, task->spawnArg1.value, GRAPHICS_ROTATION_COMPOSE);
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
                work->scale         = PYROKINESIS_FLAME_RING_INITIAL_INTENSITY;
                work->angle         = PYROKINESIS_FLAME_RING_INITIAL_RADIUS;
                task->state         = PYROKINESIS_FLAME_RING_ACTIVE;
            }
            actorRenderComposeCoord(coord);
            glowDrawFlameRing(coord, work->angle, PYROKINESIS_FLAME_RING_WIDTH, work->scale);
            work->angle += PYROKINESIS_FLAME_RING_RADIUS_STEP;
            work->scale -= PYROKINESIS_FLAME_RING_INTENSITY_STEP;
            if (work->scale >= PYROKINESIS_FLAME_RING_MIN_INTENSITY) {
                return;
            }
        }
    }
    effectKillTask(work, task);
}

#include "../../shared/glow_draw_flame_ring.inc.c"

#define JET_CONE_CLUT         0x4282
#define JET_CONE_RIM_SHORT    0x200
#define JET_CONE_RIM_LONG     0x480
#define JET_CONE_FRAME_JITTER D_pyrokinesis_80131DFC
#include "../../shared/jet_cone_draw.inc.c"

void pyrokinesisLaunchConeTask(Task* task)
{
    enum {
        PYROKINESIS_LAUNCH_CONE_INITIALIZE,
        PYROKINESIS_LAUNCH_CONE_ACTIVE,
        PYROKINESIS_LAUNCH_CONE_INITIAL_INTENSITY = 192,
        PYROKINESIS_LAUNCH_CONE_INITIAL_RADIUS    = 256,
        PYROKINESIS_LAUNCH_CONE_RADIUS_STEP       = 64,
        PYROKINESIS_LAUNCH_CONE_INTENSITY_STEP    = 16,
    };

    EffectWork* work;
    GfxCoord*   coord;
    s16         peEffectControl;

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    if (Gp_StateC08.effectPhase != ATTACHMENT_EFFECT_HELD) {
        peEffectControl = gRoomEffectState->peEffectControl;
        if (peEffectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            if (peEffectControl != ROOM_EFFECT_CONTROL_RUNNING) {
                return;
            }
            work->age++;
            if (task->state == PYROKINESIS_LAUNCH_CONE_INITIALIZE) {
                work->scale = PYROKINESIS_LAUNCH_CONE_INITIAL_INTENSITY;
                work->angle = PYROKINESIS_LAUNCH_CONE_INITIAL_RADIUS;
                task->state = PYROKINESIS_LAUNCH_CONE_ACTIVE;
            }
            actorRenderComposeCoord(coord);
            // Draw the current ring pair before advancing its radius and intensity.
            glowDrawFlameCone(task->extra.coordBody->coord, work->angle, work->scale);
            work->angle += PYROKINESIS_LAUNCH_CONE_RADIUS_STEP;
            work->scale -= PYROKINESIS_LAUNCH_CONE_INTENSITY_STEP;
            if (work->scale >= PYROKINESIS_LAUNCH_CONE_INTENSITY_STEP) {
                return;
            }
        }
    }
    effectKillTask(work, task);
}
