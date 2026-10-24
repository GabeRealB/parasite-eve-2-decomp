#include "weapons/gunblade.h"

#include "config.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "common.h"

#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/effects.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/items.h"
#include "gameplay/player_actor.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/display_types.h"
#include "main/random.h"
#include "main/gfx.h"
#include "main/scratch.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/mc_types.h"
#include "main/session_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "weapons/sounds.h"
#include "weapons/weapon.h"

#include "../shared/blade_trail.h"
#include "types.h"
#include "../shared/attack.h"

#include "assets/engine/animation/player_schema.h"

enum {
    GUNBLADE_TRAIL_ENDPOINT_NEAR = 0,
    GUNBLADE_TRAIL_ENDPOINT_FAR  = 1,
};

/// Near and far ribbon endpoints in integer coordinates of the Gunblade muzzle frame.
///
/// The task coordinate stays at endpoint 0; endpoint 1 is sampled every update.
static SVECTOR _gGunbladeTrailEndpointOffsets[] = {
    [GUNBLADE_TRAIL_ENDPOINT_NEAR] = { 0, 0x0060, 0x0080, 0 },
    [GUNBLADE_TRAIL_ENDPOINT_FAR]  = { 0, 0x0060, 0x0380, 0 },
};

/// The model `gGunbladeModel`: generated from its asset (weapons/gunblade/model) by tools/buildsteps/gen_model_source.py.
#include "assets/weapons/gunblade/model.inc"

/// The player's clips this package carries, in the package's order, and its animation bank (weapons/gunblade/bank):
/// generated from the clip and bank assets by tools/buildsteps/gen_animation_source.py.
#include "assets/weapons/gunblade/clips.inc"
#include "assets/weapons/gunblade/bank.inc"

/// Borrowed trail task receiving the ammunition selector for a deferred charge flash.
///
/// Published when the histories are seeded. Explicit retirement leaves this
/// handle unchanged; requests dereference it only behind `_gGunbladeTrailWork`.
/// Parent teardown does not invalidate either cached handle.
static Task* _gGunbladeTrailTask = NULL;

/// Borrowed trail work whose index counts deferred charge-flash requests.
///
/// Published with `_gGunbladeTrailTask` when the histories are seeded and cleared
/// before normal or cancellation retirement. Parent teardown releases the work
/// through the generic effect exit without clearing this cache; a non-NULL
/// value alone does not establish liveness after that teardown.
static EffectWork* _gGunbladeTrailWork = NULL;

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
        GUNBLADE_TRAIL_SEED           = 0,
        GUNBLADE_TRAIL_RECORD         = 1,
        GUNBLADE_TRAIL_CHARGE_PENDING = 1,
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
            case GUNBLADE_TRAIL_SEED:
                // Seed both histories with the current endpoint poses before drawing any ribbon.
                nearCoord->parent       = effectWork->parent;
                nearCoord->coord.t[0]   = _gGunbladeTrailEndpointOffsets[GUNBLADE_TRAIL_ENDPOINT_NEAR].vx;
                _gGunbladeTrailTask     = task;
                nearCoord->coord.t[1]   = _gGunbladeTrailEndpointOffsets[GUNBLADE_TRAIL_ENDPOINT_NEAR].vy;
                _gGunbladeTrailWork     = effectWork;
                nearCoord->coord.t[2]   = _gGunbladeTrailEndpointOffsets[GUNBLADE_TRAIL_ENDPOINT_NEAR].vz;
                nearCoord->composeStamp = GRAPHICS_COORD_DIRTY;
                actorRenderComposeCoord(nearCoord);
                task->state              = GUNBLADE_TRAIL_RECORD;
                farOffset                = &_gGunbladeTrailEndpointOffsets[GUNBLADE_TRAIL_ENDPOINT_FAR];
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
            case GUNBLADE_TRAIL_RECORD: {
                const SVECTOR* recordOffset;

                // Record world poses so old frames follow the camera, independently of the weapon.
                nearCoord->composeStamp = GRAPHICS_COORD_DIRTY;
                actorRenderComposeCoord(nearCoord);
                recordOffset             = &_gGunbladeTrailEndpointOffsets[GUNBLADE_TRAIL_ENDPOINT_FAR];
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
                if (effectWork->age < GUNBLADE_TRAIL_CHARGE_START_TICKS) {
                    _bladeTrailDraw(effectWork->age & (ARRAY_SIZE(_gBladeTrailFirstHistory) - 1), BLADE_TRAIL_TINT_BLUE_WHITE);
                    return;
                }
                // Post a deferred charge once the ribbon changes tint; repeated requests remain a counter.
                if (effectWork->index == GUNBLADE_TRAIL_CHARGE_PENDING) {
                    effectWork->index++;
                    flashWork = effectSpawn(EFFECT_GUNBLADE_CHARGE_FLASH, nearCoord, task->spawnArg1.value, NULL);
                    if (flashWork != NULL) {
                        taskReparent(task, flashWork->task);
                    }
                }
                _bladeTrailDraw(effectWork->age & (ARRAY_SIZE(_gBladeTrailFirstHistory) - 1), BLADE_TRAIL_TINT_YELLOW_WHITE);
                keepTask = effectWork->age < GUNBLADE_TRAIL_LIFETIME_TICKS;
                break;
            }
            default:
                return;
        }
    }
    if (!keepTask) {
        _gGunbladeTrailWork = NULL;
        effectKillTask(effectWork, task);
    }
}

