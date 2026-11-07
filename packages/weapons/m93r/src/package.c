#include "m93r.h"

#include <core/pe2.h>

#include <gameplay/items.h>
#include <gameplay/player_actor.h>
#include <gameplay/player_animation.h>
#include <gameplay/room_effects.h>
#include <gameplay/scene_runtime.h>
#include <gameplay/world_coords.h>

#include <engine/coord.h>
#include <engine/mc_types.h>
#include <engine/scratch.h>
#include <engine/session_types.h>
#include <engine/sound_ids.h>
#include <engine/task_types.h>

#include <weapons/weapon/sounds.h>
#include <weapons/weapon/weapon.h>
#include <weapons/weapon/weapon_index.h>
#include <weapons/attack/attack.h>

PE2_EDITOR_LABEL("Attack")
/// Rounds of a primary-button burst (the secondary button fires one), and the ticks between them.
PE2_OPTION_DEF(BURST_ROUNDS, int, 3)
PE2_OPTION_DEF(BURST_DELAY_FRAMES, int, 1)
/// Recovery ticks before a held action input can cancel it, and the cooldown after a recovery it cancelled.
PE2_OPTION_DEF(CANCEL_FRAMES, int, 10)
PE2_OPTION_DEF(HELD_COOLDOWN_FRAMES, int, 10)
/// Ticks of blending into the ready pose, standing still and moving or turning.
PE2_OPTION_DEF(READY_BLEND_FRAMES, int, 1)
PE2_OPTION_DEF(MOVING_READY_BLEND_FRAMES, int, 6)

PE2_EDITOR_LABEL("Model")
/// The model held in the player's weapon hand.
PE2_BIND(model, model)
PE2_BIND_ARG(model, _symbol, gM93rWeaponModel)
PE2_VALUE(model)

PE2_EDITOR_LABEL("Animation")
/// The player's clips while the M93R is equipped, by slot.
PE2_BIND(bank, animation_bank)
PE2_BIND_ARG(bank, _symbol, gM93rPlayerAnimationBank)
PE2_VALUE(bank)

/// A sound of the M93R's bank, by slot (`WEAPON_SOUND_SLOT_*`).
#define SOUND(slot) SOUND_WEAPON(WEAPON_INDEX_M93R, (slot))

void m93rAttackState(Task* playerTask)
{
    enum {
        PHASE_PREPARE    = 0,
        PHASE_WAIT_READY = 1,
        PHASE_BURST      = 2,
        PHASE_RECOVER    = 3,
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
        case PHASE_PREPARE:
            actor->state             = PLAYER_ACTOR_STATE_ATTACK;
            actor->mode              = GAME_ACTOR_MODE_NORMAL;
            actor->turnRateIndex     = 0;
            actor->animationState    = 0;
            actor->statePhase        = PHASE_WAIT_READY;
            actor->rumblePosted      = 0;
            actor->stateTimer        = 0;
            actor->attackCancelTicks = PE2_OPTION(CANCEL_FRAMES);
            burstRounds              = 1;
            if (actor->attackButton == PLAYER_ACTOR_ATTACK_BUTTON_PRIMARY) {
                burstRounds = PE2_OPTION(BURST_ROUNDS);
            }
            actor->actionValue = burstRounds;
            playerActorSetWeaponAttackFlags(playerTask, 0, actor->attackButton == PLAYER_ACTOR_ATTACK_BUTTON_PRIMARY);
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= (WORLD_COLLISION_BODY_CLIP_TO_GRID_CONTACT | WORLD_COLLISION_BODY_SINGLE_CONTACT);
            readyBlendFrames                                      = PE2_OPTION(READY_BLEND_FRAMES);
            if (((u16)actor->movementMode | actor->turnSign) != 0) {
                readyBlendFrames = PE2_OPTION(MOVING_READY_BLEND_FRAMES);
            }
            playerActorPlayChildSlotsWithBlend(playerTask, PLAYER_ANIMATION_AIM_IDLE, 0, readyBlendFrames);
            actor->movementMode = GAME_ACTOR_MOVEMENT_STOPPED;
            break;
        case PHASE_WAIT_READY:
            if (animationGetCurrentRecord(&actor->animationContext, actor->animationSlots + 1) !=
                NULL) {
                actor->statePhase++;
            }
            break;
        case PHASE_BURST:
            // Keep contacts active for one frame between successive burst rounds.
            if (actor->actionValue != 0) {
                burstDelay = actor->stateTimer;
                if (burstDelay == 0) {
                    actor->actionValue--;
                    actor->stateTimer                                     = PE2_OPTION(BURST_DELAY_FRAMES);
                    actor->rumblePosted                                   = 0;
                    actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
                    equipmentConsumeWeaponLoad(WEAPON_ITEM_ID(WEAPON_INDEX_M93R), EQUIPMENT_WEAPON_LOAD_CONSUME_PRIMARY);
                    if (playerActorQueryWeaponLoads(PLAYER_ACTOR_WEAPON_LOAD_PRIMARY) == 0) {
                        actor->actionValue = 0;
                    }
                    worldCoordPlaySound(playerTask->extra.tmd->coords, SOUND(WEAPON_SOUND_SLOT_SHOT), 1);
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
                actor->statePhase = PHASE_RECOVER;
                _weaponResolveShotImpact(actor, rootCoord, impactCoord);
            }
            break;
        case PHASE_RECOVER:
            if (actor->attackCancelTicks != 0) {
                actor->attackCancelTicks--;
            }
            if ((actor->padHeld & actor->actionPadMask) != 0) {
                cancelOnHeldInput = actor->attackCancelTicks == 0;
            }
            if (_weaponIsArmSlotAdvancing(playerTask) == 0 || cancelOnHeldInput) {
                if (cancelOnHeldInput) {
                    actor->attackControl.cooldownTicks = PE2_OPTION(HELD_COOLDOWN_FRAMES);
                } else {
                    actor->attackControl.cooldownTicks = 0;
                }
                playerActorFinishWeaponAttack(playerTask);
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BLOCK(GfxCoord);
}
