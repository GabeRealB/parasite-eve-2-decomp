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

/// Manifest-selected weapon index, equal to its inventory item ID minus 0x7F.
///
/// Required integer preprocessor expression: 30 MP5A5, 31 MP5A5(+1), 32 MP5A5(+2).
/// Occupies contact-key bits 8..15 and sound-bank bits 16..23, and selects the
/// muzzle/casing profile. `WEAPON_ITEM_ID` converts it to the weapon item whose
/// primary or secondary load is consumed.
#ifndef WEAPON_INDEX
#error "WEAPON_INDEX is a per-package build parameter"
#endif

/// MP5A5 alternate-fire flash origin in integer weapon-root coordinates.
static SVECTOR _gMp5a5MuzzleFlashOffset = { 0, 576, 64, 0 };

static s16 _gMuzzleFlashAngles[MUZZLE_FLASH_STREAK_COUNT];

/// Selects the MP5A5 alternate-fire flash offset in weapon-root coordinates.
///
/// Binds a carrier-owned `SVECTOR`: XYZ are signed integer model units, and
/// `pad` is unused. The task copies each component once during initialization.
/// Keep this binding scoped to the task-fragment inclusion.
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
                    actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].key    = gPlayerStatus.weaponSlotItem | WORLD_COLLISION_CONTACT_ATTACK | (WEAPON_INDEX << 8);
                    weaponShape->end0Radius                               = weaponShape->end1Radius;
                    actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= WORLD_COLLISION_BODY_SINGLE_CONTACT;
                    playerActorSetWeaponAttackFlags(playerTask, 0, 1);
                    worldCoordPlaySound(playerTask->extra.tmd->coords, MP5A5_PRIMARY_SOUND_BASE | (WEAPON_INDEX << 16), 1);
                    effectSpawn(EFFECT_HANDGUN_MUZZLE_FLASH,
                                actor->equipmentTasks[1]->extra.tmd->coords,
                                WEAPON_INDEX, NULL);
                    equipmentConsumeWeaponLoad(WEAPON_ITEM_ID(WEAPON_INDEX), EQUIPMENT_WEAPON_LOAD_CONSUME_PRIMARY);
                    playerActorPlayChildSlotsWithBlend(playerTask, MP5A5_ANIMATION_PRIMARY, 0, 2);
                } else {
                    actor->statePhase                                     = MP5A5_PHASE_SECONDARY_IMPACT;
                    actor->attackControl.cooldownTicks                    = MP5A5_SECONDARY_COOLDOWN_FRAMES;
                    actor->stateTimer                                     = MP5A5_SECONDARY_ACTIVE_FRAMES;
                    actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].key    = MP5A5_SECONDARY_KEY_BASE | (WEAPON_INDEX << 8);
                    weaponShape->end0Radius                               = MP5A5_SECONDARY_SPREAD_RADIUS;
                    actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_SINGLE_CONTACT);
                    playerActorSetWeaponAttackFlags(playerTask, 0, 0);
                    worldCoordPlaySound(playerTask->extra.tmd->coords, MP5A5_SECONDARY_SOUND_BASE | (WEAPON_INDEX << 16), 0);
                    equipmentConsumeWeaponLoad(WEAPON_ITEM_ID(WEAPON_INDEX), EQUIPMENT_WEAPON_LOAD_CONSUME_SECONDARY);
                    flashWork = effectSpawn(EFFECT_MP5A5_ALT_FIRE_MUZZLE_FLASH,
                                            actor->equipmentTasks[1]->extra.tmd->coords,
                                            WEAPON_INDEX, NULL);
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
            if (playerActorIsSlotAdvancingLinearly(playerTask, gPlayerActorAttachmentParentParts[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId][1], 0, 0) == 0) {
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

/// Encoding-1 local translations and rotations for MP5A5 left turning.
static AnimationPackedPose _gMp5a5TurnLeftAnimationBank1[] = {
#include "assets/mp5a5_animation_03A00_bank1.inc"
};

/// Encoding-4 rotations for MP5A5 left turning, in eight-unit angle steps.
static AnimationPackedRotation _gMp5a5TurnLeftAnimationBank4[] = {
#include "assets/mp5a5_animation_03A00_bank4.inc"
};

/// Left-turn keyframes and loop controls; segment durations count normal-rate frames.
static AnimationRecord _gMp5a5TurnLeftAnimationRecords[] = {
#include "assets/mp5a5_animation_03A00_records.inc"
};

/// Absolute record starts for the left-turn clip's 19 player-rig tracks.
///
/// The final serialized halfword is retained but is not another track start.
static u16 _gMp5a5TurnLeftAnimationIndices[] = {
#include "assets/mp5a5_animation_03A00_indices.inc"
};

/// MP5A5 player's stationary left-turn clip in native bank slot 5.
///
/// Provides player-rig tracks 0..18; child playback uses tracks 1..18.
/// Actor control supplies the root yaw separately from these local poses.
/// Playback borrows the data for the lifetime of the loaded weapon package.
static AnimationSet _gMp5a5TurnLeftAnimation = {
    .records           = _gMp5a5TurnLeftAnimationRecords,
    .trackStartIndices = _gMp5a5TurnLeftAnimationIndices,
    .poseBanks         = { NULL, _gMp5a5TurnLeftAnimationBank1, NULL, NULL, _gMp5a5TurnLeftAnimationBank4, NULL, NULL, NULL },
};

/// Encoding-1 local translations and rotations for MP5A5 right turning.
static AnimationPackedPose _gMp5a5TurnRightAnimationBank1[] = {
#include "assets/mp5a5_animation_04264_bank1.inc"
};

/// Encoding-4 rotations for MP5A5 right turning, in eight-unit angle steps.
static AnimationPackedRotation _gMp5a5TurnRightAnimationBank4[] = {
#include "assets/mp5a5_animation_04264_bank4.inc"
};

/// Right-turn keyframes and loop controls; segment durations count normal-rate frames.
static AnimationRecord _gMp5a5TurnRightAnimationRecords[] = {
#include "assets/mp5a5_animation_04264_records.inc"
};

/// Absolute record starts for the right-turn clip's 19 player-rig tracks.
///
/// The final serialized halfword is retained but is not another track start.
static u16 _gMp5a5TurnRightAnimationIndices[] = {
#include "assets/mp5a5_animation_04264_indices.inc"
};

/// MP5A5 player's stationary right-turn clip in native bank slot 6.
///
/// Provides player-rig tracks 0..18; child playback uses tracks 1..18.
/// Actor control supplies the root yaw separately from these local poses.
/// Playback borrows the data for the lifetime of the loaded weapon package.
static AnimationSet _gMp5a5TurnRightAnimation = {
    .records           = _gMp5a5TurnRightAnimationRecords,
    .trackStartIndices = _gMp5a5TurnRightAnimationIndices,
    .poseBanks         = { NULL, _gMp5a5TurnRightAnimationBank1, NULL, NULL, _gMp5a5TurnRightAnimationBank4, NULL, NULL, NULL },
};

/// Encoding-1 local translations and rotations for MP5A5 item use.
static AnimationPackedPose _gMp5a5ItemUseAnimationBank1[] = {
#include "assets/mp5a5_animation_04578_bank1.inc"
};

/// Encoding-4 rotations for MP5A5 item use, in eight-unit angle steps.
static AnimationPackedRotation _gMp5a5ItemUseAnimationBank4[] = {
#include "assets/mp5a5_animation_04578_bank4.inc"
};

/// Item-use keyframes and stop controls; segment durations count normal-rate frames.
static AnimationRecord _gMp5a5ItemUseAnimationRecords[] = {
#include "assets/mp5a5_animation_04578_records.inc"
};

/// Absolute record starts for the item-use clip's 19 player-rig tracks.
///
/// The final serialized halfword is retained but is not another track start.
static u16 _gMp5a5ItemUseAnimationIndices[] = {
#include "assets/mp5a5_animation_04578_indices.inc"
};

/// MP5A5 player's item-use presentation clip in native bank slot 40.
///
/// Provides player-rig tracks 0..18; child playback uses tracks 1..18.
/// The item effect is applied separately from this presentation.
/// Playback borrows the data for the lifetime of the loaded weapon package.
static AnimationSet _gMp5a5ItemUseAnimation = {
    .records           = _gMp5a5ItemUseAnimationRecords,
    .trackStartIndices = _gMp5a5ItemUseAnimationIndices,
    .poseBanks         = { NULL, _gMp5a5ItemUseAnimationBank1, NULL, NULL, _gMp5a5ItemUseAnimationBank4, NULL, NULL, NULL },
};

/// Encoding-1 local translations and rotations for MP5A5 stair ascent.
static AnimationPackedPose _gMp5a5StairAscentAnimationBank1[] = {
#include "assets/mp5a5_animation_04CD4_bank1.inc"
};

/// Encoding-4 rotations for MP5A5 stair ascent, in eight-unit angle steps.
static AnimationPackedRotation _gMp5a5StairAscentAnimationBank4[] = {
#include "assets/mp5a5_animation_04CD4_bank4.inc"
};

/// Stair-ascent keyframes and loop controls, with stride cues on player-rig track 1.
///
/// Segment durations count normal-rate frames; cue bits gate stair displacement.
static AnimationRecord _gMp5a5StairAscentAnimationRecords[] = {
#include "assets/mp5a5_animation_04CD4_records.inc"
};

/// Absolute record starts for the stair-ascent clip's 19 player-rig tracks.
///
/// The final serialized halfword is retained but is not another track start.
static u16 _gMp5a5StairAscentAnimationIndices[] = {
#include "assets/mp5a5_animation_04CD4_indices.inc"
};

/// MP5A5 player's repeating stair-ascent clip in native bank slot 36.
///
/// Provides player-rig tracks 0..18; child playback uses tracks 1..18.
/// Actor control supplies root displacement, timed by the clip's stride cues.
/// Playback borrows the data for the lifetime of the loaded weapon package.
static AnimationSet _gMp5a5StairAscentAnimation = {
    .records           = _gMp5a5StairAscentAnimationRecords,
    .trackStartIndices = _gMp5a5StairAscentAnimationIndices,
    .poseBanks         = { NULL, _gMp5a5StairAscentAnimationBank1, NULL, NULL, _gMp5a5StairAscentAnimationBank4, NULL, NULL, NULL },
};

/// Encoding-1 local translations and rotations for MP5A5 stair descent.
static AnimationPackedPose _gMp5a5StairDescentAnimationBank1[] = {
#include "assets/mp5a5_animation_0545C_bank1.inc"
};

/// Encoding-4 rotations for MP5A5 stair descent, in eight-unit angle steps.
static AnimationPackedRotation _gMp5a5StairDescentAnimationBank4[] = {
#include "assets/mp5a5_animation_0545C_bank4.inc"
};

/// Stair-descent keyframes and loop controls, with stride cues on player-rig track 1.
///
/// Segment durations count normal-rate frames; cue bits gate stair displacement.
static AnimationRecord _gMp5a5StairDescentAnimationRecords[] = {
#include "assets/mp5a5_animation_0545C_records.inc"
};

/// Absolute record starts for the stair-descent clip's 19 player-rig tracks.
///
/// The final serialized halfword is retained but is not another track start.
static u16 _gMp5a5StairDescentAnimationIndices[] = {
#include "assets/mp5a5_animation_0545C_indices.inc"
};

/// MP5A5 player's repeating stair-descent clip in native bank slot 37.
///
/// Provides player-rig tracks 0..18; child playback uses tracks 1..18.
/// Actor control supplies root displacement, timed by the clip's stride cues.
/// Playback borrows the data for the lifetime of the loaded weapon package.
static AnimationSet _gMp5a5StairDescentAnimation = {
    .records           = _gMp5a5StairDescentAnimationRecords,
    .trackStartIndices = _gMp5a5StairDescentAnimationIndices,
    .poseBanks         = { NULL, _gMp5a5StairDescentAnimationBank1, NULL, NULL, _gMp5a5StairDescentAnimationBank4, NULL, NULL, NULL },
};

/// Encoding-1 local translations and rotations for MP5A5's odd-count ascent finish.
static AnimationPackedPose _gMp5a5StairAscentFinishOddAnimationBank1[] = {
#include "assets/mp5a5_animation_05730_bank1.inc"
};

/// Encoding-4 rotations for MP5A5's odd-count ascent finish, in eight-unit angle steps.
static AnimationPackedRotation _gMp5a5StairAscentFinishOddAnimationBank4[] = {
#include "assets/mp5a5_animation_05730_bank4.inc"
};

/// Odd-count ascent-finish keyframes and stop controls, with a track-1 footstep cue.
///
/// Segment durations count normal-rate frames.
static AnimationRecord _gMp5a5StairAscentFinishOddAnimationRecords[] = {
#include "assets/mp5a5_animation_05730_records.inc"
};

/// Absolute record starts for the odd-count ascent finish's 19 player-rig tracks.
///
/// The final serialized halfword is retained but is not another track start.
static u16 _gMp5a5StairAscentFinishOddAnimationIndices[] = {
#include "assets/mp5a5_animation_05730_indices.inc"
};

/// MP5A5 player's stair-ascent finishing stride for an odd initial step count.
///
/// Native bank slot 39 provides player-rig tracks 0..18; child playback uses 1..18.
/// Actor control supplies the final level root displacement separately.
/// Playback borrows the data for the lifetime of the loaded weapon package.
static AnimationSet _gMp5a5StairAscentFinishOddAnimation = {
    .records           = _gMp5a5StairAscentFinishOddAnimationRecords,
    .trackStartIndices = _gMp5a5StairAscentFinishOddAnimationIndices,
    .poseBanks         = { NULL, _gMp5a5StairAscentFinishOddAnimationBank1, NULL, NULL, _gMp5a5StairAscentFinishOddAnimationBank4, NULL, NULL, NULL },
};

/// Encoding selectors used by these MP5A5 player-rig clips.
enum {
    MP5A5_ANIMATION_TRANSLATION_ROTATION_BANK = 1,
    MP5A5_ANIMATION_ROTATION_BANK             = 4,
};

/// Encoding-1 local translations and rotations for even-count stair-ascent finish.
///
/// Tracks 0 and 1 use integer model coordinates and 4096 angle units per turn.
/// Read-only payload retains its serialized position in the data section.
static const AnimationPackedPose _gMp5a5StairAscentFinishEvenAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/mp5a5_animation_05AC0_bank1.inc"
};

