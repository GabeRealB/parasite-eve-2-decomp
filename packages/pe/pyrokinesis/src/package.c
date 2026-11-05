#include "pyrokinesis.h"

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
#include <gameplay/pad_script.h>
#include <gameplay/room_effects.h>
#include <gameplay/scene.h>
#include <gameplay/world_collision.h>
#include <gameplay/world_coords.h>

#include <engine/coord.h>
#include <engine/display.h>
#include <engine/display_types.h>
#include <engine/random.h>
#include <engine/gfx.h>
#include <engine/gfx_types.h>
#include <engine/mem.h>
#include <engine/scratch.h>
#include <engine/session.h>
#include <engine/session_types.h>
#include <engine/sound.h>
#include <engine/task.h>
#include <engine/task_types.h>
#include <engine/tmd_types.h>
#include <effects/glow/glow.h>
#include <pe/effects/pyro_flame/pyro_flame.h>
/// Signed effect-age argument whose low bit selects one of the two flame cells.
#define SPRITE_QUAD_FRAME_T s16
#include <effects/sprite_quad/sprite_quad.h>
#include <effects/jet_cone/jet_cone.h>

PE2_EDITOR_LABEL("Volley")
/// The frames the volley's second and third flames launch on; the first launches at once.
PE2_OPTION_DEF(VOLLEY_2_FRAME, int, 8)
PE2_OPTION_DEF(VOLLEY_3_FRAME, int, 16)

PE2_EDITOR_LABEL("Flame")
/// The flame's speed along the caster's facing, plus what each level adds.
PE2_OPTION_DEF(SPEED, int, 0x1C0)
PE2_OPTION_DEF(LEVEL_SPEED, int, 64)
/// Its radius as it launches, while it travels, and the radius a burst grows to; each level adds the level radius to
/// the last two.
PE2_OPTION_DEF(LAUNCH_RADIUS, int, 0x500)
PE2_OPTION_DEF(FLAME_RADIUS, int, 0x380)
PE2_OPTION_DEF(BURST_RADIUS, int, 0x580)
PE2_OPTION_DEF(LEVEL_RADIUS, int, 512)
/// What its radius loses each frame once a wall stops it, and gains each frame of a burst; a stopped flame sheds puffs
/// until it is down to the end radius, and goes.
PE2_OPTION_DEF(RADIUS_STEP, int, 0x40)
PE2_OPTION_DEF(SHRINK_END_RADIUS, int, 0x80)
/// Each frame of travel counts as this much of the spell's range; the flame sheds puffs up to the trail frames, and
/// goes after the travel frames.
PE2_OPTION_DEF(RANGE_PER_FRAME, int, 6)
PE2_OPTION_DEF(TRAIL_FRAMES, int, 0x1E)
PE2_OPTION_DEF(TRAVEL_FRAMES, int, 0x1F)
/// Its brightness, and what the third of a volley loses each frame after its burst.
PE2_OPTION_DEF(BRIGHTNESS, int, 0xC0)
PE2_OPTION_DEF(FADE_STEP, int, 8)
/// The screen's tint as it launches, and the tint of the glow under it, color words.
PE2_OPTION_DEF(IGNITION_COLOR, int, 0x3F7FFF)
PE2_OPTION_DEF(GROUND_GLOW_TINT, int, 0x202030)
/// The pad's rumble as it launches, in frames, what each level adds, and its strength from start to end.
PE2_OPTION_DEF(RUMBLE_FRAMES, int, 8)
PE2_OPTION_DEF(LEVEL_RUMBLE_FRAMES, int, 2)
PE2_OPTION_DEF(RUMBLE_START, int, 0xFF)
PE2_OPTION_DEF(RUMBLE_END, int, 8)

PE2_EDITOR_LABEL("Light")
/// The light it casts: its inner radius and what each level adds, its outer radius as a multiple of the inner, and its
/// intensity, plus a random value's bits under the mask; green and blue are dimmer than red by their shifts.
PE2_OPTION_DEF(LIGHT_RADIUS, int, 0x200)
PE2_OPTION_DEF(LEVEL_LIGHT_RADIUS, int, 512)
PE2_OPTION_DEF(LIGHT_OUTER_SCALE, int, 16)
PE2_OPTION_DEF(LIGHT_INTENSITY, int, 0x800)
PE2_OPTION_DEF(LIGHT_FLICKER_MASK, int, 0x700)
PE2_OPTION_DEF(LIGHT_GREEN_SHIFT, int, 1)
PE2_OPTION_DEF(LIGHT_BLUE_SHIFT, int, 2)

