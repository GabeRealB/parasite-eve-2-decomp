#include "gunblade.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include <core/pe2.h>

#include <gameplay/display.h>
#include <gameplay/actor_render.h>
#include <gameplay/effects.h>
#include <gameplay/hud_sprites.h>
#include <gameplay/room_effects.h>
#include <gameplay/scene.h>
#include <gameplay/items.h>
#include <gameplay/player_actor.h>
#include <gameplay/scene_runtime.h>
#include <gameplay/world_collision.h>
#include <gameplay/world_coords.h>
#include <gameplay/player_animation.h>

#include <engine/coord.h>
#include <engine/display_types.h>
#include <engine/random.h>
#include <engine/gfx.h>
#include <engine/scratch.h>
#include <engine/task.h>
#include <engine/task_types.h>
#include <engine/mc_types.h>
#include <engine/session_types.h>
#include <engine/wipsys.h>
#include <engine/wipsys_types.h>
#include <engine/sound_ids.h>

#include <weapons/weapon/sounds.h>
#include <weapons/weapon/weapon.h>
#include <weapons/effects/blade_trail/blade_trail.h>
#include <weapons/attack/attack.h>
#include <weapons/weapon/weapon_index.h>

enum {
    TRAIL_ENDPOINT_NEAR = 0,
    TRAIL_ENDPOINT_FAR  = 1,
};

/// Near and far ribbon endpoints in integer coordinates of the Gunblade muzzle frame.
///
/// The task coordinate stays at endpoint 0; endpoint 1 is sampled every update.
static SVECTOR _gTrailEndpointOffsets[] = {
    [TRAIL_ENDPOINT_NEAR] = { 0, 0x0060, 0x0080, 0 },
    [TRAIL_ENDPOINT_FAR]  = { 0, 0x0060, 0x0380, 0 },
};

/// The model and the player's clips: here, as the package stores them after the data above.
PE2_VALUE(model)
PE2_VALUE(bank)

/// Borrowed trail task receiving the ammunition selector for a deferred charge flash.
///
/// Published when the histories are seeded. Explicit retirement leaves this
/// handle unchanged; requests dereference it only behind `_gTrailWork`.
/// Parent teardown does not invalidate either cached handle.
static Task* _gTrailTask = NULL;

/// Borrowed trail work whose index counts deferred charge-flash requests.
///
/// Published with `_gTrailTask` when the histories are seeded and cleared
/// before normal or cancellation retirement. Parent teardown releases the work
/// through the generic effect exit without clearing this cache; a non-NULL
/// value alone does not establish liveness after that teardown.
static EffectWork* _gTrailWork = NULL;

/// Eight zero bytes of `.data` that no code reads or writes, in the US, European or Japanese release (the Japanese
/// code skips the same eight bytes between its trail task and first ring). Not alignment: the rings need only 4.
/// An unused static the original source defined here, which GCC 2.8.1 still emits; its type is unknown.
static s32 s_unused_8012E24C[2] = { 0, 0 };

/// Eight retained world-coordinate snapshots of the Gunblade ribbon's near endpoint.
///
/// Seeded at local (0, 96, 128), then overwritten at age modulo eight.
/// Each local matrix retains the endpoint under the persistent view parent;
/// recomposition refreshes its view-space cache before drawing seven segments.
/// The ribbon reads only the cache's translation, in game-coordinate units.
/// Shared by all trail tasks in this loaded Gunblade overlay; reseeded on entry.
static GfxCoord _gBladeTrailFirstHistory[8] = { 0 };

/// Eight retained world-coordinate snapshots of the Gunblade ribbon's far endpoint.
///
/// Local (0, 96, 896), paired slot-for-slot with the near-end history.
/// Retained under the view parent and recomposed for drawing; only translation
/// is meaningful, since the temporary endpoint's rotation is not initialized.
/// All trail tasks share this overlay-owned ring and reseed it on entry.
static GfxCoord _gBladeTrailSecondHistory[8] = { 0 };

