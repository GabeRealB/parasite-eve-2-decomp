#include "weapons/m950.h"

#include <psyq/sys/types.h>

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

void m950AttackState(Task* playerTask)
{
    enum {
        M950_PHASE_PREPARE             = 0,
        M950_PHASE_WAIT_READY          = 1,
        M950_PHASE_ARM_SHOT            = 2,
        M950_PHASE_SHOT_DELAY          = 3,
        M950_PHASE_IMPACT              = 4,
        M950_PHASE_RECOVER             = 5,
        M950_PLAYER_ATTACK_STATE       = 4,
        M950_ANIMATION_READY           = 9,
        M950_ANIMATION_PRIMARY         = 0xA,
        M950_READY_BLEND_FRAMES        = 1,
        M950_MOVING_READY_BLEND_FRAMES = 5,
        M950_IMPACT_SOUND              = SOUND_COMMON(0x17),
        M950_SHOT_DELAY_FRAMES         = 4,
        M950_CANCEL_FRAMES             = 9,
        M950_WEAPON_ID                 = 3,
        M950_FIRE_SOUND                = SOUND_WEAPON(3, 4),
    };
    GameActor* actor;
    GfxCoord*  weaponCoord;
    GfxCoord*  impactCoord;
    s32        readyBlendFrames;

    impactCoord = SCRATCH_STACK_RESERVE_BLOCK(GfxCoord);
    actor       = playerTask->work;
    weaponCoord = actor->equipmentTasks[1]->extra.tmd->coords;
    switch (actor->statePhase) {
        case M950_PHASE_PREPARE:
            actor->state                                          = M950_PLAYER_ATTACK_STATE;
            actor->turnRateIndex                                  = 2;
            actor->mode                                           = GAME_ACTOR_MODE_NORMAL;
            actor->animationState                                 = 0;
            actor->statePhase                                    += 1;
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= (WORLD_COLLISION_BODY_CLIP_TO_GRID_CONTACT | WORLD_COLLISION_BODY_SINGLE_CONTACT);
            playerActorSetWeaponAttackFlags(playerTask, 0, 1);
            readyBlendFrames = M950_READY_BLEND_FRAMES;
            if (((u16)actor->movementMode | actor->turnSign) != 0) {
                readyBlendFrames = M950_MOVING_READY_BLEND_FRAMES;
            }
            playerActorPlayChildSlotsWithBlend(playerTask, M950_ANIMATION_READY, 0, readyBlendFrames);
            actor->movementMode = 0;
            break;
        case M950_PHASE_WAIT_READY:
            if (animationGetCurrentRecord(&actor->animationContext, actor->animationSlots + 1) !=
                NULL) {
                actor->statePhase++;
            }
            break;
        case M950_PHASE_ARM_SHOT:
        fire:
            // Rearming falls through so this dispatch counts toward the four-tick delay.
            actor->statePhase                  = M950_PHASE_SHOT_DELAY;
            actor->attackControl.cooldownTicks = 0;
            actor->rumblePosted                = 0;
            actor->stateTimer                  = M950_SHOT_DELAY_FRAMES;
            /* fallthrough */
        case M950_PHASE_SHOT_DELAY:
            if (--actor->stateTimer == 0) {
                actor->statePhase++;
                actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
                equipmentConsumeWeaponLoad(WEAPON_ITEM_ID(M950_WEAPON_ID), EQUIPMENT_WEAPON_LOAD_CONSUME_PRIMARY);
                worldCoordPlaySound(playerTask->extra.tmd->coords, M950_FIRE_SOUND, 1);
                effectSpawn(EFFECT_HANDGUN_MUZZLE_FLASH, weaponCoord, 3, NULL);
                playerActorPlayChildSlotsWithBlend(playerTask, M950_ANIMATION_PRIMARY, 0, 2);
            }
            break;
        case M950_PHASE_IMPACT:
            actor->attackCancelTicks = M950_CANCEL_FRAMES;
            actor->statePhase++;
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
            if (playerActorSpawnWeaponImpact(actor->weaponContacts, weaponCoord, impactCoord) != 0) {
                worldCoordPlaySound(impactCoord, M950_IMPACT_SOUND, 1);
            }
            /* fallthrough */
        case M950_PHASE_RECOVER:
            if (playerActorReadAttackButton(playerTask) != 0 && playerActorQueryWeaponLoads(PLAYER_ACTOR_WEAPON_LOAD_PRIMARY) > 0) {
                goto fire;
            }
            if (actor->attackCancelTicks != 0) {
                actor->attackCancelTicks--;
            }
            if (playerActorIsSlotAdvancingLinearly(playerTask, gPlayerActorAttachmentParentParts[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId][1], 0, 0) == 0 ||
                ((actor->padHeld & actor->actionPadMask) != 0 && actor->attackCancelTicks == 0)) {
                playerActorFinishWeaponAttack(playerTask);
            }
            break;
    }
    playerActorTrackLockTarget(playerTask);
    SCRATCH_STACK_RELEASE_BLOCK(GfxCoord);
}

/// The model `gM950WeaponModel`: generated from its asset (weapons/m950/model) by tools/buildsteps/gen_model_source.py.
#include "assets/weapons/m950/model.inc"

/// The player's clips this package carries, in the package's order, and its animation bank (weapons/m950/bank):
/// generated from the clip and bank assets by tools/buildsteps/gen_animation_source.py.
#include "assets/weapons/m950/clips.inc"
#include "assets/weapons/m950/bank.inc"
