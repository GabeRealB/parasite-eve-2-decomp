#include "weapons/m93r.h"

#include <psyq/sys/types.h>
#include <psyq/libgpu.h>

#include "common.h"

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

/// PE family-0 cast translations in model units and Euler rotations in 1/4096 turns for tracks 0 and 1.
static const AnimationPackedPose _gM93rPlayerParasiteEnergyCastFamily0AnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/m93r_animation_04388_bank1.inc"
};

/// PE family-0 cast Euler rotations in eight-angle-unit steps for player tracks 2..18.
static const AnimationPackedRotation _gM93rPlayerParasiteEnergyCastFamily0AnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m93r_animation_04388_bank4.inc"
};

/// PE family-0 cast keyframes and stops; durations are normal-rate frames and pose offsets count u32 words.
static const AnimationRecord _gM93rPlayerParasiteEnergyCastFamily0AnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m93r_animation_04388_records.inc"
};

/// Absolute PE family-0 cast record starts for tracks 0..18, followed by the retained serialized halfword.
static const u16 _gM93rPlayerParasiteEnergyCastFamily0AnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m93r_animation_04388_indices.inc"
};

/// Parasite Energy cast for clip family 0 in the player's M93R bank.
///
/// Native clip 28; starts when the attachment charge countdown reaches zero.
/// Family 0 is selected by attachment ID. Each of its 19 part tracks ends
/// with a stop; tracks 0..1 carry translation/rotation and 2..18 packed rotation.
/// Playback borrows the clip and tables; keep the M93R package loaded while
/// any slot or cached record pointer refers to them.
static AnimationSet _gM93rPlayerParasiteEnergyCastFamily0Animation = {
    .records           = _gM93rPlayerParasiteEnergyCastFamily0AnimationRecords,
    .trackStartIndices = _gM93rPlayerParasiteEnergyCastFamily0AnimationIndices,
    .poseBanks         = {
        [M93R_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM93rPlayerParasiteEnergyCastFamily0AnimationBank1,
        [M93R_ANIMATION_PACKED_ROTATION_BANK]      = _gM93rPlayerParasiteEnergyCastFamily0AnimationBank4,
    },
};

/// PE family-1 release translations in model units and Euler rotations in 1/4096 turns for tracks 0 and 1.
static const AnimationPackedPose _gM93rPlayerParasiteEnergyReleaseFamily1AnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/m93r_animation_0462C_bank1.inc"
};

/// PE family-1 release Euler rotations in eight-angle-unit steps for player tracks 2..18.
static const AnimationPackedRotation _gM93rPlayerParasiteEnergyReleaseFamily1AnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m93r_animation_0462C_bank4.inc"
};

/// PE family-1 release keyframes and stops; durations are normal-rate frames and pose offsets count u32 words.
static const AnimationRecord _gM93rPlayerParasiteEnergyReleaseFamily1AnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m93r_animation_0462C_records.inc"
};

/// Absolute PE family-1 release record starts for tracks 0..18, followed by the retained serialized halfword.
static const u16 _gM93rPlayerParasiteEnergyReleaseFamily1AnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m93r_animation_0462C_indices.inc"
};

/// Parasite Energy release for clip family 1 in the player's M93R bank.
///
/// Native clip 29; plays while the attachment's charge countdown starts.
/// Family 1 is selected by attachment ID. Each of its 19 part tracks ends
/// with a stop; tracks 0..1 carry translation/rotation and 2..18 packed rotation.
/// Playback borrows the clip and tables; keep the M93R package loaded while
/// any slot or cached record pointer refers to them.
static AnimationSet _gM93rPlayerParasiteEnergyReleaseFamily1Animation = {
    .records           = _gM93rPlayerParasiteEnergyReleaseFamily1AnimationRecords,
    .trackStartIndices = _gM93rPlayerParasiteEnergyReleaseFamily1AnimationIndices,
    .poseBanks         = {
        [M93R_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM93rPlayerParasiteEnergyReleaseFamily1AnimationBank1,
        [M93R_ANIMATION_PACKED_ROTATION_BANK]      = _gM93rPlayerParasiteEnergyReleaseFamily1AnimationBank4,
    },
};

/// PE family-1 charge translations in model units and Euler rotations in 1/4096 turns for tracks 0 and 1.
static const AnimationPackedPose _gM93rPlayerParasiteEnergyChargeFamily1AnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/m93r_animation_0482C_bank1.inc"
};

/// PE family-1 charge Euler rotations in eight-angle-unit steps for player tracks 2..18.
static const AnimationPackedRotation _gM93rPlayerParasiteEnergyChargeFamily1AnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m93r_animation_0482C_bank4.inc"
};

/// PE family-1 charge keyframes, loop controls and retained stops.
///
/// Durations are normal-rate frames and pose offsets count u32 words.
static const AnimationRecord _gM93rPlayerParasiteEnergyChargeFamily1AnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m93r_animation_0482C_records.inc"
};

/// Absolute PE family-1 charge record starts for tracks 0..18, followed by the retained serialized halfword.
static const u16 _gM93rPlayerParasiteEnergyChargeFamily1AnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m93r_animation_0482C_indices.inc"
};

/// Looping Parasite Energy charge pose for clip family 1 in the player's M93R bank.
///
/// Native clip 30; follows release until the attachment countdown reaches zero,
/// then yields to casting. Family 1 is selected by attachment ID. Each of the
/// 19 part tracks jumps back to its charge keyframe before a retained stop.
/// Tracks 0..1 carry translation/rotation and 2..18 packed rotation. Playback
/// borrows the clip and tables; keep the M93R package loaded while any slot
/// or cached record pointer refers to them.
static AnimationSet _gM93rPlayerParasiteEnergyChargeFamily1Animation = {
    .records           = _gM93rPlayerParasiteEnergyChargeFamily1AnimationRecords,
    .trackStartIndices = _gM93rPlayerParasiteEnergyChargeFamily1AnimationIndices,
    .poseBanks         = {
        [M93R_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM93rPlayerParasiteEnergyChargeFamily1AnimationBank1,
        [M93R_ANIMATION_PACKED_ROTATION_BANK]      = _gM93rPlayerParasiteEnergyChargeFamily1AnimationBank4,
    },
};

/// PE family-1 cast translations in model units and Euler rotations in 1/4096 turns for tracks 0 and 1.
static const AnimationPackedPose _gM93rPlayerParasiteEnergyCastFamily1AnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/m93r_animation_04B80_bank1.inc"
};

