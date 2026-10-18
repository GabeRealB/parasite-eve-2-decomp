#include "weapons/as12.h"

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
#include "main/sound.h"
#include "main/session_types.h"
#include "main/task_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "weapons/weapon.h"

enum { AS12_AMMUNITION_BUCKSHOT  = 13,
       AS12_FIREFLY_IMPACT_SOUND = SOUND_WEAPON(15, 4) };

/// Requests the AS12 Firefly impact sound at the first recorded weapon contact.
///
/// `firstContact` is a readable `const WorldCollisionContact*` supplying signed-halfword
/// XYZ in view-space game coordinates; `impactCoord` is a writable `GfxCoord*`.
/// The contact need not be the surface selected by the impact picker. Requires
/// the Firefly load selected in live player state and a disjoint writable node.
/// Writes only its three cached translation words, with no jitter, then reads
/// that origin synchronously for sound and latches combat noise. The retained
/// cached rotation is loaded into the GTE, but multiplies a zero input point.
/// Leaves the contact, local matrix, parent and composition stamp untouched.
/// Requires initialized projection/scratch state; retains neither pointer.
///
/// Both pointer arguments repeat and must have no side effects. Reads live
/// `gPlayerStatus.weaponSlotItem`; captures no caller locals. The block is one
/// statement with no return/break; invoke it as a standalone statement inside braces.
#define AS12_PLAY_FIREFLY_IMPACT(firstContact, impactCoord)                                                                                                                          \
    {                                                                                                                                                                                \
        enum { AS12_IMPACT_AMMUNITION_BANK_SHIFT = 24,                                                                                                                               \
               AS12_IMPACT_SIGNAL_NOISE          = 1 };                                                                                                                                       \
        (impactCoord)->workm.t[0] = (firstContact)->point.vx;                                                                                                                        \
        (impactCoord)->workm.t[1] = (firstContact)->point.vy;                                                                                                                        \
        (impactCoord)->workm.t[2] = (firstContact)->point.vz;                                                                                                                        \
        worldCoordPlaySound((impactCoord),                                                                                                                                           \
                            ((gPlayerStatus.weaponSlotItem - AS12_AMMUNITION_BUCKSHOT) << AS12_IMPACT_AMMUNITION_BANK_SHIFT) | AS12_FIREFLY_IMPACT_SOUND, AS12_IMPACT_SIGNAL_NOISE); \
    }

void as12AttackState(Task* playerTask)
{
    enum {
        AS12_PHASE_PREPARE             = 0,
        AS12_PHASE_WAIT_READY          = 1,
        AS12_PHASE_ARM_SHOT            = 2,
        AS12_PHASE_FIRE                = 3,
        AS12_PHASE_IMPACT              = 4,
        AS12_PHASE_RECOVER             = 5,
        AS12_PLAYER_ATTACK_STATE       = 4,
        AS12_ANIMATION_READY           = 9,
        AS12_ANIMATION_PRIMARY         = 0xA,
        AS12_READY_BLEND_FRAMES        = 1,
        AS12_IMPACT_SOUND              = SOUND_COMMON(0x17),
        AS12_WEAPON_ID                 = 15,
        AS12_MOVING_READY_BLEND_FRAMES = 5,
        AS12_COOLDOWN_FRAMES           = 0x21,
        AS12_CANCEL_FRAMES             = 0x16,
        AS12_AMMUNITION_FIREFLY        = 14,
        AS12_FIRE_SOUND                = SOUND_WEAPON(15, 5),
        AS12_EFFECT_AMMUNITION_SHIFT   = 16,
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
            actor->state          = AS12_PLAYER_ATTACK_STATE;
            actor->mode           = GAME_ACTOR_MODE_NORMAL;
            actor->turnRateIndex  = 0;
            actor->animationState = 0;
            actor->statePhase++;
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= WORLD_COLLISION_BODY_CLIP_TO_GRID_CONTACT;
            if (gPlayerStatus.weaponSlotItem == AS12_AMMUNITION_FIREFLY) {
                actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= WORLD_COLLISION_BODY_SINGLE_CONTACT;
            } else {
                actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= ~WORLD_COLLISION_BODY_SINGLE_CONTACT;
            }
            readyBlendFrames = AS12_READY_BLEND_FRAMES;
            if (((u16)actor->movementMode | actor->turnSign) != 0) {
                readyBlendFrames = AS12_MOVING_READY_BLEND_FRAMES;
            }
            playerActorPlayChildSlotsWithBlend(playerTask, AS12_ANIMATION_READY, 0, readyBlendFrames);
            actor->movementMode = 0;
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
            equipmentConsumeWeaponLoad(WEAPON_ITEM_ID(AS12_WEAPON_ID), EQUIPMENT_WEAPON_LOAD_CONSUME_PRIMARY);
            worldCoordPlaySound(playerTask->extra.tmd->coords,
                                ((gPlayerStatus.weaponSlotItem - AS12_AMMUNITION_BUCKSHOT) << 0x18) | AS12_FIRE_SOUND, 1);
            effectSpawn(EFFECT_SHOTGUN_MUZZLE_FLASH,
                        actor->equipmentTasks[1]->extra.tmd->coords,
                        (gPlayerStatus.weaponSlotItem << AS12_EFFECT_AMMUNITION_SHIFT) | AS12_WEAPON_ID, NULL);
            playerActorResetChildSlots(playerTask, AS12_ANIMATION_PRIMARY);
            break;
        case AS12_PHASE_IMPACT:
            // Firefly uses the first contact position; other non-Buckshot loads use the picked impact.
            actor->attackCancelTicks = AS12_CANCEL_FRAMES;
            actor->statePhase++;
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
            if (gPlayerStatus.weaponSlotItem != AS12_AMMUNITION_BUCKSHOT) {
                impactFound = playerActorSpawnWeaponImpact(actor->weaponContacts, rootCoord, impactCoord);
                if (gPlayerStatus.weaponSlotItem == AS12_AMMUNITION_FIREFLY) {
                    if (impactFound != 0 || worldCollisionCountContactsByKind(actor->weaponContacts, WORLD_COLLISION_CONTACT_ENEMY_BODY) != 0) {
                        AS12_PLAY_FIREFLY_IMPACT(&actor->weaponContacts[0], impactCoord);
                    }
                } else if (impactFound != 0) {
                    worldCoordPlaySound(impactCoord, AS12_IMPACT_SOUND, 1);
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
            if (playerActorIsSlotAdvancingLinearly(playerTask, gPlayerActorAttachmentParentParts[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId][1], 0, 0) == 0 ||
                ((actor->padHeld & actor->actionPadMask) != 0 && actor->attackCancelTicks == 0)) {
                playerActorFinishWeaponAttack(playerTask);
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BLOCK(GfxCoord);
}
#undef AS12_PLAY_FIREFLY_IMPACT

/// The model `gAs12Model`: generated from its asset (weapons/as12/model) by tools/buildsteps/gen_model_source.py.
#include "assets/weapons/as12/model.inc"

/// The player's clips this package carries, in the package's order, and its animation bank (weapons/as12/bank):
/// generated from the clip and bank assets by tools/buildsteps/gen_animation_source.py.
#include "assets/weapons/as12/clips.inc"
#include "assets/weapons/as12/bank.inc"
