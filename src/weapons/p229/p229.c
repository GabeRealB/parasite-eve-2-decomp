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

/// Translation/rotation poses for `_gP229PlayerTurnLeftAnimation`, in model units and 1/4096 turns.
static const AnimationPackedPose _gP229PlayerTurnLeftAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/p229_animation_03E34_bank1.inc"
};

/// Rotation-only poses for `_gP229PlayerTurnLeftAnimation`, packed in eight-angle-unit steps.
static const AnimationPackedRotation _gP229PlayerTurnLeftAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/p229_animation_03E34_bank4.inc"
};

/// Keyframe durations and loop controls for `_gP229PlayerTurnLeftAnimation`'s nineteen part tracks.
static const AnimationRecord _gP229PlayerTurnLeftAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/p229_animation_03E34_records.inc"
};

/// Absolute record starts for `_gP229PlayerTurnLeftAnimation`'s model-part tracks 0..18.
///
/// The final serialized halfword is retained after the nineteen track starts;
/// it is outside the playback track range.
static const u16 _gP229PlayerTurnLeftAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/p229_animation_03E34_indices.inc"
};

/// Leftward player turn with the P229, selected by native-bank set 5.
///
/// Stationary left input and negative scripted yaw changes select this loop.
/// Tracks 0 and 1 carry translation/rotation; tracks 2..18 carry rotation only.
/// The package must remain loaded during player playback.
static AnimationSet _gP229PlayerTurnLeftAnimation = {
    .records           = _gP229PlayerTurnLeftAnimationRecords,
    .trackStartIndices = _gP229PlayerTurnLeftAnimationIndices,
    .poseBanks         = {
        [P229_ANIMATION_TRANSLATION_ROTATION_BANK] = _gP229PlayerTurnLeftAnimationBank1,
        [P229_ANIMATION_PACKED_ROTATION_BANK]      = _gP229PlayerTurnLeftAnimationBank4,
    },
};

/// Translation/rotation poses for `_gP229PlayerTurnRightAnimation`, in model units and 1/4096 turns.
static const AnimationPackedPose _gP229PlayerTurnRightAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/p229_animation_04698_bank1.inc"
};

/// Rotation-only poses for `_gP229PlayerTurnRightAnimation`, packed in eight-angle-unit steps.
static const AnimationPackedRotation _gP229PlayerTurnRightAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/p229_animation_04698_bank4.inc"
};

/// Keyframe durations and loop controls for `_gP229PlayerTurnRightAnimation`'s nineteen part tracks.
static const AnimationRecord _gP229PlayerTurnRightAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/p229_animation_04698_records.inc"
};

/// Absolute record starts for `_gP229PlayerTurnRightAnimation`'s model-part tracks 0..18.
///
/// The final serialized halfword is retained after the nineteen track starts;
/// it is outside the playback track range.
static const u16 _gP229PlayerTurnRightAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/p229_animation_04698_indices.inc"
};

/// Rightward player turn with the P229, selected by native-bank set 6.
///
/// Stationary right input and nonnegative scripted yaw changes select this loop.
/// Tracks 0 and 1 carry translation/rotation; tracks 2..18 carry rotation only.
/// The package must remain loaded during player playback.
static AnimationSet _gP229PlayerTurnRightAnimation = {
    .records           = _gP229PlayerTurnRightAnimationRecords,
    .trackStartIndices = _gP229PlayerTurnRightAnimationIndices,
    .poseBanks         = {
        [P229_ANIMATION_TRANSLATION_ROTATION_BANK] = _gP229PlayerTurnRightAnimationBank1,
        [P229_ANIMATION_PACKED_ROTATION_BANK]      = _gP229PlayerTurnRightAnimationBank4,
    },
};

/// Translation/rotation poses for `_gP229PlayerItemUseAnimation`, in model units and 1/4096 turns.
static const AnimationPackedPose _gP229PlayerItemUseAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/p229_animation_049AC_bank1.inc"
};

/// Rotation-only poses for `_gP229PlayerItemUseAnimation`, packed in eight-angle-unit steps.
static const AnimationPackedRotation _gP229PlayerItemUseAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/p229_animation_049AC_bank4.inc"
};

/// Keyframe durations and stop controls for `_gP229PlayerItemUseAnimation`'s nineteen part tracks.
static const AnimationRecord _gP229PlayerItemUseAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/p229_animation_049AC_records.inc"
};

/// Absolute record starts for `_gP229PlayerItemUseAnimation`'s model-part tracks 0..18.
///
/// The final serialized halfword is retained after the nineteen track starts;
/// it is outside the playback track range.
static const u16 _gP229PlayerItemUseAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/p229_animation_049AC_indices.inc"
};

/// Player item-use gesture with the P229 bank, selected by native-bank set 40.
///
/// The scripted item-use state blends into this non-looping clip before returning
/// to aim locomotion; the clip itself does not apply an item. Tracks 0 and 1
/// carry translation/rotation; tracks 2..18 carry rotation only.
/// The package must remain loaded during player playback.
static AnimationSet _gP229PlayerItemUseAnimation = {
    .records           = _gP229PlayerItemUseAnimationRecords,
    .trackStartIndices = _gP229PlayerItemUseAnimationIndices,
    .poseBanks         = {
        [P229_ANIMATION_TRANSLATION_ROTATION_BANK] = _gP229PlayerItemUseAnimationBank1,
        [P229_ANIMATION_PACKED_ROTATION_BANK]      = _gP229PlayerItemUseAnimationBank4,
    },
};

/// Translation/rotation poses for `_gP229PlayerStairAscentAnimation`, in model units and 1/4096 turns.
static const AnimationPackedPose _gP229PlayerStairAscentAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/p229_animation_05108_bank1.inc"
};

/// Rotation-only poses for `_gP229PlayerStairAscentAnimation`, packed in eight-angle-unit steps.
static const AnimationPackedRotation _gP229PlayerStairAscentAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/p229_animation_05108_bank4.inc"
};

/// Keyframe durations, footstep cues and loop controls for `_gP229PlayerStairAscentAnimation`'s nineteen part tracks.
static const AnimationRecord _gP229PlayerStairAscentAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/p229_animation_05108_records.inc"
};

/// Absolute record starts for `_gP229PlayerStairAscentAnimation`'s model-part tracks 0..18.
///
/// The final serialized halfword is retained after the nineteen track starts;
/// it is outside the playback track range.
static const u16 _gP229PlayerStairAscentAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/p229_animation_05108_indices.inc"
};

/// Player stair-ascent loop with the P229 bank, selected by native-bank set 36.
///
/// A zero stair-direction request selects ascent. Track 1's footstep cues gate
/// the scripted strides while the flight continues. Tracks 0 and 1 carry
/// translation/rotation; tracks 2..18 carry rotation only.
/// The package must remain loaded during player playback.
static AnimationSet _gP229PlayerStairAscentAnimation = {
    .records           = _gP229PlayerStairAscentAnimationRecords,
    .trackStartIndices = _gP229PlayerStairAscentAnimationIndices,
    .poseBanks         = {
        [P229_ANIMATION_TRANSLATION_ROTATION_BANK] = _gP229PlayerStairAscentAnimationBank1,
        [P229_ANIMATION_PACKED_ROTATION_BANK]      = _gP229PlayerStairAscentAnimationBank4,
    },
};

/// Translation/rotation poses for `_gP229PlayerStairDescentAnimation`, in model units and 1/4096 turns.
static const AnimationPackedPose _gP229PlayerStairDescentAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/p229_animation_05890_bank1.inc"
};

/// Rotation-only poses for `_gP229PlayerStairDescentAnimation`, packed in eight-angle-unit steps.
static const AnimationPackedRotation _gP229PlayerStairDescentAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/p229_animation_05890_bank4.inc"
};

/// Keyframe durations, footstep cues and loop controls for `_gP229PlayerStairDescentAnimation`'s nineteen part tracks.
static const AnimationRecord _gP229PlayerStairDescentAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/p229_animation_05890_records.inc"
};

/// Absolute record starts for `_gP229PlayerStairDescentAnimation`'s model-part tracks 0..18.
///
/// The final serialized halfword is retained after the nineteen track starts;
/// it is outside the playback track range.
static const u16 _gP229PlayerStairDescentAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/p229_animation_05890_indices.inc"
};

