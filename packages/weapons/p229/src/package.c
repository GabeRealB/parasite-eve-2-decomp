#include "p229.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include <core/pe2.h>

#include <gameplay/effects.h>
#include <gameplay/items.h>
#include <gameplay/player_actor.h>
#include <gameplay/room_effects.h>
#include <gameplay/scene_runtime.h>
#include <gameplay/world_coords.h>
#include <gameplay/player_animation.h>
#include <gameplay/world_collision_types.h>

#include <engine/mc_types.h>
#include <engine/scratch.h>
#include <engine/session_types.h>
#include <engine/task.h>
#include <engine/task_types.h>
#include <engine/wipsys.h>
#include <engine/wipsys_types.h>
#include <engine/sound_ids.h>

#include <weapons/weapon/weapon.h>
#include <weapons/weapon/sounds.h>
#include <weapons/effects/muzzle_flash/muzzle_flash.h>
#include <weapons/weapon/weapon_index.h>

PE2_EDITOR_LABEL("Attack")
/// Attack tuning, in dispatch ticks.
PE2_OPTION_DEF(READY_BLEND_FRAMES, int, 1)
PE2_OPTION_DEF(MOVING_READY_BLEND_FRAMES, int, 5)
PE2_OPTION_DEF(SECONDARY_SPREAD_RADIUS, int, 0xC00)
PE2_OPTION_DEF(PRIMARY_CANCEL_FRAMES, int, 0xA)
PE2_OPTION_DEF(SECONDARY_CANCEL_FRAMES, int, 6)
PE2_OPTION_DEF(SECONDARY_COOLDOWN_FRAMES, int, 0x12)
PE2_OPTION_DEF(PRIMARY_COOLDOWN_FRAMES, int, 0xC)

PE2_EDITOR_LABEL("Model")
/// The model held in the player's weapon hand.
PE2_BIND(model, model)
PE2_BIND_ARG(model, _symbol, gP229WeaponModel)

PE2_EDITOR_LABEL("Animation")
/// The player's clips while it is equipped, by slot.
PE2_BIND(bank, animation_bank)
PE2_BIND_ARG(bank, _symbol, gP229PlayerAnimationBank)

/// A sound of the P229's bank, by slot (`WEAPON_SOUND_SLOT_*`, or one below).
#define SOUND(slot) SOUND_WEAPON(WEAPON_INDEX_P229, (slot))

enum {
    SOUND_SECONDARY_SHOT = 5, // A round of the second load
};

/// Attack tuning and ids; times in dispatch ticks.
enum {
    PRIMARY_KEY_BASE = WORLD_COLLISION_CONTACT_ATTACK | (WEAPON_INDEX_P229 << 8),
    SECONDARY_KEY    = WORLD_COLLISION_CONTACT_ATTACK | (WEAPON_INDEX_P229 << 8) | 0x16,
};

/// Muzzle offset of the P229, in the firing hand's coordinate frame.
static SVECTOR _gMuzzleFlashOffset = { 0, 0x140, 0x20, 0 };

/// Retained streak directions shared by every live flash in this weapon package.
///
/// Angles use 4096 units per turn, one random direction in each quadrant.
/// Each newly initialized flash replaces the directions older flashes read.
static s16 _gMuzzleFlashAngles[MUZZLE_FLASH_STREAK_COUNT];

#include <weapons/effects/muzzle_flash/task.inc.c>
#include <weapons/attack/attack.h>

DEFINE_ALIAS(_muzzleFlashTask, p229MuzzleFlashTask);

/// The model and the player's clips: here, as the package stores them after the data above.
PE2_VALUE(model)
PE2_VALUE(bank)

