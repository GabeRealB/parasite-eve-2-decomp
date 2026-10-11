#include "weapons/m950.h"

#include <psyq/sys/types.h>
#include <psyq/libgpu.h>

#include "common.h"

#include "gameplay/animation.h"
#include "gameplay/items.h"
#include "gameplay/player_actor.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/scratch.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task_types.h"

#include "weapons/weapon.h"

/// Encoding-indexed `AnimationSet.poseBanks` slots used by the M950 player clips.
enum {
    M950_ANIMATION_TRANSLATION_ROTATION_BANK = 1,
    M950_ANIMATION_PACKED_ROTATION_BANK      = 4,
};

void m950AttackState(Task* playerTask)
{
    enum {
        M950_PHASE_PREPARE             = 0,
        M950_PHASE_WAIT_READY          = 1,
        M950_PHASE_ARM_SHOT            = 2,
        M950_PHASE_SHOT_DELAY          = 3,
        M950_PHASE_IMPACT              = 4,
        M950_PHASE_RECOVER             = 5,
        M950_PLAYER_ATTACK_STATE       = 4,
        M950_ANIMATION_READY           = 9,
        M950_ANIMATION_PRIMARY         = 0xA,
        M950_READY_BLEND_FRAMES        = 1,
        M950_MOVING_READY_BLEND_FRAMES = 5,
        M950_IMPACT_SOUND              = SOUND_COMMON(0x17),
        M950_SHOT_DELAY_FRAMES         = 4,
        M950_CANCEL_FRAMES             = 9,
        M950_WEAPON_ID                 = 3,
        M950_FIRE_SOUND                = SOUND_WEAPON(3, 4),
    };
    GameActor* actor;
    GfxCoord*  weaponCoord;
    GfxCoord*  impactCoord;
    s32        readyBlendFrames;

    impactCoord = SCRATCH_STACK_RESERVE_BLOCK(GfxCoord);
    actor       = playerTask->work;
    weaponCoord = actor->equipmentTasks[1]->extra.tmd->coords;
    switch (actor->statePhase) {
        case M950_PHASE_PREPARE:
            actor->state                                          = M950_PLAYER_ATTACK_STATE;
            actor->turnRateIndex                                  = 2;
            actor->mode                                           = GAME_ACTOR_MODE_NORMAL;
            actor->animationState                                 = 0;
            actor->statePhase                                    += 1;
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= (WORLD_COLLISION_BODY_CLIP_TO_GRID_CONTACT | WORLD_COLLISION_BODY_SINGLE_CONTACT);
            playerActorSetWeaponAttackFlags(playerTask, 0, 1);
            readyBlendFrames = M950_READY_BLEND_FRAMES;
            if (((u16)actor->movementMode | actor->turnSign) != 0) {
                readyBlendFrames = M950_MOVING_READY_BLEND_FRAMES;
            }
            playerActorPlayChildSlotsWithBlend(playerTask, M950_ANIMATION_READY, 0, readyBlendFrames);
            actor->movementMode = 0;
            break;
        case M950_PHASE_WAIT_READY:
            if (animationGetCurrentRecord(&actor->animationContext, actor->animationSlots + 1) !=
                NULL) {
                actor->statePhase++;
            }
            break;
        case M950_PHASE_ARM_SHOT:
        fire:
            // Rearming falls through so this dispatch counts toward the four-tick delay.
            actor->statePhase                  = M950_PHASE_SHOT_DELAY;
            actor->attackControl.cooldownTicks = 0;
            actor->rumblePosted                = 0;
            actor->stateTimer                  = M950_SHOT_DELAY_FRAMES;
            /* fallthrough */
        case M950_PHASE_SHOT_DELAY:
            if (--actor->stateTimer == 0) {
                actor->statePhase++;
                actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
                equipmentConsumeWeaponLoad(WEAPON_ITEM(M950_WEAPON_ID), EQUIPMENT_WEAPON_LOAD_CONSUME_PRIMARY);
                worldCoordPlaySound(playerTask->extra.tmd->coords, M950_FIRE_SOUND, 1);
                effectSpawn(EFFECT_HANDGUN_MUZZLE_FLASH, weaponCoord, 3, NULL);
                playerActorPlayChildSlotsWithBlend(playerTask, M950_ANIMATION_PRIMARY, 0, 2);
            }
            break;
        case M950_PHASE_IMPACT:
            actor->attackCancelTicks = M950_CANCEL_FRAMES;
            actor->statePhase++;
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
            if (playerActorSpawnWeaponImpact(actor->weaponContacts, weaponCoord, impactCoord) != 0) {
                worldCoordPlaySound(impactCoord, M950_IMPACT_SOUND, 1);
            }
            /* fallthrough */
        case M950_PHASE_RECOVER:
            if (playerActorReadAttackButton(playerTask) != 0 && playerActorQueryWeaponLoads(PLAYER_ACTOR_WEAPON_LOAD_PRIMARY) > 0) {
                goto fire;
            }
            if (actor->attackCancelTicks != 0) {
                actor->attackCancelTicks--;
            }
            if (playerActorIsSlotAdvancingLinearly(playerTask, D_80112E04[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId][1], 0, 0) == 0 ||
                ((actor->padHeld & actor->actionPadMask) != 0 && actor->attackCancelTicks == 0)) {
                playerActorFinishWeaponAttack(playerTask);
            }
            break;
    }
    playerActorTrackLockTarget(playerTask);
    SCRATCH_STACK_RELEASE_BLOCK(GfxCoord);
}

/// Initial identity transform and self-parented root of the rigid M950 model.
///
/// Read-only source tables retain their serialized positions in .data.
static const TmdBone _gM950WeaponModelSkeleton[] __attribute__((section(".data"))) = {
#include "assets/m950_model_00548_skeleton.inc"
};

/// Vertex count of the weapon's sole skeletal part: all 36 vertices.
static const u32 _gM950WeaponModelPartVerts[ARRAY_SIZE(_gM950WeaponModelSkeleton)] __attribute__((section(".data"))) = {
#include "assets/m950_model_00548_partVerts.inc"
};

/// Writable M950 vertices in integer coordinates local to the weapon root.
static SVECTOR _gM950WeaponModelVerts[] = {
#include "assets/m950_model_00548_verts.inc"
};

/// Writable M950 lighting normals in part-local axes, with twelve fractional bits.
static SVECTOR _gM950WeaponModelNormals[] = {
#include "assets/m950_model_00548_normals.inc"
};

/// Writable commands for four textured triangles and 28 textured quads.
///
/// Handler slots resolve in place. The skeletal group and empty final group
/// each end with `TMD_STREAM_GROUP_END`; `TMD_STREAM_END` closes the stream.
static u32 _gM950WeaponModelStream[] = {
#include "assets/m950_model_00548_stream.inc"
};

TmdSource gM950WeaponModel = {
    .handlersResolved    = false,
    .bufferHalfBytes     = (4 * sizeof(POLY_GT3)) + (28 * sizeof(POLY_GT4)),
    .preXformRegionBytes = 0,
    .partCount           = ARRAY_SIZE(_gM950WeaponModelSkeleton),
    .partVertexCounts    = _gM950WeaponModelPartVerts,
    .verts               = _gM950WeaponModelVerts,
    .normals             = _gM950WeaponModelNormals,
    .skeleton            = _gM950WeaponModelSkeleton,
    .stream              = _gM950WeaponModelStream,
};

/// Root and body translation/rotation keys for the M950 player's idle pose.
///
/// Model-unit translations and 1/4096-turn angles; read-only banks remain in .data.
static const AnimationPackedPose _gM950PlayerIdleAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_00A90_bank1.inc"
};

/// Packed joint rotations for the M950 player's idle pose, in eight-angle-unit steps.
static const AnimationPackedRotation _gM950PlayerIdleAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_00A90_bank4.inc"
};

/// Keyframes and loop/stop controls holding the M950 player's idle pose.
///
/// Pose offsets count u32 words, jump targets count records and durations count normal-rate frames.
static const AnimationRecord _gM950PlayerIdleAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_00A90_records.inc"
};

/// Record starts for the idle pose's nineteen player model-part tracks.
///
/// Tracks 0/1 use bank 1 and tracks 2..18 use bank 4. Retains the resource's
/// twentieth zero halfword; ordinary child playback uses tracks 1..18.
static const u16 _gM950PlayerIdleAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_00A90_indices.inc"
};

/// Stationary idle pose in the M950 player's native animation bank.
///
/// Bank ids 1, 21, 22 and 41 share this nineteen-track pose. The descriptor and
/// its read-only records and banks must remain loaded throughout playback.
static AnimationSet _gM950PlayerIdleAnimation = {
    .records           = _gM950PlayerIdleAnimationRecords,
    .trackStartIndices = _gM950PlayerIdleAnimationIndices,
    .poseBanks         = { NULL, _gM950PlayerIdleAnimationBank1, NULL, NULL, _gM950PlayerIdleAnimationBank4, NULL, NULL, NULL },
};

/// Root and body translation/rotation keys for the M950 bank's unarmed forward walk.
///
/// Model-unit translations and 1/4096-turn angles; read-only banks remain in .data.
static const AnimationPackedPose _gM950PlayerUnarmedWalkAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_01134_bank1.inc"
};

/// Packed joint rotations for the unarmed forward walk, in eight-angle-unit steps.
static const AnimationPackedRotation _gM950PlayerUnarmedWalkAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_01134_bank4.inc"
};

/// Keyframes and loop/stop controls for the M950 bank's unarmed forward walk.
///
/// Pose offsets count u32 words, jump targets count records and durations count normal-rate frames.
static const AnimationRecord _gM950PlayerUnarmedWalkAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_01134_records.inc"
};

