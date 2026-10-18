#include "weapons/pa3.h"
#include "weapons/sp12.h"

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
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/scratch.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "weapons/weapon.h"

/// One-based weapon index 13 (item 0x8C), selecting the PA3 build.
///
/// Integer preprocessor constant for `WEAPON_INDEX` tests. Selects the held
/// model, player animation bank, sound bank and shotgun muzzle-flash profile.
#define PA3_WEAPON_INDEX_STANDARD 0xD

/// One-based weapon index 14 (item 0x8D), selecting the SP12 build.
///
/// Integer preprocessor constant for `WEAPON_INDEX` tests. Selects the SP12
/// model, player animation bank, sound bank and shotgun muzzle-flash profile.
#define PA3_WEAPON_INDEX_SP12 0xE

/// Manifest-selected weapon index, equal to its inventory item ID minus 0x7F.
///
/// Required integer preprocessor expression: 13 PA3 or 14 SP12. It selects
/// assets, the firing/pump sound bank and the shotgun flash profile;
/// `WEAPON_ITEM_ID` converts it to the weapon item whose load is consumed.
#ifndef WEAPON_INDEX
#error "WEAPON_INDEX is a per-package build parameter"
#elif WEAPON_INDEX != PA3_WEAPON_INDEX_STANDARD && WEAPON_INDEX != PA3_WEAPON_INDEX_SP12
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
        PA3_PHASE_PREPARE              = 0,
        PA3_PHASE_WAIT_READY           = 1,
        PA3_PHASE_FIRE                 = 2,
        PA3_PHASE_IMPACT               = 3,
        PA3_PHASE_PUMP_DELAY           = 4,
        PA3_PHASE_RECOVER              = 5,
        PA3_PLAYER_ATTACK_STATE        = 4,
        PA3_ANIMATION_READY            = 9,
        PA3_ANIMATION_PRIMARY          = 0xA,
        PA3_READY_BLEND_FRAMES         = 1,
        PA3_MOVING_READY_BLEND_FRAMES  = 8,
        PA3_IMPACT_SOUND               = SOUND_COMMON(0x17),
        PA3_PUMP_DELAY_FRAMES          = 0x1F,
        PA3_AMMUNITION_BUCKSHOT        = WEAPON_AMMUNITION_INDEX(0xAC),
        PA3_AMMUNITION_FIREFLY         = WEAPON_AMMUNITION_INDEX(0xAD),
        PA3_FIREFLY_CONTACT_SOUND_BASE = SOUND_WEAPON(0, 4),
        PA3_FIRE_SOUND_BASE            = SOUND_WEAPON(0, 5),
        PA3_PUMP_SOUND_BASE            = SOUND_WEAPON(0, 2),
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
            actor->state             = PA3_PLAYER_ATTACK_STATE;
            actor->statePhase        = PA3_PHASE_WAIT_READY;
            actor->attackCancelTicks = PA3_ATTACK_CANCEL_TICKS;
            actor->mode              = GAME_ACTOR_MODE_NORMAL;
            actor->turnRateIndex     = 0;
            actor->animationState    = 0;
            actor->rumblePosted      = 0;
            actor->stateTimer        = PA3_PUMP_DELAY_FRAMES;
            playerActorSetWeaponAttackFlags(playerTask, 0, 0);
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= WORLD_COLLISION_BODY_CLIP_TO_GRID_CONTACT;
            if (gPlayerStatus.weaponSlotItem == PA3_AMMUNITION_FIREFLY) {
                actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= WORLD_COLLISION_BODY_SINGLE_CONTACT;
            } else {
                actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= ~WORLD_COLLISION_BODY_SINGLE_CONTACT;
            }
            readyBlendFrames = PA3_READY_BLEND_FRAMES;
            if (((u16)actor->movementMode | actor->turnSign) != 0) {
                readyBlendFrames = PA3_MOVING_READY_BLEND_FRAMES;
            }
            playerActorPlayChildSlotsWithBlend(playerTask, PA3_ANIMATION_READY, 0, readyBlendFrames);
            actor->movementMode = 0;
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
                                ((gPlayerStatus.weaponSlotItem - PA3_AMMUNITION_BUCKSHOT) << 0x18) | PA3_FIRE_SOUND_BASE | (WEAPON_INDEX << 16), 1);
            effectSpawn(EFFECT_SHOTGUN_MUZZLE_FLASH,
                        actor->equipmentTasks[1]->extra.tmd->coords,
                        (gPlayerStatus.weaponSlotItem << 0x10) | WEAPON_INDEX, NULL);
            playerActorPlayChildSlotsWithBlend(playerTask, PA3_ANIMATION_PRIMARY, 1, 3);
            break;
        case PA3_PHASE_IMPACT:
            actor->statePhase++;
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
            if (gPlayerStatus.weaponSlotItem != PA3_AMMUNITION_BUCKSHOT) {
                gridImpact = playerActorSpawnWeaponImpact(actor->weaponContacts, rootCoord, impactCoord);
                if (gPlayerStatus.weaponSlotItem == PA3_AMMUNITION_FIREFLY) {
                    if (gridImpact != 0 || worldCollisionCountContactsByKind(actor->weaponContacts, WORLD_COLLISION_CONTACT_ENEMY_BODY) != 0) {
                        impactCoord->workm.t[0] = actor->weaponContacts[0].point.vx;
                        impactCoord->workm.t[1] = actor->weaponContacts[0].point.vy;
                        impactCoord->workm.t[2] = actor->weaponContacts[0].point.vz;
                        worldCoordPlaySound(impactCoord,
                                            ((gPlayerStatus.weaponSlotItem - PA3_AMMUNITION_BUCKSHOT) << 0x18) | PA3_FIREFLY_CONTACT_SOUND_BASE | (WEAPON_INDEX << 16), 1);
                    }
                } else if (gridImpact != 0) {
                    worldCoordPlaySound(impactCoord, PA3_IMPACT_SOUND, 1);
                }
            }
            /* fallthrough */
        case PA3_PHASE_PUMP_DELAY:
            // Pump timing and recovery checks run together through the fallthrough.
            if (--actor->stateTimer == 0) {
                actor->statePhase++;
                worldCoordPlaySound(playerTask->extra.tmd->coords,
                                    ((gPlayerStatus.weaponSlotItem - PA3_AMMUNITION_BUCKSHOT) << 0x18) | PA3_PUMP_SOUND_BASE | (WEAPON_INDEX << 16), 0);
            }
            /* fallthrough */
        case PA3_PHASE_RECOVER:
            if (actor->attackCancelTicks != 0) {
                actor->attackCancelTicks--;
            }
            if (playerActorIsSlotAdvancingLinearly(playerTask, gPlayerActorAttachmentParentParts[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId][1], 0, 0) == 0 ||
                ((actor->padHeld & actor->actionPadMask) != 0 && actor->attackCancelTicks == 0)) {
                actor->attackControl.cooldownTicks = 1;
                playerActorFinishWeaponAttack(playerTask);
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BLOCK(GfxCoord);
}