/// PE family-1 cast Euler rotations in eight-angle-unit steps for player tracks 2..18.
static const AnimationPackedRotation _gM93rPlayerParasiteEnergyCastFamily1AnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m93r_animation_04B80_bank4.inc"
};

/// PE family-1 cast keyframes and stops; durations are normal-rate frames and pose offsets count u32 words.
static const AnimationRecord _gM93rPlayerParasiteEnergyCastFamily1AnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m93r_animation_04B80_records.inc"
};

/// Absolute PE family-1 cast record starts for tracks 0..18, followed by the retained serialized halfword.
static const u16 _gM93rPlayerParasiteEnergyCastFamily1AnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m93r_animation_04B80_indices.inc"
};

/// Parasite Energy cast for clip family 1 in the player's M93R bank.
///
/// Native clip 31; starts when the attachment charge countdown reaches zero.
/// Family 1 is selected by attachment ID. Each of its 19 part tracks ends
/// with a stop; tracks 0..1 carry translation/rotation and 2..18 packed rotation.
/// Playback borrows the clip and tables; keep the M93R package loaded while
/// any slot or cached record pointer refers to them.
static AnimationSet _gM93rPlayerParasiteEnergyCastFamily1Animation = {
    .records           = _gM93rPlayerParasiteEnergyCastFamily1AnimationRecords,
    .trackStartIndices = _gM93rPlayerParasiteEnergyCastFamily1AnimationIndices,
    .poseBanks         = {
        [M93R_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM93rPlayerParasiteEnergyCastFamily1AnimationBank1,
        [M93R_ANIMATION_PACKED_ROTATION_BANK]      = _gM93rPlayerParasiteEnergyCastFamily1AnimationBank4,
    },
};

/// Part-4 hit translations in model units and Euler rotations in 1/4096 turns for tracks 0 and 1.
static const AnimationPackedPose _gM93rPlayerHitPart4AnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/m93r_animation_04E34_bank1.inc"
};

/// Part-4 hit Euler rotations in eight-angle-unit steps for player tracks 2..18.
static const AnimationPackedRotation _gM93rPlayerHitPart4AnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m93r_animation_04E34_bank4.inc"
};

/// Part-4 hit keyframes and stops; durations are normal-rate frames and pose offsets count u32 words.
static const AnimationRecord _gM93rPlayerHitPart4AnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m93r_animation_04E34_records.inc"
};

/// Absolute part-4 hit record starts for tracks 0..18, followed by the retained serialized halfword.
static const u16 _gM93rPlayerHitPart4AnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m93r_animation_04E34_indices.inc"
};

/// Player hit reaction for the collision sphere on model part 4 in the M93R bank.
///
/// Native clip 16; hit region 1 selects this reaction with a three-frame blend.
/// Each of its 19 part tracks ends with a stop; tracks 0..1 carry
/// translation/rotation and 2..18 packed rotation. Playback borrows the clip
/// and tables; keep the M93R package loaded while any slot or cached record
/// pointer refers to them.
static AnimationSet _gM93rPlayerHitPart4Animation = {
    .records           = _gM93rPlayerHitPart4AnimationRecords,
    .trackStartIndices = _gM93rPlayerHitPart4AnimationIndices,
    .poseBanks         = {
        [M93R_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM93rPlayerHitPart4AnimationBank1,
        [M93R_ANIMATION_PACKED_ROTATION_BANK]      = _gM93rPlayerHitPart4AnimationBank4,
    },
};

/// Other-body hit translations in model units and Euler rotations in 1/4096 turns for tracks 0 and 1.
static const AnimationPackedPose _gM93rPlayerHitOtherBodyAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/m93r_animation_05154_bank1.inc"
};

/// Other-body hit Euler rotations in eight-angle-unit steps for player tracks 2..18.
static const AnimationPackedRotation _gM93rPlayerHitOtherBodyAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m93r_animation_05154_bank4.inc"
};

/// Other-body hit keyframes and stops; durations are normal-rate frames and pose offsets count u32 words.
static const AnimationRecord _gM93rPlayerHitOtherBodyAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m93r_animation_05154_records.inc"
};

/// Absolute other-body hit record starts for tracks 0..18, followed by the retained serialized halfword.
static const u16 _gM93rPlayerHitOtherBodyAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m93r_animation_05154_indices.inc"
};

/// Player hit reaction for the other collision bodies in the M93R bank.
///
/// Native clip 17; hits on the root or part-1 sphere select this reaction
/// with a three-frame blend. Each of its 19 part tracks ends with a stop;
/// tracks 0..1 carry translation/rotation and 2..18 packed rotation. Playback
/// borrows the clip and tables; keep the M93R package loaded while any slot
/// or cached record pointer refers to them.
static AnimationSet _gM93rPlayerHitOtherBodyAnimation = {
    .records           = _gM93rPlayerHitOtherBodyAnimationRecords,
    .trackStartIndices = _gM93rPlayerHitOtherBodyAnimationIndices,
    .poseBanks         = {
        [M93R_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM93rPlayerHitOtherBodyAnimationBank1,
        [M93R_ANIMATION_PACKED_ROTATION_BANK]      = _gM93rPlayerHitOtherBodyAnimationBank4,
    },
};

/// Death translations in model units and Euler rotations in 1/4096 turns for tracks 0 and 1.
///
/// Read-only animation tables retain their serialized positions in .data.
static const AnimationPackedPose _gM93rPlayerDeathAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/m93r_animation_05918_bank1.inc"
};

/// Death Euler rotations in eight-angle-unit steps for player tracks 2..18.
static const AnimationPackedRotation _gM93rPlayerDeathAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m93r_animation_05918_bank4.inc"
};

/// Death keyframes and stops; durations are normal-rate frames and keyframe offsets count u32 words.
static const AnimationRecord _gM93rPlayerDeathAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m93r_animation_05918_records.inc"
};

/// Absolute death record starts for tracks 0..18, followed by the retained serialized halfword.
static const u16 _gM93rPlayerDeathAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m93r_animation_05918_indices.inc"
};

