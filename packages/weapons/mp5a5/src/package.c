#include "mp5a5.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include <core/pe2.h>

#include <gameplay/effects.h>
#include <gameplay/items.h>
#include <gameplay/player_actor.h>
#include <gameplay/player_animation.h>
#include <gameplay/room_effects.h>
#include <gameplay/scene_runtime.h>
#include <gameplay/world_collision_types.h>
#include <gameplay/world_coords.h>

#include <engine/mc_types.h>
#include <engine/scratch.h>
#include <engine/session_types.h>
#include <engine/sound_ids.h>
#include <engine/task.h>
#include <engine/task_types.h>
#include <engine/wipsys.h>
#include <engine/wipsys_types.h>

#include <weapons/attack/attack.h>
#include <weapons/effects/muzzle_flash/muzzle_flash.h>
#include <weapons/weapon/sounds.h>
#include <weapons/weapon/weapon.h>

/// The weapon the package is (its index: the item it is, less 0x7F): 30 the MP5A5, 31 and 32 its upgrades. It keys its
/// contacts and its sounds, and picks its muzzle and casing.
PE2_OPTION_DEF(WEAPON_INDEX, int)

PE2_EDITOR_LABEL("Attack")
/// Ticks of blending into the ready pose, standing still and moving or turning.
PE2_OPTION_DEF(READY_BLEND_FRAMES, int, 1)
PE2_OPTION_DEF(MOVING_READY_BLEND_FRAMES, int, 5)
/// Ticks between a primary round's trigger and its impact.
PE2_OPTION_DEF(PRIMARY_SHOT_DELAY_FRAMES, int, 3)
/// The secondary load's spread, and the ticks its contact stays active and its cooldown lasts.
PE2_OPTION_DEF(SECONDARY_SPREAD_RADIUS, int, 0xC00)
PE2_OPTION_DEF(SECONDARY_ACTIVE_FRAMES, int, 0x12)
PE2_OPTION_DEF(SECONDARY_COOLDOWN_FRAMES, int, 0x12)

PE2_EDITOR_LABEL("Model")
/// The model held in the player's weapon hand.
PE2_BIND(model, model)
PE2_BIND_ARG(model, _symbol, gMp5a5WeaponModel)

PE2_EDITOR_LABEL("Animation")
/// The player's clips while it is equipped, by slot.
PE2_BIND(bank, animation_bank)
PE2_BIND_ARG(bank, _symbol, gMp5a5PlayerAnimationBank)

/// A sound of the weapon's bank, by slot (`WEAPON_SOUND_SLOT_*`, or one below).
#define SOUND(slot) SOUND_WEAPON(PE2_OPTION(WEAPON_INDEX), (slot))

enum {
    SOUND_SECONDARY_SHOT = 5,                                     // A round of the second load
    SECONDARY_KEY_BASE   = WORLD_COLLISION_CONTACT_ATTACK | 0x16, // The secondary contact's key, less the weapon
};

/// MP5A5 alternate-fire flash origin in integer weapon-root coordinates.
static SVECTOR _gMuzzleFlashOffset = { 0, 576, 64, 0 };

/// Retained streak directions shared by every live flash in this weapon package.
///
/// Angles use 4096 units per turn, one random direction in each quadrant.
/// Each newly initialized flash replaces the directions older flashes read.
static s16 _gMuzzleFlashAngles[MUZZLE_FLASH_STREAK_COUNT];

#include <weapons/effects/muzzle_flash/task.inc.c>

DEFINE_ALIAS(_muzzleFlashTask, mp5a5MuzzleFlashTask);

/// The model and the player's clips: here, as the package stores them after the data above.
PE2_VALUE(model)
PE2_VALUE(bank)

