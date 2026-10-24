#include "weapons/pa3.h"

#include "config.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "gameplay/items.h"
#include "gameplay/player_actor.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/mc_types.h"
#include "main/scratch.h"
#include "main/session_types.h"
#include "main/task_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "weapons/weapon.h"
#include "weapons/sounds.h"
#include "../shared/attack.h"

#include "assets/engine/animation/player_schema.h"

/// Manifest-selected weapon index, equal to its inventory item ID minus 0x7F.
///
/// Required integer preprocessor expression: 13 PA3 or 14 SP12. It selects
/// assets, the firing/pump sound bank and the shotgun flash profile;
/// `WEAPON_ITEM_ID` converts it to the weapon item whose load is consumed.
#ifndef WEAPON_INDEX
#error "WEAPON_INDEX is a per-package build parameter"
#elif WEAPON_INDEX != WEAPON_INDEX_PA3 && WEAPON_INDEX != WEAPON_INDEX_SP12
#error "WEAPON_INDEX must select a PA3 or SP12 build"
#endif

/// Recovery dispatch ticks before held action buttons may finish a shot early.
///
/// Required nonnegative integer preprocessor expression fitting s8: 32 PA3,
/// 36 SP12. Seeds `attackCancelTicks` during preparation, but counts down only
/// from impact through pump delay and recovery. Animation completion can end
/// the attack before this countdown expires.
#ifndef PA3_ATTACK_CANCEL_TICKS
#error "PA3_ATTACK_CANCEL_TICKS is a per-package build parameter"
#elif PA3_ATTACK_CANCEL_TICKS < 0 || PA3_ATTACK_CANCEL_TICKS > 127
#error "PA3_ATTACK_CANCEL_TICKS must fit a nonnegative s8"
#endif

