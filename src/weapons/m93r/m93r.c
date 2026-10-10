#include "weapons/m93r.h"

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

/// Encoding indices for the M93R clips' heterogeneous pose-bank table.
enum {
    M93R_ANIMATION_TRANSLATION_ROTATION_BANK = 1,
    M93R_ANIMATION_PACKED_ROTATION_BANK      = 4,
};

/// Initial identity transform and self-parented root of the weapon's sole part.
///
/// Read-only skeleton and count data retain their serialized positions in .data.
static const TmdBone _gM93rWeaponModelSkeleton[] __attribute__((section(".data"))) = {
#include "assets/m93r_model_00520_skeleton.inc"
};

/// Vertex count of the weapon's sole part: all 32 vertices.
static const u32 _gM93rWeaponModelPartVerts[ARRAY_SIZE(_gM93rWeaponModelSkeleton)] __attribute__((section(".data"))) = {
#include "assets/m93r_model_00520_partVerts.inc"
};

/// Writable weapon vertices in integer coordinates local to the hand anchor.
static SVECTOR _gM93rWeaponModelVerts[] = {
#include "assets/m93r_model_00520_verts.inc"
};

/// Writable weapon lighting normals with twelve fractional bits, indexed independently of vertices.
static SVECTOR _gM93rWeaponModelNormals[] = {
#include "assets/m93r_model_00520_normals.inc"
};

/// Writable commands for four textured triangles and 28 textured quads, ending in `TMD_STREAM_END`.
///
/// First model creation resolves the commands' draw-handler slots in place.
static u32 _gM93rWeaponModelStream[] = {
#include "assets/m93r_model_00520_stream.inc"
};

TmdSource gM93rWeaponModel = {
    .handlersResolved    = false,
    .bufferHalfBytes     = (4 * sizeof(POLY_GT3)) + (28 * sizeof(POLY_GT4)),
    .preXformRegionBytes = 0,
    .partCount           = ARRAY_SIZE(_gM93rWeaponModelSkeleton),
    .partVertexCounts    = _gM93rWeaponModelPartVerts,
    .verts               = _gM93rWeaponModelVerts,
    .normals             = _gM93rWeaponModelNormals,
    .skeleton            = _gM93rWeaponModelSkeleton,
    .stream              = _gM93rWeaponModelStream,
};

/// Idle translations in model units and Euler rotations in 1/4096 turns for tracks 0 and 1.
///
/// Read-only animation tables retain their serialized positions in .data.
static const AnimationPackedPose _gM93rPlayerIdleAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/m93r_animation_00A68_bank1.inc"
};

/// Idle Euler rotations in eight-angle-unit steps for player tracks 2..18.
static const AnimationPackedRotation _gM93rPlayerIdleAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m93r_animation_00A68_bank4.inc"
};

/// Idle keyframes and controls; durations are normal-rate frames and pose offsets count u32 words.
static const AnimationRecord _gM93rPlayerIdleAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m93r_animation_00A68_records.inc"
};

/// Absolute idle record starts for player tracks 0..18, followed by the retained serialized halfword.
static const u16 _gM93rPlayerIdleAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m93r_animation_00A68_indices.inc"
};

/// Standing idle for the player using the M93R animation bank.
///
/// Native clip 1; bank entries 11, 15, 21, 22 and 41 also select this pose.
/// Contains 19 player-part tracks, with translation/rotation for tracks 0 and 1
/// and packed rotation for tracks 2..18. Playback borrows the set and tables;
/// the M93R package must remain loaded while any slot refers to them.
static AnimationSet _gM93rPlayerIdleAnimation = {
    .records           = _gM93rPlayerIdleAnimationRecords,
    .trackStartIndices = _gM93rPlayerIdleAnimationIndices,
    .poseBanks         = { NULL, _gM93rPlayerIdleAnimationBank1, NULL, NULL, _gM93rPlayerIdleAnimationBank4, NULL, NULL, NULL },
};

/// Unarmed-walk translations in model units and Euler rotations in 1/4096 turns for tracks 0 and 1.
static const AnimationPackedPose _gM93rPlayerUnarmedWalkAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/m93r_animation_0110C_bank1.inc"
};

/// Unarmed-walk Euler rotations in eight-angle-unit steps for player tracks 2..18.
static const AnimationPackedRotation _gM93rPlayerUnarmedWalkAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m93r_animation_0110C_bank4.inc"
};

/// Unarmed-walk keyframes and controls; durations are normal-rate frames and pose offsets count u32 words.
static const AnimationRecord _gM93rPlayerUnarmedWalkAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m93r_animation_0110C_records.inc"
};

/// Absolute unarmed-walk record starts for player tracks 0..18, followed by the retained serialized halfword.
static const u16 _gM93rPlayerUnarmedWalkAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m93r_animation_0110C_indices.inc"
};

/// Forward walk while the player has no attached weapon model, in the M93R bank.
///
/// Native clip 19. Contains 19 player-part tracks, with translation/rotation
/// for tracks 0 and 1 and packed rotation for tracks 2..18. Playback borrows
/// the set and tables; the M93R package must remain loaded during those uses.
static AnimationSet _gM93rPlayerUnarmedWalkAnimation = {
    .records           = _gM93rPlayerUnarmedWalkAnimationRecords,
    .trackStartIndices = _gM93rPlayerUnarmedWalkAnimationIndices,
    .poseBanks         = { NULL, _gM93rPlayerUnarmedWalkAnimationBank1, NULL, NULL, _gM93rPlayerUnarmedWalkAnimationBank4, NULL, NULL, NULL },
};

/// Left-turn translations in model units and Euler rotations in 1/4096 turns for tracks 0 and 1.
static const AnimationPackedPose _gM93rPlayerTurnLeftAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/m93r_animation_0196C_bank1.inc"
};

/// Left-turn Euler rotations in eight-angle-unit steps for player tracks 2..18.
static const AnimationPackedRotation _gM93rPlayerTurnLeftAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m93r_animation_0196C_bank4.inc"
};

/// Left-turn keyframes and controls; durations are normal-rate frames and pose offsets count u32 words.
static const AnimationRecord _gM93rPlayerTurnLeftAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m93r_animation_0196C_records.inc"
};