/// Encoding-4 rotations for even-count stair-ascent finish, in eight-unit angle steps.
///
/// Tracks 2..18 preserve their model-part translations.
static const AnimationPackedRotation _gMp5a5StairAscentFinishEvenAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/mp5a5_animation_05AC0_bank4.inc"
};

/// Keyframes and stop controls for even-count stair-ascent finish.
///
/// Track 1 carries `ANIMATION_RECORD_CUE_2`; segment durations count normal-rate frames.
/// Pose offsets count four-byte words.
static const AnimationRecord _gMp5a5StairAscentFinishEvenAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/mp5a5_animation_05AC0_records.inc"
};

/// Absolute record starts for the 19 tracks of even-count stair-ascent finish.
///
/// The final serialized halfword is retained but is not another track start.
static const u16 _gMp5a5StairAscentFinishEvenAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/mp5a5_animation_05AC0_indices.inc"
};

/// MP5A5 player's stair-ascent finishing stride for an even initial step count.
///
/// Native bank slot 38 supplies player-rig tracks 0..18; child playback uses 1..18.
/// Actor control supplies the final level root displacement separately.
/// Playback borrows this clip while the MP5A5 or either upgrade package is loaded.
static AnimationSet _gMp5a5StairAscentFinishEvenAnimation = {
    .records           = _gMp5a5StairAscentFinishEvenAnimationRecords,
    .trackStartIndices = _gMp5a5StairAscentFinishEvenAnimationIndices,
    .poseBanks         = {
        [MP5A5_ANIMATION_TRANSLATION_ROTATION_BANK] = _gMp5a5StairAscentFinishEvenAnimationBank1,
        [MP5A5_ANIMATION_ROTATION_BANK]             = _gMp5a5StairAscentFinishEvenAnimationBank4,
    },
};

/// Encoding-1 local translations and rotations for Parasite Energy family-0 release.
///
/// Tracks 0 and 1 use integer model coordinates and 4096 angle units per turn.
/// Read-only payload retains its serialized position in the data section.
static const AnimationPackedPose _gMp5a5ParasiteEnergyReleaseFamily0AnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/mp5a5_animation_05F48_bank1.inc"
};

/// Encoding-4 rotations for Parasite Energy family-0 release, in eight-unit angle steps.
///
/// Tracks 2..18 preserve their model-part translations.
static const AnimationPackedRotation _gMp5a5ParasiteEnergyReleaseFamily0AnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/mp5a5_animation_05F48_bank4.inc"
};

/// Keyframes and stop controls for Parasite Energy family-0 release.
///
/// Segment durations count normal-rate frames; pose offsets count four-byte words.
static const AnimationRecord _gMp5a5ParasiteEnergyReleaseFamily0AnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/mp5a5_animation_05F48_records.inc"
};

/// Absolute record starts for the 19 tracks of Parasite Energy family-0 release.
///
/// The final serialized halfword is retained but is not another track start.
static const u16 _gMp5a5ParasiteEnergyReleaseFamily0AnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/mp5a5_animation_05F48_indices.inc"
};