void pa3AttackState(Task* playerTask)
{
    enum {
        PA3_PHASE_PREPARE    = 0,
        PA3_PHASE_WAIT_READY = 1,
        PA3_PHASE_FIRE       = 2,
        PA3_PHASE_IMPACT     = 3,
        PA3_PHASE_PUMP_DELAY = 4,
        PA3_PHASE_RECOVER    = 5,
    };
    GameActor* actor;
    GfxCoord*  rootCoord;
    GfxCoord*  impactCoord;
    s32        readyBlendFrames;
    s32        gridImpact;

    impactCoord = SCRATCH_STACK_RESERVE_BLOCK(GfxCoord);
    actor       = playerTask->work;
    rootCoord   = playerTask->extra.tmd->coords;
    switch (actor->statePhase) {
        case PA3_PHASE_PREPARE:
            actor->state             = PLAYER_ACTOR_STATE_ATTACK;
            actor->statePhase        = PA3_PHASE_WAIT_READY;
            actor->attackCancelTicks = PA3_ATTACK_CANCEL_TICKS;
            actor->mode              = GAME_ACTOR_MODE_NORMAL;
            actor->turnRateIndex     = 0;
            actor->animationState    = 0;
            actor->rumblePosted      = 0;
            actor->stateTimer        = PA3_PUMP_DELAY_FRAMES;
            playerActorSetWeaponAttackFlags(playerTask, 0, 0);
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= WORLD_COLLISION_BODY_CLIP_TO_GRID_CONTACT;
            if (gPlayerStatus.weaponSlotItem == SHOTGUN_ROUND_FIREFLY) {
                actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= WORLD_COLLISION_BODY_SINGLE_CONTACT;
            } else {
                actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= ~WORLD_COLLISION_BODY_SINGLE_CONTACT;
            }
            readyBlendFrames = PA3_READY_BLEND_FRAMES;
            if (((u16)actor->movementMode | actor->turnSign) != 0) {
                readyBlendFrames = PA3_MOVING_READY_BLEND_FRAMES;
            }
            playerActorPlayChildSlotsWithBlend(playerTask, PLAYER_ANIMATION_AIM_IDLE, 0, readyBlendFrames);
            actor->movementMode = GAME_ACTOR_MOVEMENT_STOPPED;
            /* fallthrough */
        case PA3_PHASE_WAIT_READY:
            if (animationGetCurrentRecord(&actor->animationContext, actor->animationSlots + 1) !=
                NULL) {
                actor->statePhase++;
            }
            break;
        case PA3_PHASE_FIRE:
            actor->statePhase++;
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
            equipmentConsumeWeaponLoad(WEAPON_ITEM_ID(WEAPON_INDEX), EQUIPMENT_WEAPON_LOAD_CONSUME_PRIMARY);
            worldCoordPlaySound(playerTask->extra.tmd->coords,
                                WEAPON_SOUND_VARIANT(gPlayerStatus.weaponSlotItem - SHOTGUN_ROUND_FIRST) | PA3_SOUND(SHOTGUN_SOUND_SLOT_SHOT), 1);
            effectSpawn(EFFECT_SHOTGUN_MUZZLE_FLASH,
                        actor->equipmentTasks[1]->extra.tmd->coords,
                        (gPlayerStatus.weaponSlotItem << 0x10) | WEAPON_INDEX, NULL);
            playerActorPlayChildSlotsWithBlend(playerTask, PLAYER_ANIMATION_FIRE, 1, 3);
            break;
        case PA3_PHASE_IMPACT:
            actor->statePhase++;
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
            if (gPlayerStatus.weaponSlotItem != SHOTGUN_ROUND_BUCKSHOT) {
                gridImpact = playerActorSpawnWeaponImpact(actor->weaponContacts, rootCoord, impactCoord);
                if (gPlayerStatus.weaponSlotItem == SHOTGUN_ROUND_FIREFLY) {
                    if (gridImpact != 0 || worldCollisionCountContactsByKind(actor->weaponContacts, WORLD_COLLISION_CONTACT_ENEMY_BODY) != 0) {
                        impactCoord->workm.t[0] = actor->weaponContacts[0].point.vx;
                        impactCoord->workm.t[1] = actor->weaponContacts[0].point.vy;
                        impactCoord->workm.t[2] = actor->weaponContacts[0].point.vz;
                        worldCoordPlaySound(impactCoord,
                                            WEAPON_SOUND_VARIANT(gPlayerStatus.weaponSlotItem - SHOTGUN_ROUND_FIRST) | PA3_SOUND(SHOTGUN_SOUND_SLOT_FIREFLY_IMPACT), 1);
                    }
                } else if (gridImpact != 0) {
                    worldCoordPlaySound(impactCoord, SOUND_BULLET_IMPACT, 1);
                }
            }
            /* fallthrough */
        case PA3_PHASE_PUMP_DELAY:
            // Pump timing and recovery checks run together through the fallthrough.
            if (--actor->stateTimer == 0) {
                actor->statePhase++;
                worldCoordPlaySound(playerTask->extra.tmd->coords,
                                    WEAPON_SOUND_VARIANT(gPlayerStatus.weaponSlotItem - SHOTGUN_ROUND_FIRST) | PA3_SOUND(PA3_SOUND_PUMP), 0);
            }
            /* fallthrough */
        case PA3_PHASE_RECOVER:
            if (actor->attackCancelTicks != 0) {
                actor->attackCancelTicks--;
            }
            if (_weaponIsArmSlotAdvancing(playerTask) == 0 ||
                ((actor->padHeld & actor->actionPadMask) != 0 && actor->attackCancelTicks == 0)) {
                actor->attackControl.cooldownTicks = 1;
                playerActorFinishWeaponAttack(playerTask);
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BLOCK(GfxCoord);
}

/* Each package carries its own model. */
#if WEAPON_INDEX == WEAPON_INDEX_PA3
/// The model `gPa3WeaponModel`: generated from its asset (weapons/pa3/model) by tools/buildsteps/gen_model_source.py.
#include "assets/weapons/pa3/model.inc"
#elif WEAPON_INDEX == WEAPON_INDEX_SP12
/// The model `gSp12Model`: generated from its asset (weapons/sp12/model) by tools/buildsteps/gen_model_source.py.
#include "assets/weapons/sp12/model.inc"
#endif

/* Each package carries its own animation bank. */
#if WEAPON_INDEX == WEAPON_INDEX_PA3
/// The player's clips this package carries, in the package's order, and its animation bank (weapons/pa3/bank):
/// generated from the clip and bank assets by tools/buildsteps/gen_animation_source.py.
#include "assets/weapons/pa3/clips.inc"
#include "assets/weapons/pa3/bank.inc"
#elif WEAPON_INDEX == WEAPON_INDEX_SP12
/// The player's clips this package carries, in the package's order, and its animation bank (weapons/sp12/bank):
/// generated from the clip and bank assets by tools/buildsteps/gen_animation_source.py.
#include "assets/weapons/sp12/clips.inc"
#include "assets/weapons/sp12/bank.inc"
#endif

PACKAGE_ALIASES