void mp5a5AttackState(Task* playerTask)
{
    enum {
        PHASE_PREPARE          = 0,
        PHASE_WAIT_READY       = 1,
        PHASE_ARM_SHOT         = 2,
        PHASE_SHOT_DELAY       = 3,
        PHASE_PRIMARY_IMPACT   = 4,
        PHASE_SECONDARY_IMPACT = 5,
        PHASE_RECOVER          = 6,
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
        case PHASE_PREPARE:
            actor->state                                          = PLAYER_ACTOR_STATE_ATTACK;
            actor->turnRateIndex                                  = 2;
            actor->mode                                           = GAME_ACTOR_MODE_NORMAL;
            actor->animationState                                 = 0;
            actor->stateTimer                                     = 1;
            actor->statePhase                                    += 1;
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= WORLD_COLLISION_BODY_CLIP_TO_GRID_CONTACT;
            readyBlendFrames                                      = PE2_OPTION(READY_BLEND_FRAMES);
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
        case PHASE_ARM_SHOT:
        fire:
            actor->statePhase   = PHASE_SHOT_DELAY;
            actor->rumblePosted = 0;
            /* fallthrough */
        case PHASE_SHOT_DELAY:
            if (--actor->stateTimer == 0) {
                weaponShape = &actor->weaponShape;
                // Primary fire clips at its first contact; secondary fire keeps a wide shape.
                if (actor->attackButton == PLAYER_ACTOR_ATTACK_BUTTON_PRIMARY) {
                    actor->statePhase                                     = PHASE_PRIMARY_IMPACT;
                    actor->stateTimer                                     = PE2_OPTION(PRIMARY_SHOT_DELAY_FRAMES);
                    actor->attackControl.cooldownTicks                    = 0;
                    actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].key    = gPlayerStatus.weaponSlotItem | WORLD_COLLISION_CONTACT_ATTACK | (PE2_OPTION(WEAPON_INDEX) << 8);
                    weaponShape->end0Radius                               = weaponShape->end1Radius;
                    actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= WORLD_COLLISION_BODY_SINGLE_CONTACT;
                    playerActorSetWeaponAttackFlags(playerTask, 0, 1);
                    worldCoordPlaySound(playerTask->extra.tmd->coords, SOUND(WEAPON_SOUND_SLOT_SHOT), 1);
                    effectSpawn(EFFECT_HANDGUN_MUZZLE_FLASH,
                                actor->equipmentTasks[1]->extra.tmd->coords,
                                PE2_OPTION(WEAPON_INDEX), NULL);
                    equipmentConsumeWeaponLoad(WEAPON_ITEM_ID(PE2_OPTION(WEAPON_INDEX)), EQUIPMENT_WEAPON_LOAD_CONSUME_PRIMARY);
                    playerActorPlayChildSlotsWithBlend(playerTask, PLAYER_ANIMATION_FIRE, 0, 2);
                } else {
                    actor->statePhase                                     = PHASE_SECONDARY_IMPACT;
                    actor->attackControl.cooldownTicks                    = PE2_OPTION(SECONDARY_COOLDOWN_FRAMES);
                    actor->stateTimer                                     = PE2_OPTION(SECONDARY_ACTIVE_FRAMES);
                    actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].key    = SECONDARY_KEY_BASE | (PE2_OPTION(WEAPON_INDEX) << 8);
                    weaponShape->end0Radius                               = PE2_OPTION(SECONDARY_SPREAD_RADIUS);
                    actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_SINGLE_CONTACT);
                    playerActorSetWeaponAttackFlags(playerTask, 0, 0);
                    worldCoordPlaySound(playerTask->extra.tmd->coords, SOUND(SOUND_SECONDARY_SHOT), 0);
                    equipmentConsumeWeaponLoad(WEAPON_ITEM_ID(PE2_OPTION(WEAPON_INDEX)), EQUIPMENT_WEAPON_LOAD_CONSUME_SECONDARY);
                    flashWork = effectSpawn(EFFECT_MP5A5_ALT_FIRE_MUZZLE_FLASH,
                                            actor->equipmentTasks[1]->extra.tmd->coords,
                                            PE2_OPTION(WEAPON_INDEX), NULL);
                    if (flashWork != NULL) {
                        taskReparent(actor->equipmentTasks[1], flashWork->task);
                    }
                }
                actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
            }
            break;
        case PHASE_PRIMARY_IMPACT:
        case PHASE_SECONDARY_IMPACT:
            if (actor->statePhase < PHASE_SECONDARY_IMPACT && playerActorSpawnWeaponImpact(actor->weaponContacts, rootCoord, impactCoord) != 0) {
                worldCoordPlaySound(impactCoord, SOUND_BULLET_IMPACT, 1);
            }
            actor->statePhase                                     = PHASE_RECOVER;
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
            /* fallthrough */
        case PHASE_RECOVER:
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