/// Player death motion in the M93R animation bank, ending in a held pose.
///
/// Native clip 18, selected when HP reaches zero. Each of its 19 part tracks
/// stops at its final keyframe; tracks 0..1 use translation/rotation and 2..18
/// packed rotation. Playback borrows the set and tables; keep the M93R package
/// loaded while any slot or cached record pointer refers to them.
static AnimationSet _gM93rPlayerDeathAnimation = {
    .records           = _gM93rPlayerDeathAnimationRecords,
    .trackStartIndices = _gM93rPlayerDeathAnimationIndices,
    .poseBanks         = {
        [M93R_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM93rPlayerDeathAnimationBank1,
        [M93R_ANIMATION_PACKED_ROTATION_BANK]      = _gM93rPlayerDeathAnimationBank4,
    },
};

/// High-health idle translations in model units and Euler rotations in 1/4096 turns for tracks 0 and 1.
static const AnimationPackedPose _gM93rPlayerHighHealthIdleAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/m93r_animation_06BB0_bank1.inc"
};

/// High-health idle Euler rotations in eight-angle-unit steps for player tracks 2..18.
static const AnimationPackedRotation _gM93rPlayerHighHealthIdleAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m93r_animation_06BB0_bank4.inc"
};

/// High-health idle keyframes and controls; durations are normal-rate frames and keyframe offsets count u32 words.
static const AnimationRecord _gM93rPlayerHighHealthIdleAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m93r_animation_06BB0_records.inc"
};

/// Absolute high-health idle record starts for tracks 0..18, followed by the retained serialized halfword.
static const u16 _gM93rPlayerHighHealthIdleAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m93r_animation_06BB0_indices.inc"
};

/// Long-idle player motion above half maximum HP in the M93R animation bank.
///
/// Native clip 23, blended over five frames after 300 direction-free control
/// ticks. The HP threshold truncates to an integer. All 19 part tracks repeat
/// through control jumps; tracks 0..1 use translation/rotation and 2..18 packed
/// rotation. Playback borrows the set and tables; keep the M93R package loaded
/// while any slot or cached record pointer refers to them.
static AnimationSet _gM93rPlayerHighHealthIdleAnimation = {
    .records           = _gM93rPlayerHighHealthIdleAnimationRecords,
    .trackStartIndices = _gM93rPlayerHighHealthIdleAnimationIndices,
    .poseBanks         = {
        [M93R_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM93rPlayerHighHealthIdleAnimationBank1,
        [M93R_ANIMATION_PACKED_ROTATION_BANK]      = _gM93rPlayerHighHealthIdleAnimationBank4,
    },
};

/// Low-health idle translations in model units and Euler rotations in 1/4096 turns for tracks 0 and 1.
static const AnimationPackedPose _gM93rPlayerLowHealthIdleAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/m93r_animation_07728_bank1.inc"
};

/// Low-health idle Euler rotations in eight-angle-unit steps for player tracks 2..18.
static const AnimationPackedRotation _gM93rPlayerLowHealthIdleAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m93r_animation_07728_bank4.inc"
};

/// Low-health idle keyframes and controls; durations are normal-rate frames and keyframe offsets count u32 words.
static const AnimationRecord _gM93rPlayerLowHealthIdleAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m93r_animation_07728_records.inc"
};

/// Absolute low-health idle record starts for tracks 0..18, followed by the retained serialized halfword.
static const u16 _gM93rPlayerLowHealthIdleAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m93r_animation_07728_indices.inc"
};

/// Long-idle player motion at or below quarter maximum HP, also used for paralysis.
///
/// Native M93R clip 25. Normal control blends to it over five frames after 300
/// direction-free ticks; paralysis blends all child tracks over six frames.
/// The HP threshold truncates to an integer. All 19 part tracks repeat through
/// control jumps; tracks 0..1 use translation/rotation and 2..18 packed rotation.
/// Playback borrows the set and tables; keep the M93R package loaded while any
/// slot or cached record pointer refers to them.
static AnimationSet _gM93rPlayerLowHealthIdleAnimation = {
    .records           = _gM93rPlayerLowHealthIdleAnimationRecords,
    .trackStartIndices = _gM93rPlayerLowHealthIdleAnimationIndices,
    .poseBanks         = {
        [M93R_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM93rPlayerLowHealthIdleAnimationBank1,
        [M93R_ANIMATION_PACKED_ROTATION_BANK]      = _gM93rPlayerLowHealthIdleAnimationBank4,
    },
};

/// Mid-health idle translations in model units and Euler rotations in 1/4096 turns for tracks 0 and 1.
static const AnimationPackedPose _gM93rPlayerMidHealthIdleAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/m93r_animation_07E44_bank1.inc"
};

/// Mid-health idle Euler rotations in eight-angle-unit steps for player tracks 2..18.
static const AnimationPackedRotation _gM93rPlayerMidHealthIdleAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m93r_animation_07E44_bank4.inc"
};

/// Mid-health idle keyframes and controls; durations are normal-rate frames and keyframe offsets count u32 words.
static const AnimationRecord _gM93rPlayerMidHealthIdleAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m93r_animation_07E44_records.inc"
};

/// Absolute mid-health idle record starts for tracks 0..18, followed by the retained serialized halfword.
static const u16 _gM93rPlayerMidHealthIdleAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m93r_animation_07E44_indices.inc"
};

/// Long-idle player motion above quarter and at or below half maximum HP in the M93R bank.
///
/// Native clip 24, blended over five frames after 300 direction-free control
/// ticks. Both HP thresholds truncate to integers. All 19 part tracks repeat
/// through control jumps; tracks 0..1 use translation/rotation and 2..18 packed
/// rotation. Playback borrows the set and tables; keep the M93R package loaded
/// while any slot or cached record pointer refers to them.
static AnimationSet _gM93rPlayerMidHealthIdleAnimation = {
    .records           = _gM93rPlayerMidHealthIdleAnimationRecords,
    .trackStartIndices = _gM93rPlayerMidHealthIdleAnimationIndices,
    .poseBanks         = {
        [M93R_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM93rPlayerMidHealthIdleAnimationBank1,
        [M93R_ANIMATION_PACKED_ROTATION_BANK]      = _gM93rPlayerMidHealthIdleAnimationBank4,
    },
};

/// Presentation-entry translations in model units and Euler rotations in 1/4096 turns for tracks 0 and 1.
static const AnimationPackedPose _gM93rPlayerScriptedPresentationEnterAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/m93r_animation_082C4_bank1.inc"
};