/// Player stair-descent loop with the P229 bank, selected by native-bank set 37.
///
/// A nonzero stair-direction request selects descent. Track 1's footstep cues
/// gate the scripted strides through the final step. Tracks 0 and 1 carry
/// translation/rotation; tracks 2..18 carry rotation only.
/// The package must remain loaded during player playback.
static AnimationSet _gP229PlayerStairDescentAnimation = {
    .records           = _gP229PlayerStairDescentAnimationRecords,
    .trackStartIndices = _gP229PlayerStairDescentAnimationIndices,
    .poseBanks         = {
        [P229_ANIMATION_TRANSLATION_ROTATION_BANK] = _gP229PlayerStairDescentAnimationBank1,
        [P229_ANIMATION_PACKED_ROTATION_BANK]      = _gP229PlayerStairDescentAnimationBank4,
    },
};

/// Translation/rotation poses for `_gP229PlayerStairAscentFinishOddAnimation`, in model units and 1/4096 turns.
static const AnimationPackedPose _gP229PlayerStairAscentFinishOddAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/p229_animation_05B64_bank1.inc"
};

/// Rotation-only poses for `_gP229PlayerStairAscentFinishOddAnimation`, packed in eight-angle-unit steps.
static const AnimationPackedRotation _gP229PlayerStairAscentFinishOddAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/p229_animation_05B64_bank4.inc"
};

/// Keyframe durations, the final footstep cue and stop controls for `_gP229PlayerStairAscentFinishOddAnimation`'s nineteen part tracks.
static const AnimationRecord _gP229PlayerStairAscentFinishOddAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/p229_animation_05B64_records.inc"
};

/// Absolute record starts for `_gP229PlayerStairAscentFinishOddAnimation`'s model-part tracks 0..18.
///
/// The final serialized halfword is retained after the nineteen track starts;
/// it is outside the playback track range.
static const u16 _gP229PlayerStairAscentFinishOddAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/p229_animation_05B64_indices.inc"
};

/// Player stair-ascent finish for an odd initial step count, in native-bank set 39.
///
/// This non-looping P229-bank clip completes the level stride at the top of
/// the flight, with a footstep cue on track 1. Tracks 0 and 1 carry
/// translation/rotation; tracks 2..18 carry rotation only.
/// The package must remain loaded during player playback.
static AnimationSet _gP229PlayerStairAscentFinishOddAnimation = {
    .records           = _gP229PlayerStairAscentFinishOddAnimationRecords,
    .trackStartIndices = _gP229PlayerStairAscentFinishOddAnimationIndices,
    .poseBanks         = {
        [P229_ANIMATION_TRANSLATION_ROTATION_BANK] = _gP229PlayerStairAscentFinishOddAnimationBank1,
        [P229_ANIMATION_PACKED_ROTATION_BANK]      = _gP229PlayerStairAscentFinishOddAnimationBank4,
    },
};

/// Translation/rotation poses for `_gP229PlayerStairAscentFinishEvenAnimation`, in model units and 1/4096 turns.
static const AnimationPackedPose _gP229PlayerStairAscentFinishEvenAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/p229_animation_05EF4_bank1.inc"
};

/// Rotation-only poses for `_gP229PlayerStairAscentFinishEvenAnimation`, packed in eight-angle-unit steps.
static const AnimationPackedRotation _gP229PlayerStairAscentFinishEvenAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/p229_animation_05EF4_bank4.inc"
};

/// Keyframe durations, a track-1 footstep cue and stop controls for `_gP229PlayerStairAscentFinishEvenAnimation`.
static const AnimationRecord _gP229PlayerStairAscentFinishEvenAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/p229_animation_05EF4_records.inc"
};

/// Absolute record starts for `_gP229PlayerStairAscentFinishEvenAnimation`'s model-part tracks 0..18.
///
/// The final serialized halfword is retained after the nineteen track starts;
/// it is outside the playback track range.
static const u16 _gP229PlayerStairAscentFinishEvenAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/p229_animation_05EF4_indices.inc"
};

/// Player stair-ascent finish for an even initial step count, in native-bank set 38.
///
/// This non-looping P229-bank clip completes the level stride at the top of
/// the flight, with a footstep cue on track 1. Tracks 0 and 1 carry
/// translation/rotation; tracks 2..18 carry rotation only.
/// The package must remain loaded during player playback.
static AnimationSet _gP229PlayerStairAscentFinishEvenAnimation = {
    .records           = _gP229PlayerStairAscentFinishEvenAnimationRecords,
    .trackStartIndices = _gP229PlayerStairAscentFinishEvenAnimationIndices,
    .poseBanks         = {
        [P229_ANIMATION_TRANSLATION_ROTATION_BANK] = _gP229PlayerStairAscentFinishEvenAnimationBank1,
        [P229_ANIMATION_PACKED_ROTATION_BANK]      = _gP229PlayerStairAscentFinishEvenAnimationBank4,
    },
};

/// Translation/rotation poses for `_gP229PlayerParasiteEnergyReleaseFamily0Animation`, in model units and 1/4096 turns.
static const AnimationPackedPose _gP229PlayerParasiteEnergyReleaseFamily0AnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/p229_animation_0637C_bank1.inc"
};

/// Rotation-only poses for `_gP229PlayerParasiteEnergyReleaseFamily0Animation`, packed in eight-angle-unit steps.
static const AnimationPackedRotation _gP229PlayerParasiteEnergyReleaseFamily0AnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/p229_animation_0637C_bank4.inc"
};

/// Keyframe durations and stop controls for `_gP229PlayerParasiteEnergyReleaseFamily0Animation`'s nineteen part tracks.
static const AnimationRecord _gP229PlayerParasiteEnergyReleaseFamily0AnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/p229_animation_0637C_records.inc"
};

/// Absolute record starts for `_gP229PlayerParasiteEnergyReleaseFamily0Animation`'s model-part tracks 0..18.
///
/// The final serialized halfword is retained after the nineteen track starts;
/// it is outside the playback track range.
static const u16 _gP229PlayerParasiteEnergyReleaseFamily0AnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/p229_animation_0637C_indices.inc"
};

/// Player Parasite Energy family-0 release gesture in P229 native-bank set 26.
///
/// Starts the normal-mode action before the family-0 charge loop. The attachment
/// ID selects the clip family, shared by spells and item actions.
/// Tracks 0 and 1 carry translation/rotation; tracks 2..18 carry rotation only.
/// Every track stops at its final pose; the package must remain loaded during playback.
static AnimationSet _gP229PlayerParasiteEnergyReleaseFamily0Animation = {
    .records           = _gP229PlayerParasiteEnergyReleaseFamily0AnimationRecords,
    .trackStartIndices = _gP229PlayerParasiteEnergyReleaseFamily0AnimationIndices,
    .poseBanks         = {
        [P229_ANIMATION_TRANSLATION_ROTATION_BANK] = _gP229PlayerParasiteEnergyReleaseFamily0AnimationBank1,
        [P229_ANIMATION_PACKED_ROTATION_BANK]      = _gP229PlayerParasiteEnergyReleaseFamily0AnimationBank4,
    },
};

/// Translation/rotation poses for `_gP229PlayerParasiteEnergyChargeFamily0Animation`, in model units and 1/4096 turns.
static const AnimationPackedPose _gP229PlayerParasiteEnergyChargeFamily0AnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/p229_animation_06578_bank1.inc"
};

/// Rotation-only poses for `_gP229PlayerParasiteEnergyChargeFamily0Animation`, packed in eight-angle-unit steps.
static const AnimationPackedRotation _gP229PlayerParasiteEnergyChargeFamily0AnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/p229_animation_06578_bank4.inc"
};

/// Keyframe durations and loop controls for `_gP229PlayerParasiteEnergyChargeFamily0Animation`'s nineteen part tracks.
///
/// Each track jumps to its first keyframe; the serialized stop following each
/// jump is retained even though normal forward playback does not reach it.
static const AnimationRecord _gP229PlayerParasiteEnergyChargeFamily0AnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/p229_animation_06578_records.inc"
};

/// Absolute record starts for `_gP229PlayerParasiteEnergyChargeFamily0Animation`'s model-part tracks 0..18.
///
/// The final serialized halfword is retained after the nineteen track starts;
/// it is outside the playback track range.
static const u16 _gP229PlayerParasiteEnergyChargeFamily0AnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/p229_animation_06578_indices.inc"
};