/// Record starts for the unarmed forward walk's nineteen player model-part tracks.
///
/// Tracks 0/1 use bank 1 and tracks 2..18 use bank 4. Retains the resource's
/// twentieth zero halfword; ordinary child playback uses tracks 1..18.
static const u16 _gM950PlayerUnarmedWalkAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_01134_indices.inc"
};

/// Unarmed forward-walk clip at id 19 in the M950 player's native animation bank.
///
/// Selected while the weapon child is absent. Its nineteen player model-part
/// tracks and all borrowed clip data must remain loaded throughout playback.
static AnimationSet _gM950PlayerUnarmedWalkAnimation = {
    .records           = _gM950PlayerUnarmedWalkAnimationRecords,
    .trackStartIndices = _gM950PlayerUnarmedWalkAnimationIndices,
    .poseBanks         = { NULL, _gM950PlayerUnarmedWalkAnimationBank1, NULL, NULL, _gM950PlayerUnarmedWalkAnimationBank4, NULL, NULL, NULL },
};

/// Root and body translation/rotation keys for forward walking while holding the M950.
///
/// Model-unit translations and 1/4096-turn angles; read-only banks remain in .data.
static const AnimationPackedPose _gM950PlayerWalkAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_01A80_bank1.inc"
};

/// Packed joint rotations for the M950 forward walk, in eight-angle-unit steps.
static const AnimationPackedRotation _gM950PlayerWalkAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_01A80_bank4.inc"
};

/// Keyframes and loop/stop controls for forward walking while holding the M950.
///
/// Pose offsets count u32 words, jump targets count records and durations count normal-rate frames.
static const AnimationRecord _gM950PlayerWalkAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_01A80_records.inc"
};

/// Record starts for the armed forward walk's nineteen player model-part tracks.
///
/// Tracks 0/1 use bank 1 and tracks 2..18 use bank 4. Retains the resource's
/// twentieth zero halfword; ordinary child playback uses tracks 1..18.
static const u16 _gM950PlayerWalkAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_01A80_indices.inc"
};

/// Armed forward-walk clip at id 2 in the M950 player's native animation bank.
///
/// Its nineteen player model-part tracks and all borrowed clip data must remain
/// loaded throughout playback.
static AnimationSet _gM950PlayerWalkAnimation = {
    .records           = _gM950PlayerWalkAnimationRecords,
    .trackStartIndices = _gM950PlayerWalkAnimationIndices,
    .poseBanks         = { NULL, _gM950PlayerWalkAnimationBank1, NULL, NULL, _gM950PlayerWalkAnimationBank4, NULL, NULL, NULL },
};

/// Root and body translation/rotation keys for backward walking in the M950 bank.
///
/// Model-unit translations and 1/4096-turn angles; read-only banks remain in .data.
static const AnimationPackedPose _gM950PlayerBackwardWalkAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_02358_bank1.inc"
};

/// Packed joint rotations for the backward walk, in eight-angle-unit steps.
static const AnimationPackedRotation _gM950PlayerBackwardWalkAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_02358_bank4.inc"
};

/// Keyframes and loop/stop controls for backward walking in the M950 bank.
///
/// Pose offsets count u32 words, jump targets count records and durations count normal-rate frames.
static const AnimationRecord _gM950PlayerBackwardWalkAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_02358_records.inc"
};

/// Record starts for the backward walk's nineteen player model-part tracks.
///
/// Tracks 0/1 use bank 1 and tracks 2..18 use bank 4. Retains the resource's
/// twentieth zero halfword; ordinary child playback uses tracks 1..18.
static const u16 _gM950PlayerBackwardWalkAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_02358_indices.inc"
};

/// Backward-walk clip at id 3 in the M950 player's native animation bank.
///
/// Its nineteen player model-part tracks and all borrowed clip data must remain
/// loaded throughout playback.
static AnimationSet _gM950PlayerBackwardWalkAnimation = {
    .records           = _gM950PlayerBackwardWalkAnimationRecords,
    .trackStartIndices = _gM950PlayerBackwardWalkAnimationIndices,
    .poseBanks         = { NULL, _gM950PlayerBackwardWalkAnimationBank1, NULL, NULL, _gM950PlayerBackwardWalkAnimationBank4, NULL, NULL, NULL },
};

/// Root and body translation/rotation keys for running in the M950 bank.
///
/// Model-unit translations and 1/4096-turn angles; read-only banks remain in .data.
static const AnimationPackedPose _gM950PlayerRunAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_02A8C_bank1.inc"
};

/// Packed joint rotations for the run, in eight-angle-unit steps.
static const AnimationPackedRotation _gM950PlayerRunAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_02A8C_bank4.inc"
};

/// Keyframes and loop/stop controls for running in the M950 bank.
///
/// Pose offsets count u32 words, jump targets count records and durations count normal-rate frames.
static const AnimationRecord _gM950PlayerRunAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_02A8C_records.inc"
};

/// Record starts for the run's nineteen player model-part tracks.
///
/// Tracks 0/1 use bank 1 and tracks 2..18 use bank 4. Retains the resource's
/// twentieth zero halfword; ordinary child playback uses tracks 1..18.
static const u16 _gM950PlayerRunAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_02A8C_indices.inc"
};

/// Run clip at id 4 in the M950 player's native animation bank.
///
/// Its nineteen player model-part tracks and all borrowed clip data must remain
/// loaded throughout playback.
static AnimationSet _gM950PlayerRunAnimation = {
    .records           = _gM950PlayerRunAnimationRecords,
    .trackStartIndices = _gM950PlayerRunAnimationIndices,
    .poseBanks         = { NULL, _gM950PlayerRunAnimationBank1, NULL, NULL, _gM950PlayerRunAnimationBank4, NULL, NULL, NULL },
};

/// Left-turn translation/rotation keys for player tracks 0 and 1 in the M950 bank.
///
/// Translations use model units and Euler angles use 1/4096 turns. Read-only
/// clip tables retain their serialized positions in .data.
static const AnimationPackedPose _gM950PlayerTurnLeftAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_032EC_bank1.inc"
};

/// Left-turn joint rotations for player tracks 2..18, in eight-angle-unit steps.
static const AnimationPackedRotation _gM950PlayerTurnLeftAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_032EC_bank4.inc"
};

/// Left-turn keyframes and loop/stop controls for the M950 player's nineteen tracks.
///
/// Pose offsets count u32 words, jump targets count records and durations count normal-rate frames.
static const AnimationRecord _gM950PlayerTurnLeftAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_032EC_records.inc"
};

/// Absolute left-turn record starts for player tracks 0..18, followed by a serialized zero halfword.
///
/// Tracks 0/1 use bank 1 and tracks 2..18 use bank 4; child playback uses tracks 1..18.
static const u16 _gM950PlayerTurnLeftAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_032EC_indices.inc"
};

/// Stationary left-turn clip at id 5 in the M950 player's native animation bank.
///
/// Also used for scripted turns with a negative yaw delta. All nineteen tracks
/// loop; the M950 package must remain loaded while playback borrows the clip.
static AnimationSet _gM950PlayerTurnLeftAnimation = {
    .records           = _gM950PlayerTurnLeftAnimationRecords,
    .trackStartIndices = _gM950PlayerTurnLeftAnimationIndices,
    .poseBanks         = { [1] = _gM950PlayerTurnLeftAnimationBank1, [4] = _gM950PlayerTurnLeftAnimationBank4 },
};

/// Right-turn translation/rotation keys for player tracks 0 and 1 in the M950 bank.
///
/// Translations use model units and Euler angles use 1/4096 turns. Read-only
/// clip tables retain their serialized positions in .data.
static const AnimationPackedPose _gM950PlayerTurnRightAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_03B50_bank1.inc"
};

/// Right-turn joint rotations for player tracks 2..18, in eight-angle-unit steps.
static const AnimationPackedRotation _gM950PlayerTurnRightAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_03B50_bank4.inc"
};

/// Right-turn keyframes and loop/stop controls for the M950 player's nineteen tracks.
///
/// Pose offsets count u32 words, jump targets count records and durations count normal-rate frames.
static const AnimationRecord _gM950PlayerTurnRightAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_03B50_records.inc"
};

/// Absolute right-turn record starts for player tracks 0..18, followed by a serialized zero halfword.
///
/// Tracks 0/1 use bank 1 and tracks 2..18 use bank 4; child playback uses tracks 1..18.
static const u16 _gM950PlayerTurnRightAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_03B50_indices.inc"
};

/// Stationary right-turn clip at id 6 in the M950 player's native animation bank.
///
/// Also used for scripted turns with a nonnegative yaw delta. All nineteen tracks
/// loop; the M950 package must remain loaded while playback borrows the clip.
static AnimationSet _gM950PlayerTurnRightAnimation = {
    .records           = _gM950PlayerTurnRightAnimationRecords,
    .trackStartIndices = _gM950PlayerTurnRightAnimationIndices,
    .poseBanks         = { [1] = _gM950PlayerTurnRightAnimationBank1, [4] = _gM950PlayerTurnRightAnimationBank4 },
};

/// Item-use translation/rotation keys for player tracks 0 and 1 in the M950 bank.
///
/// Translations use model units and Euler angles use 1/4096 turns. Read-only
/// clip tables retain their serialized positions in .data.
static const AnimationPackedPose _gM950PlayerItemUseAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_03E64_bank1.inc"
};

/// Item-use joint rotations for player tracks 2..18, in eight-angle-unit steps.
static const AnimationPackedRotation _gM950PlayerItemUseAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_03E64_bank4.inc"
};

/// Item-use keyframes and terminal stop controls for the M950 player's nineteen tracks.
///
/// Pose offsets count u32 words and durations count normal-rate frames; this clip has no cue flags.
static const AnimationRecord _gM950PlayerItemUseAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_03E64_records.inc"
};

/// Absolute item-use record starts for player tracks 0..18, followed by a serialized zero halfword.
///
/// Tracks 0/1 use bank 1 and tracks 2..18 use bank 4; child playback uses tracks 1..18.
static const u16 _gM950PlayerItemUseAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_03E64_indices.inc"
};

