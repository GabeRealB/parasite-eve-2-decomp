#include "weapons/m4a1_hammer.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "types.h"

#include "gameplay/animation.h"
#include "gameplay/items.h"
#include "gameplay/player_actor.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/scratch.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task_types.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "weapons/weapon.h"

void m4a1HammerAttackState(Task* playerTask)
{
    enum {
        M4A1_HAMMER_PHASE_PREPARE                = 0,
        M4A1_HAMMER_PHASE_WAIT_READY             = 1,
        M4A1_HAMMER_PHASE_SELECT_ATTACK          = 2,
        M4A1_HAMMER_PHASE_BURST                  = 3,
        M4A1_HAMMER_PHASE_IMPACT                 = 4,
        M4A1_HAMMER_PHASE_DISCHARGE              = 5,
        M4A1_HAMMER_PHASE_RECOVER                = 6,
        M4A1_HAMMER_PLAYER_ATTACK_STATE          = 4,
        M4A1_HAMMER_ANIMATION_READY              = 9,
        M4A1_HAMMER_ANIMATION_PRIMARY            = 0xA,
        M4A1_HAMMER_ANIMATION_SECONDARY          = 0xB,
        M4A1_HAMMER_READY_BLEND_FRAMES           = 1,
        M4A1_HAMMER_MOVING_READY_BLEND_FRAMES    = 8,
        M4A1_HAMMER_IMPACT_SOUND                 = SOUND_COMMON(0x17),
        M4A1_HAMMER_BURST_ROUNDS                 = 3,
        M4A1_HAMMER_BURST_CANCEL_FRAMES          = 9,
        M4A1_HAMMER_SECONDARY_CANCEL_FRAMES      = 0x1C,
        M4A1_HAMMER_BURST_DELAY_FRAMES           = 3,
        M4A1_HAMMER_RECOVERY_COOLDOWN_FRAMES     = 0xC,
        M4A1_HAMMER_RIFLE_RADIUS                 = 0x100,
        M4A1_HAMMER_DISCHARGE_RADIUS             = 0x400,
        M4A1_HAMMER_SECONDARY_ACTION_FRAMES      = 0x14,
        M4A1_HAMMER_RIFLE_KEY_BASE               = WORLD_COLLISION_CONTACT_ATTACK | (25 << 8),
        M4A1_HAMMER_DISCHARGE_KEY                = WORLD_COLLISION_CONTACT_ATTACK | (25 << 8) | 0x1C,
        M4A1_HAMMER_DISCHARGE_REACH              = 2560,
        M4A1_HAMMER_WEAPON_ID                    = 25,
        M4A1_HAMMER_PRIMARY_SOUND                = SOUND_WEAPON(25, 4),
        M4A1_HAMMER_DISCHARGE_SOUND              = SOUND_WEAPON(25, 5),
        M4A1_HAMMER_BURST_IMPACT_TICKS_LEFT      = 2,
        M4A1_HAMMER_SECONDARY_TURN_RATE_INDEX    = 2,
        M4A1_HAMMER_DISCHARGE_WINDUP_FRAMES      = 3,
        M4A1_HAMMER_DISCHARGE_CONTACT_TICKS_LEFT = 1,
    };
    GameActor*             actor;
    GfxCoord*              rootCoord;
    GfxCoord*              impactCoord;
    WorldCollisionCapsule* weaponShape;
    Task*                  glowTask;
    s32                    readyBlendFrames;
    s32                    phaseDelay;
    u16                    collisionFlags;

    actor       = playerTask->work;
    rootCoord   = playerTask->extra.tmd->coords;
    weaponShape = &actor->weaponShape;
    impactCoord = SCRATCH_STACK_RESERVE_BLOCK(GfxCoord);
    switch (actor->statePhase) {
        case M4A1_HAMMER_PHASE_PREPARE:
            readyBlendFrames                                      = M4A1_HAMMER_READY_BLEND_FRAMES;
            actor->state                                          = M4A1_HAMMER_PLAYER_ATTACK_STATE;
            actor->mode                                           = GAME_ACTOR_MODE_NORMAL;
            actor->animationState                                 = 0;
            actor->statePhase                                    += readyBlendFrames;
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= WORLD_COLLISION_BODY_CLIP_TO_GRID_CONTACT;
            if (((u16)actor->movementMode | actor->turnSign) != 0) {
                readyBlendFrames = M4A1_HAMMER_MOVING_READY_BLEND_FRAMES;
            }
            playerActorPlayChildSlotsWithBlend(playerTask, M4A1_HAMMER_ANIMATION_READY, 0, readyBlendFrames);
            actor->movementMode = 0;
            break;
        case M4A1_HAMMER_PHASE_WAIT_READY:
            if (animationGetCurrentRecord(&actor->animationContext, actor->animationSlots + 1) !=
                NULL) {
                actor->statePhase++;
            }
            break;
        case M4A1_HAMMER_PHASE_SELECT_ATTACK:
            actor->rumblePosted = 0;
            if (actor->attackButton & PLAYER_ACTOR_ATTACK_BUTTON_PRIMARY) {
                actor->statePhase                                     = M4A1_HAMMER_PHASE_BURST;
                actor->turnRateIndex                                  = 0;
                actor->stateTimer                                     = 0;
                actor->attackCancelTicks                              = M4A1_HAMMER_BURST_CANCEL_FRAMES;
                actor->actionValue                                    = M4A1_HAMMER_BURST_ROUNDS;
                actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].key    = gPlayerStatus.weaponSlotItem | M4A1_HAMMER_RIFLE_KEY_BASE;
                weaponShape->end0Radius                               = M4A1_HAMMER_RIFLE_RADIUS;
                weaponShape->end1Radius                               = M4A1_HAMMER_RIFLE_RADIUS;
                weaponShape->ends[0].vz                               = weaponShape->ends[1].vz + D_80112F60[0x19];
                actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= WORLD_COLLISION_BODY_SINGLE_CONTACT;
                playerActorSetWeaponAttackFlags(playerTask, 0, 1);
            } else if (actor->attackButton & PLAYER_ACTOR_ATTACK_BUTTON_SECONDARY) {
                actor->statePhase                                     = M4A1_HAMMER_PHASE_DISCHARGE;
                actor->turnRateIndex                                  = M4A1_HAMMER_SECONDARY_TURN_RATE_INDEX;
                actor->attackCancelTicks                              = M4A1_HAMMER_SECONDARY_CANCEL_FRAMES;
                actor->actionValue                                    = M4A1_HAMMER_SECONDARY_ACTION_FRAMES;
                actor->stateTimer                                     = M4A1_HAMMER_DISCHARGE_WINDUP_FRAMES;
                actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].key    = M4A1_HAMMER_DISCHARGE_KEY;
                weaponShape->end0Radius                               = M4A1_HAMMER_DISCHARGE_RADIUS;
                weaponShape->end1Radius                               = M4A1_HAMMER_DISCHARGE_RADIUS;
                weaponShape->ends[0].vz                               = weaponShape->ends[1].vz + M4A1_HAMMER_DISCHARGE_REACH;
                actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_SINGLE_CONTACT);
                playerActorSetWeaponAttackFlags(playerTask, 0, 0);
                glowTask = actor->weaponEffectTask;
                if (glowTask != NULL) {
                    glowTask->spawnArg1.value = M4A1_HAMMER_GLOW_CHARGED;
                }
                equipmentConsumeWeaponLoad(WEAPON_ITEM(M4A1_HAMMER_WEAPON_ID), EQUIPMENT_WEAPON_LOAD_CONSUME_SECONDARY);
                worldCoordPlaySound(playerTask->extra.tmd->coords, M4A1_HAMMER_DISCHARGE_SOUND, 1);
                playerActorPlayChildSlotsWithBlend(playerTask, M4A1_HAMMER_ANIMATION_SECONDARY, 0, 2);
                break;
            }
            /* fallthrough */
        case M4A1_HAMMER_PHASE_BURST:
            if (actor->actionValue != 0) {
                phaseDelay = actor->stateTimer;
                if (phaseDelay == 0) {
                    actor->actionValue--;
                    actor->stateTimer                                     = M4A1_HAMMER_BURST_DELAY_FRAMES;
                    actor->rumblePosted                                   = 0;
                    actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
                    equipmentConsumeWeaponLoad(WEAPON_ITEM(M4A1_HAMMER_WEAPON_ID), EQUIPMENT_WEAPON_LOAD_CONSUME_PRIMARY);
                    if (playerActorQueryWeaponLoads(PLAYER_ACTOR_WEAPON_LOAD_PRIMARY) == 0) {
                        actor->actionValue = 0;
                    }
                    worldCoordPlaySound(playerTask->extra.tmd->coords, M4A1_HAMMER_PRIMARY_SOUND, 1);
                    effectSpawn(EFFECT_RIFLE_MUZZLE_FLASH,
                                actor->equipmentTasks[1]->extra.tmd->coords,
                                M4A1_HAMMER_WEAPON_ID, NULL);
                    playerActorPlayChildSlotsWithBlend(playerTask, M4A1_HAMMER_ANIMATION_PRIMARY, 0, 2);
                    break;
                }
                actor->stateTimer = phaseDelay - 1;
                if (phaseDelay - 1 == M4A1_HAMMER_BURST_IMPACT_TICKS_LEFT) {
                    actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
                    if (playerActorSpawnWeaponImpact(actor->weaponContacts, rootCoord, impactCoord) != 0) {
                        worldCoordPlaySound(impactCoord, M4A1_HAMMER_IMPACT_SOUND, 1);
                    }
                }
                break;
            }
            /* fallthrough */
        case M4A1_HAMMER_PHASE_IMPACT:
            actor->statePhase                                     = M4A1_HAMMER_PHASE_RECOVER;
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
            if (playerActorSpawnWeaponImpact(actor->weaponContacts, rootCoord, impactCoord) != 0) {
                worldCoordPlaySound(impactCoord, M4A1_HAMMER_IMPACT_SOUND, 1);
            }
            break;
        case M4A1_HAMMER_PHASE_DISCHARGE:
            // The discharge enables contacts for one tick before recovery.
            phaseDelay        = actor->stateTimer - 1;
            actor->stateTimer = phaseDelay;
            if (phaseDelay == M4A1_HAMMER_DISCHARGE_CONTACT_TICKS_LEFT) {
                collisionFlags                                       = actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags | (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
                actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags = collisionFlags;
            } else if (phaseDelay == 0) {
                actor->statePhase = M4A1_HAMMER_PHASE_RECOVER;
                if (playerActorQueryWeaponLoads(PLAYER_ACTOR_WEAPON_LOAD_SECONDARY) == 0) {
                    actor->weaponEffectTask->spawnArg1.value = M4A1_HAMMER_GLOW_OFF;
                }
                collisionFlags                                       = actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
                actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags = collisionFlags;
            }
            /* fallthrough */
        case M4A1_HAMMER_PHASE_RECOVER:
            if (actor->attackCancelTicks != 0) {
                actor->attackCancelTicks--;
            }
            if (playerActorIsSlotAdvancingLinearly(playerTask, D_80112E04[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId][1], 0, 0) == 0 ||
                ((actor->padHeld & actor->actionPadMask) != 0 && actor->attackCancelTicks == 0)) {
                actor->attackControl.cooldownTicks = M4A1_HAMMER_RECOVERY_COOLDOWN_FRAMES;
                playerActorFinishWeaponAttack(playerTask);
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BLOCK(GfxCoord);
}