/// MP5A5 player's Parasite Energy family-0 release clip in native bank slot 26.
///
/// Starts the release/charge/cast sequence while the attachment countdown begins.
/// Family 0 selects spell IDs 300..600 with tens digit other than 3, or item IDs
/// from 601 with tens digit 1. Supplies tracks 0..18; child playback uses 1..18.
/// Playback borrows this clip while the MP5A5 or either upgrade package is loaded.
static AnimationSet _gMp5a5ParasiteEnergyReleaseFamily0Animation = {
    .records           = _gMp5a5ParasiteEnergyReleaseFamily0AnimationRecords,
    .trackStartIndices = _gMp5a5ParasiteEnergyReleaseFamily0AnimationIndices,
    .poseBanks         = {
        [MP5A5_ANIMATION_TRANSLATION_ROTATION_BANK] = _gMp5a5ParasiteEnergyReleaseFamily0AnimationBank1,
        [MP5A5_ANIMATION_ROTATION_BANK]             = _gMp5a5ParasiteEnergyReleaseFamily0AnimationBank4,
    },
};

/// Encoding-1 local translations and rotations for Parasite Energy family-0 charge.
///
/// Tracks 0 and 1 use integer model coordinates and 4096 angle units per turn.
/// Read-only payload retains its serialized position in the data section.
static const AnimationPackedPose _gMp5a5ParasiteEnergyChargeFamily0AnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/mp5a5_animation_06144_bank1.inc"
};

/// Encoding-4 rotations for Parasite Energy family-0 charge, in eight-unit angle steps.
///
/// Tracks 2..18 preserve their model-part translations.
static const AnimationPackedRotation _gMp5a5ParasiteEnergyChargeFamily0AnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/mp5a5_animation_06144_bank4.inc"
};

/// Keyframes and loop/stop controls for Parasite Energy family-0 charge.
///
/// Track-local jumps repeat the charge; forward playback bypasses the final stop.
/// Segment durations count normal-rate frames; pose offsets count four-byte words.
static const AnimationRecord _gMp5a5ParasiteEnergyChargeFamily0AnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/mp5a5_animation_06144_records.inc"
};

/// Absolute record starts for the 19 tracks of Parasite Energy family-0 charge.
///
/// The final serialized halfword is retained but is not another track start.
static const u16 _gMp5a5ParasiteEnergyChargeFamily0AnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/mp5a5_animation_06144_indices.inc"
};

/// MP5A5 player's Parasite Energy family-0 charge loop in native bank slot 27.
///
/// Repeats after the family-0 release until the attachment countdown reaches zero
/// or the action is cancelled. Supplies tracks 0..18; child playback uses 1..18.
/// Playback borrows this clip while the MP5A5 or either upgrade package is loaded.
static AnimationSet _gMp5a5ParasiteEnergyChargeFamily0Animation = {
    .records           = _gMp5a5ParasiteEnergyChargeFamily0AnimationRecords,
    .trackStartIndices = _gMp5a5ParasiteEnergyChargeFamily0AnimationIndices,
    .poseBanks         = {
        [MP5A5_ANIMATION_TRANSLATION_ROTATION_BANK] = _gMp5a5ParasiteEnergyChargeFamily0AnimationBank1,
        [MP5A5_ANIMATION_ROTATION_BANK]             = _gMp5a5ParasiteEnergyChargeFamily0AnimationBank4,
    },
};

/// Encoding-1 local translations and rotations for Parasite Energy family-0 cast.
///
/// Tracks 0 and 1 use integer model coordinates and 4096 angle units per turn.
/// Read-only payload retains its serialized position in the data section.
static const AnimationPackedPose _gMp5a5ParasiteEnergyCastFamily0AnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/mp5a5_animation_0641C_bank1.inc"
};

/// Encoding-4 rotations for Parasite Energy family-0 cast, in eight-unit angle steps.
///
/// Tracks 2..18 preserve their model-part translations.
static const AnimationPackedRotation _gMp5a5ParasiteEnergyCastFamily0AnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/mp5a5_animation_0641C_bank4.inc"
};

/// Keyframes and stop controls for Parasite Energy family-0 cast.
///
/// Segment durations count normal-rate frames; pose offsets count four-byte words.
static const AnimationRecord _gMp5a5ParasiteEnergyCastFamily0AnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/mp5a5_animation_0641C_records.inc"
};

/// Absolute record starts for the 19 tracks of Parasite Energy family-0 cast.
///
/// The final serialized halfword is retained but is not another track start.
static const u16 _gMp5a5ParasiteEnergyCastFamily0AnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/mp5a5_animation_0641C_indices.inc"
};

/// MP5A5 player's Parasite Energy family-0 cast clip in native bank slot 28.
///
/// Begins after the family-0 charge when the attachment countdown reaches zero.
/// Supplies player-rig tracks 0..18; child playback uses 1..18.
/// Playback borrows this clip while the MP5A5 or either upgrade package is loaded.
static AnimationSet _gMp5a5ParasiteEnergyCastFamily0Animation = {
    .records           = _gMp5a5ParasiteEnergyCastFamily0AnimationRecords,
    .trackStartIndices = _gMp5a5ParasiteEnergyCastFamily0AnimationIndices,
    .poseBanks         = {
        [MP5A5_ANIMATION_TRANSLATION_ROTATION_BANK] = _gMp5a5ParasiteEnergyCastFamily0AnimationBank1,
        [MP5A5_ANIMATION_ROTATION_BANK]             = _gMp5a5ParasiteEnergyCastFamily0AnimationBank4,
    },
};

/// Encoding-1 local translations and rotations for Parasite Energy family-1 release.
///
/// Tracks 0 and 1 use integer model coordinates and 4096 angle units per turn.
/// Read-only payload retains its serialized position in the data section.
static const AnimationPackedPose _gMp5a5ParasiteEnergyReleaseFamily1AnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/mp5a5_animation_066C0_bank1.inc"
};

/// Encoding-4 rotations for Parasite Energy family-1 release, in eight-unit angle steps.
///
/// Tracks 2..18 preserve their model-part translations.
static const AnimationPackedRotation _gMp5a5ParasiteEnergyReleaseFamily1AnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/mp5a5_animation_066C0_bank4.inc"
};

/// Keyframes and stop controls for Parasite Energy family-1 release.
///
/// Segment durations count normal-rate frames; pose offsets count four-byte words.
static const AnimationRecord _gMp5a5ParasiteEnergyReleaseFamily1AnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/mp5a5_animation_066C0_records.inc"
};

/// Absolute record starts for the 19 tracks of Parasite Energy family-1 release.
///
/// The final serialized halfword is retained but is not another track start.
static const u16 _gMp5a5ParasiteEnergyReleaseFamily1AnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/mp5a5_animation_066C0_indices.inc"
};

/// MP5A5 player's Parasite Energy family-1 release clip in native bank slot 29.
///
/// Starts the release/charge/cast sequence while the attachment countdown begins.
/// Family 1 selects spell IDs below 300 with tens digit other than 3, or item IDs
/// from 601 with tens digit other than 1. Tracks 0..18; child playback uses 1..18.
/// Playback borrows this clip while the MP5A5 or either upgrade package is loaded.
static AnimationSet _gMp5a5ParasiteEnergyReleaseFamily1Animation = {
    .records           = _gMp5a5ParasiteEnergyReleaseFamily1AnimationRecords,
    .trackStartIndices = _gMp5a5ParasiteEnergyReleaseFamily1AnimationIndices,
    .poseBanks         = {
        [MP5A5_ANIMATION_TRANSLATION_ROTATION_BANK] = _gMp5a5ParasiteEnergyReleaseFamily1AnimationBank1,
        [MP5A5_ANIMATION_ROTATION_BANK]             = _gMp5a5ParasiteEnergyReleaseFamily1AnimationBank4,
    },
};

/// Encoding-1 local translations and rotations for Parasite Energy family-1 charge.
///
/// Tracks 0 and 1 use integer model coordinates and 4096 angle units per turn.
/// Read-only payload retains its serialized position in the data section.
static const AnimationPackedPose _gMp5a5ParasiteEnergyChargeFamily1AnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/mp5a5_animation_068C0_bank1.inc"
};

/// Encoding-4 rotations for Parasite Energy family-1 charge, in eight-unit angle steps.
///
/// Tracks 2..18 preserve their model-part translations.
static const AnimationPackedRotation _gMp5a5ParasiteEnergyChargeFamily1AnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/mp5a5_animation_068C0_bank4.inc"
};

/// Keyframes and loop/stop controls for Parasite Energy family-1 charge.
///
/// Track-local jumps repeat the charge; forward playback bypasses the final stop.
/// Segment durations count normal-rate frames; pose offsets count four-byte words.
static const AnimationRecord _gMp5a5ParasiteEnergyChargeFamily1AnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/mp5a5_animation_068C0_records.inc"
};

