#include "weapons/mp5a5.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"
#include "types.h"

#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
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
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "weapons/weapon.h"

#include "overlay.h"

#include "../../shared/muzzle_flash.h"

/// The weapon's index: 0x1E for the MP5A5, 0x1F and 0x20 for its two upgrades.
/// The three packages are this source built once each, and each declares its
/// index in the manifest. It keys the firing sounds, the shot id and the item.
#ifndef WEAPON_ID
#error "WEAPON_ID is a per-package build parameter"
#endif

/// MP5A5 alternate-fire flash origin in integer weapon-root coordinates.
static SVECTOR _gMp5a5MuzzleFlashOffset = { 0, 576, 64, 0 };

static s16 _gMuzzleFlashAngles[MUZZLE_FLASH_STREAK_COUNT];

#define MUZZLE_FLASH_OFFSET _gMp5a5MuzzleFlashOffset
#include "../../shared/muzzle_flash_task.inc.c"
#undef MUZZLE_FLASH_OFFSET

void mp5a5MuzzleFlashTask(Task* task)
{
    _muzzleFlashTask(task);
}

#include "../../shared/muzzle_flash_core.inc.c"

#include "../../shared/muzzle_flash_streak.inc.c"

void mp5a5AttackState(Task* playerTask)
{
    enum {
        MP5A5_PHASE_PREPARE             = 0,
        MP5A5_PHASE_WAIT_READY          = 1,
        MP5A5_PHASE_ARM_SHOT            = 2,
        MP5A5_PHASE_SHOT_DELAY          = 3,
        MP5A5_PHASE_PRIMARY_IMPACT      = 4,
        MP5A5_PHASE_SECONDARY_IMPACT    = 5,
        MP5A5_PHASE_RECOVER             = 6,
        MP5A5_PLAYER_ATTACK_STATE       = 4,
        MP5A5_ANIMATION_READY           = 9,
        MP5A5_ANIMATION_PRIMARY         = 0xA,
        MP5A5_READY_BLEND_FRAMES        = 1,
        MP5A5_MOVING_READY_BLEND_FRAMES = 5,
        MP5A5_IMPACT_SOUND              = SOUND_COMMON(0x17),
        MP5A5_SECONDARY_SPREAD_RADIUS   = 0xC00,
        MP5A5_SECONDARY_COOLDOWN_FRAMES = 0x12,
        MP5A5_SECONDARY_ACTIVE_FRAMES   = 0x12,
        MP5A5_PRIMARY_SHOT_DELAY_FRAMES = 3,
        MP5A5_SECONDARY_KEY_BASE        = WORLD_COLLISION_CONTACT_ATTACK | 0x16,
        MP5A5_PRIMARY_SOUND_BASE        = SOUND_WEAPON(0, 4),
        MP5A5_SECONDARY_SOUND_BASE      = SOUND_WEAPON(0, 5),
    };
    GameActor*             actor;
    GfxCoord*              rootCoord;
    GfxCoord*              impactCoord;
    WorldCollisionCapsule* weaponShape;
    EffectWork*            flashWork;
    s32                    readyBlendFrames;

    impactCoord = SCRATCH_STACK_RESERVE_BLOCK(GfxCoord);
    actor       = playerTask->work;
    rootCoord   = playerTask->extra.tmd->coords;
    switch (actor->statePhase) {
        case MP5A5_PHASE_PREPARE:
            actor->state                                          = MP5A5_PLAYER_ATTACK_STATE;
            actor->turnRateIndex                                  = 2;
            actor->mode                                           = GAME_ACTOR_MODE_NORMAL;
            actor->animationState                                 = 0;
            actor->stateTimer                                     = 1;
            actor->statePhase                                    += 1;
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= WORLD_COLLISION_BODY_CLIP_TO_GRID_CONTACT;
            readyBlendFrames                                      = MP5A5_READY_BLEND_FRAMES;
            if (((u16)actor->movementMode | actor->turnSign) != 0) {
                readyBlendFrames = MP5A5_MOVING_READY_BLEND_FRAMES;
            }
            playerActorPlayChildSlotsWithBlend(playerTask, MP5A5_ANIMATION_READY, 0, readyBlendFrames);
            actor->movementMode = 0;
            break;
        case MP5A5_PHASE_WAIT_READY:
            if (animationGetCurrentRecord(&actor->animationContext, actor->animationSlots + 1) !=
                NULL) {
                actor->statePhase++;
            }
            break;
        case MP5A5_PHASE_ARM_SHOT:
        fire:
            actor->statePhase   = MP5A5_PHASE_SHOT_DELAY;
            actor->rumblePosted = 0;
            /* fallthrough */
        case MP5A5_PHASE_SHOT_DELAY:
            if (--actor->stateTimer == 0) {
                weaponShape = &actor->weaponShape;
                // Primary fire clips at its first contact; secondary fire keeps a wide shape.
                if (actor->attackButton == PLAYER_ACTOR_ATTACK_BUTTON_PRIMARY) {
                    actor->statePhase                                     = MP5A5_PHASE_PRIMARY_IMPACT;
                    actor->stateTimer                                     = MP5A5_PRIMARY_SHOT_DELAY_FRAMES;
                    actor->attackControl.cooldownTicks                    = 0;
                    actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].key    = gPlayerStatus.weaponSlotItem | WORLD_COLLISION_CONTACT_ATTACK | (WEAPON_ID << 8);
                    weaponShape->end0Radius                               = weaponShape->end1Radius;
                    actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= WORLD_COLLISION_BODY_SINGLE_CONTACT;
                    playerActorSetWeaponAttackFlags(playerTask, 0, 1);
                    worldCoordPlaySound(playerTask->extra.tmd->coords, MP5A5_PRIMARY_SOUND_BASE | (WEAPON_ID << 16), 1);
                    effectSpawn(EFFECT_HANDGUN_MUZZLE_FLASH,
                                actor->equipmentTasks[1]->extra.tmd->coords,
                                WEAPON_ID, NULL);
                    equipmentConsumeWeaponLoad(WEAPON_ITEM(WEAPON_ID), EQUIPMENT_WEAPON_LOAD_CONSUME_PRIMARY);
                    playerActorPlayChildSlotsWithBlend(playerTask, MP5A5_ANIMATION_PRIMARY, 0, 2);
                } else {
                    actor->statePhase                                     = MP5A5_PHASE_SECONDARY_IMPACT;
                    actor->attackControl.cooldownTicks                    = MP5A5_SECONDARY_COOLDOWN_FRAMES;
                    actor->stateTimer                                     = MP5A5_SECONDARY_ACTIVE_FRAMES;
                    actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].key    = MP5A5_SECONDARY_KEY_BASE | (WEAPON_ID << 8);
                    weaponShape->end0Radius                               = MP5A5_SECONDARY_SPREAD_RADIUS;
                    actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_SINGLE_CONTACT);
                    playerActorSetWeaponAttackFlags(playerTask, 0, 0);
                    worldCoordPlaySound(playerTask->extra.tmd->coords, MP5A5_SECONDARY_SOUND_BASE | (WEAPON_ID << 16), 0);
                    equipmentConsumeWeaponLoad(WEAPON_ITEM(WEAPON_ID), EQUIPMENT_WEAPON_LOAD_CONSUME_SECONDARY);
                    flashWork = effectSpawn(EFFECT_MP5A5_ALT_FIRE_MUZZLE_FLASH,
                                            actor->equipmentTasks[1]->extra.tmd->coords,
                                            WEAPON_ID, NULL);
                    if (flashWork != NULL) {
                        taskReparent(actor->equipmentTasks[1], flashWork->task);
                    }
                }
                actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
            }
            break;
        case MP5A5_PHASE_PRIMARY_IMPACT:
        case MP5A5_PHASE_SECONDARY_IMPACT:
            if (actor->statePhase < MP5A5_PHASE_SECONDARY_IMPACT && playerActorSpawnWeaponImpact(actor->weaponContacts, rootCoord, impactCoord) != 0) {
                worldCoordPlaySound(impactCoord, MP5A5_IMPACT_SOUND, 1);
            }
            actor->statePhase                                     = MP5A5_PHASE_RECOVER;
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
            /* fallthrough */
        case MP5A5_PHASE_RECOVER:
            // Reuse the shot-entry path when primary input survives the cooldown.
            if (playerActorReadAttackButton(playerTask) == PLAYER_ACTOR_ATTACK_BUTTON_PRIMARY && playerActorQueryWeaponLoads(PLAYER_ACTOR_WEAPON_LOAD_PRIMARY) > 0 && actor->attackControl.cooldownTicks == 0) {
                goto fire;
            }
            if (playerActorIsSlotAdvancingLinearly(playerTask, D_80112E04[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId][1], 0, 0) == 0) {
                playerActorFinishWeaponAttack(playerTask);
            }
            break;
    }
    playerActorTrackLockTarget(playerTask);
    SCRATCH_STACK_RELEASE_BLOCK(GfxCoord);
}

