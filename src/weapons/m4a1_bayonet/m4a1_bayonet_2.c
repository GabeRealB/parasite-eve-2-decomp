#include "weapons/m4a1_bayonet.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "types.h"

#include "gameplay/animation.h"
#include "gameplay/effects.h"
#include "gameplay/items.h"
#include "gameplay/player_actor.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
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

/// One-based weapon index passed to the resident use counter.
enum { M4A1_BAYONET_WEAPON_ID = 26 };

#include "weapons/weapon.h"

enum { M4A1_BAYONET_IMPACT_SOUND = SOUND_COMMON(0x17) };

/// Ends M4A1 bayonet rifle-shot contact tests and requests the chosen surface's impact effect and sound.
///
/// Clears only the weapon body's grid/pair enables; keeps it linked and its
/// complete six-entry contact table intact. Requires live player/room state,
/// initialized contacts and `rootCoord->workm` in the contacts' view frame.
/// Enemy-body hits suppress surface selection. A selected eligible grid point
/// receives 0..7 units of XYZ jitter; only `impactCoord`'s three cached
/// translation words are written. Failure leaves that temporary node untouched.
/// On success, requests the common impact sound synchronously and latches
/// combat noise. Scratch/GTE requirements follow the picker and sound APIs;
/// this helper neither releases nor retains the caller's writable impact node.
static inline void _m4a1BayonetResolveShotImpact(GameActor* actor, const GfxCoord* rootCoord, GfxCoord* impactCoord)
{
    enum { M4A1_BAYONET_IMPACT_SIGNAL_NOISE = 1 };

    actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
    if (playerActorSpawnWeaponImpact(actor->weaponContacts, rootCoord, impactCoord) != 0) {
        worldCoordPlaySound(impactCoord, M4A1_BAYONET_IMPACT_SOUND, M4A1_BAYONET_IMPACT_SIGNAL_NOISE);
    }
}