/// Player Parasite Energy family-0 charge loop in P229 native-bank set 27.
///
/// Loops after the release gesture until the attachment countdown reaches zero,
/// then gives way to the family-0 cast gesture.
/// Tracks 0 and 1 carry translation/rotation; tracks 2..18 carry rotation only.
/// The package must remain loaded during player playback.
static AnimationSet _gP229PlayerParasiteEnergyChargeFamily0Animation = {
    .records           = _gP229PlayerParasiteEnergyChargeFamily0AnimationRecords,
    .trackStartIndices = _gP229PlayerParasiteEnergyChargeFamily0AnimationIndices,
    .poseBanks         = {
        [P229_ANIMATION_TRANSLATION_ROTATION_BANK] = _gP229PlayerParasiteEnergyChargeFamily0AnimationBank1,
        [P229_ANIMATION_PACKED_ROTATION_BANK]      = _gP229PlayerParasiteEnergyChargeFamily0AnimationBank4,
    },
};

/// Translation/rotation poses for `_gP229PlayerParasiteEnergyCastFamily0Animation`, in model units and 1/4096 turns.
static const AnimationPackedPose _gP229PlayerParasiteEnergyCastFamily0AnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/p229_animation_06850_bank1.inc"
};

/// Rotation-only poses for `_gP229PlayerParasiteEnergyCastFamily0Animation`, packed in eight-angle-unit steps.
static const AnimationPackedRotation _gP229PlayerParasiteEnergyCastFamily0AnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/p229_animation_06850_bank4.inc"
};

/// Keyframe durations and stop controls for `_gP229PlayerParasiteEnergyCastFamily0Animation`'s nineteen part tracks.
static const AnimationRecord _gP229PlayerParasiteEnergyCastFamily0AnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/p229_animation_06850_records.inc"
};

/// Absolute record starts for `_gP229PlayerParasiteEnergyCastFamily0Animation`'s model-part tracks 0..18.
///
/// The final serialized halfword is retained after the nineteen track starts;
/// it is outside the playback track range.
static const u16 _gP229PlayerParasiteEnergyCastFamily0AnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/p229_animation_06850_indices.inc"
};

/// Player Parasite Energy family-0 cast gesture in P229 native-bank set 28.
///
/// Replaces the charge loop when the attachment countdown reaches zero and
/// completes before the player's previous locomotion or aim state resumes.
/// Tracks 0 and 1 carry translation/rotation; tracks 2..18 carry rotation only.
/// Every track stops at its final pose; the package must remain loaded during playback.
static AnimationSet _gP229PlayerParasiteEnergyCastFamily0Animation = {
    .records           = _gP229PlayerParasiteEnergyCastFamily0AnimationRecords,
    .trackStartIndices = _gP229PlayerParasiteEnergyCastFamily0AnimationIndices,
    .poseBanks         = {
        [P229_ANIMATION_TRANSLATION_ROTATION_BANK] = _gP229PlayerParasiteEnergyCastFamily0AnimationBank1,
        [P229_ANIMATION_PACKED_ROTATION_BANK]      = _gP229PlayerParasiteEnergyCastFamily0AnimationBank4,
    },
};

/// Translation/rotation poses for `_gP229PlayerParasiteEnergyReleaseFamily1Animation`, in model units and 1/4096 turns.
static const AnimationPackedPose _gP229PlayerParasiteEnergyReleaseFamily1AnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/p229_animation_06AF4_bank1.inc"
};

/// Rotation-only poses for `_gP229PlayerParasiteEnergyReleaseFamily1Animation`, packed in eight-angle-unit steps.
static const AnimationPackedRotation _gP229PlayerParasiteEnergyReleaseFamily1AnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/p229_animation_06AF4_bank4.inc"
};

/// Keyframe durations and stop controls for `_gP229PlayerParasiteEnergyReleaseFamily1Animation`'s nineteen part tracks.
static const AnimationRecord _gP229PlayerParasiteEnergyReleaseFamily1AnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/p229_animation_06AF4_records.inc"
};

/// Absolute record starts for `_gP229PlayerParasiteEnergyReleaseFamily1Animation`'s model-part tracks 0..18.
///
/// The final serialized halfword is retained after the nineteen track starts;
/// it is outside the playback track range.
static const u16 _gP229PlayerParasiteEnergyReleaseFamily1AnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/p229_animation_06AF4_indices.inc"
};

/// Player Parasite Energy family-1 release gesture in P229 native-bank set 29.
///
/// Starts the normal-mode action before the family-1 charge loop. The attachment
/// ID selects the clip family, shared by spells and item actions.
/// Tracks 0 and 1 carry translation/rotation; tracks 2..18 carry rotation only.
/// Every track stops at its final pose; the package must remain loaded during playback.
static AnimationSet _gP229PlayerParasiteEnergyReleaseFamily1Animation = {
    .records           = _gP229PlayerParasiteEnergyReleaseFamily1AnimationRecords,
    .trackStartIndices = _gP229PlayerParasiteEnergyReleaseFamily1AnimationIndices,
    .poseBanks         = {
        [P229_ANIMATION_TRANSLATION_ROTATION_BANK] = _gP229PlayerParasiteEnergyReleaseFamily1AnimationBank1,
        [P229_ANIMATION_PACKED_ROTATION_BANK]      = _gP229PlayerParasiteEnergyReleaseFamily1AnimationBank4,
    },
};

/// Translation/rotation poses for `_gP229PlayerParasiteEnergyChargeFamily1Animation`, in model units and 1/4096 turns.
static const AnimationPackedPose _gP229PlayerParasiteEnergyChargeFamily1AnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/p229_animation_06CF4_bank1.inc"
};

/// Rotation-only poses for `_gP229PlayerParasiteEnergyChargeFamily1Animation`, packed in eight-angle-unit steps.
static const AnimationPackedRotation _gP229PlayerParasiteEnergyChargeFamily1AnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/p229_animation_06CF4_bank4.inc"
};

/// Keyframe durations and loop controls for `_gP229PlayerParasiteEnergyChargeFamily1Animation`'s nineteen part tracks.
///
/// Each track jumps to its first keyframe; the serialized stop following each
/// jump is retained even though normal forward playback does not reach it.
static const AnimationRecord _gP229PlayerParasiteEnergyChargeFamily1AnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/p229_animation_06CF4_records.inc"
};

/// Absolute record starts for `_gP229PlayerParasiteEnergyChargeFamily1Animation`'s model-part tracks 0..18.
///
/// The final serialized halfword is retained after the nineteen track starts;
/// it is outside the playback track range.
static const u16 _gP229PlayerParasiteEnergyChargeFamily1AnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/p229_animation_06CF4_indices.inc"
};

/// Player Parasite Energy family-1 charge loop in P229 native-bank set 30.
///
/// Loops after the release gesture until the attachment countdown reaches zero,
/// then gives way to the family-1 cast gesture in set 31.
/// Tracks 0 and 1 carry translation/rotation; tracks 2..18 carry rotation only.
/// The package must remain loaded during player playback.
static AnimationSet _gP229PlayerParasiteEnergyChargeFamily1Animation = {
    .records           = _gP229PlayerParasiteEnergyChargeFamily1AnimationRecords,
    .trackStartIndices = _gP229PlayerParasiteEnergyChargeFamily1AnimationIndices,
    .poseBanks         = {
        [P229_ANIMATION_TRANSLATION_ROTATION_BANK] = _gP229PlayerParasiteEnergyChargeFamily1AnimationBank1,
        [P229_ANIMATION_PACKED_ROTATION_BANK]      = _gP229PlayerParasiteEnergyChargeFamily1AnimationBank4,
    },
};

/// Translation/rotation poses for `_gP229PlayerParasiteEnergyCastFamily1Animation`, in model units and 1/4096 turns.
static const AnimationPackedPose _gP229PlayerParasiteEnergyCastFamily1AnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/p229_animation_07048_bank1.inc"
};

/// Rotation-only poses for `_gP229PlayerParasiteEnergyCastFamily1Animation`, packed in eight-angle-unit steps.
static const AnimationPackedRotation _gP229PlayerParasiteEnergyCastFamily1AnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/p229_animation_07048_bank4.inc"
};

/// Keyframes and stop controls for `_gP229PlayerParasiteEnergyCastFamily1Animation`'s nineteen part tracks.
///
/// Pose offsets count four-byte words; keyframe durations count normal-rate frames.
static const AnimationRecord _gP229PlayerParasiteEnergyCastFamily1AnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/p229_animation_07048_records.inc"
};