/// Presentation-entry Euler rotations in eight-angle-unit steps for player tracks 2..18.
static const AnimationPackedRotation _gM93rPlayerScriptedPresentationEnterAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m93r_animation_082C4_bank4.inc"
};

/// Presentation-entry keyframes and stops; durations are normal-rate frames and keyframe offsets count u32 words.
static const AnimationRecord _gM93rPlayerScriptedPresentationEnterAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m93r_animation_082C4_records.inc"
};

/// Absolute presentation-entry record starts for tracks 0..18, followed by the retained serialized halfword.
static const u16 _gM93rPlayerScriptedPresentationEnterAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m93r_animation_082C4_indices.inc"
};

/// Player entry into a scripted presentation using the M93R animation bank.
///
/// Native clip 32, selected by a zero presentation variant before CAP/dialogue
/// playback. Each of its 19 part tracks stops at its final keyframe; tracks
/// 0..1 use translation/rotation and 2..18 packed rotation. Playback borrows
/// the set and tables; keep the M93R package loaded while any slot or cached
/// record pointer refers to them.
static AnimationSet _gM93rPlayerScriptedPresentationEnterAnimation = {
    .records           = _gM93rPlayerScriptedPresentationEnterAnimationRecords,
    .trackStartIndices = _gM93rPlayerScriptedPresentationEnterAnimationIndices,
    .poseBanks         = {
        [M93R_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM93rPlayerScriptedPresentationEnterAnimationBank1,
        [M93R_ANIMATION_PACKED_ROTATION_BANK]      = _gM93rPlayerScriptedPresentationEnterAnimationBank4,
    },
};

/// Presentation-exit translations in model units and Euler rotations in 1/4096 turns for tracks 0 and 1.
static const AnimationPackedPose _gM93rPlayerScriptedPresentationExitAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/m93r_animation_0849C_bank1.inc"
};

/// Presentation-exit Euler rotations in eight-angle-unit steps for player tracks 2..18.
static const AnimationPackedRotation _gM93rPlayerScriptedPresentationExitAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m93r_animation_0849C_bank4.inc"
};

/// Presentation-exit keyframes and stops; durations are normal-rate frames and keyframe offsets count u32 words.
static const AnimationRecord _gM93rPlayerScriptedPresentationExitAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m93r_animation_0849C_records.inc"
};

/// Absolute presentation-exit record starts for tracks 0..18, followed by the retained serialized halfword.
static const u16 _gM93rPlayerScriptedPresentationExitAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m93r_animation_0849C_indices.inc"
};

/// Player exit from a scripted presentation using the M93R animation bank.
///
/// Native clip 33, selected by a nonzero presentation variant after CAP/dialogue
/// playback and before control resumes. Each of its 19 part tracks stops at its
/// final keyframe; tracks 0..1 use translation/rotation and 2..18 packed rotation.
/// Playback borrows the set and tables; keep the M93R package loaded while any
/// slot or cached record pointer refers to them.
static AnimationSet _gM93rPlayerScriptedPresentationExitAnimation = {
    .records           = _gM93rPlayerScriptedPresentationExitAnimationRecords,
    .trackStartIndices = _gM93rPlayerScriptedPresentationExitAnimationIndices,
    .poseBanks         = {
        [M93R_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM93rPlayerScriptedPresentationExitAnimationBank1,
        [M93R_ANIMATION_PACKED_ROTATION_BANK]      = _gM93rPlayerScriptedPresentationExitAnimationBank4,
    },
};

/// Family-2 PE release translations in model units and Euler rotations in 1/4096 turns for tracks 0..1.
static const AnimationPackedPose _gM93rPlayerParasiteEnergyReleaseFamily2AnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m93r_animation_08A00_bank1.inc"
};

/// Family-2 PE release Euler rotations in eight-angle-unit steps for player tracks 2..18.
static const AnimationPackedRotation _gM93rPlayerParasiteEnergyReleaseFamily2AnimationBank4[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m93r_animation_08A00_bank4.inc"
};

/// Family-2 PE release keyframes and stops; durations are normal-rate frames and pose offsets count u32 words.
static const AnimationRecord _gM93rPlayerParasiteEnergyReleaseFamily2AnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m93r_animation_08A00_records.inc"
};

/// Absolute family-2 PE release record starts for tracks 0..18, followed by the retained serialized halfword.
static const u16 _gM93rPlayerParasiteEnergyReleaseFamily2AnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m93r_animation_08A00_indices.inc"
};

/// Player Parasite Energy release clip for family 2 in the M93R animation bank.
///
/// Native clip 42 precedes the charge phase. Family 2 is selected for non-item
/// attachments whose decimal tens digit is 3. All 19 tracks stop at their final
/// keyframe; tracks 0..1 use translation/rotation and 2..18 packed rotation.
/// Playback borrows the set and tables; keep the M93R package loaded while any
/// slot or cached record pointer refers to them. Read-only tables retain .data
/// placement as part of the serialized package layout.
static AnimationSet _gM93rPlayerParasiteEnergyReleaseFamily2Animation = {
    .records           = _gM93rPlayerParasiteEnergyReleaseFamily2AnimationRecords,
    .trackStartIndices = _gM93rPlayerParasiteEnergyReleaseFamily2AnimationIndices,
    .poseBanks         = {
        [M93R_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM93rPlayerParasiteEnergyReleaseFamily2AnimationBank1,
        [M93R_ANIMATION_PACKED_ROTATION_BANK]      = _gM93rPlayerParasiteEnergyReleaseFamily2AnimationBank4,
    },
};

/// Family-2 PE charge translations in model units and Euler rotations in 1/4096 turns for tracks 0..1.
static const AnimationPackedPose _gM93rPlayerParasiteEnergyChargeFamily2AnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m93r_animation_08BF4_bank1.inc"
};

/// Family-2 PE charge Euler rotations in eight-angle-unit steps for player tracks 2..18.
static const AnimationPackedRotation _gM93rPlayerParasiteEnergyChargeFamily2AnimationBank4[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m93r_animation_08BF4_bank4.inc"
};

/// Family-2 PE charge keyframes, loop jumps and retained stops.
///
/// Durations count normal-rate frames, pose offsets count u32 words, and jumps
/// select absolute record indices.
static const AnimationRecord _gM93rPlayerParasiteEnergyChargeFamily2AnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m93r_animation_08BF4_records.inc"
};