/// Absolute left-turn record starts for player tracks 0..18, followed by the retained serialized halfword.
static const u16 _gM93rPlayerTurnLeftAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m93r_animation_0196C_indices.inc"
};

/// Stationary left turn for the player using the M93R animation bank.
///
/// Native clip 5, also used for a scripted turn toward a negative yaw delta.
/// Contains 19 player-part tracks, with translation/rotation for tracks 0 and 1
/// and packed rotation for tracks 2..18. Playback borrows the set and tables;
/// the M93R package must remain loaded while any slot refers to them.
static AnimationSet _gM93rPlayerTurnLeftAnimation = {
    .records           = _gM93rPlayerTurnLeftAnimationRecords,
    .trackStartIndices = _gM93rPlayerTurnLeftAnimationIndices,
    .poseBanks         = { NULL, _gM93rPlayerTurnLeftAnimationBank1, NULL, NULL, _gM93rPlayerTurnLeftAnimationBank4, NULL, NULL, NULL },
};

/// Right-turn translations in model units and Euler rotations in 1/4096 turns for tracks 0 and 1.
static const AnimationPackedPose _gM93rPlayerTurnRightAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/m93r_animation_021D0_bank1.inc"
};

/// Right-turn Euler rotations in eight-angle-unit steps for player tracks 2..18.
static const AnimationPackedRotation _gM93rPlayerTurnRightAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m93r_animation_021D0_bank4.inc"
};

/// Right-turn keyframes and controls; durations are normal-rate frames and pose offsets count u32 words.
static const AnimationRecord _gM93rPlayerTurnRightAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m93r_animation_021D0_records.inc"
};

/// Absolute right-turn record starts for player tracks 0..18, followed by the retained serialized halfword.
static const u16 _gM93rPlayerTurnRightAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m93r_animation_021D0_indices.inc"
};

/// Stationary right turn for the player using the M93R animation bank.
///
/// Native clip 6, also used for a scripted turn toward a nonnegative yaw delta.
/// Contains 19 player-part tracks, with translation/rotation for tracks 0 and 1
/// and packed rotation for tracks 2..18. Playback borrows the set and tables;
/// the M93R package must remain loaded while any slot refers to them.
static AnimationSet _gM93rPlayerTurnRightAnimation = {
    .records           = _gM93rPlayerTurnRightAnimationRecords,
    .trackStartIndices = _gM93rPlayerTurnRightAnimationIndices,
    .poseBanks         = { NULL, _gM93rPlayerTurnRightAnimationBank1, NULL, NULL, _gM93rPlayerTurnRightAnimationBank4, NULL, NULL, NULL },
};

/// Item-use translations in model units and Euler rotations in 1/4096 turns for tracks 0 and 1.
static const AnimationPackedPose _gM93rPlayerItemUseAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/m93r_animation_024E4_bank1.inc"
};

/// Item-use Euler rotations in eight-angle-unit steps for player tracks 2..18.
static const AnimationPackedRotation _gM93rPlayerItemUseAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m93r_animation_024E4_bank4.inc"
};

/// Item-use keyframes and controls; durations are normal-rate frames and pose offsets count u32 words.
static const AnimationRecord _gM93rPlayerItemUseAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m93r_animation_024E4_records.inc"
};

/// Absolute item-use record starts for player tracks 0..18, followed by the retained serialized halfword.
static const u16 _gM93rPlayerItemUseAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m93r_animation_024E4_indices.inc"
};

/// Item-use presentation for the player using the M93R animation bank.
///
/// Native clip 40. Contains 19 player-part tracks, with translation/rotation
/// for tracks 0 and 1 and packed rotation for tracks 2..18. Playback borrows
/// the set and tables; the M93R package must remain loaded during those uses.
static AnimationSet _gM93rPlayerItemUseAnimation = {
    .records           = _gM93rPlayerItemUseAnimationRecords,
    .trackStartIndices = _gM93rPlayerItemUseAnimationIndices,
    .poseBanks         = { NULL, _gM93rPlayerItemUseAnimationBank1, NULL, NULL, _gM93rPlayerItemUseAnimationBank4, NULL, NULL, NULL },
};