/// Item-use clip at id 40 in the M950 player's native animation bank.
///
/// Played during scripted item use before resuming aim locomotion. All nineteen
/// tracks stop; the M950 package must remain loaded while playback borrows the clip.
static AnimationSet _gM950PlayerItemUseAnimation = {
    .records           = _gM950PlayerItemUseAnimationRecords,
    .trackStartIndices = _gM950PlayerItemUseAnimationIndices,
    .poseBanks         = { [1] = _gM950PlayerItemUseAnimationBank1, [4] = _gM950PlayerItemUseAnimationBank4 },
};

/// Stair-ascent translation/rotation keys for player tracks 0 and 1 in the M950 bank.
///
/// Translations use model units and Euler angles use 1/4096 turns. Read-only
/// clip tables retain their serialized positions in .data.
static const AnimationPackedPose _gM950PlayerStairAscentAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_045C0_bank1.inc"
};

/// Stair-ascent joint rotations for player tracks 2..18, in eight-angle-unit steps.
static const AnimationPackedRotation _gM950PlayerStairAscentAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_045C0_bank4.inc"
};

/// Stair-ascent keyframes, loop/stop controls and alternating footstep cues on track 1.
///
/// Pose offsets count u32 words, jump targets count records and durations count normal-rate frames.
static const AnimationRecord _gM950PlayerStairAscentAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_045C0_records.inc"
};

/// Absolute stair-ascent record starts for player tracks 0..18, followed by a serialized zero halfword.
///
/// Tracks 0/1 use bank 1 and tracks 2..18 use bank 4; child playback uses tracks 1..18.
static const u16 _gM950PlayerStairAscentAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_045C0_indices.inc"
};

/// Stair-ascent clip at id 36 in the M950 player's native animation bank.
///
/// All nineteen tracks loop; track 1's footstep cues gate each scripted stride.
/// The M950 package must remain loaded while playback borrows the clip.
static AnimationSet _gM950PlayerStairAscentAnimation = {
    .records           = _gM950PlayerStairAscentAnimationRecords,
    .trackStartIndices = _gM950PlayerStairAscentAnimationIndices,
    .poseBanks         = { [1] = _gM950PlayerStairAscentAnimationBank1, [4] = _gM950PlayerStairAscentAnimationBank4 },
};

/// Stair-descent translation/rotation keys for player tracks 0 and 1 in the M950 bank.
///
/// Translations use model units and Euler angles use 1/4096 turns. Read-only
/// clip tables retain their serialized positions in .data.
static const AnimationPackedPose _gM950PlayerStairDescentAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_04D48_bank1.inc"
};

/// Stair-descent joint rotations for player tracks 2..18, in eight-angle-unit steps.
static const AnimationPackedRotation _gM950PlayerStairDescentAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_04D48_bank4.inc"
};

/// Stair-descent keyframes, loop/stop controls and alternating footstep cues on track 1.
///
/// Pose offsets count u32 words, jump targets count records and durations count normal-rate frames.
static const AnimationRecord _gM950PlayerStairDescentAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_04D48_records.inc"
};

/// Absolute stair-descent record starts for player tracks 0..18, followed by a serialized zero halfword.
///
/// Tracks 0/1 use bank 1 and tracks 2..18 use bank 4; child playback uses tracks 1..18.
static const u16 _gM950PlayerStairDescentAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_04D48_indices.inc"
};

/// Stair-descent clip at id 37 in the M950 player's native animation bank.
///
/// All nineteen tracks loop; track 1's footstep cues gate each scripted stride.
/// The M950 package must remain loaded while playback borrows the clip.
static AnimationSet _gM950PlayerStairDescentAnimation = {
    .records           = _gM950PlayerStairDescentAnimationRecords,
    .trackStartIndices = _gM950PlayerStairDescentAnimationIndices,
    .poseBanks         = { [1] = _gM950PlayerStairDescentAnimationBank1, [4] = _gM950PlayerStairDescentAnimationBank4 },
};

/// Odd-step stair-finish translation/rotation keys for player tracks 0 and 1 in the M950 bank.
///
/// Translations use model units and Euler angles use 1/4096 turns. Read-only
/// clip tables retain their serialized positions in .data.
static const AnimationPackedPose _gM950PlayerStairAscentFinishOddAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_0501C_bank1.inc"
};

/// Odd-step stair-finish joint rotations for player tracks 2..18, in eight-angle-unit steps.
static const AnimationPackedRotation _gM950PlayerStairAscentFinishOddAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_0501C_bank4.inc"
};

/// Odd-step stair-finish keyframes, terminal stops and one final footstep cue on track 1.
///
/// Pose offsets count u32 words and durations count normal-rate frames.
static const AnimationRecord _gM950PlayerStairAscentFinishOddAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_0501C_records.inc"
};

/// Absolute odd-step stair-finish record starts for tracks 0..18, followed by a serialized zero halfword.
///
/// Tracks 0/1 use bank 1 and tracks 2..18 use bank 4; child playback uses tracks 1..18.
static const u16 _gM950PlayerStairAscentFinishOddAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_0501C_indices.inc"
};

/// Stair-ascent finish clip at id 39 for an odd step count in the M950 player bank.
///
/// All nineteen tracks stop; track 1 cues the final footstep. The M950 package
/// must remain loaded while playback borrows the clip.
static AnimationSet _gM950PlayerStairAscentFinishOddAnimation = {
    .records           = _gM950PlayerStairAscentFinishOddAnimationRecords,
    .trackStartIndices = _gM950PlayerStairAscentFinishOddAnimationIndices,
    .poseBanks         = { [1] = _gM950PlayerStairAscentFinishOddAnimationBank1, [4] = _gM950PlayerStairAscentFinishOddAnimationBank4 },
};

/// Translation/Euler keys for `_gM950PlayerStairAscentFinishEvenAnimation`'s tracks 0 and 1.
///
/// Translations use model units and angles use 1/4096 turns. Read-only clip
/// tables retain their serialized positions in .data.
static const AnimationPackedPose _gM950PlayerStairAscentFinishEvenAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_053AC_bank1.inc"
};

/// Rotation-only keys for `_gM950PlayerStairAscentFinishEvenAnimation`'s tracks 2..18, in eight-angle-unit steps.
static const AnimationPackedRotation _gM950PlayerStairAscentFinishEvenAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_053AC_bank4.inc"
};

/// Keyframes, terminal stops and the final footstep cue for `_gM950PlayerStairAscentFinishEvenAnimation`.
///
/// Track 1 carries the cue. Pose offsets count four-byte words and durations
/// count normal-rate frames.
static const AnimationRecord _gM950PlayerStairAscentFinishEvenAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_053AC_records.inc"
};

/// Absolute record starts for `_gM950PlayerStairAscentFinishEvenAnimation`'s nineteen player tracks.
///
/// Tracks 0/1 use bank 1 and tracks 2..18 use bank 4; ordinary child playback
/// uses tracks 1..18. The resource's final zero halfword is retained.
static const u16 _gM950PlayerStairAscentFinishEvenAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_053AC_indices.inc"
};

/// Stair-ascent finish clip at id 38 for an even step count in the M950 player bank.
///
/// All nineteen tracks stop; track 1 cues the final footstep. The initial
/// ascent step-count parity selects this finish. The M950 package must remain
/// loaded while playback borrows the clip.
static AnimationSet _gM950PlayerStairAscentFinishEvenAnimation = {
    .records           = _gM950PlayerStairAscentFinishEvenAnimationRecords,
    .trackStartIndices = _gM950PlayerStairAscentFinishEvenAnimationIndices,
    .poseBanks         = {
        [M950_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM950PlayerStairAscentFinishEvenAnimationBank1,
        [M950_ANIMATION_PACKED_ROTATION_BANK]      = _gM950PlayerStairAscentFinishEvenAnimationBank4,
    },
};

/// Translation/Euler keys for `_gM950PlayerPeReleaseFamily0Animation`'s tracks 0 and 1.
///
/// Translations use model units and angles use 1/4096 turns. Read-only clip
/// tables retain their serialized positions in .data.
static const AnimationPackedPose _gM950PlayerPeReleaseFamily0AnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_05834_bank1.inc"
};

/// Rotation-only keys for `_gM950PlayerPeReleaseFamily0Animation`'s tracks 2..18, in eight-angle-unit steps.
static const AnimationPackedRotation _gM950PlayerPeReleaseFamily0AnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_05834_bank4.inc"
};

/// Keyframes and terminal stops for `_gM950PlayerPeReleaseFamily0Animation`'s nineteen tracks.
///
/// Pose offsets count four-byte words and durations count normal-rate frames.
/// No keyframe carries a cue.
static const AnimationRecord _gM950PlayerPeReleaseFamily0AnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_05834_records.inc"
};

/// Absolute record starts for `_gM950PlayerPeReleaseFamily0Animation`'s nineteen player tracks.
///
/// Tracks 0/1 use bank 1 and tracks 2..18 use bank 4; ordinary child playback
/// uses tracks 1..18. The resource's final zero halfword is retained.
static const u16 _gM950PlayerPeReleaseFamily0AnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_05834_indices.inc"
};

/// Family-0 Parasite Energy release clip at id 26 in the M950 player bank.
///
/// Family 0 covers spell IDs 300..600 except tens digit 3, and item IDs at or
/// above 601 with tens digit 1. Release starts the attachment countdown before
/// charge and cast. All nineteen tracks stop; the M950 package must remain
/// loaded while playback borrows the clip.
static AnimationSet _gM950PlayerPeReleaseFamily0Animation = {
    .records           = _gM950PlayerPeReleaseFamily0AnimationRecords,
    .trackStartIndices = _gM950PlayerPeReleaseFamily0AnimationIndices,
    .poseBanks         = {
        [M950_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM950PlayerPeReleaseFamily0AnimationBank1,
        [M950_ANIMATION_PACKED_ROTATION_BANK]      = _gM950PlayerPeReleaseFamily0AnimationBank4,
    },
};

/// Translation/Euler keys for `_gM950PlayerPeChargeFamily0Animation`'s tracks 0 and 1.
///
/// Translations use model units and angles use 1/4096 turns. Read-only clip
/// tables retain their serialized positions in .data.
static const AnimationPackedPose _gM950PlayerPeChargeFamily0AnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_05A30_bank1.inc"
};

