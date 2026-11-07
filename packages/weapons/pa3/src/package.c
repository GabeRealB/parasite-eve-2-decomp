#include "pa3.h"

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

/// The weapon the package is (its index: the item it is, less 0x7F): 13 the PA3, 14 the SP12. It also picks the shotgun's flash.
PE2_OPTION_DEF(WEAPON_INDEX, int)

PE2_EDITOR_LABEL("Attack")
/// Attack tuning, in dispatch ticks.
PE2_OPTION_DEF(READY_BLEND_FRAMES, int, 1)
PE2_OPTION_DEF(MOVING_READY_BLEND_FRAMES, int, 8)
PE2_OPTION_DEF(PUMP_DELAY_FRAMES, int, 0x1F)

/// Recovery ticks before a held action input can end it early.
PE2_OPTION_DEF(ATTACK_CANCEL_FRAMES, int, 32)
STATIC_ASSERT(PE2_OPTION(ATTACK_CANCEL_FRAMES) >= 0 && PE2_OPTION(ATTACK_CANCEL_FRAMES) <= 127, attack_cancel_ticks_is_a_nonnegative_s8);

PE2_EDITOR_LABEL("Model")
/// The model held in the player's weapon hand.
PE2_BIND(model, model)
#if PE2_OPTION(WEAPON_INDEX) == WEAPON_INDEX_SP12
PE2_BIND_ARG(model, _symbol, gSp12WeaponModel)
#else
PE2_BIND_ARG(model, _symbol, gPa3WeaponModel)
#endif
PE2_VALUE(model)

PE2_EDITOR_LABEL("Animation")
/// The player's clips while it is equipped, by slot.
PE2_BIND(bank, animation_bank)
#if PE2_OPTION(WEAPON_INDEX) == WEAPON_INDEX_SP12
PE2_BIND_ARG(bank, _symbol, gSp12PlayerAnimationBank)
#else
PE2_BIND_ARG(bank, _symbol, gPa3PlayerAnimationBank)
#endif
PE2_VALUE(bank)

/// A sound of the bank of the weapon being built, by slot (`WEAPON_SOUND_SLOT_*`, `SHOTGUN_SOUND_SLOT_*`, or one below).
#define SOUND(slot) SOUND_WEAPON(PE2_OPTION(WEAPON_INDEX), (slot))

enum {
    /// The pump: the shotguns' banks keep it in the reload-start slot, which their reload plays as its last cue.
    SOUND_PUMP = WEAPON_SOUND_SLOT_RELOAD_START,
};

void pa3AttackState(Task* playerTask)
{
    enum {
        PHASE_PREPARE    = 0,
        PHASE_WAIT_READY = 1,
        PHASE_FIRE       = 2,
        PHASE_IMPACT     = 3,
        PHASE_PUMP_DELAY = 4,
        PHASE_RECOVER    = 5,
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
        case PHASE_PREPARE:
            actor->state             = PLAYER_ACTOR_STATE_ATTACK;
            actor->statePhase        = PHASE_WAIT_READY;
            actor->attackCancelTicks = PE2_OPTION(ATTACK_CANCEL_FRAMES);
            actor->mode              = GAME_ACTOR_MODE_NORMAL;
            actor->turnRateIndex     = 0;
            actor->animationState    = 0;
            actor->rumblePosted      = 0;
            actor->stateTimer        = PE2_OPTION(PUMP_DELAY_FRAMES);
            playerActorSetWeaponAttackFlags(playerTask, 0, 0);
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
            /* fallthrough */
        case PHASE_WAIT_READY:
            if (animationGetCurrentRecord(&actor->animationContext, actor->animationSlots + 1) !=
                NULL) {
                actor->statePhase++;
            }
            break;
        case PHASE_FIRE:
            actor->statePhase++;
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
            equipmentConsumeWeaponLoad(WEAPON_ITEM_ID(PE2_OPTION(WEAPON_INDEX)), EQUIPMENT_WEAPON_LOAD_CONSUME_PRIMARY);
            worldCoordPlaySound(playerTask->extra.tmd->coords,
                                WEAPON_SOUND_VARIANT(gPlayerStatus.weaponSlotItem - SHOTGUN_ROUND_FIRST) | SOUND(SHOTGUN_SOUND_SLOT_SHOT), 1);
            effectSpawn(EFFECT_SHOTGUN_MUZZLE_FLASH,
                        actor->equipmentTasks[1]->extra.tmd->coords,
                        (gPlayerStatus.weaponSlotItem << 0x10) | PE2_OPTION(WEAPON_INDEX), NULL);
            playerActorPlayChildSlotsWithBlend(playerTask, PLAYER_ANIMATION_FIRE, 1, 3);
            break;
        case PHASE_IMPACT:
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
                                            WEAPON_SOUND_VARIANT(gPlayerStatus.weaponSlotItem - SHOTGUN_ROUND_FIRST) | SOUND(SHOTGUN_SOUND_SLOT_FIREFLY_IMPACT), 1);
                    }
                } else if (gridImpact != 0) {
                    worldCoordPlaySound(impactCoord, SOUND_BULLET_IMPACT, 1);
                }
            }
            /* fallthrough */
        case PHASE_PUMP_DELAY:
            // Pump timing and recovery checks run together through the fallthrough.
            if (--actor->stateTimer == 0) {
                actor->statePhase++;
                worldCoordPlaySound(playerTask->extra.tmd->coords,
                                    WEAPON_SOUND_VARIANT(gPlayerStatus.weaponSlotItem - SHOTGUN_ROUND_FIRST) | SOUND(SOUND_PUMP), 0);
            }
            /* fallthrough */
        case PHASE_RECOVER:
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