/// Absolute record starts for the 19 tracks of Parasite Energy family-1 charge.
///
/// The final serialized halfword is retained but is not another track start.
static const u16 _gMp5a5ParasiteEnergyChargeFamily1AnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/mp5a5_animation_068C0_indices.inc"
};

/// MP5A5 player's Parasite Energy family-1 charge loop in native bank slot 30.
///
/// Repeats after the family-1 release until the attachment countdown reaches zero
/// or the action is cancelled. Supplies tracks 0..18; child playback uses 1..18.
/// Playback borrows this clip while the MP5A5 or either upgrade package is loaded.
static AnimationSet _gMp5a5ParasiteEnergyChargeFamily1Animation = {
    .records           = _gMp5a5ParasiteEnergyChargeFamily1AnimationRecords,
    .trackStartIndices = _gMp5a5ParasiteEnergyChargeFamily1AnimationIndices,
    .poseBanks         = {
        [MP5A5_ANIMATION_TRANSLATION_ROTATION_BANK] = _gMp5a5ParasiteEnergyChargeFamily1AnimationBank1,
        [MP5A5_ANIMATION_ROTATION_BANK]             = _gMp5a5ParasiteEnergyChargeFamily1AnimationBank4,
    },
};

/// Encoding-1 local translations and rotations for Parasite Energy family-1 cast.
///
/// Tracks 0 and 1 use integer model coordinates and 4096 angle units per turn.
/// Read-only payload retains its serialized position in the data section.
static const AnimationPackedPose _gMp5a5ParasiteEnergyCastFamily1AnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/mp5a5_animation_06C14_bank1.inc"
};

/// Encoding-4 rotations for Parasite Energy family-1 cast, in eight-unit angle steps.
///
/// Tracks 2..18 preserve their model-part translations.
static const AnimationPackedRotation _gMp5a5ParasiteEnergyCastFamily1AnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/mp5a5_animation_06C14_bank4.inc"
};

/// Keyframes and stop controls for Parasite Energy family-1 cast.
///
/// Segment durations count normal-rate frames; pose offsets count four-byte words.
static const AnimationRecord _gMp5a5ParasiteEnergyCastFamily1AnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/mp5a5_animation_06C14_records.inc"
};

/// Absolute record starts for the 19 tracks of Parasite Energy family-1 cast.
///
/// The final serialized halfword is retained but is not another track start.
static const u16 _gMp5a5ParasiteEnergyCastFamily1AnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/mp5a5_animation_06C14_indices.inc"
};

/// MP5A5 player's Parasite Energy family-1 cast clip in native bank slot 31.
///
/// Begins after the family-1 charge when the attachment countdown reaches zero.
/// Supplies player-rig tracks 0..18; child playback uses 1..18.
/// Playback borrows this clip while the MP5A5 or either upgrade package is loaded.
static AnimationSet _gMp5a5ParasiteEnergyCastFamily1Animation = {
    .records           = _gMp5a5ParasiteEnergyCastFamily1AnimationRecords,
    .trackStartIndices = _gMp5a5ParasiteEnergyCastFamily1AnimationIndices,
    .poseBanks         = {
        [MP5A5_ANIMATION_TRANSLATION_ROTATION_BANK] = _gMp5a5ParasiteEnergyCastFamily1AnimationBank1,
        [MP5A5_ANIMATION_ROTATION_BANK]             = _gMp5a5ParasiteEnergyCastFamily1AnimationBank4,
    },
};

/// Encoding-1 local translations and rotations for the MP5A5 part-4 hit reaction.
///
/// Tracks 0 and 1 use integer model coordinates and 4096 angle units per turn.
/// Read-only payload retains its serialized position in the data section.
static const AnimationPackedPose _gMp5a5Part4HitAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/mp5a5_animation_06EC8_bank1.inc"
};

/// Encoding-4 rotations for the part-4 hit reaction, in eight-unit angle steps.
///
/// Tracks 2..18 preserve their model-part translations.
static const AnimationPackedRotation _gMp5a5Part4HitAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/mp5a5_animation_06EC8_bank4.inc"
};

/// Keyframes and stop controls for the MP5A5 part-4 hit reaction.
///
/// Segment durations count normal-rate frames; pose offsets count four-byte words.
static const AnimationRecord _gMp5a5Part4HitAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/mp5a5_animation_06EC8_records.inc"
};

/// Absolute record starts for the 19 tracks of the part-4 hit reaction.
///
/// The final serialized halfword is retained but is not another track start.
static const u16 _gMp5a5Part4HitAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/mp5a5_animation_06EC8_indices.inc"
};

/// MP5A5 player's hit reaction for the model-part-4 collision body, in bank slot 16.
///
/// Hit region 1 selects this clip for pending and newly resolved contact damage.
/// Supplies player-rig tracks 0..18; child playback uses 1..18.
/// Playback borrows this clip while the MP5A5 or either upgrade package is loaded.
static AnimationSet _gMp5a5Part4HitAnimation = {
    .records           = _gMp5a5Part4HitAnimationRecords,
    .trackStartIndices = _gMp5a5Part4HitAnimationIndices,
    .poseBanks         = {
        [MP5A5_ANIMATION_TRANSLATION_ROTATION_BANK] = _gMp5a5Part4HitAnimationBank1,
        [MP5A5_ANIMATION_ROTATION_BANK]             = _gMp5a5Part4HitAnimationBank4,
    },
};

/// Encoding-1 local translations and rotations for the MP5A5 other-body hit reaction.
///
/// Tracks 0 and 1 use integer model coordinates and 4096 angle units per turn.
/// Read-only payload retains its serialized position in the data section.
static const AnimationPackedPose _gMp5a5OtherBodyHitAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/mp5a5_animation_071E8_bank1.inc"
};

/// Encoding-4 rotations for the other-body hit reaction, in eight-unit angle steps.
///
/// Tracks 2..18 preserve their model-part translations.
static const AnimationPackedRotation _gMp5a5OtherBodyHitAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/mp5a5_animation_071E8_bank4.inc"
};

/// Keyframes and stop controls for the MP5A5 other-body hit reaction.
///
/// Segment durations count normal-rate frames; pose offsets count four-byte words.
static const AnimationRecord _gMp5a5OtherBodyHitAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/mp5a5_animation_071E8_records.inc"
};

/// Absolute record starts for the 19 tracks of the other-body hit reaction.
///
/// The final serialized halfword is retained but is not another track start.
static const u16 _gMp5a5OtherBodyHitAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/mp5a5_animation_071E8_indices.inc"
};

/// MP5A5 player's hit reaction for the root or model-part-1 body, in bank slot 17.
///
/// Pending/contact hit playback selects this clip when the hit region is not 1.
/// Supplies player-rig tracks 0..18; child playback uses 1..18.
/// Playback borrows this clip while the MP5A5 or either upgrade package is loaded.
static AnimationSet _gMp5a5OtherBodyHitAnimation = {
    .records           = _gMp5a5OtherBodyHitAnimationRecords,
    .trackStartIndices = _gMp5a5OtherBodyHitAnimationIndices,
    .poseBanks         = {
        [MP5A5_ANIMATION_TRANSLATION_ROTATION_BANK] = _gMp5a5OtherBodyHitAnimationBank1,
        [MP5A5_ANIMATION_ROTATION_BANK]             = _gMp5a5OtherBodyHitAnimationBank4,
    },
};

/// Encoding-1 local translations and rotations for the MP5A5 player's death clip.
///
/// Tracks 0 and 1 use integer model coordinates and 4096 angle units per turn.
/// Read-only payload retains its serialized position in the data section.
static const AnimationPackedPose _gMp5a5DeathAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/mp5a5_animation_079AC_bank1.inc"
};

/// Encoding-4 rotations for the death clip, in eight-unit angle steps.
///
/// Tracks 2..18 preserve their model-part translations.
static const AnimationPackedRotation _gMp5a5DeathAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/mp5a5_animation_079AC_bank4.inc"
};

/// Keyframes and stop controls holding the MP5A5 player's final death pose.
///
/// Segment durations count normal-rate frames; pose offsets count four-byte words.
static const AnimationRecord _gMp5a5DeathAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/mp5a5_animation_079AC_records.inc"
};

/// Absolute record starts for the 19 tracks of the death clip.
///
/// The final serialized halfword is retained but is not another track start.
static const u16 _gMp5a5DeathAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/mp5a5_animation_079AC_indices.inc"
};

