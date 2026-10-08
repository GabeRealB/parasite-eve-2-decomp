#include "weapons/gunblade.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "gunblade_private.h"

#include "gameplay/animation.h"
#include "gameplay/effects.h"
#include "gameplay/items.h"
#include "gameplay/player_actor.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/scratch.h"
#include "main/sound.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "weapons/weapon.h"

/// Divisor that turns the model's forward axis into the distance the spinning
/// slash covers per frame: 4096 / 136, about 30 coordinate units.
enum { GUNBLADE_SPIN_ADVANCE_DIVISOR = 136 };

/// One-based weapon index passed to the resident use counter.
enum { GUNBLADE_WEAPON_ID = 23 };

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

/// Applies the Gunblade spin's gated forward displacement and publishes its movement sign.
///
/// `advanceThisFrame` is 0 outside a spinning-slash tick, 1 during one. Reads
/// the root's unnormalized local Z axis into `forward` (4096 per unit), then
/// stores the flag in the actor before calculating `advance`. Each signed
/// component is divided by 136 toward zero and narrowed to s16 before s32
/// multiplication by the flag. Adds XYZ in the root parent's coordinate units;
/// a scaled root therefore scales the step too. Requires live disjoint writable
/// actor, root and vector storage, with representable translation sums.
/// Leaves vector pads and the root's composition stamp untouched; the caller
/// owns cache invalidation and scratch lifetime. Retains no pointer.
///
/// `rootCoord`, `actor`, `advance` and `forward` are respectively `GfxCoord*`,
/// `GameActor*`, `VECTOR*` and `SVECTOR*`; `advanceThisFrame` must be s32.
/// Arguments repeat and must have no side effects; captures no caller locals.
/// The block has no return/break; invoke it as a standalone statement inside braces.
#define GUNBLADE_ADVANCE_ATTACK(rootCoord, actor, advance, forward, advanceThisFrame)                         \
    {                                                                                                         \
        gfxReadMatrixZAxis(&(rootCoord)->coord, (forward));                                                   \
        (actor)->movementSign    = (advanceThisFrame);                                                        \
        (advance)->vx            = (s16)((forward)->vx / GUNBLADE_SPIN_ADVANCE_DIVISOR) * (advanceThisFrame); \
        (advance)->vy            = (s16)((forward)->vy / GUNBLADE_SPIN_ADVANCE_DIVISOR) * (advanceThisFrame); \
        (advance)->vz            = (s16)((forward)->vz / GUNBLADE_SPIN_ADVANCE_DIVISOR) * (advanceThisFrame); \
        (rootCoord)->coord.t[0] += (advance)->vx;                                                             \
        (rootCoord)->coord.t[1] += (advance)->vy;                                                             \
        (rootCoord)->coord.t[2] += (advance)->vz;                                                             \
    }