void p229AttackState(Task* playerTask)
{
    enum {
        PHASE_PREPARE          = 0,
        PHASE_WAIT_READY       = 1,
        PHASE_FIRE             = 2,
        PHASE_PRIMARY_IMPACT   = 3,
        PHASE_SECONDARY_IMPACT = 4,
        PHASE_RECOVER          = 5,
    };
    GameActor*             actor;
    GfxCoord*              rootCoord;
    GfxCoord*              impactCoord;
    WorldCollisionCapsule* weaponShape;
    EffectWork*            flashWork;
    s32                    readyBlendFrames;
    s16                    cooldownFrames;

    actor       = playerTask->work;
    rootCoord   = playerTask->extra.tmd->coords;
    weaponShape = &actor->weaponShape;
    impactCoord = SCRATCH_STACK_RESERVE_BLOCK(GfxCoord);
    switch (actor->statePhase) {
        case PHASE_PREPARE:
            actor->state                                          = PLAYER_ACTOR_STATE_ATTACK;
            actor->mode                                           = GAME_ACTOR_MODE_NORMAL;
            actor->turnRateIndex                                  = 0;
            actor->animationState                                 = 0;
            actor->rumblePosted                                   = 0;
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
        case PHASE_FIRE:
            // Primary fire clips at its first contact; secondary fire keeps a wide shape.
            if (actor->attackButton == PLAYER_ACTOR_ATTACK_BUTTON_PRIMARY) {
                actor->statePhase                                     = PHASE_PRIMARY_IMPACT;
                actor->attackCancelTicks                              = PE2_OPTION(PRIMARY_CANCEL_FRAMES);
                actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].key    = gPlayerStatus.weaponSlotItem | PRIMARY_KEY_BASE;
                weaponShape->end0Radius                               = weaponShape->end1Radius;
                actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= WORLD_COLLISION_BODY_SINGLE_CONTACT;
                playerActorSetWeaponAttackFlags(playerTask, 0, 0);
                equipmentConsumeWeaponLoad(WEAPON_ITEM_ID(WEAPON_INDEX_P229), EQUIPMENT_WEAPON_LOAD_CONSUME_PRIMARY);
                worldCoordPlaySound(playerTask->extra.tmd->coords, SOUND(WEAPON_SOUND_SLOT_SHOT), 0);
                effectSpawn(EFFECT_HANDGUN_MUZZLE_FLASH,
                            actor->equipmentTasks[1]->extra.tmd->coords, WEAPON_INDEX_P229,
                            NULL);
                playerActorResetChildSlots(playerTask, PLAYER_ANIMATION_FIRE);
            } else {
                actor->statePhase                                     = PHASE_SECONDARY_IMPACT;
                actor->attackCancelTicks                              = PE2_OPTION(SECONDARY_CANCEL_FRAMES);
                actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].key    = SECONDARY_KEY;
                weaponShape->end0Radius                               = PE2_OPTION(SECONDARY_SPREAD_RADIUS);
                actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_SINGLE_CONTACT);
                playerActorSetWeaponAttackFlags(playerTask, 0, 1);
                equipmentConsumeWeaponLoad(WEAPON_ITEM_ID(WEAPON_INDEX_P229), EQUIPMENT_WEAPON_LOAD_CONSUME_SECONDARY);
                worldCoordPlaySound(playerTask->extra.tmd->coords, SOUND(SOUND_SECONDARY_SHOT), 0);
                flashWork = effectSpawn(EFFECT_P229_MUZZLE_FLASH,
                                        actor->equipmentTasks[1]->extra.tmd->coords, WEAPON_INDEX_P229,
                                        NULL);
                if (flashWork != NULL) {
                    taskReparent(actor->equipmentTasks[1], flashWork->task);
                }
            }
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
            break;
        case PHASE_PRIMARY_IMPACT:
        case PHASE_SECONDARY_IMPACT:
            if (actor->statePhase == PHASE_PRIMARY_IMPACT && playerActorSpawnWeaponImpact(actor->weaponContacts, rootCoord, impactCoord) != 0) {
                worldCoordPlaySound(impactCoord, SOUND_BULLET_IMPACT, 0);
            }
            actor->statePhase                                     = PHASE_RECOVER;
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
            /* fallthrough */
        case PHASE_RECOVER:
            if (actor->attackCancelTicks != 0) {
                actor->attackCancelTicks--;
            }
            if (_weaponIsArmSlotAdvancing(playerTask) == 0 ||
                ((actor->padHeld & actor->actionPadMask) != 0 && actor->attackCancelTicks == 0)) {
                cooldownFrames = PE2_OPTION(SECONDARY_COOLDOWN_FRAMES);
                if (actor->attackButton == PLAYER_ACTOR_ATTACK_BUTTON_PRIMARY) {
                    cooldownFrames = PE2_OPTION(PRIMARY_COOLDOWN_FRAMES);
                }
                actor->attackControl.cooldownTicks = cooldownFrames;
                playerActorFinishWeaponAttack(playerTask);
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BLOCK(GfxCoord);
}