PE2_EDITOR_LABEL("Jet cones")
/// The rims of the cones behind the second and third flames of a volley: the base one's radius, and the extended
/// one's.
PE2_OPTION_DEF(JET_BASE_RIM_RADIUS, int, 0x480)
PE2_OPTION_DEF(JET_TAIL_RIM_RADIUS, int, 0x200)

PE2_EDITOR_LABEL("Puffs")
/// The range of a trail puff's rising speed, a power of two, and its size.
PE2_OPTION_DEF(PUFF_RISE_JITTER, int, 32)
PE2_OPTION_DEF(PUFF_SIZE, int, 768)

PE2_EDITOR_LABEL("Impact rings")
/// A ring's first radius and brightness, its width, and what it grows and loses each frame; it goes when its brightness
/// is spent.
PE2_OPTION_DEF(RING_RADIUS, int, 256)
PE2_OPTION_DEF(RING_BRIGHTNESS, int, 128)
PE2_OPTION_DEF(RING_WIDTH, int, 256)
PE2_OPTION_DEF(RING_GROWTH, int, 128)
PE2_OPTION_DEF(RING_FADE_STEP, int, 8)

PE2_EDITOR_LABEL("Launch cone")
/// The cone at the launch point: its first radius and brightness, and what it grows and loses each frame.
PE2_OPTION_DEF(CONE_RADIUS, int, 256)
PE2_OPTION_DEF(CONE_BRIGHTNESS, int, 192)
PE2_OPTION_DEF(CONE_GROWTH, int, 64)
PE2_OPTION_DEF(CONE_FADE_STEP, int, 16)
PE2_EDITOR_LABEL("")

STATIC_ASSERT(IS_COLOR(PE2_OPTION(IGNITION_COLOR)) && IS_COLOR(PE2_OPTION(GROUND_GLOW_TINT)), colors);
STATIC_ASSERT(POWER_OF_TWO(PE2_OPTION(PUFF_RISE_JITTER)), puff_rise_jitter_is_a_mask);
STATIC_ASSERT(PE2_OPTION(VOLLEY_2_FRAME) > 1 && PE2_OPTION(VOLLEY_2_FRAME) < PE2_OPTION(VOLLEY_3_FRAME), the_volley_launches_in_order);
STATIC_ASSERT(PE2_OPTION(RADIUS_STEP) > 0 && PE2_OPTION(FADE_STEP) > 0 && PE2_OPTION(RING_FADE_STEP) > 0 &&
                  PE2_OPTION(CONE_FADE_STEP) > 0,
              steps);

/// The flame's collision, which the cast allocates and frees: a damage sphere keyed with the spell's damage id, which an
/// enemy's body bursts, and a sphere the room's grid stops, both on the flame and sharing one contact.
typedef struct {
    WorldCollisionBody    damageBody;  // Its radius is the flame's
    WorldCollisionBody    gridBody;    // An eighth of the launch radius
    WorldCollisionContact contacts[1]; // Cleared each frame it neither bursts nor stops the flame
} _FlameCollision;

static void _drawGroundGlow(const GfxCoord* groundCoord, s32 halfSize);

enum {
    CAST_VARIANT_COUNT        = 3,
    CAST_SOUND_LEVEL_1        = (s32)0xE00B0002,
    CAST_SOUND_LEVEL_2        = (s32)0xE00E0002,
    CAST_SOUND_LEVEL_3_FIRST  = (s32)0xE0110002,
    CAST_SOUND_LEVEL_3_SECOND = (s32)0xE0110003,
    CAST_SOUND_LEVEL_3_THIRD  = (s32)0xE0110004,
};

/// The cue each level's flame plays, by its place in a volley.
static s32 _gCastSoundScripts[ATTACHMENT_LEVEL_COUNT][CAST_VARIANT_COUNT] = {
    { CAST_SOUND_LEVEL_1, CAST_SOUND_LEVEL_1, CAST_SOUND_LEVEL_1 },
    { CAST_SOUND_LEVEL_2, CAST_SOUND_LEVEL_2, CAST_SOUND_LEVEL_2 },
    { CAST_SOUND_LEVEL_3_FIRST, CAST_SOUND_LEVEL_3_SECOND, CAST_SOUND_LEVEL_3_THIRD },
};