/// Initial identity transform of the weapon's sole, self-parented root part.
static TmdBone _gMp5a5WeaponModelSkeleton[] = {
#include "assets/mp5a5_model_01318_skeleton.inc"
};

/// Number of part-local vertices belonging to the weapon's root part.
static u32 _gMp5a5WeaponModelPartVerts[] = {
#include "assets/mp5a5_model_01318_partVerts.inc"
};

/// Equipped MP5A5 vertices in integer weapon-root coordinates.
static SVECTOR _gMp5a5WeaponModelVerts[] = {
#include "assets/mp5a5_model_01318_verts.inc"
};

/// Equipped MP5A5 lighting normals, indexed independently of its vertices.
static SVECTOR _gMp5a5WeaponModelNormals[] = {
#include "assets/mp5a5_model_01318_normals.inc"
};

/// Writable draw commands for the weapon's single part, including stream terminators.
static u32 _gMp5a5WeaponModelStream[] = {
#include "assets/mp5a5_model_01318_stream.inc"
};

TmdSource gMp5a5WeaponModel = {
    0,
    sizeof(POLY_FT4) + 2 * sizeof(POLY_GT3) + 52 * sizeof(POLY_GT4),
    0,
    ARRAY_SIZE(_gMp5a5WeaponModelSkeleton),
    _gMp5a5WeaponModelPartVerts,
    _gMp5a5WeaponModelVerts,
    _gMp5a5WeaponModelNormals,
    _gMp5a5WeaponModelSkeleton,
    _gMp5a5WeaponModelStream,
};

/// Encoding-1 local translations and rotations for the MP5A5 idle pose.
static AnimationPackedPose _gMp5a5IdleAnimationBank1[] = {
#include "assets/mp5a5_animation_01AF0_bank1.inc"
};

/// Encoding-4 rotations for the MP5A5 idle pose, in eight-unit angle steps.
static AnimationPackedRotation _gMp5a5IdleAnimationBank4[] = {
#include "assets/mp5a5_animation_01AF0_bank4.inc"
};

/// Keyframes and control records holding the MP5A5 idle pose on each player-rig track.
static AnimationRecord _gMp5a5IdleAnimationRecords[] = {
#include "assets/mp5a5_animation_01AF0_records.inc"
};