/// MP5A5 player's death clip in native bank slot 18.
///
/// Selected at zero HP by normal control and when scripted control yields to death.
/// Supplies player-rig tracks 0..18; child playback uses 1..18.
/// Playback borrows this clip while the MP5A5 or either upgrade package is loaded.
static AnimationSet _gMp5a5DeathAnimation = {
    .records           = _gMp5a5DeathAnimationRecords,
    .trackStartIndices = _gMp5a5DeathAnimationIndices,
    .poseBanks         = {
        [MP5A5_ANIMATION_TRANSLATION_ROTATION_BANK] = _gMp5a5DeathAnimationBank1,
        [MP5A5_ANIMATION_ROTATION_BANK]             = _gMp5a5DeathAnimationBank4,
    },
};

/// Encoding-1 local translations and rotations for the MP5A5 player's healthy idle.
///
/// Tracks 0 and 1 use integer model coordinates and 4096 angle units per turn.
/// Read-only payload retains its serialized position in the data section.
static const AnimationPackedPose _gMp5a5HealthyIdleAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/mp5a5_animation_08C44_bank1.inc"
};

/// Encoding-4 rotations for healthy idle, in eight-unit angle steps.
///
/// Tracks 2..18 preserve their model-part translations.
static const AnimationPackedRotation _gMp5a5HealthyIdleAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/mp5a5_animation_08C44_bank4.inc"
};

/// Keyframes and loop/stop controls for the MP5A5 player's healthy idle.
///
/// Track-local jumps repeat the tail; forward playback bypasses the final stop.
/// Segment durations count normal-rate frames; pose offsets count four-byte words.
static const AnimationRecord _gMp5a5HealthyIdleAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/mp5a5_animation_08C44_records.inc"
};

/// Absolute record starts for the 19 tracks of healthy idle.
///
/// The final serialized halfword is retained but is not another track start.
static const u16 _gMp5a5HealthyIdleAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/mp5a5_animation_08C44_indices.inc"
};

/// MP5A5 player's healthy idle clip in native bank slot 23.
///
/// The idle timeout selects this clip while HP exceeds half the maximum.
/// Supplies player-rig tracks 0..18; child playback uses 1..18.
/// Playback borrows this clip while the MP5A5 or either upgrade package is loaded.
static AnimationSet _gMp5a5HealthyIdleAnimation = {
    .records           = _gMp5a5HealthyIdleAnimationRecords,
    .trackStartIndices = _gMp5a5HealthyIdleAnimationIndices,
    .poseBanks         = {
        [MP5A5_ANIMATION_TRANSLATION_ROTATION_BANK] = _gMp5a5HealthyIdleAnimationBank1,
        [MP5A5_ANIMATION_ROTATION_BANK]             = _gMp5a5HealthyIdleAnimationBank4,
    },
};

/// Encoding-1 local translations and rotations for MP5A5 low-health idle/paralysis.
///
/// Tracks 0 and 1 use integer model coordinates and 4096 angle units per turn.
/// Read-only payload retains its serialized position in the data section.
static const AnimationPackedPose _gMp5a5LowHealthIdleAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/mp5a5_animation_097BC_bank1.inc"
};

/// Encoding-4 rotations for low-health idle/paralysis, in eight-unit angle steps.
///
/// Tracks 2..18 preserve their model-part translations.
static const AnimationPackedRotation _gMp5a5LowHealthIdleAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/mp5a5_animation_097BC_bank4.inc"
};

/// Keyframes and loop/stop controls for MP5A5 low-health idle/paralysis.
///
/// Track-local jumps repeat the tail; forward playback bypasses the final stop.
/// Segment durations count normal-rate frames; pose offsets count four-byte words.
static const AnimationRecord _gMp5a5LowHealthIdleAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/mp5a5_animation_097BC_records.inc"
};

/// Absolute record starts for the 19 tracks of low-health idle/paralysis.
///
/// The final serialized halfword is retained but is not another track start.
static const u16 _gMp5a5LowHealthIdleAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/mp5a5_animation_097BC_indices.inc"
};

/// MP5A5 player's low-health idle clip, also used for paralysis, in bank slot 25.
///
/// Selected by the idle timeout while HP is at most a quarter of the maximum.
/// Paralysis blends the same clip across child tracks 1..18 regardless of HP.
/// Supplies player-rig tracks 0..18.
/// Playback borrows this clip while the MP5A5 or either upgrade package is loaded.
static AnimationSet _gMp5a5LowHealthIdleAnimation = {
    .records           = _gMp5a5LowHealthIdleAnimationRecords,
    .trackStartIndices = _gMp5a5LowHealthIdleAnimationIndices,
    .poseBanks         = {
        [MP5A5_ANIMATION_TRANSLATION_ROTATION_BANK] = _gMp5a5LowHealthIdleAnimationBank1,
        [MP5A5_ANIMATION_ROTATION_BANK]             = _gMp5a5LowHealthIdleAnimationBank4,
    },
};

/// Encoding-1 local translations and rotations for mid-health idle.
///
/// Tracks 0 and 1 use integer model coordinates and 4096 angle units per turn.
/// Read-only payload retains its serialized position in the data section.
static const AnimationPackedPose _gMp5a5MidHealthIdleAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/mp5a5_animation_09ED8_bank1.inc"
};

/// Encoding-4 rotations for mid-health idle, in eight-unit angle steps.
///
/// Tracks 2..18 preserve their model-part translations.
static const AnimationPackedRotation _gMp5a5MidHealthIdleAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/mp5a5_animation_09ED8_bank4.inc"
};

/// Keyframes and loop/stop controls for mid-health idle.
///
/// Track-local jumps repeat the tail; forward playback bypasses the final stop.
/// Segment durations count normal-rate frames; pose offsets count four-byte words.
static const AnimationRecord _gMp5a5MidHealthIdleAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/mp5a5_animation_09ED8_records.inc"
};

/// Absolute record starts for the 19 tracks of mid-health idle.
///
/// The final serialized halfword is retained but is not another track start.
static const u16 _gMp5a5MidHealthIdleAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/mp5a5_animation_09ED8_indices.inc"
};

/// MP5A5 player's mid-health idle clip in native bank slot 24.
///
/// Selected after 300 idle ticks while HP is above a quarter and at most half
/// the maximum. Track-local jumps repeat the idle motion.
/// Supplies player-rig tracks 0..18; child playback uses 1..18.
/// Playback borrows this clip while the MP5A5 or either upgrade package is loaded.
static AnimationSet _gMp5a5MidHealthIdleAnimation = {
    .records           = _gMp5a5MidHealthIdleAnimationRecords,
    .trackStartIndices = _gMp5a5MidHealthIdleAnimationIndices,
    .poseBanks         = {
        [MP5A5_ANIMATION_TRANSLATION_ROTATION_BANK] = _gMp5a5MidHealthIdleAnimationBank1,
        [MP5A5_ANIMATION_ROTATION_BANK]             = _gMp5a5MidHealthIdleAnimationBank4,
    },
};

/// Encoding-1 local translations and rotations for scripted presentation entry.
///
/// Tracks 0 and 1 use integer model coordinates and 4096 angle units per turn.
/// Read-only payload retains its serialized position in the data section.
static const AnimationPackedPose _gMp5a5ScriptedPresentationEntryAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/mp5a5_animation_0A358_bank1.inc"
};

/// Encoding-4 rotations for scripted presentation entry, in eight-unit angle steps.
///
/// Tracks 2..18 preserve their model-part translations.
static const AnimationPackedRotation _gMp5a5ScriptedPresentationEntryAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/mp5a5_animation_0A358_bank4.inc"
};

/// Keyframes and stop controls for scripted presentation entry.
///
/// Segment durations count normal-rate frames; pose offsets count four-byte words.
static const AnimationRecord _gMp5a5ScriptedPresentationEntryAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/mp5a5_animation_0A358_records.inc"
};

/// Absolute record starts for the 19 tracks of scripted presentation entry.
///
/// The final serialized halfword is retained but is not another track start.
static const u16 _gMp5a5ScriptedPresentationEntryAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/mp5a5_animation_0A358_indices.inc"
};

/// MP5A5 player's scripted event-entry clip in native bank slots 32 and 34.
///
/// The zero presentation selector plays slot 32 before CAP weapon events
/// and scripted room interactions. All tracks stop at their final pose.
/// Supplies player-rig tracks 0..18; child playback uses 1..18.
/// Playback borrows this clip while the MP5A5 or either upgrade package is loaded.
static AnimationSet _gMp5a5ScriptedPresentationEntryAnimation = {
    .records           = _gMp5a5ScriptedPresentationEntryAnimationRecords,
    .trackStartIndices = _gMp5a5ScriptedPresentationEntryAnimationIndices,
    .poseBanks         = {
        [MP5A5_ANIMATION_TRANSLATION_ROTATION_BANK] = _gMp5a5ScriptedPresentationEntryAnimationBank1,
        [MP5A5_ANIMATION_ROTATION_BANK]             = _gMp5a5ScriptedPresentationEntryAnimationBank4,
    },
};