void m93rAttackState(Task* playerTask)
{
    enum {
        M93R_PHASE_PREPARE             = 0,
        M93R_PHASE_WAIT_READY          = 1,
        M93R_PHASE_BURST               = 2,
        M93R_PHASE_RECOVER             = 3,
        M93R_PLAYER_ATTACK_STATE       = 4,
        M93R_ANIMATION_READY           = 9,
        M93R_ANIMATION_PRIMARY         = 0xA,
        M93R_READY_BLEND_FRAMES        = 1,
        M93R_MOVING_READY_BLEND_FRAMES = 6,
        M93R_IMPACT_SOUND              = SOUND_COMMON(0x17),
        M93R_BURST_ROUNDS              = 3,
        M93R_BURST_DELAY_FRAMES        = 1,
        M93R_CANCEL_FRAMES             = 0xA,
        M93R_HELD_COOLDOWN_FRAMES      = 0xA,
        M93R_WEAPON_ID                 = 2,
        M93R_FIRE_SOUND                = SOUND_WEAPON(2, 4),
    };
    GameActor* actor;
    GfxCoord*  rootCoord;
    GfxCoord*  impactCoord;
    s32        readyBlendFrames;
    s32        burstDelay;
    s16        burstRounds;
    s32        cancelOnHeldInput;

    impactCoord       = SCRATCH_STACK_RESERVE_BLOCK(GfxCoord);
    actor             = playerTask->work;
    rootCoord         = playerTask->extra.tmd->coords;
    cancelOnHeldInput = 0;
    switch (actor->statePhase) {
        case M93R_PHASE_PREPARE:
            actor->state             = M93R_PLAYER_ATTACK_STATE;
            actor->mode              = GAME_ACTOR_MODE_NORMAL;
            actor->turnRateIndex     = 0;
            actor->animationState    = 0;
            actor->statePhase        = M93R_PHASE_WAIT_READY;
            actor->rumblePosted      = 0;
            actor->stateTimer        = 0;
            actor->attackCancelTicks = M93R_CANCEL_FRAMES;
            burstRounds              = 1;
            if (actor->attackButton == PLAYER_ACTOR_ATTACK_BUTTON_PRIMARY) {
                burstRounds = M93R_BURST_ROUNDS;
            }
            actor->actionValue = burstRounds;
            playerActorSetWeaponAttackFlags(playerTask, 0, actor->attackButton == PLAYER_ACTOR_ATTACK_BUTTON_PRIMARY);
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= (WORLD_COLLISION_BODY_CLIP_TO_GRID_CONTACT | WORLD_COLLISION_BODY_SINGLE_CONTACT);
            readyBlendFrames                                      = M93R_READY_BLEND_FRAMES;
            if (((u16)actor->movementMode | actor->turnSign) != 0) {
                readyBlendFrames = M93R_MOVING_READY_BLEND_FRAMES;
            }
            playerActorPlayChildSlotsWithBlend(playerTask, M93R_ANIMATION_READY, 0, readyBlendFrames);
            actor->movementMode = 0;
            break;
        case M93R_PHASE_WAIT_READY:
            if (animationGetCurrentRecord(&actor->animationContext, actor->animationSlots + 1) !=
                NULL) {
                actor->statePhase++;
            }
            break;
        case M93R_PHASE_BURST:
            // Keep contacts active for one frame between successive burst rounds.
            if (actor->actionValue != 0) {
                burstDelay = actor->stateTimer;
                if (burstDelay == 0) {
                    actor->actionValue--;
                    actor->stateTimer                                     = M93R_BURST_DELAY_FRAMES;
                    actor->rumblePosted                                   = 0;
                    actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
                    equipmentConsumeWeaponLoad(WEAPON_ITEM(M93R_WEAPON_ID), EQUIPMENT_WEAPON_LOAD_CONSUME_PRIMARY);
                    if (playerActorQueryWeaponLoads(PLAYER_ACTOR_WEAPON_LOAD_PRIMARY) == 0) {
                        actor->actionValue = 0;
                    }
                    worldCoordPlaySound(playerTask->extra.tmd->coords, M93R_FIRE_SOUND, 1);
                    effectSpawn(EFFECT_HANDGUN_MUZZLE_FLASH,
                                actor->equipmentTasks[1]->extra.tmd->coords, M93R_WEAPON_ID,
                                NULL);
                    playerActorPlayChildSlotsWithBlend(playerTask, M93R_ANIMATION_PRIMARY, 1, 2);
                    break;
                }
                burstDelay--;
                actor->stateTimer = burstDelay;
                if (burstDelay == 0) {
                    actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
                    if (playerActorSpawnWeaponImpact(actor->weaponContacts, rootCoord, impactCoord) != 0) {
                        worldCoordPlaySound(impactCoord, M93R_IMPACT_SOUND, 1);
                    }
                }
            } else {
                actor->statePhase                                     = M93R_PHASE_RECOVER;
                actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
                if (playerActorSpawnWeaponImpact(actor->weaponContacts, rootCoord, impactCoord) != 0) {
                    worldCoordPlaySound(impactCoord, M93R_IMPACT_SOUND, 1);
                }
            }
            break;
        case M93R_PHASE_RECOVER:
            if (actor->attackCancelTicks != 0) {
                actor->attackCancelTicks--;
            }
            if ((actor->padHeld & actor->actionPadMask) != 0) {
                cancelOnHeldInput = actor->attackCancelTicks == 0;
            }
            if (playerActorIsSlotAdvancingLinearly(playerTask, D_80112E04[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId][1], 0, 0) == 0 || cancelOnHeldInput) {
                if (cancelOnHeldInput) {
                    actor->attackControl.cooldownTicks = M93R_HELD_COOLDOWN_FRAMES;
                } else {
                    actor->attackControl.cooldownTicks = 0;
                }
                playerActorFinishWeaponAttack(playerTask);
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BLOCK(GfxCoord);
}

/// Stair-ascent translations in model units and Euler rotations in 1/4096 turns for tracks 0 and 1.
///
/// Read-only animation tables retain their serialized positions in .data.
static const AnimationPackedPose _gM93rPlayerStairAscentAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/m93r_animation_02C40_bank1.inc"
};

/// Stair-ascent Euler rotations in eight-angle-unit steps for player tracks 2..18.
static const AnimationPackedRotation _gM93rPlayerStairAscentAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m93r_animation_02C40_bank4.inc"
};

/// Stair-ascent keyframes and loop controls; durations are normal-rate frames and pose offsets count u32 words.
static const AnimationRecord _gM93rPlayerStairAscentAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m93r_animation_02C40_records.inc"
};

/// Absolute stair-ascent record starts for player tracks 0..18, followed by the retained serialized halfword.
static const u16 _gM93rPlayerStairAscentAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m93r_animation_02C40_indices.inc"
};

/// Looping stair-ascent stride for the player using the M93R animation bank.
///
/// Native clip 36; its footstep cues gate displacement along the stair flight.
/// Contains 19 part tracks: translation/rotation for 0..1 and packed rotation
/// for 2..18. Playback borrows the clip and tables; keep the M93R package loaded
/// while any slot or cached record pointer refers to them.
static AnimationSet _gM93rPlayerStairAscentAnimation = {
    .records           = _gM93rPlayerStairAscentAnimationRecords,
    .trackStartIndices = _gM93rPlayerStairAscentAnimationIndices,
    .poseBanks         = {
        [M93R_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM93rPlayerStairAscentAnimationBank1,
        [M93R_ANIMATION_PACKED_ROTATION_BANK]      = _gM93rPlayerStairAscentAnimationBank4,
    },
};

/// Stair-descent translations in model units and Euler rotations in 1/4096 turns for tracks 0 and 1.
static const AnimationPackedPose _gM93rPlayerStairDescentAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/m93r_animation_033C8_bank1.inc"
};

/// Stair-descent Euler rotations in eight-angle-unit steps for player tracks 2..18.
static const AnimationPackedRotation _gM93rPlayerStairDescentAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m93r_animation_033C8_bank4.inc"
};