/// Absolute record starts for the idle pose's 19 player-rig tracks.
///
/// The final serialized halfword is retained but is not another track start.
static u16 _gMp5a5IdleAnimationIndices[] = {
#include "assets/mp5a5_animation_01AF0_indices.inc"
};

/// MP5A5 player's stationary idle pose, also reused by bank slots 21, 22 and 41.
///
/// Provides 19 player-rig tracks; these do not animate the one-part weapon model.
/// Playback borrows the data for the lifetime of the loaded weapon package.
static AnimationSet _gMp5a5IdleAnimation = {
    _gMp5a5IdleAnimationRecords,
    _gMp5a5IdleAnimationIndices,
    { NULL, _gMp5a5IdleAnimationBank1, NULL, NULL, _gMp5a5IdleAnimationBank4, NULL, NULL, NULL },
};

/// Encoding-1 local translations and rotations for MP5A5 forward walking.
static AnimationPackedPose _gMp5a5WalkAnimationBank1[] = {
#include "assets/mp5a5_animation_02194_bank1.inc"
};

/// Encoding-4 rotations for MP5A5 forward walking, in eight-unit angle steps.
static AnimationPackedRotation _gMp5a5WalkAnimationBank4[] = {
#include "assets/mp5a5_animation_02194_bank4.inc"
};

/// Forward-walk keyframes and control records; segment durations count normal-rate frames.
static AnimationRecord _gMp5a5WalkAnimationRecords[] = {
#include "assets/mp5a5_animation_02194_records.inc"
};

/// Absolute record starts for the forward-walk clip's 19 player-rig tracks.
///
/// The final serialized halfword is retained but is not another track start.
static u16 _gMp5a5WalkAnimationIndices[] = {
#include "assets/mp5a5_animation_02194_indices.inc"
};

/// MP5A5 player's forward-walk clip, also selected for unarmed walking in bank slot 19.
///
/// Provides 19 player-rig tracks; these do not animate the one-part weapon model.
/// Playback borrows the data for the lifetime of the loaded weapon package.
static AnimationSet _gMp5a5WalkAnimation = {
    _gMp5a5WalkAnimationRecords,
    _gMp5a5WalkAnimationIndices,
    { NULL, _gMp5a5WalkAnimationBank1, NULL, NULL, _gMp5a5WalkAnimationBank4, NULL, NULL, NULL },
};

/// Encoding-1 local translations and rotations for MP5A5 backward walking.
static AnimationPackedPose _gMp5a5BackwardWalkAnimationBank1[] = {
#include "assets/mp5a5_animation_02A6C_bank1.inc"
};

/// Encoding-4 rotations for MP5A5 backward walking, in eight-unit angle steps.
static AnimationPackedRotation _gMp5a5BackwardWalkAnimationBank4[] = {
#include "assets/mp5a5_animation_02A6C_bank4.inc"
};

/// Backward-walk keyframes and control records; segment durations count normal-rate frames.
static AnimationRecord _gMp5a5BackwardWalkAnimationRecords[] = {
#include "assets/mp5a5_animation_02A6C_records.inc"
};

/// Absolute record starts for the backward-walk clip's 19 player-rig tracks.
///
/// The final serialized halfword is retained but is not another track start.
static u16 _gMp5a5BackwardWalkAnimationIndices[] = {
#include "assets/mp5a5_animation_02A6C_indices.inc"
};

/// MP5A5 player's backward-walk clip in native bank slot 3.
///
/// Provides 19 player-rig tracks; these do not animate the one-part weapon model.
/// Playback borrows the data for the lifetime of the loaded weapon package.
static AnimationSet _gMp5a5BackwardWalkAnimation = {
    _gMp5a5BackwardWalkAnimationRecords,
    _gMp5a5BackwardWalkAnimationIndices,
    { NULL, _gMp5a5BackwardWalkAnimationBank1, NULL, NULL, _gMp5a5BackwardWalkAnimationBank4, NULL, NULL, NULL },
};

/// Encoding-1 local translations and rotations for MP5A5 running.
static AnimationPackedPose _gMp5a5RunAnimationBank1[] = {
#include "assets/mp5a5_animation_031A0_bank1.inc"
};

/// Encoding-4 rotations for MP5A5 running, in eight-unit angle steps.
static AnimationPackedRotation _gMp5a5RunAnimationBank4[] = {
#include "assets/mp5a5_animation_031A0_bank4.inc"
};

/// Run keyframes and control records; segment durations count normal-rate frames.
static AnimationRecord _gMp5a5RunAnimationRecords[] = {
#include "assets/mp5a5_animation_031A0_records.inc"
};

/// Absolute record starts for the run clip's 19 player-rig tracks.
///
/// The final serialized halfword is retained but is not another track start.
static u16 _gMp5a5RunAnimationIndices[] = {
#include "assets/mp5a5_animation_031A0_indices.inc"
};