/// Encoding-1 local translations and rotations for scripted presentation exit.
///
/// Tracks 0 and 1 use integer model coordinates and 4096 angle units per turn.
/// Read-only payload retains its serialized position in the data section.
static const AnimationPackedPose _gMp5a5ScriptedPresentationExitAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/mp5a5_animation_0A530_bank1.inc"
};

/// Encoding-4 rotations for scripted presentation exit, in eight-unit angle steps.
///
/// Tracks 2..18 preserve their model-part translations.
static const AnimationPackedRotation _gMp5a5ScriptedPresentationExitAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/mp5a5_animation_0A530_bank4.inc"
};

/// Keyframes and stop controls for scripted presentation exit.
///
/// Segment durations count normal-rate frames; pose offsets count four-byte words.
static const AnimationRecord _gMp5a5ScriptedPresentationExitAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/mp5a5_animation_0A530_records.inc"
};

/// Absolute record starts for the 19 tracks of scripted presentation exit.
///
/// The final serialized halfword is retained but is not another track start.
static const u16 _gMp5a5ScriptedPresentationExitAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/mp5a5_animation_0A530_indices.inc"
};

/// MP5A5 player's scripted event-exit clip in native bank slots 33 and 35.
///
/// The nonzero presentation selector plays slot 33 after CAP weapon events
/// and scripted room interactions. All tracks stop at their final pose.
/// Supplies player-rig tracks 0..18; child playback uses 1..18.
/// Playback borrows this clip while the MP5A5 or either upgrade package is loaded.
static AnimationSet _gMp5a5ScriptedPresentationExitAnimation = {
    .records           = _gMp5a5ScriptedPresentationExitAnimationRecords,
    .trackStartIndices = _gMp5a5ScriptedPresentationExitAnimationIndices,
    .poseBanks         = {
        [MP5A5_ANIMATION_TRANSLATION_ROTATION_BANK] = _gMp5a5ScriptedPresentationExitAnimationBank1,
        [MP5A5_ANIMATION_ROTATION_BANK]             = _gMp5a5ScriptedPresentationExitAnimationBank4,
    },
};

/// Encoding-1 local translations and rotations for Parasite Energy family-2 release.
///
/// Tracks 0 and 1 use integer model coordinates and 4096 angle units per turn.
/// Read-only payload retains its serialized position in the data section.
static const AnimationPackedPose _gMp5a5ParasiteEnergyReleaseFamily2AnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/mp5a5_animation_0AA94_bank1.inc"
};

/// Encoding-4 rotations for Parasite Energy family-2 release, in eight-unit angle steps.
///
/// Tracks 2..18 preserve their model-part translations.
static const AnimationPackedRotation _gMp5a5ParasiteEnergyReleaseFamily2AnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/mp5a5_animation_0AA94_bank4.inc"
};

/// Keyframes and stop controls for Parasite Energy family-2 release.
///
/// Segment durations count normal-rate frames; pose offsets count four-byte words.
static const AnimationRecord _gMp5a5ParasiteEnergyReleaseFamily2AnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/mp5a5_animation_0AA94_records.inc"
};

/// Absolute record starts for the 19 tracks of Parasite Energy family-2 release.
///
/// The final serialized halfword is retained but is not another track start.
static const u16 _gMp5a5ParasiteEnergyReleaseFamily2AnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/mp5a5_animation_0AA94_indices.inc"
};

/// MP5A5 player's Parasite Energy family-2 release clip in native bank slot 42.
///
/// Starts the release/charge/cast sequence while the attachment countdown begins.
/// Family 2 selects attachment IDs below 601 whose decimal tens digit is 3.
/// Supplies player-rig tracks 0..18; child playback uses 1..18.
/// Playback borrows this clip while the MP5A5 or either upgrade package is loaded.
static AnimationSet _gMp5a5ParasiteEnergyReleaseFamily2Animation = {
    .records           = _gMp5a5ParasiteEnergyReleaseFamily2AnimationRecords,
    .trackStartIndices = _gMp5a5ParasiteEnergyReleaseFamily2AnimationIndices,
    .poseBanks         = {
        [MP5A5_ANIMATION_TRANSLATION_ROTATION_BANK] = _gMp5a5ParasiteEnergyReleaseFamily2AnimationBank1,
        [MP5A5_ANIMATION_ROTATION_BANK]             = _gMp5a5ParasiteEnergyReleaseFamily2AnimationBank4,
    },
};

/// Encoding-1 local translations and rotations for Parasite Energy family-2 charge.
///
/// Tracks 0 and 1 use integer model coordinates and 4096 angle units per turn.
/// Read-only payload retains its serialized position in the data section.
static const AnimationPackedPose _gMp5a5ParasiteEnergyChargeFamily2AnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/mp5a5_animation_0AC88_bank1.inc"
};

/// Encoding-4 rotations for Parasite Energy family-2 charge, in eight-unit angle steps.
///
/// Tracks 2..18 preserve their model-part translations.
static const AnimationPackedRotation _gMp5a5ParasiteEnergyChargeFamily2AnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/mp5a5_animation_0AC88_bank4.inc"
};

/// Keyframes and loop/stop controls for Parasite Energy family-2 charge.
///
/// Track-local jumps restart the two-keyframe loop; playback bypasses the final stop.
/// Segment durations count normal-rate frames; pose offsets count four-byte words.
static const AnimationRecord _gMp5a5ParasiteEnergyChargeFamily2AnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/mp5a5_animation_0AC88_records.inc"
};

/// Absolute record starts for the 19 tracks of Parasite Energy family-2 charge.
///
/// The final serialized halfword is retained but is not another track start.
static const u16 _gMp5a5ParasiteEnergyChargeFamily2AnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/mp5a5_animation_0AC88_indices.inc"
};

/// MP5A5 player's Parasite Energy family-2 charge clip in native bank slot 43.
///
/// Loops the charge stance after release until the attachment countdown reaches zero.
/// Family 2 selects attachment IDs below 601 whose decimal tens digit is 3.
/// Supplies player-rig tracks 0..18; child playback uses 1..18.
/// Playback borrows this clip while the MP5A5 or either upgrade package is loaded.
static AnimationSet _gMp5a5ParasiteEnergyChargeFamily2Animation = {
    .records           = _gMp5a5ParasiteEnergyChargeFamily2AnimationRecords,
    .trackStartIndices = _gMp5a5ParasiteEnergyChargeFamily2AnimationIndices,
    .poseBanks         = {
        [MP5A5_ANIMATION_TRANSLATION_ROTATION_BANK] = _gMp5a5ParasiteEnergyChargeFamily2AnimationBank1,
        [MP5A5_ANIMATION_ROTATION_BANK]             = _gMp5a5ParasiteEnergyChargeFamily2AnimationBank4,
    },
};

/// Encoding-1 local translations and rotations for Parasite Energy family-2 cast.
///
/// Tracks 0 and 1 use integer model coordinates and 4096 angle units per turn.
/// Read-only payload retains its serialized position in the data section.
static const AnimationPackedPose _gMp5a5ParasiteEnergyCastFamily2AnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/mp5a5_animation_0B108_bank1.inc"
};

/// Encoding-4 rotations for Parasite Energy family-2 cast, in eight-unit angle steps.
///
/// Tracks 2..18 preserve their model-part translations.
static const AnimationPackedRotation _gMp5a5ParasiteEnergyCastFamily2AnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/mp5a5_animation_0B108_bank4.inc"
};

/// Keyframes and stop controls for Parasite Energy family-2 cast.
///
/// Segment durations count normal-rate frames; pose offsets count four-byte words.
static const AnimationRecord _gMp5a5ParasiteEnergyCastFamily2AnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/mp5a5_animation_0B108_records.inc"
};

/// Absolute record starts for the 19 tracks of Parasite Energy family-2 cast.
///
/// The final serialized halfword is retained but is not another track start.
static const u16 _gMp5a5ParasiteEnergyCastFamily2AnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/mp5a5_animation_0B108_indices.inc"
};

/// MP5A5 player's Parasite Energy family-2 cast clip in native bank slot 44.
///
/// Starts when the attachment countdown reaches zero and stops at its final pose.
/// Family 2 selects attachment IDs below 601 whose decimal tens digit is 3.
/// Supplies player-rig tracks 0..18; child playback uses 1..18.
/// Playback borrows this clip while the MP5A5 or either upgrade package is loaded.
static AnimationSet _gMp5a5ParasiteEnergyCastFamily2Animation = {
    .records           = _gMp5a5ParasiteEnergyCastFamily2AnimationRecords,
    .trackStartIndices = _gMp5a5ParasiteEnergyCastFamily2AnimationIndices,
    .poseBanks         = {
        [MP5A5_ANIMATION_TRANSLATION_ROTATION_BANK] = _gMp5a5ParasiteEnergyCastFamily2AnimationBank1,
        [MP5A5_ANIMATION_ROTATION_BANK]             = _gMp5a5ParasiteEnergyCastFamily2AnimationBank4,
    },
};

