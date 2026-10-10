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

/// Pose-bank selectors used by the P229 player's translation/rotation and rotation-only tracks.
enum {
    P229_ANIMATION_TRANSLATION_ROTATION_BANK = 1,
    P229_ANIMATION_PACKED_ROTATION_BANK      = 4,
};

/// Muzzle offset of the P229, in the firing hand's coordinate frame.
static SVECTOR _gMuzzleOffset = { 0, 0x140, 0x20, 0 };

static s16 _gMuzzleFlashAngles[MUZZLE_FLASH_STREAK_COUNT];

#define MUZZLE_FLASH_OFFSET _gMuzzleOffset
#include "../../shared/muzzle_flash_task.inc.c"
#undef MUZZLE_FLASH_OFFSET

void p229MuzzleFlashTask(Task* task)
{
    _muzzleFlashTask(task);
}

#include "../../shared/muzzle_flash_core.inc.c"

#include "../../shared/muzzle_flash_streak.inc.c"

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
                equipmentConsumeWeaponLoad(WEAPON_ITEM(P229_WEAPON_ID), EQUIPMENT_WEAPON_LOAD_CONSUME_PRIMARY);
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
                equipmentConsumeWeaponLoad(WEAPON_ITEM(P229_WEAPON_ID), EQUIPMENT_WEAPON_LOAD_CONSUME_SECONDARY);
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
            if (playerActorIsSlotAdvancingLinearly(playerTask, D_80112E04[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId][1], 0, 0) == 0 ||
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

/// Identity transform and self-parented root of the rigid held pistol.
///
/// Read-only asset tables retain their serialized positions in .data.
static const TmdBone _gP229WeaponModelSkeleton[] __attribute__((section(".data"))) = {
#include "assets/p229_model_01110_skeleton.inc"
};

/// Vertex count of the pistol's sole part, covering all 30 local vertices.
static const u32 _gP229WeaponModelPartVerts[ARRAY_SIZE(_gP229WeaponModelSkeleton)] __attribute__((section(".data"))) = {
#include "assets/p229_model_01110_partVerts.inc"
};

/// Pistol vertices in integer coordinates local to its single root part.
static SVECTOR _gP229WeaponModelVerts[] = {
#include "assets/p229_model_01110_verts.inc"
};

/// Part-local lighting normals with twelve fractional bits, indexed independently of vertices.
static SVECTOR _gP229WeaponModelNormals[] = {
#include "assets/p229_model_01110_normals.inc"
};

/// Writable draw commands for one flat textured quad, two Gouraud triangles and 24 Gouraud quads.
///
/// First creation resolves handler slots in place. The part group and an empty
/// trailing group end with `TMD_STREAM_GROUP_END`, then `TMD_STREAM_END`.
/// All packets use the directly transformed region of each primitive-buffer half.
static u32 _gP229WeaponModelStream[] = {
#include "assets/p229_model_01110_stream.inc"
};

TmdSource gP229WeaponModel = {
    .handlersResolved    = false,
    .bufferHalfBytes     = sizeof(POLY_FT4) + (2 * sizeof(POLY_GT3)) + (24 * sizeof(POLY_GT4)),
    .preXformRegionBytes = 0,
    .partCount           = ARRAY_SIZE(_gP229WeaponModelSkeleton),
    .partVertexCounts    = _gP229WeaponModelPartVerts,
    .verts               = _gP229WeaponModelVerts,
    .normals             = _gP229WeaponModelNormals,
    .skeleton            = _gP229WeaponModelSkeleton,
    .stream              = _gP229WeaponModelStream,
};

/// Translation/rotation poses for `_gP229PlayerIdleAnimation`, in model units and 1/4096 turns.
static const AnimationPackedPose _gP229PlayerIdleAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/p229_animation_015D8_bank1.inc"
};

/// Rotation-only poses for `_gP229PlayerIdleAnimation`, packed in eight-angle-unit steps.
static const AnimationPackedRotation _gP229PlayerIdleAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/p229_animation_015D8_bank4.inc"
};

/// Keyframe durations and loop controls for `_gP229PlayerIdleAnimation`'s nineteen part tracks.
static const AnimationRecord _gP229PlayerIdleAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/p229_animation_015D8_records.inc"
};

/// Absolute record starts for `_gP229PlayerIdleAnimation`'s model-part tracks 0..18.
///
/// The final serialized halfword is retained after the nineteen track starts;
/// it is outside the playback track range.
static const u16 _gP229PlayerIdleAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/p229_animation_015D8_indices.inc"
};