/// Absolute record starts for `_gP229PlayerParasiteEnergyCastFamily1Animation`'s model-part tracks 0..18.
///
/// The final serialized halfword is retained outside the playback track range.
static const u16 _gP229PlayerParasiteEnergyCastFamily1AnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/p229_animation_07048_indices.inc"
};

/// Player Parasite Energy family-1 cast gesture in P229 native-bank set 31.
///
/// Starts after the family-1 attachment countdown reaches zero and plays child
/// slots 1..18. Tracks 0 and 1 carry translation/rotation; tracks 2..18 carry
/// rotation only. Every track stops at its final pose. The package and borrowed
/// clip data must remain loaded throughout playback.
static AnimationSet _gP229PlayerParasiteEnergyCastFamily1Animation = {
    .records           = _gP229PlayerParasiteEnergyCastFamily1AnimationRecords,
    .trackStartIndices = _gP229PlayerParasiteEnergyCastFamily1AnimationIndices,
    .poseBanks         = {
        [P229_ANIMATION_TRANSLATION_ROTATION_BANK] = _gP229PlayerParasiteEnergyCastFamily1AnimationBank1,
        [P229_ANIMATION_PACKED_ROTATION_BANK]      = _gP229PlayerParasiteEnergyCastFamily1AnimationBank4,
    },
};

/// Translation/rotation poses for `_gP229PlayerPart4HitReactionAnimation`, in model units and 1/4096 turns.
static const AnimationPackedPose _gP229PlayerPart4HitReactionAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/p229_animation_072FC_bank1.inc"
};

/// Rotation-only poses for `_gP229PlayerPart4HitReactionAnimation`, packed in eight-angle-unit steps.
static const AnimationPackedRotation _gP229PlayerPart4HitReactionAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/p229_animation_072FC_bank4.inc"
};

/// Keyframes and stop controls for `_gP229PlayerPart4HitReactionAnimation`'s nineteen part tracks.
///
/// Pose offsets count four-byte words; keyframe durations count normal-rate frames.
static const AnimationRecord _gP229PlayerPart4HitReactionAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/p229_animation_072FC_records.inc"
};

/// Absolute record starts for `_gP229PlayerPart4HitReactionAnimation`'s model-part tracks 0..18.
///
/// The final serialized halfword is retained outside the playback track range.
static const u16 _gP229PlayerPart4HitReactionAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/p229_animation_072FC_indices.inc"
};

/// Player hit reaction for the model-part-4 collision sphere in P229 native-bank set 16.
///
/// Hit region 1 selects this clip with a three-frame blend on child slots 1..18.
/// Tracks 0 and 1 carry translation/rotation; tracks 2..18 carry rotation only.
/// Every track stops at its final pose. The package and borrowed clip data must
/// remain loaded throughout playback.
static AnimationSet _gP229PlayerPart4HitReactionAnimation = {
    .records           = _gP229PlayerPart4HitReactionAnimationRecords,
    .trackStartIndices = _gP229PlayerPart4HitReactionAnimationIndices,
    .poseBanks         = {
        [P229_ANIMATION_TRANSLATION_ROTATION_BANK] = _gP229PlayerPart4HitReactionAnimationBank1,
        [P229_ANIMATION_PACKED_ROTATION_BANK]      = _gP229PlayerPart4HitReactionAnimationBank4,
    },
};

/// Translation/rotation poses for `_gP229PlayerOtherBodyHitReactionAnimation`, in model units and 1/4096 turns.
static const AnimationPackedPose _gP229PlayerOtherBodyHitReactionAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/p229_animation_0761C_bank1.inc"
};

/// Rotation-only poses for `_gP229PlayerOtherBodyHitReactionAnimation`, packed in eight-angle-unit steps.
static const AnimationPackedRotation _gP229PlayerOtherBodyHitReactionAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/p229_animation_0761C_bank4.inc"
};

/// Keyframes and stop controls for `_gP229PlayerOtherBodyHitReactionAnimation`'s nineteen part tracks.
///
/// Pose offsets count four-byte words; keyframe durations count normal-rate frames.
static const AnimationRecord _gP229PlayerOtherBodyHitReactionAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/p229_animation_0761C_records.inc"
};

/// Absolute record starts for `_gP229PlayerOtherBodyHitReactionAnimation`'s model-part tracks 0..18.
///
/// The final serialized halfword is retained outside the playback track range.
static const u16 _gP229PlayerOtherBodyHitReactionAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/p229_animation_0761C_indices.inc"
};

/// Player hit reaction for collision bodies other than model part 4 in P229 native-bank set 17.
///
/// Hit region 2 selects this clip with a three-frame blend on child slots 1..18.
/// Tracks 0 and 1 carry translation/rotation; tracks 2..18 carry rotation only.
/// Every track stops at its final pose. The package and borrowed clip data must
/// remain loaded throughout playback.
static AnimationSet _gP229PlayerOtherBodyHitReactionAnimation = {
    .records           = _gP229PlayerOtherBodyHitReactionAnimationRecords,
    .trackStartIndices = _gP229PlayerOtherBodyHitReactionAnimationIndices,
    .poseBanks         = {
        [P229_ANIMATION_TRANSLATION_ROTATION_BANK] = _gP229PlayerOtherBodyHitReactionAnimationBank1,
        [P229_ANIMATION_PACKED_ROTATION_BANK]      = _gP229PlayerOtherBodyHitReactionAnimationBank4,
    },
};

/// Translation/rotation poses for `_gP229PlayerDeathPoseAnimation`, in model units and 1/4096 turns.
static const AnimationPackedPose _gP229PlayerDeathPoseAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/p229_animation_07DE0_bank1.inc"
};

/// Rotation-only poses for `_gP229PlayerDeathPoseAnimation`, packed in eight-angle-unit steps.
static const AnimationPackedRotation _gP229PlayerDeathPoseAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/p229_animation_07DE0_bank4.inc"
};

/// Keyframes and stop controls for `_gP229PlayerDeathPoseAnimation`'s nineteen part tracks.
///
/// Pose offsets count four-byte words; keyframe durations count normal-rate frames.
static const AnimationRecord _gP229PlayerDeathPoseAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/p229_animation_07DE0_records.inc"
};

/// Absolute record starts for `_gP229PlayerDeathPoseAnimation`'s model-part tracks 0..18.
///
/// The final serialized halfword is retained outside the playback track range.
static const u16 _gP229PlayerDeathPoseAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/p229_animation_07DE0_indices.inc"
};

/// Player death pose in P229 native-bank set 18.
///
/// Normal and scripted control select this clip when HP reaches zero, playing
/// child slots 1..18 with movement and turning stopped.
/// Tracks 0 and 1 carry translation/rotation; tracks 2..18 carry rotation only.
/// Every track stops at its final pose. The package and borrowed clip data must
/// remain loaded throughout playback.
static AnimationSet _gP229PlayerDeathPoseAnimation = {
    .records           = _gP229PlayerDeathPoseAnimationRecords,
    .trackStartIndices = _gP229PlayerDeathPoseAnimationIndices,
    .poseBanks         = {
        [P229_ANIMATION_TRANSLATION_ROTATION_BANK] = _gP229PlayerDeathPoseAnimationBank1,
        [P229_ANIMATION_PACKED_ROTATION_BANK]      = _gP229PlayerDeathPoseAnimationBank4,
    },
};

/// Translation/rotation poses for `_gP229PlayerHighHealthIdleAnimation`, in model units and 1/4096 turns.
static const AnimationPackedPose _gP229PlayerHighHealthIdleAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/p229_animation_09078_bank1.inc"
};

/// Rotation-only poses for `_gP229PlayerHighHealthIdleAnimation`, packed in eight-angle-unit steps.
static const AnimationPackedRotation _gP229PlayerHighHealthIdleAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/p229_animation_09078_bank4.inc"
};

/// Keyframes and loop controls for `_gP229PlayerHighHealthIdleAnimation`'s nineteen part tracks.
///
/// Pose offsets count four-byte words; durations count normal-rate frames.
/// Body tracks jump to a repeating suffix after their entry gesture. The stop
/// following each jump is retained but is not reached by forward playback.
static const AnimationRecord _gP229PlayerHighHealthIdleAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/p229_animation_09078_records.inc"
};

/// Absolute record starts for `_gP229PlayerHighHealthIdleAnimation`'s model-part tracks 0..18.
///
/// The final serialized halfword is retained outside the playback track range.
static const u16 _gP229PlayerHighHealthIdleAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/p229_animation_09078_indices.inc"
};