#include "../shared/blade_trail_draw.inc.c"

enum {
    GUNBLADE_FLASH_INIT = 0,
    GUNBLADE_FLASH_DRAW = 1,
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
#define GUNBLADE_DRAW_CHARGE_BANDS(effectWork, coord, rgb)                                                     \
    {                                                                                                          \
        effectDrawOuterGlowBand(coord, (s16)((effectWork)->step * 3 / 2), GUNBLADE_FLASH_BAND_WIDTH, rgb);     \
        if ((effectWork)->age & 1) {                                                                           \
            effectDrawOuterGlowBand(coord, GUNBLADE_FLASH_BAND_WIDTH, (s16)((effectWork)->step * 3 / 2), rgb); \
        }                                                                                                      \
        effectDrawScreenTint(rgb, GPU_BLEND_ADD);                                                              \
        (effectWork)->period -= GUNBLADE_FLASH_BAND_BRIGHTNESS_STEP;                                           \
        (effectWork)->step   += GUNBLADE_FLASH_BAND_RADIUS_STEP;                                               \
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
            if (task->state == GUNBLADE_FLASH_INIT) {
                effectSpawn(EFFECT_IMPACT_FLASH, coord, GUNBLADE_FLASH_IMPACT_RADIUS, NULL);
                effectSpawn(EFFECT_EXPANDING_COLOR_BAND, coord, GUNBLADE_FLASH_BAND_BUCKSHOT_ARGUMENT, NULL);
                effectSpawn(EFFECT_EXPANDING_COLOR_BAND, coord, (GUNBLADE_FLASH_BAND_BUCKSHOT_ARGUMENT | GUNBLADE_FLASH_BAND_SECOND_ANGLE), NULL);
                effectSpawn(EFFECT_EXPANDING_COLOR_BAND, coord, (GUNBLADE_FLASH_BAND_BUCKSHOT_ARGUMENT | GUNBLADE_FLASH_BAND_THIRD_ANGLE), NULL);
                for (sparkIndex = 0; sparkIndex < GUNBLADE_FLASH_SPARK_COUNT; sparkIndex++) {
                    effectSpawn(EFFECT_SPARK_STREAK, coord, 0, NULL);
                }
                task->state       = GUNBLADE_FLASH_DRAW;
                effectWork->scale = effectWork->period = GUNBLADE_FLASH_INITIAL_BRIGHTNESS;
                effectWork->angle = effectWork->step = GUNBLADE_FLASH_INITIAL_HALF_RADIUS;
            }
            rgb[0] = rgb[1]    = effectWork->scale;
            rgb[2]             = effectWork->scale >> 2;
            effectWork->angle += GUNBLADE_FLASH_DISC_RADIUS_STEP;
            effectDrawGouraudDisc(coord, (s16)(effectWork->angle * 2), rgb);
            if (effectWork->period >= GUNBLADE_FLASH_BAND_BRIGHTNESS_MIN) {
                rgb[0] = rgb[1] = effectWork->period;
                rgb[2]          = effectWork->period >> 2;
                GUNBLADE_DRAW_CHARGE_BANDS(effectWork, coord, rgb);
                return;
            }
            effectWork->scale -= GUNBLADE_FLASH_DISC_BRIGHTNESS_STEP;
            if (effectWork->scale < GUNBLADE_FLASH_DISC_BRIGHTNESS_MIN) {
                effectKillTask(effectWork, task);
            }
            return;
        case SHOTGUN_ROUND_FIREFLY:
            if (task->state == GUNBLADE_FLASH_INIT) {
                effectSpawn(EFFECT_IMPACT_FLASH, coord, GUNBLADE_FLASH_IMPACT_RADIUS, NULL);
                effectSpawn(EFFECT_EXPANDING_COLOR_BAND, coord, GUNBLADE_FLASH_BAND_FIREFLY_ARGUMENT, NULL);
                effectSpawn(EFFECT_EXPANDING_COLOR_BAND, coord, (GUNBLADE_FLASH_BAND_FIREFLY_ARGUMENT | GUNBLADE_FLASH_BAND_SECOND_ANGLE), NULL);
                effectSpawn(EFFECT_EXPANDING_COLOR_BAND, coord, (GUNBLADE_FLASH_BAND_FIREFLY_ARGUMENT | GUNBLADE_FLASH_BAND_THIRD_ANGLE), NULL);
                for (sparkIndex = 0; sparkIndex < GUNBLADE_FLASH_BOUNCING_SPARK_COUNT; sparkIndex++) {
                    RANDOM_LCG_ADVANCE();
                    effectSpawn(EFFECT_BOUNCING_SPARK, coord, (RANDOM_LCG_DRAW() & GUNBLADE_FLASH_BOUNCING_SPARK_SPEED_MASK) | GUNBLADE_FLASH_BOUNCING_SPARK_COLOR, NULL);
                }
                task->state       = GUNBLADE_FLASH_DRAW;
                effectWork->scale = effectWork->period = GUNBLADE_FLASH_INITIAL_BRIGHTNESS;
                effectWork->angle = effectWork->step = GUNBLADE_FLASH_INITIAL_HALF_RADIUS;
            }
            rgb[0]             = effectWork->scale;
            rgb[1]             = effectWork->scale >> 1;
            rgb[2]             = effectWork->scale >> 2;
            effectWork->angle += GUNBLADE_FLASH_DISC_RADIUS_STEP;
            effectDrawGouraudDisc(coord, (s16)(effectWork->angle * 2), rgb);
            if (effectWork->period >= GUNBLADE_FLASH_BAND_BRIGHTNESS_MIN) {
                rgb[0] = effectWork->period;
                rgb[1] = effectWork->period >> 1;
                rgb[2] = effectWork->period >> 2;
                GUNBLADE_DRAW_CHARGE_BANDS(effectWork, coord, rgb);
                return;
            }
            effectWork->scale -= GUNBLADE_FLASH_DISC_BRIGHTNESS_STEP;
            if (effectWork->scale < GUNBLADE_FLASH_DISC_BRIGHTNESS_MIN) {
                effectKillTask(effectWork, task);
            }
            return;
        case SHOTGUN_ROUND_RIFLED_SLUG:
            if (task->state == GUNBLADE_FLASH_INIT) {
                effectSpawn(EFFECT_IMPACT_FLASH, coord, GUNBLADE_FLASH_IMPACT_RADIUS, NULL);
                effectSpawn(EFFECT_EXPANDING_COLOR_BAND, coord, GUNBLADE_FLASH_BAND_SLUG_ARGUMENT, NULL);
                effectSpawn(EFFECT_EXPANDING_COLOR_BAND, coord, (GUNBLADE_FLASH_BAND_SLUG_ARGUMENT | GUNBLADE_FLASH_BAND_SECOND_ANGLE), NULL);
                effectSpawn(EFFECT_EXPANDING_COLOR_BAND, coord, (GUNBLADE_FLASH_BAND_SLUG_ARGUMENT | GUNBLADE_FLASH_BAND_THIRD_ANGLE), NULL);
                for (sparkIndex = 0; sparkIndex < GUNBLADE_FLASH_SPARK_COUNT; sparkIndex++) {
                    effectSpawn(EFFECT_SPARK_STREAK, coord, GUNBLADE_FLASH_SLUG_SPARK_ARGUMENT, NULL);
                }
                task->state       = GUNBLADE_FLASH_DRAW;
                effectWork->scale = effectWork->period = GUNBLADE_FLASH_INITIAL_BRIGHTNESS;
                effectWork->angle = effectWork->step = GUNBLADE_FLASH_INITIAL_HALF_RADIUS;
            }
            rgb[0]             = effectWork->scale >> 2;
            rgb[1]             = effectWork->scale >> 1;
            rgb[2]             = effectWork->scale;
            effectWork->angle += GUNBLADE_FLASH_DISC_RADIUS_STEP;
            effectDrawGouraudDisc(coord, (s16)(effectWork->angle * 2), rgb);
            if (effectWork->period >= GUNBLADE_FLASH_BAND_BRIGHTNESS_MIN) {
                rgb[0] = effectWork->period >> 2;
                rgb[1] = effectWork->period >> 1;
                rgb[2] = effectWork->period;
                GUNBLADE_DRAW_CHARGE_BANDS(effectWork, coord, rgb);
                return;
            }
            effectWork->scale -= GUNBLADE_FLASH_DISC_BRIGHTNESS_STEP;
            if (effectWork->scale < GUNBLADE_FLASH_DISC_BRIGHTNESS_MIN) {
                effectKillTask(effectWork, task);
            }
            return;
    }
}