/// Each jet cone segment's random offset into its animation, chosen when a flame launches.
static s16 _gConeFrameOffsets[EFFECT_BAND_SEGMENT_COUNT] = { 0 };

/// Moves the flame by its velocity and composes it.
static inline void _advanceCastCoord(GfxCoord* coord, const EffectWork* effect)
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
        PHASE_JITTER_MASK     = 0xFF,
        LIGHT_HOLD_FRAMES     = 4,
        STATE_INITIALIZE      = 0,
        STATE_TRAVELLING      = 1,
        STATE_SHRINKING       = 2,
        STATE_BURSTING        = 3,
        STATE_BURST_THEN_FADE = 4,
        SPELL_DAMAGE_KEY_BASE = WORLD_COLLISION_CONTACT_ATTACK | 0x8000,
        ANGLE_MASK            = 0xFFF,
        IMPACT_BEARING_LIMIT  = 0x556, // Three rings a sixth of a turn apart
        IMPACT_BEARING_STEP   = 0x2AA,
    };
    EffectWork*                    effect;
    GfxCoord*                      coord;
    _FlameCollision*               collision;
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
        case STATE_INITIALIZE:
            if ((gAttachmentState.effectPhase == ATTACHMENT_EFFECT_HELD) || ((peEffectControl = gRoomEffectState->peEffectControl), peEffectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN)) {
                effectKillTask(effect, task);
                return;
            }
            if (peEffectControl != ROOM_EFFECT_CONTROL_RUNNING) {
                effect->age = effect->age - 1;
                return;
            }
            collision = memCalloc(sizeof(_FlameCollision), 0);
            if (collision == NULL) {
                effect->age = 0;
                return;
            }
            playerCoord = (gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords;
            // Borrow only the player's rotation; launch in that forward frame.
            gfxCopyRotationWords(&coord->coord, &playerCoord->coord);
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(coord);
            effect->move.vx = 0;
            effect->move.vy = 0;
            effect->move.vz = (gAttachmentState.attachId % 10) * PE2_OPTION(LEVEL_SPEED) + PE2_OPTION(SPEED);
            gte_SetRotMatrix(&playerCoord->coord);
            gte_ldv0(&effect->move);
            gte_rtv0();
            gte_stsv(&effect->move);
            for (rimIndex = 0; rimIndex < ARRAY_SIZE(_gConeFrameOffsets); rimIndex++) {
                RANDOM_LCG_ADVANCE();
                _gConeFrameOffsets[rimIndex] = RANDOM_LCG_DRAW() & PHASE_JITTER_MASK;
            }
            effect->scale = PE2_OPTION(BRIGHTNESS);
            effect->angle = PE2_OPTION(LAUNCH_RADIUS);
            RANDOM_LCG_ADVANCE();
            effect->period = RANDOM_LCG_DRAW() & ANGLE_MASK;
            effect->index  = (gAttachmentState.attachId % 10) - 1;
            pan            = (s8)worldCoordGetOriginAudioPan(coord);
            sndEvtRequestScriptStart(_gCastSoundScripts[effect->index][task->spawnArg1.value], pan,
                                     (s8)worldCoordGetOriginAudioDepth(coord));
            padScriptSpawnVariableMotorRamp((s16)(effect->index * PE2_OPTION(LEVEL_RUMBLE_FRAMES) + PE2_OPTION(RUMBLE_FRAMES)), PE2_OPTION(RUMBLE_START), PE2_OPTION(RUMBLE_END));
            // Choose the sound with the requested variant before normalizing cone behavior.
            if (effect->index == 1) {
                task->spawnArg1.value = 1;
            } else if (task->spawnArg1.value == 1) {
                task->spawnArg1.value = 0;
            }
            task->work                             = collision;
            collision->damageBody.coord            = coord;
            collision->damageBody.context.contacts = collision->contacts;
            collision->damageBody.key              = ((u16)(gAttachmentState.attachId / 100) - 1) * 9 +
                                        ((u16)((u16)(gAttachmentState.attachId % 100) / 10) - 1) * 3 +
                                        (u16)(gAttachmentState.attachId % 10) + SPELL_DAMAGE_KEY_BASE;
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
            rgb[0] = COLOR_RED(PE2_OPTION(IGNITION_COLOR));
            rgb[1] = COLOR_GREEN(PE2_OPTION(IGNITION_COLOR));
            rgb[2] = COLOR_BLUE(PE2_OPTION(IGNITION_COLOR));
            effectDrawScreenTint(rgb, GPU_BLEND_ADD);
            task->state = STATE_TRAVELLING;
            spriteQuadDraw(coord, effect->age, effect->angle, effect->period);
            glowDrawFlameDisc(coord, effect->angle, (s16)((u16)effect->scale << 16 >> 17));
            if (worldCollisionCountContactsByKind(collision->damageBody.context.contacts, WORLD_COLLISION_CONTACT_ENEMY_BODY) != 0) {
                worldCollisionUnlinkBody(&collision->damageBody);
                radius        = effect->index * PE2_OPTION(LEVEL_RADIUS) + PE2_OPTION(FLAME_RADIUS);
                effect->angle = radius;
                for (rimIndex = 0; rimIndex < IMPACT_BEARING_LIMIT; rimIndex += IMPACT_BEARING_STEP) {
                    spawned = effectSpawn(EFFECT_PYROKINESIS_FLAME_RING, coord, rimIndex, NULL);
                    if (spawned != NULL) {
                        taskReparent(task, spawned->task);
                    }
                }
                nextState = STATE_BURSTING;
                if (task->spawnArg1.value == 2) {
                    nextState = STATE_BURST_THEN_FADE;
                }
                task->state = nextState;
                return;
            }
            if (worldCollisionFindContactIndex(collision->gridBody.context.contacts, WORLD_COLLISION_CONTACT_GRID) != 0) {
                worldCollisionUnlinkBody(&collision->gridBody);
                task->state = STATE_SHRINKING;
                return;
            }
            worldCollisionClearContacts(collision->contacts);
            return;
        case STATE_TRAVELLING:
            if ((gAttachmentState.effectPhase == ATTACHMENT_EFFECT_HELD) || ((peEffectControl = gRoomEffectState->peEffectControl), peEffectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN)) {
                worldCollisionUnlinkBody(&collision->damageBody);
                worldCollisionUnlinkBody(&collision->gridBody);
                effectKillTask(effect, task);
                return;
            }
            if (peEffectControl != ROOM_EFFECT_CONTROL_RUNNING) {
                effect->age = effect->age - 1;
                return;
            }
            radius                       = effect->index * PE2_OPTION(LEVEL_RADIUS) + PE2_OPTION(FLAME_RADIUS);
            effect->angle                = radius;
            collision->damageBody.radius = radius;
            _advanceCastCoord(coord, effect);
            spriteQuadDraw(coord, effect->age, effect->angle, effect->period);
            glowDrawFlameDisc(coord, effect->angle, (s16)((u16)effect->scale << 16 >> 17));
            if (task->spawnArg1.value != 0) {
                _jetConeDraw(coord, effect->age, effect->angle, 0);
                _jetConeDraw(coord, effect->age, effect->angle, 1);
            }
            if (effect->age < PE2_OPTION(TRAIL_FRAMES)) {
                spawned = effectSpawn(EFFECT_PYROKINESIS_FLAME_PUFF, coord, 0, NULL);
                if (spawned != NULL) {
                    taskReparent(task, spawned->task);
                }
            }
            if (gRoomEffectState->groundTraceEnabled != 0) {
                if (worldCollisionProjectGroundCoord(coord, &groundCoord) == 1) {
                    _drawGroundGlow(&groundCoord, effect->angle);
                }
            }
            lightSlot->framesLeft = LIGHT_HOLD_FRAMES;
            pointLight->inner     = effect->index * PE2_OPTION(LEVEL_LIGHT_RADIUS) + PE2_OPTION(LIGHT_RADIUS);
            pointLight->outer     = pointLight->inner * PE2_OPTION(LIGHT_OUTER_SCALE);
            RANDOM_LCG_ADVANCE();
            lightIntensity           = (RANDOM_LCG_DRAW() & PE2_OPTION(LIGHT_FLICKER_MASK)) + PE2_OPTION(LIGHT_INTENSITY);
            pointLight->head.color.r = lightIntensity;
            pointLight->head.color.g = (u16)pointLight->head.color.r >> PE2_OPTION(LIGHT_GREEN_SHIFT);
            pointLight->head.color.b = pointLight->head.color.r >> PE2_OPTION(LIGHT_BLUE_SHIFT);
            lightCoord->coord.t[0]   = coord->coord.t[0];
            lightCoord->coord.t[1]   = coord->coord.t[1];
            lightCoord->coord.t[2]   = coord->coord.t[2];
            lightCoord->composeStamp = GRAPHICS_COORD_DIRTY;
            if (worldCollisionCountContactsByKind(collision->damageBody.context.contacts, WORLD_COLLISION_CONTACT_ENEMY_BODY) != 0) {
                worldCollisionUnlinkBody(&collision->damageBody);
                for (rimIndex = 0; rimIndex < IMPACT_BEARING_LIMIT; rimIndex += IMPACT_BEARING_STEP) {
                    spawned = effectSpawn(EFFECT_PYROKINESIS_FLAME_RING, coord, rimIndex, NULL);
                    if (spawned != NULL) {
                        taskReparent(task, spawned->task);
                    }
                }
                nextState = STATE_BURSTING;
                if (task->spawnArg1.value == 2) {
                    nextState = STATE_BURST_THEN_FADE;
                }
                task->state = nextState;
                return;
            }
            if (worldCollisionFindContactIndex(collision->gridBody.context.contacts, WORLD_COLLISION_CONTACT_GRID) != 0) {
                worldCollisionUnlinkBody(&collision->gridBody);
                task->state = STATE_SHRINKING;
                return;
            }
            age = effect->age;
            if (age * PE2_OPTION(RANGE_PER_FRAME) > gAttachmentAreaTable[ATTACHMENT_INDEX_PYROKINESIS][effect->index].area.extent) {
                worldCollisionUnlinkBody(&collision->damageBody);
                worldCollisionUnlinkBody(&collision->gridBody);
                task->state = STATE_SHRINKING;
                return;
            }
            if (age < PE2_OPTION(TRAVEL_FRAMES)) {
                worldCollisionClearContacts(collision->contacts);
                return;
            }
            worldCollisionUnlinkBody(&collision->damageBody);
            worldCollisionUnlinkBody(&collision->gridBody);
            effectKillTask(effect, task);
            return;
        case STATE_SHRINKING:
            // The grid probe is gone; the remaining damage sphere shrinks in place.
            if ((gAttachmentState.effectPhase == ATTACHMENT_EFFECT_HELD) || ((peEffectControl = gRoomEffectState->peEffectControl), peEffectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN)) {
                worldCollisionUnlinkBody(&collision->damageBody);
                effectKillTask(effect, task);
                return;
            }
            if (peEffectControl != ROOM_EFFECT_CONTROL_RUNNING) {
                effect->age = effect->age - 1;
                return;
            }
            actorRenderComposeCoord(coord);
            radius                       = (u16)effect->angle - PE2_OPTION(RADIUS_STEP);
            effect->angle                = radius;
            collision->damageBody.radius = radius;
            spriteQuadDraw(coord, effect->age, effect->angle, effect->period);
            glowDrawFlameDisc(coord, effect->angle, (s16)((u16)effect->scale << 16 >> 17));
            if (effect->angle >= (PE2_OPTION(SHRINK_END_RADIUS) + 1)) {
                spawned = effectSpawn(EFFECT_PYROKINESIS_FLAME_PUFF, coord, 0, NULL);
                if (spawned != NULL) {
                    taskReparent(task, spawned->task);
                }
            }
            if (worldCollisionCountContactsByKind(collision->damageBody.context.contacts, WORLD_COLLISION_CONTACT_ENEMY_BODY) != 0) {
                worldCollisionUnlinkBody(&collision->damageBody);
                for (rimIndex = 0; rimIndex < IMPACT_BEARING_LIMIT; rimIndex += IMPACT_BEARING_STEP) {
                    spawned = effectSpawn(EFFECT_PYROKINESIS_FLAME_RING, coord, rimIndex, NULL);
                    if (spawned != NULL) {
                        taskReparent(task, spawned->task);
                    }
                }
                nextState = STATE_BURSTING;
                if (task->spawnArg1.value == 2) {
                    nextState = STATE_BURST_THEN_FADE;
                }
                task->state = nextState;
                return;
            }
            if (effect->angle < PE2_OPTION(SHRINK_END_RADIUS)) {
                worldCollisionUnlinkBody(&collision->damageBody);
                effectKillTask(effect, task);
                return;
            }
            worldCollisionClearContacts(collision->contacts);
            return;
        case STATE_BURSTING:
            if ((gAttachmentState.effectPhase == ATTACHMENT_EFFECT_HELD) || ((peEffectControl = gRoomEffectState->peEffectControl), peEffectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN)) {
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
            effect->angle = effect->angle + PE2_OPTION(RADIUS_STEP);
            if (effect->angle > (effect->index * PE2_OPTION(LEVEL_RADIUS) + PE2_OPTION(BURST_RADIUS))) {
                worldCollisionUnlinkBody(&collision->gridBody);
                effectKillTask(effect, task);
                return;
            }
            return;
        case STATE_BURST_THEN_FADE:
            // Radius keeps expanding after its limit while this variant fades intensity.
            if ((gAttachmentState.effectPhase == ATTACHMENT_EFFECT_HELD) || ((peEffectControl = gRoomEffectState->peEffectControl), peEffectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN)) {
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
            effect->angle = effect->angle + PE2_OPTION(RADIUS_STEP);
            if (effect->angle > (effect->index * PE2_OPTION(LEVEL_RADIUS) + PE2_OPTION(BURST_RADIUS))) {
                if (effect->scale >= (PE2_OPTION(FADE_STEP) + 1)) {
                    effect->scale = effect->scale - PE2_OPTION(FADE_STEP);
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
        VOLLEY_FIRST   = 0,
        VOLLEY_SECOND  = 1,
        VOLLEY_THIRD   = 2,
        VOLLEY_RELEASE = 3,
    };
    EffectWork* effect;
    GfxCoord*   coord;
    s16         battleState;
    s16         peEffectControl;

    effect = task->spawnArg2.pointer;
    coord  = task->extra.coordBody->coord;
    if (gAttachmentState.effectPhase != ATTACHMENT_EFFECT_HELD) {
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
                    case VOLLEY_FIRST:
                        effectSpawn((EFFECT_PYROKINESIS_CAST | EFFECT_SPAWN_UNLIMITED), coord, 0, 0);
                        task->state = VOLLEY_SECOND;
                        return;
                    case VOLLEY_SECOND:
                        if (effect->age == PE2_OPTION(VOLLEY_2_FRAME)) {
                            effectSpawn((EFFECT_PYROKINESIS_CAST | EFFECT_SPAWN_UNLIMITED), coord, 1, 0);
                            task->state = VOLLEY_THIRD;
                        }
                        return;
                    case VOLLEY_THIRD:
                        if (effect->age == PE2_OPTION(VOLLEY_3_FRAME)) {
                            effectSpawn(EFFECT_PYROKINESIS_CAST | EFFECT_SPAWN_UNLIMITED, coord, 2, 0);
                            task->state = VOLLEY_RELEASE;
                        }
                        return;
                    case VOLLEY_RELEASE:
                        break;
                    default:
                        return;
                }
            }
        }
    }
    effectKillTask(effect, task);
}

