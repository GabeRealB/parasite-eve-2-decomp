#include "weapons/as12.h"

#include "config.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

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

void as12AttackState(Task* playerTask)
{
    enum {
        AS12_PHASE_PREPARE           = 0,
        AS12_PHASE_WAIT_READY        = 1,
        AS12_PHASE_ARM_SHOT          = 2,
        AS12_PHASE_FIRE              = 3,
        AS12_PHASE_IMPACT            = 4,
        AS12_PHASE_RECOVER           = 5,
        AS12_EFFECT_AMMUNITION_SHIFT = 16,
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
        case AS12_PHASE_PREPARE:
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
            readyBlendFrames = AS12_READY_BLEND_FRAMES;
            if (((u16)actor->movementMode | actor->turnSign) != 0) {
                readyBlendFrames = AS12_MOVING_READY_BLEND_FRAMES;
            }
            playerActorPlayChildSlotsWithBlend(playerTask, PLAYER_ANIMATION_AIM_IDLE, 0, readyBlendFrames);
            actor->movementMode = GAME_ACTOR_MOVEMENT_STOPPED;
            break;
        case AS12_PHASE_WAIT_READY:
            if (animationGetCurrentRecord(&actor->animationContext, actor->animationSlots + 1) !=
                NULL) {
                actor->statePhase++;
            }
            break;
        case AS12_PHASE_ARM_SHOT:
        fire:
            actor->statePhase                  = AS12_PHASE_FIRE;
            actor->attackControl.cooldownTicks = AS12_COOLDOWN_FRAMES;
            actor->rumblePosted                = 0;
            playerActorSetWeaponAttackFlags(playerTask, 0, actor->attackButton != PLAYER_ACTOR_ATTACK_BUTTON_PRIMARY);
            /* fallthrough */
        case AS12_PHASE_FIRE:
            actor->statePhase++;
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
            equipmentConsumeWeaponLoad(WEAPON_ITEM_ID(WEAPON_INDEX_AS12), EQUIPMENT_WEAPON_LOAD_CONSUME_PRIMARY);
            worldCoordPlaySound(playerTask->extra.tmd->coords,
                                WEAPON_SOUND_VARIANT(gPlayerStatus.weaponSlotItem - SHOTGUN_ROUND_FIRST) | AS12_SOUND(SHOTGUN_SOUND_SLOT_SHOT), 1);
            effectSpawn(EFFECT_SHOTGUN_MUZZLE_FLASH,
                        actor->equipmentTasks[1]->extra.tmd->coords,
                        (gPlayerStatus.weaponSlotItem << AS12_EFFECT_AMMUNITION_SHIFT) | WEAPON_INDEX_AS12, NULL);
            playerActorResetChildSlots(playerTask, PLAYER_ANIMATION_FIRE);
            break;
        case AS12_PHASE_IMPACT:
            // Firefly uses the first contact position; other non-Buckshot loads use the picked impact.
            actor->attackCancelTicks = AS12_CANCEL_FRAMES;
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
                                            WEAPON_SOUND_VARIANT(gPlayerStatus.weaponSlotItem - SHOTGUN_ROUND_FIRST) | AS12_SOUND(SHOTGUN_SOUND_SLOT_FIREFLY_IMPACT), 1);
                    }
                } else if (impactFound != 0) {
                    worldCoordPlaySound(impactCoord, SOUND_BULLET_IMPACT, 1);
                }
            }
            /* fallthrough */
        case AS12_PHASE_RECOVER:
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

/// The model `gAs12Model`: generated from its asset (weapons/as12/model) by tools/buildsteps/gen_model_source.py.
#include "assets/weapons/as12/model.inc"

/// The player's clips this package carries, in the package's order, and its animation bank (weapons/as12/bank):
/// generated from the clip and bank assets by tools/buildsteps/gen_animation_source.py.
#include "assets/weapons/as12/clips.inc"
#include "assets/weapons/as12/bank.inc"
