#include "weapons/mp5a5.h"

#include "config.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "common.h"
#include "types.h"

#include "gameplay/effects.h"
#include "gameplay/items.h"
#include "gameplay/player_actor.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_coords.h"

#include "main/mc_types.h"
#include "main/scratch.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "weapons/weapon.h"
#include "weapons/sounds.h"

#include "../shared/muzzle_flash.h"

/// Manifest-selected weapon index, equal to its inventory item ID minus 0x7F.
///
/// Required integer preprocessor expression: 30 MP5A5, 31 MP5A5(+1), 32 MP5A5(+2).
/// Occupies contact-key bits 8..15 and sound-bank bits 16..23, and selects the
/// muzzle/casing profile. `WEAPON_ITEM_ID` converts it to the weapon item whose
/// primary or secondary load is consumed.
#ifndef WEAPON_INDEX
#error "WEAPON_INDEX is a per-package build parameter"
#endif

/// MP5A5 alternate-fire flash origin in integer weapon-root coordinates.
static SVECTOR _gMuzzleFlashOffset = { 0, 576, 64, 0 };

/// Retained streak directions shared by every live flash in this weapon package.
///
/// Angles use 4096 units per turn, one random direction in each quadrant.
/// Each newly initialized flash replaces the directions older flashes read.
static s16 _gMuzzleFlashAngles[MUZZLE_FLASH_STREAK_COUNT];

#include "../shared/muzzle_flash.inc.c"
#include "../shared/attack.h"

DEFINE_ALIAS(_muzzleFlashTask, mp5a5MuzzleFlashTask);

#include "assets/engine/animation/player_schema.h"