/// Encoding-1 local translations and rotations for forward aim walking.
///
/// Tracks 0 and 1 use integer model coordinates and 4096 angle units per turn.
/// Read-only, word-aligned payload retains its serialized data-section position.
static const AnimationPackedPose _gMp5a5AimForwardWalkAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/mp5a5_animation_0BBB0_bank1.inc"
};

/// Encoding-4 rotations for forward aim walking, in eight-unit angle steps.
///
/// Tracks 2..18 preserve their model-part translations.
static const AnimationPackedRotation _gMp5a5AimForwardWalkAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/mp5a5_animation_0BBB0_bank4.inc"
};

/// Keyframes, footstep cues and loop controls for forward aim walking.
///
/// Track 1 carries `ANIMATION_RECORD_CUE_1` and `ANIMATION_RECORD_CUE_2`.
/// Track-local jumps repeat the stride before the trailing stop records.
/// Segment durations count normal-rate frames; pose offsets count four-byte words.
static const AnimationRecord _gMp5a5AimForwardWalkAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/mp5a5_animation_0BBB0_records.inc"
};

/// Absolute record starts for the 19 tracks of forward aim walking.
///
/// The final serialized halfword is retained but is not another track start.
static const u16 _gMp5a5AimForwardWalkAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/mp5a5_animation_0BBB0_indices.inc"
};

/// MP5A5 player's forward aim-walk loop in native bank slot 12.
///
/// Used for forward movement while aiming; actor control supplies root displacement.
/// Supplies player-rig tracks 0..18; child playback uses 1..18.
/// Playback borrows this clip while the MP5A5 or either upgrade package is loaded.
static AnimationSet _gMp5a5AimForwardWalkAnimation = {
    .records           = _gMp5a5AimForwardWalkAnimationRecords,
    .trackStartIndices = _gMp5a5AimForwardWalkAnimationIndices,
    .poseBanks         = {
        [MP5A5_ANIMATION_TRANSLATION_ROTATION_BANK] = _gMp5a5AimForwardWalkAnimationBank1,
        [MP5A5_ANIMATION_ROTATION_BANK]             = _gMp5a5AimForwardWalkAnimationBank4,
    },
};

/// Encoding-1 local translations and rotations for the steady aim pose.
///
/// Tracks 0 and 1 use integer model coordinates and 4096 angle units per turn.
/// Read-only, word-aligned payload retains its serialized data-section position.
static const AnimationPackedPose _gMp5a5AimHoldAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/mp5a5_animation_0BD88_bank1.inc"
};

/// Encoding-4 rotations for the steady aim pose, in eight-unit angle steps.
///
/// Tracks 2..18 preserve their model-part translations.
static const AnimationPackedRotation _gMp5a5AimHoldAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/mp5a5_animation_0BD88_bank4.inc"
};

/// Repeated pose keyframes and loop controls for the steady aim pose.
///
/// Each track loops two equal one-frame keyframes before its trailing stop.
/// Pose offsets count four-byte words; no action or footstep cues are present.
static const AnimationRecord _gMp5a5AimHoldAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/mp5a5_animation_0BD88_records.inc"
};

/// Absolute record starts for the 19 tracks of the steady aim pose.
///
/// The final serialized halfword is retained but is not another track start.
static const u16 _gMp5a5AimHoldAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/mp5a5_animation_0BD88_indices.inc"
};

/// MP5A5 player's steady aim pose in native bank slot 9.
///
/// Holds stationary aim and prepares either attack; secondary fire continues this pose.
/// Also supplies the aim hold used after enemy grab recovery.
/// Supplies player-rig tracks 0..18; child playback uses 1..18.
/// Playback borrows this clip while the MP5A5 or either upgrade package is loaded.
static AnimationSet _gMp5a5AimHoldAnimation = {
    .records           = _gMp5a5AimHoldAnimationRecords,
    .trackStartIndices = _gMp5a5AimHoldAnimationIndices,
    .poseBanks         = {
        [MP5A5_ANIMATION_TRANSLATION_ROTATION_BANK] = _gMp5a5AimHoldAnimationBank1,
        [MP5A5_ANIMATION_ROTATION_BANK]             = _gMp5a5AimHoldAnimationBank4,
    },
};

/// Encoding-1 local translations and rotations for backward aim walking or turning.
///
/// Tracks 0 and 1 use integer model coordinates and 4096 angle units per turn.
/// Read-only, word-aligned payload retains its serialized data-section position.
static const AnimationPackedPose _gMp5a5AimBackwardOrTurnAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/mp5a5_animation_0C4BC_bank1.inc"
};

/// Encoding-4 rotations for backward aim walking or turning, in eight-unit angle steps.
///
/// Tracks 2..18 preserve their model-part translations.
static const AnimationPackedRotation _gMp5a5AimBackwardOrTurnAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/mp5a5_animation_0C4BC_bank4.inc"
};

/// Keyframes, footstep cues and loop controls for backward aim walking or turning.
///
/// Track 1 carries `ANIMATION_RECORD_CUE_1` and `ANIMATION_RECORD_CUE_2`.
/// Track-local jumps repeat the stride before the trailing stop records.
/// Segment durations count normal-rate frames; pose offsets count four-byte words.
static const AnimationRecord _gMp5a5AimBackwardOrTurnAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/mp5a5_animation_0C4BC_records.inc"
};

/// Absolute record starts for the 19 tracks of backward aim walking or turning.
///
/// The final serialized halfword is retained but is not another track start.
static const u16 _gMp5a5AimBackwardOrTurnAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/mp5a5_animation_0C4BC_indices.inc"
};

/// MP5A5 player's backward aim-walk and stationary-turn loop in native bank slot 13.
///
/// Used for backward movement or turning while aiming; actor control supplies root motion.
/// Supplies player-rig tracks 0..18; child playback uses 1..18.
/// Playback borrows this clip while the MP5A5 or either upgrade package is loaded.
static AnimationSet _gMp5a5AimBackwardOrTurnAnimation = {
    .records           = _gMp5a5AimBackwardOrTurnAnimationRecords,
    .trackStartIndices = _gMp5a5AimBackwardOrTurnAnimationIndices,
    .poseBanks         = {
        [MP5A5_ANIMATION_TRANSLATION_ROTATION_BANK] = _gMp5a5AimBackwardOrTurnAnimationBank1,
        [MP5A5_ANIMATION_ROTATION_BANK]             = _gMp5a5AimBackwardOrTurnAnimationBank4,
    },
};

/// Encoding-1 local translations and rotations for the MP5A5 reload.
///
/// Tracks 0 and 1 use integer model coordinates and 4096 angle units per turn.
/// Read-only, word-aligned payload retains its serialized data-section position.
static const AnimationPackedPose _gMp5a5ReloadAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/mp5a5_animation_0D268_bank1.inc"
};

/// Encoding-4 rotations for the MP5A5 reload, in eight-unit angle steps.
///
/// Tracks 2..18 preserve their model-part translations.
static const AnimationPackedRotation _gMp5a5ReloadAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/mp5a5_animation_0D268_bank4.inc"
};

/// Keyframes, the load-completion cue and stop controls for the MP5A5 reload.
///
/// Track 1 carries `ANIMATION_RECORD_CUE_MASK` together at the loading keyframe.
/// Segment durations count normal-rate frames; pose offsets count four-byte words.
static const AnimationRecord _gMp5a5ReloadAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/mp5a5_animation_0D268_records.inc"
};

/// Absolute record starts for the 19 tracks of the MP5A5 reload.
///
/// The final serialized halfword is retained but is not another track start.
static const u16 _gMp5a5ReloadAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/mp5a5_animation_0D268_indices.inc"
};

/// MP5A5 player's reload clip shared by native bank slots 14, 15 and 20.
///
/// Slots 14/15 select the primary/secondary load; slot 20 serves battle-end reloads.
/// Track 1's combined cue applies the selected load; each track stops at its final pose.
/// Supplies player-rig tracks 0..18; child playback uses 1..18.
/// Playback borrows this clip while the MP5A5 or either upgrade package is loaded.
static AnimationSet _gMp5a5ReloadAnimation = {
    .records           = _gMp5a5ReloadAnimationRecords,
    .trackStartIndices = _gMp5a5ReloadAnimationIndices,
    .poseBanks         = {
        [MP5A5_ANIMATION_TRANSLATION_ROTATION_BANK] = _gMp5a5ReloadAnimationBank1,
        [MP5A5_ANIMATION_ROTATION_BANK]             = _gMp5a5ReloadAnimationBank4,
    },
};