/// Player idle gesture for HP above half maximum in P229 native-bank set 23.
///
/// Selected after 300 direction-free locomotion ticks with a five-frame blend
/// on child slots 1..18; the half-maximum threshold truncates down. Tracks 0 and
/// 1 carry translation/rotation; tracks 2..18 carry rotation only. All tracks
/// loop, with body tracks repeating a suffix after their entry gesture.
/// The package and borrowed clip data must remain loaded throughout playback.
static AnimationSet _gP229PlayerHighHealthIdleAnimation = {
    .records           = _gP229PlayerHighHealthIdleAnimationRecords,
    .trackStartIndices = _gP229PlayerHighHealthIdleAnimationIndices,
    .poseBanks         = {
        [P229_ANIMATION_TRANSLATION_ROTATION_BANK] = _gP229PlayerHighHealthIdleAnimationBank1,
        [P229_ANIMATION_PACKED_ROTATION_BANK]      = _gP229PlayerHighHealthIdleAnimationBank4,
    },
};

/// Translation/rotation poses for `_gP229PlayerLowHealthIdleAnimation`, in model units and 1/4096 turns.
static const AnimationPackedPose _gP229PlayerLowHealthIdleAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/p229_animation_09BF0_bank1.inc"
};

/// Rotation-only poses for `_gP229PlayerLowHealthIdleAnimation`, packed in eight-angle-unit steps.
static const AnimationPackedRotation _gP229PlayerLowHealthIdleAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/p229_animation_09BF0_bank4.inc"
};

/// Keyframes and loop controls for `_gP229PlayerLowHealthIdleAnimation`'s nineteen part tracks.
///
/// Pose offsets count four-byte words; durations count normal-rate frames.
/// Moving tracks jump to a repeating suffix after their entry gesture. The stop
/// following each jump is retained but is not reached by forward playback.
static const AnimationRecord _gP229PlayerLowHealthIdleAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/p229_animation_09BF0_records.inc"
};

/// Absolute record starts for `_gP229PlayerLowHealthIdleAnimation`'s model-part tracks 0..18.
///
/// The final serialized halfword is retained outside the playback track range.
static const u16 _gP229PlayerLowHealthIdleAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/p229_animation_09BF0_indices.inc"
};

/// Player idle gesture for HP at or below a quarter maximum in P229 native-bank set 25.
///
/// Selected after 300 direction-free locomotion ticks with a five-frame blend;
/// the quarter-maximum threshold truncates down. Paralysis selects the same
/// clip regardless of HP with a six-frame blend. Both uses play child slots
/// 1..18. Tracks 0 and 1 carry translation/rotation; tracks 2..18 carry rotation
/// only. All tracks loop, with moving tracks repeating a suffix after entry.
/// The package and borrowed clip data must remain loaded throughout playback.
static AnimationSet _gP229PlayerLowHealthIdleAnimation = {
    .records           = _gP229PlayerLowHealthIdleAnimationRecords,
    .trackStartIndices = _gP229PlayerLowHealthIdleAnimationIndices,
    .poseBanks         = {
        [P229_ANIMATION_TRANSLATION_ROTATION_BANK] = _gP229PlayerLowHealthIdleAnimationBank1,
        [P229_ANIMATION_PACKED_ROTATION_BANK]      = _gP229PlayerLowHealthIdleAnimationBank4,
    },
};

/// Translation/rotation poses for `_gP229PlayerMidHealthIdleAnimation`, in model units and 1/4096 turns.
static const AnimationPackedPose _gP229PlayerMidHealthIdleAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/p229_animation_0A30C_bank1.inc"
};

/// Rotation-only poses for `_gP229PlayerMidHealthIdleAnimation`, packed in eight-angle-unit steps.
static const AnimationPackedRotation _gP229PlayerMidHealthIdleAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/p229_animation_0A30C_bank4.inc"
};

/// Keyframe durations in normal-rate frames and loop controls for `_gP229PlayerMidHealthIdleAnimation`'s nineteen part tracks.
static const AnimationRecord _gP229PlayerMidHealthIdleAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/p229_animation_0A30C_records.inc"
};

/// Absolute record starts for `_gP229PlayerMidHealthIdleAnimation`'s model-part tracks 0..18.
///
/// The final serialized halfword is retained after the nineteen track starts;
/// it is outside the playback track range.
static const u16 _gP229PlayerMidHealthIdleAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/p229_animation_0A30C_indices.inc"
};

/// Player idle gesture for HP above a quarter and at most half maximum in P229 native-bank set 24.
///
/// Selected after 300 direction-free locomotion ticks with a five-frame blend;
/// both HP thresholds truncate down. Child slots 1..18 play the gesture.
/// Tracks 0 and 1 carry translation/rotation; tracks 2..18 carry rotation only.
/// All tracks loop, with moving tracks repeating a suffix after entry.
/// The package and borrowed clip data must remain loaded throughout playback.
static AnimationSet _gP229PlayerMidHealthIdleAnimation = {
    .records           = _gP229PlayerMidHealthIdleAnimationRecords,
    .trackStartIndices = _gP229PlayerMidHealthIdleAnimationIndices,
    .poseBanks         = {
        [P229_ANIMATION_TRANSLATION_ROTATION_BANK] = _gP229PlayerMidHealthIdleAnimationBank1,
        [P229_ANIMATION_PACKED_ROTATION_BANK]      = _gP229PlayerMidHealthIdleAnimationBank4,
    },
};

/// Translation/rotation poses for `_gP229PlayerScriptedPresentationEntryAnimation`, in model units and 1/4096 turns.
static const AnimationPackedPose _gP229PlayerScriptedPresentationEntryAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/p229_animation_0A78C_bank1.inc"
};

/// Rotation-only poses for `_gP229PlayerScriptedPresentationEntryAnimation`, packed in eight-angle-unit steps.
static const AnimationPackedRotation _gP229PlayerScriptedPresentationEntryAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/p229_animation_0A78C_bank4.inc"
};

/// Keyframe durations in normal-rate frames and stop controls for `_gP229PlayerScriptedPresentationEntryAnimation`'s nineteen part tracks.
static const AnimationRecord _gP229PlayerScriptedPresentationEntryAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/p229_animation_0A78C_records.inc"
};

/// Absolute record starts for `_gP229PlayerScriptedPresentationEntryAnimation`'s model-part tracks 0..18.
///
/// The final serialized halfword is retained after the nineteen track starts;
/// it is outside the playback track range.
static const u16 _gP229PlayerScriptedPresentationEntryAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/p229_animation_0A78C_indices.inc"
};

/// Player presentation on entry to scripted events, shared by P229 native-bank sets 32 and 34.
///
/// The scripted presentation's zero selector plays set 32 before CAP event
/// playback. Child slots 1..18 play the clip; every track stops at its final
/// pose. Tracks 0 and 1 carry translation/rotation; tracks 2..18 carry rotation
/// only. The package and borrowed clip data must remain loaded during playback.
static AnimationSet _gP229PlayerScriptedPresentationEntryAnimation = {
    .records           = _gP229PlayerScriptedPresentationEntryAnimationRecords,
    .trackStartIndices = _gP229PlayerScriptedPresentationEntryAnimationIndices,
    .poseBanks         = {
        [P229_ANIMATION_TRANSLATION_ROTATION_BANK] = _gP229PlayerScriptedPresentationEntryAnimationBank1,
        [P229_ANIMATION_PACKED_ROTATION_BANK]      = _gP229PlayerScriptedPresentationEntryAnimationBank4,
    },
};

/// Translation/rotation poses for `_gP229PlayerScriptedPresentationExitAnimation`, in model units and 1/4096 turns.
static const AnimationPackedPose _gP229PlayerScriptedPresentationExitAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/p229_animation_0A964_bank1.inc"
};

/// Rotation-only poses for `_gP229PlayerScriptedPresentationExitAnimation`, packed in eight-angle-unit steps.
static const AnimationPackedRotation _gP229PlayerScriptedPresentationExitAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/p229_animation_0A964_bank4.inc"
};

/// Keyframe durations in normal-rate frames and stop controls for `_gP229PlayerScriptedPresentationExitAnimation`'s nineteen part tracks.
static const AnimationRecord _gP229PlayerScriptedPresentationExitAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/p229_animation_0A964_records.inc"
};

