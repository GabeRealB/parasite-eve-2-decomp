#include "weapons/p08.h"
#include "weapons/mongoose.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

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

#include "weapons/weapon.h"

/// One-based weapon index 4 (item 0x83), selecting the ordinary P08 build.
///
/// Integer preprocessor constant for `WEAPON_INDEX` tests. Uses the P08 held
/// model and player animation bank; firing selects this index's sound bank.
#define P08_WEAPON_INDEX_STANDARD 0x4

/// One-based weapon index 1 (item 0x80), selecting the snail-magazine P08 build.
///
/// Integer preprocessor constant for `WEAPON_INDEX` tests. Carries its own
/// held-model record and the ordinary P08's player animation bank.
#define P08_WEAPON_INDEX_SNAIL 0x1

/// One-based weapon index 9 (item 0x88), selecting the Mongoose build.
///
/// Integer preprocessor constant for `WEAPON_INDEX` tests. Selects the Mongoose
/// held model, player animation bank and firing sound bank.
#define P08_WEAPON_INDEX_MONGOOSE 0x9

/// Muzzle-offset and flash profiles passed to the gameplay flash controllers.
enum {
    P08_FLASH_PROFILE_HANDGUN  = 1,
    P08_FLASH_PROFILE_MONGOOSE = 9,
};

/// Manifest-selected weapon index, equal to its inventory item ID minus 0x7F.
///
/// Required integer preprocessor expression: 4 P08, 1 snail-magazine P08 or
/// 9 Mongoose. It selects assets and the firing sound bank; `WEAPON_ITEM_ID`
/// converts it to the weapon item whose load is consumed.
#ifndef WEAPON_INDEX
#error "WEAPON_INDEX is a per-package build parameter"
#elif WEAPON_INDEX != P08_WEAPON_INDEX_STANDARD && WEAPON_INDEX != P08_WEAPON_INDEX_SNAIL && WEAPON_INDEX != P08_WEAPON_INDEX_MONGOOSE
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
        P08_PHASE_PREPARE             = 0,
        P08_PHASE_WAIT_READY          = 1,
        P08_PHASE_FIRE                = 2,
        P08_PHASE_IMPACT              = 3,
        P08_PHASE_RECOVER             = 4,
        P08_PLAYER_ATTACK_STATE       = 4,
        P08_ANIMATION_READY           = 9,
        P08_ANIMATION_PRIMARY         = 0xA,
        P08_READY_BLEND_FRAMES        = 1,
        P08_MOVING_READY_BLEND_FRAMES = 5,
        P08_IMPACT_SOUND              = SOUND_COMMON(0x17),
        P08_CANCEL_FRAMES             = 0xB,
        P08_PRIMARY_SOUND_BASE        = SOUND_WEAPON(0, 4),
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
            actor->state             = P08_PLAYER_ATTACK_STATE;
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
            playerActorPlayChildSlotsWithBlend(playerTask, P08_ANIMATION_READY, 0, readyBlendFrames);
            actor->movementMode = 0;
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
            worldCoordPlaySound(playerTask->extra.tmd->coords, P08_PRIMARY_SOUND_BASE | (WEAPON_INDEX << 16), 1);
            effectSpawn(P08_MUZZLE_FLASH_EFFECT,
                        actor->equipmentTasks[1]->extra.tmd->coords,
                        P08_MUZZLE_FLASH_PROFILE, NULL);
            playerActorResetChildSlots(playerTask, P08_ANIMATION_PRIMARY);
            break;
        case P08_PHASE_IMPACT:
            actor->statePhase++;
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
            if (playerActorSpawnWeaponImpact(actor->weaponContacts, rootCoord, impactCoord) != 0) {
                worldCoordPlaySound(impactCoord, P08_IMPACT_SOUND, 1);
            }
            /* fallthrough */
        case P08_PHASE_RECOVER:
            if (actor->attackCancelTicks != 0) {
                actor->attackCancelTicks--;
            }
            if (playerActorIsSlotAdvancingLinearly(playerTask, gPlayerActorAttachmentParentParts[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId][1], 0, 0) == 0 ||
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
#if WEAPON_INDEX == P08_WEAPON_INDEX_STANDARD || WEAPON_INDEX == P08_WEAPON_INDEX_SNAIL
#include "assets/weapons/p08/model.inc"
#elif WEAPON_INDEX == P08_WEAPON_INDEX_MONGOOSE
#include "assets/weapons/mongoose/model.inc"
#endif

/// The player's clips this package carries, in the package's order, and its animation bank: generated from the
/// clip assets (characters/aya, weapons/mongoose) and the banks weapons/p08/bank (both P08s) and
/// weapons/mongoose/bank by tools/buildsteps/gen_animation_source.py.
#if WEAPON_INDEX == P08_WEAPON_INDEX_STANDARD
#include "assets/weapons/p08/clips.inc"
#include "assets/weapons/p08/bank.inc"
#elif WEAPON_INDEX == P08_WEAPON_INDEX_SNAIL
#include "assets/weapons/p08/snail_clips.inc"
#include "assets/weapons/p08/bank.inc"
#elif WEAPON_INDEX == P08_WEAPON_INDEX_MONGOOSE
#include "assets/weapons/mongoose/clips.inc"
#include "assets/weapons/mongoose/bank.inc"
#endif

PACKAGE_ALIASES