/// Standing player pose in the P229 bank, shared by set indices 1, 21, 22 and 41.
///
/// Locomotion selects set 1 when movement and turning stop. Tracks 0 and 1
/// carry translation/rotation; tracks 2..18 carry rotation only. These tracks
/// animate the player rig, and the package must remain loaded during playback.
static AnimationSet _gP229PlayerIdleAnimation = {
    .records           = _gP229PlayerIdleAnimationRecords,
    .trackStartIndices = _gP229PlayerIdleAnimationIndices,
    .poseBanks         = {
        [P229_ANIMATION_TRANSLATION_ROTATION_BANK] = _gP229PlayerIdleAnimationBank1,
        [P229_ANIMATION_PACKED_ROTATION_BANK]      = _gP229PlayerIdleAnimationBank4,
    },
};

/// Translation/rotation poses for `_gP229PlayerUnarmedWalkAnimation`, in model units and 1/4096 turns.
static const AnimationPackedPose _gP229PlayerUnarmedWalkAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/p229_animation_01C7C_bank1.inc"
};

/// Rotation-only poses for `_gP229PlayerUnarmedWalkAnimation`, packed in eight-angle-unit steps.
static const AnimationPackedRotation _gP229PlayerUnarmedWalkAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/p229_animation_01C7C_bank4.inc"
};

/// Keyframe durations and loop controls for `_gP229PlayerUnarmedWalkAnimation`'s nineteen part tracks.
static const AnimationRecord _gP229PlayerUnarmedWalkAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/p229_animation_01C7C_records.inc"
};

/// Absolute record starts for `_gP229PlayerUnarmedWalkAnimation`'s model-part tracks 0..18.
///
/// The final serialized halfword is retained after the nineteen track starts;
/// it is outside the playback track range.
static const u16 _gP229PlayerUnarmedWalkAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/p229_animation_01C7C_indices.inc"
};

/// Forward player walk when the P229 bank is active but the held model is absent.
///
/// Locomotion selects set 19 when equipment slot 1 has no task. Tracks 0 and 1
/// carry translation/rotation; tracks 2..18 carry rotation only. These tracks
/// animate the player rig, and the package must remain loaded during playback.
static AnimationSet _gP229PlayerUnarmedWalkAnimation = {
    .records           = _gP229PlayerUnarmedWalkAnimationRecords,
    .trackStartIndices = _gP229PlayerUnarmedWalkAnimationIndices,
    .poseBanks         = {
        [P229_ANIMATION_TRANSLATION_ROTATION_BANK] = _gP229PlayerUnarmedWalkAnimationBank1,
        [P229_ANIMATION_PACKED_ROTATION_BANK]      = _gP229PlayerUnarmedWalkAnimationBank4,
    },
};

/// Translation/rotation poses for `_gP229PlayerWalkAnimation`, in model units and 1/4096 turns.
static const AnimationPackedPose _gP229PlayerWalkAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/p229_animation_025C8_bank1.inc"
};

/// Rotation-only poses for `_gP229PlayerWalkAnimation`, packed in eight-angle-unit steps.
static const AnimationPackedRotation _gP229PlayerWalkAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/p229_animation_025C8_bank4.inc"
};

/// Keyframe durations and loop controls for `_gP229PlayerWalkAnimation`'s nineteen part tracks.
static const AnimationRecord _gP229PlayerWalkAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/p229_animation_025C8_records.inc"
};

/// Absolute record starts for `_gP229PlayerWalkAnimation`'s model-part tracks 0..18.
///
/// The final serialized halfword is retained after the nineteen track starts;
/// it is outside the playback track range.
static const u16 _gP229PlayerWalkAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/p229_animation_025C8_indices.inc"
};

/// Forward player walk with the P229 held, selected by native-bank set 2.
///
/// Locomotion selects it for forward movement without running while equipment
/// slot 1 has a task. Tracks 0 and 1 carry translation/rotation; tracks 2..18
/// carry rotation only. The package must remain loaded during player playback.
static AnimationSet _gP229PlayerWalkAnimation = {
    .records           = _gP229PlayerWalkAnimationRecords,
    .trackStartIndices = _gP229PlayerWalkAnimationIndices,
    .poseBanks         = {
        [P229_ANIMATION_TRANSLATION_ROTATION_BANK] = _gP229PlayerWalkAnimationBank1,
        [P229_ANIMATION_PACKED_ROTATION_BANK]      = _gP229PlayerWalkAnimationBank4,
    },
};

/// Translation/rotation poses for `_gP229PlayerBackwardWalkAnimation`, in model units and 1/4096 turns.
static const AnimationPackedPose _gP229PlayerBackwardWalkAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/p229_animation_02EA0_bank1.inc"
};

/// Rotation-only poses for `_gP229PlayerBackwardWalkAnimation`, packed in eight-angle-unit steps.
static const AnimationPackedRotation _gP229PlayerBackwardWalkAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/p229_animation_02EA0_bank4.inc"
};