void gunbladeAttackState(Task* playerTask)
{
    enum {
        GUNBLADE_PHASE_PREPARE                 = 0,
        GUNBLADE_PHASE_WAIT_READY              = 1,
        GUNBLADE_PHASE_SELECT_ATTACK           = 2,
        GUNBLADE_PHASE_SWING_WINDUP            = 3,
        GUNBLADE_PHASE_CHARGE_WINDOW           = 4,
        GUNBLADE_PHASE_SWING_FOLLOWTHROUGH     = 5,
        GUNBLADE_PHASE_SHOT_IMPACT             = 6,
        GUNBLADE_PHASE_RECOVER                 = 7,
        GUNBLADE_PLAYER_ATTACK_STATE           = 4,
        GUNBLADE_ANIMATION_READY               = 9,
        GUNBLADE_READY_BLEND_FRAMES            = 1,
        GUNBLADE_IMPACT_SOUND                  = SOUND_COMMON(0x17),
        GUNBLADE_MOVING_READY_BLEND_FRAMES     = 6,
        GUNBLADE_ANIMATION_SLASH               = 0xA,
        GUNBLADE_ANIMATION_SHOT                = 0xB,
        GUNBLADE_SLASH_WINDUP_FRAMES           = 0x12,
        GUNBLADE_SPIN_COUNTER_TICKS            = 0x39,
        GUNBLADE_CHARGE_WINDOW_FRAMES          = 8,
        GUNBLADE_SPIN_YAW_STEP                 = 0xC,
        GUNBLADE_SLASH_KEY                     = 0x2171B,
        GUNBLADE_ATTACK_KEY_BASE               = 0x21700,
        GUNBLADE_SLASH_RADIUS                  = 0x180,
        GUNBLADE_SHOT_REACH                    = 0x2200,
        GUNBLADE_SHOT_RADIUS                   = 0x100,
        GUNBLADE_BUCKSHOT_RADIUS               = 0x900,
        GUNBLADE_AMMUNITION_BUCKSHOT           = 13,
        GUNBLADE_AMMUNITION_FIREFLY            = 14,
        GUNBLADE_AMMUNITION_SLUG               = 15,
        GUNBLADE_CHARGED_SLUG_KEY_LOW          = 0x20,
        GUNBLADE_CHARGED_AMMUNITION_KEY_OFFSET = 0xB,
        GUNBLADE_CONTACT_SOUND_POSTED          = 1,
        GUNBLADE_EMPTY_SOUND                   = SOUND_WEAPON(23, 1),
        GUNBLADE_FIREFLY_IMPACT_SOUND          = SOUND_WEAPON(23, 4),
        GUNBLADE_FIRE_SOUND                    = SOUND_WEAPON(23, 5),
        GUNBLADE_SLASH_SOUND                   = SOUND_WEAPON(23, 6),
        GUNBLADE_CONTACT_SOUND                 = SOUND_WEAPON(23, 7),
        GUNBLADE_CHARGE_SOUND                  = SOUND_WEAPON(23, 8),
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
    ammunitionSoundBits = (gPlayerStatus.weaponSlotItem - GUNBLADE_AMMUNITION_BUCKSHOT) << 24;
    weaponCapsule       = &actor->weaponShape;
    scratch             = SCRATCH_STACK_RESERVE_BLOCK(_GunbladeAttackScratch);
    rootCoord           = playerTask->extra.tmd->coords;
    if (ammunitionSoundBits < 0) {
        ammunitionSoundBits = 0;
    }
    switch (actor->statePhase) {
        case GUNBLADE_PHASE_PREPARE:
            actor->mode           = GAME_ACTOR_MODE_NORMAL;
            actor->state          = GUNBLADE_PLAYER_ATTACK_STATE;
            actor->turnRateIndex  = 0;
            actor->animationState = 0;
            playerActorSetWeaponAttackFlags(playerTask, 0, 0);
            readyBlendFrames                                      = GUNBLADE_READY_BLEND_FRAMES;
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= WORLD_COLLISION_BODY_CLIP_TO_GRID_CONTACT;
            actor->statePhase++;
            if (((u16)actor->movementMode | actor->turnSign) != 0) {
                readyBlendFrames = GUNBLADE_MOVING_READY_BLEND_FRAMES;
            }
            playerActorPlayChildSlotsWithBlend(playerTask, GUNBLADE_ANIMATION_READY, 0, readyBlendFrames);
            actor->movementMode = 0;
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
                weaponRecordUse(GUNBLADE_WEAPON_ID);
                actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].key = GUNBLADE_SLASH_KEY;
                {
                    u16 slashReach            = weaponCapsule->ends[1].vz + D_80112F60[GUNBLADE_WEAPON_ID];
                    weaponCapsule->end0Radius = GUNBLADE_SLASH_RADIUS;
                    weaponCapsule->end1Radius = GUNBLADE_SLASH_RADIUS;
                    weaponCapsule->ends[0].vz = slashReach;
                }
                actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_SINGLE_CONTACT);
                playerActorPlayChildSlotsWithBlend(playerTask, GUNBLADE_ANIMATION_SLASH, 0, 3);
                break;
            }
            actor->statePhase = GUNBLADE_PHASE_SHOT_IMPACT;
            actor->weaponShape.ends[0].vz =
                actor->weaponShape.ends[1].vz + GUNBLADE_SHOT_REACH;
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].key = gPlayerStatus.weaponSlotItem | GUNBLADE_ATTACK_KEY_BASE;
            weaponCapsule->end1Radius                          = GUNBLADE_SHOT_RADIUS;
            weaponCapsule->ends[0].vz                          = weaponCapsule->ends[1].vz + GUNBLADE_SHOT_REACH;
            muzzleRadius                                       = GUNBLADE_BUCKSHOT_RADIUS;
            if (gPlayerStatus.weaponSlotItem != GUNBLADE_AMMUNITION_BUCKSHOT) {
                muzzleRadius = GUNBLADE_SHOT_RADIUS;
            }
            weaponCapsule->end0Radius = muzzleRadius;
            if (gPlayerStatus.weaponSlotItem == GUNBLADE_AMMUNITION_FIREFLY) {
                actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= WORLD_COLLISION_BODY_SINGLE_CONTACT;
            } else {
                actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_SINGLE_CONTACT);
            }
            equipmentConsumeWeaponLoad(WEAPON_ITEM(GUNBLADE_WEAPON_ID), EQUIPMENT_WEAPON_LOAD_CONSUME_PRIMARY);
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
            worldCoordPlaySound(playerTask->extra.tmd->coords, ammunitionSoundBits | GUNBLADE_FIRE_SOUND, 1);
            effectSpawn(EFFECT_SHOTGUN_MUZZLE_FLASH, actor->equipmentTasks[1]->extra.tmd->coords,
                        (gPlayerStatus.weaponSlotItem << 16) | GUNBLADE_WEAPON_ID, NULL);
            playerActorPlayChildSlotsWithBlend(playerTask, GUNBLADE_ANIMATION_SHOT, 0, 3);
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
                    worldCoordPlaySound(playerTask->extra.tmd->coords, ammunitionSoundBits | GUNBLADE_SLASH_SOUND, 0);
                    trailWork = effectSpawn(EFFECT_GUNBLADE_TRAIL,
                                            actor->equipmentTasks[1]->extra.tmd->coords,
                                            GUNBLADE_WEAPON_ID, NULL);
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
                    if (gPlayerStatus.weaponSlotItem < GUNBLADE_AMMUNITION_SLUG) {
                        chargedAttackKey = gPlayerStatus.weaponSlotItem + GUNBLADE_CHARGED_AMMUNITION_KEY_OFFSET;
                    } else {
                        chargedAttackKey = GUNBLADE_CHARGED_SLUG_KEY_LOW;
                    }
                    actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].key = chargedAttackKey | GUNBLADE_ATTACK_KEY_BASE;
                    equipmentConsumeWeaponLoad(WEAPON_ITEM(GUNBLADE_WEAPON_ID), EQUIPMENT_WEAPON_LOAD_CONSUME_PRIMARY);
                    worldCoordPlaySound(playerTask->extra.tmd->coords, ammunitionSoundBits | GUNBLADE_CHARGE_SOUND, 1);
                    gunbladeRequestChargeFlash(gPlayerStatus.weaponSlotItem);
                } else {
                    worldCoordPlaySound(playerTask->extra.tmd->coords, ammunitionSoundBits | GUNBLADE_EMPTY_SOUND, 0);
                }
            }
            if (actor->actionValue != GUNBLADE_CONTACT_SOUND_POSTED && worldCollisionCountContactsByKind(actor->weaponContacts, WORLD_COLLISION_CONTACT_ENEMY_BODY) != 0) {
                actor->actionValue = GUNBLADE_CONTACT_SOUND_POSTED;
                worldCoordPlaySound(playerTask->extra.tmd->coords, ammunitionSoundBits | GUNBLADE_CONTACT_SOUND, 0);
            }
            if (actor->gunbladeSpinTicks != 0) {
                advanceThisFrame          = 1;
                actor->rotation.vy       += GUNBLADE_SPIN_YAW_STEP;
                actor->gunbladeSpinTicks -= 1;
            }
            break;
        case GUNBLADE_PHASE_SHOT_IMPACT:
            actor->statePhase++;
            if (gPlayerStatus.weaponSlotItem != GUNBLADE_AMMUNITION_BUCKSHOT) {
                impactFound = playerActorSpawnWeaponImpact(actor->weaponContacts, rootCoord, &scratch->impactCoord);
                if (gPlayerStatus.weaponSlotItem == GUNBLADE_AMMUNITION_FIREFLY) {
                    if (impactFound != 0 || worldCollisionCountContactsByKind(actor->weaponContacts, WORLD_COLLISION_CONTACT_ENEMY_BODY) != 0) {
                        scratch->impactCoord.workm.t[0] = actor->weaponContacts[0].point.vx;
                        scratch->impactCoord.workm.t[1] = actor->weaponContacts[0].point.vy;
                        scratch->impactCoord.workm.t[2] = actor->weaponContacts[0].point.vz;
                        worldCoordPlaySound(&scratch->impactCoord, ammunitionSoundBits | GUNBLADE_FIREFLY_IMPACT_SOUND, 1);
                    }
                } else if (impactFound != 0) {
                    worldCoordPlaySound(&scratch->impactCoord, GUNBLADE_IMPACT_SOUND, 1);
                }
            }
            /* fallthrough */
        case GUNBLADE_PHASE_RECOVER:
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
            if (playerActorIsSlotAdvancingLinearly(playerTask, D_80112E04[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId][1], 0, 0) == 0) {
                playerActorFinishWeaponAttack(playerTask);
            }
            break;
    }
    // Only spinning-slash ticks move the actor along its unnormalized local forward axis.
    GUNBLADE_ADVANCE_ATTACK(rootCoord, actor, &scratch->advance, &scratch->forward, advanceThisFrame);
    SCRATCH_STACK_RELEASE_BLOCK(_GunbladeAttackScratch);
}
#undef GUNBLADE_ADVANCE_ATTACK
