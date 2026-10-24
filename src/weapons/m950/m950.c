#include "weapons/m950.h"

#include "config.h"

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

void m950AttackState(Task* playerTask)
{
    enum {
        M950_PHASE_PREPARE    = 0,
        M950_PHASE_WAIT_READY = 1,
        M950_PHASE_ARM_SHOT   = 2,
        M950_PHASE_SHOT_DELAY = 3,
        M950_PHASE_IMPACT     = 4,
        M950_PHASE_RECOVER    = 5,
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
            actor->state                                          = PLAYER_ACTOR_STATE_ATTACK;
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
            playerActorPlayChildSlotsWithBlend(playerTask, PLAYER_ANIMATION_AIM_IDLE, 0, readyBlendFrames);
            actor->movementMode = GAME_ACTOR_MOVEMENT_STOPPED;
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
                equipmentConsumeWeaponLoad(WEAPON_ITEM_ID(WEAPON_INDEX_M950), EQUIPMENT_WEAPON_LOAD_CONSUME_PRIMARY);
                worldCoordPlaySound(playerTask->extra.tmd->coords, M950_SOUND(WEAPON_SOUND_SLOT_SHOT), 1);
                effectSpawn(EFFECT_HANDGUN_MUZZLE_FLASH, weaponCoord, 3, NULL);
                playerActorPlayChildSlotsWithBlend(playerTask, PLAYER_ANIMATION_FIRE, 0, 2);
            }
            break;
        case M950_PHASE_IMPACT:
            actor->attackCancelTicks = M950_CANCEL_FRAMES;
            actor->statePhase++;
            _weaponResolveShotImpact(actor, weaponCoord, impactCoord);
            /* fallthrough */
        case M950_PHASE_RECOVER:
            if (playerActorReadAttackButton(playerTask) != 0 && playerActorQueryWeaponLoads(PLAYER_ACTOR_WEAPON_LOAD_PRIMARY) > 0) {
                goto fire;
            }
            if (actor->attackCancelTicks != 0) {
                actor->attackCancelTicks--;
            }
            if (_weaponIsArmSlotAdvancing(playerTask) == 0 ||
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