#include <effects/glow/flame.inc.c>

/// Projects the ground glow's four corners, leaving the last one's depth to read.
static inline void _projectGroundGlow(EffectQuadScratch* quadScratch)
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

/// Draws the additive glow on the ground under the flame, a square `halfSize` out alternating two cells; a corner
/// failing to project drops it.
static void _drawGroundGlow(const GfxCoord* groundCoord, s32 halfSize)
{
    enum {
        GROUND_GLOW_TEXTURE_DEPTH_4BIT = 0,
        GROUND_GLOW_CELL_SIZE          = 32,
        GROUND_GLOW_LEFT_U             = 192,
        GROUND_GLOW_TOP_V              = 56,
        GROUND_GLOW_UV_SPAN            = GROUND_GLOW_CELL_SIZE - 1,
    };

    EffectQuadScratch* quadScratch;
    s32                cornerIndex;
    POLY_FT4*          quad;
    s32                textureU;

    quadScratch = SCRATCH_STACK_RESERVE_BLOCK(EffectQuadScratch);
    gte_SetTransMatrix(&GsWSMATRIX);
    // Rotate the ground-plane offsets, then centre them on the composed hit.
    for (cornerIndex = 0; cornerIndex < ARRAY_SIZE(gEffectUnitQuadCorners); cornerIndex++) {
        quadScratch->vertices[cornerIndex].vx = (u16)((EffectUnitQuadCorner*)gEffectUnitQuadCorners)[cornerIndex].axis0Sign * halfSize;
        quadScratch->vertices[cornerIndex].vy = 0;
        quadScratch->vertices[cornerIndex].vz = (u16)((EffectUnitQuadCorner*)gEffectUnitQuadCorners)[cornerIndex].axis1Sign * halfSize;
        gte_SetRotMatrix(&gGfxViewCoord.workm);
        gte_ldv0(&quadScratch->vertices[cornerIndex]);
        gte_rtv0();
        gte_stsv(&quadScratch->vertices[cornerIndex]);
        quadScratch->vertices[cornerIndex].vx += groundCoord->workm.t[0];
        quadScratch->vertices[cornerIndex].vy += groundCoord->workm.t[1];
        quadScratch->vertices[cornerIndex].vz += groundCoord->workm.t[2];
    }

    _projectGroundGlow(quadScratch);
    if (quadScratch->projectionFlags >= 0) {
        gte_stszotz(&quadScratch->depth);
        quadScratch->depth++;
        quad           = gGpuPrimCursor;
        gGpuPrimCursor = quad + 1;
        setPolyFT4(quad);
        setSemiTrans(quad, 1);
        setRGB0(quad, COLOR_RED(PE2_OPTION(GROUND_GLOW_TINT)), COLOR_GREEN(PE2_OPTION(GROUND_GLOW_TINT)), COLOR_BLUE(PE2_OPTION(GROUND_GLOW_TINT)));
        quad->tpage = getTPage(GROUND_GLOW_TEXTURE_DEPTH_4BIT, GPU_BLEND_ADD, 512, 0);
        quad->clut  = getClut(192, 266);
        textureU    = (gDisplayState.animFrame & 1) * GROUND_GLOW_CELL_SIZE + GROUND_GLOW_LEFT_U;
        quad->v0    = GROUND_GLOW_TOP_V;
        quad->u0    = textureU;
        textureU    = (gDisplayState.animFrame & 1) * GROUND_GLOW_CELL_SIZE + GROUND_GLOW_LEFT_U + GROUND_GLOW_UV_SPAN;
        quad->v1    = GROUND_GLOW_TOP_V;
        quad->u1    = textureU;
        textureU    = (gDisplayState.animFrame & 1) * GROUND_GLOW_CELL_SIZE + GROUND_GLOW_LEFT_U;
        quad->v2    = GROUND_GLOW_TOP_V + GROUND_GLOW_UV_SPAN;
        quad->u2    = textureU;
        textureU    = (gDisplayState.animFrame & 1) * GROUND_GLOW_CELL_SIZE + GROUND_GLOW_LEFT_U + GROUND_GLOW_UV_SPAN;
        quad->v3    = GROUND_GLOW_TOP_V + GROUND_GLOW_UV_SPAN;
        quad->u3    = textureU;
        quad->x0    = quadScratch->screenCorners[0].vx;
        quad->y0    = quadScratch->screenCorners[0].vy;
        quad->x1    = quadScratch->screenCorners[1].vx;
        quad->y1    = quadScratch->screenCorners[1].vy;
        quad->x2    = quadScratch->screenCorners[2].vx;
        quad->y2    = quadScratch->screenCorners[2].vy;
        quad->x3    = quadScratch->screenCorners[3].vx;
        quad->y3    = quadScratch->screenCorners[3].vy;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(quadScratch->depth),
                quad);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectQuadScratch);
}