#undef GUNBLADE_DRAW_CHARGE_BANDS

/// Requests a deferred charge flash on the active gunblade trail.
///
/// ammunitionIndex is PlayerStatus::weaponSlotItem, consumed unchanged by the
/// flash effect (13..15 select its three drawn grades). With no live trail this
/// is a no-op. Otherwise the trail's pending counter is incremented; its update
/// spawns the flash only when that counter equals one starting at age nine.
/// Keep the cached trail task/work live and the gunblade overlay loaded.
static void gunbladeRequestChargeFlash(s32 ammunitionIndex)
{
    EffectWork* trailWork = _gGunbladeTrailWork;

    if (trailWork != NULL) {
        _gGunbladeTrailTask->spawnArg1.value = ammunitionIndex;
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
/// `GUNBLADE_SPIN_ADVANCE_DIVISOR` and multiplied by the frame's
/// `GameActor.movementSign`: 1 while the spin runs, 0 otherwise. It is added
/// to the root coordinate's local translation in every state. Neither
/// vector's `pad` is written.
typedef struct {
    GfxCoord impactCoord; // Node standing at the round's impact point: the source of the impact sound
    VECTOR   advance;     // This frame's displacement of the model's root coordinate, in coordinate units; zero outside the spin
    SVECTOR  forward;     // Model's forward axis: the Z column of its root coordinate's local matrix, 4096 per unit
} _GunbladeAttackScratch;
STATIC_ASSERT_SIZEOF(_GunbladeAttackScratch, 0x68);

void gunbladeAttackState(Task* playerTask)
{
    enum {
        GUNBLADE_PHASE_PREPARE             = 0,
        GUNBLADE_PHASE_WAIT_READY          = 1,
        GUNBLADE_PHASE_SELECT_ATTACK       = 2,
        GUNBLADE_PHASE_SWING_WINDUP        = 3,
        GUNBLADE_PHASE_CHARGE_WINDOW       = 4,
        GUNBLADE_PHASE_SWING_FOLLOWTHROUGH = 5,
        GUNBLADE_PHASE_SHOT_IMPACT         = 6,
        GUNBLADE_PHASE_RECOVER             = 7,
        GUNBLADE_CONTACT_SOUND_POSTED      = 1,
    };
    GameActor*              actor;
    GfxCoord*               rootCoord;
    _GunbladeAttackScratch* scratch;
    WorldCollisionCapsule*  weaponCapsule;
    EffectWork*             trailWork;
    s32                     ammunitionSoundBits;
    s32                     readyBlendFrames;
    s32                     impactFound;
    s32                     chargedAttackKey;
    s16                     muzzleRadius;
    s32                     advanceThisFrame;

    advanceThisFrame    = 0;
    actor               = playerTask->work;
    ammunitionSoundBits = WEAPON_SOUND_VARIANT(gPlayerStatus.weaponSlotItem - SHOTGUN_ROUND_FIRST);
    weaponCapsule       = &actor->weaponShape;
    scratch             = SCRATCH_STACK_RESERVE_BLOCK(_GunbladeAttackScratch);
    rootCoord           = playerTask->extra.tmd->coords;
    if (ammunitionSoundBits < 0) {
        ammunitionSoundBits = 0;
    }
    switch (actor->statePhase) {
        case GUNBLADE_PHASE_PREPARE:
            actor->mode           = GAME_ACTOR_MODE_NORMAL;
            actor->state          = PLAYER_ACTOR_STATE_ATTACK;
            actor->turnRateIndex  = 0;
            actor->animationState = 0;
            playerActorSetWeaponAttackFlags(playerTask, 0, 0);
            readyBlendFrames                                      = GUNBLADE_READY_BLEND_FRAMES;
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= WORLD_COLLISION_BODY_CLIP_TO_GRID_CONTACT;
            actor->statePhase++;
            if (((u16)actor->movementMode | actor->turnSign) != 0) {
                readyBlendFrames = GUNBLADE_MOVING_READY_BLEND_FRAMES;
            }
            playerActorPlayChildSlotsWithBlend(playerTask, PLAYER_ANIMATION_AIM_IDLE, 0, readyBlendFrames);
            actor->movementMode = GAME_ACTOR_MOVEMENT_STOPPED;
            break;
        case GUNBLADE_PHASE_WAIT_READY:
            if (animationGetCurrentRecord(&actor->animationContext, actor->animationSlots + 1) !=
                NULL) {
                actor->statePhase++;
            }
            break;
        case GUNBLADE_PHASE_SELECT_ATTACK:
            // The slash extends the capsule briefly; the shot uses ammunition-dependent spread.
            if (actor->attackButton == PLAYER_ACTOR_ATTACK_BUTTON_PRIMARY) {
                actor->statePhase        = GUNBLADE_PHASE_SWING_WINDUP;
                actor->stateTimer        = GUNBLADE_SLASH_WINDUP_FRAMES;
                actor->actionValue       = 0;
                actor->gunbladeSpinTicks = GUNBLADE_SPIN_COUNTER_TICKS;
                weaponRecordUse(WEAPON_INDEX_GUNBLADE);
                actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].key = GUNBLADE_SLASH_KEY;
                {
                    u16 slashReach            = weaponCapsule->ends[1].vz + gPlayerActorWeaponReaches[WEAPON_INDEX_GUNBLADE];
                    weaponCapsule->end0Radius = GUNBLADE_SLASH_RADIUS;
                    weaponCapsule->end1Radius = GUNBLADE_SLASH_RADIUS;
                    weaponCapsule->ends[0].vz = slashReach;
                }
                actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_SINGLE_CONTACT);
                playerActorPlayChildSlotsWithBlend(playerTask, PLAYER_ANIMATION_FIRE, 0, 3);
                break;
            }
            actor->statePhase = GUNBLADE_PHASE_SHOT_IMPACT;
            actor->weaponShape.ends[0].vz =
                actor->weaponShape.ends[1].vz + GUNBLADE_SHOT_REACH;
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].key = gPlayerStatus.weaponSlotItem | GUNBLADE_ATTACK_KEY_BASE;
            weaponCapsule->end1Radius                          = GUNBLADE_SHOT_RADIUS;
            weaponCapsule->ends[0].vz                          = weaponCapsule->ends[1].vz + GUNBLADE_SHOT_REACH;
            muzzleRadius                                       = GUNBLADE_BUCKSHOT_RADIUS;
            if (gPlayerStatus.weaponSlotItem != SHOTGUN_ROUND_BUCKSHOT) {
                muzzleRadius = GUNBLADE_SHOT_RADIUS;
            }
            weaponCapsule->end0Radius = muzzleRadius;
            if (gPlayerStatus.weaponSlotItem == SHOTGUN_ROUND_FIREFLY) {
                actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= WORLD_COLLISION_BODY_SINGLE_CONTACT;
            } else {
                actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_SINGLE_CONTACT);
            }
            equipmentConsumeWeaponLoad(WEAPON_ITEM_ID(WEAPON_INDEX_GUNBLADE), EQUIPMENT_WEAPON_LOAD_CONSUME_PRIMARY);
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
            worldCoordPlaySound(playerTask->extra.tmd->coords, ammunitionSoundBits | GUNBLADE_SOUND(GUNBLADE_SOUND_SHOT), 1);
            effectSpawn(EFFECT_SHOTGUN_MUZZLE_FLASH, actor->equipmentTasks[1]->extra.tmd->coords,
                        (gPlayerStatus.weaponSlotItem << 16) | WEAPON_INDEX_GUNBLADE, NULL);
            playerActorPlayChildSlotsWithBlend(playerTask, PLAYER_ANIMATION_SECONDARY_FIRE, 0, 3);
            break;
        case GUNBLADE_PHASE_SWING_WINDUP:
        case GUNBLADE_PHASE_CHARGE_WINDOW:
        case GUNBLADE_PHASE_SWING_FOLLOWTHROUGH:
            actor->stateTimer = actor->stateTimer - 1;
            if (actor->stateTimer == 0) {
                if (actor->statePhase == GUNBLADE_PHASE_SWING_WINDUP) {
                    actor->statePhase                                     = GUNBLADE_PHASE_CHARGE_WINDOW;
                    actor->stateTimer                                     = GUNBLADE_CHARGE_WINDOW_FRAMES;
                    actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
                    worldCoordPlaySound(playerTask->extra.tmd->coords, ammunitionSoundBits | GUNBLADE_SOUND(GUNBLADE_SOUND_SLASH), 0);
                    trailWork = effectSpawn(EFFECT_GUNBLADE_TRAIL,
                                            actor->equipmentTasks[1]->extra.tmd->coords,
                                            WEAPON_INDEX_GUNBLADE, NULL);
                    if (trailWork != NULL) {
                        taskReparent(actor->equipmentTasks[1], trailWork->task);
                    }
                } else if (actor->statePhase >= GUNBLADE_PHASE_CHARGE_WINDOW) {
                    actor->statePhase = GUNBLADE_PHASE_RECOVER;
                }
            }
            if (actor->statePhase == GUNBLADE_PHASE_CHARGE_WINDOW && playerActorReadAttackButton(playerTask) == PLAYER_ACTOR_ATTACK_BUTTON_SECONDARY) {
                actor->statePhase = GUNBLADE_PHASE_SWING_FOLLOWTHROUGH;
                if (playerActorQueryWeaponLoads(PLAYER_ACTOR_WEAPON_LOAD_PRIMARY) != 0) {
                    if (gPlayerStatus.weaponSlotItem < SHOTGUN_ROUND_RIFLED_SLUG) {
                        chargedAttackKey = gPlayerStatus.weaponSlotItem + GUNBLADE_CHARGED_AMMUNITION_KEY_OFFSET;
                    } else {
                        chargedAttackKey = GUNBLADE_CHARGED_SLUG_KEY_LOW;
                    }
                    actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].key = chargedAttackKey | GUNBLADE_ATTACK_KEY_BASE;
                    equipmentConsumeWeaponLoad(WEAPON_ITEM_ID(WEAPON_INDEX_GUNBLADE), EQUIPMENT_WEAPON_LOAD_CONSUME_PRIMARY);
                    worldCoordPlaySound(playerTask->extra.tmd->coords, ammunitionSoundBits | GUNBLADE_SOUND(GUNBLADE_SOUND_CHARGED_SLASH), 1);
                    gunbladeRequestChargeFlash(gPlayerStatus.weaponSlotItem);
                } else {
                    worldCoordPlaySound(playerTask->extra.tmd->coords, ammunitionSoundBits | GUNBLADE_SOUND(WEAPON_SOUND_SLOT_CLICK), 0);
                }
            }
            if (actor->actionValue != GUNBLADE_CONTACT_SOUND_POSTED && worldCollisionCountContactsByKind(actor->weaponContacts, WORLD_COLLISION_CONTACT_ENEMY_BODY) != 0) {
                actor->actionValue = GUNBLADE_CONTACT_SOUND_POSTED;
                worldCoordPlaySound(playerTask->extra.tmd->coords, ammunitionSoundBits | GUNBLADE_SOUND(GUNBLADE_SOUND_CONTACT), 0);
            }
            if (actor->gunbladeSpinTicks != 0) {
                advanceThisFrame          = 1;
                actor->rotation.vy       += GUNBLADE_SPIN_YAW_STEP;
                actor->gunbladeSpinTicks -= 1;
            }
            break;
        case GUNBLADE_PHASE_SHOT_IMPACT:
            actor->statePhase++;
            if (gPlayerStatus.weaponSlotItem != SHOTGUN_ROUND_BUCKSHOT) {
                impactFound = playerActorSpawnWeaponImpact(actor->weaponContacts, rootCoord, &scratch->impactCoord);
                if (gPlayerStatus.weaponSlotItem == SHOTGUN_ROUND_FIREFLY) {
                    if (impactFound != 0 || worldCollisionCountContactsByKind(actor->weaponContacts, WORLD_COLLISION_CONTACT_ENEMY_BODY) != 0) {
                        scratch->impactCoord.workm.t[0] = actor->weaponContacts[0].point.vx;
                        scratch->impactCoord.workm.t[1] = actor->weaponContacts[0].point.vy;
                        scratch->impactCoord.workm.t[2] = actor->weaponContacts[0].point.vz;
                        worldCoordPlaySound(&scratch->impactCoord, ammunitionSoundBits | GUNBLADE_SOUND(GUNBLADE_SOUND_FIREFLY_IMPACT), 1);
                    }
                } else if (impactFound != 0) {
                    worldCoordPlaySound(&scratch->impactCoord, SOUND_BULLET_IMPACT, 1);
                }
            }
            /* fallthrough */
        case GUNBLADE_PHASE_RECOVER:
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
    scratch->advance.vx    = (s16)(scratch->forward.vx / GUNBLADE_SPIN_ADVANCE_DIVISOR) * advanceThisFrame;
    scratch->advance.vy    = (s16)(scratch->forward.vy / GUNBLADE_SPIN_ADVANCE_DIVISOR) * advanceThisFrame;
    scratch->advance.vz    = (s16)(scratch->forward.vz / GUNBLADE_SPIN_ADVANCE_DIVISOR) * advanceThisFrame;
    rootCoord->coord.t[0] += scratch->advance.vx;
    rootCoord->coord.t[1] += scratch->advance.vy;
    rootCoord->coord.t[2] += scratch->advance.vz;
    SCRATCH_STACK_RELEASE_BLOCK(_GunbladeAttackScratch);
}