/// Absolute record starts for `_gP229PlayerScriptedPresentationExitAnimation`'s model-part tracks 0..18.
///
/// The final serialized halfword is retained after the nineteen track starts;
/// it is outside the playback track range.
static const u16 _gP229PlayerScriptedPresentationExitAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/p229_animation_0A964_indices.inc"
};

/// Player presentation on exit from scripted events, shared by P229 native-bank sets 33 and 35.
///
/// The scripted presentation's nonzero selector plays set 33 after CAP event
/// playback, before player control resumes. Child slots 1..18 play the clip;
/// every track stops at its final pose. Tracks 0 and 1 carry translation/rotation;
/// tracks 2..18 carry rotation only. The package must remain loaded during playback.
static AnimationSet _gP229PlayerScriptedPresentationExitAnimation = {
    .records           = _gP229PlayerScriptedPresentationExitAnimationRecords,
    .trackStartIndices = _gP229PlayerScriptedPresentationExitAnimationIndices,
    .poseBanks         = {
        [P229_ANIMATION_TRANSLATION_ROTATION_BANK] = _gP229PlayerScriptedPresentationExitAnimationBank1,
        [P229_ANIMATION_PACKED_ROTATION_BANK]      = _gP229PlayerScriptedPresentationExitAnimationBank4,
    },
};

/// Translation/rotation poses for `_gP229PlayerParasiteEnergyReleaseFamily2Animation`, in model units and 1/4096 turns.
static const AnimationPackedPose _gP229PlayerParasiteEnergyReleaseFamily2AnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/p229_animation_0AEC8_bank1.inc"
};

/// Rotation-only poses for `_gP229PlayerParasiteEnergyReleaseFamily2Animation`, packed in eight-angle-unit steps.
static const AnimationPackedRotation _gP229PlayerParasiteEnergyReleaseFamily2AnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/p229_animation_0AEC8_bank4.inc"
};

/// Keyframe durations in normal-rate frames and stop controls for `_gP229PlayerParasiteEnergyReleaseFamily2Animation`'s nineteen part tracks.
static const AnimationRecord _gP229PlayerParasiteEnergyReleaseFamily2AnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/p229_animation_0AEC8_records.inc"
};

/// Absolute record starts for `_gP229PlayerParasiteEnergyReleaseFamily2Animation`'s model-part tracks 0..18.
///
/// The final serialized halfword is retained after the nineteen track starts;
/// it is outside the playback track range.
static const u16 _gP229PlayerParasiteEnergyReleaseFamily2AnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/p229_animation_0AEC8_indices.inc"
};

/// Player Parasite Energy family-2 release gesture in P229 native-bank set 42.
///
/// Starts the normal-mode action before the family-2 charge loop, with a
/// six-frame entry blend. Spell attachment IDs below 601 whose decimal tens
/// digit is 3 select this family. Child slots 1..18 play the clip.
/// Tracks 0 and 1 carry translation/rotation; tracks 2..18 carry rotation only.
/// Every track stops at its final pose; the package must remain loaded during playback.
static AnimationSet _gP229PlayerParasiteEnergyReleaseFamily2Animation = {
    .records           = _gP229PlayerParasiteEnergyReleaseFamily2AnimationRecords,
    .trackStartIndices = _gP229PlayerParasiteEnergyReleaseFamily2AnimationIndices,
    .poseBanks         = {
        [P229_ANIMATION_TRANSLATION_ROTATION_BANK] = _gP229PlayerParasiteEnergyReleaseFamily2AnimationBank1,
        [P229_ANIMATION_PACKED_ROTATION_BANK]      = _gP229PlayerParasiteEnergyReleaseFamily2AnimationBank4,
    },
};

/// Translation/rotation poses for `_gP229PlayerParasiteEnergyChargeFamily2Animation`, in model units and 1/4096 turns.
static const AnimationPackedPose _gP229PlayerParasiteEnergyChargeFamily2AnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/p229_animation_0B0BC_bank1.inc"
};

/// Rotation-only poses for `_gP229PlayerParasiteEnergyChargeFamily2Animation`, packed in eight-angle-unit steps.
static const AnimationPackedRotation _gP229PlayerParasiteEnergyChargeFamily2AnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/p229_animation_0B0BC_bank4.inc"
};

/// Keyframe durations in normal-rate frames and loop controls for `_gP229PlayerParasiteEnergyChargeFamily2Animation`'s nineteen part tracks.
static const AnimationRecord _gP229PlayerParasiteEnergyChargeFamily2AnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/p229_animation_0B0BC_records.inc"
};

/// Absolute record starts for `_gP229PlayerParasiteEnergyChargeFamily2Animation`'s model-part tracks 0..18.
///
/// The final serialized halfword is retained after the nineteen track starts;
/// it is outside the playback track range.
static const u16 _gP229PlayerParasiteEnergyChargeFamily2AnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/p229_animation_0B0BC_indices.inc"
};

/// Player Parasite Energy family-2 charge loop in P229 native-bank set 43.
///
/// Follows the family-2 release gesture and repeats while the attachment's
/// countdown remains nonzero; reaching zero starts the cast clip. Spell
/// attachment IDs below 601 whose decimal tens digit is 3 select this family.
/// Child slots 1..18 play the loop. Tracks 0 and 1 carry translation/rotation;
/// tracks 2..18 carry rotation only. All tracks loop back to their start.
/// The package and borrowed clip data must remain loaded throughout playback.
static AnimationSet _gP229PlayerParasiteEnergyChargeFamily2Animation = {
    .records           = _gP229PlayerParasiteEnergyChargeFamily2AnimationRecords,
    .trackStartIndices = _gP229PlayerParasiteEnergyChargeFamily2AnimationIndices,
    .poseBanks         = {
        [P229_ANIMATION_TRANSLATION_ROTATION_BANK] = _gP229PlayerParasiteEnergyChargeFamily2AnimationBank1,
        [P229_ANIMATION_PACKED_ROTATION_BANK]      = _gP229PlayerParasiteEnergyChargeFamily2AnimationBank4,
    },
};

/// Translation/rotation poses for `_gP229PlayerParasiteEnergyCastFamily2Animation`, in model units and 1/4096 turns.
static const AnimationPackedPose _gP229PlayerParasiteEnergyCastFamily2AnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/p229_animation_0B53C_bank1.inc"
};

/// Rotation-only poses for `_gP229PlayerParasiteEnergyCastFamily2Animation`, packed in eight-angle-unit steps.
static const AnimationPackedRotation _gP229PlayerParasiteEnergyCastFamily2AnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/p229_animation_0B53C_bank4.inc"
};

/// Keyframe durations in normal-rate frames and stop controls for `_gP229PlayerParasiteEnergyCastFamily2Animation`'s nineteen part tracks.
static const AnimationRecord _gP229PlayerParasiteEnergyCastFamily2AnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/p229_animation_0B53C_records.inc"
};

/// Absolute record starts for `_gP229PlayerParasiteEnergyCastFamily2Animation`'s model-part tracks 0..18.
///
/// The final serialized halfword is retained after the nineteen track starts;
/// it is outside the playback track range.
static const u16 _gP229PlayerParasiteEnergyCastFamily2AnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/p229_animation_0B53C_indices.inc"
};

/// Player Parasite Energy family-2 cast gesture in P229 native-bank set 44.
///
/// Starts when the family-2 charge countdown reaches zero, before returning
/// to locomotion or aiming. Spell attachment IDs below 601 whose decimal tens
/// digit is 3 select this family. Child slots 1..18 play the clip.
/// Tracks 0 and 1 carry translation/rotation; tracks 2..18 carry rotation only.
/// Every track stops at its final pose; the package must remain loaded during playback.
static AnimationSet _gP229PlayerParasiteEnergyCastFamily2Animation = {
    .records           = _gP229PlayerParasiteEnergyCastFamily2AnimationRecords,
    .trackStartIndices = _gP229PlayerParasiteEnergyCastFamily2AnimationIndices,
    .poseBanks         = {
        [P229_ANIMATION_TRANSLATION_ROTATION_BANK] = _gP229PlayerParasiteEnergyCastFamily2AnimationBank1,
        [P229_ANIMATION_PACKED_ROTATION_BANK]      = _gP229PlayerParasiteEnergyCastFamily2AnimationBank4,
    },
};

/// Translation/rotation poses for `_gP229PlayerAimHoldAnimation`, in model units and 1/4096 turns.
static const AnimationPackedPose _gP229PlayerAimHoldAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/p229_animation_0B718_bank1.inc"
};