/// MP5A5 player's run clip in native bank slot 4.
///
/// Provides 19 player-rig tracks; these do not animate the one-part weapon model.
/// Playback borrows the data for the lifetime of the loaded weapon package.
static AnimationSet _gMp5a5RunAnimation = {
    _gMp5a5RunAnimationRecords,
    _gMp5a5RunAnimationIndices,
    { NULL, _gMp5a5RunAnimationBank1, NULL, NULL, _gMp5a5RunAnimationBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMp5a5Animation03A00Bank1[19] = {
#include "assets/mp5a5_animation_03A00_bank1.inc"
};

static AnimationPackedRotation _gMp5a5Animation03A00Bank4[169] = {
#include "assets/mp5a5_animation_03A00_bank4.inc"
};

static AnimationRecord _gMp5a5Animation03A00Records[290] = {
#include "assets/mp5a5_animation_03A00_records.inc"
};

static u16 _gMp5a5Animation03A00Indices[20] = {
#include "assets/mp5a5_animation_03A00_indices.inc"
};

static AnimationSet _gMp5a5Animation03A00 = {
    _gMp5a5Animation03A00Records,
    _gMp5a5Animation03A00Indices,
    { NULL, _gMp5a5Animation03A00Bank1, NULL, NULL, _gMp5a5Animation03A00Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMp5a5Animation04264Bank1[19] = {
#include "assets/mp5a5_animation_04264_bank1.inc"
};

static AnimationPackedRotation _gMp5a5Animation04264Bank4[170] = {
#include "assets/mp5a5_animation_04264_bank4.inc"
};

static AnimationRecord _gMp5a5Animation04264Records[290] = {
#include "assets/mp5a5_animation_04264_records.inc"
};

static u16 _gMp5a5Animation04264Indices[20] = {
#include "assets/mp5a5_animation_04264_indices.inc"
};

static AnimationSet _gMp5a5Animation04264 = {
    _gMp5a5Animation04264Records,
    _gMp5a5Animation04264Indices,
    { NULL, _gMp5a5Animation04264Bank1, NULL, NULL, _gMp5a5Animation04264Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMp5a5Animation04578Bank1[3] = {
#include "assets/mp5a5_animation_04578_bank1.inc"
};

static AnimationPackedRotation _gMp5a5Animation04578Bank4[69] = {
#include "assets/mp5a5_animation_04578_bank4.inc"
};

static AnimationRecord _gMp5a5Animation04578Records[99] = {
#include "assets/mp5a5_animation_04578_records.inc"
};

static u16 _gMp5a5Animation04578Indices[20] = {
#include "assets/mp5a5_animation_04578_indices.inc"
};

static AnimationSet _gMp5a5Animation04578 = {
    _gMp5a5Animation04578Records,
    _gMp5a5Animation04578Indices,
    { NULL, _gMp5a5Animation04578Bank1, NULL, NULL, _gMp5a5Animation04578Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMp5a5Animation04CD4Bank1[14] = {
#include "assets/mp5a5_animation_04CD4_bank1.inc"
};

static AnimationPackedRotation _gMp5a5Animation04CD4Bank4[156] = {
#include "assets/mp5a5_animation_04CD4_bank4.inc"
};

static AnimationRecord _gMp5a5Animation04CD4Records[253] = {
#include "assets/mp5a5_animation_04CD4_records.inc"
};

static u16 _gMp5a5Animation04CD4Indices[20] = {
#include "assets/mp5a5_animation_04CD4_indices.inc"
};

static AnimationSet _gMp5a5Animation04CD4 = {
    _gMp5a5Animation04CD4Records,
    _gMp5a5Animation04CD4Indices,
    { NULL, _gMp5a5Animation04CD4Bank1, NULL, NULL, _gMp5a5Animation04CD4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMp5a5Animation0545CBank1[16] = {
#include "assets/mp5a5_animation_0545C_bank1.inc"
};

static AnimationPackedRotation _gMp5a5Animation0545CBank4[167] = {
#include "assets/mp5a5_animation_0545C_bank4.inc"
};

static AnimationRecord _gMp5a5Animation0545CRecords[247] = {
#include "assets/mp5a5_animation_0545C_records.inc"
};

static u16 _gMp5a5Animation0545CIndices[20] = {
#include "assets/mp5a5_animation_0545C_indices.inc"
};

static AnimationSet _gMp5a5Animation0545C = {
    _gMp5a5Animation0545CRecords,
    _gMp5a5Animation0545CIndices,
    { NULL, _gMp5a5Animation0545CBank1, NULL, NULL, _gMp5a5Animation0545CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMp5a5Animation05730Bank1[6] = {
#include "assets/mp5a5_animation_05730_bank1.inc"
};

static AnimationPackedRotation _gMp5a5Animation05730Bank4[52] = {
#include "assets/mp5a5_animation_05730_bank4.inc"
};

static AnimationRecord _gMp5a5Animation05730Records[91] = {
#include "assets/mp5a5_animation_05730_records.inc"
};

static u16 _gMp5a5Animation05730Indices[20] = {
#include "assets/mp5a5_animation_05730_indices.inc"
};

static AnimationSet _gMp5a5Animation05730 = {
    _gMp5a5Animation05730Records,
    _gMp5a5Animation05730Indices,
    { NULL, _gMp5a5Animation05730Bank1, NULL, NULL, _gMp5a5Animation05730Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMp5a5Animation05AC0Bank1[7] = {
#include "assets/mp5a5_animation_05AC0_bank1.inc"
};

static AnimationPackedRotation _gMp5a5Animation05AC0Bank4[73] = {
#include "assets/mp5a5_animation_05AC0_bank4.inc"
};

static AnimationRecord _gMp5a5Animation05AC0Records[114] = {
#include "assets/mp5a5_animation_05AC0_records.inc"
};

static u16 _gMp5a5Animation05AC0Indices[20] = {
#include "assets/mp5a5_animation_05AC0_indices.inc"
};

static AnimationSet _gMp5a5Animation05AC0 = {
    _gMp5a5Animation05AC0Records,
    _gMp5a5Animation05AC0Indices,
    { NULL, _gMp5a5Animation05AC0Bank1, NULL, NULL, _gMp5a5Animation05AC0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMp5a5Animation05F48Bank1[9] = {
#include "assets/mp5a5_animation_05F48_bank1.inc"
};

static AnimationPackedRotation _gMp5a5Animation05F48Bank4[104] = {
#include "assets/mp5a5_animation_05F48_bank4.inc"
};

static AnimationRecord _gMp5a5Animation05F48Records[139] = {
#include "assets/mp5a5_animation_05F48_records.inc"
};

static u16 _gMp5a5Animation05F48Indices[20] = {
#include "assets/mp5a5_animation_05F48_indices.inc"
};

static AnimationSet _gMp5a5Animation05F48 = {
    _gMp5a5Animation05F48Records,
    _gMp5a5Animation05F48Indices,
    { NULL, _gMp5a5Animation05F48Bank1, NULL, NULL, _gMp5a5Animation05F48Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMp5a5Animation06144Bank1[3] = {
#include "assets/mp5a5_animation_06144_bank1.inc"
};

static AnimationPackedRotation _gMp5a5Animation06144Bank4[22] = {
#include "assets/mp5a5_animation_06144_bank4.inc"
};

static AnimationRecord _gMp5a5Animation06144Records[76] = {
#include "assets/mp5a5_animation_06144_records.inc"
};

static u16 _gMp5a5Animation06144Indices[20] = {
#include "assets/mp5a5_animation_06144_indices.inc"
};

static AnimationSet _gMp5a5Animation06144 = {
    _gMp5a5Animation06144Records,
    _gMp5a5Animation06144Indices,
    { NULL, _gMp5a5Animation06144Bank1, NULL, NULL, _gMp5a5Animation06144Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMp5a5Animation0641CBank1[6] = {
#include "assets/mp5a5_animation_0641C_bank1.inc"
};

static AnimationPackedRotation _gMp5a5Animation0641CBank4[57] = {
#include "assets/mp5a5_animation_0641C_bank4.inc"
};

static AnimationRecord _gMp5a5Animation0641CRecords[87] = {
#include "assets/mp5a5_animation_0641C_records.inc"
};

static u16 _gMp5a5Animation0641CIndices[20] = {
#include "assets/mp5a5_animation_0641C_indices.inc"
};

static AnimationSet _gMp5a5Animation0641C = {
    _gMp5a5Animation0641CRecords,
    _gMp5a5Animation0641CIndices,
    { NULL, _gMp5a5Animation0641CBank1, NULL, NULL, _gMp5a5Animation0641CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMp5a5Animation066C0Bank1[4] = {
#include "assets/mp5a5_animation_066C0_bank1.inc"
};

static AnimationPackedRotation _gMp5a5Animation066C0Bank4[55] = {
#include "assets/mp5a5_animation_066C0_bank4.inc"
};

static AnimationRecord _gMp5a5Animation066C0Records[82] = {
#include "assets/mp5a5_animation_066C0_records.inc"
};

static u16 _gMp5a5Animation066C0Indices[20] = {
#include "assets/mp5a5_animation_066C0_indices.inc"
};

static AnimationSet _gMp5a5Animation066C0 = {
    _gMp5a5Animation066C0Records,
    _gMp5a5Animation066C0Indices,
    { NULL, _gMp5a5Animation066C0Bank1, NULL, NULL, _gMp5a5Animation066C0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMp5a5Animation068C0Bank1[3] = {
#include "assets/mp5a5_animation_068C0_bank1.inc"
};

static AnimationPackedRotation _gMp5a5Animation068C0Bank4[23] = {
#include "assets/mp5a5_animation_068C0_bank4.inc"
};

static AnimationRecord _gMp5a5Animation068C0Records[76] = {
#include "assets/mp5a5_animation_068C0_records.inc"
};

static u16 _gMp5a5Animation068C0Indices[20] = {
#include "assets/mp5a5_animation_068C0_indices.inc"
};

static AnimationSet _gMp5a5Animation068C0 = {
    _gMp5a5Animation068C0Records,
    _gMp5a5Animation068C0Indices,
    { NULL, _gMp5a5Animation068C0Bank1, NULL, NULL, _gMp5a5Animation068C0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMp5a5Animation06C14Bank1[8] = {
#include "assets/mp5a5_animation_06C14_bank1.inc"
};

static AnimationPackedRotation _gMp5a5Animation06C14Bank4[68] = {
#include "assets/mp5a5_animation_06C14_bank4.inc"
};

static AnimationRecord _gMp5a5Animation06C14Records[101] = {
#include "assets/mp5a5_animation_06C14_records.inc"
};

static u16 _gMp5a5Animation06C14Indices[20] = {
#include "assets/mp5a5_animation_06C14_indices.inc"
};

static AnimationSet _gMp5a5Animation06C14 = {
    _gMp5a5Animation06C14Records,
    _gMp5a5Animation06C14Indices,
    { NULL, _gMp5a5Animation06C14Bank1, NULL, NULL, _gMp5a5Animation06C14Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMp5a5Animation06EC8Bank1[5] = {
#include "assets/mp5a5_animation_06EC8_bank1.inc"
};

static AnimationPackedRotation _gMp5a5Animation06EC8Bank4[55] = {
#include "assets/mp5a5_animation_06EC8_bank4.inc"
};

static AnimationRecord _gMp5a5Animation06EC8Records[83] = {
#include "assets/mp5a5_animation_06EC8_records.inc"
};

static u16 _gMp5a5Animation06EC8Indices[20] = {
#include "assets/mp5a5_animation_06EC8_indices.inc"
};

static AnimationSet _gMp5a5Animation06EC8 = {
    _gMp5a5Animation06EC8Records,
    _gMp5a5Animation06EC8Indices,
    { NULL, _gMp5a5Animation06EC8Bank1, NULL, NULL, _gMp5a5Animation06EC8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMp5a5Animation071E8Bank1[6] = {
#include "assets/mp5a5_animation_071E8_bank1.inc"
};

static AnimationPackedRotation _gMp5a5Animation071E8Bank4[66] = {
#include "assets/mp5a5_animation_071E8_bank4.inc"
};

static AnimationRecord _gMp5a5Animation071E8Records[96] = {
#include "assets/mp5a5_animation_071E8_records.inc"
};

static u16 _gMp5a5Animation071E8Indices[20] = {
#include "assets/mp5a5_animation_071E8_indices.inc"
};

static AnimationSet _gMp5a5Animation071E8 = {
    _gMp5a5Animation071E8Records,
    _gMp5a5Animation071E8Indices,
    { NULL, _gMp5a5Animation071E8Bank1, NULL, NULL, _gMp5a5Animation071E8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMp5a5Animation079ACBank1[18] = {
#include "assets/mp5a5_animation_079AC_bank1.inc"
};

static AnimationPackedRotation _gMp5a5Animation079ACBank4[184] = {
#include "assets/mp5a5_animation_079AC_bank4.inc"
};

static AnimationRecord _gMp5a5Animation079ACRecords[239] = {
#include "assets/mp5a5_animation_079AC_records.inc"
};

static u16 _gMp5a5Animation079ACIndices[20] = {
#include "assets/mp5a5_animation_079AC_indices.inc"
};

static AnimationSet _gMp5a5Animation079AC = {
    _gMp5a5Animation079ACRecords,
    _gMp5a5Animation079ACIndices,
    { NULL, _gMp5a5Animation079ACBank1, NULL, NULL, _gMp5a5Animation079ACBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMp5a5Animation08C44Bank1[29] = {
#include "assets/mp5a5_animation_08C44_bank1.inc"
};

static AnimationPackedRotation _gMp5a5Animation08C44Bank4[450] = {
#include "assets/mp5a5_animation_08C44_bank4.inc"
};

static AnimationRecord _gMp5a5Animation08C44Records[633] = {
#include "assets/mp5a5_animation_08C44_records.inc"
};

static u16 _gMp5a5Animation08C44Indices[20] = {
#include "assets/mp5a5_animation_08C44_indices.inc"
};

static AnimationSet _gMp5a5Animation08C44 = {
    _gMp5a5Animation08C44Records,
    _gMp5a5Animation08C44Indices,
    { NULL, _gMp5a5Animation08C44Bank1, NULL, NULL, _gMp5a5Animation08C44Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMp5a5Animation097BCBank1[12] = {
#include "assets/mp5a5_animation_097BC_bank1.inc"
};

static AnimationPackedRotation _gMp5a5Animation097BCBank4[266] = {
#include "assets/mp5a5_animation_097BC_bank4.inc"
};

static AnimationRecord _gMp5a5Animation097BCRecords[412] = {
#include "assets/mp5a5_animation_097BC_records.inc"
};

static u16 _gMp5a5Animation097BCIndices[20] = {
#include "assets/mp5a5_animation_097BC_indices.inc"
};

static AnimationSet _gMp5a5Animation097BC = {
    _gMp5a5Animation097BCRecords,
    _gMp5a5Animation097BCIndices,
    { NULL, _gMp5a5Animation097BCBank1, NULL, NULL, _gMp5a5Animation097BCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMp5a5Animation09ED8Bank1[9] = {
#include "assets/mp5a5_animation_09ED8_bank1.inc"
};

static AnimationPackedRotation _gMp5a5Animation09ED8Bank4[144] = {
#include "assets/mp5a5_animation_09ED8_bank4.inc"
};

static AnimationRecord _gMp5a5Animation09ED8Records[264] = {
#include "assets/mp5a5_animation_09ED8_records.inc"
};

static u16 _gMp5a5Animation09ED8Indices[20] = {
#include "assets/mp5a5_animation_09ED8_indices.inc"
};

static AnimationSet _gMp5a5Animation09ED8 = {
    _gMp5a5Animation09ED8Records,
    _gMp5a5Animation09ED8Indices,
    { NULL, _gMp5a5Animation09ED8Bank1, NULL, NULL, _gMp5a5Animation09ED8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMp5a5Animation0A358Bank1[6] = {
#include "assets/mp5a5_animation_0A358_bank1.inc"
};

static AnimationPackedRotation _gMp5a5Animation0A358Bank4[107] = {
#include "assets/mp5a5_animation_0A358_bank4.inc"
};

static AnimationRecord _gMp5a5Animation0A358Records[143] = {
#include "assets/mp5a5_animation_0A358_records.inc"
};

static u16 _gMp5a5Animation0A358Indices[20] = {
#include "assets/mp5a5_animation_0A358_indices.inc"
};

static AnimationSet _gMp5a5Animation0A358 = {
    _gMp5a5Animation0A358Records,
    _gMp5a5Animation0A358Indices,
    { NULL, _gMp5a5Animation0A358Bank1, NULL, NULL, _gMp5a5Animation0A358Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMp5a5Animation0A530Bank1[3] = {
#include "assets/mp5a5_animation_0A530_bank1.inc"
};

static AnimationPackedRotation _gMp5a5Animation0A530Bank4[32] = {
#include "assets/mp5a5_animation_0A530_bank4.inc"
};

static AnimationRecord _gMp5a5Animation0A530Records[57] = {
#include "assets/mp5a5_animation_0A530_records.inc"
};

static u16 _gMp5a5Animation0A530Indices[20] = {
#include "assets/mp5a5_animation_0A530_indices.inc"
};

static AnimationSet _gMp5a5Animation0A530 = {
    _gMp5a5Animation0A530Records,
    _gMp5a5Animation0A530Indices,
    { NULL, _gMp5a5Animation0A530Bank1, NULL, NULL, _gMp5a5Animation0A530Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMp5a5Animation0AA94Bank1[11] = {
#include "assets/mp5a5_animation_0AA94_bank1.inc"
};

static AnimationPackedRotation _gMp5a5Animation0AA94Bank4[125] = {
#include "assets/mp5a5_animation_0AA94_bank4.inc"
};

static AnimationRecord _gMp5a5Animation0AA94Records[167] = {
#include "assets/mp5a5_animation_0AA94_records.inc"
};

static u16 _gMp5a5Animation0AA94Indices[20] = {
#include "assets/mp5a5_animation_0AA94_indices.inc"
};

static AnimationSet _gMp5a5Animation0AA94 = {
    _gMp5a5Animation0AA94Records,
    _gMp5a5Animation0AA94Indices,
    { NULL, _gMp5a5Animation0AA94Bank1, NULL, NULL, _gMp5a5Animation0AA94Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMp5a5Animation0AC88Bank1[3] = {
#include "assets/mp5a5_animation_0AC88_bank1.inc"
};

static AnimationPackedRotation _gMp5a5Animation0AC88Bank4[20] = {
#include "assets/mp5a5_animation_0AC88_bank4.inc"
};

static AnimationRecord _gMp5a5Animation0AC88Records[76] = {
#include "assets/mp5a5_animation_0AC88_records.inc"
};

static u16 _gMp5a5Animation0AC88Indices[20] = {
#include "assets/mp5a5_animation_0AC88_indices.inc"
};

static AnimationSet _gMp5a5Animation0AC88 = {
    _gMp5a5Animation0AC88Records,
    _gMp5a5Animation0AC88Indices,
    { NULL, _gMp5a5Animation0AC88Bank1, NULL, NULL, _gMp5a5Animation0AC88Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMp5a5Animation0B108Bank1[8] = {
#include "assets/mp5a5_animation_0B108_bank1.inc"
};

static AnimationPackedRotation _gMp5a5Animation0B108Bank4[105] = {
#include "assets/mp5a5_animation_0B108_bank4.inc"
};

static AnimationRecord _gMp5a5Animation0B108Records[139] = {
#include "assets/mp5a5_animation_0B108_records.inc"
};

static u16 _gMp5a5Animation0B108Indices[20] = {
#include "assets/mp5a5_animation_0B108_indices.inc"
};

static AnimationSet _gMp5a5Animation0B108 = {
    _gMp5a5Animation0B108Records,
    _gMp5a5Animation0B108Indices,
    { NULL, _gMp5a5Animation0B108Bank1, NULL, NULL, _gMp5a5Animation0B108Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMp5a5Animation0BBB0Bank1[19] = {
#include "assets/mp5a5_animation_0BBB0_bank1.inc"
};

static AnimationPackedRotation _gMp5a5Animation0BBB0Bank4[269] = {
#include "assets/mp5a5_animation_0BBB0_bank4.inc"
};

static AnimationRecord _gMp5a5Animation0BBB0Records[336] = {
#include "assets/mp5a5_animation_0BBB0_records.inc"
};

static u16 _gMp5a5Animation0BBB0Indices[20] = {
#include "assets/mp5a5_animation_0BBB0_indices.inc"
};

static AnimationSet _gMp5a5Animation0BBB0 = {
    _gMp5a5Animation0BBB0Records,
    _gMp5a5Animation0BBB0Indices,
    { NULL, _gMp5a5Animation0BBB0Bank1, NULL, NULL, _gMp5a5Animation0BBB0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMp5a5Animation0BD88Bank1[2] = {
#include "assets/mp5a5_animation_0BD88_bank1.inc"
};

static AnimationPackedRotation _gMp5a5Animation0BD88Bank4[16] = {
#include "assets/mp5a5_animation_0BD88_bank4.inc"
};

static AnimationRecord _gMp5a5Animation0BD88Records[76] = {
#include "assets/mp5a5_animation_0BD88_records.inc"
};

static u16 _gMp5a5Animation0BD88Indices[20] = {
#include "assets/mp5a5_animation_0BD88_indices.inc"
};

static AnimationSet _gMp5a5Animation0BD88 = {
    _gMp5a5Animation0BD88Records,
    _gMp5a5Animation0BD88Indices,
    { NULL, _gMp5a5Animation0BD88Bank1, NULL, NULL, _gMp5a5Animation0BD88Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMp5a5Animation0C4BCBank1[12] = {
#include "assets/mp5a5_animation_0C4BC_bank1.inc"
};

static AnimationPackedRotation _gMp5a5Animation0C4BCBank4[165] = {
#include "assets/mp5a5_animation_0C4BC_bank4.inc"
};

static AnimationRecord _gMp5a5Animation0C4BCRecords[240] = {
#include "assets/mp5a5_animation_0C4BC_records.inc"
};

static u16 _gMp5a5Animation0C4BCIndices[20] = {
#include "assets/mp5a5_animation_0C4BC_indices.inc"
};

static AnimationSet _gMp5a5Animation0C4BC = {
    _gMp5a5Animation0C4BCRecords,
    _gMp5a5Animation0C4BCIndices,
    { NULL, _gMp5a5Animation0C4BCBank1, NULL, NULL, _gMp5a5Animation0C4BCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMp5a5Animation0D268Bank1[23] = {
#include "assets/mp5a5_animation_0D268_bank1.inc"
};

static AnimationPackedRotation _gMp5a5Animation0D268Bank4[369] = {
#include "assets/mp5a5_animation_0D268_bank4.inc"
};

static AnimationRecord _gMp5a5Animation0D268Records[417] = {
#include "assets/mp5a5_animation_0D268_records.inc"
};

static u16 _gMp5a5Animation0D268Indices[20] = {
#include "assets/mp5a5_animation_0D268_indices.inc"
};

static AnimationSet _gMp5a5Animation0D268 = {
    _gMp5a5Animation0D268Records,
    _gMp5a5Animation0D268Indices,
    { NULL, _gMp5a5Animation0D268Bank1, NULL, NULL, _gMp5a5Animation0D268Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMp5a5Animation0D768Bank1[9] = {
#include "assets/mp5a5_animation_0D768_bank1.inc"
};

static AnimationPackedRotation _gMp5a5Animation0D768Bank4[120] = {
#include "assets/mp5a5_animation_0D768_bank4.inc"
};

static AnimationRecord _gMp5a5Animation0D768Records[153] = {
#include "assets/mp5a5_animation_0D768_records.inc"
};

static u16 _gMp5a5Animation0D768Indices[20] = {
#include "assets/mp5a5_animation_0D768_indices.inc"
};

static AnimationSet _gMp5a5Animation0D768 = {
    _gMp5a5Animation0D768Records,
    _gMp5a5Animation0D768Indices,
    { NULL, _gMp5a5Animation0D768Bank1, NULL, NULL, _gMp5a5Animation0D768Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMp5a5Animation0DC58Bank1[9] = {
#include "assets/mp5a5_animation_0DC58_bank1.inc"
};

static AnimationPackedRotation _gMp5a5Animation0DC58Bank4[116] = {
#include "assets/mp5a5_animation_0DC58_bank4.inc"
};

static AnimationRecord _gMp5a5Animation0DC58Records[153] = {
#include "assets/mp5a5_animation_0DC58_records.inc"
};

static u16 _gMp5a5Animation0DC58Indices[20] = {
#include "assets/mp5a5_animation_0DC58_indices.inc"
};

static AnimationSet _gMp5a5Animation0DC58 = {
    _gMp5a5Animation0DC58Records,
    _gMp5a5Animation0DC58Indices,
    { NULL, _gMp5a5Animation0DC58Bank1, NULL, NULL, _gMp5a5Animation0DC58Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMp5a5Animation0E1E4Bank1[10] = {
#include "assets/mp5a5_animation_0E1E4_bank1.inc"
};

static AnimationPackedRotation _gMp5a5Animation0E1E4Bank4[136] = {
#include "assets/mp5a5_animation_0E1E4_bank4.inc"
};

static AnimationRecord _gMp5a5Animation0E1E4Records[169] = {
#include "assets/mp5a5_animation_0E1E4_records.inc"
};

static u16 _gMp5a5Animation0E1E4Indices[20] = {
#include "assets/mp5a5_animation_0E1E4_indices.inc"
};

static AnimationSet _gMp5a5Animation0E1E4 = {
    _gMp5a5Animation0E1E4Records,
    _gMp5a5Animation0E1E4Indices,
    { NULL, _gMp5a5Animation0E1E4Bank1, NULL, NULL, _gMp5a5Animation0E1E4Bank4, NULL, NULL, NULL },
};

AnimationBank D_mp5a5_8012B3CC = { { {
    NULL,
    &_gMp5a5IdleAnimation,
    &_gMp5a5WalkAnimation,
    &_gMp5a5BackwardWalkAnimation,
    &_gMp5a5RunAnimation,
    &_gMp5a5Animation03A00,
    &_gMp5a5Animation04264,
    &_gMp5a5Animation0DC58,
    &_gMp5a5Animation0E1E4,
    &_gMp5a5Animation0BD88,
    &_gMp5a5Animation0D768,
    &_gMp5a5Animation0D768,
    &_gMp5a5Animation0BBB0,
    &_gMp5a5Animation0C4BC,
    &_gMp5a5Animation0D268,
    &_gMp5a5Animation0D268,
    &_gMp5a5Animation06EC8,
    &_gMp5a5Animation071E8,
    &_gMp5a5Animation079AC,
    &_gMp5a5WalkAnimation,
    &_gMp5a5Animation0D268,
    &_gMp5a5IdleAnimation,
    &_gMp5a5IdleAnimation,
    &_gMp5a5Animation08C44,
    &_gMp5a5Animation09ED8,
    &_gMp5a5Animation097BC,
    &_gMp5a5Animation05F48,
    &_gMp5a5Animation06144,
    &_gMp5a5Animation0641C,
    &_gMp5a5Animation066C0,
    &_gMp5a5Animation068C0,
    &_gMp5a5Animation06C14,
    &_gMp5a5Animation0A358,
    &_gMp5a5Animation0A530,
    &_gMp5a5Animation0A358,
    &_gMp5a5Animation0A530,
    &_gMp5a5Animation04CD4,
    &_gMp5a5Animation0545C,
    &_gMp5a5Animation05AC0,
    &_gMp5a5Animation05730,
    &_gMp5a5Animation04578,
    &_gMp5a5IdleAnimation,
    &_gMp5a5Animation0AA94,
    &_gMp5a5Animation0AC88,
    &_gMp5a5Animation0B108,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
} } };

/// Retained streak directions shared by every live flash in this weapon package.
///
/// Angles use 4096 units per turn, one random direction in each quadrant.
/// Each newly initialized flash replaces the directions older flashes read.
static s16 _gMuzzleFlashAngles[MUZZLE_FLASH_STREAK_COUNT] = { 0, 0, 0, 0 };

PACKAGE_ALIASES
