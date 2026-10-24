#include "weapons/m4a1.h"

#include "config.h"

#include "common.h"
#include "types.h"

#include "gameplay/items.h"
#include "gameplay/player_actor.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/mc_types.h"
#include "main/scratch.h"
#include "main/session_types.h"
#include "main/task_types.h"

#include "weapons/weapon.h"
#include "weapons/sounds.h"

#include "../shared/attack.h"

#include "assets/engine/animation/player_schema.h"

/// Manifest-selected weapon index, equal to its inventory item ID minus 0x7F.
///
/// Required integer preprocessor expression: 16 M4A1, 20 M4A1(+1), 21 M4A1(+2).
/// Selects the firing sound bank and rifle-flash muzzle/casing profile;
/// `WEAPON_ITEM_ID` converts it to the weapon item whose load is consumed.
#ifndef WEAPON_INDEX
#error "WEAPON_INDEX is a per-package build parameter"
#endif

void m4a1AttackState(Task* playerTask)
{
    enum {
        M4A1_PHASE_PREPARE    = 0,
        M4A1_PHASE_WAIT_READY = 1,
        M4A1_PHASE_BURST      = 2,
        M4A1_PHASE_RECOVER    = 3,
    };
    GameActor* actor;
    GfxCoord*  rootCoord;
    GfxCoord*  impactCoord;
    s32        readyBlendFrames;
    s32        shotDelay;
    // Preserve the halfword shot count before storing it in actionValue.
    s16 burstRounds;

    impactCoord = SCRATCH_STACK_RESERVE_BLOCK(GfxCoord);
    actor       = playerTask->work;
    rootCoord   = playerTask->extra.tmd->coords;
    switch (actor->statePhase) {
        case M4A1_PHASE_PREPARE:
            actor->state             = PLAYER_ACTOR_STATE_ATTACK;
            actor->mode              = GAME_ACTOR_MODE_NORMAL;
            actor->turnRateIndex     = 0;
            actor->animationState    = 0;
            actor->statePhase        = M4A1_PHASE_WAIT_READY;
            actor->rumblePosted      = 0;
            actor->stateTimer        = 0;
            actor->attackCancelTicks = M4A1_CANCEL_FRAMES;
            burstRounds              = 1;
            if (actor->attackButton == PLAYER_ACTOR_ATTACK_BUTTON_PRIMARY) {
                burstRounds = M4A1_BURST_ROUNDS;
            }
            actor->actionValue = burstRounds;
            playerActorSetWeaponAttackFlags(playerTask, 0, actor->attackButton == PLAYER_ACTOR_ATTACK_BUTTON_PRIMARY);
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= (WORLD_COLLISION_BODY_CLIP_TO_GRID_CONTACT | WORLD_COLLISION_BODY_SINGLE_CONTACT);
            readyBlendFrames                                      = M4A1_READY_BLEND_FRAMES;
            if (((u16)actor->movementMode | actor->turnSign) != 0) {
                readyBlendFrames = M4A1_MOVING_READY_BLEND_FRAMES;
            }
            playerActorPlayChildSlotsWithBlend(playerTask, PLAYER_ANIMATION_AIM_IDLE, 0, readyBlendFrames);
            actor->movementMode = GAME_ACTOR_MOVEMENT_STOPPED;
            break;
        case M4A1_PHASE_WAIT_READY:
            if (animationGetCurrentRecord(&actor->animationContext, actor->animationSlots + 1) !=
                NULL) {
                actor->statePhase++;
            }
            break;
        case M4A1_PHASE_BURST:
            // Enable contacts for one dispatch and resolve each round on the next.
            if (actor->actionValue != 0) {
                shotDelay = actor->stateTimer;
                if (shotDelay == 0) {
                    actor->actionValue--;
                    actor->stateTimer                                     = M4A1_SHOT_DELAY_FRAMES;
                    actor->rumblePosted                                   = 0;
                    actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
                    equipmentConsumeWeaponLoad(WEAPON_ITEM_ID(WEAPON_INDEX), EQUIPMENT_WEAPON_LOAD_CONSUME_PRIMARY);
                    if (playerActorQueryWeaponLoads(PLAYER_ACTOR_WEAPON_LOAD_PRIMARY) == 0) {
                        actor->actionValue = 0;
                    }
                    worldCoordPlaySound(playerTask->extra.tmd->coords, M4A1_SOUND(WEAPON_SOUND_SLOT_SHOT), 1);
                    effectSpawn(EFFECT_RIFLE_MUZZLE_FLASH,
                                actor->equipmentTasks[1]->extra.tmd->coords,
                                WEAPON_INDEX, NULL);
                    playerActorPlayChildSlotsWithBlend(playerTask, PLAYER_ANIMATION_FIRE, 0, 2);
                    break;
                }
                actor->stateTimer = shotDelay - 1;
                if (shotDelay - 1 == M4A1_IMPACT_DELAY_FRAMES) {
                    _weaponResolveShotImpact(actor, rootCoord, impactCoord);
                }
            } else {
                // Resolve the final impact before entering recovery.
                actor->statePhase = M4A1_PHASE_RECOVER;
                _weaponResolveShotImpact(actor, rootCoord, impactCoord);
            }
            break;
        case M4A1_PHASE_RECOVER:
            if (actor->attackCancelTicks != 0) {
                actor->attackCancelTicks--;
            }
            if (_weaponIsArmSlotAdvancing(playerTask) == 0 ||
                ((actor->padHeld & actor->actionPadMask) != 0 && actor->attackCancelTicks == 0)) {
                actor->attackControl.cooldownTicks = M4A1_RECOVERY_COOLDOWN_FRAMES;
                playerActorFinishWeaponAttack(playerTask);
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BLOCK(GfxCoord);
}

/// The model `gM4a1RifleModel`: generated from its asset (weapons/m4a1/model) by tools/buildsteps/gen_model_source.py.
#include "assets/weapons/m4a1/model.inc"

/// The player's clips this package carries, in the package's order, and its animation bank (weapons/m4a1/bank):
/// generated from the clip and bank assets by tools/buildsteps/gen_animation_source.py.
#if WEAPON_INDEX == 0x10
#include "assets/weapons/m4a1/clips.inc"
#elif WEAPON_INDEX == 0x14
#include "assets/weapons/m4a1/p1_clips.inc"
#elif WEAPON_INDEX == 0x15
#include "assets/weapons/m4a1/p2_clips.inc"
#else
#error "WEAPON_INDEX selects no package of this source"
#endif
#include "assets/weapons/m4a1/bank.inc"

PACKAGE_ALIASES