/// Absolute family-2 PE charge record starts for tracks 0..18, followed by the retained serialized halfword.
static const u16 _gM93rPlayerParasiteEnergyChargeFamily2AnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m93r_animation_08BF4_indices.inc"
};

/// Player Parasite Energy charge loop for family 2 in the M93R animation bank.
///
/// Native clip 43 loops while the attachment countdown runs; clip 44 replaces
/// it when charging finishes. Family 2 is selected for non-item attachments
/// whose decimal tens digit is 3. All 19 tracks loop; tracks 0..1 use
/// translation/rotation and 2..18 packed rotation.
/// Playback borrows the set and tables; keep the M93R package loaded while any
/// slot or cached record pointer refers to them. Read-only tables retain .data
/// placement as part of the serialized package layout.
static AnimationSet _gM93rPlayerParasiteEnergyChargeFamily2Animation = {
    .records           = _gM93rPlayerParasiteEnergyChargeFamily2AnimationRecords,
    .trackStartIndices = _gM93rPlayerParasiteEnergyChargeFamily2AnimationIndices,
    .poseBanks         = {
        [M93R_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM93rPlayerParasiteEnergyChargeFamily2AnimationBank1,
        [M93R_ANIMATION_PACKED_ROTATION_BANK]      = _gM93rPlayerParasiteEnergyChargeFamily2AnimationBank4,
    },
};

/// Family-2 PE cast translations in model units and Euler rotations in 1/4096 turns for tracks 0..1.
static const AnimationPackedPose _gM93rPlayerParasiteEnergyCastFamily2AnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m93r_animation_09074_bank1.inc"
};

/// Family-2 PE cast Euler rotations in eight-angle-unit steps for player tracks 2..18.
static const AnimationPackedRotation _gM93rPlayerParasiteEnergyCastFamily2AnimationBank4[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m93r_animation_09074_bank4.inc"
};

/// Family-2 PE cast keyframes and stops; durations are normal-rate frames and pose offsets count u32 words.
static const AnimationRecord _gM93rPlayerParasiteEnergyCastFamily2AnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m93r_animation_09074_records.inc"
};

/// Absolute family-2 PE cast record starts for tracks 0..18, followed by the retained serialized halfword.
static const u16 _gM93rPlayerParasiteEnergyCastFamily2AnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m93r_animation_09074_indices.inc"
};

/// Player Parasite Energy cast clip for family 2 in the M93R animation bank.
///
/// Native clip 44 finishes the charge action before locomotion or aim resumes.
/// Family 2 is selected for non-item attachments whose decimal tens digit is 3.
/// All 19 tracks stop at their final keyframe; tracks 0..1 use
/// translation/rotation and 2..18 packed rotation.
/// Playback borrows the set and tables; keep the M93R package loaded while any
/// slot or cached record pointer refers to them. Read-only tables retain .data
/// placement as part of the serialized package layout.
static AnimationSet _gM93rPlayerParasiteEnergyCastFamily2Animation = {
    .records           = _gM93rPlayerParasiteEnergyCastFamily2AnimationRecords,
    .trackStartIndices = _gM93rPlayerParasiteEnergyCastFamily2AnimationIndices,
    .poseBanks         = {
        [M93R_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM93rPlayerParasiteEnergyCastFamily2AnimationBank1,
        [M93R_ANIMATION_PACKED_ROTATION_BANK]      = _gM93rPlayerParasiteEnergyCastFamily2AnimationBank4,
    },
};

/// Aim-idle translations in model units and Euler rotations in 1/4096 turns for tracks 0..1.
static const AnimationPackedPose _gM93rPlayerAimIdleAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m93r_animation_09250_bank1.inc"
};

/// Aim-idle Euler rotations in eight-angle-unit steps for player tracks 2..18.
static const AnimationPackedRotation _gM93rPlayerAimIdleAnimationBank4[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m93r_animation_09250_bank4.inc"
};

/// Aim-idle keyframes, loop jumps and retained stops.
///
/// Durations count normal-rate frames, pose offsets count u32 words, and jumps
/// select absolute record indices.
static const AnimationRecord _gM93rPlayerAimIdleAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m93r_animation_09250_records.inc"
};

/// Absolute aim-idle record starts for tracks 0..18, followed by the retained serialized halfword.
static const u16 _gM93rPlayerAimIdleAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m93r_animation_09250_indices.inc"
};

/// Player stationary aiming loop and attack-ready pose in the M93R animation bank.
///
/// Native clip 9 is selected when aiming without movement or turning and when
/// preparing an M93R burst. All 19 tracks loop; tracks 0..1 use
/// translation/rotation and 2..18 packed rotation.
/// Playback borrows the set and tables; keep the M93R package loaded while any
/// slot or cached record pointer refers to them. Read-only tables retain .data
/// placement as part of the serialized package layout.
static AnimationSet _gM93rPlayerAimIdleAnimation = {
    .records           = _gM93rPlayerAimIdleAnimationRecords,
    .trackStartIndices = _gM93rPlayerAimIdleAnimationIndices,
    .poseBanks         = {
        [M93R_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM93rPlayerAimIdleAnimationBank1,
        [M93R_ANIMATION_PACKED_ROTATION_BANK]      = _gM93rPlayerAimIdleAnimationBank4,
    },
};

/// Backward/turn aim translations in model units and Euler rotations in 1/4096 turns for tracks 0..1.
static const AnimationPackedPose _gM93rPlayerAimBackwardOrTurnAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m93r_animation_09900_bank1.inc"
};

/// Backward/turn aim Euler rotations in eight-angle-unit steps for player tracks 2..18.
static const AnimationPackedRotation _gM93rPlayerAimBackwardOrTurnAnimationBank4[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m93r_animation_09900_bank4.inc"
};

/// Backward/turn aim keyframes, loop jumps and retained stops.
///
/// Durations count normal-rate frames and pose offsets count u32 words.
/// Track 1 carries the paired footstep cues `ANIMATION_RECORD_CUE_1` and
/// `ANIMATION_RECORD_CUE_2`; jump offsets are absolute record indices.
static const AnimationRecord _gM93rPlayerAimBackwardOrTurnAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m93r_animation_09900_records.inc"
};

/// Absolute backward/turn aim record starts for tracks 0..18, followed by the retained serialized halfword.
static const u16 _gM93rPlayerAimBackwardOrTurnAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m93r_animation_09900_indices.inc"
};