void gunbladeTrailTask(Task* task)
{
    enum {
        TRAIL_SEED           = 0,
        TRAIL_RECORD         = 1,
        TRAIL_CHARGE_PENDING = 1,
    };
    GfxCoord       farEndpoint;
    GfxCoord*      nearCoord;
    GfxCoord*      historyFrame;
    EffectWork*    effectWork;
    EffectWork*    flashWork;
    s32            keepTask;
    const SVECTOR* farOffset;
    s32            historyIndex;

    effectWork = task->spawnArg2.pointer;
    nearCoord  = task->extra.coordBody->coord;
    if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        keepTask = gRoomEffectState->effectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN;
    } else {
        effectWork->age++;
        switch (task->state) {
            case TRAIL_SEED:
                // Seed both histories with the current endpoint poses before drawing any ribbon.
                nearCoord->parent       = effectWork->parent;
                nearCoord->coord.t[0]   = _gTrailEndpointOffsets[TRAIL_ENDPOINT_NEAR].vx;
                _gTrailTask             = task;
                nearCoord->coord.t[1]   = _gTrailEndpointOffsets[TRAIL_ENDPOINT_NEAR].vy;
                _gTrailWork             = effectWork;
                nearCoord->coord.t[2]   = _gTrailEndpointOffsets[TRAIL_ENDPOINT_NEAR].vz;
                nearCoord->composeStamp = GRAPHICS_COORD_DIRTY;
                actorRenderComposeCoord(nearCoord);
                task->state              = TRAIL_RECORD;
                farOffset                = &_gTrailEndpointOffsets[TRAIL_ENDPOINT_FAR];
                farEndpoint.parent       = effectWork->parent;
                farEndpoint.coord.t[0]   = farOffset->vx;
                farEndpoint.coord.t[1]   = farOffset->vy;
                farEndpoint.coord.t[2]   = farOffset->vz;
                farEndpoint.composeStamp = GRAPHICS_COORD_DIRTY;
                actorRenderComposeCoord(&farEndpoint);
                for (historyIndex = 0; historyIndex < ARRAY_SIZE(_gBladeTrailFirstHistory); historyIndex++) {
                    historyFrame = &_gBladeTrailFirstHistory[historyIndex];
                    _bladeTrailStoreFrame(historyFrame, nearCoord);
                    historyFrame = &_gBladeTrailSecondHistory[historyIndex];
                    _bladeTrailStoreFrame(historyFrame, &farEndpoint);
                }
                return;
            case TRAIL_RECORD: {
                const SVECTOR* recordOffset;

                // Record world poses so old frames follow the camera, independently of the weapon.
                nearCoord->composeStamp = GRAPHICS_COORD_DIRTY;
                actorRenderComposeCoord(nearCoord);
                recordOffset             = &_gTrailEndpointOffsets[TRAIL_ENDPOINT_FAR];
                farEndpoint.parent       = effectWork->parent;
                farEndpoint.coord.t[0]   = recordOffset->vx;
                farEndpoint.coord.t[1]   = recordOffset->vy;
                farEndpoint.coord.t[2]   = recordOffset->vz;
                farEndpoint.composeStamp = GRAPHICS_COORD_DIRTY;
                actorRenderComposeCoord(&farEndpoint);
                historyFrame = &_gBladeTrailFirstHistory[effectWork->age & (ARRAY_SIZE(_gBladeTrailFirstHistory) - 1)];
                _bladeTrailStoreFrame(historyFrame, nearCoord);
                historyFrame = &_gBladeTrailSecondHistory[effectWork->age & (ARRAY_SIZE(_gBladeTrailFirstHistory) - 1)];
                _bladeTrailStoreFrame(historyFrame, &farEndpoint);
                for (historyIndex = 0; historyIndex < ARRAY_SIZE(_gBladeTrailFirstHistory); historyIndex++) {
                    historyFrame               = &_gBladeTrailFirstHistory[historyIndex];
                    historyFrame->composeStamp = GRAPHICS_COORD_DIRTY;
                    actorRenderComposeCoord(historyFrame);
                    historyFrame               = &_gBladeTrailSecondHistory[historyIndex];
                    historyFrame->composeStamp = GRAPHICS_COORD_DIRTY;
                    actorRenderComposeCoord(historyFrame);
                }
                if (effectWork->age < PE2_OPTION(TRAIL_CHARGE_START_FRAMES)) {
                    _bladeTrailDraw(effectWork->age & (ARRAY_SIZE(_gBladeTrailFirstHistory) - 1), BLADE_TRAIL_TINT_BLUE_WHITE);
                    return;
                }
                // Post a deferred charge once the ribbon changes tint; repeated requests remain a counter.
                if (effectWork->index == TRAIL_CHARGE_PENDING) {
                    effectWork->index++;
                    flashWork = effectSpawn(EFFECT_GUNBLADE_CHARGE_FLASH, nearCoord, task->spawnArg1.value, NULL);
                    if (flashWork != NULL) {
                        taskReparent(task, flashWork->task);
                    }
                }
                _bladeTrailDraw(effectWork->age & (ARRAY_SIZE(_gBladeTrailFirstHistory) - 1), BLADE_TRAIL_TINT_YELLOW_WHITE);
                keepTask = effectWork->age < PE2_OPTION(TRAIL_LIFETIME_FRAMES);
                break;
            }
            default:
                return;
        }
    }
    if (!keepTask) {
        _gTrailWork = NULL;
        effectKillTask(effectWork, task);
    }
}

#include <weapons/effects/blade_trail/draw.inc.c>

