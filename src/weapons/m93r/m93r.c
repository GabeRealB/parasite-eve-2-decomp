#include "weapons/m93r.h"

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

#include "../shared/attack.h"

#include "assets/engine/animation/player_schema.h"

/// The held weapon model, `gM93rWeaponModel`, with its arrays: generated from the asset
/// (weapons/m93r/model.usda, textured through weapons/m93r/binding) by tools/buildsteps/gen_model_source.py.
#include "assets/weapons/m93r/model.inc"

/// The player's clips this package carries, in the package's order, and its animation bank: generated
/// from the clip assets (characters/aya, weapons/m93r) and the bank weapons/m93r/bank by
/// tools/buildsteps/gen_animation_source.py.
#include "assets/weapons/m93r/clips.inc"
#include "assets/weapons/m93r/bank.inc"

void m93rAttackState(Task* playerTask)
{
    enum {
        M93R_PHASE_PREPARE    = 0,
        M93R_PHASE_WAIT_READY = 1,
        M93R_PHASE_BURST      = 2,
        M93R_PHASE_RECOVER    = 3,
    };
    GameActor* actor;
    GfxCoord*  rootCoord;
    GfxCoord*  impactCoord;
    s32        readyBlendFrames;
    s32        burstDelay;
    s16        burstRounds;
    s32        cancelOnHeldInput;

    impactCoord       = SCRATCH_STACK_RESERVE_BLOCK(GfxCoord);
    actor             = playerTask->work;
    rootCoord         = playerTask->extra.tmd->coords;
    cancelOnHeldInput = 0;
    switch (actor->statePhase) {
        case M93R_PHASE_PREPARE:
            actor->state             = PLAYER_ACTOR_STATE_ATTACK;
            actor->mode              = GAME_ACTOR_MODE_NORMAL;
            actor->turnRateIndex     = 0;
            actor->animationState    = 0;
            actor->statePhase        = M93R_PHASE_WAIT_READY;
            actor->rumblePosted      = 0;
            actor->stateTimer        = 0;
            actor->attackCancelTicks = M93R_CANCEL_FRAMES;
            burstRounds              = 1;
            if (actor->attackButton == PLAYER_ACTOR_ATTACK_BUTTON_PRIMARY) {
                burstRounds = M93R_BURST_ROUNDS;
            }
            actor->actionValue = burstRounds;
            playerActorSetWeaponAttackFlags(playerTask, 0, actor->attackButton == PLAYER_ACTOR_ATTACK_BUTTON_PRIMARY);
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= (WORLD_COLLISION_BODY_CLIP_TO_GRID_CONTACT | WORLD_COLLISION_BODY_SINGLE_CONTACT);
            readyBlendFrames                                      = M93R_READY_BLEND_FRAMES;
            if (((u16)actor->movementMode | actor->turnSign) != 0) {
                readyBlendFrames = M93R_MOVING_READY_BLEND_FRAMES;
            }
            playerActorPlayChildSlotsWithBlend(playerTask, PLAYER_ANIMATION_AIM_IDLE, 0, readyBlendFrames);
            actor->movementMode = GAME_ACTOR_MOVEMENT_STOPPED;
            break;
        case M93R_PHASE_WAIT_READY:
            if (animationGetCurrentRecord(&actor->animationContext, actor->animationSlots + 1) !=
                NULL) {
                actor->statePhase++;
            }
            break;
        case M93R_PHASE_BURST:
            // Keep contacts active for one frame between successive burst rounds.
            if (actor->actionValue != 0) {
                burstDelay = actor->stateTimer;
                if (burstDelay == 0) {
                    actor->actionValue--;
                    actor->stateTimer                                     = M93R_BURST_DELAY_FRAMES;
                    actor->rumblePosted                                   = 0;
                    actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
                    equipmentConsumeWeaponLoad(WEAPON_ITEM_ID(WEAPON_INDEX_M93R), EQUIPMENT_WEAPON_LOAD_CONSUME_PRIMARY);
                    if (playerActorQueryWeaponLoads(PLAYER_ACTOR_WEAPON_LOAD_PRIMARY) == 0) {
                        actor->actionValue = 0;
                    }
                    worldCoordPlaySound(playerTask->extra.tmd->coords, M93R_SOUND(WEAPON_SOUND_SLOT_SHOT), 1);
                    effectSpawn(EFFECT_HANDGUN_MUZZLE_FLASH,
                                actor->equipmentTasks[1]->extra.tmd->coords, WEAPON_INDEX_M93R,
                                NULL);
                    playerActorPlayChildSlotsWithBlend(playerTask, PLAYER_ANIMATION_FIRE, 1, 2);
                    break;
                }
                burstDelay--;
                actor->stateTimer = burstDelay;
                if (burstDelay == 0) {
                    _weaponResolveShotImpact(actor, rootCoord, impactCoord);
                }
            } else {
                actor->statePhase = M93R_PHASE_RECOVER;
                _weaponResolveShotImpact(actor, rootCoord, impactCoord);
            }
            break;
        case M93R_PHASE_RECOVER:
            if (actor->attackCancelTicks != 0) {
                actor->attackCancelTicks--;
            }
            if ((actor->padHeld & actor->actionPadMask) != 0) {
                cancelOnHeldInput = actor->attackCancelTicks == 0;
            }
            if (_weaponIsArmSlotAdvancing(playerTask) == 0 || cancelOnHeldInput) {
                if (cancelOnHeldInput) {
                    actor->attackControl.cooldownTicks = M93R_HELD_COOLDOWN_FRAMES;
                } else {
                    actor->attackControl.cooldownTicks = 0;
                }
                playerActorFinishWeaponAttack(playerTask);
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BLOCK(GfxCoord);
}