/// Player backward movement or stationary turning while aiming with the M93R.
///
/// Native clip 13 is selected for backward aim movement or a turn without
/// movement. All 19 tracks loop, with paired footstep cues on track 1;
/// tracks 0..1 use translation/rotation and 2..18 packed rotation.
/// Playback borrows the set and tables; keep the M93R package loaded while any
/// slot or cached record pointer refers to them. Read-only tables retain .data
/// placement as part of the serialized package layout.
static AnimationSet _gM93rPlayerAimBackwardOrTurnAnimation = {
    .records           = _gM93rPlayerAimBackwardOrTurnAnimationRecords,
    .trackStartIndices = _gM93rPlayerAimBackwardOrTurnAnimationIndices,
    .poseBanks         = {
        [M93R_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM93rPlayerAimBackwardOrTurnAnimationBank1,
        [M93R_ANIMATION_PACKED_ROTATION_BANK]      = _gM93rPlayerAimBackwardOrTurnAnimationBank4,
    },
};

/// Forward-aim translations in model units and Euler rotations in 1/4096 turns for tracks 0..1.
static const AnimationPackedPose _gM93rPlayerAimForwardAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m93r_animation_0A4EC_bank1.inc"
};

/// Forward-aim Euler rotations in eight-angle-unit steps for player tracks 2..18.
static const AnimationPackedRotation _gM93rPlayerAimForwardAnimationBank4[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m93r_animation_0A4EC_bank4.inc"
};

/// Forward-aim keyframes, loop jumps and retained stops.
///
/// Durations count normal-rate frames and pose offsets count u32 words.
/// Track 1 carries the paired footstep cues `ANIMATION_RECORD_CUE_1` and
/// `ANIMATION_RECORD_CUE_2`; jump offsets are absolute record indices.
static const AnimationRecord _gM93rPlayerAimForwardAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m93r_animation_0A4EC_records.inc"
};

/// Absolute forward-aim record starts for tracks 0..18, followed by the retained serialized halfword.
static const u16 _gM93rPlayerAimForwardAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m93r_animation_0A4EC_indices.inc"
};

/// Player forward movement while aiming with the M93R.
///
/// Native clip 12 is selected for forward aim movement. All 19 tracks loop,
/// with paired footstep cues on track 1; tracks 0..1 use translation/rotation
/// and 2..18 packed rotation.
/// Playback borrows the set and tables; keep the M93R package loaded while any
/// slot or cached record pointer refers to them. Read-only tables retain .data
/// placement as part of the serialized package layout.
static AnimationSet _gM93rPlayerAimForwardAnimation = {
    .records           = _gM93rPlayerAimForwardAnimationRecords,
    .trackStartIndices = _gM93rPlayerAimForwardAnimationIndices,
    .poseBanks         = {
        [M93R_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM93rPlayerAimForwardAnimationBank1,
        [M93R_ANIMATION_PACKED_ROTATION_BANK]      = _gM93rPlayerAimForwardAnimationBank4,
    },
};

/// Reload translations in model units and Euler rotations in 1/4096 turns for tracks 0 and 1.
///
/// Read-only animation tables retain their serialized positions in .data.
static const AnimationPackedPose _gM93rPlayerReloadAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/m93r_animation_0AF84_bank1.inc"
};

/// Reload Euler rotations in eight-angle-unit steps for player tracks 2..18.
static const AnimationPackedRotation _gM93rPlayerReloadAnimationBank4[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m93r_animation_0AF84_bank4.inc"
};

/// Reload keyframes and stops; durations count normal-rate frames and pose offsets count u32 words.
///
/// Track 1 marks ammunition loading with both `ANIMATION_RECORD_CUE_1` and
/// `ANIMATION_RECORD_CUE_2` on the same keyframe.
static const AnimationRecord _gM93rPlayerReloadAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m93r_animation_0AF84_records.inc"
};

/// Absolute reload record starts for tracks 0..18, followed by the retained serialized halfword.
static const u16 _gM93rPlayerReloadAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m93r_animation_0AF84_indices.inc"
};

/// Player primary-ammunition reload with the M93R, also used after battle.
///
/// Native clips 14 and 20 select the same set. All 19 tracks end in stops;
/// tracks 0..1 use translation/rotation and 2..18 packed rotation. Track 1
/// carries the ammunition-loading cue.
/// Playback borrows the set and tables; keep the M93R package loaded while
/// any slot or cached record pointer refers to them.
static AnimationSet _gM93rPlayerReloadAnimation = {
    .records           = _gM93rPlayerReloadAnimationRecords,
    .trackStartIndices = _gM93rPlayerReloadAnimationIndices,
    .poseBanks         = {
        [M93R_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM93rPlayerReloadAnimationBank1,
        [M93R_ANIMATION_PACKED_ROTATION_BANK]      = _gM93rPlayerReloadAnimationBank4,
    },
};

/// Firing translations in model units and Euler rotations in 1/4096 turns for tracks 0 and 1.
static const AnimationPackedPose _gM93rPlayerFireAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/m93r_animation_0B4D4_bank1.inc"
};

/// Firing Euler rotations in eight-angle-unit steps for player tracks 2..18.
static const AnimationPackedRotation _gM93rPlayerFireAnimationBank4[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m93r_animation_0B4D4_bank4.inc"
};

/// Firing keyframes and stops; durations count normal-rate frames and pose offsets count u32 words.
static const AnimationRecord _gM93rPlayerFireAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m93r_animation_0B4D4_records.inc"
};

/// Absolute firing record starts for tracks 0..18, followed by the retained serialized halfword.
static const u16 _gM93rPlayerFireAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m93r_animation_0B4D4_indices.inc"
};

/// Player firing motion for each M93R round, shared by single shots and bursts.
///
/// Native clip 10 is blended in for two normal-rate frames on each shot.
/// All 19 tracks end in stops and carry no cue bits; tracks 0..1 use
/// translation/rotation and 2..18 packed rotation.
/// Playback borrows the set and tables; keep the M93R package loaded while
/// any slot or cached record pointer refers to them.
static AnimationSet _gM93rPlayerFireAnimation = {
    .records           = _gM93rPlayerFireAnimationRecords,
    .trackStartIndices = _gM93rPlayerFireAnimationIndices,
    .poseBanks         = {
        [M93R_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM93rPlayerFireAnimationBank1,
        [M93R_ANIMATION_PACKED_ROTATION_BANK]      = _gM93rPlayerFireAnimationBank4,
    },
};

/// Aim-entry translations in model units and Euler rotations in 1/4096 turns for tracks 0 and 1.
static const AnimationPackedPose _gM93rPlayerAimEnterAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/m93r_animation_0BBE8_bank1.inc"
};

