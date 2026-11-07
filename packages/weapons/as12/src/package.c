#include "as12.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include <core/pe2.h>

#include <gameplay/items.h>
#include <gameplay/player_actor.h>
#include <gameplay/room_effects.h>
#include <gameplay/scene_runtime.h>
#include <gameplay/world_collision.h>
#include <gameplay/world_coords.h>
#include <gameplay/player_animation.h>

#include <engine/coord.h>
#include <engine/mc_types.h>
#include <engine/scratch.h>
#include <engine/session_types.h>
#include <engine/task_types.h>
#include <engine/wipsys.h>
#include <engine/wipsys_types.h>
#include <engine/sound_ids.h>

#include <weapons/weapon/weapon.h>
#include <weapons/weapon/sounds.h>
#include <weapons/attack/attack.h>
#include <weapons/weapon/weapon_index.h>

PE2_EDITOR_LABEL("Attack")
/// Attack tuning, in dispatch ticks: blending into the ready pose standing still and moving, the cooldown after a
/// shot, and the recovery ticks before a held action input can cancel it.
PE2_OPTION_DEF(READY_BLEND_FRAMES, int, 1)
PE2_OPTION_DEF(MOVING_READY_BLEND_FRAMES, int, 5)
PE2_OPTION_DEF(COOLDOWN_FRAMES, int, 0x21)
PE2_OPTION_DEF(CANCEL_FRAMES, int, 0x16)

PE2_EDITOR_LABEL("Model")
/// The model held in the player's weapon hand.
PE2_BIND(model, model)
PE2_BIND_ARG(model, _symbol, gAs12Model)
PE2_VALUE(model)

PE2_EDITOR_LABEL("Animation")
/// The player's clips while it is equipped, by slot.
PE2_BIND(bank, animation_bank)
PE2_BIND_ARG(bank, _symbol, gAs12PlayerAnimationBank)
PE2_VALUE(bank)

/// A sound of the AS12's bank, by slot (`WEAPON_SOUND_SLOT_*` or `SHOTGUN_SOUND_SLOT_*`).
#define SOUND(slot) SOUND_WEAPON(WEAPON_INDEX_AS12, (slot))

void as12AttackState(Task* playerTask)
{
    enum {
        PHASE_PREPARE           = 0,
        PHASE_WAIT_READY        = 1,
        PHASE_ARM_SHOT          = 2,
        PHASE_FIRE              = 3,
        PHASE_IMPACT            = 4,
        PHASE_RECOVER           = 5,
        EFFECT_AMMUNITION_SHIFT = 16,
    };
    GameActor* actor;
    GfxCoord*  rootCoord;
    GfxCoord*  impactCoord;
    s32        readyBlendFrames;
    s32        impactFound;

    impactCoord = SCRATCH_STACK_RESERVE_BLOCK(GfxCoord);
    actor       = playerTask->work;
    rootCoord   = playerTask->extra.tmd->coords;
    switch (actor->statePhase) {
        case PHASE_PREPARE:
            actor->state          = PLAYER_ACTOR_STATE_ATTACK;
            actor->mode           = GAME_ACTOR_MODE_NORMAL;
            actor->turnRateIndex  = 0;
            actor->animationState = 0;
            actor->statePhase++;
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= WORLD_COLLISION_BODY_CLIP_TO_GRID_CONTACT;
            if (gPlayerStatus.weaponSlotItem == SHOTGUN_ROUND_FIREFLY) {
                actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= WORLD_COLLISION_BODY_SINGLE_CONTACT;
            } else {
                actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= ~WORLD_COLLISION_BODY_SINGLE_CONTACT;
            }
            readyBlendFrames = PE2_OPTION(READY_BLEND_FRAMES);
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
        case PHASE_ARM_SHOT:
        fire:
            actor->statePhase                  = PHASE_FIRE;
            actor->attackControl.cooldownTicks = PE2_OPTION(COOLDOWN_FRAMES);
            actor->rumblePosted                = 0;
            playerActorSetWeaponAttackFlags(playerTask, 0, actor->attackButton != PLAYER_ACTOR_ATTACK_BUTTON_PRIMARY);
            /* fallthrough */
        case PHASE_FIRE:
            actor->statePhase++;
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
            equipmentConsumeWeaponLoad(WEAPON_ITEM_ID(WEAPON_INDEX_AS12), EQUIPMENT_WEAPON_LOAD_CONSUME_PRIMARY);
            worldCoordPlaySound(playerTask->extra.tmd->coords,
                                WEAPON_SOUND_VARIANT(gPlayerStatus.weaponSlotItem - SHOTGUN_ROUND_FIRST) | SOUND(SHOTGUN_SOUND_SLOT_SHOT), 1);
            effectSpawn(EFFECT_SHOTGUN_MUZZLE_FLASH,
                        actor->equipmentTasks[1]->extra.tmd->coords,
                        (gPlayerStatus.weaponSlotItem << EFFECT_AMMUNITION_SHIFT) | WEAPON_INDEX_AS12, NULL);
            playerActorResetChildSlots(playerTask, PLAYER_ANIMATION_FIRE);
            break;
        case PHASE_IMPACT:
            // Firefly uses the first contact position; other non-Buckshot loads use the picked impact.
            actor->attackCancelTicks = PE2_OPTION(CANCEL_FRAMES);
            actor->statePhase++;
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
            if (gPlayerStatus.weaponSlotItem != SHOTGUN_ROUND_BUCKSHOT) {
                impactFound = playerActorSpawnWeaponImpact(actor->weaponContacts, rootCoord, impactCoord);
                if (gPlayerStatus.weaponSlotItem == SHOTGUN_ROUND_FIREFLY) {
                    if (impactFound != 0 || worldCollisionCountContactsByKind(actor->weaponContacts, WORLD_COLLISION_CONTACT_ENEMY_BODY) != 0) {
                        // Requests the AS12 Firefly impact sound at the first recorded weapon contact.
                        impactCoord->workm.t[0] = actor->weaponContacts[0].point.vx;
                        impactCoord->workm.t[1] = actor->weaponContacts[0].point.vy;
                        impactCoord->workm.t[2] = actor->weaponContacts[0].point.vz;
                        worldCoordPlaySound(impactCoord,
                                            WEAPON_SOUND_VARIANT(gPlayerStatus.weaponSlotItem - SHOTGUN_ROUND_FIRST) | SOUND(SHOTGUN_SOUND_SLOT_FIREFLY_IMPACT), 1);
                    }
                } else if (impactFound != 0) {
                    worldCoordPlaySound(impactCoord, SOUND_BULLET_IMPACT, 1);
                }
            }
            /* fallthrough */
        case PHASE_RECOVER:
            if (playerActorReadAttackButton(playerTask) != 0 && playerActorQueryWeaponLoads(PLAYER_ACTOR_WEAPON_LOAD_PRIMARY) > 0 && actor->attackControl.cooldownTicks == 0) {
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
    SCRATCH_STACK_RELEASE_BLOCK(GfxCoord);
}