/// The flame's sprite: two 56-texel cells alternating by frame.
#define SPRITE_QUAD_TEXTURE_PAGE getTPage(0, GPU_BLEND_ADD, 576, 0)
#define SPRITE_QUAD_CLUT         getClut(192, 266)
#define SPRITE_QUAD_CELL_WIDTH   56
#define SPRITE_QUAD_COLUMN_MASK  1
#define SPRITE_QUAD_U_BASE       0x70
#define SPRITE_QUAD_TOP_V        0xC8
#define SPRITE_QUAD_BOTTOM_V     (SPRITE_QUAD_TOP_V + SPRITE_QUAD_CELL_WIDTH - 1)
#define SPRITE_QUAD_SCALE        (SPRITE_QUAD_CELL_WIDTH - 1)
#include <effects/sprite_quad/draw.inc.c>

void pyrokinesisFlamePuffTask(Task* task)
{
    enum {
        FLAME_PUFF_INITIALIZE,
        FLAME_PUFF_ACTIVE,
    };

    EffectWork* work;
    GfxCoord*   coord;
    s16         peEffectControl;
    s16         textureFrame;
    s32         nextY;

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    if (gAttachmentState.effectPhase != ATTACHMENT_EFFECT_HELD) {
        peEffectControl = gRoomEffectState->peEffectControl;
        if (peEffectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            if (peEffectControl != ROOM_EFFECT_CONTROL_RUNNING) {
                return;
            }
            work->age++;
            if (task->state == FLAME_PUFF_INITIALIZE) {
                // Choose a fixed rise velocity and screen rotation once per puff.
                RANDOM_LCG_ADVANCE();
                work->move.vy = -(RANDOM_LCG_DRAW() & (PE2_OPTION(PUFF_RISE_JITTER) - 1));
                RANDOM_LCG_ADVANCE();
                work->scale = RANDOM_LCG_DRAW() & (PYRO_FLAME_FULL_TURN - 1);
                task->state = FLAME_PUFF_ACTIVE;
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
                    _pyroFlameDrawSprite(coord, textureFrame, PE2_OPTION(PUFF_SIZE), work->scale);
                }
                return;
            }
        }
    }
    effectKillTask(work, task);
}