PE2_EDITOR_LABEL("Attack")
/// Attack tuning, in dispatch ticks.
PE2_OPTION_DEF(READY_BLEND_FRAMES, int, 1)
PE2_OPTION_DEF(MOVING_READY_BLEND_FRAMES, int, 6)
PE2_OPTION_DEF(SLASH_WINDUP_FRAMES, int, 0x12)
PE2_OPTION_DEF(SPIN_COUNTER_FRAMES, int, 0x39)
PE2_OPTION_DEF(CHARGE_WINDOW_FRAMES, int, 8)
PE2_OPTION_DEF(SPIN_YAW_STEP, int, 0xC)
PE2_OPTION_DEF(SLASH_RADIUS, int, 0x180)
PE2_OPTION_DEF(SHOT_REACH, int, 0x2200)
PE2_OPTION_DEF(SHOT_RADIUS, int, 0x100)
PE2_OPTION_DEF(BUCKSHOT_RADIUS, int, 0x900)
/// Divisor that turns the model's forward axis into the distance the spinning
/// slash covers per frame: 4096 / 136, about 30 coordinate units.
PE2_OPTION_DEF(SPIN_ADVANCE_DIVISOR, int, 136)
/// The slash's ribbon trail (`gunbladeTrailTask`); times in dispatch ticks.
PE2_OPTION_DEF(TRAIL_CHARGE_START_FRAMES, int, 9)
PE2_OPTION_DEF(TRAIL_LIFETIME_FRAMES, int, 13)
/// The charged slash's flash (`gunbladeChargeFlashTask`): brightness 0..255, radii in game units, angles in 4096 per turn.
PE2_OPTION_DEF(FLASH_INITIAL_BRIGHTNESS, int, 0xE0)
PE2_OPTION_DEF(FLASH_INITIAL_HALF_RADIUS, int, 0x80)
PE2_OPTION_DEF(FLASH_DISC_RADIUS_STEP, int, 0x10)
PE2_OPTION_DEF(FLASH_BAND_RADIUS_STEP, int, 0x40)
PE2_OPTION_DEF(FLASH_BAND_BRIGHTNESS_STEP, int, 0x10)
PE2_OPTION_DEF(FLASH_DISC_BRIGHTNESS_STEP, int, 0x20)
PE2_OPTION_DEF(FLASH_BAND_BRIGHTNESS_MIN, int, 0x11)
PE2_OPTION_DEF(FLASH_DISC_BRIGHTNESS_MIN, int, 0x20)
PE2_OPTION_DEF(FLASH_BAND_WIDTH, int, 0x60)
PE2_OPTION_DEF(FLASH_IMPACT_RADIUS, int, 0x600)
PE2_OPTION_DEF(FLASH_BAND_SECOND_ANGLE, int, 0x2AA)
PE2_OPTION_DEF(FLASH_BAND_THIRD_ANGLE, int, 0x555)
PE2_OPTION_DEF(FLASH_SPARK_COUNT, int, 8)
PE2_OPTION_DEF(FLASH_BOUNCING_SPARK_COUNT, int, 4)
PE2_OPTION_DEF(FLASH_BOUNCING_SPARK_COLOR, int, 0x100)

PE2_EDITOR_LABEL("Model")
/// The model held in the player's weapon hand.
PE2_BIND(model, model)
PE2_BIND_ARG(model, _symbol, gGunbladeModel)

PE2_EDITOR_LABEL("Animation")
/// The player's clips while it is equipped, by slot.
PE2_BIND(bank, animation_bank)
PE2_BIND_ARG(bank, _symbol, gGunbladePlayerAnimationBank)

/// A sound of the Gunblade's bank, by slot (`WEAPON_SOUND_SLOT_*`, or one below).
#define SOUND(slot) SOUND_WEAPON(WEAPON_INDEX_GUNBLADE, (slot))

/// The Gunblade's own slots.
enum {
    SOUND_FIREFLY_IMPACT = 4,
    SOUND_SHOT           = 5,
    SOUND_SLASH          = 6,
    SOUND_CONTACT        = 7,
    SOUND_CHARGED_SLASH  = 8, // The slash fired with a round (the slash's contact and the shot)
};

/// The contacts' keys.
enum {
    SLASH_KEY                     = 0x2171B,
    ATTACK_KEY_BASE               = 0x21700,
    CHARGED_SLUG_KEY_LOW          = 0x20,
    CHARGED_AMMUNITION_KEY_OFFSET = 0xB,
};

/// The charged slash's flash (`gunbladeChargeFlashTask`): what its effects are told - the color band's kind, high half
/// - and the bouncing sparks' speed bits.
enum {
    FLASH_BOUNCING_SPARK_SPEED_MASK = 0x3F,
    FLASH_BAND_BUCKSHOT_ARGUMENT    = 0x10000,
    FLASH_BAND_FIREFLY_ARGUMENT     = 0x20000,
    FLASH_BAND_SLUG_ARGUMENT        = 0x30000,
    FLASH_SLUG_SPARK_ARGUMENT       = 1,
};

