#include "weapons/p229.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "gte.h"
#include "types.h"

#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/effects.h"
#include "gameplay/items.h"
#include "gameplay/light.h"
#include "gameplay/player_actor.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gamemain.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/scratch.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"

#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "weapons/weapon.h"

#include "overlay.h"

#include "../../shared/effects/muzzle_flash.h"

/// Muzzle offset of the P229, in the firing hand's coordinate frame.
static SVECTOR _gMuzzleOffset = { 0, 0x140, 0x20, 0 };

static s16 _gMuzzleFlashAngles[MUZZLE_FLASH_STREAK_COUNT];

/// Selects the P229 secondary-fire flash offset in weapon-root coordinates.
///
/// Binds a carrier-owned `SVECTOR`: XYZ are signed integer model units, and
/// `pad` is unused. The task copies each component once during initialization.
/// Keep this binding scoped to the task-fragment inclusion.
#define MUZZLE_FLASH_OFFSET _gMuzzleOffset
#include "../../shared/effects/muzzle_flash_task.inc.c"
#undef MUZZLE_FLASH_OFFSET

void p229MuzzleFlashTask(Task* task)
{
    _muzzleFlashTask(task);
}

#include "../../shared/effects/muzzle_flash.inc.c"

void p229AttackState(Task* playerTask)
{
    enum {
        P229_PHASE_PREPARE             = 0,
        P229_PHASE_WAIT_READY          = 1,
        P229_PHASE_FIRE                = 2,
        P229_PHASE_PRIMARY_IMPACT      = 3,
        P229_PHASE_SECONDARY_IMPACT    = 4,
        P229_PHASE_RECOVER             = 5,
        P229_PLAYER_ATTACK_STATE       = 4,
        P229_ANIMATION_READY           = 9,
        P229_ANIMATION_PRIMARY         = 0xA,
        P229_READY_BLEND_FRAMES        = 1,
        P229_MOVING_READY_BLEND_FRAMES = 5,
        P229_IMPACT_SOUND              = SOUND_COMMON(0x17),
        P229_SECONDARY_SPREAD_RADIUS   = 0xC00,
        P229_PRIMARY_CANCEL_FRAMES     = 0xA,
        P229_SECONDARY_CANCEL_FRAMES   = 6,
        P229_SECONDARY_COOLDOWN_FRAMES = 0x12,
        P229_PRIMARY_COOLDOWN_FRAMES   = 0xC,
        P229_PRIMARY_KEY_BASE          = WORLD_COLLISION_CONTACT_ATTACK | (5 << 8),
        P229_SECONDARY_KEY             = WORLD_COLLISION_CONTACT_ATTACK | (5 << 8) | 0x16,
        P229_WEAPON_ID                 = 5,
        P229_PRIMARY_SOUND             = SOUND_WEAPON(5, 4),
        P229_SECONDARY_SOUND           = SOUND_WEAPON(5, 5),
    };
    GameActor*             actor;
    GfxCoord*              rootCoord;
    GfxCoord*              impactCoord;
    WorldCollisionCapsule* weaponShape;
    EffectWork*            flashWork;
    s32                    readyBlendFrames;
    s16                    cooldownFrames;

    actor       = playerTask->work;
    rootCoord   = playerTask->extra.tmd->coords;
    weaponShape = &actor->weaponShape;
    impactCoord = SCRATCH_STACK_RESERVE_BLOCK(GfxCoord);
    switch (actor->statePhase) {
        case P229_PHASE_PREPARE:
            actor->state                                          = P229_PLAYER_ATTACK_STATE;
            actor->mode                                           = GAME_ACTOR_MODE_NORMAL;
            actor->turnRateIndex                                  = 0;
            actor->animationState                                 = 0;
            actor->rumblePosted                                   = 0;
            actor->statePhase                                    += 1;
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= WORLD_COLLISION_BODY_CLIP_TO_GRID_CONTACT;
            readyBlendFrames                                      = P229_READY_BLEND_FRAMES;
            if (((u16)actor->movementMode | actor->turnSign) != 0) {
                readyBlendFrames = P229_MOVING_READY_BLEND_FRAMES;
            }
            playerActorPlayChildSlotsWithBlend(playerTask, P229_ANIMATION_READY, 0, readyBlendFrames);
            actor->movementMode = 0;
            break;
        case P229_PHASE_WAIT_READY:
            if (animationGetCurrentRecord(&actor->animationContext, actor->animationSlots + 1) !=
                NULL) {
                actor->statePhase++;
            }
            break;
        case P229_PHASE_FIRE:
            // Primary fire clips at its first contact; secondary fire keeps a wide shape.
            if (actor->attackButton == PLAYER_ACTOR_ATTACK_BUTTON_PRIMARY) {
                actor->statePhase                                     = P229_PHASE_PRIMARY_IMPACT;
                actor->attackCancelTicks                              = P229_PRIMARY_CANCEL_FRAMES;
                actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].key    = gPlayerStatus.weaponSlotItem | P229_PRIMARY_KEY_BASE;
                weaponShape->end0Radius                               = weaponShape->end1Radius;
                actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= WORLD_COLLISION_BODY_SINGLE_CONTACT;
                playerActorSetWeaponAttackFlags(playerTask, 0, 0);
                equipmentConsumeWeaponLoad(WEAPON_ITEM_ID(P229_WEAPON_ID), EQUIPMENT_WEAPON_LOAD_CONSUME_PRIMARY);
                worldCoordPlaySound(playerTask->extra.tmd->coords, P229_PRIMARY_SOUND, 0);
                effectSpawn(EFFECT_HANDGUN_MUZZLE_FLASH,
                            actor->equipmentTasks[1]->extra.tmd->coords, P229_WEAPON_ID,
                            NULL);
                playerActorResetChildSlots(playerTask, P229_ANIMATION_PRIMARY);
            } else {
                actor->statePhase                                     = P229_PHASE_SECONDARY_IMPACT;
                actor->attackCancelTicks                              = P229_SECONDARY_CANCEL_FRAMES;
                actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].key    = P229_SECONDARY_KEY;
                weaponShape->end0Radius                               = P229_SECONDARY_SPREAD_RADIUS;
                actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_SINGLE_CONTACT);
                playerActorSetWeaponAttackFlags(playerTask, 0, 1);
                equipmentConsumeWeaponLoad(WEAPON_ITEM_ID(P229_WEAPON_ID), EQUIPMENT_WEAPON_LOAD_CONSUME_SECONDARY);
                worldCoordPlaySound(playerTask->extra.tmd->coords, P229_SECONDARY_SOUND, 0);
                flashWork = effectSpawn(EFFECT_P229_MUZZLE_FLASH,
                                        actor->equipmentTasks[1]->extra.tmd->coords, P229_WEAPON_ID,
                                        NULL);
                if (flashWork != NULL) {
                    taskReparent(actor->equipmentTasks[1], flashWork->task);
                }
            }
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
            break;
        case P229_PHASE_PRIMARY_IMPACT:
        case P229_PHASE_SECONDARY_IMPACT:
            if (actor->statePhase == P229_PHASE_PRIMARY_IMPACT && playerActorSpawnWeaponImpact(actor->weaponContacts, rootCoord, impactCoord) != 0) {
                worldCoordPlaySound(impactCoord, P229_IMPACT_SOUND, 0);
            }
            actor->statePhase                                     = P229_PHASE_RECOVER;
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
            /* fallthrough */
        case P229_PHASE_RECOVER:
            if (actor->attackCancelTicks != 0) {
                actor->attackCancelTicks--;
            }
            if (playerActorIsSlotAdvancingLinearly(playerTask, gPlayerActorAttachmentParentParts[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId][1], 0, 0) == 0 ||
                ((actor->padHeld & actor->actionPadMask) != 0 && actor->attackCancelTicks == 0)) {
                cooldownFrames = P229_SECONDARY_COOLDOWN_FRAMES;
                if (actor->attackButton == PLAYER_ACTOR_ATTACK_BUTTON_PRIMARY) {
                    cooldownFrames = P229_PRIMARY_COOLDOWN_FRAMES;
                }
                actor->attackControl.cooldownTicks = cooldownFrames;
                playerActorFinishWeaponAttack(playerTask);
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BLOCK(GfxCoord);
}

/// The held weapon model, `gP229WeaponModel`, with its arrays: generated from the asset
/// (weapons/p229/model.usda, textured through weapons/p229/binding) by tools/buildsteps/gen_model_source.py.
#include "assets/weapons/p229/model.inc"

/// The player's clips this package carries, in the package's order, and its animation bank: generated
/// from the clip assets (characters/aya) and the bank weapons/p229/bank by
/// tools/buildsteps/gen_animation_source.py.
#include "assets/weapons/p229/clips.inc"
#include "assets/weapons/p229/bank.inc"

/// Retained streak directions shared by every live flash in this weapon package.
///
/// Angles use 4096 units per turn, one random direction in each quadrant.
/// Each newly initialized flash replaces the directions older flashes read.
static s16 _gMuzzleFlashAngles[MUZZLE_FLASH_STREAK_COUNT] = { 0, 0, 0, 0 };