void mp5a5AttackState(Task* playerTask)
{
    enum {
        MP5A5_PHASE_PREPARE          = 0,
        MP5A5_PHASE_WAIT_READY       = 1,
        MP5A5_PHASE_ARM_SHOT         = 2,
        MP5A5_PHASE_SHOT_DELAY       = 3,
        MP5A5_PHASE_PRIMARY_IMPACT   = 4,
        MP5A5_PHASE_SECONDARY_IMPACT = 5,
        MP5A5_PHASE_RECOVER          = 6,
    };
    GameActor*             actor;
    GfxCoord*              rootCoord;
    GfxCoord*              impactCoord;
    WorldCollisionCapsule* weaponShape;
    EffectWork*            flashWork;
    s32                    readyBlendFrames;

    impactCoord = SCRATCH_STACK_RESERVE_BLOCK(GfxCoord);
    actor       = playerTask->work;
    rootCoord   = playerTask->extra.tmd->coords;
    switch (actor->statePhase) {
        case MP5A5_PHASE_PREPARE:
            actor->state                                          = PLAYER_ACTOR_STATE_ATTACK;
            actor->turnRateIndex                                  = 2;
            actor->mode                                           = GAME_ACTOR_MODE_NORMAL;
            actor->animationState                                 = 0;
            actor->stateTimer                                     = 1;
            actor->statePhase                                    += 1;
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= WORLD_COLLISION_BODY_CLIP_TO_GRID_CONTACT;
            readyBlendFrames                                      = MP5A5_READY_BLEND_FRAMES;
            if (((u16)actor->movementMode | actor->turnSign) != 0) {
                readyBlendFrames = MP5A5_MOVING_READY_BLEND_FRAMES;
            }
            playerActorPlayChildSlotsWithBlend(playerTask, PLAYER_ANIMATION_AIM_IDLE, 0, readyBlendFrames);
            actor->movementMode = GAME_ACTOR_MOVEMENT_STOPPED;
            break;
        case MP5A5_PHASE_WAIT_READY:
            if (animationGetCurrentRecord(&actor->animationContext, actor->animationSlots + 1) !=
                NULL) {
                actor->statePhase++;
            }
            break;
        case MP5A5_PHASE_ARM_SHOT:
        fire:
            actor->statePhase   = MP5A5_PHASE_SHOT_DELAY;
            actor->rumblePosted = 0;
            /* fallthrough */
        case MP5A5_PHASE_SHOT_DELAY:
            if (--actor->stateTimer == 0) {
                weaponShape = &actor->weaponShape;
                // Primary fire clips at its first contact; secondary fire keeps a wide shape.
                if (actor->attackButton == PLAYER_ACTOR_ATTACK_BUTTON_PRIMARY) {
                    actor->statePhase                                     = MP5A5_PHASE_PRIMARY_IMPACT;
                    actor->stateTimer                                     = MP5A5_PRIMARY_SHOT_DELAY_FRAMES;
                    actor->attackControl.cooldownTicks                    = 0;
                    actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].key    = gPlayerStatus.weaponSlotItem | WORLD_COLLISION_CONTACT_ATTACK | (WEAPON_INDEX << 8);
                    weaponShape->end0Radius                               = weaponShape->end1Radius;
                    actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= WORLD_COLLISION_BODY_SINGLE_CONTACT;
                    playerActorSetWeaponAttackFlags(playerTask, 0, 1);
                    worldCoordPlaySound(playerTask->extra.tmd->coords, MP5A5_SOUND(WEAPON_SOUND_SLOT_SHOT), 1);
                    effectSpawn(EFFECT_HANDGUN_MUZZLE_FLASH,
                                actor->equipmentTasks[1]->extra.tmd->coords,
                                WEAPON_INDEX, NULL);
                    equipmentConsumeWeaponLoad(WEAPON_ITEM_ID(WEAPON_INDEX), EQUIPMENT_WEAPON_LOAD_CONSUME_PRIMARY);
                    playerActorPlayChildSlotsWithBlend(playerTask, PLAYER_ANIMATION_FIRE, 0, 2);
                } else {
                    actor->statePhase                                     = MP5A5_PHASE_SECONDARY_IMPACT;
                    actor->attackControl.cooldownTicks                    = MP5A5_SECONDARY_COOLDOWN_FRAMES;
                    actor->stateTimer                                     = MP5A5_SECONDARY_ACTIVE_FRAMES;
                    actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].key    = MP5A5_SECONDARY_KEY_BASE | (WEAPON_INDEX << 8);
                    weaponShape->end0Radius                               = MP5A5_SECONDARY_SPREAD_RADIUS;
                    actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_SINGLE_CONTACT);
                    playerActorSetWeaponAttackFlags(playerTask, 0, 0);
                    worldCoordPlaySound(playerTask->extra.tmd->coords, MP5A5_SOUND(MP5A5_SOUND_SECONDARY_SHOT), 0);
                    equipmentConsumeWeaponLoad(WEAPON_ITEM_ID(WEAPON_INDEX), EQUIPMENT_WEAPON_LOAD_CONSUME_SECONDARY);
                    flashWork = effectSpawn(EFFECT_MP5A5_ALT_FIRE_MUZZLE_FLASH,
                                            actor->equipmentTasks[1]->extra.tmd->coords,
                                            WEAPON_INDEX, NULL);
                    if (flashWork != NULL) {
                        taskReparent(actor->equipmentTasks[1], flashWork->task);
                    }
                }
                actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
            }
            break;
        case MP5A5_PHASE_PRIMARY_IMPACT:
        case MP5A5_PHASE_SECONDARY_IMPACT:
            if (actor->statePhase < MP5A5_PHASE_SECONDARY_IMPACT && playerActorSpawnWeaponImpact(actor->weaponContacts, rootCoord, impactCoord) != 0) {
                worldCoordPlaySound(impactCoord, SOUND_BULLET_IMPACT, 1);
            }
            actor->statePhase                                     = MP5A5_PHASE_RECOVER;
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
            /* fallthrough */
        case MP5A5_PHASE_RECOVER:
            // Reuse the shot-entry path when primary input survives the cooldown.
            if (playerActorReadAttackButton(playerTask) == PLAYER_ACTOR_ATTACK_BUTTON_PRIMARY && playerActorQueryWeaponLoads(PLAYER_ACTOR_WEAPON_LOAD_PRIMARY) > 0 && actor->attackControl.cooldownTicks == 0) {
                goto fire;
            }
            if (_weaponIsArmSlotAdvancing(playerTask) == 0) {
                playerActorFinishWeaponAttack(playerTask);
            }
            break;
    }
    playerActorTrackLockTarget(playerTask);
    SCRATCH_STACK_RELEASE_BLOCK(GfxCoord);
}

/// The model `gMp5a5WeaponModel`: generated from its asset (weapons/mp5a5/model) by tools/buildsteps/gen_model_source.py.
#include "assets/weapons/mp5a5/model.inc"

/// The player's clips this package carries, in the package's order, and its animation bank (weapons/mp5a5/bank):
/// generated from the clip and bank assets by tools/buildsteps/gen_animation_source.py.
#if WEAPON_INDEX == 0x1e
#include "assets/weapons/mp5a5/clips.inc"
#elif WEAPON_INDEX == 0x1f
#include "assets/weapons/mp5a5/p1_clips.inc"
#elif WEAPON_INDEX == 0x20
#include "assets/weapons/mp5a5/p2_clips.inc"
#else
#error "WEAPON_INDEX selects no package of this source"
#endif
#include "assets/weapons/mp5a5/bank.inc"

PACKAGE_ALIASES