#include <pe/effects/pyro_flame/draw_sprite.inc.c>

void pyrokinesisFlameRingTask(Task* task)
{
    enum {
        FLAME_RING_INITIALIZE,
        FLAME_RING_ACTIVE,
    };

    EffectWork* work;
    GfxCoord*   coord;
    s16         peEffectControl;

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    if (gAttachmentState.effectPhase != ATTACHMENT_EFFECT_HELD) {
        peEffectControl = gRoomEffectState->peEffectControl;
        if (peEffectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            if (peEffectControl != ROOM_EFFECT_CONTROL_RUNNING) {
                return;
            }
            if (task->state == FLAME_RING_INITIALIZE) {
                // Tilt the local XZ ring by the spawn angle once; expansion keeps that plane.
                gfxRotMatrixZ(&coord->coord, task->spawnArg1.value, GRAPHICS_ROTATION_COMPOSE);
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
                work->scale         = PE2_OPTION(RING_BRIGHTNESS);
                work->angle         = PE2_OPTION(RING_RADIUS);
                task->state         = FLAME_RING_ACTIVE;
            }
            actorRenderComposeCoord(coord);
            glowDrawFlameRing(coord, work->angle, PE2_OPTION(RING_WIDTH), work->scale);
            work->angle += PE2_OPTION(RING_GROWTH);
            work->scale -= PE2_OPTION(RING_FADE_STEP);
            if (work->scale >= (PE2_OPTION(RING_FADE_STEP) + 1)) {
                return;
            }
        }
    }
    effectKillTask(work, task);
}

