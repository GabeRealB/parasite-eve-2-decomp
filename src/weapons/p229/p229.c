#include "weapons/p229.h"

#include "config.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

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

/// Muzzle offset of the P229, in the firing hand's coordinate frame.
static SVECTOR _gMuzzleFlashOffset = { 0, 0x140, 0x20, 0 };

/// Retained streak directions shared by every live flash in this weapon package.
///
/// Angles use 4096 units per turn, one random direction in each quadrant.
/// Each newly initialized flash replaces the directions older flashes read.
static s16 _gMuzzleFlashAngles[MUZZLE_FLASH_STREAK_COUNT];

#include "../shared/muzzle_flash.inc.c"
#include "../shared/attack.h"

DEFINE_ALIAS(_muzzleFlashTask, p229MuzzleFlashTask);

#include "assets/engine/animation/player_schema.h"

void p229AttackState(Task* playerTask)
{
    enum {
        P229_PHASE_PREPARE          = 0,
        P229_PHASE_WAIT_READY       = 1,
        P229_PHASE_FIRE             = 2,
        P229_PHASE_PRIMARY_IMPACT   = 3,
        P229_PHASE_SECONDARY_IMPACT = 4,
        P229_PHASE_RECOVER          = 5,
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
        case P229_PHASE_PREPARE:
            actor->state                                          = PLAYER_ACTOR_STATE_ATTACK;
            actor->mode                                           = GAME_ACTOR_MODE_NORMAL;
            actor->turnRateIndex                                  = 0;
            actor->animationState                                 = 0;
            actor->rumblePosted                                   = 0;
            actor->statePhase                                    += 1;
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= WORLD_COLLISION_BODY_CLIP_TO_GRID_CONTACT;
            readyBlendFrames                                      = P229_READY_BLEND_FRAMES;
            if (((u16)actor->movementMode | actor->turnSign) != 0) {
                readyBlendFrames = P229_MOVING_READY_BLEND_FRAMES;
            }
            playerActorPlayChildSlotsWithBlend(playerTask, PLAYER_ANIMATION_AIM_IDLE, 0, readyBlendFrames);
            actor->movementMode = GAME_ACTOR_MOVEMENT_STOPPED;
            break;
        case P229_PHASE_WAIT_READY:
            if (animationGetCurrentRecord(&actor->animationContext, actor->animationSlots + 1) !=
                NULL) {
                actor->statePhase++;
            }
            break;
        case P229_PHASE_FIRE:
            // Primary fire clips at its first contact; secondary fire keeps a wide shape.
            if (actor->attackButton == PLAYER_ACTOR_ATTACK_BUTTON_PRIMARY) {
                actor->statePhase                                     = P229_PHASE_PRIMARY_IMPACT;
                actor->attackCancelTicks                              = P229_PRIMARY_CANCEL_FRAMES;
                actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].key    = gPlayerStatus.weaponSlotItem | P229_PRIMARY_KEY_BASE;
                weaponShape->end0Radius                               = weaponShape->end1Radius;
                actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= WORLD_COLLISION_BODY_SINGLE_CONTACT;
                playerActorSetWeaponAttackFlags(playerTask, 0, 0);
                equipmentConsumeWeaponLoad(WEAPON_ITEM_ID(WEAPON_INDEX_P229), EQUIPMENT_WEAPON_LOAD_CONSUME_PRIMARY);
                worldCoordPlaySound(playerTask->extra.tmd->coords, P229_SOUND(WEAPON_SOUND_SLOT_SHOT), 0);
                effectSpawn(EFFECT_HANDGUN_MUZZLE_FLASH,
                            actor->equipmentTasks[1]->extra.tmd->coords, WEAPON_INDEX_P229,
                            NULL);
                playerActorResetChildSlots(playerTask, PLAYER_ANIMATION_FIRE);
            } else {
                actor->statePhase                                     = P229_PHASE_SECONDARY_IMPACT;
                actor->attackCancelTicks                              = P229_SECONDARY_CANCEL_FRAMES;
                actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].key    = P229_SECONDARY_KEY;
                weaponShape->end0Radius                               = P229_SECONDARY_SPREAD_RADIUS;
                actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_SINGLE_CONTACT);
                playerActorSetWeaponAttackFlags(playerTask, 0, 1);
                equipmentConsumeWeaponLoad(WEAPON_ITEM_ID(WEAPON_INDEX_P229), EQUIPMENT_WEAPON_LOAD_CONSUME_SECONDARY);
                worldCoordPlaySound(playerTask->extra.tmd->coords, P229_SOUND(P229_SOUND_SECONDARY_SHOT), 0);
                flashWork = effectSpawn(EFFECT_P229_MUZZLE_FLASH,
                                        actor->equipmentTasks[1]->extra.tmd->coords, WEAPON_INDEX_P229,
                                        NULL);
                if (flashWork != NULL) {
                    taskReparent(actor->equipmentTasks[1], flashWork->task);
                }
            }
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
            break;
        case P229_PHASE_PRIMARY_IMPACT:
        case P229_PHASE_SECONDARY_IMPACT:
            if (actor->statePhase == P229_PHASE_PRIMARY_IMPACT && playerActorSpawnWeaponImpact(actor->weaponContacts, rootCoord, impactCoord) != 0) {
                worldCoordPlaySound(impactCoord, SOUND_BULLET_IMPACT, 0);
            }
            actor->statePhase                                     = P229_PHASE_RECOVER;
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
            /* fallthrough */
        case P229_PHASE_RECOVER:
            if (actor->attackCancelTicks != 0) {
                actor->attackCancelTicks--;
            }
            if (_weaponIsArmSlotAdvancing(playerTask) == 0 ||
                ((actor->padHeld & actor->actionPadMask) != 0 && actor->attackCancelTicks == 0)) {
                cooldownFrames = P229_SECONDARY_COOLDOWN_FRAMES;
                if (actor->attackButton == PLAYER_ACTOR_ATTACK_BUTTON_PRIMARY) {
                    cooldownFrames = P229_PRIMARY_COOLDOWN_FRAMES;
                }
                actor->attackControl.cooldownTicks = cooldownFrames;
                playerActorFinishWeaponAttack(playerTask);
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BLOCK(GfxCoord);
}

/// The held weapon model, `gP229WeaponModel`, with its arrays: generated from the asset
/// (weapons/p229/model.usda, textured through weapons/p229/binding) by tools/buildsteps/gen_model_source.py.
#include "assets/weapons/p229/model.inc"

/// The player's clips this package carries, in the package's order, and its animation bank: generated
/// from the clip assets (characters/aya) and the bank weapons/p229/bank by
/// tools/buildsteps/gen_animation_source.py.
#include "assets/weapons/p229/clips.inc"
#include "assets/weapons/p229/bank.inc"