/// Rotation-only keys for `_gM950PlayerPeChargeFamily0Animation`'s tracks 2..18, in eight-angle-unit steps.
static const AnimationPackedRotation _gM950PlayerPeChargeFamily0AnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_05A30_bank4.inc"
};

/// Keyframes, loop jumps and retained stop records for `_gM950PlayerPeChargeFamily0Animation`.
///
/// All nineteen tracks jump to their starts; the trailing stops are not reached
/// during forward playback. Pose offsets count four-byte words, jump targets
/// count records and durations count normal-rate frames. No keyframe carries a cue.
static const AnimationRecord _gM950PlayerPeChargeFamily0AnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_05A30_records.inc"
};

/// Absolute record starts for `_gM950PlayerPeChargeFamily0Animation`'s nineteen player tracks.
///
/// Tracks 0/1 use bank 1 and tracks 2..18 use bank 4; ordinary child playback
/// uses tracks 1..18. The resource's final zero halfword is retained.
static const u16 _gM950PlayerPeChargeFamily0AnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_05A30_indices.inc"
};

/// Looping family-0 Parasite Energy charge clip at id 27 in the M950 player bank.
///
/// Played between `_gM950PlayerPeReleaseFamily0Animation` and the family-0 cast
/// while the attachment countdown remains nonzero. All nineteen tracks loop;
/// the M950 package must remain loaded while playback borrows the clip.
static AnimationSet _gM950PlayerPeChargeFamily0Animation = {
    .records           = _gM950PlayerPeChargeFamily0AnimationRecords,
    .trackStartIndices = _gM950PlayerPeChargeFamily0AnimationIndices,
    .poseBanks         = {
        [M950_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM950PlayerPeChargeFamily0AnimationBank1,
        [M950_ANIMATION_PACKED_ROTATION_BANK]      = _gM950PlayerPeChargeFamily0AnimationBank4,
    },
};

/// Translation/Euler keys for `_gM950PlayerPeCastFamily0Animation`'s tracks 0 and 1.
///
/// Translations use model units and angles use 1/4096 turns. Read-only clip
/// tables retain their serialized positions in .data.
static const AnimationPackedPose _gM950PlayerPeCastFamily0AnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_05D08_bank1.inc"
};

/// Rotation-only keys for `_gM950PlayerPeCastFamily0Animation`'s tracks 2..18, in eight-angle-unit steps.
static const AnimationPackedRotation _gM950PlayerPeCastFamily0AnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_05D08_bank4.inc"
};

/// Keyframes and terminal stops for `_gM950PlayerPeCastFamily0Animation`'s nineteen tracks.
///
/// Pose offsets count four-byte words and durations count normal-rate frames.
/// No keyframe carries a cue.
static const AnimationRecord _gM950PlayerPeCastFamily0AnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_05D08_records.inc"
};

/// Absolute record starts for `_gM950PlayerPeCastFamily0Animation`'s nineteen player tracks.
///
/// Tracks 0/1 use bank 1 and tracks 2..18 use bank 4; ordinary child playback
/// uses tracks 1..18. The resource's final zero halfword is retained.
static const u16 _gM950PlayerPeCastFamily0AnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_05D08_indices.inc"
};

/// Family-0 Parasite Energy cast clip at id 28 in the M950 player bank.
///
/// Follows `_gM950PlayerPeChargeFamily0Animation` when the attachment countdown
/// reaches zero. Its body-track completion allows normal control to resume.
/// All nineteen tracks stop; the M950 package must remain loaded while playback
/// borrows the clip.
static AnimationSet _gM950PlayerPeCastFamily0Animation = {
    .records           = _gM950PlayerPeCastFamily0AnimationRecords,
    .trackStartIndices = _gM950PlayerPeCastFamily0AnimationIndices,
    .poseBanks         = {
        [M950_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM950PlayerPeCastFamily0AnimationBank1,
        [M950_ANIMATION_PACKED_ROTATION_BANK]      = _gM950PlayerPeCastFamily0AnimationBank4,
    },
};

/// Translation/Euler keys for `_gM950PlayerPeReleaseFamily1Animation`'s tracks 0 and 1.
///
/// Translations use model units and angles use 1/4096 turns. Read-only clip
/// tables retain their serialized positions in .data.
static const AnimationPackedPose _gM950PlayerPeReleaseFamily1AnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_05FAC_bank1.inc"
};

/// Rotation-only keys for `_gM950PlayerPeReleaseFamily1Animation`'s tracks 2..18, in eight-angle-unit steps.
static const AnimationPackedRotation _gM950PlayerPeReleaseFamily1AnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_05FAC_bank4.inc"
};

/// Keyframes and terminal stops for `_gM950PlayerPeReleaseFamily1Animation`'s nineteen tracks.
///
/// Pose offsets count four-byte words and durations count normal-rate frames.
/// No keyframe carries a cue.
static const AnimationRecord _gM950PlayerPeReleaseFamily1AnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_05FAC_records.inc"
};

/// Absolute record starts for `_gM950PlayerPeReleaseFamily1Animation`'s nineteen player tracks.
///
/// Tracks 0/1 use bank 1 and tracks 2..18 use bank 4; ordinary child playback
/// uses tracks 1..18. The resource's final zero halfword is retained.
static const u16 _gM950PlayerPeReleaseFamily1AnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_05FAC_indices.inc"
};

/// Family-1 Parasite Energy release clip at id 29 in the M950 player bank.
///
/// Family 1 covers spell IDs below 300 except tens digit 3, and item IDs at or
/// above 601 with a tens digit other than 1. Release starts the attachment
/// countdown before charge and cast. All nineteen tracks stop; the M950
/// package must remain loaded while playback borrows the clip.
static AnimationSet _gM950PlayerPeReleaseFamily1Animation = {
    .records           = _gM950PlayerPeReleaseFamily1AnimationRecords,
    .trackStartIndices = _gM950PlayerPeReleaseFamily1AnimationIndices,
    .poseBanks         = {
        [M950_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM950PlayerPeReleaseFamily1AnimationBank1,
        [M950_ANIMATION_PACKED_ROTATION_BANK]      = _gM950PlayerPeReleaseFamily1AnimationBank4,
    },
};

/// Translation/Euler keys for `_gM950PlayerPeChargeFamily1Animation`'s tracks 0 and 1.
///
/// Translations use model units and angles use 1/4096 turns. Read-only clip
/// tables retain their serialized positions in .data.
static const AnimationPackedPose _gM950PlayerPeChargeFamily1AnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_061AC_bank1.inc"
};

/// Rotation-only keys for `_gM950PlayerPeChargeFamily1Animation`'s tracks 2..18, in eight-angle-unit steps.
static const AnimationPackedRotation _gM950PlayerPeChargeFamily1AnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_061AC_bank4.inc"
};

/// Keyframes, loop jumps and retained stop records for `_gM950PlayerPeChargeFamily1Animation`.
///
/// All nineteen tracks jump to their starts; the trailing stops are not reached
/// during forward playback. Pose offsets count four-byte words, jump targets
/// count records and durations count normal-rate frames. No keyframe carries a cue.
static const AnimationRecord _gM950PlayerPeChargeFamily1AnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_061AC_records.inc"
};

/// Absolute record starts for `_gM950PlayerPeChargeFamily1Animation`'s nineteen player tracks.
///
/// Tracks 0/1 use bank 1 and tracks 2..18 use bank 4; ordinary child playback
/// uses tracks 1..18. The resource's final zero halfword is retained.
static const u16 _gM950PlayerPeChargeFamily1AnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_061AC_indices.inc"
};

/// Looping family-1 Parasite Energy charge clip at id 30 in the M950 player bank.
///
/// Played after `_gM950PlayerPeReleaseFamily1Animation` while the attachment
/// countdown remains nonzero, before the family-1 cast. All nineteen tracks
/// loop; the M950 package must remain loaded while playback borrows the clip.
static AnimationSet _gM950PlayerPeChargeFamily1Animation = {
    .records           = _gM950PlayerPeChargeFamily1AnimationRecords,
    .trackStartIndices = _gM950PlayerPeChargeFamily1AnimationIndices,
    .poseBanks         = {
        [M950_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM950PlayerPeChargeFamily1AnimationBank1,
        [M950_ANIMATION_PACKED_ROTATION_BANK]      = _gM950PlayerPeChargeFamily1AnimationBank4,
    },
};

/// Translation/Euler keys for `_gM950PlayerPeCastFamily1Animation`'s tracks 0 and 1.
///
/// Translations use model units and angles use 1/4096 turns. Read-only clip
/// tables retain their serialized positions in .data.
static const AnimationPackedPose _gM950PlayerPeCastFamily1AnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_06500_bank1.inc"
};

/// Rotation-only keys for `_gM950PlayerPeCastFamily1Animation`'s tracks 2..18, in eight-angle-unit steps.
static const AnimationPackedRotation _gM950PlayerPeCastFamily1AnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_06500_bank4.inc"
};

/// Keyframes and terminal stops for `_gM950PlayerPeCastFamily1Animation`'s nineteen tracks.
///
/// Pose offsets count four-byte words and durations count normal-rate frames.
/// No keyframe carries a cue.
static const AnimationRecord _gM950PlayerPeCastFamily1AnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_06500_records.inc"
};