enum {
    FLASH_INIT = 0,
    FLASH_DRAW = 1,
};

/// Draws the charge flash's additive bands and screen tint, then steps its fade.
///
/// `effectWork` is live writable EffectWork; `coord` borrows a composed
/// view-space GfxCoord and `rgb` three readable tint bytes. The first band has
/// inner radius 3 * step / 2 and width 96 game units; odd ages draw a second
/// band with those dimensions swapped. Radius narrows to s16 before drawing.
/// After all draws, period loses 16 brightness units and step gains 64 game
/// units. The caller prepares RGB and checks period >= 17 before this phase.
/// Requires the drawers' scratch/GTE state and unchecked frame-arena space;
/// input storage must stay clear of their workspace. Retains no input pointer.
/// Pointer expressions repeat and must have no side effects. Captures no caller
/// locals. Expands to a block with no control transfer; use inside braces.
///
/// A macro, not an inline function: as one, `rgb`'s stack address is kept in a
/// saved register across the three calls instead of being formed at each, and
/// the task no longer matches.
#define DRAW_CHARGE_BANDS(effectWork, coord, rgb)                                                                 \
    {                                                                                                             \
        effectDrawOuterGlowBand(coord, (s16)((effectWork)->step * 3 / 2), PE2_OPTION(FLASH_BAND_WIDTH), rgb);     \
        if ((effectWork)->age & 1) {                                                                              \
            effectDrawOuterGlowBand(coord, PE2_OPTION(FLASH_BAND_WIDTH), (s16)((effectWork)->step * 3 / 2), rgb); \
        }                                                                                                         \
        effectDrawScreenTint(rgb, GPU_BLEND_ADD);                                                                 \
        (effectWork)->period -= PE2_OPTION(FLASH_BAND_BRIGHTNESS_STEP);                                           \
        (effectWork)->step   += PE2_OPTION(FLASH_BAND_RADIUS_STEP);                                               \
    }