/// Encoding-1 local translations and rotations for the MP5A5 firing clip.
///
/// Tracks 0 and 1 use integer model coordinates and 4096 angle units per turn.
/// Read-only, word-aligned payload retains its serialized data-section position.
static const AnimationPackedPose _gMp5a5FireAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/mp5a5_animation_0D768_bank1.inc"
};

/// Encoding-4 rotations for the MP5A5 firing clip, in eight-unit angle steps.
///
/// Tracks 2..18 preserve their model-part translations.
static const AnimationPackedRotation _gMp5a5FireAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/mp5a5_animation_0D768_bank4.inc"
};

/// Keyframes and stop controls for the MP5A5 firing clip.
///
/// Segment durations count normal-rate frames; pose offsets count four-byte words.
/// No action or footstep cues are present.
static const AnimationRecord _gMp5a5FireAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/mp5a5_animation_0D768_records.inc"
};

/// Absolute record starts for the 19 tracks of the MP5A5 firing clip.
///
/// The final serialized halfword is retained but is not another track start.
static const u16 _gMp5a5FireAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/mp5a5_animation_0D768_indices.inc"
};

/// MP5A5 player's firing clip shared by native bank slots 10 and 11.
///
/// Primary fire restarts slot 10 with a two-frame blend on every shot.
/// Each track stops at its final pose. Supplies tracks 0..18; child playback uses 1..18.
/// Playback borrows this clip while the MP5A5 or either upgrade package is loaded.
static AnimationSet _gMp5a5FireAnimation = {
    .records           = _gMp5a5FireAnimationRecords,
    .trackStartIndices = _gMp5a5FireAnimationIndices,
    .poseBanks         = {
        [MP5A5_ANIMATION_TRANSLATION_ROTATION_BANK] = _gMp5a5FireAnimationBank1,
        [MP5A5_ANIMATION_ROTATION_BANK]             = _gMp5a5FireAnimationBank4,
    },
};

/// Encoding-1 local translations and rotations for entering the MP5A5 aim stance.
///
/// Tracks 0 and 1 use integer model coordinates and 4096 angle units per turn.
/// Read-only, word-aligned payload retains its serialized data-section position.
static const AnimationPackedPose _gMp5a5AimEntryAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/mp5a5_animation_0DC58_bank1.inc"
};

/// Encoding-4 rotations for entering the MP5A5 aim stance, in eight-unit angle steps.
///
/// Tracks 2..18 preserve their model-part translations.
static const AnimationPackedRotation _gMp5a5AimEntryAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/mp5a5_animation_0DC58_bank4.inc"
};

/// Keyframes and stop controls for entering the MP5A5 aim stance.
///
/// Segment durations count normal-rate frames; pose offsets count four-byte words.
/// No action or footstep cues are present.
static const AnimationRecord _gMp5a5AimEntryAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/mp5a5_animation_0DC58_records.inc"
};

/// Absolute record starts for the 19 tracks of MP5A5 aim entry.
///
/// The final serialized halfword is retained but is not another track start.
static const u16 _gMp5a5AimEntryAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/mp5a5_animation_0DC58_indices.inc"
};

/// MP5A5 player's aim-entry clip in native bank slot 7.
///
/// Stops in the aim stance, then normal animation control selects the aim-hold clip.
/// Enemy recovery can also borrow this set to restore the player's aim pose.
/// Supplies player-rig tracks 0..18; child playback uses 1..18.
/// Playback borrows this clip while the MP5A5 or either upgrade package is loaded.
static AnimationSet _gMp5a5AimEntryAnimation = {
    .records           = _gMp5a5AimEntryAnimationRecords,
    .trackStartIndices = _gMp5a5AimEntryAnimationIndices,
    .poseBanks         = {
        [MP5A5_ANIMATION_TRANSLATION_ROTATION_BANK] = _gMp5a5AimEntryAnimationBank1,
        [MP5A5_ANIMATION_ROTATION_BANK]             = _gMp5a5AimEntryAnimationBank4,
    },
};

/// Aim-exit translations in model units and rotations in 1/4096 turns for tracks 0..1.
static const AnimationPackedPose _gMp5a5AimExitAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/mp5a5_animation_0E1E4_bank1.inc"
};

/// Aim-exit rotations for tracks 2..18, packed in eight-unit angular steps.
static const AnimationPackedRotation _gMp5a5AimExitAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/mp5a5_animation_0E1E4_bank4.inc"
};

/// Aim-exit keyframes and stop controls, without action cues.
///
/// Durations count normal-rate frames and pose offsets count four-byte words.
static const AnimationRecord _gMp5a5AimExitAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/mp5a5_animation_0E1E4_records.inc"
};

/// Absolute aim-exit record starts for tracks 0..18, followed by a retained serialized halfword.
static const u16 _gMp5a5AimExitAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/mp5a5_animation_0E1E4_indices.inc"
};
STATIC_ASSERT(ARRAY_SIZE(_gMp5a5AimExitAnimationIndices) == GAME_ACTOR_NORMAL_ANIMATION_SLOTS + 1, mp5a5_aim_exit_track_starts);

/// Player transition out of MP5A5 aiming, at native clip id 8.
///
/// Normal and scripted attack exits play child tracks 1..18; every track stops.
/// Tracks 0..1 carry translation/rotation and 2..18 rotation only. The MP5A5
/// or active upgrade package must stay loaded while slots or cached records refer to it.
static AnimationSet _gMp5a5AimExitAnimation = {
    .records           = _gMp5a5AimExitAnimationRecords,
    .trackStartIndices = _gMp5a5AimExitAnimationIndices,
    .poseBanks         = {
        [MP5A5_ANIMATION_TRANSLATION_ROTATION_BANK] = _gMp5a5AimExitAnimationBank1,
        [MP5A5_ANIMATION_ROTATION_BANK]             = _gMp5a5AimExitAnimationBank4,
    },
};

AnimationBank gMp5a5PlayerAnimationBank = {
    .table = {
        .sets = {
            NULL,
            &_gMp5a5IdleAnimation,
            &_gMp5a5WalkAnimation,
            &_gMp5a5BackwardWalkAnimation,
            &_gMp5a5RunAnimation,
            &_gMp5a5TurnLeftAnimation,
            &_gMp5a5TurnRightAnimation,
            &_gMp5a5AimEntryAnimation,
            &_gMp5a5AimExitAnimation,
            &_gMp5a5AimHoldAnimation,
            &_gMp5a5FireAnimation,
            &_gMp5a5FireAnimation,
            &_gMp5a5AimForwardWalkAnimation,
            &_gMp5a5AimBackwardOrTurnAnimation,
            &_gMp5a5ReloadAnimation,
            &_gMp5a5ReloadAnimation,
            &_gMp5a5Part4HitAnimation,
            &_gMp5a5OtherBodyHitAnimation,
            &_gMp5a5DeathAnimation,
            &_gMp5a5WalkAnimation,
            &_gMp5a5ReloadAnimation,
            &_gMp5a5IdleAnimation,
            &_gMp5a5IdleAnimation,
            &_gMp5a5HealthyIdleAnimation,
            &_gMp5a5MidHealthIdleAnimation,
            &_gMp5a5LowHealthIdleAnimation,
            &_gMp5a5ParasiteEnergyReleaseFamily0Animation,
            &_gMp5a5ParasiteEnergyChargeFamily0Animation,
            &_gMp5a5ParasiteEnergyCastFamily0Animation,
            &_gMp5a5ParasiteEnergyReleaseFamily1Animation,
            &_gMp5a5ParasiteEnergyChargeFamily1Animation,
            &_gMp5a5ParasiteEnergyCastFamily1Animation,
            &_gMp5a5ScriptedPresentationEntryAnimation,
            &_gMp5a5ScriptedPresentationExitAnimation,
            &_gMp5a5ScriptedPresentationEntryAnimation,
            &_gMp5a5ScriptedPresentationExitAnimation,
            &_gMp5a5StairAscentAnimation,
            &_gMp5a5StairDescentAnimation,
            &_gMp5a5StairAscentFinishEvenAnimation,
            &_gMp5a5StairAscentFinishOddAnimation,
            &_gMp5a5ItemUseAnimation,
            &_gMp5a5IdleAnimation,
            &_gMp5a5ParasiteEnergyReleaseFamily2Animation,
            &_gMp5a5ParasiteEnergyChargeFamily2Animation,
            &_gMp5a5ParasiteEnergyCastFamily2Animation,
            // Unused base slots and all raw extension words start cleared.
        },
    },
};

/// Retained streak directions shared by every live flash in this weapon package.
///
/// Angles use 4096 units per turn, one random direction in each quadrant.
/// Each newly initialized flash replaces the directions older flashes read.
static s16 _gMuzzleFlashAngles[MUZZLE_FLASH_STREAK_COUNT] = { 0, 0, 0, 0 };

PACKAGE_ALIASES