/// Absolute record starts for `_gM950PlayerPeCastFamily1Animation`'s nineteen player tracks.
///
/// Tracks 0/1 use bank 1 and tracks 2..18 use bank 4; ordinary child playback
/// uses tracks 1..18. The resource's final zero halfword is retained.
static const u16 _gM950PlayerPeCastFamily1AnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_06500_indices.inc"
};

/// Family-1 Parasite Energy cast clip at id 31 in the M950 player bank.
///
/// Family 1 covers spell IDs below 300 except tens digit 3, and item IDs at or
/// above 601 with a tens digit other than 1. Cast follows release and charge
/// when the attachment countdown reaches zero. All nineteen tracks stop;
/// the M950 package must remain loaded while playback borrows the clip.
static AnimationSet _gM950PlayerPeCastFamily1Animation = {
    .records           = _gM950PlayerPeCastFamily1AnimationRecords,
    .trackStartIndices = _gM950PlayerPeCastFamily1AnimationIndices,
    .poseBanks         = {
        [M950_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM950PlayerPeCastFamily1AnimationBank1,
        [M950_ANIMATION_PACKED_ROTATION_BANK]      = _gM950PlayerPeCastFamily1AnimationBank4,
    },
};

/// Translation/Euler keys for `_gM950PlayerHitPart4Animation`'s tracks 0 and 1.
///
/// Translations use model units and angles use 1/4096 turns. Read-only clip
/// tables retain their serialized positions in .data.
static const AnimationPackedPose _gM950PlayerHitPart4AnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_067B4_bank1.inc"
};

/// Rotation-only keys for `_gM950PlayerHitPart4Animation`'s tracks 2..18, in eight-angle-unit steps.
static const AnimationPackedRotation _gM950PlayerHitPart4AnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_067B4_bank4.inc"
};

/// Keyframes and terminal stops for `_gM950PlayerHitPart4Animation`'s nineteen tracks.
///
/// Pose offsets count four-byte words and durations count normal-rate frames.
/// No keyframe carries a cue.
static const AnimationRecord _gM950PlayerHitPart4AnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_067B4_records.inc"
};

/// Absolute record starts for `_gM950PlayerHitPart4Animation`'s nineteen player tracks.
///
/// Tracks 0/1 use bank 1 and tracks 2..18 use bank 4; ordinary child playback
/// uses tracks 1..18. The resource's final zero halfword is retained.
static const u16 _gM950PlayerHitPart4AnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_067B4_indices.inc"
};

/// Hit reaction for the player collision body at model part 4, at id 16 in the M950 bank.
///
/// Selected for pending and contact hits recorded as region 1, with a
/// three-frame entry blend. All nineteen tracks stop; the M950 package
/// must remain loaded while playback borrows the clip.
static AnimationSet _gM950PlayerHitPart4Animation = {
    .records           = _gM950PlayerHitPart4AnimationRecords,
    .trackStartIndices = _gM950PlayerHitPart4AnimationIndices,
    .poseBanks         = {
        [M950_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM950PlayerHitPart4AnimationBank1,
        [M950_ANIMATION_PACKED_ROTATION_BANK]      = _gM950PlayerHitPart4AnimationBank4,
    },
};

/// Translation/Euler keys for `_gM950PlayerHitOtherBodyAnimation`'s tracks 0 and 1.
///
/// Translations use model units and angles use 1/4096 turns. Read-only clip
/// tables retain their serialized positions in .data.
static const AnimationPackedPose _gM950PlayerHitOtherBodyAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_06AD4_bank1.inc"
};

/// Rotation-only keys for `_gM950PlayerHitOtherBodyAnimation`'s tracks 2..18, in eight-angle-unit steps.
static const AnimationPackedRotation _gM950PlayerHitOtherBodyAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_06AD4_bank4.inc"
};

/// Keyframes and terminal stops for `_gM950PlayerHitOtherBodyAnimation`'s nineteen tracks.
///
/// Pose offsets count four-byte words and durations count normal-rate frames.
/// No keyframe carries a cue.
static const AnimationRecord _gM950PlayerHitOtherBodyAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_06AD4_records.inc"
};

/// Absolute record starts for `_gM950PlayerHitOtherBodyAnimation`'s nineteen player tracks.
///
/// Tracks 0/1 use bank 1 and tracks 2..18 use bank 4; ordinary child playback
/// uses tracks 1..18. The resource's final zero halfword is retained.
static const u16 _gM950PlayerHitOtherBodyAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_06AD4_indices.inc"
};

/// Hit reaction for the player's other collision bodies, at id 17 in the M950 bank.
///
/// Used for pending and contact hits outside region 1, including the root
/// and model-part-1 bodies, with a three-frame entry blend. All nineteen
/// tracks stop; the M950 package must remain loaded while playback borrows the clip.
static AnimationSet _gM950PlayerHitOtherBodyAnimation = {
    .records           = _gM950PlayerHitOtherBodyAnimationRecords,
    .trackStartIndices = _gM950PlayerHitOtherBodyAnimationIndices,
    .poseBanks         = {
        [M950_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM950PlayerHitOtherBodyAnimationBank1,
        [M950_ANIMATION_PACKED_ROTATION_BANK]      = _gM950PlayerHitOtherBodyAnimationBank4,
    },
};

/// Translation/Euler keys for `_gM950PlayerStoppedPoseAnimation`'s tracks 0 and 1.
///
/// Translations use model units and angles use 1/4096 turns. Read-only clip
/// tables retain their serialized positions in .data.
static const AnimationPackedPose _gM950PlayerStoppedPoseAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_07298_bank1.inc"
};

/// Rotation-only keys for `_gM950PlayerStoppedPoseAnimation`'s tracks 2..18, in eight-angle-unit steps.
static const AnimationPackedRotation _gM950PlayerStoppedPoseAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_07298_bank4.inc"
};

/// Keyframes and terminal stops for `_gM950PlayerStoppedPoseAnimation`'s nineteen tracks.
///
/// Pose offsets count four-byte words and durations count normal-rate frames.
/// No keyframe carries a cue.
static const AnimationRecord _gM950PlayerStoppedPoseAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_07298_records.inc"
};

/// Absolute record starts for `_gM950PlayerStoppedPoseAnimation`'s nineteen player tracks.
///
/// Tracks 0/1 use bank 1 and tracks 2..18 use bank 4; ordinary child playback
/// uses tracks 1..18. The resource's final zero halfword is retained.
static const u16 _gM950PlayerStoppedPoseAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_07298_indices.inc"
};

/// Stopped-pose clip used at zero player HP, at id 18 in the M950 bank.
///
/// Plays with displacement, turning and targeting disabled. Normal control
/// requests a four-frame entry blend; scripted control restores the native
/// bank before requesting it. All nineteen tracks stop; the M950 package
/// must remain loaded while playback borrows the clip.
static AnimationSet _gM950PlayerStoppedPoseAnimation = {
    .records           = _gM950PlayerStoppedPoseAnimationRecords,
    .trackStartIndices = _gM950PlayerStoppedPoseAnimationIndices,
    .poseBanks         = {
        [M950_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM950PlayerStoppedPoseAnimationBank1,
        [M950_ANIMATION_PACKED_ROTATION_BANK]      = _gM950PlayerStoppedPoseAnimationBank4,
    },
};

/// Translation/Euler keys for `_gM950PlayerHealthyIdleAnimation`'s tracks 0 and 1.
///
/// Translations use model units and angles use 1/4096 turns. Read-only clip
/// tables retain their serialized positions in .data.
static const AnimationPackedPose _gM950PlayerHealthyIdleAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_08530_bank1.inc"
};

/// Rotation-only keys for `_gM950PlayerHealthyIdleAnimation`'s tracks 2..18, in eight-angle-unit steps.
static const AnimationPackedRotation _gM950PlayerHealthyIdleAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_08530_bank4.inc"
};

/// Keyframes, tail-loop jumps and retained stops for `_gM950PlayerHealthyIdleAnimation`.
///
/// All nineteen tracks loop within their own spans; the trailing stops are
/// not reached during forward playback. Pose offsets count four-byte words,
/// jump targets count records and durations count normal-rate frames.
/// No keyframe carries a cue.
static const AnimationRecord _gM950PlayerHealthyIdleAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_08530_records.inc"
};

/// Absolute record starts for `_gM950PlayerHealthyIdleAnimation`'s nineteen player tracks.
///
/// Tracks 0/1 use bank 1 and tracks 2..18 use bank 4; ordinary child playback
/// uses tracks 1..18. The resource's final zero halfword is retained.
static const u16 _gM950PlayerHealthyIdleAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_08530_indices.inc"
};

/// Looping idle clip for the player above half maximum HP, at id 23 in the M950 bank.
///
/// Normal locomotion selects it after 300 ticks without held directions,
/// with a five-frame entry blend. All nineteen tracks loop; the M950
/// package must remain loaded while playback borrows the clip.
static AnimationSet _gM950PlayerHealthyIdleAnimation = {
    .records           = _gM950PlayerHealthyIdleAnimationRecords,
    .trackStartIndices = _gM950PlayerHealthyIdleAnimationIndices,
    .poseBanks         = {
        [M950_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM950PlayerHealthyIdleAnimationBank1,
        [M950_ANIMATION_PACKED_ROTATION_BANK]      = _gM950PlayerHealthyIdleAnimationBank4,
    },
};

/// Translation/Euler keys for `_gM950PlayerLowHealthIdleAnimation`'s tracks 0 and 1.
///
/// Translations use model units and angles use 1/4096 turns. Read-only clip
/// tables retain their serialized positions in .data.
static const AnimationPackedPose _gM950PlayerLowHealthIdleAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_090A8_bank1.inc"
};

/// Rotation-only keys for `_gM950PlayerLowHealthIdleAnimation`'s tracks 2..18, in eight-angle-unit steps.
static const AnimationPackedRotation _gM950PlayerLowHealthIdleAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_090A8_bank4.inc"
};