void gunbladeChargeFlashTask(Task* task)
{
    EffectWork* effectWork;
    GfxCoord*   coord;
    u8          rgb[3];
    s32         sparkIndex;

    coord      = task->extra.coordBody->coord;
    effectWork = task->spawnArg2.pointer;

    if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        if (gRoomEffectState->effectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            effectKillTask(effectWork, task);
        }
        return;
    }

    actorRenderComposeCoord(coord);
    effectWork->age++;

    // Ammunition changes the RGB channel ratios and the initial spark recipe.
    switch (task->spawnArg1.value) {
        case SHOTGUN_ROUND_BUCKSHOT:
            if (task->state == FLASH_INIT) {
                effectSpawn(EFFECT_IMPACT_FLASH, coord, PE2_OPTION(FLASH_IMPACT_RADIUS), NULL);
                effectSpawn(EFFECT_EXPANDING_COLOR_BAND, coord, FLASH_BAND_BUCKSHOT_ARGUMENT, NULL);
                effectSpawn(EFFECT_EXPANDING_COLOR_BAND, coord, (FLASH_BAND_BUCKSHOT_ARGUMENT | PE2_OPTION(FLASH_BAND_SECOND_ANGLE)), NULL);
                effectSpawn(EFFECT_EXPANDING_COLOR_BAND, coord, (FLASH_BAND_BUCKSHOT_ARGUMENT | PE2_OPTION(FLASH_BAND_THIRD_ANGLE)), NULL);
                for (sparkIndex = 0; sparkIndex < PE2_OPTION(FLASH_SPARK_COUNT); sparkIndex++) {
                    effectSpawn(EFFECT_SPARK_STREAK, coord, 0, NULL);
                }
                task->state       = FLASH_DRAW;
                effectWork->scale = effectWork->period = PE2_OPTION(FLASH_INITIAL_BRIGHTNESS);
                effectWork->angle = effectWork->step = PE2_OPTION(FLASH_INITIAL_HALF_RADIUS);
            }
            rgb[0] = rgb[1]    = effectWork->scale;
            rgb[2]             = effectWork->scale >> 2;
            effectWork->angle += PE2_OPTION(FLASH_DISC_RADIUS_STEP);
            effectDrawGouraudDisc(coord, (s16)(effectWork->angle * 2), rgb);
            if (effectWork->period >= PE2_OPTION(FLASH_BAND_BRIGHTNESS_MIN)) {
                rgb[0] = rgb[1] = effectWork->period;
                rgb[2]          = effectWork->period >> 2;
                DRAW_CHARGE_BANDS(effectWork, coord, rgb);
                return;
            }
            effectWork->scale -= PE2_OPTION(FLASH_DISC_BRIGHTNESS_STEP);
            if (effectWork->scale < PE2_OPTION(FLASH_DISC_BRIGHTNESS_MIN)) {
                effectKillTask(effectWork, task);
            }
            return;
        case SHOTGUN_ROUND_FIREFLY:
            if (task->state == FLASH_INIT) {
                effectSpawn(EFFECT_IMPACT_FLASH, coord, PE2_OPTION(FLASH_IMPACT_RADIUS), NULL);
                effectSpawn(EFFECT_EXPANDING_COLOR_BAND, coord, FLASH_BAND_FIREFLY_ARGUMENT, NULL);
                effectSpawn(EFFECT_EXPANDING_COLOR_BAND, coord, (FLASH_BAND_FIREFLY_ARGUMENT | PE2_OPTION(FLASH_BAND_SECOND_ANGLE)), NULL);
                effectSpawn(EFFECT_EXPANDING_COLOR_BAND, coord, (FLASH_BAND_FIREFLY_ARGUMENT | PE2_OPTION(FLASH_BAND_THIRD_ANGLE)), NULL);
                for (sparkIndex = 0; sparkIndex < PE2_OPTION(FLASH_BOUNCING_SPARK_COUNT); sparkIndex++) {
                    RANDOM_LCG_ADVANCE();
                    effectSpawn(EFFECT_BOUNCING_SPARK, coord, (RANDOM_LCG_DRAW() & FLASH_BOUNCING_SPARK_SPEED_MASK) | PE2_OPTION(FLASH_BOUNCING_SPARK_COLOR), NULL);
                }
                task->state       = FLASH_DRAW;
                effectWork->scale = effectWork->period = PE2_OPTION(FLASH_INITIAL_BRIGHTNESS);
                effectWork->angle = effectWork->step = PE2_OPTION(FLASH_INITIAL_HALF_RADIUS);
            }
            rgb[0]             = effectWork->scale;
            rgb[1]             = effectWork->scale >> 1;
            rgb[2]             = effectWork->scale >> 2;
            effectWork->angle += PE2_OPTION(FLASH_DISC_RADIUS_STEP);
            effectDrawGouraudDisc(coord, (s16)(effectWork->angle * 2), rgb);
            if (effectWork->period >= PE2_OPTION(FLASH_BAND_BRIGHTNESS_MIN)) {
                rgb[0] = effectWork->period;
                rgb[1] = effectWork->period >> 1;
                rgb[2] = effectWork->period >> 2;
                DRAW_CHARGE_BANDS(effectWork, coord, rgb);
                return;
            }
            effectWork->scale -= PE2_OPTION(FLASH_DISC_BRIGHTNESS_STEP);
            if (effectWork->scale < PE2_OPTION(FLASH_DISC_BRIGHTNESS_MIN)) {
                effectKillTask(effectWork, task);
            }
            return;
        case SHOTGUN_ROUND_RIFLED_SLUG:
            if (task->state == FLASH_INIT) {
                effectSpawn(EFFECT_IMPACT_FLASH, coord, PE2_OPTION(FLASH_IMPACT_RADIUS), NULL);
                effectSpawn(EFFECT_EXPANDING_COLOR_BAND, coord, FLASH_BAND_SLUG_ARGUMENT, NULL);
                effectSpawn(EFFECT_EXPANDING_COLOR_BAND, coord, (FLASH_BAND_SLUG_ARGUMENT | PE2_OPTION(FLASH_BAND_SECOND_ANGLE)), NULL);
                effectSpawn(EFFECT_EXPANDING_COLOR_BAND, coord, (FLASH_BAND_SLUG_ARGUMENT | PE2_OPTION(FLASH_BAND_THIRD_ANGLE)), NULL);
                for (sparkIndex = 0; sparkIndex < PE2_OPTION(FLASH_SPARK_COUNT); sparkIndex++) {
                    effectSpawn(EFFECT_SPARK_STREAK, coord, FLASH_SLUG_SPARK_ARGUMENT, NULL);
                }
                task->state       = FLASH_DRAW;
                effectWork->scale = effectWork->period = PE2_OPTION(FLASH_INITIAL_BRIGHTNESS);
                effectWork->angle = effectWork->step = PE2_OPTION(FLASH_INITIAL_HALF_RADIUS);
            }
            rgb[0]             = effectWork->scale >> 2;
            rgb[1]             = effectWork->scale >> 1;
            rgb[2]             = effectWork->scale;
            effectWork->angle += PE2_OPTION(FLASH_DISC_RADIUS_STEP);
            effectDrawGouraudDisc(coord, (s16)(effectWork->angle * 2), rgb);
            if (effectWork->period >= PE2_OPTION(FLASH_BAND_BRIGHTNESS_MIN)) {
                rgb[0] = effectWork->period >> 2;
                rgb[1] = effectWork->period >> 1;
                rgb[2] = effectWork->period;
                DRAW_CHARGE_BANDS(effectWork, coord, rgb);
                return;
            }
            effectWork->scale -= PE2_OPTION(FLASH_DISC_BRIGHTNESS_STEP);
            if (effectWork->scale < PE2_OPTION(FLASH_DISC_BRIGHTNESS_MIN)) {
                effectKillTask(effectWork, task);
            }
            return;
    }
}