/// Aim-entry Euler rotations in eight-angle-unit steps for player tracks 2..18.
static const AnimationPackedRotation _gM93rPlayerAimEnterAnimationBank4[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m93r_animation_0BBE8_bank4.inc"
};

/// Aim-entry keyframes and stops; durations count normal-rate frames and pose offsets count u32 words.
static const AnimationRecord _gM93rPlayerAimEnterAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m93r_animation_0BBE8_records.inc"
};

/// Absolute aim-entry record starts for tracks 0..18, followed by the retained serialized halfword.
static const u16 _gM93rPlayerAimEnterAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m93r_animation_0BBE8_indices.inc"
};

/// Player transition into the M93R aiming stance.
///
/// Native clip 7, also selected by bank entry 34 and borrowed for recovery
/// from an enemy hold. All 19 tracks end in stops without cue bits; tracks
/// 0..1 use translation/rotation and 2..18 packed rotation.
/// Playback borrows the set and tables; keep the M93R package loaded while
/// any slot or cached record pointer refers to them.
static AnimationSet _gM93rPlayerAimEnterAnimation = {
    .records           = _gM93rPlayerAimEnterAnimationRecords,
    .trackStartIndices = _gM93rPlayerAimEnterAnimationIndices,
    .poseBanks         = {
        [M93R_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM93rPlayerAimEnterAnimationBank1,
        [M93R_ANIMATION_PACKED_ROTATION_BANK]      = _gM93rPlayerAimEnterAnimationBank4,
    },
};

/// Aim-exit translations in model units and Euler rotations in 1/4096 turns for tracks 0 and 1.
static const AnimationPackedPose _gM93rPlayerAimExitAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/m93r_animation_0C2B8_bank1.inc"
};

/// Aim-exit Euler rotations in eight-angle-unit steps for player tracks 2..18.
static const AnimationPackedRotation _gM93rPlayerAimExitAnimationBank4[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m93r_animation_0C2B8_bank4.inc"
};

/// Aim-exit keyframes and stops; durations count normal-rate frames and pose offsets count u32 words.
static const AnimationRecord _gM93rPlayerAimExitAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m93r_animation_0C2B8_records.inc"
};

/// Absolute aim-exit record starts for tracks 0..18, followed by the retained serialized halfword.
static const u16 _gM93rPlayerAimExitAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m93r_animation_0C2B8_indices.inc"
};

/// Player transition out of the M93R aiming stance.
///
/// Native clip 8; bank entry 35 also selects this set.
/// All 19 tracks end in stops without cue bits; tracks 0..1 use
/// translation/rotation and 2..18 packed rotation.
/// Playback borrows the set and tables; keep the M93R package loaded while
/// any slot or cached record pointer refers to them.
static AnimationSet _gM93rPlayerAimExitAnimation = {
    .records           = _gM93rPlayerAimExitAnimationRecords,
    .trackStartIndices = _gM93rPlayerAimExitAnimationIndices,
    .poseBanks         = {
        [M93R_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM93rPlayerAimExitAnimationBank1,
        [M93R_ANIMATION_PACKED_ROTATION_BANK]      = _gM93rPlayerAimExitAnimationBank4,
    },
};

/// Forward-walk translations in model units and Euler rotations in 1/4096 turns for tracks 0 and 1.
static const AnimationPackedPose _gM93rPlayerWalkAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/m93r_animation_0CC04_bank1.inc"
};

/// Forward-walk Euler rotations in eight-angle-unit steps for player tracks 2..18.
static const AnimationPackedRotation _gM93rPlayerWalkAnimationBank4[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m93r_animation_0CC04_bank4.inc"
};

/// Forward-walk keyframes, loop jumps and retained stops.
///
/// Durations count normal-rate frames and pose offsets count u32 words.
/// Track 1 carries alternating footstep cues `ANIMATION_RECORD_CUE_1` and
/// `ANIMATION_RECORD_CUE_2`; jump offsets are absolute record indices.
static const AnimationRecord _gM93rPlayerWalkAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m93r_animation_0CC04_records.inc"
};

/// Absolute forward-walk record starts for tracks 0..18, followed by the retained serialized halfword.
static const u16 _gM93rPlayerWalkAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m93r_animation_0CC04_indices.inc"
};

/// Player forward walk while holding the M93R.
///
/// Native clip 2 is selected when the weapon model is attached, including
/// scripted walks. All 19 tracks loop, with footstep cues on track 1;
/// tracks 0..1 use translation/rotation and 2..18 packed rotation.
/// Playback borrows the set and tables; keep the M93R package loaded while
/// any slot or cached record pointer refers to them.
static AnimationSet _gM93rPlayerWalkAnimation = {
    .records           = _gM93rPlayerWalkAnimationRecords,
    .trackStartIndices = _gM93rPlayerWalkAnimationIndices,
    .poseBanks         = {
        [M93R_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM93rPlayerWalkAnimationBank1,
        [M93R_ANIMATION_PACKED_ROTATION_BANK]      = _gM93rPlayerWalkAnimationBank4,
    },
};

/// Backward-walk translations in model units and Euler rotations in 1/4096 turns for tracks 0 and 1.
static const AnimationPackedPose _gM93rPlayerBackwardWalkAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/m93r_animation_0D4DC_bank1.inc"
};

/// Backward-walk Euler rotations in eight-angle-unit steps for player tracks 2..18.
static const AnimationPackedRotation _gM93rPlayerBackwardWalkAnimationBank4[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m93r_animation_0D4DC_bank4.inc"
};

/// Backward-walk keyframes, loop jumps and retained stops.
///
/// Durations count normal-rate frames and pose offsets count u32 words.
/// Track 1 carries alternating footstep cues `ANIMATION_RECORD_CUE_1` and
/// `ANIMATION_RECORD_CUE_2`; jump offsets are absolute record indices.
static const AnimationRecord _gM93rPlayerBackwardWalkAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m93r_animation_0D4DC_records.inc"
};