/// Stair-descent keyframes and loop controls; durations are normal-rate frames and pose offsets count u32 words.
static const AnimationRecord _gM93rPlayerStairDescentAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m93r_animation_033C8_records.inc"
};

/// Absolute stair-descent record starts for player tracks 0..18, followed by the retained serialized halfword.
static const u16 _gM93rPlayerStairDescentAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m93r_animation_033C8_indices.inc"
};

/// Looping stair-descent stride for the player using the M93R animation bank.
///
/// Native clip 37; its footstep cues gate displacement along the stair flight.
/// Contains 19 part tracks: translation/rotation for 0..1 and packed rotation
/// for 2..18. Playback borrows the clip and tables; keep the M93R package loaded
/// while any slot or cached record pointer refers to them.
static AnimationSet _gM93rPlayerStairDescentAnimation = {
    .records           = _gM93rPlayerStairDescentAnimationRecords,
    .trackStartIndices = _gM93rPlayerStairDescentAnimationIndices,
    .poseBanks         = {
        [M93R_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM93rPlayerStairDescentAnimationBank1,
        [M93R_ANIMATION_PACKED_ROTATION_BANK]      = _gM93rPlayerStairDescentAnimationBank4,
    },
};

/// Odd-count stair-finish translations in model units and Euler rotations in 1/4096 turns for tracks 0 and 1.
static const AnimationPackedPose _gM93rPlayerStairAscentFinishOddAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/m93r_animation_0369C_bank1.inc"
};

/// Odd-count stair-finish Euler rotations in eight-angle-unit steps for player tracks 2..18.
static const AnimationPackedRotation _gM93rPlayerStairAscentFinishOddAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m93r_animation_0369C_bank4.inc"
};

/// Odd-count stair-finish keyframes and stops; durations are normal-rate frames and pose offsets count u32 words.
static const AnimationRecord _gM93rPlayerStairAscentFinishOddAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m93r_animation_0369C_records.inc"
};

/// Absolute odd-count stair-finish record starts for tracks 0..18, followed by the retained serialized halfword.
static const u16 _gM93rPlayerStairAscentFinishOddAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m93r_animation_0369C_indices.inc"
};

/// Finishing stride after an odd-count stair ascent in the player's M93R bank.
///
/// Native clip 39, selected by the initial stair count's parity. Each of its
/// 19 part tracks ends with a stop; tracks 0..1 carry translation/rotation and
/// 2..18 packed rotation. Playback borrows the clip and tables; keep the M93R
/// package loaded while any slot or cached record pointer refers to them.
static AnimationSet _gM93rPlayerStairAscentFinishOddAnimation = {
    .records           = _gM93rPlayerStairAscentFinishOddAnimationRecords,
    .trackStartIndices = _gM93rPlayerStairAscentFinishOddAnimationIndices,
    .poseBanks         = {
        [M93R_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM93rPlayerStairAscentFinishOddAnimationBank1,
        [M93R_ANIMATION_PACKED_ROTATION_BANK]      = _gM93rPlayerStairAscentFinishOddAnimationBank4,
    },
};

/// Even-count stair-finish translations in model units and Euler rotations in 1/4096 turns for tracks 0 and 1.
static const AnimationPackedPose _gM93rPlayerStairAscentFinishEvenAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/m93r_animation_03A2C_bank1.inc"
};

/// Even-count stair-finish Euler rotations in eight-angle-unit steps for player tracks 2..18.
static const AnimationPackedRotation _gM93rPlayerStairAscentFinishEvenAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m93r_animation_03A2C_bank4.inc"
};

/// Even-count stair-finish keyframes and stops; durations are normal-rate frames and pose offsets count u32 words.
static const AnimationRecord _gM93rPlayerStairAscentFinishEvenAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m93r_animation_03A2C_records.inc"
};

/// Absolute even-count stair-finish record starts for tracks 0..18, followed by the retained serialized halfword.
static const u16 _gM93rPlayerStairAscentFinishEvenAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m93r_animation_03A2C_indices.inc"
};

/// Finishing stride after an even-count stair ascent in the player's M93R bank.
///
/// Native clip 38, selected by the initial stair count's parity. Each of its
/// 19 part tracks ends with a stop; tracks 0..1 carry translation/rotation and
/// 2..18 packed rotation. Playback borrows the clip and tables; keep the M93R
/// package loaded while any slot or cached record pointer refers to them.
static AnimationSet _gM93rPlayerStairAscentFinishEvenAnimation = {
    .records           = _gM93rPlayerStairAscentFinishEvenAnimationRecords,
    .trackStartIndices = _gM93rPlayerStairAscentFinishEvenAnimationIndices,
    .poseBanks         = {
        [M93R_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM93rPlayerStairAscentFinishEvenAnimationBank1,
        [M93R_ANIMATION_PACKED_ROTATION_BANK]      = _gM93rPlayerStairAscentFinishEvenAnimationBank4,
    },
};

/// PE family-0 release translations in model units and Euler rotations in 1/4096 turns for tracks 0 and 1.
static const AnimationPackedPose _gM93rPlayerParasiteEnergyReleaseFamily0AnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/m93r_animation_03EB4_bank1.inc"
};

/// PE family-0 release Euler rotations in eight-angle-unit steps for player tracks 2..18.
static const AnimationPackedRotation _gM93rPlayerParasiteEnergyReleaseFamily0AnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m93r_animation_03EB4_bank4.inc"
};

/// PE family-0 release keyframes and stops; durations are normal-rate frames and pose offsets count u32 words.
static const AnimationRecord _gM93rPlayerParasiteEnergyReleaseFamily0AnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m93r_animation_03EB4_records.inc"
};

/// Absolute PE family-0 release record starts for tracks 0..18, followed by the retained serialized halfword.
static const u16 _gM93rPlayerParasiteEnergyReleaseFamily0AnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m93r_animation_03EB4_indices.inc"
};