/// Keyframe durations and loop controls for `_gP229PlayerBackwardWalkAnimation`'s nineteen part tracks.
static const AnimationRecord _gP229PlayerBackwardWalkAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/p229_animation_02EA0_records.inc"
};

/// Absolute record starts for `_gP229PlayerBackwardWalkAnimation`'s model-part tracks 0..18.
///
/// The final serialized halfword is retained after the nineteen track starts;
/// it is outside the playback track range.
static const u16 _gP229PlayerBackwardWalkAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/p229_animation_02EA0_indices.inc"
};

/// Backward player walk with the P229, selected by native-bank set 3.
///
/// Locomotion selects it for negative movement input. Tracks 0 and 1 carry
/// translation/rotation; tracks 2..18 carry rotation only. These tracks animate
/// the player rig, and the package must remain loaded during playback.
static AnimationSet _gP229PlayerBackwardWalkAnimation = {
    .records           = _gP229PlayerBackwardWalkAnimationRecords,
    .trackStartIndices = _gP229PlayerBackwardWalkAnimationIndices,
    .poseBanks         = {
        [P229_ANIMATION_TRANSLATION_ROTATION_BANK] = _gP229PlayerBackwardWalkAnimationBank1,
        [P229_ANIMATION_PACKED_ROTATION_BANK]      = _gP229PlayerBackwardWalkAnimationBank4,
    },
};

/// Translation/rotation poses for `_gP229PlayerRunAnimation`, in model units and 1/4096 turns.
static const AnimationPackedPose _gP229PlayerRunAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/p229_animation_035D4_bank1.inc"
};

/// Rotation-only poses for `_gP229PlayerRunAnimation`, packed in eight-angle-unit steps.
static const AnimationPackedRotation _gP229PlayerRunAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/p229_animation_035D4_bank4.inc"
};

/// Keyframe durations and loop controls for `_gP229PlayerRunAnimation`'s nineteen part tracks.
static const AnimationRecord _gP229PlayerRunAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/p229_animation_035D4_records.inc"
};

/// Absolute record starts for `_gP229PlayerRunAnimation`'s model-part tracks 0..18.
///
/// The final serialized halfword is retained after the nineteen track starts;
/// it is outside the playback track range.
static const u16 _gP229PlayerRunAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/p229_animation_035D4_indices.inc"
};

/// Forward player run with the P229, selected by native-bank set 4.
///
/// Locomotion selects it for permitted running according to the saved movement
/// mode and run input. Tracks 0 and 1 carry translation/rotation; tracks 2..18
/// carry rotation only. The package must remain loaded during player playback.
static AnimationSet _gP229PlayerRunAnimation = {
    .records           = _gP229PlayerRunAnimationRecords,
    .trackStartIndices = _gP229PlayerRunAnimationIndices,
    .poseBanks         = {
        [P229_ANIMATION_TRANSLATION_ROTATION_BANK] = _gP229PlayerRunAnimationBank1,
        [P229_ANIMATION_PACKED_ROTATION_BANK]      = _gP229PlayerRunAnimationBank4,
    },
};

static AnimationPackedPose _gP229Animation03E34Bank1[19] = {
#include "assets/p229_animation_03E34_bank1.inc"
};

static AnimationPackedRotation _gP229Animation03E34Bank4[169] = {
#include "assets/p229_animation_03E34_bank4.inc"
};

static AnimationRecord _gP229Animation03E34Records[290] = {
#include "assets/p229_animation_03E34_records.inc"
};

static u16 _gP229Animation03E34Indices[20] = {
#include "assets/p229_animation_03E34_indices.inc"
};