#undef DRAW_CHARGE_BANDS

/// Requests a deferred charge flash on the active gunblade trail.
///
/// ammunitionIndex is PlayerStatus::weaponSlotItem, consumed unchanged by the
/// flash effect (13..15 select its three drawn grades). With no live trail this
/// is a no-op. Otherwise the trail's pending counter is incremented; its update
/// spawns the flash only when that counter equals one starting at age nine.
/// Keep the cached trail task/work live and the gunblade overlay loaded.
static void gunbladeRequestChargeFlash(s32 ammunitionIndex)
{
    EffectWork* trailWork = _gTrailWork;

    if (trailWork != NULL) {
        _gTrailTask->spawnArg1.value = ammunitionIndex;
        trailWork->index++;
    }
}

/// One-based weapon index passed to the resident use counter.

/// Scratch-stack block for the gunblade's attack handler.
///
/// The handler reserves one block each frame and releases it before
/// returning; the block is not cleared. It stages two unrelated things: the
/// node a fired round's impact sound is placed at, and the forward step the
/// spinning slash moves the actor by.
///
/// Only the translation of `impactCoord`'s composed transform is written. A
/// sound's pan and depth come from projecting the node's origin, which reads
/// nothing else, so the rest of the node is whatever the scratch stack last
/// held. The position is a weapon contact point, offset by up to 7 units per
/// axis when the impact picker reports it, in the space the weapon node's
/// composed transform is expressed in.
///
/// `forward` is read from the root coordinate's local matrix and is not
/// normalized. `advance` is each component of it divided by
/// `PE2_OPTION(SPIN_ADVANCE_DIVISOR)` and multiplied by the frame's
/// `GameActor.movementSign`: 1 while the spin runs, 0 otherwise. It is added
/// to the root coordinate's local translation in every state. Neither
/// vector's `pad` is written.
typedef struct {
    GfxCoord impactCoord; // Node standing at the round's impact point: the source of the impact sound
    VECTOR   advance;     // This frame's displacement of the model's root coordinate, in coordinate units; zero outside the spin
    SVECTOR  forward;     // Model's forward axis: the Z column of its root coordinate's local matrix, 4096 per unit
} _AttackScratch;
STATIC_ASSERT_SIZEOF(_AttackScratch, 0x68);