/* Each package carries its own model. */
#if WEAPON_INDEX == PA3_WEAPON_INDEX_STANDARD
/// The model `gPa3WeaponModel`: generated from its asset (weapons/pa3/model) by tools/buildsteps/gen_model_source.py.
#include "assets/weapons/pa3/model.inc"
#elif WEAPON_INDEX == PA3_WEAPON_INDEX_SP12
/// The model `gSp12Model`: generated from its asset (weapons/sp12/model) by tools/buildsteps/gen_model_source.py.
#include "assets/weapons/sp12/model.inc"
#endif

/* Each package carries its own animation bank. */
#if WEAPON_INDEX == PA3_WEAPON_INDEX_STANDARD
/// The player's clips this package carries, in the package's order, and its animation bank (weapons/pa3/bank):
/// generated from the clip and bank assets by tools/buildsteps/gen_animation_source.py.
#include "assets/weapons/pa3/clips.inc"
#include "assets/weapons/pa3/bank.inc"
#elif WEAPON_INDEX == PA3_WEAPON_INDEX_SP12
/// The player's clips this package carries, in the package's order, and its animation bank (weapons/sp12/bank):
/// generated from the clip and bank assets by tools/buildsteps/gen_animation_source.py.
#include "assets/weapons/sp12/clips.inc"
#include "assets/weapons/sp12/bank.inc"
#endif

PACKAGE_ALIASES