/// Absolute backward-walk record starts for tracks 0..18, followed by the retained serialized halfword.
static const u16 _gM93rPlayerBackwardWalkAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m93r_animation_0D4DC_indices.inc"
};

/// Player backward walk in normal locomotion with the M93R.
///
/// Native clip 3 is selected for negative movement input. All 19 tracks loop,
/// with footstep cues on track 1; tracks 0..1 use translation/rotation and
/// 2..18 packed rotation.
/// Playback borrows the set and tables; keep the M93R package loaded while
/// any slot or cached record pointer refers to them.
static AnimationSet _gM93rPlayerBackwardWalkAnimation = {
    .records           = _gM93rPlayerBackwardWalkAnimationRecords,
    .trackStartIndices = _gM93rPlayerBackwardWalkAnimationIndices,
    .poseBanks         = {
        [M93R_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM93rPlayerBackwardWalkAnimationBank1,
        [M93R_ANIMATION_PACKED_ROTATION_BANK]      = _gM93rPlayerBackwardWalkAnimationBank4,
    },
};

/// Run translations in model units and rotations in 1/4096 turns for tracks 0..1.
static const AnimationPackedPose _gM93rPlayerRunAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/m93r_animation_0DC10_bank1.inc"
};

/// Run rotations for tracks 2..18, packed in eight-unit angular steps.
static const AnimationPackedRotation _gM93rPlayerRunAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m93r_animation_0DC10_bank4.inc"
};

/// Run keyframes, loop jumps and retained stops; durations count normal-rate frames.
///
/// Pose offsets count four-byte words; jump targets are absolute record indices.
/// Track 1 carries alternating footstep cues `ANIMATION_RECORD_CUE_1` and
/// `ANIMATION_RECORD_CUE_2`.
static const AnimationRecord _gM93rPlayerRunAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m93r_animation_0DC10_records.inc"
};

/// Absolute run record starts for tracks 0..18, followed by a retained serialized halfword.
static const u16 _gM93rPlayerRunAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m93r_animation_0DC10_indices.inc"
};
STATIC_ASSERT(ARRAY_SIZE(_gM93rPlayerRunAnimationIndices) == GAME_ACTOR_NORMAL_ANIMATION_SLOTS + 1, m93r_run_track_starts);

/// Looping player run with the M93R, at native clip id 4.
///
/// Normal locomotion and scripted run-to playback use child tracks 1..18;
/// track 1 supplies paired footstep cues. Root displacement is driven separately.
/// Tracks 0..1 carry translation/rotation and 2..18 rotation only. Keep the
/// M93R package loaded while any slot or cached record refers to this clip.
static AnimationSet _gM93rPlayerRunAnimation = {
    .records           = _gM93rPlayerRunAnimationRecords,
    .trackStartIndices = _gM93rPlayerRunAnimationIndices,
    .poseBanks         = {
        [M93R_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM93rPlayerRunAnimationBank1,
        [M93R_ANIMATION_PACKED_ROTATION_BANK]      = _gM93rPlayerRunAnimationBank4,
    },
};

AnimationBank gM93rPlayerAnimationBank = {
    .table = {
        .sets = {
            [1]  = &_gM93rPlayerIdleAnimation,
            [2]  = &_gM93rPlayerWalkAnimation,
            [3]  = &_gM93rPlayerBackwardWalkAnimation,
            [4]  = &_gM93rPlayerRunAnimation,
            [5]  = &_gM93rPlayerTurnLeftAnimation,
            [6]  = &_gM93rPlayerTurnRightAnimation,
            [7]  = &_gM93rPlayerAimEnterAnimation,
            [8]  = &_gM93rPlayerAimExitAnimation,
            [9]  = &_gM93rPlayerAimIdleAnimation,
            [10] = &_gM93rPlayerFireAnimation,
            [11] = &_gM93rPlayerIdleAnimation,
            [12] = &_gM93rPlayerAimForwardAnimation,
            [13] = &_gM93rPlayerAimBackwardOrTurnAnimation,
            [14] = &_gM93rPlayerReloadAnimation,
            [15] = &_gM93rPlayerIdleAnimation,
            [16] = &_gM93rPlayerHitPart4Animation,
            [17] = &_gM93rPlayerHitOtherBodyAnimation,
            [18] = &_gM93rPlayerDeathAnimation,
            [19] = &_gM93rPlayerUnarmedWalkAnimation,
            [20] = &_gM93rPlayerReloadAnimation,
            [21] = &_gM93rPlayerIdleAnimation,
            [22] = &_gM93rPlayerIdleAnimation,
            [23] = &_gM93rPlayerHighHealthIdleAnimation,
            [24] = &_gM93rPlayerMidHealthIdleAnimation,
            [25] = &_gM93rPlayerLowHealthIdleAnimation,
            [26] = &_gM93rPlayerParasiteEnergyReleaseFamily0Animation,
            [27] = &_gM93rPlayerParasiteEnergyChargeFamily0Animation,
            [28] = &_gM93rPlayerParasiteEnergyCastFamily0Animation,
            [29] = &_gM93rPlayerParasiteEnergyReleaseFamily1Animation,
            [30] = &_gM93rPlayerParasiteEnergyChargeFamily1Animation,
            [31] = &_gM93rPlayerParasiteEnergyCastFamily1Animation,
            [32] = &_gM93rPlayerScriptedPresentationEnterAnimation,
            [33] = &_gM93rPlayerScriptedPresentationExitAnimation,
            [34] = &_gM93rPlayerAimEnterAnimation,
            [35] = &_gM93rPlayerAimExitAnimation,
            [36] = &_gM93rPlayerStairAscentAnimation,
            [37] = &_gM93rPlayerStairDescentAnimation,
            [38] = &_gM93rPlayerStairAscentFinishEvenAnimation,
            [39] = &_gM93rPlayerStairAscentFinishOddAnimation,
            [40] = &_gM93rPlayerItemUseAnimation,
            [41] = &_gM93rPlayerIdleAnimation,
            [42] = &_gM93rPlayerParasiteEnergyReleaseFamily2Animation,
            [43] = &_gM93rPlayerParasiteEnergyChargeFamily2Animation,
            [44] = &_gM93rPlayerParasiteEnergyCastFamily2Animation,
            // Unused native slots and the writable extension start cleared.
        },
    },
};