static AnimationSet _gP229Animation03E34 = {
    _gP229Animation03E34Records,
    _gP229Animation03E34Indices,
    { NULL, _gP229Animation03E34Bank1, NULL, NULL, _gP229Animation03E34Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gP229Animation04698Bank1[19] = {
#include "assets/p229_animation_04698_bank1.inc"
};

static AnimationPackedRotation _gP229Animation04698Bank4[170] = {
#include "assets/p229_animation_04698_bank4.inc"
};

static AnimationRecord _gP229Animation04698Records[290] = {
#include "assets/p229_animation_04698_records.inc"
};

static u16 _gP229Animation04698Indices[20] = {
#include "assets/p229_animation_04698_indices.inc"
};

static AnimationSet _gP229Animation04698 = {
    _gP229Animation04698Records,
    _gP229Animation04698Indices,
    { NULL, _gP229Animation04698Bank1, NULL, NULL, _gP229Animation04698Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gP229Animation049ACBank1[3] = {
#include "assets/p229_animation_049AC_bank1.inc"
};

static AnimationPackedRotation _gP229Animation049ACBank4[69] = {
#include "assets/p229_animation_049AC_bank4.inc"
};

static AnimationRecord _gP229Animation049ACRecords[99] = {
#include "assets/p229_animation_049AC_records.inc"
};

static u16 _gP229Animation049ACIndices[20] = {
#include "assets/p229_animation_049AC_indices.inc"
};

static AnimationSet _gP229Animation049AC = {
    _gP229Animation049ACRecords,
    _gP229Animation049ACIndices,
    { NULL, _gP229Animation049ACBank1, NULL, NULL, _gP229Animation049ACBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gP229Animation05108Bank1[14] = {
#include "assets/p229_animation_05108_bank1.inc"
};

static AnimationPackedRotation _gP229Animation05108Bank4[156] = {
#include "assets/p229_animation_05108_bank4.inc"
};

static AnimationRecord _gP229Animation05108Records[253] = {
#include "assets/p229_animation_05108_records.inc"
};

static u16 _gP229Animation05108Indices[20] = {
#include "assets/p229_animation_05108_indices.inc"
};

static AnimationSet _gP229Animation05108 = {
    _gP229Animation05108Records,
    _gP229Animation05108Indices,
    { NULL, _gP229Animation05108Bank1, NULL, NULL, _gP229Animation05108Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gP229Animation05890Bank1[16] = {
#include "assets/p229_animation_05890_bank1.inc"
};

static AnimationPackedRotation _gP229Animation05890Bank4[167] = {
#include "assets/p229_animation_05890_bank4.inc"
};

static AnimationRecord _gP229Animation05890Records[247] = {
#include "assets/p229_animation_05890_records.inc"
};

static u16 _gP229Animation05890Indices[20] = {
#include "assets/p229_animation_05890_indices.inc"
};

static AnimationSet _gP229Animation05890 = {
    _gP229Animation05890Records,
    _gP229Animation05890Indices,
    { NULL, _gP229Animation05890Bank1, NULL, NULL, _gP229Animation05890Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gP229Animation05B64Bank1[6] = {
#include "assets/p229_animation_05B64_bank1.inc"
};

static AnimationPackedRotation _gP229Animation05B64Bank4[52] = {
#include "assets/p229_animation_05B64_bank4.inc"
};

static AnimationRecord _gP229Animation05B64Records[91] = {
#include "assets/p229_animation_05B64_records.inc"
};

static u16 _gP229Animation05B64Indices[20] = {
#include "assets/p229_animation_05B64_indices.inc"
};

static AnimationSet _gP229Animation05B64 = {
    _gP229Animation05B64Records,
    _gP229Animation05B64Indices,
    { NULL, _gP229Animation05B64Bank1, NULL, NULL, _gP229Animation05B64Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gP229Animation05EF4Bank1[7] = {
#include "assets/p229_animation_05EF4_bank1.inc"
};

static AnimationPackedRotation _gP229Animation05EF4Bank4[73] = {
#include "assets/p229_animation_05EF4_bank4.inc"
};

static AnimationRecord _gP229Animation05EF4Records[114] = {
#include "assets/p229_animation_05EF4_records.inc"
};

static u16 _gP229Animation05EF4Indices[20] = {
#include "assets/p229_animation_05EF4_indices.inc"
};

static AnimationSet _gP229Animation05EF4 = {
    _gP229Animation05EF4Records,
    _gP229Animation05EF4Indices,
    { NULL, _gP229Animation05EF4Bank1, NULL, NULL, _gP229Animation05EF4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gP229Animation0637CBank1[9] = {
#include "assets/p229_animation_0637C_bank1.inc"
};

static AnimationPackedRotation _gP229Animation0637CBank4[104] = {
#include "assets/p229_animation_0637C_bank4.inc"
};

static AnimationRecord _gP229Animation0637CRecords[139] = {
#include "assets/p229_animation_0637C_records.inc"
};

static u16 _gP229Animation0637CIndices[20] = {
#include "assets/p229_animation_0637C_indices.inc"
};

static AnimationSet _gP229Animation0637C = {
    _gP229Animation0637CRecords,
    _gP229Animation0637CIndices,
    { NULL, _gP229Animation0637CBank1, NULL, NULL, _gP229Animation0637CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gP229Animation06578Bank1[3] = {
#include "assets/p229_animation_06578_bank1.inc"
};

static AnimationPackedRotation _gP229Animation06578Bank4[22] = {
#include "assets/p229_animation_06578_bank4.inc"
};

static AnimationRecord _gP229Animation06578Records[76] = {
#include "assets/p229_animation_06578_records.inc"
};

static u16 _gP229Animation06578Indices[20] = {
#include "assets/p229_animation_06578_indices.inc"
};

static AnimationSet _gP229Animation06578 = {
    _gP229Animation06578Records,
    _gP229Animation06578Indices,
    { NULL, _gP229Animation06578Bank1, NULL, NULL, _gP229Animation06578Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gP229Animation06850Bank1[6] = {
#include "assets/p229_animation_06850_bank1.inc"
};

static AnimationPackedRotation _gP229Animation06850Bank4[57] = {
#include "assets/p229_animation_06850_bank4.inc"
};

static AnimationRecord _gP229Animation06850Records[87] = {
#include "assets/p229_animation_06850_records.inc"
};

static u16 _gP229Animation06850Indices[20] = {
#include "assets/p229_animation_06850_indices.inc"
};

static AnimationSet _gP229Animation06850 = {
    _gP229Animation06850Records,
    _gP229Animation06850Indices,
    { NULL, _gP229Animation06850Bank1, NULL, NULL, _gP229Animation06850Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gP229Animation06AF4Bank1[4] = {
#include "assets/p229_animation_06AF4_bank1.inc"
};

static AnimationPackedRotation _gP229Animation06AF4Bank4[55] = {
#include "assets/p229_animation_06AF4_bank4.inc"
};

static AnimationRecord _gP229Animation06AF4Records[82] = {
#include "assets/p229_animation_06AF4_records.inc"
};

static u16 _gP229Animation06AF4Indices[20] = {
#include "assets/p229_animation_06AF4_indices.inc"
};

static AnimationSet _gP229Animation06AF4 = {
    _gP229Animation06AF4Records,
    _gP229Animation06AF4Indices,
    { NULL, _gP229Animation06AF4Bank1, NULL, NULL, _gP229Animation06AF4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gP229Animation06CF4Bank1[3] = {
#include "assets/p229_animation_06CF4_bank1.inc"
};

static AnimationPackedRotation _gP229Animation06CF4Bank4[23] = {
#include "assets/p229_animation_06CF4_bank4.inc"
};

static AnimationRecord _gP229Animation06CF4Records[76] = {
#include "assets/p229_animation_06CF4_records.inc"
};

static u16 _gP229Animation06CF4Indices[20] = {
#include "assets/p229_animation_06CF4_indices.inc"
};

static AnimationSet _gP229Animation06CF4 = {
    _gP229Animation06CF4Records,
    _gP229Animation06CF4Indices,
    { NULL, _gP229Animation06CF4Bank1, NULL, NULL, _gP229Animation06CF4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gP229Animation07048Bank1[8] = {
#include "assets/p229_animation_07048_bank1.inc"
};

static AnimationPackedRotation _gP229Animation07048Bank4[68] = {
#include "assets/p229_animation_07048_bank4.inc"
};

static AnimationRecord _gP229Animation07048Records[101] = {
#include "assets/p229_animation_07048_records.inc"
};

static u16 _gP229Animation07048Indices[20] = {
#include "assets/p229_animation_07048_indices.inc"
};

static AnimationSet _gP229Animation07048 = {
    _gP229Animation07048Records,
    _gP229Animation07048Indices,
    { NULL, _gP229Animation07048Bank1, NULL, NULL, _gP229Animation07048Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gP229Animation072FCBank1[5] = {
#include "assets/p229_animation_072FC_bank1.inc"
};

static AnimationPackedRotation _gP229Animation072FCBank4[55] = {
#include "assets/p229_animation_072FC_bank4.inc"
};

static AnimationRecord _gP229Animation072FCRecords[83] = {
#include "assets/p229_animation_072FC_records.inc"
};

static u16 _gP229Animation072FCIndices[20] = {
#include "assets/p229_animation_072FC_indices.inc"
};

static AnimationSet _gP229Animation072FC = {
    _gP229Animation072FCRecords,
    _gP229Animation072FCIndices,
    { NULL, _gP229Animation072FCBank1, NULL, NULL, _gP229Animation072FCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gP229Animation0761CBank1[6] = {
#include "assets/p229_animation_0761C_bank1.inc"
};

static AnimationPackedRotation _gP229Animation0761CBank4[66] = {
#include "assets/p229_animation_0761C_bank4.inc"
};

static AnimationRecord _gP229Animation0761CRecords[96] = {
#include "assets/p229_animation_0761C_records.inc"
};

static u16 _gP229Animation0761CIndices[20] = {
#include "assets/p229_animation_0761C_indices.inc"
};

static AnimationSet _gP229Animation0761C = {
    _gP229Animation0761CRecords,
    _gP229Animation0761CIndices,
    { NULL, _gP229Animation0761CBank1, NULL, NULL, _gP229Animation0761CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gP229Animation07DE0Bank1[18] = {
#include "assets/p229_animation_07DE0_bank1.inc"
};

static AnimationPackedRotation _gP229Animation07DE0Bank4[184] = {
#include "assets/p229_animation_07DE0_bank4.inc"
};

static AnimationRecord _gP229Animation07DE0Records[239] = {
#include "assets/p229_animation_07DE0_records.inc"
};

static u16 _gP229Animation07DE0Indices[20] = {
#include "assets/p229_animation_07DE0_indices.inc"
};

static AnimationSet _gP229Animation07DE0 = {
    _gP229Animation07DE0Records,
    _gP229Animation07DE0Indices,
    { NULL, _gP229Animation07DE0Bank1, NULL, NULL, _gP229Animation07DE0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gP229Animation09078Bank1[29] = {
#include "assets/p229_animation_09078_bank1.inc"
};

static AnimationPackedRotation _gP229Animation09078Bank4[450] = {
#include "assets/p229_animation_09078_bank4.inc"
};

static AnimationRecord _gP229Animation09078Records[633] = {
#include "assets/p229_animation_09078_records.inc"
};

static u16 _gP229Animation09078Indices[20] = {
#include "assets/p229_animation_09078_indices.inc"
};

static AnimationSet _gP229Animation09078 = {
    _gP229Animation09078Records,
    _gP229Animation09078Indices,
    { NULL, _gP229Animation09078Bank1, NULL, NULL, _gP229Animation09078Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gP229Animation09BF0Bank1[12] = {
#include "assets/p229_animation_09BF0_bank1.inc"
};

static AnimationPackedRotation _gP229Animation09BF0Bank4[266] = {
#include "assets/p229_animation_09BF0_bank4.inc"
};

static AnimationRecord _gP229Animation09BF0Records[412] = {
#include "assets/p229_animation_09BF0_records.inc"
};

static u16 _gP229Animation09BF0Indices[20] = {
#include "assets/p229_animation_09BF0_indices.inc"
};

static AnimationSet _gP229Animation09BF0 = {
    _gP229Animation09BF0Records,
    _gP229Animation09BF0Indices,
    { NULL, _gP229Animation09BF0Bank1, NULL, NULL, _gP229Animation09BF0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gP229Animation0A30CBank1[9] = {
#include "assets/p229_animation_0A30C_bank1.inc"
};

static AnimationPackedRotation _gP229Animation0A30CBank4[144] = {
#include "assets/p229_animation_0A30C_bank4.inc"
};

static AnimationRecord _gP229Animation0A30CRecords[264] = {
#include "assets/p229_animation_0A30C_records.inc"
};

static u16 _gP229Animation0A30CIndices[20] = {
#include "assets/p229_animation_0A30C_indices.inc"
};

static AnimationSet _gP229Animation0A30C = {
    _gP229Animation0A30CRecords,
    _gP229Animation0A30CIndices,
    { NULL, _gP229Animation0A30CBank1, NULL, NULL, _gP229Animation0A30CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gP229Animation0A78CBank1[6] = {
#include "assets/p229_animation_0A78C_bank1.inc"
};

static AnimationPackedRotation _gP229Animation0A78CBank4[107] = {
#include "assets/p229_animation_0A78C_bank4.inc"
};

static AnimationRecord _gP229Animation0A78CRecords[143] = {
#include "assets/p229_animation_0A78C_records.inc"
};

static u16 _gP229Animation0A78CIndices[20] = {
#include "assets/p229_animation_0A78C_indices.inc"
};

static AnimationSet _gP229Animation0A78C = {
    _gP229Animation0A78CRecords,
    _gP229Animation0A78CIndices,
    { NULL, _gP229Animation0A78CBank1, NULL, NULL, _gP229Animation0A78CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gP229Animation0A964Bank1[3] = {
#include "assets/p229_animation_0A964_bank1.inc"
};

static AnimationPackedRotation _gP229Animation0A964Bank4[32] = {
#include "assets/p229_animation_0A964_bank4.inc"
};

static AnimationRecord _gP229Animation0A964Records[57] = {
#include "assets/p229_animation_0A964_records.inc"
};

static u16 _gP229Animation0A964Indices[20] = {
#include "assets/p229_animation_0A964_indices.inc"
};

static AnimationSet _gP229Animation0A964 = {
    _gP229Animation0A964Records,
    _gP229Animation0A964Indices,
    { NULL, _gP229Animation0A964Bank1, NULL, NULL, _gP229Animation0A964Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gP229Animation0AEC8Bank1[11] = {
#include "assets/p229_animation_0AEC8_bank1.inc"
};

static AnimationPackedRotation _gP229Animation0AEC8Bank4[125] = {
#include "assets/p229_animation_0AEC8_bank4.inc"
};

static AnimationRecord _gP229Animation0AEC8Records[167] = {
#include "assets/p229_animation_0AEC8_records.inc"
};

static u16 _gP229Animation0AEC8Indices[20] = {
#include "assets/p229_animation_0AEC8_indices.inc"
};

static AnimationSet _gP229Animation0AEC8 = {
    _gP229Animation0AEC8Records,
    _gP229Animation0AEC8Indices,
    { NULL, _gP229Animation0AEC8Bank1, NULL, NULL, _gP229Animation0AEC8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gP229Animation0B0BCBank1[3] = {
#include "assets/p229_animation_0B0BC_bank1.inc"
};

static AnimationPackedRotation _gP229Animation0B0BCBank4[20] = {
#include "assets/p229_animation_0B0BC_bank4.inc"
};

static AnimationRecord _gP229Animation0B0BCRecords[76] = {
#include "assets/p229_animation_0B0BC_records.inc"
};

static u16 _gP229Animation0B0BCIndices[20] = {
#include "assets/p229_animation_0B0BC_indices.inc"
};

static AnimationSet _gP229Animation0B0BC = {
    _gP229Animation0B0BCRecords,
    _gP229Animation0B0BCIndices,
    { NULL, _gP229Animation0B0BCBank1, NULL, NULL, _gP229Animation0B0BCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gP229Animation0B53CBank1[8] = {
#include "assets/p229_animation_0B53C_bank1.inc"
};

static AnimationPackedRotation _gP229Animation0B53CBank4[105] = {
#include "assets/p229_animation_0B53C_bank4.inc"
};

static AnimationRecord _gP229Animation0B53CRecords[139] = {
#include "assets/p229_animation_0B53C_records.inc"
};

static u16 _gP229Animation0B53CIndices[20] = {
#include "assets/p229_animation_0B53C_indices.inc"
};

static AnimationSet _gP229Animation0B53C = {
    _gP229Animation0B53CRecords,
    _gP229Animation0B53CIndices,
    { NULL, _gP229Animation0B53CBank1, NULL, NULL, _gP229Animation0B53CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gP229Animation0B718Bank1[2] = {
#include "assets/p229_animation_0B718_bank1.inc"
};

static AnimationPackedRotation _gP229Animation0B718Bank4[17] = {
#include "assets/p229_animation_0B718_bank4.inc"
};

static AnimationRecord _gP229Animation0B718Records[76] = {
#include "assets/p229_animation_0B718_records.inc"
};

static u16 _gP229Animation0B718Indices[20] = {
#include "assets/p229_animation_0B718_indices.inc"
};

static AnimationSet _gP229Animation0B718 = {
    _gP229Animation0B718Records,
    _gP229Animation0B718Indices,
    { NULL, _gP229Animation0B718Bank1, NULL, NULL, _gP229Animation0B718Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gP229Animation0BE08Bank1[12] = {
#include "assets/p229_animation_0BE08_bank1.inc"
};

static AnimationPackedRotation _gP229Animation0BE08Bank4[164] = {
#include "assets/p229_animation_0BE08_bank4.inc"
};

static AnimationRecord _gP229Animation0BE08Records[224] = {
#include "assets/p229_animation_0BE08_records.inc"
};

static u16 _gP229Animation0BE08Indices[20] = {
#include "assets/p229_animation_0BE08_indices.inc"
};

static AnimationSet _gP229Animation0BE08 = {
    _gP229Animation0BE08Records,
    _gP229Animation0BE08Indices,
    { NULL, _gP229Animation0BE08Bank1, NULL, NULL, _gP229Animation0BE08Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gP229Animation0C9F4Bank1[19] = {
#include "assets/p229_animation_0C9F4_bank1.inc"
};

static AnimationPackedRotation _gP229Animation0C9F4Bank4[304] = {
#include "assets/p229_animation_0C9F4_bank4.inc"
};

static AnimationRecord _gP229Animation0C9F4Records[382] = {
#include "assets/p229_animation_0C9F4_records.inc"
};

static u16 _gP229Animation0C9F4Indices[20] = {
#include "assets/p229_animation_0C9F4_indices.inc"
};

static AnimationSet _gP229Animation0C9F4 = {
    _gP229Animation0C9F4Records,
    _gP229Animation0C9F4Indices,
    { NULL, _gP229Animation0C9F4Bank1, NULL, NULL, _gP229Animation0C9F4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gP229Animation0D31CBank1[15] = {
#include "assets/p229_animation_0D31C_bank1.inc"
};

static AnimationPackedRotation _gP229Animation0D31CBank4[231] = {
#include "assets/p229_animation_0D31C_bank4.inc"
};

static AnimationRecord _gP229Animation0D31CRecords[290] = {
#include "assets/p229_animation_0D31C_records.inc"
};

static u16 _gP229Animation0D31CIndices[20] = {
#include "assets/p229_animation_0D31C_indices.inc"
};

static AnimationSet _gP229Animation0D31C = {
    _gP229Animation0D31CRecords,
    _gP229Animation0D31CIndices,
    { NULL, _gP229Animation0D31CBank1, NULL, NULL, _gP229Animation0D31CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gP229Animation0D6ECBank1[6] = {
#include "assets/p229_animation_0D6EC_bank1.inc"
};

static AnimationPackedRotation _gP229Animation0D6ECBank4[81] = {
#include "assets/p229_animation_0D6EC_bank4.inc"
};

static AnimationRecord _gP229Animation0D6ECRecords[125] = {
#include "assets/p229_animation_0D6EC_records.inc"
};

static u16 _gP229Animation0D6ECIndices[20] = {
#include "assets/p229_animation_0D6EC_indices.inc"
};

static AnimationSet _gP229Animation0D6EC = {
    _gP229Animation0D6ECRecords,
    _gP229Animation0D6ECIndices,
    { NULL, _gP229Animation0D6ECBank1, NULL, NULL, _gP229Animation0D6ECBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gP229Animation0DD68Bank1[11] = {
#include "assets/p229_animation_0DD68_bank1.inc"
};

static AnimationPackedRotation _gP229Animation0DD68Bank4[161] = {
#include "assets/p229_animation_0DD68_bank4.inc"
};

static AnimationRecord _gP229Animation0DD68Records[201] = {
#include "assets/p229_animation_0DD68_records.inc"
};

static u16 _gP229Animation0DD68Indices[20] = {
#include "assets/p229_animation_0DD68_indices.inc"
};

static AnimationSet _gP229Animation0DD68 = {
    _gP229Animation0DD68Records,
    _gP229Animation0DD68Indices,
    { NULL, _gP229Animation0DD68Bank1, NULL, NULL, _gP229Animation0DD68Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gP229Animation0E334Bank1[10] = {
#include "assets/p229_animation_0E334_bank1.inc"
};

static AnimationPackedRotation _gP229Animation0E334Bank4[145] = {
#include "assets/p229_animation_0E334_bank4.inc"
};

static AnimationRecord _gP229Animation0E334Records[176] = {
#include "assets/p229_animation_0E334_records.inc"
};

static u16 _gP229Animation0E334Indices[20] = {
#include "assets/p229_animation_0E334_indices.inc"
};

static AnimationSet _gP229Animation0E334 = {
    _gP229Animation0E334Records,
    _gP229Animation0E334Indices,
    { NULL, _gP229Animation0E334Bank1, NULL, NULL, _gP229Animation0E334Bank4, NULL, NULL, NULL },
};

AnimationBank D_p229_8012B51C = { { {
    NULL,
    &_gP229PlayerIdleAnimation,
    &_gP229PlayerWalkAnimation,
    &_gP229PlayerBackwardWalkAnimation,
    &_gP229PlayerRunAnimation,
    &_gP229Animation03E34,
    &_gP229Animation04698,
    &_gP229Animation0DD68,
    &_gP229Animation0E334,
    &_gP229Animation0B718,
    &_gP229Animation0D6EC,
    &_gP229Animation0D6EC,
    &_gP229Animation0C9F4,
    &_gP229Animation0BE08,
    &_gP229Animation0D31C,
    &_gP229Animation0D31C,
    &_gP229Animation072FC,
    &_gP229Animation0761C,
    &_gP229Animation07DE0,
    &_gP229PlayerUnarmedWalkAnimation,
    &_gP229Animation0D31C,
    &_gP229PlayerIdleAnimation,
    &_gP229PlayerIdleAnimation,
    &_gP229Animation09078,
    &_gP229Animation0A30C,
    &_gP229Animation09BF0,
    &_gP229Animation0637C,
    &_gP229Animation06578,
    &_gP229Animation06850,
    &_gP229Animation06AF4,
    &_gP229Animation06CF4,
    &_gP229Animation07048,
    &_gP229Animation0A78C,
    &_gP229Animation0A964,
    &_gP229Animation0A78C,
    &_gP229Animation0A964,
    &_gP229Animation05108,
    &_gP229Animation05890,
    &_gP229Animation05EF4,
    &_gP229Animation05B64,
    &_gP229Animation049AC,
    &_gP229PlayerIdleAnimation,
    &_gP229Animation0AEC8,
    &_gP229Animation0B0BC,
    &_gP229Animation0B53C,
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