/// Rotation-only poses for `_gP229PlayerAimHoldAnimation`, packed in eight-angle-unit steps.
static const AnimationPackedRotation _gP229PlayerAimHoldAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/p229_animation_0B718_bank4.inc"
};

/// Repeated keyframes and loop controls for `_gP229PlayerAimHoldAnimation`'s nineteen part tracks.
///
/// Each track repeats one pose through two one-frame keyframes and a jump to
/// its first record. Forward playback never reaches the serialized stop after
/// each jump.
static const AnimationRecord _gP229PlayerAimHoldAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/p229_animation_0B718_records.inc"
};

/// Absolute record starts for `_gP229PlayerAimHoldAnimation`'s model-part tracks 0..18.
///
/// The final serialized halfword is retained after the nineteen track starts;
/// it is outside the playback track range.
static const u16 _gP229PlayerAimHoldAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/p229_animation_0B718_indices.inc"
};

/// Stationary aiming pose of the P229 player, set index 9.
///
/// Aim entry settles into this pose, and aim locomotion selects it while
/// movement and turning are stopped. Attacks also blend into it before firing.
/// Every track loops at a fixed pose. Tracks 0 and 1 carry translation/rotation;
/// tracks 2..18 carry rotation only. Child slots 1..18 play the clip, and the
/// package must remain loaded while any playback slot borrows its data.
static AnimationSet _gP229PlayerAimHoldAnimation = {
    .records           = _gP229PlayerAimHoldAnimationRecords,
    .trackStartIndices = _gP229PlayerAimHoldAnimationIndices,
    .poseBanks         = {
        [P229_ANIMATION_TRANSLATION_ROTATION_BANK] = _gP229PlayerAimHoldAnimationBank1,
        [P229_ANIMATION_PACKED_ROTATION_BANK]      = _gP229PlayerAimHoldAnimationBank4,
    },
};

/// Translation/rotation poses for `_gP229PlayerAimBackwardWalkAnimation`, in model units and 1/4096 turns.
static const AnimationPackedPose _gP229PlayerAimBackwardWalkAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/p229_animation_0BE08_bank1.inc"
};

/// Rotation-only poses for `_gP229PlayerAimBackwardWalkAnimation`, packed in eight-angle-unit steps.
static const AnimationPackedRotation _gP229PlayerAimBackwardWalkAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/p229_animation_0BE08_bank4.inc"
};

/// Keyframe durations in normal-rate frames and loop controls for `_gP229PlayerAimBackwardWalkAnimation`.
///
/// Track 1 carries alternating footstep cues; every track loops within its own
/// record span. The serialized stops following the loop jumps are retained.
static const AnimationRecord _gP229PlayerAimBackwardWalkAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/p229_animation_0BE08_records.inc"
};

/// Absolute record starts for `_gP229PlayerAimBackwardWalkAnimation`'s model-part tracks 0..18.
///
/// The final serialized halfword is retained after the nineteen track starts;
/// it is outside the playback track range.
static const u16 _gP229PlayerAimBackwardWalkAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/p229_animation_0BE08_indices.inc"
};

/// Backward aiming walk of the P229 player, also used for turning in place, set index 13.
///
/// Aim locomotion and reload cancellation select this clip for backward input
/// or turning without movement. Child slots 1..18 play the looping clip.
/// Tracks 0 and 1 carry translation/rotation; tracks 2..18 carry rotation only.
/// The package must remain loaded while any playback slot borrows its data.
static AnimationSet _gP229PlayerAimBackwardWalkAnimation = {
    .records           = _gP229PlayerAimBackwardWalkAnimationRecords,
    .trackStartIndices = _gP229PlayerAimBackwardWalkAnimationIndices,
    .poseBanks         = {
        [P229_ANIMATION_TRANSLATION_ROTATION_BANK] = _gP229PlayerAimBackwardWalkAnimationBank1,
        [P229_ANIMATION_PACKED_ROTATION_BANK]      = _gP229PlayerAimBackwardWalkAnimationBank4,
    },
};

/// Translation/rotation poses for `_gP229PlayerAimForwardWalkAnimation`, in model units and 1/4096 turns.
static const AnimationPackedPose _gP229PlayerAimForwardWalkAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/p229_animation_0C9F4_bank1.inc"
};

/// Rotation-only poses for `_gP229PlayerAimForwardWalkAnimation`, packed in eight-angle-unit steps.
static const AnimationPackedRotation _gP229PlayerAimForwardWalkAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/p229_animation_0C9F4_bank4.inc"
};

/// Keyframe durations in normal-rate frames and loop controls for `_gP229PlayerAimForwardWalkAnimation`.
///
/// Track 1 carries alternating footstep cues; every track loops within its own
/// record span. The serialized stops following the loop jumps are retained.
static const AnimationRecord _gP229PlayerAimForwardWalkAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/p229_animation_0C9F4_records.inc"
};

/// Absolute record starts for `_gP229PlayerAimForwardWalkAnimation`'s model-part tracks 0..18.
///
/// The final serialized halfword is retained after the nineteen track starts;
/// it is outside the playback track range.
static const u16 _gP229PlayerAimForwardWalkAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/p229_animation_0C9F4_indices.inc"
};

/// Forward aiming walk of the P229 player, set index 12.
///
/// Aim locomotion and reload cancellation select this clip for forward input.
/// Child slots 1..18 play the looping clip. Tracks 0 and 1 carry
/// translation/rotation; tracks 2..18 carry rotation only. The package must
/// remain loaded while any playback slot borrows its data.
static AnimationSet _gP229PlayerAimForwardWalkAnimation = {
    .records           = _gP229PlayerAimForwardWalkAnimationRecords,
    .trackStartIndices = _gP229PlayerAimForwardWalkAnimationIndices,
    .poseBanks         = {
        [P229_ANIMATION_TRANSLATION_ROTATION_BANK] = _gP229PlayerAimForwardWalkAnimationBank1,
        [P229_ANIMATION_PACKED_ROTATION_BANK]      = _gP229PlayerAimForwardWalkAnimationBank4,
    },
};

/// Translation/rotation poses for `_gP229PlayerReloadAnimation`, in model units and 1/4096 turns.
static const AnimationPackedPose _gP229PlayerReloadAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/p229_animation_0D31C_bank1.inc"
};

/// Rotation-only poses for `_gP229PlayerReloadAnimation`, packed in eight-angle-unit steps.
static const AnimationPackedRotation _gP229PlayerReloadAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/p229_animation_0D31C_bank4.inc"
};

/// Keyframe durations in normal-rate frames and stop controls for `_gP229PlayerReloadAnimation`.
///
/// Track 1's combined cue bits mark the load sound and ammunition transfer.
/// Every track stops at its final pose.
static const AnimationRecord _gP229PlayerReloadAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/p229_animation_0D31C_records.inc"
};

/// Absolute record starts for `_gP229PlayerReloadAnimation`'s model-part tracks 0..18.
///
/// The final serialized halfword is retained after the nineteen track starts;
/// it is outside the playback track range.
static const u16 _gP229PlayerReloadAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/p229_animation_0D31C_indices.inc"
};

/// P229 player reload shared by primary, secondary and battle-end set indices 14, 15 and 20.
///
/// Child slots 1..18 play the clip; its load cue applies the selected ammunition.
/// Every track stops at its final pose. Tracks 0 and 1 carry translation/rotation;
/// tracks 2..18 carry rotation only. The package must remain loaded while any
/// playback slot borrows its data.
static AnimationSet _gP229PlayerReloadAnimation = {
    .records           = _gP229PlayerReloadAnimationRecords,
    .trackStartIndices = _gP229PlayerReloadAnimationIndices,
    .poseBanks         = {
        [P229_ANIMATION_TRANSLATION_ROTATION_BANK] = _gP229PlayerReloadAnimationBank1,
        [P229_ANIMATION_PACKED_ROTATION_BANK]      = _gP229PlayerReloadAnimationBank4,
    },
};

/// Translation/rotation poses for `_gP229PlayerFireAnimation`, in model units and 1/4096 turns.
static const AnimationPackedPose _gP229PlayerFireAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/p229_animation_0D6EC_bank1.inc"
};

/// Rotation-only poses for `_gP229PlayerFireAnimation`, packed in eight-angle-unit steps.
static const AnimationPackedRotation _gP229PlayerFireAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/p229_animation_0D6EC_bank4.inc"
};