/// Keyframes, tail-loop jumps and retained stops for `_gM950PlayerLowHealthIdleAnimation`.
///
/// All nineteen tracks loop within their own spans; the trailing stops are
/// not reached during forward playback. Pose offsets count four-byte words,
/// jump targets count records and durations count normal-rate frames.
/// No keyframe carries a cue.
static const AnimationRecord _gM950PlayerLowHealthIdleAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_090A8_records.inc"
};

/// Absolute record starts for `_gM950PlayerLowHealthIdleAnimation`'s nineteen player tracks.
///
/// Tracks 0/1 use bank 1 and tracks 2..18 use bank 4; ordinary child playback
/// uses tracks 1..18. The resource's final zero halfword is retained.
static const u16 _gM950PlayerLowHealthIdleAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_090A8_indices.inc"
};

/// Looping low-health idle and paralysis clip at id 25 in the M950 player bank.
///
/// Normal locomotion selects it at or below a quarter maximum HP after 300
/// ticks without held directions, with a five-frame blend. Paralysis selects
/// it immediately with a six-frame blend. Both uses play all child tracks
/// 1..18. All nineteen tracks loop; the M950 package must remain loaded
/// while playback borrows the clip.
static AnimationSet _gM950PlayerLowHealthIdleAnimation = {
    .records           = _gM950PlayerLowHealthIdleAnimationRecords,
    .trackStartIndices = _gM950PlayerLowHealthIdleAnimationIndices,
    .poseBanks         = {
        [M950_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM950PlayerLowHealthIdleAnimationBank1,
        [M950_ANIMATION_PACKED_ROTATION_BANK]      = _gM950PlayerLowHealthIdleAnimationBank4,
    },
};

/// Mid-health idle translations in model units and Euler angles in 1/4096 turns for tracks 0 and 1.
///
/// Read-only clip tables retain their serialized positions in .data;
/// pose banks are word-aligned for playback's word offsets.
static const AnimationPackedPose _gM950PlayerMidHealthIdleAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m950_animation_097C4_bank1.inc"
};

/// Mid-health idle joint rotations for player tracks 2..18, in eight-angle-unit steps.
static const AnimationPackedRotation _gM950PlayerMidHealthIdleAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_097C4_bank4.inc"
};

/// Mid-health idle keyframes, loop controls and retained unreachable stop records.
///
/// Pose offsets count u32 words, jump targets count records and durations
/// count normal-rate frames. This clip has no keyframe cue flags.
static const AnimationRecord _gM950PlayerMidHealthIdleAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_097C4_records.inc"
};

/// Absolute mid-health idle record starts for player tracks 0..18, followed by a serialized zero halfword.
///
/// Tracks 0/1 use bank 1 and tracks 2..18 use bank 4; child playback uses tracks 1..18.
static const u16 _gM950PlayerMidHealthIdleAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_097C4_indices.inc"
};

/// Looping mid-health idle clip at id 24 in the M950 player's native animation bank.
///
/// Selected above a quarter and at or below half maximum HP after 300 ticks
/// without held directions, with a five-frame blend. All nineteen tracks loop;
/// the M950 package must remain loaded while playback borrows the clip.
static AnimationSet _gM950PlayerMidHealthIdleAnimation = {
    .records           = _gM950PlayerMidHealthIdleAnimationRecords,
    .trackStartIndices = _gM950PlayerMidHealthIdleAnimationIndices,
    .poseBanks         = {
        [M950_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM950PlayerMidHealthIdleAnimationBank1,
        [M950_ANIMATION_PACKED_ROTATION_BANK]      = _gM950PlayerMidHealthIdleAnimationBank4,
    },
};

/// Presentation-entry translations in model units and Euler angles in 1/4096 turns for tracks 0 and 1.
///
/// Read-only clip tables retain their serialized positions in .data;
/// pose banks are word-aligned for playback's word offsets.
static const AnimationPackedPose _gM950PlayerScriptedPresentationEnterAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m950_animation_09C44_bank1.inc"
};

/// Presentation-entry joint rotations for player tracks 2..18, in eight-angle-unit steps.
static const AnimationPackedRotation _gM950PlayerScriptedPresentationEnterAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_09C44_bank4.inc"
};

/// Presentation-entry keyframes and terminal stop controls for the M950 player's nineteen tracks.
///
/// Pose offsets count u32 words and durations count normal-rate frames;
/// this clip has no keyframe cue flags.
static const AnimationRecord _gM950PlayerScriptedPresentationEnterAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_09C44_records.inc"
};

/// Absolute presentation-entry record starts for player tracks 0..18, followed by a serialized zero halfword.
///
/// Tracks 0/1 use bank 1 and tracks 2..18 use bank 4; child playback uses tracks 1..18.
static const u16 _gM950PlayerScriptedPresentationEnterAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_09C44_indices.inc"
};

/// Scripted presentation-entry clip at ids 32 and 34 in the M950 player bank.
///
/// A zero presentation selector plays id 32 before a CAP event or interaction,
/// with no entry blend. All nineteen tracks stop. Id 34 is a retained table
/// alias; the M950 package must remain loaded while playback borrows the clip.
static AnimationSet _gM950PlayerScriptedPresentationEnterAnimation = {
    .records           = _gM950PlayerScriptedPresentationEnterAnimationRecords,
    .trackStartIndices = _gM950PlayerScriptedPresentationEnterAnimationIndices,
    .poseBanks         = {
        [M950_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM950PlayerScriptedPresentationEnterAnimationBank1,
        [M950_ANIMATION_PACKED_ROTATION_BANK]      = _gM950PlayerScriptedPresentationEnterAnimationBank4,
    },
};

/// Presentation-exit translations in model units and Euler angles in 1/4096 turns for tracks 0 and 1.
///
/// Read-only clip tables retain their serialized positions in .data;
/// pose banks are word-aligned for playback's word offsets.
static const AnimationPackedPose _gM950PlayerScriptedPresentationExitAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m950_animation_09E1C_bank1.inc"
};

/// Presentation-exit joint rotations for player tracks 2..18, in eight-angle-unit steps.
static const AnimationPackedRotation _gM950PlayerScriptedPresentationExitAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_09E1C_bank4.inc"
};

/// Presentation-exit keyframes and terminal stop controls for the M950 player's nineteen tracks.
///
/// Pose offsets count u32 words and durations count normal-rate frames;
/// this clip has no keyframe cue flags.
static const AnimationRecord _gM950PlayerScriptedPresentationExitAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_09E1C_records.inc"
};

/// Absolute presentation-exit record starts for player tracks 0..18, followed by a serialized zero halfword.
///
/// Tracks 0/1 use bank 1 and tracks 2..18 use bank 4; child playback uses tracks 1..18.
static const u16 _gM950PlayerScriptedPresentationExitAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_09E1C_indices.inc"
};

/// Scripted presentation-exit clip at ids 33 and 35 in the M950 player bank.
///
/// A nonzero presentation selector plays id 33 after a CAP event or interaction,
/// with no entry blend. All nineteen tracks stop. Id 35 is a retained table
/// alias; the M950 package must remain loaded while playback borrows the clip.
static AnimationSet _gM950PlayerScriptedPresentationExitAnimation = {
    .records           = _gM950PlayerScriptedPresentationExitAnimationRecords,
    .trackStartIndices = _gM950PlayerScriptedPresentationExitAnimationIndices,
    .poseBanks         = {
        [M950_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM950PlayerScriptedPresentationExitAnimationBank1,
        [M950_ANIMATION_PACKED_ROTATION_BANK]      = _gM950PlayerScriptedPresentationExitAnimationBank4,
    },
};

/// PE family-2 release translations in model units and Euler angles in 1/4096 turns for tracks 0 and 1.
///
/// Read-only clip tables retain their serialized positions in .data;
/// pose banks are word-aligned for playback's word offsets.
static const AnimationPackedPose _gM950PlayerPeReleaseFamily2AnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m950_animation_0A380_bank1.inc"
};

/// PE family-2 release joint rotations for player tracks 2..18, in eight-angle-unit steps.
static const AnimationPackedRotation _gM950PlayerPeReleaseFamily2AnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_0A380_bank4.inc"
};

/// PE family-2 release keyframes and terminal stop controls for the M950 player's nineteen tracks.
///
/// Pose offsets count u32 words and durations count normal-rate frames;
/// this clip has no keyframe cue flags.
static const AnimationRecord _gM950PlayerPeReleaseFamily2AnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_0A380_records.inc"
};

/// Absolute PE family-2 release record starts for player tracks 0..18, followed by a serialized zero halfword.
///
/// Tracks 0/1 use bank 1 and tracks 2..18 use bank 4; child playback uses tracks 1..18.
static const u16 _gM950PlayerPeReleaseFamily2AnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_0A380_indices.inc"
};

/// Parasite Energy family-2 release clip at id 42 in the M950 player bank.
///
/// Spell attachment IDs with decimal tens digit 3 select this family. Release
/// starts the attachment countdown and blends child tracks over six frames
/// before charge. All nineteen tracks stop; keep the M950 package loaded
/// while playback borrows the clip.
static AnimationSet _gM950PlayerPeReleaseFamily2Animation = {
    .records           = _gM950PlayerPeReleaseFamily2AnimationRecords,
    .trackStartIndices = _gM950PlayerPeReleaseFamily2AnimationIndices,
    .poseBanks         = {
        [M950_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM950PlayerPeReleaseFamily2AnimationBank1,
        [M950_ANIMATION_PACKED_ROTATION_BANK]      = _gM950PlayerPeReleaseFamily2AnimationBank4,
    },
};

/// PE family-2 charge translations in model units and Euler angles in 1/4096 turns for tracks 0 and 1.
///
/// Read-only clip tables retain their serialized positions in .data;
/// pose banks are word-aligned for playback's word offsets.
static const AnimationPackedPose _gM950PlayerPeChargeFamily2AnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m950_animation_0A574_bank1.inc"
};