void m4a1BayonetAttackState(Task* playerTask)
{
    enum {
        M4A1_BAYONET_PHASE_PREPARE             = 0,
        M4A1_BAYONET_PHASE_WAIT_READY          = 1,
        M4A1_BAYONET_PHASE_SELECT_ATTACK       = 2,
        M4A1_BAYONET_PHASE_BURST               = 3,
        M4A1_BAYONET_PHASE_IMPACT              = 4,
        M4A1_BAYONET_PHASE_THRUST_WINDUP       = 5,
        M4A1_BAYONET_PHASE_THRUST_CONTACT      = 6,
        M4A1_BAYONET_PHASE_RECOVER             = 7,
        M4A1_BAYONET_PLAYER_ATTACK_STATE       = 4,
        M4A1_BAYONET_ANIMATION_READY           = 9,
        M4A1_BAYONET_ANIMATION_PRIMARY         = 0xA,
        M4A1_BAYONET_READY_BLEND_FRAMES        = 1,
        M4A1_BAYONET_MOVING_READY_BLEND_FRAMES = 8,
        M4A1_BAYONET_ANIMATION_THRUST          = 0xB,
        M4A1_BAYONET_BURST_ROUNDS              = 3,
        M4A1_BAYONET_SHOT_DELAY_FRAMES         = 3,
        M4A1_BAYONET_IMPACT_DELAY_FRAMES       = 2,
        M4A1_BAYONET_BURST_CANCEL_FRAMES       = 9,
        M4A1_BAYONET_THRUST_CANCEL_FRAMES      = 0xA,
        M4A1_BAYONET_THRUST_WINDUP_FRAMES      = 0x12,
        M4A1_BAYONET_THRUST_CONTACT_FRAMES     = 0xA,
        M4A1_BAYONET_THRUST_COOLDOWN_FRAMES    = 0x1C,
        M4A1_BAYONET_RECOVERY_COOLDOWN_FRAMES  = 0xC,
        M4A1_BAYONET_THRUST_REACH              = 0x340,
        M4A1_BAYONET_ATTACK_KEY_BASE           = 0x21A00,
        M4A1_BAYONET_THRUST_KEY                = 0x21A1D,
        M4A1_BAYONET_FIRE_SOUND                = SOUND_WEAPON(26, 4),
        M4A1_BAYONET_CONTACT_SOUND             = SOUND_WEAPON(26, 5),
        M4A1_BAYONET_THRUST_SOUND              = SOUND_WEAPON(26, 6),
    };
    GameActor*             actor;
    GfxCoord*              rootCoord;
    GfxCoord*              impactCoord;
    WorldCollisionCapsule* weaponCapsule;
    EffectWork*            trailWork;
    s32                    readyBlendFrames;
    s32                    shotDelay;
    s16                    thrustTicksLeft;

    actor         = playerTask->work;
    rootCoord     = playerTask->extra.tmd->coords;
    weaponCapsule = &actor->weaponShape;
    impactCoord   = SCRATCH_STACK_RESERVE_BLOCK(GfxCoord);
    switch (actor->statePhase) {
        case M4A1_BAYONET_PHASE_PREPARE:
            readyBlendFrames      = M4A1_BAYONET_READY_BLEND_FRAMES;
            actor->state          = M4A1_BAYONET_PLAYER_ATTACK_STATE;
            actor->mode           = GAME_ACTOR_MODE_NORMAL;
            actor->turnRateIndex  = 0;
            actor->animationState = 0;
            actor->statePhase++;
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= WORLD_COLLISION_BODY_CLIP_TO_GRID_CONTACT;
            if (((u16)actor->movementMode | actor->turnSign) != 0) {
                readyBlendFrames = M4A1_BAYONET_MOVING_READY_BLEND_FRAMES;
            }
            playerActorPlayChildSlotsWithBlend(playerTask, M4A1_BAYONET_ANIMATION_READY, 0, readyBlendFrames);
            actor->movementMode = 0;
            break;
        case M4A1_BAYONET_PHASE_WAIT_READY:
            if (animationGetCurrentRecord(&actor->animationContext, actor->animationSlots + 1) !=
                NULL) {
                actor->statePhase++;
            }
            break;
        case M4A1_BAYONET_PHASE_SELECT_ATTACK:
            // Input selects a timed burst or the separate bayonet contact window.
            actor->rumblePosted = 0;
            if (actor->attackButton & PLAYER_ACTOR_ATTACK_BUTTON_PRIMARY) {
                actor->statePhase                                     = M4A1_BAYONET_PHASE_BURST;
                actor->stateTimer                                     = 0;
                actor->attackCancelTicks                              = M4A1_BAYONET_BURST_CANCEL_FRAMES;
                actor->actionValue                                    = M4A1_BAYONET_BURST_ROUNDS;
                actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].key    = gPlayerStatus.weaponSlotItem | M4A1_BAYONET_ATTACK_KEY_BASE;
                actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= WORLD_COLLISION_BODY_SINGLE_CONTACT;
                playerActorSetWeaponAttackFlags(playerTask, 0, 1);
            } else if (actor->attackButton & PLAYER_ACTOR_ATTACK_BUTTON_SECONDARY) {
                actor->statePhase        = M4A1_BAYONET_PHASE_THRUST_WINDUP;
                actor->attackCancelTicks = M4A1_BAYONET_THRUST_CANCEL_FRAMES;
                actor->actionValue       = M4A1_BAYONET_THRUST_WINDUP_FRAMES;
                weaponRecordUse(M4A1_BAYONET_WEAPON_ID);
                actor->attackControl.cooldownTicks                    = M4A1_BAYONET_THRUST_COOLDOWN_FRAMES;
                weaponCapsule->ends[0].vz                             = weaponCapsule->ends[1].vz + M4A1_BAYONET_THRUST_REACH;
                actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].key    = M4A1_BAYONET_THRUST_KEY;
                actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_SINGLE_CONTACT);
                playerActorSetWeaponAttackFlags(playerTask, 0, 0);
                playerActorPlayChildSlotsWithBlend(playerTask, M4A1_BAYONET_ANIMATION_THRUST, 1, 3);
                break;
            }
            /* fallthrough */
        case M4A1_BAYONET_PHASE_BURST:
            if (actor->actionValue != 0) {
                shotDelay = actor->stateTimer;
                if (shotDelay == 0) {
                    actor->actionValue--;
                    actor->stateTimer                                     = M4A1_BAYONET_SHOT_DELAY_FRAMES;
                    actor->rumblePosted                                   = 0;
                    weaponCapsule->ends[0].vz                             = weaponCapsule->ends[1].vz + D_80112F60[M4A1_BAYONET_WEAPON_ID];
                    actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
                    equipmentConsumeWeaponLoad(WEAPON_ITEM(M4A1_BAYONET_WEAPON_ID), EQUIPMENT_WEAPON_LOAD_CONSUME_PRIMARY);
                    if (playerActorQueryWeaponLoads(PLAYER_ACTOR_WEAPON_LOAD_PRIMARY) == 0) {
                        actor->actionValue = 0;
                    }
                    worldCoordPlaySound(playerTask->extra.tmd->coords, M4A1_BAYONET_FIRE_SOUND, 1);
                    effectSpawn(EFFECT_RIFLE_MUZZLE_FLASH,
                                actor->equipmentTasks[1]->extra.tmd->coords,
                                M4A1_BAYONET_WEAPON_ID, NULL);
                    playerActorPlayChildSlotsWithBlend(playerTask, M4A1_BAYONET_ANIMATION_PRIMARY, 0, 2);
                    break;
                }
                actor->stateTimer = shotDelay - 1;
                if (shotDelay - 1 == M4A1_BAYONET_IMPACT_DELAY_FRAMES) {
                    _m4a1BayonetResolveShotImpact(actor, rootCoord, impactCoord);
                }
                break;
            }
            /* fallthrough */
        case M4A1_BAYONET_PHASE_IMPACT:
            actor->statePhase = M4A1_BAYONET_PHASE_RECOVER;
            _m4a1BayonetResolveShotImpact(actor, rootCoord, impactCoord);
            break;
        case M4A1_BAYONET_PHASE_THRUST_WINDUP:
        case M4A1_BAYONET_PHASE_THRUST_CONTACT:
            thrustTicksLeft    = (u16)actor->actionValue - 1;
            actor->actionValue = thrustTicksLeft;
            if (thrustTicksLeft == 0) {
                if (actor->statePhase == M4A1_BAYONET_PHASE_THRUST_WINDUP) {
                    actor->statePhase++;
                    actor->actionValue                                    = M4A1_BAYONET_THRUST_CONTACT_FRAMES;
                    actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
                    worldCoordPlaySound(playerTask->extra.tmd->coords, M4A1_BAYONET_THRUST_SOUND, 0);
                    trailWork = effectSpawn(EFFECT_M4A1_BAYONET_TRAIL,
                                            actor->equipmentTasks[1]->extra.tmd->coords,
                                            M4A1_BAYONET_WEAPON_ID, NULL);
                    if (trailWork != NULL) {
                        taskReparent(actor->equipmentTasks[1], trailWork->task);
                    }
                } else {
                    actor->statePhase                                     = M4A1_BAYONET_PHASE_RECOVER;
                    actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
                }
            }
            if (worldCollisionCountContactsByKind(actor->weaponContacts, WORLD_COLLISION_CONTACT_ENEMY_BODY) != 0) {
                worldCoordPlaySound(playerTask->extra.tmd->coords, M4A1_BAYONET_CONTACT_SOUND, 0);
            }
            break;
        case M4A1_BAYONET_PHASE_RECOVER:
            if (actor->attackCancelTicks != 0) {
                actor->attackCancelTicks--;
            }
            if (playerActorIsSlotAdvancingLinearly(playerTask, D_80112E04[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId][1], 0, 0) == 0 ||
                ((actor->padHeld & actor->actionPadMask) != 0 && actor->attackCancelTicks == 0)) {
                actor->attackControl.cooldownTicks = M4A1_BAYONET_RECOVERY_COOLDOWN_FRAMES;
                playerActorFinishWeaponAttack(playerTask);
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BLOCK(GfxCoord);
}