/// Parasite Energy release for clip family 0 in the player's M93R bank.
///
/// Native clip 26; plays while the attachment's charge countdown starts.
/// Family 0 is selected by attachment ID. Contains 19 part tracks, with
/// translation/rotation for 0..1 and packed rotation for 2..18, ending in stops.
/// Playback borrows the clip and tables; keep the M93R package loaded while
/// any slot or cached record pointer refers to them.
static AnimationSet _gM93rPlayerParasiteEnergyReleaseFamily0Animation = {
    .records           = _gM93rPlayerParasiteEnergyReleaseFamily0AnimationRecords,
    .trackStartIndices = _gM93rPlayerParasiteEnergyReleaseFamily0AnimationIndices,
    .poseBanks         = {
        [M93R_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM93rPlayerParasiteEnergyReleaseFamily0AnimationBank1,
        [M93R_ANIMATION_PACKED_ROTATION_BANK]      = _gM93rPlayerParasiteEnergyReleaseFamily0AnimationBank4,
    },
};

/// PE family-0 charge translations in model units and Euler rotations in 1/4096 turns for tracks 0 and 1.
static const AnimationPackedPose _gM93rPlayerParasiteEnergyChargeFamily0AnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/m93r_animation_040B0_bank1.inc"
};

/// PE family-0 charge Euler rotations in eight-angle-unit steps for player tracks 2..18.
static const AnimationPackedRotation _gM93rPlayerParasiteEnergyChargeFamily0AnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m93r_animation_040B0_bank4.inc"
};

/// PE family-0 charge keyframes and loop controls; durations are normal-rate frames and pose offsets count u32 words.
static const AnimationRecord _gM93rPlayerParasiteEnergyChargeFamily0AnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m93r_animation_040B0_records.inc"
};

/// Absolute PE family-0 charge record starts for tracks 0..18, followed by the retained serialized halfword.
static const u16 _gM93rPlayerParasiteEnergyChargeFamily0AnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m93r_animation_040B0_indices.inc"
};