/// PE family-2 charge joint rotations for player tracks 2..18, in eight-angle-unit steps.
static const AnimationPackedRotation _gM950PlayerPeChargeFamily2AnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_0A574_bank4.inc"
};

/// PE family-2 charge keyframes, loop controls and retained unreachable stop records.
///
/// Pose offsets count u32 words, jump targets count records and durations
/// count normal-rate frames. This clip has no keyframe cue flags.
static const AnimationRecord _gM950PlayerPeChargeFamily2AnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_0A574_records.inc"
};

/// Absolute PE family-2 charge record starts for player tracks 0..18, followed by a serialized zero halfword.
///
/// Tracks 0/1 use bank 1 and tracks 2..18 use bank 4; child playback uses tracks 1..18.
static const u16 _gM950PlayerPeChargeFamily2AnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_0A574_indices.inc"
};

/// Looping Parasite Energy family-2 charge clip at id 43 in the M950 player bank.
///
/// Spell attachment IDs with decimal tens digit 3 select this family. Child
/// tracks restart without a blend after release, holding the charge until the
/// attachment countdown expires or the action is cancelled. All nineteen
/// tracks loop; keep the M950 package loaded while playback borrows the clip.
static AnimationSet _gM950PlayerPeChargeFamily2Animation = {
    .records           = _gM950PlayerPeChargeFamily2AnimationRecords,
    .trackStartIndices = _gM950PlayerPeChargeFamily2AnimationIndices,
    .poseBanks         = {
        [M950_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM950PlayerPeChargeFamily2AnimationBank1,
        [M950_ANIMATION_PACKED_ROTATION_BANK]      = _gM950PlayerPeChargeFamily2AnimationBank4,
    },
};

/// PE family-2 cast translations in model units and Euler angles in 1/4096 turns for tracks 0 and 1.
///
/// Read-only clip tables retain their serialized positions in .data;
/// pose banks are word-aligned for playback's word offsets.
static const AnimationPackedPose _gM950PlayerPeCastFamily2AnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m950_animation_0A9F4_bank1.inc"
};

/// PE family-2 cast joint rotations for player tracks 2..18, in eight-angle-unit steps.
static const AnimationPackedRotation _gM950PlayerPeCastFamily2AnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_0A9F4_bank4.inc"
};

/// PE family-2 cast keyframes and terminal stop controls for the M950 player's nineteen tracks.
///
/// Pose offsets count u32 words and durations count normal-rate frames;
/// this clip has no keyframe cue flags.
static const AnimationRecord _gM950PlayerPeCastFamily2AnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_0A9F4_records.inc"
};

/// Absolute PE family-2 cast record starts for player tracks 0..18, followed by a serialized zero halfword.
///
/// Tracks 0/1 use bank 1 and tracks 2..18 use bank 4; child playback uses tracks 1..18.
static const u16 _gM950PlayerPeCastFamily2AnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_0A9F4_indices.inc"
};

/// Parasite Energy family-2 cast clip at id 44 in the M950 player bank.
///
/// Spell attachment IDs with decimal tens digit 3 select this family. Child
/// tracks restart without a blend when the attachment countdown expires.
/// All nineteen tracks stop before locomotion or aiming resumes; keep the
/// M950 package loaded while playback borrows the clip.
static AnimationSet _gM950PlayerPeCastFamily2Animation = {
    .records           = _gM950PlayerPeCastFamily2AnimationRecords,
    .trackStartIndices = _gM950PlayerPeCastFamily2AnimationIndices,
    .poseBanks         = {
        [M950_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM950PlayerPeCastFamily2AnimationBank1,
        [M950_ANIMATION_PACKED_ROTATION_BANK]      = _gM950PlayerPeCastFamily2AnimationBank4,
    },
};

/// Forward-aim translations in model units and Euler angles in 1/4096 turns for tracks 0 and 1.
///
/// Read-only clip tables retain their serialized positions in .data;
/// pose banks are word-aligned for playback's word offsets.
static const AnimationPackedPose _gM950PlayerAimForwardAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m950_animation_0B49C_bank1.inc"
};

/// Forward-aim joint rotations for player tracks 2..18, in eight-angle-unit steps.
static const AnimationPackedRotation _gM950PlayerAimForwardAnimationBank4[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m950_animation_0B49C_bank4.inc"
};

/// Forward-aim keyframes, loop jumps and retained terminal stops.
///
/// Durations count normal-rate frames, pose offsets count u32 words and jumps
/// select absolute record indices. Track 1 carries the paired footstep cues
/// `ANIMATION_RECORD_CUE_2` and `ANIMATION_RECORD_CUE_1`.
static const AnimationRecord _gM950PlayerAimForwardAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_0B49C_records.inc"
};

/// Absolute forward-aim record starts for tracks 0..18, followed by a serialized zero halfword.
static const u16 _gM950PlayerAimForwardAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_0B49C_indices.inc"
};
STATIC_ASSERT(ARRAY_SIZE(_gM950PlayerAimForwardAnimationIndices) == GAME_ACTOR_NORMAL_ANIMATION_SLOTS + 1, m950_aim_forward_track_starts);

/// Player forward movement while aiming with the M950, at native clip id 12.
///
/// All nineteen tracks loop, with paired footstep cues on track 1. Child
/// playback uses tracks 1..18; tracks 0/1 use bank 1 and 2..18 use bank 4.
/// Playback borrows the descriptor and tables; keep the M950 package loaded
/// while any slot or cached record pointer refers to them.
static AnimationSet _gM950PlayerAimForwardAnimation = {
    .records           = _gM950PlayerAimForwardAnimationRecords,
    .trackStartIndices = _gM950PlayerAimForwardAnimationIndices,
    .poseBanks         = {
        [M950_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM950PlayerAimForwardAnimationBank1,
        [M950_ANIMATION_PACKED_ROTATION_BANK]      = _gM950PlayerAimForwardAnimationBank4,
    },
};

/// Aim-idle translations in model units and Euler angles in 1/4096 turns for tracks 0 and 1.
///
/// Read-only clip tables retain their serialized positions in .data;
/// pose banks are word-aligned for playback's word offsets.
static const AnimationPackedPose _gM950PlayerAimIdleAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m950_animation_0B674_bank1.inc"
};

/// Aim-idle joint rotations for player tracks 2..18, in eight-angle-unit steps.
static const AnimationPackedRotation _gM950PlayerAimIdleAnimationBank4[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m950_animation_0B674_bank4.inc"
};

/// Identical aim-idle keyframes, loop jumps and retained terminal stops, with no cue flags.
///
/// Durations count normal-rate frames, pose offsets count u32 words and jumps
/// select absolute record indices.
static const AnimationRecord _gM950PlayerAimIdleAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_0B674_records.inc"
};

/// Absolute aim-idle record starts for tracks 0..18, followed by a serialized zero halfword.
static const u16 _gM950PlayerAimIdleAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_0B674_indices.inc"
};
STATIC_ASSERT(ARRAY_SIZE(_gM950PlayerAimIdleAnimationIndices) == GAME_ACTOR_NORMAL_ANIMATION_SLOTS + 1, m950_aim_idle_track_starts);

/// Player stationary aiming loop and attack-ready pose in the M950 bank, at native clip id 9.
///
/// All nineteen tracks loop between identical poses. Child playback uses
/// tracks 1..18; tracks 0/1 use bank 1 and 2..18 use bank 4. The same set is
/// borrowed by enemy-driven player recovery playback. Keep the M950 package
/// loaded while any slot or cached record pointer refers to its descriptor or tables.
static AnimationSet _gM950PlayerAimIdleAnimation = {
    .records           = _gM950PlayerAimIdleAnimationRecords,
    .trackStartIndices = _gM950PlayerAimIdleAnimationIndices,
    .poseBanks         = {
        [M950_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM950PlayerAimIdleAnimationBank1,
        [M950_ANIMATION_PACKED_ROTATION_BANK]      = _gM950PlayerAimIdleAnimationBank4,
    },
};

/// Backward/turn aim translations in model units and Euler angles in 1/4096 turns for tracks 0 and 1.
///
/// Read-only clip tables retain their serialized positions in .data;
/// pose banks are word-aligned for playback's word offsets.
static const AnimationPackedPose _gM950PlayerAimBackwardOrTurnAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m950_animation_0BDA8_bank1.inc"
};

/// Backward/turn aim joint rotations for player tracks 2..18, in eight-angle-unit steps.
static const AnimationPackedRotation _gM950PlayerAimBackwardOrTurnAnimationBank4[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m950_animation_0BDA8_bank4.inc"
};

/// Backward/turn aim keyframes, loop jumps and retained terminal stops.
///
/// Durations count normal-rate frames, pose offsets count u32 words and jumps
/// select absolute record indices. Track 1 carries the paired footstep cues
/// `ANIMATION_RECORD_CUE_2` and `ANIMATION_RECORD_CUE_1`.
static const AnimationRecord _gM950PlayerAimBackwardOrTurnAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_0BDA8_records.inc"
};

/// Absolute backward/turn aim record starts for tracks 0..18, followed by a serialized zero halfword.
static const u16 _gM950PlayerAimBackwardOrTurnAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_0BDA8_indices.inc"
};
STATIC_ASSERT(ARRAY_SIZE(_gM950PlayerAimBackwardOrTurnAnimationIndices) == GAME_ACTOR_NORMAL_ANIMATION_SLOTS + 1, m950_aim_backward_or_turn_track_starts);

