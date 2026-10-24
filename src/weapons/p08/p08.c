#include "weapons/p08.h"

#include "config.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

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
/// Required integer preprocessor expression: 4 P08, 1 snail-magazine P08 or
/// 9 Mongoose. It selects assets and the firing sound bank; `WEAPON_ITEM_ID`
/// converts it to the weapon item whose load is consumed.
#ifndef WEAPON_INDEX
#error "WEAPON_INDEX is a per-package build parameter"
#elif WEAPON_INDEX != WEAPON_INDEX_P08 && WEAPON_INDEX != WEAPON_INDEX_P08_SNAIL && WEAPON_INDEX != WEAPON_INDEX_MONGOOSE
#error "WEAPON_INDEX must select a P08, snail-magazine P08 or Mongoose build"
#endif

/// Manifest-selected packed effect ID for the muzzle flash spawned per shot.
///
/// Required s32 constant expression from `effect_ids.h`: both P08 builds use
/// `EFFECT_HANDGUN_MUZZLE_FLASH`; Mongoose uses `EFFECT_ACTOR_MUZZLE_FLASH`.
#ifndef P08_MUZZLE_FLASH_EFFECT
#error "P08_MUZZLE_FLASH_EFFECT is a per-package build parameter"
#endif

/// Manifest-selected muzzle-offset and flash profile for the chosen controller.
///
/// Required s32 constant expression: `P08_FLASH_PROFILE_HANDGUN` (1) for both
/// P08 builds, or `P08_FLASH_PROFILE_MONGOOSE` (9). The ordinary P08's weapon
/// index is 4, but it shares profile 1 with the snail-magazine P08.
#ifndef P08_MUZZLE_FLASH_PROFILE
#error "P08_MUZZLE_FLASH_PROFILE is a per-package build parameter"
#endif

/// Active player-actor ticks blocking another attack after firing recovery ends.
///
/// Required nonnegative integer preprocessor expression fitting s16: 12 for
/// both P08 builds, 15 for Mongoose. Stored in `attackControl.cooldownTicks`;
/// the actor tick decrements it before dispatch, and actor holds pause it.
#ifndef P08_RECOVERY_COOLDOWN_TICKS
#error "P08_RECOVERY_COOLDOWN_TICKS is a per-package build parameter"
#elif P08_RECOVERY_COOLDOWN_TICKS < 0 || P08_RECOVERY_COOLDOWN_TICKS > 32767
#error "P08_RECOVERY_COOLDOWN_TICKS must fit a nonnegative s16"
#endif

void p08AttackState(Task* playerTask)
{
    enum {
        P08_PHASE_PREPARE    = 0,
        P08_PHASE_WAIT_READY = 1,
        P08_PHASE_FIRE       = 2,
        P08_PHASE_IMPACT     = 3,
        P08_PHASE_RECOVER    = 4,
    };
    GameActor* actor;
    GfxCoord*  rootCoord;
    GfxCoord*  impactCoord;
    s32        readyBlendFrames;

    impactCoord = SCRATCH_STACK_RESERVE_BLOCK(GfxCoord);
    actor       = playerTask->work;
    rootCoord   = playerTask->extra.tmd->coords;
    switch (actor->statePhase) {
        case P08_PHASE_PREPARE:
            actor->state             = PLAYER_ACTOR_STATE_ATTACK;
            actor->statePhase        = P08_PHASE_WAIT_READY;
            readyBlendFrames         = P08_READY_BLEND_FRAMES;
            actor->mode              = GAME_ACTOR_MODE_NORMAL;
            actor->turnRateIndex     = 0;
            actor->animationState    = 0;
            actor->rumblePosted      = 0;
            actor->attackCancelTicks = P08_CANCEL_FRAMES;
            if (((u16)actor->movementMode | actor->turnSign) != 0) {
                readyBlendFrames = P08_MOVING_READY_BLEND_FRAMES;
            }
            playerActorPlayChildSlotsWithBlend(playerTask, PLAYER_ANIMATION_AIM_IDLE, 0, readyBlendFrames);
            actor->movementMode = GAME_ACTOR_MOVEMENT_STOPPED;
            break;
        case P08_PHASE_WAIT_READY:
            if (animationGetCurrentRecord(&actor->animationContext, actor->animationSlots + 1) !=
                NULL) {
                actor->statePhase++;
            }
            break;
        case P08_PHASE_FIRE:
            actor->statePhase++;
            playerActorSetWeaponAttackFlags(playerTask, 0, 0);
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= (WORLD_COLLISION_BODY_CLIP_TO_GRID_CONTACT | WORLD_COLLISION_BODY_SINGLE_CONTACT | WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
            equipmentConsumeWeaponLoad(WEAPON_ITEM_ID(WEAPON_INDEX), EQUIPMENT_WEAPON_LOAD_CONSUME_PRIMARY);
            worldCoordPlaySound(playerTask->extra.tmd->coords, P08_SOUND(WEAPON_SOUND_SLOT_SHOT), 1);
            effectSpawn(P08_MUZZLE_FLASH_EFFECT,
                        actor->equipmentTasks[1]->extra.tmd->coords,
                        P08_MUZZLE_FLASH_PROFILE, NULL);
            playerActorResetChildSlots(playerTask, PLAYER_ANIMATION_FIRE);
            break;
        case P08_PHASE_IMPACT:
            actor->statePhase++;
            _weaponResolveShotImpact(actor, rootCoord, impactCoord);
            /* fallthrough */
        case P08_PHASE_RECOVER:
            if (actor->attackCancelTicks != 0) {
                actor->attackCancelTicks--;
            }
            if (_weaponIsArmSlotAdvancing(playerTask) == 0 ||
                ((actor->padHeld & actor->actionPadMask) != 0 && actor->attackCancelTicks == 0)) {
                actor->attackControl.cooldownTicks = P08_RECOVERY_COOLDOWN_TICKS;
                playerActorFinishWeaponAttack(playerTask);
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BLOCK(GfxCoord);
}

/// The held weapon model, generated from its asset by tools/buildsteps/gen_model_source.py: the P08 and its
/// snail-magazine variant carry one model, `gP08WeaponModel` (weapons/p08/model), the Mongoose its own,
/// `gMongooseModel` (weapons/mongoose/model).
#if WEAPON_INDEX == WEAPON_INDEX_P08 || WEAPON_INDEX == WEAPON_INDEX_P08_SNAIL
#include "assets/weapons/p08/model.inc"
#elif WEAPON_INDEX == WEAPON_INDEX_MONGOOSE
#include "assets/weapons/mongoose/model.inc"
#endif

/// The player's clips this package carries, in the package's order, and its animation bank: generated from the
/// clip assets (characters/aya, weapons/mongoose) and the banks weapons/p08/bank (both P08s) and
/// weapons/mongoose/bank by tools/buildsteps/gen_animation_source.py.
#if WEAPON_INDEX == WEAPON_INDEX_P08
#include "assets/weapons/p08/clips.inc"
#include "assets/weapons/p08/bank.inc"
#elif WEAPON_INDEX == WEAPON_INDEX_P08_SNAIL
#include "assets/weapons/p08/snail_clips.inc"
#include "assets/weapons/p08/bank.inc"
#elif WEAPON_INDEX == WEAPON_INDEX_MONGOOSE
#include "assets/weapons/mongoose/clips.inc"
#include "assets/weapons/mongoose/bank.inc"
#endif

PACKAGE_ALIASES