void gunbladeAttackState(Task* playerTask)
{
    enum {
        PHASE_PREPARE             = 0,
        PHASE_WAIT_READY          = 1,
        PHASE_SELECT_ATTACK       = 2,
        PHASE_SWING_WINDUP        = 3,
        PHASE_CHARGE_WINDOW       = 4,
        PHASE_SWING_FOLLOWTHROUGH = 5,
        PHASE_SHOT_IMPACT         = 6,
        PHASE_RECOVER             = 7,
        CONTACT_SOUND_POSTED      = 1,
    };
    GameActor*             actor;
    GfxCoord*              rootCoord;
    _AttackScratch*        scratch;
    WorldCollisionCapsule* weaponCapsule;
    EffectWork*            trailWork;
    s32                    ammunitionSoundBits;
    s32                    readyBlendFrames;
    s32                    impactFound;
    s32                    chargedAttackKey;
    s16                    muzzleRadius;
    s32                    advanceThisFrame;

    advanceThisFrame    = 0;
    actor               = playerTask->work;
    ammunitionSoundBits = WEAPON_SOUND_VARIANT(gPlayerStatus.weaponSlotItem - SHOTGUN_ROUND_FIRST);
    weaponCapsule       = &actor->weaponShape;
    scratch             = SCRATCH_STACK_RESERVE_BLOCK(_AttackScratch);
    rootCoord           = playerTask->extra.tmd->coords;
    if (ammunitionSoundBits < 0) {
        ammunitionSoundBits = 0;
    }
    switch (actor->statePhase) {
        case PHASE_PREPARE:
            actor->mode           = GAME_ACTOR_MODE_NORMAL;
            actor->state          = PLAYER_ACTOR_STATE_ATTACK;
            actor->turnRateIndex  = 0;
            actor->animationState = 0;
            playerActorSetWeaponAttackFlags(playerTask, 0, 0);
            readyBlendFrames                                      = PE2_OPTION(READY_BLEND_FRAMES);
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= WORLD_COLLISION_BODY_CLIP_TO_GRID_CONTACT;
            actor->statePhase++;
            if (((u16)actor->movementMode | actor->turnSign) != 0) {
                readyBlendFrames = PE2_OPTION(MOVING_READY_BLEND_FRAMES);
            }
            playerActorPlayChildSlotsWithBlend(playerTask, PLAYER_ANIMATION_AIM_IDLE, 0, readyBlendFrames);
            actor->movementMode = GAME_ACTOR_MOVEMENT_STOPPED;
            break;
        case PHASE_WAIT_READY:
            if (animationGetCurrentRecord(&actor->animationContext, actor->animationSlots + 1) !=
                NULL) {
                actor->statePhase++;
            }
            break;
        case PHASE_SELECT_ATTACK:
            // The slash extends the capsule briefly; the shot uses ammunition-dependent spread.
            if (actor->attackButton == PLAYER_ACTOR_ATTACK_BUTTON_PRIMARY) {
                actor->statePhase        = PHASE_SWING_WINDUP;
                actor->stateTimer        = PE2_OPTION(SLASH_WINDUP_FRAMES);
                actor->actionValue       = 0;
                actor->gunbladeSpinTicks = PE2_OPTION(SPIN_COUNTER_FRAMES);
                weaponRecordUse(WEAPON_INDEX_GUNBLADE);
                actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].key = SLASH_KEY;
                {
                    u16 slashReach            = weaponCapsule->ends[1].vz + gPlayerActorWeaponReaches[WEAPON_INDEX_GUNBLADE];
                    weaponCapsule->end0Radius = PE2_OPTION(SLASH_RADIUS);
                    weaponCapsule->end1Radius = PE2_OPTION(SLASH_RADIUS);
                    weaponCapsule->ends[0].vz = slashReach;
                }
                actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_SINGLE_CONTACT);
                playerActorPlayChildSlotsWithBlend(playerTask, PLAYER_ANIMATION_FIRE, 0, 3);
                break;
            }
            actor->statePhase = PHASE_SHOT_IMPACT;
            actor->weaponShape.ends[0].vz =
                actor->weaponShape.ends[1].vz + PE2_OPTION(SHOT_REACH);
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].key = gPlayerStatus.weaponSlotItem | ATTACK_KEY_BASE;
            weaponCapsule->end1Radius                          = PE2_OPTION(SHOT_RADIUS);
            weaponCapsule->ends[0].vz                          = weaponCapsule->ends[1].vz + PE2_OPTION(SHOT_REACH);
            muzzleRadius                                       = PE2_OPTION(BUCKSHOT_RADIUS);
            if (gPlayerStatus.weaponSlotItem != SHOTGUN_ROUND_BUCKSHOT) {
                muzzleRadius = PE2_OPTION(SHOT_RADIUS);
            }
            weaponCapsule->end0Radius = muzzleRadius;
            if (gPlayerStatus.weaponSlotItem == SHOTGUN_ROUND_FIREFLY) {
                actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= WORLD_COLLISION_BODY_SINGLE_CONTACT;
            } else {
                actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_SINGLE_CONTACT);
            }
            equipmentConsumeWeaponLoad(WEAPON_ITEM_ID(WEAPON_INDEX_GUNBLADE), EQUIPMENT_WEAPON_LOAD_CONSUME_PRIMARY);
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
            worldCoordPlaySound(playerTask->extra.tmd->coords, ammunitionSoundBits | SOUND(SOUND_SHOT), 1);
            effectSpawn(EFFECT_SHOTGUN_MUZZLE_FLASH, actor->equipmentTasks[1]->extra.tmd->coords,
                        (gPlayerStatus.weaponSlotItem << 16) | WEAPON_INDEX_GUNBLADE, NULL);
            playerActorPlayChildSlotsWithBlend(playerTask, PLAYER_ANIMATION_SECONDARY_FIRE, 0, 3);
            break;
        case PHASE_SWING_WINDUP:
        case PHASE_CHARGE_WINDOW:
        case PHASE_SWING_FOLLOWTHROUGH:
            actor->stateTimer = actor->stateTimer - 1;
            if (actor->stateTimer == 0) {
                if (actor->statePhase == PHASE_SWING_WINDUP) {
                    actor->statePhase                                     = PHASE_CHARGE_WINDOW;
                    actor->stateTimer                                     = PE2_OPTION(CHARGE_WINDOW_FRAMES);
                    actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
                    worldCoordPlaySound(playerTask->extra.tmd->coords, ammunitionSoundBits | SOUND(SOUND_SLASH), 0);
                    trailWork = effectSpawn(EFFECT_GUNBLADE_TRAIL,
                                            actor->equipmentTasks[1]->extra.tmd->coords,
                                            WEAPON_INDEX_GUNBLADE, NULL);
                    if (trailWork != NULL) {
                        taskReparent(actor->equipmentTasks[1], trailWork->task);
                    }
                } else if (actor->statePhase >= PHASE_CHARGE_WINDOW) {
                    actor->statePhase = PHASE_RECOVER;
                }
            }
            if (actor->statePhase == PHASE_CHARGE_WINDOW && playerActorReadAttackButton(playerTask) == PLAYER_ACTOR_ATTACK_BUTTON_SECONDARY) {
                actor->statePhase = PHASE_SWING_FOLLOWTHROUGH;
                if (playerActorQueryWeaponLoads(PLAYER_ACTOR_WEAPON_LOAD_PRIMARY) != 0) {
                    if (gPlayerStatus.weaponSlotItem < SHOTGUN_ROUND_RIFLED_SLUG) {
                        chargedAttackKey = gPlayerStatus.weaponSlotItem + CHARGED_AMMUNITION_KEY_OFFSET;
                    } else {
                        chargedAttackKey = CHARGED_SLUG_KEY_LOW;
                    }
                    actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].key = chargedAttackKey | ATTACK_KEY_BASE;
                    equipmentConsumeWeaponLoad(WEAPON_ITEM_ID(WEAPON_INDEX_GUNBLADE), EQUIPMENT_WEAPON_LOAD_CONSUME_PRIMARY);
                    worldCoordPlaySound(playerTask->extra.tmd->coords, ammunitionSoundBits | SOUND(SOUND_CHARGED_SLASH), 1);
                    gunbladeRequestChargeFlash(gPlayerStatus.weaponSlotItem);
                } else {
                    worldCoordPlaySound(playerTask->extra.tmd->coords, ammunitionSoundBits | SOUND(WEAPON_SOUND_SLOT_CLICK), 0);
                }
            }
            if (actor->actionValue != CONTACT_SOUND_POSTED && worldCollisionCountContactsByKind(actor->weaponContacts, WORLD_COLLISION_CONTACT_ENEMY_BODY) != 0) {
                actor->actionValue = CONTACT_SOUND_POSTED;
                worldCoordPlaySound(playerTask->extra.tmd->coords, ammunitionSoundBits | SOUND(SOUND_CONTACT), 0);
            }
            if (actor->gunbladeSpinTicks != 0) {
                advanceThisFrame          = 1;
                actor->rotation.vy       += PE2_OPTION(SPIN_YAW_STEP);
                actor->gunbladeSpinTicks -= 1;
            }
            break;
        case PHASE_SHOT_IMPACT:
            actor->statePhase++;
            if (gPlayerStatus.weaponSlotItem != SHOTGUN_ROUND_BUCKSHOT) {
                impactFound = playerActorSpawnWeaponImpact(actor->weaponContacts, rootCoord, &scratch->impactCoord);
                if (gPlayerStatus.weaponSlotItem == SHOTGUN_ROUND_FIREFLY) {
                    if (impactFound != 0 || worldCollisionCountContactsByKind(actor->weaponContacts, WORLD_COLLISION_CONTACT_ENEMY_BODY) != 0) {
                        scratch->impactCoord.workm.t[0] = actor->weaponContacts[0].point.vx;
                        scratch->impactCoord.workm.t[1] = actor->weaponContacts[0].point.vy;
                        scratch->impactCoord.workm.t[2] = actor->weaponContacts[0].point.vz;
                        worldCoordPlaySound(&scratch->impactCoord, ammunitionSoundBits | SOUND(SOUND_FIREFLY_IMPACT), 1);
                    }
                } else if (impactFound != 0) {
                    worldCoordPlaySound(&scratch->impactCoord, SOUND_BULLET_IMPACT, 1);
                }
            }
            /* fallthrough */
        case PHASE_RECOVER:
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
            if (_weaponIsArmSlotAdvancing(playerTask) == 0) {
                playerActorFinishWeaponAttack(playerTask);
            }
            break;
    }
    // Only spinning-slash ticks move the actor along its unnormalized local forward axis (4096 / 136, about
    // 30 units a frame), and the actor publishes whether it moves.
    gfxReadMatrixZAxis(&rootCoord->coord, &scratch->forward);
    actor->movementSign    = advanceThisFrame;
    scratch->advance.vx    = (s16)(scratch->forward.vx / PE2_OPTION(SPIN_ADVANCE_DIVISOR)) * advanceThisFrame;
    scratch->advance.vy    = (s16)(scratch->forward.vy / PE2_OPTION(SPIN_ADVANCE_DIVISOR)) * advanceThisFrame;
    scratch->advance.vz    = (s16)(scratch->forward.vz / PE2_OPTION(SPIN_ADVANCE_DIVISOR)) * advanceThisFrame;
    rootCoord->coord.t[0] += scratch->advance.vx;
    rootCoord->coord.t[1] += scratch->advance.vy;
    rootCoord->coord.t[2] += scratch->advance.vz;
    SCRATCH_STACK_RELEASE_BLOCK(_AttackScratch);
}