/// Player backward movement or stationary turning while aiming with the M950, at native clip id 13.
///
/// All nineteen tracks loop, with paired footstep cues on track 1. Child
/// playback uses tracks 1..18; tracks 0/1 use bank 1 and 2..18 use bank 4.
/// Playback borrows the descriptor and tables; keep the M950 package loaded
/// while any slot or cached record pointer refers to them.
static AnimationSet _gM950PlayerAimBackwardOrTurnAnimation = {
    .records           = _gM950PlayerAimBackwardOrTurnAnimationRecords,
    .trackStartIndices = _gM950PlayerAimBackwardOrTurnAnimationIndices,
    .poseBanks         = {
        [M950_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM950PlayerAimBackwardOrTurnAnimationBank1,
        [M950_ANIMATION_PACKED_ROTATION_BANK]      = _gM950PlayerAimBackwardOrTurnAnimationBank4,
    },
};

/// Reload translations in model units and Euler angles in 1/4096 turns for tracks 0 and 1.
///
/// Read-only clip tables retain their serialized positions in .data;
/// pose banks are word-aligned for playback's word offsets.
static const AnimationPackedPose _gM950PlayerReloadAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m950_animation_0D914_bank1.inc"
};

/// Reload joint rotations for player tracks 2..18, in eight-angle-unit steps.
static const AnimationPackedRotation _gM950PlayerReloadAnimationBank4[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m950_animation_0D914_bank4.inc"
};

/// Reload keyframes and terminal stops, with two full-mask cues on track 1.
///
/// The first `ANIMATION_RECORD_CUE_MASK` cue starts the reload sound; the second
/// applies ammunition and plays the loading sound. Pose offsets count u32
/// words and durations count normal-rate frames.
static const AnimationRecord _gM950PlayerReloadAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_0D914_records.inc"
};

/// Absolute reload record starts for tracks 0..18, followed by a serialized zero halfword.
static const u16 _gM950PlayerReloadAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_0D914_indices.inc"
};
STATIC_ASSERT(ARRAY_SIZE(_gM950PlayerReloadAnimationIndices) == GAME_ACTOR_NORMAL_ANIMATION_SLOTS + 1, m950_reload_track_starts);

/// Player M950 reload clip shared by native ids 14, 15 and 20.
///
/// Primary-load reload selects id 14 and battle-end reload selects id 20.
/// All nineteen tracks stop; child playback uses tracks 1..18, with the two
/// sound/loading cues on track 1. Tracks 0/1 use bank 1 and 2..18 use bank 4.
/// Playback borrows the descriptor and tables; keep the M950 package loaded
/// while any slot or cached record pointer refers to them.
static AnimationSet _gM950PlayerReloadAnimation = {
    .records           = _gM950PlayerReloadAnimationRecords,
    .trackStartIndices = _gM950PlayerReloadAnimationIndices,
    .poseBanks         = {
        [M950_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM950PlayerReloadAnimationBank1,
        [M950_ANIMATION_PACKED_ROTATION_BANK]      = _gM950PlayerReloadAnimationBank4,
    },
};

/// Firing translations in model units and Euler angles in 1/4096 turns for tracks 0 and 1.
///
/// Read-only clip tables retain their serialized positions in .data;
/// pose banks are word-aligned for playback's word offsets.
static const AnimationPackedPose _gM950PlayerFireAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m950_animation_0DE14_bank1.inc"
};

/// Firing joint rotations for player tracks 2..18, in eight-angle-unit steps.
static const AnimationPackedRotation _gM950PlayerFireAnimationBank4[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m950_animation_0DE14_bank4.inc"
};

/// Firing keyframes and terminal stops for nineteen tracks, with no cue flags.
///
/// Pose offsets count u32 words and durations count normal-rate frames.
static const AnimationRecord _gM950PlayerFireAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_0DE14_records.inc"
};

/// Absolute firing record starts for tracks 0..18, followed by a serialized zero halfword.
static const u16 _gM950PlayerFireAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_0DE14_indices.inc"
};
STATIC_ASSERT(ARRAY_SIZE(_gM950PlayerFireAnimationIndices) == GAME_ACTOR_NORMAL_ANIMATION_SLOTS + 1, m950_fire_track_starts);

/// Player M950 firing clip shared by native ids 10 and 11.
///
/// Each shot restarts id 10 on the child tracks with a two-frame blend.
/// All nineteen tracks stop; child playback uses tracks 1..18, with tracks
/// 0/1 in bank 1 and 2..18 in bank 4. Playback borrows the descriptor and tables;
/// keep the M950 package loaded while any slot or cached record pointer refers to them.
static AnimationSet _gM950PlayerFireAnimation = {
    .records           = _gM950PlayerFireAnimationRecords,
    .trackStartIndices = _gM950PlayerFireAnimationIndices,
    .poseBanks         = {
        [M950_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM950PlayerFireAnimationBank1,
        [M950_ANIMATION_PACKED_ROTATION_BANK]      = _gM950PlayerFireAnimationBank4,
    },
};

/// Aim-entry translations in model units and Euler angles in 1/4096 turns for tracks 0 and 1.
///
/// Read-only clip tables retain their serialized positions in .data;
/// pose banks are word-aligned for playback's word offsets.
static const AnimationPackedPose _gM950PlayerAimEntryAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m950_animation_0E304_bank1.inc"
};

/// Aim-entry joint rotations for player tracks 2..18, in eight-angle-unit steps.
static const AnimationPackedRotation _gM950PlayerAimEntryAnimationBank4[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m950_animation_0E304_bank4.inc"
};

/// Aim-entry keyframes and terminal stops for nineteen tracks, with no cue flags.
///
/// Pose offsets count u32 words and durations count normal-rate frames.
static const AnimationRecord _gM950PlayerAimEntryAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_0E304_records.inc"
};

/// Absolute aim-entry record starts for tracks 0..18, followed by a serialized zero halfword.
static const u16 _gM950PlayerAimEntryAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m950_animation_0E304_indices.inc"
};
STATIC_ASSERT(ARRAY_SIZE(_gM950PlayerAimEntryAnimationIndices) == GAME_ACTOR_NORMAL_ANIMATION_SLOTS + 1, m950_aim_entry_track_starts);

/// Player transition into M950 aiming, at native clip id 7.
///
/// All nineteen tracks stop; child playback uses tracks 1..18, with tracks
/// 0/1 in bank 1 and 2..18 in bank 4. The same set is borrowed by enemy-driven
/// player recovery playback. Keep the M950 package loaded while any slot or
/// cached record pointer refers to the descriptor or tables.
static AnimationSet _gM950PlayerAimEntryAnimation = {
    .records           = _gM950PlayerAimEntryAnimationRecords,
    .trackStartIndices = _gM950PlayerAimEntryAnimationIndices,
    .poseBanks         = {
        [M950_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM950PlayerAimEntryAnimationBank1,
        [M950_ANIMATION_PACKED_ROTATION_BANK]      = _gM950PlayerAimEntryAnimationBank4,
    },
};

static AnimationPackedPose _gM950Animation0E890Bank1[10] = {
#include "assets/m950_animation_0E890_bank1.inc"
};

static AnimationPackedRotation _gM950Animation0E890Bank4[136] = {
#include "assets/m950_animation_0E890_bank4.inc"
};

static AnimationRecord _gM950Animation0E890Records[169] = {
#include "assets/m950_animation_0E890_records.inc"
};

static u16 _gM950Animation0E890Indices[20] = {
#include "assets/m950_animation_0E890_indices.inc"
};

static AnimationSet _gM950Animation0E890 = {
    _gM950Animation0E890Records,
    _gM950Animation0E890Indices,
    { NULL, _gM950Animation0E890Bank1, NULL, NULL, _gM950Animation0E890Bank4, NULL, NULL, NULL },
};

AnimationBank D_m950_8012BA78 = { { {
    NULL,
    &_gM950PlayerIdleAnimation,
    &_gM950PlayerWalkAnimation,
    &_gM950PlayerBackwardWalkAnimation,
    &_gM950PlayerRunAnimation,
    &_gM950PlayerTurnLeftAnimation,
    &_gM950PlayerTurnRightAnimation,
    &_gM950PlayerAimEntryAnimation,
    &_gM950Animation0E890,
    &_gM950PlayerAimIdleAnimation,
    &_gM950PlayerFireAnimation,
    &_gM950PlayerFireAnimation,
    &_gM950PlayerAimForwardAnimation,
    &_gM950PlayerAimBackwardOrTurnAnimation,
    &_gM950PlayerReloadAnimation,
    &_gM950PlayerReloadAnimation,
    &_gM950PlayerHitPart4Animation,
    &_gM950PlayerHitOtherBodyAnimation,
    &_gM950PlayerStoppedPoseAnimation,
    &_gM950PlayerUnarmedWalkAnimation,
    &_gM950PlayerReloadAnimation,
    &_gM950PlayerIdleAnimation,
    &_gM950PlayerIdleAnimation,
    &_gM950PlayerHealthyIdleAnimation,
    &_gM950PlayerMidHealthIdleAnimation,
    &_gM950PlayerLowHealthIdleAnimation,
    &_gM950PlayerPeReleaseFamily0Animation,
    &_gM950PlayerPeChargeFamily0Animation,
    &_gM950PlayerPeCastFamily0Animation,
    &_gM950PlayerPeReleaseFamily1Animation,
    &_gM950PlayerPeChargeFamily1Animation,
    &_gM950PlayerPeCastFamily1Animation,
    &_gM950PlayerScriptedPresentationEnterAnimation,
    &_gM950PlayerScriptedPresentationExitAnimation,
    &_gM950PlayerScriptedPresentationEnterAnimation,
    &_gM950PlayerScriptedPresentationExitAnimation,
    &_gM950PlayerStairAscentAnimation,
    &_gM950PlayerStairDescentAnimation,
    &_gM950PlayerStairAscentFinishEvenAnimation,
    &_gM950PlayerStairAscentFinishOddAnimation,
    &_gM950PlayerItemUseAnimation,
    &_gM950PlayerIdleAnimation,
    &_gM950PlayerPeReleaseFamily2Animation,
    &_gM950PlayerPeChargeFamily2Animation,
    &_gM950PlayerPeCastFamily2Animation,
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