/// Looping Parasite Energy charge pose for clip family 0 in the player's M93R bank.
///
/// Native clip 27; follows the release clip until the attachment countdown
/// reaches zero, then yields to casting. Family 0 is selected by attachment ID.
/// Contains 19 part tracks: translation/rotation for 0..1 and packed rotation
/// for 2..18. Playback borrows the clip and tables; keep the M93R package loaded
/// while any slot or cached record pointer refers to them.
static AnimationSet _gM93rPlayerParasiteEnergyChargeFamily0Animation = {
    .records           = _gM93rPlayerParasiteEnergyChargeFamily0AnimationRecords,
    .trackStartIndices = _gM93rPlayerParasiteEnergyChargeFamily0AnimationIndices,
    .poseBanks         = {
        [M93R_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM93rPlayerParasiteEnergyChargeFamily0AnimationBank1,
        [M93R_ANIMATION_PACKED_ROTATION_BANK]      = _gM93rPlayerParasiteEnergyChargeFamily0AnimationBank4,
    },
};

static AnimationPackedPose _gM93rAnimation04388Bank1[6] = {
#include "assets/m93r_animation_04388_bank1.inc"
};

static AnimationPackedRotation _gM93rAnimation04388Bank4[57] = {
#include "assets/m93r_animation_04388_bank4.inc"
};

static AnimationRecord _gM93rAnimation04388Records[87] = {
#include "assets/m93r_animation_04388_records.inc"
};

static u16 _gM93rAnimation04388Indices[20] = {
#include "assets/m93r_animation_04388_indices.inc"
};

static AnimationSet _gM93rAnimation04388 = {
    _gM93rAnimation04388Records,
    _gM93rAnimation04388Indices,
    { NULL, _gM93rAnimation04388Bank1, NULL, NULL, _gM93rAnimation04388Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM93rAnimation0462CBank1[4] = {
#include "assets/m93r_animation_0462C_bank1.inc"
};

static AnimationPackedRotation _gM93rAnimation0462CBank4[55] = {
#include "assets/m93r_animation_0462C_bank4.inc"
};

static AnimationRecord _gM93rAnimation0462CRecords[82] = {
#include "assets/m93r_animation_0462C_records.inc"
};

static u16 _gM93rAnimation0462CIndices[20] = {
#include "assets/m93r_animation_0462C_indices.inc"
};

static AnimationSet _gM93rAnimation0462C = {
    _gM93rAnimation0462CRecords,
    _gM93rAnimation0462CIndices,
    { NULL, _gM93rAnimation0462CBank1, NULL, NULL, _gM93rAnimation0462CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM93rAnimation0482CBank1[3] = {
#include "assets/m93r_animation_0482C_bank1.inc"
};

static AnimationPackedRotation _gM93rAnimation0482CBank4[23] = {
#include "assets/m93r_animation_0482C_bank4.inc"
};

static AnimationRecord _gM93rAnimation0482CRecords[76] = {
#include "assets/m93r_animation_0482C_records.inc"
};

static u16 _gM93rAnimation0482CIndices[20] = {
#include "assets/m93r_animation_0482C_indices.inc"
};

static AnimationSet _gM93rAnimation0482C = {
    _gM93rAnimation0482CRecords,
    _gM93rAnimation0482CIndices,
    { NULL, _gM93rAnimation0482CBank1, NULL, NULL, _gM93rAnimation0482CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM93rAnimation04B80Bank1[8] = {
#include "assets/m93r_animation_04B80_bank1.inc"
};

static AnimationPackedRotation _gM93rAnimation04B80Bank4[68] = {
#include "assets/m93r_animation_04B80_bank4.inc"
};

static AnimationRecord _gM93rAnimation04B80Records[101] = {
#include "assets/m93r_animation_04B80_records.inc"
};

static u16 _gM93rAnimation04B80Indices[20] = {
#include "assets/m93r_animation_04B80_indices.inc"
};

static AnimationSet _gM93rAnimation04B80 = {
    _gM93rAnimation04B80Records,
    _gM93rAnimation04B80Indices,
    { NULL, _gM93rAnimation04B80Bank1, NULL, NULL, _gM93rAnimation04B80Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM93rAnimation04E34Bank1[5] = {
#include "assets/m93r_animation_04E34_bank1.inc"
};

static AnimationPackedRotation _gM93rAnimation04E34Bank4[55] = {
#include "assets/m93r_animation_04E34_bank4.inc"
};

static AnimationRecord _gM93rAnimation04E34Records[83] = {
#include "assets/m93r_animation_04E34_records.inc"
};

static u16 _gM93rAnimation04E34Indices[20] = {
#include "assets/m93r_animation_04E34_indices.inc"
};

static AnimationSet _gM93rAnimation04E34 = {
    _gM93rAnimation04E34Records,
    _gM93rAnimation04E34Indices,
    { NULL, _gM93rAnimation04E34Bank1, NULL, NULL, _gM93rAnimation04E34Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM93rAnimation05154Bank1[6] = {
#include "assets/m93r_animation_05154_bank1.inc"
};

static AnimationPackedRotation _gM93rAnimation05154Bank4[66] = {
#include "assets/m93r_animation_05154_bank4.inc"
};

static AnimationRecord _gM93rAnimation05154Records[96] = {
#include "assets/m93r_animation_05154_records.inc"
};

static u16 _gM93rAnimation05154Indices[20] = {
#include "assets/m93r_animation_05154_indices.inc"
};

static AnimationSet _gM93rAnimation05154 = {
    _gM93rAnimation05154Records,
    _gM93rAnimation05154Indices,
    { NULL, _gM93rAnimation05154Bank1, NULL, NULL, _gM93rAnimation05154Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM93rAnimation05918Bank1[18] = {
#include "assets/m93r_animation_05918_bank1.inc"
};

static AnimationPackedRotation _gM93rAnimation05918Bank4[184] = {
#include "assets/m93r_animation_05918_bank4.inc"
};

static AnimationRecord _gM93rAnimation05918Records[239] = {
#include "assets/m93r_animation_05918_records.inc"
};

static u16 _gM93rAnimation05918Indices[20] = {
#include "assets/m93r_animation_05918_indices.inc"
};

static AnimationSet _gM93rAnimation05918 = {
    _gM93rAnimation05918Records,
    _gM93rAnimation05918Indices,
    { NULL, _gM93rAnimation05918Bank1, NULL, NULL, _gM93rAnimation05918Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM93rAnimation06BB0Bank1[29] = {
#include "assets/m93r_animation_06BB0_bank1.inc"
};

static AnimationPackedRotation _gM93rAnimation06BB0Bank4[450] = {
#include "assets/m93r_animation_06BB0_bank4.inc"
};

static AnimationRecord _gM93rAnimation06BB0Records[633] = {
#include "assets/m93r_animation_06BB0_records.inc"
};

static u16 _gM93rAnimation06BB0Indices[20] = {
#include "assets/m93r_animation_06BB0_indices.inc"
};

static AnimationSet _gM93rAnimation06BB0 = {
    _gM93rAnimation06BB0Records,
    _gM93rAnimation06BB0Indices,
    { NULL, _gM93rAnimation06BB0Bank1, NULL, NULL, _gM93rAnimation06BB0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM93rAnimation07728Bank1[12] = {
#include "assets/m93r_animation_07728_bank1.inc"
};

static AnimationPackedRotation _gM93rAnimation07728Bank4[266] = {
#include "assets/m93r_animation_07728_bank4.inc"
};

static AnimationRecord _gM93rAnimation07728Records[412] = {
#include "assets/m93r_animation_07728_records.inc"
};

static u16 _gM93rAnimation07728Indices[20] = {
#include "assets/m93r_animation_07728_indices.inc"
};

static AnimationSet _gM93rAnimation07728 = {
    _gM93rAnimation07728Records,
    _gM93rAnimation07728Indices,
    { NULL, _gM93rAnimation07728Bank1, NULL, NULL, _gM93rAnimation07728Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM93rAnimation07E44Bank1[9] = {
#include "assets/m93r_animation_07E44_bank1.inc"
};

static AnimationPackedRotation _gM93rAnimation07E44Bank4[144] = {
#include "assets/m93r_animation_07E44_bank4.inc"
};

static AnimationRecord _gM93rAnimation07E44Records[264] = {
#include "assets/m93r_animation_07E44_records.inc"
};

static u16 _gM93rAnimation07E44Indices[20] = {
#include "assets/m93r_animation_07E44_indices.inc"
};

static AnimationSet _gM93rAnimation07E44 = {
    _gM93rAnimation07E44Records,
    _gM93rAnimation07E44Indices,
    { NULL, _gM93rAnimation07E44Bank1, NULL, NULL, _gM93rAnimation07E44Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM93rAnimation082C4Bank1[6] = {
#include "assets/m93r_animation_082C4_bank1.inc"
};

static AnimationPackedRotation _gM93rAnimation082C4Bank4[107] = {
#include "assets/m93r_animation_082C4_bank4.inc"
};

static AnimationRecord _gM93rAnimation082C4Records[143] = {
#include "assets/m93r_animation_082C4_records.inc"
};

static u16 _gM93rAnimation082C4Indices[20] = {
#include "assets/m93r_animation_082C4_indices.inc"
};

static AnimationSet _gM93rAnimation082C4 = {
    _gM93rAnimation082C4Records,
    _gM93rAnimation082C4Indices,
    { NULL, _gM93rAnimation082C4Bank1, NULL, NULL, _gM93rAnimation082C4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM93rAnimation0849CBank1[3] = {
#include "assets/m93r_animation_0849C_bank1.inc"
};

static AnimationPackedRotation _gM93rAnimation0849CBank4[32] = {
#include "assets/m93r_animation_0849C_bank4.inc"
};

static AnimationRecord _gM93rAnimation0849CRecords[57] = {
#include "assets/m93r_animation_0849C_records.inc"
};

static u16 _gM93rAnimation0849CIndices[20] = {
#include "assets/m93r_animation_0849C_indices.inc"
};

static AnimationSet _gM93rAnimation0849C = {
    _gM93rAnimation0849CRecords,
    _gM93rAnimation0849CIndices,
    { NULL, _gM93rAnimation0849CBank1, NULL, NULL, _gM93rAnimation0849CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM93rAnimation08A00Bank1[11] = {
#include "assets/m93r_animation_08A00_bank1.inc"
};

static AnimationPackedRotation _gM93rAnimation08A00Bank4[125] = {
#include "assets/m93r_animation_08A00_bank4.inc"
};

static AnimationRecord _gM93rAnimation08A00Records[167] = {
#include "assets/m93r_animation_08A00_records.inc"
};

static u16 _gM93rAnimation08A00Indices[20] = {
#include "assets/m93r_animation_08A00_indices.inc"
};

static AnimationSet _gM93rAnimation08A00 = {
    _gM93rAnimation08A00Records,
    _gM93rAnimation08A00Indices,
    { NULL, _gM93rAnimation08A00Bank1, NULL, NULL, _gM93rAnimation08A00Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM93rAnimation08BF4Bank1[3] = {
#include "assets/m93r_animation_08BF4_bank1.inc"
};

static AnimationPackedRotation _gM93rAnimation08BF4Bank4[20] = {
#include "assets/m93r_animation_08BF4_bank4.inc"
};

static AnimationRecord _gM93rAnimation08BF4Records[76] = {
#include "assets/m93r_animation_08BF4_records.inc"
};

static u16 _gM93rAnimation08BF4Indices[20] = {
#include "assets/m93r_animation_08BF4_indices.inc"
};

static AnimationSet _gM93rAnimation08BF4 = {
    _gM93rAnimation08BF4Records,
    _gM93rAnimation08BF4Indices,
    { NULL, _gM93rAnimation08BF4Bank1, NULL, NULL, _gM93rAnimation08BF4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM93rAnimation09074Bank1[8] = {
#include "assets/m93r_animation_09074_bank1.inc"
};

static AnimationPackedRotation _gM93rAnimation09074Bank4[105] = {
#include "assets/m93r_animation_09074_bank4.inc"
};

static AnimationRecord _gM93rAnimation09074Records[139] = {
#include "assets/m93r_animation_09074_records.inc"
};

static u16 _gM93rAnimation09074Indices[20] = {
#include "assets/m93r_animation_09074_indices.inc"
};

static AnimationSet _gM93rAnimation09074 = {
    _gM93rAnimation09074Records,
    _gM93rAnimation09074Indices,
    { NULL, _gM93rAnimation09074Bank1, NULL, NULL, _gM93rAnimation09074Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM93rAnimation09250Bank1[2] = {
#include "assets/m93r_animation_09250_bank1.inc"
};

static AnimationPackedRotation _gM93rAnimation09250Bank4[17] = {
#include "assets/m93r_animation_09250_bank4.inc"
};

static AnimationRecord _gM93rAnimation09250Records[76] = {
#include "assets/m93r_animation_09250_records.inc"
};

static u16 _gM93rAnimation09250Indices[20] = {
#include "assets/m93r_animation_09250_indices.inc"
};

static AnimationSet _gM93rAnimation09250 = {
    _gM93rAnimation09250Records,
    _gM93rAnimation09250Indices,
    { NULL, _gM93rAnimation09250Bank1, NULL, NULL, _gM93rAnimation09250Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM93rAnimation09900Bank1[11] = {
#include "assets/m93r_animation_09900_bank1.inc"
};

static AnimationPackedRotation _gM93rAnimation09900Bank4[151] = {
#include "assets/m93r_animation_09900_bank4.inc"
};

static AnimationRecord _gM93rAnimation09900Records[224] = {
#include "assets/m93r_animation_09900_records.inc"
};

static u16 _gM93rAnimation09900Indices[20] = {
#include "assets/m93r_animation_09900_indices.inc"
};

static AnimationSet _gM93rAnimation09900 = {
    _gM93rAnimation09900Records,
    _gM93rAnimation09900Indices,
    { NULL, _gM93rAnimation09900Bank1, NULL, NULL, _gM93rAnimation09900Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM93rAnimation0A4ECBank1[19] = {
#include "assets/m93r_animation_0A4EC_bank1.inc"
};

static AnimationPackedRotation _gM93rAnimation0A4ECBank4[304] = {
#include "assets/m93r_animation_0A4EC_bank4.inc"
};

static AnimationRecord _gM93rAnimation0A4ECRecords[382] = {
#include "assets/m93r_animation_0A4EC_records.inc"
};

static u16 _gM93rAnimation0A4ECIndices[20] = {
#include "assets/m93r_animation_0A4EC_indices.inc"
};

static AnimationSet _gM93rAnimation0A4EC = {
    _gM93rAnimation0A4ECRecords,
    _gM93rAnimation0A4ECIndices,
    { NULL, _gM93rAnimation0A4ECBank1, NULL, NULL, _gM93rAnimation0A4ECBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM93rAnimation0AF84Bank1[18] = {
#include "assets/m93r_animation_0AF84_bank1.inc"
};

static AnimationPackedRotation _gM93rAnimation0AF84Bank4[279] = {
#include "assets/m93r_animation_0AF84_bank4.inc"
};

static AnimationRecord _gM93rAnimation0AF84Records[325] = {
#include "assets/m93r_animation_0AF84_records.inc"
};

static u16 _gM93rAnimation0AF84Indices[20] = {
#include "assets/m93r_animation_0AF84_indices.inc"
};

static AnimationSet _gM93rAnimation0AF84 = {
    _gM93rAnimation0AF84Records,
    _gM93rAnimation0AF84Indices,
    { NULL, _gM93rAnimation0AF84Bank1, NULL, NULL, _gM93rAnimation0AF84Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM93rAnimation0B4D4Bank1[8] = {
#include "assets/m93r_animation_0B4D4_bank1.inc"
};

static AnimationPackedRotation _gM93rAnimation0B4D4Bank4[120] = {
#include "assets/m93r_animation_0B4D4_bank4.inc"
};

static AnimationRecord _gM93rAnimation0B4D4Records[176] = {
#include "assets/m93r_animation_0B4D4_records.inc"
};

static u16 _gM93rAnimation0B4D4Indices[20] = {
#include "assets/m93r_animation_0B4D4_indices.inc"
};

static AnimationSet _gM93rAnimation0B4D4 = {
    _gM93rAnimation0B4D4Records,
    _gM93rAnimation0B4D4Indices,
    { NULL, _gM93rAnimation0B4D4Bank1, NULL, NULL, _gM93rAnimation0B4D4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM93rAnimation0BBE8Bank1[12] = {
#include "assets/m93r_animation_0BBE8_bank1.inc"
};

static AnimationPackedRotation _gM93rAnimation0BBE8Bank4[179] = {
#include "assets/m93r_animation_0BBE8_bank4.inc"
};

static AnimationRecord _gM93rAnimation0BBE8Records[218] = {
#include "assets/m93r_animation_0BBE8_records.inc"
};

static u16 _gM93rAnimation0BBE8Indices[20] = {
#include "assets/m93r_animation_0BBE8_indices.inc"
};

static AnimationSet _gM93rAnimation0BBE8 = {
    _gM93rAnimation0BBE8Records,
    _gM93rAnimation0BBE8Indices,
    { NULL, _gM93rAnimation0BBE8Bank1, NULL, NULL, _gM93rAnimation0BBE8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM93rAnimation0C2B8Bank1[12] = {
#include "assets/m93r_animation_0C2B8_bank1.inc"
};

static AnimationPackedRotation _gM93rAnimation0C2B8Bank4[170] = {
#include "assets/m93r_animation_0C2B8_bank4.inc"
};

static AnimationRecord _gM93rAnimation0C2B8Records[210] = {
#include "assets/m93r_animation_0C2B8_records.inc"
};

static u16 _gM93rAnimation0C2B8Indices[20] = {
#include "assets/m93r_animation_0C2B8_indices.inc"
};

static AnimationSet _gM93rAnimation0C2B8 = {
    _gM93rAnimation0C2B8Records,
    _gM93rAnimation0C2B8Indices,
    { NULL, _gM93rAnimation0C2B8Bank1, NULL, NULL, _gM93rAnimation0C2B8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM93rAnimation0CC04Bank1[16] = {
#include "assets/m93r_animation_0CC04_bank1.inc"
};

static AnimationPackedRotation _gM93rAnimation0CC04Bank4[221] = {
#include "assets/m93r_animation_0CC04_bank4.inc"
};

static AnimationRecord _gM93rAnimation0CC04Records[306] = {
#include "assets/m93r_animation_0CC04_records.inc"
};

static u16 _gM93rAnimation0CC04Indices[20] = {
#include "assets/m93r_animation_0CC04_indices.inc"
};

static AnimationSet _gM93rAnimation0CC04 = {
    _gM93rAnimation0CC04Records,
    _gM93rAnimation0CC04Indices,
    { NULL, _gM93rAnimation0CC04Bank1, NULL, NULL, _gM93rAnimation0CC04Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM93rAnimation0D4DCBank1[19] = {
#include "assets/m93r_animation_0D4DC_bank1.inc"
};

static AnimationPackedRotation _gM93rAnimation0D4DCBank4[193] = {
#include "assets/m93r_animation_0D4DC_bank4.inc"
};

static AnimationRecord _gM93rAnimation0D4DCRecords[296] = {
#include "assets/m93r_animation_0D4DC_records.inc"
};

static u16 _gM93rAnimation0D4DCIndices[20] = {
#include "assets/m93r_animation_0D4DC_indices.inc"
};

static AnimationSet _gM93rAnimation0D4DC = {
    _gM93rAnimation0D4DCRecords,
    _gM93rAnimation0D4DCIndices,
    { NULL, _gM93rAnimation0D4DCBank1, NULL, NULL, _gM93rAnimation0D4DCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM93rAnimation0DC10Bank1[13] = {
#include "assets/m93r_animation_0DC10_bank1.inc"
};

static AnimationPackedRotation _gM93rAnimation0DC10Bank4[167] = {
#include "assets/m93r_animation_0DC10_bank4.inc"
};

static AnimationRecord _gM93rAnimation0DC10Records[235] = {
#include "assets/m93r_animation_0DC10_records.inc"
};

static u16 _gM93rAnimation0DC10Indices[20] = {
#include "assets/m93r_animation_0DC10_indices.inc"
};

static AnimationSet _gM93rAnimation0DC10 = {
    _gM93rAnimation0DC10Records,
    _gM93rAnimation0DC10Indices,
    { NULL, _gM93rAnimation0DC10Bank1, NULL, NULL, _gM93rAnimation0DC10Bank4, NULL, NULL, NULL },
};

AnimationBank D_m93r_8012ADF8 = { { {
    NULL,
    &_gM93rPlayerIdleAnimation,
    &_gM93rAnimation0CC04,
    &_gM93rAnimation0D4DC,
    &_gM93rAnimation0DC10,
    &_gM93rPlayerTurnLeftAnimation,
    &_gM93rPlayerTurnRightAnimation,
    &_gM93rAnimation0BBE8,
    &_gM93rAnimation0C2B8,
    &_gM93rAnimation09250,
    &_gM93rAnimation0B4D4,
    &_gM93rPlayerIdleAnimation,
    &_gM93rAnimation0A4EC,
    &_gM93rAnimation09900,
    &_gM93rAnimation0AF84,
    &_gM93rPlayerIdleAnimation,
    &_gM93rAnimation04E34,
    &_gM93rAnimation05154,
    &_gM93rAnimation05918,
    &_gM93rPlayerUnarmedWalkAnimation,
    &_gM93rAnimation0AF84,
    &_gM93rPlayerIdleAnimation,
    &_gM93rPlayerIdleAnimation,
    &_gM93rAnimation06BB0,
    &_gM93rAnimation07E44,
    &_gM93rAnimation07728,
    &_gM93rPlayerParasiteEnergyReleaseFamily0Animation,
    &_gM93rPlayerParasiteEnergyChargeFamily0Animation,
    &_gM93rAnimation04388,
    &_gM93rAnimation0462C,
    &_gM93rAnimation0482C,
    &_gM93rAnimation04B80,
    &_gM93rAnimation082C4,
    &_gM93rAnimation0849C,
    &_gM93rAnimation0BBE8,
    &_gM93rAnimation0C2B8,
    &_gM93rPlayerStairAscentAnimation,
    &_gM93rPlayerStairDescentAnimation,
    &_gM93rPlayerStairAscentFinishEvenAnimation,
    &_gM93rPlayerStairAscentFinishOddAnimation,
    &_gM93rPlayerItemUseAnimation,
    &_gM93rPlayerIdleAnimation,
    &_gM93rAnimation08A00,
    &_gM93rAnimation08BF4,
    &_gM93rAnimation09074,
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