/// Keyframe durations in normal-rate frames and stop controls for `_gP229PlayerFireAnimation`.
///
/// Every track stops at its final pose; no record carries an action cue.
static const AnimationRecord _gP229PlayerFireAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/p229_animation_0D6EC_records.inc"
};

/// Absolute record starts for `_gP229PlayerFireAnimation`'s model-part tracks 0..18.
///
/// The final serialized halfword is retained after the nineteen track starts;
/// it is outside the playback track range.
static const u16 _gP229PlayerFireAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/p229_animation_0D6EC_indices.inc"
};

/// P229 player firing clip shared by set indices 10 and 11.
///
/// Primary fire restarts this clip after the ready pose. The secondary attack
/// retains its ready pose. Child slots 1..18 play the clip, and every track
/// stops at its final pose. Tracks 0 and 1 carry translation/rotation;
/// tracks 2..18 carry rotation only. The package must remain loaded while any
/// playback slot borrows its data.
static AnimationSet _gP229PlayerFireAnimation = {
    .records           = _gP229PlayerFireAnimationRecords,
    .trackStartIndices = _gP229PlayerFireAnimationIndices,
    .poseBanks         = {
        [P229_ANIMATION_TRANSLATION_ROTATION_BANK] = _gP229PlayerFireAnimationBank1,
        [P229_ANIMATION_PACKED_ROTATION_BANK]      = _gP229PlayerFireAnimationBank4,
    },
};

/// Translation/rotation poses for `_gP229PlayerAimEntryAnimation`, in model units and 1/4096 turns.
static const AnimationPackedPose _gP229PlayerAimEntryAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/p229_animation_0DD68_bank1.inc"
};

/// Rotation-only poses for `_gP229PlayerAimEntryAnimation`, packed in eight-angle-unit steps.
static const AnimationPackedRotation _gP229PlayerAimEntryAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/p229_animation_0DD68_bank4.inc"
};

/// Keyframe durations in normal-rate frames and stop controls for `_gP229PlayerAimEntryAnimation`.
///
/// Every track stops at its final pose; no record carries an action cue.
static const AnimationRecord _gP229PlayerAimEntryAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/p229_animation_0DD68_records.inc"
};

/// Absolute record starts for `_gP229PlayerAimEntryAnimation`'s model-part tracks 0..18.
///
/// The final serialized halfword is retained after the nineteen track starts;
/// it is outside the playback track range.
static const u16 _gP229PlayerAimEntryAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/p229_animation_0DD68_indices.inc"
};

/// Transition into the P229 player's aiming pose, set index 7.
///
/// Aim entry plays this clip before blending into `_gP229PlayerAimHoldAnimation`.
/// Child slots 1..18 play the clip, and every track stops at its final pose.
/// Tracks 0 and 1 carry translation/rotation; tracks 2..18 carry rotation only.
/// The package must remain loaded while any playback slot borrows its data.
static AnimationSet _gP229PlayerAimEntryAnimation = {
    .records           = _gP229PlayerAimEntryAnimationRecords,
    .trackStartIndices = _gP229PlayerAimEntryAnimationIndices,
    .poseBanks         = {
        [P229_ANIMATION_TRANSLATION_ROTATION_BANK] = _gP229PlayerAimEntryAnimationBank1,
        [P229_ANIMATION_PACKED_ROTATION_BANK]      = _gP229PlayerAimEntryAnimationBank4,
    },
};

/// Aim-exit translations in model units and rotations in 1/4096 turns for tracks 0..1.
static const AnimationPackedPose _gP229PlayerAimExitAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/p229_animation_0E334_bank1.inc"
};

/// Aim-exit rotations for tracks 2..18, packed in eight-unit angular steps.
static const AnimationPackedRotation _gP229PlayerAimExitAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/p229_animation_0E334_bank4.inc"
};

/// Aim-exit keyframes and stop controls, without action cues.
///
/// Durations count normal-rate frames and pose offsets count four-byte words.
static const AnimationRecord _gP229PlayerAimExitAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/p229_animation_0E334_records.inc"
};

/// Absolute aim-exit record starts for tracks 0..18, followed by a retained serialized halfword.
static const u16 _gP229PlayerAimExitAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/p229_animation_0E334_indices.inc"
};
STATIC_ASSERT(ARRAY_SIZE(_gP229PlayerAimExitAnimationIndices) == GAME_ACTOR_NORMAL_ANIMATION_SLOTS + 1, p229_aim_exit_track_starts);

/// Player transition out of P229 aiming, at native clip id 8.
///
/// Normal and scripted attack exits play child tracks 1..18; every track stops.
/// Tracks 0..1 carry translation/rotation and 2..18 rotation only. Keep the
/// P229 package loaded while any slot or cached record refers to this clip.
static AnimationSet _gP229PlayerAimExitAnimation = {
    .records           = _gP229PlayerAimExitAnimationRecords,
    .trackStartIndices = _gP229PlayerAimExitAnimationIndices,
    .poseBanks         = {
        [P229_ANIMATION_TRANSLATION_ROTATION_BANK] = _gP229PlayerAimExitAnimationBank1,
        [P229_ANIMATION_PACKED_ROTATION_BANK]      = _gP229PlayerAimExitAnimationBank4,
    },
};

AnimationBank gP229PlayerAnimationBank = {
    .table = {
        .sets = {
            NULL,
            &_gP229PlayerIdleAnimation,
            &_gP229PlayerWalkAnimation,
            &_gP229PlayerBackwardWalkAnimation,
            &_gP229PlayerRunAnimation,
            &_gP229PlayerTurnLeftAnimation,
            &_gP229PlayerTurnRightAnimation,
            &_gP229PlayerAimEntryAnimation,
            &_gP229PlayerAimExitAnimation,
            &_gP229PlayerAimHoldAnimation,
            &_gP229PlayerFireAnimation,
            &_gP229PlayerFireAnimation,
            &_gP229PlayerAimForwardWalkAnimation,
            &_gP229PlayerAimBackwardWalkAnimation,
            &_gP229PlayerReloadAnimation,
            &_gP229PlayerReloadAnimation,
            &_gP229PlayerPart4HitReactionAnimation,
            &_gP229PlayerOtherBodyHitReactionAnimation,
            &_gP229PlayerDeathPoseAnimation,
            &_gP229PlayerUnarmedWalkAnimation,
            &_gP229PlayerReloadAnimation,
            &_gP229PlayerIdleAnimation,
            &_gP229PlayerIdleAnimation,
            &_gP229PlayerHighHealthIdleAnimation,
            &_gP229PlayerMidHealthIdleAnimation,
            &_gP229PlayerLowHealthIdleAnimation,
            &_gP229PlayerParasiteEnergyReleaseFamily0Animation,
            &_gP229PlayerParasiteEnergyChargeFamily0Animation,
            &_gP229PlayerParasiteEnergyCastFamily0Animation,
            &_gP229PlayerParasiteEnergyReleaseFamily1Animation,
            &_gP229PlayerParasiteEnergyChargeFamily1Animation,
            &_gP229PlayerParasiteEnergyCastFamily1Animation,
            &_gP229PlayerScriptedPresentationEntryAnimation,
            &_gP229PlayerScriptedPresentationExitAnimation,
            &_gP229PlayerScriptedPresentationEntryAnimation,
            &_gP229PlayerScriptedPresentationExitAnimation,
            &_gP229PlayerStairAscentAnimation,
            &_gP229PlayerStairDescentAnimation,
            &_gP229PlayerStairAscentFinishEvenAnimation,
            &_gP229PlayerStairAscentFinishOddAnimation,
            &_gP229PlayerItemUseAnimation,
            &_gP229PlayerIdleAnimation,
            &_gP229PlayerParasiteEnergyReleaseFamily2Animation,
            &_gP229PlayerParasiteEnergyChargeFamily2Animation,
            &_gP229PlayerParasiteEnergyCastFamily2Animation,
            // Unused base slots and all raw extension words start cleared.
        },
    },
};

/// Retained streak directions shared by every live flash in this weapon package.
///
/// Angles use 4096 units per turn, one random direction in each quadrant.
/// Each newly initialized flash replaces the directions older flashes read.
static s16 _gMuzzleFlashAngles[MUZZLE_FLASH_STREAK_COUNT] = { 0, 0, 0, 0 };