#include <effects/glow/flame_ring.inc.c>

/// The jet cones: their palette, rims and the segments' animation offsets.
#define JET_CONE_CLUT                     getClut(32, 266)
#define JET_CONE_EXTENDED_TAIL_RIM_RADIUS PE2_OPTION(JET_TAIL_RIM_RADIUS)
#define JET_CONE_BASE_TAIL_RIM_RADIUS     PE2_OPTION(JET_BASE_RIM_RADIUS)
#define JET_CONE_TEXTURE_FRAME_OFFSETS    _gConeFrameOffsets
#include <effects/jet_cone/draw.inc.c>

void pyrokinesisLaunchConeTask(Task* task)
{
    enum {
        LAUNCH_CONE_INITIALIZE,
        LAUNCH_CONE_ACTIVE,
    };

    EffectWork* work;
    GfxCoord*   coord;
    s16         peEffectControl;

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    if (gAttachmentState.effectPhase != ATTACHMENT_EFFECT_HELD) {
        peEffectControl = gRoomEffectState->peEffectControl;
        if (peEffectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            if (peEffectControl != ROOM_EFFECT_CONTROL_RUNNING) {
                return;
            }
            work->age++;
            if (task->state == LAUNCH_CONE_INITIALIZE) {
                work->scale = PE2_OPTION(CONE_BRIGHTNESS);
                work->angle = PE2_OPTION(CONE_RADIUS);
                task->state = LAUNCH_CONE_ACTIVE;
            }
            actorRenderComposeCoord(coord);
            // Draw the current ring pair before advancing its radius and intensity.
            glowDrawFlameCone(task->extra.coordBody->coord, work->angle, work->scale);
            work->angle += PE2_OPTION(CONE_GROWTH);
            work->scale -= PE2_OPTION(CONE_FADE_STEP);
            if (work->scale >= PE2_OPTION(CONE_FADE_STEP)) {
                return;
            }
        }
    }
    effectKillTask(work, task);
}
