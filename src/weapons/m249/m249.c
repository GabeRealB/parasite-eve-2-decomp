#include "weapons/m249.h"

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
#include "main/sound.h"
#include "main/session_types.h"
#include "main/task_types.h"

/// Scratch-stack block for the M249's attack handler.
///
/// The handler reserves one block each frame and releases it before
/// returning; the block is not cleared. It stages the node a fired round's
/// impact sound is placed at.
///
/// Only the translation of `impactCoord`'s composed transform is written, and
/// only when the impact picker reports a hit, which is the one case the node
/// is then read in. A sound's pan and depth come from projecting the node's
/// origin, to which its rotation contributes nothing, so the rest of the node
/// is whatever the scratch stack last held. The position is a weapon contact
/// point offset by up to 7 units per axis, in the space the weapon node's
/// composed transform is expressed in.
///
/// The 0x18 bytes ahead of the node are reserved with it and never accessed.
typedef struct {
    byte     field_0[0x18]; // No recovered access; role unproven
    GfxCoord impactCoord;   // Node standing at the round's impact point: the source of the impact sound
} _M249AttackScratch;
STATIC_ASSERT_SIZEOF(_M249AttackScratch, 0x68);

#include "weapons/weapon.h"

enum { M249_IMPACT_SOUND = SOUND_COMMON(0x17) };

/// Ends M249 shot contact tests and requests the chosen surface's impact effect and sound.
///
/// Clears only the weapon body's grid/pair enables; keeps it linked and its
/// complete six-entry contact table intact. Requires live player/room state,
/// initialized contacts and `rootCoord->workm` in the contacts' view frame.
/// Enemy-body hits suppress surface selection. A selected eligible grid point
/// receives 0..7 units of XYZ jitter; only `impactCoord`'s three cached
/// translation words are written. Failure leaves that temporary node untouched.
/// On success, requests the common impact sound synchronously and latches
/// combat noise. Scratch/GTE requirements follow the picker and sound APIs;
/// this helper neither releases nor retains the caller's writable impact node.
static inline void _m249ResolveShotImpact(GameActor* actor, const GfxCoord* rootCoord, GfxCoord* impactCoord)
{
    enum { M249_IMPACT_SIGNAL_NOISE = 1 };

    actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
    if (playerActorSpawnWeaponImpact(actor->weaponContacts, rootCoord, impactCoord) != 0) {
        worldCoordPlaySound(impactCoord, M249_IMPACT_SOUND, M249_IMPACT_SIGNAL_NOISE);
    }
}

void m249AttackState(Task* playerTask)
{
    enum {
        M249_PHASE_PREPARE             = 0,
        M249_PHASE_WAIT_READY          = 1,
        M249_PHASE_ARM_SHOT            = 2,
        M249_PHASE_SHOT_DELAY          = 3,
        M249_PHASE_IMPACT              = 4,
        M249_PHASE_RECOVER             = 5,
        M249_PLAYER_ATTACK_STATE       = 4,
        M249_ANIMATION_READY           = 9,
        M249_ANIMATION_PRIMARY         = 0xA,
        M249_READY_BLEND_FRAMES        = 1,
        M249_WEAPON_ID                 = 17,
        M249_MOVING_READY_BLEND_FRAMES = 8,
        M249_SHOT_DELAY_FRAMES         = 2,
        M249_FIRE_SOUND                = SOUND_WEAPON(17, 4),
    };
    GameActor*          actor;
    GfxCoord*           rootCoord;
    GfxCoord*           impactCoord;
    _M249AttackScratch* scratch;
    s32                 readyBlendFrames;

    scratch   = SCRATCH_STACK_RESERVE_BLOCK(_M249AttackScratch);
    actor     = playerTask->work;
    rootCoord = playerTask->extra.tmd->coords;
    switch (actor->statePhase) {
        case M249_PHASE_PREPARE:
            readyBlendFrames      = M249_READY_BLEND_FRAMES;
            actor->state          = M249_PLAYER_ATTACK_STATE;
            actor->turnRateIndex  = 2;
            actor->mode           = GAME_ACTOR_MODE_NORMAL;
            actor->animationState = 0;
            actor->statePhase++;
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= (WORLD_COLLISION_BODY_CLIP_TO_GRID_CONTACT | WORLD_COLLISION_BODY_SINGLE_CONTACT);
            if (((u16)actor->movementMode | actor->turnSign) != 0) {
                readyBlendFrames = M249_MOVING_READY_BLEND_FRAMES;
            }
            playerActorPlayChildSlotsWithBlend(playerTask, M249_ANIMATION_READY, 0, readyBlendFrames);
            actor->movementMode = 0;
            break;
        case M249_PHASE_WAIT_READY:
            if (animationGetCurrentRecord(&actor->animationContext, actor->animationSlots + 1) !=
                NULL) {
                actor->statePhase++;
            }
            break;
        case M249_PHASE_ARM_SHOT:
        fire:
            actor->statePhase                  = M249_PHASE_SHOT_DELAY;
            actor->attackControl.cooldownTicks = 0;
            actor->rumblePosted                = 0;
            actor->stateTimer                  = M249_SHOT_DELAY_FRAMES;
            playerActorSetWeaponAttackFlags(playerTask, 0, actor->attackButton != PLAYER_ACTOR_ATTACK_BUTTON_PRIMARY);
            /* fallthrough */
        case M249_PHASE_SHOT_DELAY:
            // The arm dispatch consumes the first of the two delay ticks.
            if (--actor->stateTimer == 0) {
                actor->statePhase++;
                actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
                equipmentConsumeWeaponLoad(WEAPON_ITEM(M249_WEAPON_ID), EQUIPMENT_WEAPON_LOAD_CONSUME_PRIMARY);
                worldCoordPlaySound(playerTask->extra.tmd->coords, M249_FIRE_SOUND, 1);
                effectSpawn(EFFECT_RIFLE_MUZZLE_FLASH,
                            actor->equipmentTasks[1]->extra.tmd->coords, M249_WEAPON_ID,
                            NULL);
                playerActorPlayChildSlotsWithBlend(playerTask, M249_ANIMATION_PRIMARY, 1, M249_SHOT_DELAY_FRAMES);
            }
            break;
        case M249_PHASE_IMPACT:
            impactCoord         = &scratch->impactCoord;
            actor->movementSign = 0;
            actor->statePhase++;
            _m249ResolveShotImpact(actor, rootCoord, impactCoord);
            /* fallthrough */
        case M249_PHASE_RECOVER:
            if (playerActorReadAttackButton(playerTask) != 0 && playerActorQueryWeaponLoads(PLAYER_ACTOR_WEAPON_LOAD_PRIMARY) > 0) {
                goto fire;
            }
            if (playerActorIsSlotAdvancingLinearly(playerTask, D_80112E04[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId][1], 0, 0) == 0) {
                playerActorFinishWeaponAttack(playerTask);
            }
            break;
    }
    playerActorTrackLockTarget(playerTask);
    SCRATCH_STACK_RELEASE_BLOCK(_M249AttackScratch);
}

/// Initial root transform copied into each held-weapon model's coordinate.
///
/// Read-only skeleton and count tables retain their serialized .data positions.
static const TmdBone _gM249WeaponModelSkeleton[] __attribute__((section(".data"))) = {
#include "assets/m249_model_0060C_skeleton.inc"
};

/// Vertex count for the weapon's single skeletal part: all 58 vertices.
static const u32 _gM249WeaponModelPartVerts[ARRAY_SIZE(_gM249WeaponModelSkeleton)] __attribute__((section(".data"))) = {
#include "assets/m249_model_0060C_partVerts.inc"
};

/// Weapon vertices in integer coordinates local to the held model's root.
static SVECTOR _gM249WeaponModelVerts[] = {
#include "assets/m249_model_0060C_verts.inc"
};

/// Lighting normals with twelve fractional bits, indexed independently of vertices.
static SVECTOR _gM249WeaponModelNormals[] = {
#include "assets/m249_model_0060C_normals.inc"
};

/// Writable commands for ten textured triangles and 51 textured quads under one root.
///
/// Creation resolves handler slots in place; the final word is `TMD_STREAM_END`.
static u32 _gM249WeaponModelStream[] = {
#include "assets/m249_model_0060C_stream.inc"
};

TmdSource gM249WeaponModel = {
    .handlersResolved    = false,
    .bufferHalfBytes     = (10 * sizeof(POLY_GT3)) + (51 * sizeof(POLY_GT4)),
    .preXformRegionBytes = 0,
    .partCount           = ARRAY_SIZE(_gM249WeaponModelSkeleton),
    .partVertexCounts    = _gM249WeaponModelPartVerts,
    .verts               = _gM249WeaponModelVerts,
    .normals             = _gM249WeaponModelNormals,
    .skeleton            = _gM249WeaponModelSkeleton,
    .stream              = _gM249WeaponModelStream,
};

/// Pose encodings used as bank indices by the M249's nineteen-track clips.
enum {
    M249_ANIMATION_TRANSLATION_ROTATION_BANK = 1,
    M249_ANIMATION_PACKED_ROTATION_BANK      = 4,
};

/// Translation and full-resolution Euler poses for the M249 standing idle (encoding 1).
///
/// Read-only clip payloads retain their serialized .data positions.
static const AnimationPackedPose _gM249IdleAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/m249_animation_00E68_bank1.inc"
};

/// Packed Euler rotations for the M249 standing idle (encoding 4, eight angle units per step).
static const AnimationPackedRotation _gM249IdleAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m249_animation_00E68_bank4.inc"
};

/// Keyframes and loop/stop controls for the M249's fixed standing pose.
static const AnimationRecord _gM249IdleAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m249_animation_00E68_records.inc"
};

/// Nineteen absolute track-start record indices and the serialized trailing zero halfword.
static const u16 _gM249IdleAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m249_animation_00E68_indices.inc"
};

/// Standing idle for the player carrying the M249, native-bank set 1.
///
/// Also reused by sets 21, 22 and 41. Tracks 0..18 belong to the player's rig;
/// ordinary child playback uses 1..18. The clip data must stay loaded during playback.
static AnimationSet _gM249IdleAnimation = {
    .records           = _gM249IdleAnimationRecords,
    .trackStartIndices = _gM249IdleAnimationIndices,
    .poseBanks         = {
        [M249_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM249IdleAnimationBank1,
        [M249_ANIMATION_PACKED_ROTATION_BANK]      = _gM249IdleAnimationBank4,
    },
};

/// Translation and full-resolution Euler poses for walking without the M249 model (encoding 1).
static const AnimationPackedPose _gM249UnarmedWalkAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/m249_animation_0150C_bank1.inc"
};

/// Packed Euler rotations for walking without the M249 model (encoding 4, eight angle units per step).
static const AnimationPackedRotation _gM249UnarmedWalkAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m249_animation_0150C_bank4.inc"
};

/// Keyframes and loop/stop controls for the M249 bank's walk without a held weapon model.
static const AnimationRecord _gM249UnarmedWalkAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m249_animation_0150C_records.inc"
};

/// Nineteen absolute track-start record indices and the serialized trailing zero halfword.
static const u16 _gM249UnarmedWalkAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m249_animation_0150C_indices.inc"
};

/// Forward walk while the M249 bank is bound and the held weapon task is absent, set 19.
///
/// The nineteen player-rig tracks repeat their walk poses; child playback uses
/// tracks 1..18. Keep the clip data loaded while any slot refers to it.
static AnimationSet _gM249UnarmedWalkAnimation = {
    .records           = _gM249UnarmedWalkAnimationRecords,
    .trackStartIndices = _gM249UnarmedWalkAnimationIndices,
    .poseBanks         = {
        [M249_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM249UnarmedWalkAnimationBank1,
        [M249_ANIMATION_PACKED_ROTATION_BANK]      = _gM249UnarmedWalkAnimationBank4,
    },
};

/// Translation and full-resolution Euler poses for the M249 left turn (encoding 1).
static const AnimationPackedPose _gM249TurnLeftAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/m249_animation_01D6C_bank1.inc"
};

/// Packed Euler rotations for the M249 left turn (encoding 4, eight angle units per step).
static const AnimationPackedRotation _gM249TurnLeftAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m249_animation_01D6C_bank4.inc"
};

/// Keyframes and loop/stop controls for the M249 left-turn cycle.
static const AnimationRecord _gM249TurnLeftAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m249_animation_01D6C_records.inc"
};

/// Nineteen absolute track-start record indices and the serialized trailing zero halfword.
static const u16 _gM249TurnLeftAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m249_animation_01D6C_indices.inc"
};

/// Stationary left turn with the M249, native-bank set 5.
///
/// Used for left input and scripted negative-yaw turns. Nineteen player-rig
/// tracks provide a repeating turn cycle; keep their data loaded during playback.
static AnimationSet _gM249TurnLeftAnimation = {
    .records           = _gM249TurnLeftAnimationRecords,
    .trackStartIndices = _gM249TurnLeftAnimationIndices,
    .poseBanks         = {
        [M249_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM249TurnLeftAnimationBank1,
        [M249_ANIMATION_PACKED_ROTATION_BANK]      = _gM249TurnLeftAnimationBank4,
    },
};

/// Translation and full-resolution Euler poses for the M249 right turn (encoding 1).
static const AnimationPackedPose _gM249TurnRightAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/m249_animation_025D0_bank1.inc"
};

/// Packed Euler rotations for the M249 right turn (encoding 4, eight angle units per step).
static const AnimationPackedRotation _gM249TurnRightAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m249_animation_025D0_bank4.inc"
};

/// Keyframes and loop/stop controls for the M249 right-turn cycle.
static const AnimationRecord _gM249TurnRightAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m249_animation_025D0_records.inc"
};

/// Nineteen absolute track-start record indices and the serialized trailing zero halfword.
static const u16 _gM249TurnRightAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m249_animation_025D0_indices.inc"
};

/// Stationary right turn with the M249, native-bank set 6.
///
/// Used for right input and scripted nonnegative-yaw turns. Nineteen player-rig
/// tracks provide a repeating turn cycle; keep their data loaded during playback.
static AnimationSet _gM249TurnRightAnimation = {
    .records           = _gM249TurnRightAnimationRecords,
    .trackStartIndices = _gM249TurnRightAnimationIndices,
    .poseBanks         = {
        [M249_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM249TurnRightAnimationBank1,
        [M249_ANIMATION_PACKED_ROTATION_BANK]      = _gM249TurnRightAnimationBank4,
    },
};

/// Translation and full-resolution Euler poses for M249-bank item use (encoding 1).
static const AnimationPackedPose _gM249ItemUseAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/m249_animation_028E4_bank1.inc"
};

/// Packed Euler rotations for M249-bank item use (encoding 4, eight angle units per step).
static const AnimationPackedRotation _gM249ItemUseAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m249_animation_028E4_bank4.inc"
};

/// Keyframes and terminal stop controls for the M249 bank's item-use gesture.
static const AnimationRecord _gM249ItemUseAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m249_animation_028E4_records.inc"
};

/// Nineteen absolute track-start record indices and the serialized trailing zero halfword.
static const u16 _gM249ItemUseAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m249_animation_028E4_indices.inc"
};

/// Item-use gesture for the player with the M249 bank bound, native-bank set 40.
///
/// Its nineteen rig tracks stop at their final poses; ordinary child playback
/// uses tracks 1..18. Keep the clip data loaded while any slot refers to it.
static AnimationSet _gM249ItemUseAnimation = {
    .records           = _gM249ItemUseAnimationRecords,
    .trackStartIndices = _gM249ItemUseAnimationIndices,
    .poseBanks         = {
        [M249_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM249ItemUseAnimationBank1,
        [M249_ANIMATION_PACKED_ROTATION_BANK]      = _gM249ItemUseAnimationBank4,
    },
};

/// Translation and full-resolution Euler poses for the M249 stair ascent (encoding 1).
///
/// Read-only clip payloads retain their serialized .data positions.
static const AnimationPackedPose _gM249StairAscentAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/m249_animation_03040_bank1.inc"
};

/// Packed Euler rotations for the M249 stair ascent (encoding 4, eight angle units per step).
static const AnimationPackedRotation _gM249StairAscentAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m249_animation_03040_bank4.inc"
};

/// Stair-ascent keyframes, track loops and stop controls, with footstep cues on track 1.
static const AnimationRecord _gM249StairAscentAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m249_animation_03040_records.inc"
};

/// Nineteen absolute track-start record indices and the serialized trailing zero halfword.
static const u16 _gM249StairAscentAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m249_animation_03040_indices.inc"
};

/// Repeating stair-ascent stride for the player with the M249 bank bound, native-bank set 36.
///
/// Its nineteen rig tracks loop; ordinary child playback uses tracks 1..18.
/// Track 1's footstep cues gate stair displacement. Keep the clip data loaded
/// while any slot refers to it.
static AnimationSet _gM249StairAscentAnimation = {
    .records           = _gM249StairAscentAnimationRecords,
    .trackStartIndices = _gM249StairAscentAnimationIndices,
    .poseBanks         = {
        [M249_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM249StairAscentAnimationBank1,
        [M249_ANIMATION_PACKED_ROTATION_BANK]      = _gM249StairAscentAnimationBank4,
    },
};

/// Translation and full-resolution Euler poses for the M249 stair descent (encoding 1).
static const AnimationPackedPose _gM249StairDescentAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/m249_animation_037C8_bank1.inc"
};

/// Packed Euler rotations for the M249 stair descent (encoding 4, eight angle units per step).
static const AnimationPackedRotation _gM249StairDescentAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m249_animation_037C8_bank4.inc"
};

/// Stair-descent keyframes, track loops and stop controls, with footstep cues on track 1.
static const AnimationRecord _gM249StairDescentAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m249_animation_037C8_records.inc"
};

/// Nineteen absolute track-start record indices and the serialized trailing zero halfword.
static const u16 _gM249StairDescentAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m249_animation_037C8_indices.inc"
};

/// Repeating stair-descent stride for the player with the M249 bank bound, native-bank set 37.
///
/// Its nineteen rig tracks loop; ordinary child playback uses tracks 1..18.
/// Track 1's footstep cues gate stair displacement. Keep the clip data loaded
/// while any slot refers to it.
static AnimationSet _gM249StairDescentAnimation = {
    .records           = _gM249StairDescentAnimationRecords,
    .trackStartIndices = _gM249StairDescentAnimationIndices,
    .poseBanks         = {
        [M249_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM249StairDescentAnimationBank1,
        [M249_ANIMATION_PACKED_ROTATION_BANK]      = _gM249StairDescentAnimationBank4,
    },
};

/// Translation and full-resolution Euler poses for an odd-count M249 stair-ascent finish (encoding 1).
static const AnimationPackedPose _gM249StairAscentFinishOddAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/m249_animation_03A9C_bank1.inc"
};

/// Packed Euler rotations for an odd-count M249 stair-ascent finish (encoding 4, eight angle units per step).
static const AnimationPackedRotation _gM249StairAscentFinishOddAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m249_animation_03A9C_bank4.inc"
};

/// Odd-count stair-ascent finish keyframes and stop controls, with a footstep cue on track 1.
static const AnimationRecord _gM249StairAscentFinishOddAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m249_animation_03A9C_records.inc"
};

/// Nineteen absolute track-start record indices and the serialized trailing zero halfword.
static const u16 _gM249StairAscentFinishOddAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m249_animation_03A9C_indices.inc"
};

/// Stair-ascent finish after an odd original step count, M249 native-bank set 39.
///
/// Plays the level stride at the top of the flight. Its nineteen rig tracks
/// stop at their final poses; ordinary child playback uses tracks 1..18.
/// Keep the clip data loaded while any slot refers to it.
static AnimationSet _gM249StairAscentFinishOddAnimation = {
    .records           = _gM249StairAscentFinishOddAnimationRecords,
    .trackStartIndices = _gM249StairAscentFinishOddAnimationIndices,
    .poseBanks         = {
        [M249_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM249StairAscentFinishOddAnimationBank1,
        [M249_ANIMATION_PACKED_ROTATION_BANK]      = _gM249StairAscentFinishOddAnimationBank4,
    },
};

/// Translation and full-resolution Euler poses for an even-count M249 stair-ascent finish (encoding 1).
static const AnimationPackedPose _gM249StairAscentFinishEvenAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/m249_animation_03E2C_bank1.inc"
};

/// Packed Euler rotations for an even-count M249 stair-ascent finish (encoding 4, eight angle units per step).
static const AnimationPackedRotation _gM249StairAscentFinishEvenAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m249_animation_03E2C_bank4.inc"
};

/// Even-count stair-ascent finish keyframes and stop controls, with a footstep cue on track 1.
static const AnimationRecord _gM249StairAscentFinishEvenAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m249_animation_03E2C_records.inc"
};

/// Nineteen absolute track-start record indices and the serialized trailing zero halfword.
static const u16 _gM249StairAscentFinishEvenAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m249_animation_03E2C_indices.inc"
};

/// Stair-ascent finish after an even original step count, M249 native-bank set 38.
///
/// Plays the level stride at the top of the flight. Its nineteen rig tracks
/// stop at their final poses; ordinary child playback uses tracks 1..18.
/// Keep the clip data loaded while any slot refers to it.
static AnimationSet _gM249StairAscentFinishEvenAnimation = {
    .records           = _gM249StairAscentFinishEvenAnimationRecords,
    .trackStartIndices = _gM249StairAscentFinishEvenAnimationIndices,
    .poseBanks         = {
        [M249_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM249StairAscentFinishEvenAnimationBank1,
        [M249_ANIMATION_PACKED_ROTATION_BANK]      = _gM249StairAscentFinishEvenAnimationBank4,
    },
};

/// Translation and full-resolution Euler poses for M249 Parasite Energy family-0 release (encoding 1).
static const AnimationPackedPose _gM249ParasiteEnergyFamily0ReleaseAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/m249_animation_042B4_bank1.inc"
};

/// Packed Euler rotations for M249 Parasite Energy family-0 release (encoding 4, eight angle units per step).
static const AnimationPackedRotation _gM249ParasiteEnergyFamily0ReleaseAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m249_animation_042B4_bank4.inc"
};

/// Keyframes and terminal stop controls for M249 Parasite Energy family-0 release.
static const AnimationRecord _gM249ParasiteEnergyFamily0ReleaseAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m249_animation_042B4_records.inc"
};

/// Nineteen absolute track-start record indices and the serialized trailing zero halfword.
static const u16 _gM249ParasiteEnergyFamily0ReleaseAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m249_animation_042B4_indices.inc"
};

/// Parasite Energy release gesture for clip family 0, M249 native-bank set 26.
///
/// Plays when the attachment countdown starts, before family 0's charge clip.
/// Family 0 serves attachment IDs 300..600 whose tens digit is not 3, and
/// IDs at least 601 whose tens digit is 1. Its nineteen rig tracks stop at
/// their final poses; ordinary child playback uses tracks 1..18. Keep the clip
/// data loaded while any slot refers to it.
static AnimationSet _gM249ParasiteEnergyFamily0ReleaseAnimation = {
    .records           = _gM249ParasiteEnergyFamily0ReleaseAnimationRecords,
    .trackStartIndices = _gM249ParasiteEnergyFamily0ReleaseAnimationIndices,
    .poseBanks         = {
        [M249_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM249ParasiteEnergyFamily0ReleaseAnimationBank1,
        [M249_ANIMATION_PACKED_ROTATION_BANK]      = _gM249ParasiteEnergyFamily0ReleaseAnimationBank4,
    },
};

/// Translation and full-resolution Euler poses for M249 Parasite Energy family-0 charge (encoding 1).
static const AnimationPackedPose _gM249ParasiteEnergyFamily0ChargeAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/m249_animation_044B0_bank1.inc"
};

/// Packed Euler rotations for M249 Parasite Energy family-0 charge (encoding 4, eight angle units per step).
static const AnimationPackedRotation _gM249ParasiteEnergyFamily0ChargeAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m249_animation_044B0_bank4.inc"
};

/// Charge-pose keyframes, track loops and stop controls for M249 Parasite Energy family 0.
static const AnimationRecord _gM249ParasiteEnergyFamily0ChargeAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m249_animation_044B0_records.inc"
};

/// Nineteen absolute track-start record indices and the serialized trailing zero halfword.
static const u16 _gM249ParasiteEnergyFamily0ChargeAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m249_animation_044B0_indices.inc"
};

/// Repeating Parasite Energy charge pose for clip family 0, M249 native-bank set 27.
///
/// Holds while the attachment countdown is nonzero, between
/// `_gM249ParasiteEnergyFamily0ReleaseAnimation` and the family-0 cast clip.
/// Its nineteen rig tracks loop; ordinary child playback uses tracks 1..18.
/// Keep the clip data loaded while any slot refers to it.
static AnimationSet _gM249ParasiteEnergyFamily0ChargeAnimation = {
    .records           = _gM249ParasiteEnergyFamily0ChargeAnimationRecords,
    .trackStartIndices = _gM249ParasiteEnergyFamily0ChargeAnimationIndices,
    .poseBanks         = {
        [M249_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM249ParasiteEnergyFamily0ChargeAnimationBank1,
        [M249_ANIMATION_PACKED_ROTATION_BANK]      = _gM249ParasiteEnergyFamily0ChargeAnimationBank4,
    },
};

/// Local translations and 1/4096-turn Euler poses for M249 Parasite Energy family-0 cast (encoding 1).
///
/// Read-only clip payloads retain their serialized .data positions.
static const AnimationPackedPose _gM249ParasiteEnergyFamily0CastAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/m249_animation_04788_bank1.inc"
};

/// Packed Euler rotations for M249 Parasite Energy family-0 cast (encoding 4, eight angle units per step).
static const AnimationPackedRotation _gM249ParasiteEnergyFamily0CastAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m249_animation_04788_bank4.inc"
};

/// Keyframes and final-pose stop controls for M249 Parasite Energy family-0 cast.
static const AnimationRecord _gM249ParasiteEnergyFamily0CastAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m249_animation_04788_records.inc"
};

/// Nineteen absolute track-start record indices and the serialized trailing zero halfword.
static const u16 _gM249ParasiteEnergyFamily0CastAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m249_animation_04788_indices.inc"
};

/// Parasite Energy cast gesture for clip family 0, M249 native-bank set 28.
///
/// Starts when the attachment countdown expires, after the family-0 charge
/// clip. All nineteen rig tracks stop at their final poses; ordinary child
/// playback uses tracks 1..18. Keep the clip data loaded while any slot or
/// cached record refers to it.
static AnimationSet _gM249ParasiteEnergyFamily0CastAnimation = {
    .records           = _gM249ParasiteEnergyFamily0CastAnimationRecords,
    .trackStartIndices = _gM249ParasiteEnergyFamily0CastAnimationIndices,
    .poseBanks         = {
        [M249_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM249ParasiteEnergyFamily0CastAnimationBank1,
        [M249_ANIMATION_PACKED_ROTATION_BANK]      = _gM249ParasiteEnergyFamily0CastAnimationBank4,
    },
};

/// Local translations and 1/4096-turn Euler poses for M249 Parasite Energy family-1 release (encoding 1).
static const AnimationPackedPose _gM249ParasiteEnergyFamily1ReleaseAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/m249_animation_04A2C_bank1.inc"
};

/// Packed Euler rotations for M249 Parasite Energy family-1 release (encoding 4, eight angle units per step).
static const AnimationPackedRotation _gM249ParasiteEnergyFamily1ReleaseAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m249_animation_04A2C_bank4.inc"
};

/// Keyframes and final-pose stop controls for M249 Parasite Energy family-1 release.
static const AnimationRecord _gM249ParasiteEnergyFamily1ReleaseAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m249_animation_04A2C_records.inc"
};

/// Nineteen absolute track-start record indices and the serialized trailing zero halfword.
static const u16 _gM249ParasiteEnergyFamily1ReleaseAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m249_animation_04A2C_indices.inc"
};

/// Parasite Energy release gesture for clip family 1, M249 native-bank set 29.
///
/// Plays when the attachment countdown starts, before the family-1 charge
/// clip. Family 1 serves attachment IDs below 300 whose tens digit is not 3,
/// and IDs at least 601 whose tens digit is not 1. All nineteen rig tracks
/// stop at their final poses; ordinary child playback uses tracks 1..18.
/// Keep the clip data loaded while any slot or cached record refers to it.
static AnimationSet _gM249ParasiteEnergyFamily1ReleaseAnimation = {
    .records           = _gM249ParasiteEnergyFamily1ReleaseAnimationRecords,
    .trackStartIndices = _gM249ParasiteEnergyFamily1ReleaseAnimationIndices,
    .poseBanks         = {
        [M249_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM249ParasiteEnergyFamily1ReleaseAnimationBank1,
        [M249_ANIMATION_PACKED_ROTATION_BANK]      = _gM249ParasiteEnergyFamily1ReleaseAnimationBank4,
    },
};

/// Local translations and 1/4096-turn Euler poses for M249 Parasite Energy family-1 charge (encoding 1).
static const AnimationPackedPose _gM249ParasiteEnergyFamily1ChargeAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/m249_animation_04C2C_bank1.inc"
};

/// Packed Euler rotations for M249 Parasite Energy family-1 charge (encoding 4, eight angle units per step).
static const AnimationPackedRotation _gM249ParasiteEnergyFamily1ChargeAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m249_animation_04C2C_bank4.inc"
};

/// Charge-pose keyframes, track loops and serialized stop controls for M249 Parasite Energy family 1.
static const AnimationRecord _gM249ParasiteEnergyFamily1ChargeAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m249_animation_04C2C_records.inc"
};

/// Nineteen absolute track-start record indices and the serialized trailing zero halfword.
static const u16 _gM249ParasiteEnergyFamily1ChargeAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m249_animation_04C2C_indices.inc"
};

/// Repeating Parasite Energy charge pose for clip family 1, M249 native-bank set 30.
///
/// Holds while the attachment countdown is nonzero, between
/// `_gM249ParasiteEnergyFamily1ReleaseAnimation` and the family-1 cast clip.
/// All nineteen rig tracks loop; their serialized stop records follow the
/// loop jumps. Ordinary child playback uses tracks 1..18. Keep the clip data
/// loaded while any slot or cached record refers to it.
static AnimationSet _gM249ParasiteEnergyFamily1ChargeAnimation = {
    .records           = _gM249ParasiteEnergyFamily1ChargeAnimationRecords,
    .trackStartIndices = _gM249ParasiteEnergyFamily1ChargeAnimationIndices,
    .poseBanks         = {
        [M249_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM249ParasiteEnergyFamily1ChargeAnimationBank1,
        [M249_ANIMATION_PACKED_ROTATION_BANK]      = _gM249ParasiteEnergyFamily1ChargeAnimationBank4,
    },
};

/// Local translations and 1/4096-turn Euler poses for M249 Parasite Energy family-1 cast (encoding 1).
static const AnimationPackedPose _gM249ParasiteEnergyFamily1CastAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/m249_animation_04F80_bank1.inc"
};

/// Packed Euler rotations for M249 Parasite Energy family-1 cast (encoding 4, eight angle units per step).
static const AnimationPackedRotation _gM249ParasiteEnergyFamily1CastAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m249_animation_04F80_bank4.inc"
};

/// Keyframes and final-pose stop controls for M249 Parasite Energy family-1 cast.
static const AnimationRecord _gM249ParasiteEnergyFamily1CastAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m249_animation_04F80_records.inc"
};

/// Nineteen absolute track-start record indices and the serialized trailing zero halfword.
static const u16 _gM249ParasiteEnergyFamily1CastAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m249_animation_04F80_indices.inc"
};

/// Parasite Energy cast gesture for clip family 1, M249 native-bank set 31.
///
/// Starts when the attachment countdown expires, after the family-1 charge
/// clip. All nineteen rig tracks stop at their final poses; ordinary child
/// playback uses tracks 1..18. Keep the clip data loaded while any slot or
/// cached record refers to it.
static AnimationSet _gM249ParasiteEnergyFamily1CastAnimation = {
    .records           = _gM249ParasiteEnergyFamily1CastAnimationRecords,
    .trackStartIndices = _gM249ParasiteEnergyFamily1CastAnimationIndices,
    .poseBanks         = {
        [M249_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM249ParasiteEnergyFamily1CastAnimationBank1,
        [M249_ANIMATION_PACKED_ROTATION_BANK]      = _gM249ParasiteEnergyFamily1CastAnimationBank4,
    },
};

/// Local translations and 1/4096-turn Euler poses for M249 part-4 hit reaction (encoding 1).
static const AnimationPackedPose _gM249Part4HitReactionAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/m249_animation_05234_bank1.inc"
};

/// Packed Euler rotations for M249 part-4 hit reaction (encoding 4, eight angle units per step).
static const AnimationPackedRotation _gM249Part4HitReactionAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m249_animation_05234_bank4.inc"
};

/// Keyframes and final-pose stop controls for M249 part-4 hit reaction.
static const AnimationRecord _gM249Part4HitReactionAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m249_animation_05234_records.inc"
};

/// Nineteen absolute track-start record indices and the serialized trailing zero halfword.
static const u16 _gM249Part4HitReactionAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m249_animation_05234_indices.inc"
};

/// Player hit reaction for the part-4 motion body, M249 native-bank set 16.
///
/// Selected for a pending hit on the sphere attached to model part 4
/// (hit region 1), with a three-frame blend on child tracks 1..18.
/// All nineteen rig tracks stop at their final poses. Keep the clip data
/// loaded while any slot or cached record refers to it.
static AnimationSet _gM249Part4HitReactionAnimation = {
    .records           = _gM249Part4HitReactionAnimationRecords,
    .trackStartIndices = _gM249Part4HitReactionAnimationIndices,
    .poseBanks         = {
        [M249_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM249Part4HitReactionAnimationBank1,
        [M249_ANIMATION_PACKED_ROTATION_BANK]      = _gM249Part4HitReactionAnimationBank4,
    },
};

/// Local translations and 1/4096-turn Euler poses for M249 other-body hit reaction (encoding 1).
static const AnimationPackedPose _gM249OtherBodyHitReactionAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/m249_animation_05554_bank1.inc"
};

/// Packed Euler rotations for M249 other-body hit reaction (encoding 4, eight angle units per step).
static const AnimationPackedRotation _gM249OtherBodyHitReactionAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m249_animation_05554_bank4.inc"
};

/// Keyframes and final-pose stop controls for M249 other-body hit reaction.
static const AnimationRecord _gM249OtherBodyHitReactionAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m249_animation_05554_records.inc"
};

/// Nineteen absolute track-start record indices and the serialized trailing zero halfword.
static const u16 _gM249OtherBodyHitReactionAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m249_animation_05554_indices.inc"
};

/// Player hit reaction for the other motion bodies, M249 native-bank set 17.
///
/// Selected for hits on the root or model-part-1 motion sphere
/// (hit region 2), and as the fallback for other pending hit regions.
/// Blends child tracks 1..18 over three frames; all nineteen rig tracks
/// stop at their final poses. Keep the clip data loaded while any slot or
/// cached record refers to it.
static AnimationSet _gM249OtherBodyHitReactionAnimation = {
    .records           = _gM249OtherBodyHitReactionAnimationRecords,
    .trackStartIndices = _gM249OtherBodyHitReactionAnimationIndices,
    .poseBanks         = {
        [M249_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM249OtherBodyHitReactionAnimationBank1,
        [M249_ANIMATION_PACKED_ROTATION_BANK]      = _gM249OtherBodyHitReactionAnimationBank4,
    },
};

/// Local translations and 1/4096-turn Euler poses for M249 death (encoding 1).
///
/// Read-only clip payloads retain their serialized .data positions.
static const AnimationPackedPose _gM249DeathAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/m249_animation_05D18_bank1.inc"
};

/// Packed Euler rotations for M249 death (encoding 4, eight angle units per step).
static const AnimationPackedRotation _gM249DeathAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m249_animation_05D18_bank4.inc"
};

/// Keyframes and final-pose stop controls for M249 death.
static const AnimationRecord _gM249DeathAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m249_animation_05D18_records.inc"
};

/// Nineteen absolute track-start record indices and the serialized trailing zero halfword.
static const u16 _gM249DeathAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m249_animation_05D18_indices.inc"
};

/// Player death motion and final held pose for the M249 bank, native set 18.
///
/// Zero HP enters stopped damage control and selects this clip, with a
/// four-frame blend from normal or scripted control. All nineteen rig tracks
/// stop at their final poses; ordinary child playback uses tracks 1..18.
/// Keep the clip loaded while any slot or cached record refers to it.
static AnimationSet _gM249DeathAnimation = {
    .records           = _gM249DeathAnimationRecords,
    .trackStartIndices = _gM249DeathAnimationIndices,
    .poseBanks         = {
        [M249_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM249DeathAnimationBank1,
        [M249_ANIMATION_PACKED_ROTATION_BANK]      = _gM249DeathAnimationBank4,
    },
};

/// Local translations and 1/4096-turn Euler poses for M249 high-health idle (encoding 1).
static const AnimationPackedPose _gM249HighHealthIdleAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/m249_animation_06FB0_bank1.inc"
};

/// Packed Euler rotations for M249 high-health idle (encoding 4, eight angle units per step).
static const AnimationPackedRotation _gM249HighHealthIdleAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m249_animation_06FB0_bank4.inc"
};

/// Keyframes and loop/stop controls for M249 high-health idle.
static const AnimationRecord _gM249HighHealthIdleAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m249_animation_06FB0_records.inc"
};

/// Nineteen absolute track-start record indices and the serialized trailing zero halfword.
static const u16 _gM249HighHealthIdleAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m249_animation_06FB0_indices.inc"
};

/// Long-idle motion for an M249 player above half maximum HP, native-bank set 23.
///
/// Normal locomotion selects it after 300 direction-free ticks with a
/// five-frame blend. All nineteen rig tracks loop; child playback uses
/// tracks 1..18. Keep the clip loaded while any slot or cached record refers to it.
static AnimationSet _gM249HighHealthIdleAnimation = {
    .records           = _gM249HighHealthIdleAnimationRecords,
    .trackStartIndices = _gM249HighHealthIdleAnimationIndices,
    .poseBanks         = {
        [M249_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM249HighHealthIdleAnimationBank1,
        [M249_ANIMATION_PACKED_ROTATION_BANK]      = _gM249HighHealthIdleAnimationBank4,
    },
};

/// Local translations and 1/4096-turn Euler poses for M249 low-health idle and paralysis (encoding 1).
static const AnimationPackedPose _gM249LowHealthIdleAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/m249_animation_07B28_bank1.inc"
};

/// Packed Euler rotations for M249 low-health idle and paralysis (encoding 4, eight angle units per step).
static const AnimationPackedRotation _gM249LowHealthIdleAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m249_animation_07B28_bank4.inc"
};

/// Keyframes and loop/stop controls for M249 low-health idle and paralysis.
static const AnimationRecord _gM249LowHealthIdleAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m249_animation_07B28_records.inc"
};

/// Nineteen absolute track-start record indices and the serialized trailing zero halfword.
static const u16 _gM249LowHealthIdleAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m249_animation_07B28_indices.inc"
};

/// Long-idle motion at or below quarter maximum HP, also M249 paralysis, native set 25.
///
/// Normal locomotion selects it after 300 direction-free ticks with a
/// five-frame blend. Paralysis selects the same clip with a six-frame blend,
/// regardless of HP. All nineteen rig tracks loop; both paths play child
/// tracks 1..18. Keep the clip loaded while any slot or cached record refers to it.
static AnimationSet _gM249LowHealthIdleAnimation = {
    .records           = _gM249LowHealthIdleAnimationRecords,
    .trackStartIndices = _gM249LowHealthIdleAnimationIndices,
    .poseBanks         = {
        [M249_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM249LowHealthIdleAnimationBank1,
        [M249_ANIMATION_PACKED_ROTATION_BANK]      = _gM249LowHealthIdleAnimationBank4,
    },
};

/// Local translations and 1/4096-turn Euler poses for M249 medium-health idle (encoding 1).
static const AnimationPackedPose _gM249MediumHealthIdleAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/m249_animation_08244_bank1.inc"
};

/// Packed Euler rotations for M249 medium-health idle (encoding 4, eight angle units per step).
static const AnimationPackedRotation _gM249MediumHealthIdleAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m249_animation_08244_bank4.inc"
};

/// Keyframes and loop/stop controls for M249 medium-health idle.
static const AnimationRecord _gM249MediumHealthIdleAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m249_animation_08244_records.inc"
};

/// Nineteen absolute track-start record indices and the serialized trailing zero halfword.
static const u16 _gM249MediumHealthIdleAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m249_animation_08244_indices.inc"
};

/// Long-idle motion above quarter and at or below half maximum HP, M249 native set 24.
///
/// Normal locomotion selects it after 300 direction-free ticks with a
/// five-frame blend. All nineteen rig tracks loop; child playback uses
/// tracks 1..18. Keep the clip loaded while any slot or cached record refers to it.
static AnimationSet _gM249MediumHealthIdleAnimation = {
    .records           = _gM249MediumHealthIdleAnimationRecords,
    .trackStartIndices = _gM249MediumHealthIdleAnimationIndices,
    .poseBanks         = {
        [M249_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM249MediumHealthIdleAnimationBank1,
        [M249_ANIMATION_PACKED_ROTATION_BANK]      = _gM249MediumHealthIdleAnimationBank4,
    },
};

/// Local translations and 1/4096-turn Euler poses for M249 scripted-event entry (encoding 1).
static const AnimationPackedPose _gM249ScriptedPresentationEnterAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/m249_animation_086C4_bank1.inc"
};

/// Packed Euler rotations for M249 scripted-event entry (encoding 4, eight angle units per step).
static const AnimationPackedRotation _gM249ScriptedPresentationEnterAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m249_animation_086C4_bank4.inc"
};

/// Keyframes and final-pose stop controls for M249 scripted-event entry.
static const AnimationRecord _gM249ScriptedPresentationEnterAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m249_animation_086C4_records.inc"
};

/// Nineteen absolute track-start record indices and the serialized trailing zero halfword.
static const u16 _gM249ScriptedPresentationEnterAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m249_animation_086C4_indices.inc"
};

/// Entry motion before a scripted event for the M249 bank, native sets 32 and 34.
///
/// Scripted presentation selects set 32 when its saved s32 variant is zero.
/// CAP weapon events and room presentations wait for this clip before their
/// command. Child tracks 1..18 play without a timed entry blend; all nineteen
/// rig tracks stop at their final poses. Keep the clip loaded through playback
/// and while any slot or cached record refers to it.
static AnimationSet _gM249ScriptedPresentationEnterAnimation = {
    .records           = _gM249ScriptedPresentationEnterAnimationRecords,
    .trackStartIndices = _gM249ScriptedPresentationEnterAnimationIndices,
    .poseBanks         = {
        [M249_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM249ScriptedPresentationEnterAnimationBank1,
        [M249_ANIMATION_PACKED_ROTATION_BANK]      = _gM249ScriptedPresentationEnterAnimationBank4,
    },
};

/// Local translations and 1/4096-turn Euler poses for M249 scripted-event exit (encoding 1).
static const AnimationPackedPose _gM249ScriptedPresentationLeaveAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/m249_animation_0889C_bank1.inc"
};

/// Packed Euler rotations for M249 scripted-event exit (encoding 4, eight angle units per step).
static const AnimationPackedRotation _gM249ScriptedPresentationLeaveAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m249_animation_0889C_bank4.inc"
};

/// Keyframes and final-pose stop controls for M249 scripted-event exit.
static const AnimationRecord _gM249ScriptedPresentationLeaveAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m249_animation_0889C_records.inc"
};

/// Nineteen absolute track-start record indices and the serialized trailing zero halfword.
static const u16 _gM249ScriptedPresentationLeaveAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m249_animation_0889C_indices.inc"
};

/// Exit motion after a scripted event for the M249 bank, native sets 33 and 35.
///
/// Scripted presentation selects set 33 when its saved s32 variant is nonzero.
/// CAP weapon events and room presentations wait for this clip before resuming
/// player control. Child tracks 1..18 play without a timed entry blend; all
/// nineteen rig tracks stop at their final poses. Keep the clip loaded through
/// playback and while any slot or cached record refers to it.
static AnimationSet _gM249ScriptedPresentationLeaveAnimation = {
    .records           = _gM249ScriptedPresentationLeaveAnimationRecords,
    .trackStartIndices = _gM249ScriptedPresentationLeaveAnimationIndices,
    .poseBanks         = {
        [M249_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM249ScriptedPresentationLeaveAnimationBank1,
        [M249_ANIMATION_PACKED_ROTATION_BANK]      = _gM249ScriptedPresentationLeaveAnimationBank4,
    },
};

/// Local translations and 1/4096-turn Euler poses for M249 Parasite Energy family-2 release (encoding 1).
///
/// Read-only clip payloads retain their serialized .data positions.
static const AnimationPackedPose _gM249ParasiteEnergyFamily2ReleaseAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/m249_animation_08E00_bank1.inc"
};

/// Packed Euler rotations for M249 Parasite Energy family-2 release (encoding 4, eight angle units per step).
static const AnimationPackedRotation _gM249ParasiteEnergyFamily2ReleaseAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m249_animation_08E00_bank4.inc"
};

/// Keyframes and final-pose stop controls for M249 Parasite Energy family-2 release.
static const AnimationRecord _gM249ParasiteEnergyFamily2ReleaseAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m249_animation_08E00_records.inc"
};

/// Nineteen absolute track-start record indices and the serialized trailing zero halfword.
static const u16 _gM249ParasiteEnergyFamily2ReleaseAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m249_animation_08E00_indices.inc"
};

/// Parasite Energy release gesture for clip family 2, M249 native-bank set 42.
///
/// Starts the attachment countdown with a six-frame entry blend. Family 2
/// serves spell IDs below 601 whose decimal tens digit is 3. All nineteen
/// rig tracks stop at their final poses; child playback uses tracks 1..18.
/// Keep the clip loaded while any slot or cached record refers to it.
static AnimationSet _gM249ParasiteEnergyFamily2ReleaseAnimation = {
    .records           = _gM249ParasiteEnergyFamily2ReleaseAnimationRecords,
    .trackStartIndices = _gM249ParasiteEnergyFamily2ReleaseAnimationIndices,
    .poseBanks         = {
        [M249_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM249ParasiteEnergyFamily2ReleaseAnimationBank1,
        [M249_ANIMATION_PACKED_ROTATION_BANK]      = _gM249ParasiteEnergyFamily2ReleaseAnimationBank4,
    },
};

/// Local translations and 1/4096-turn Euler poses for M249 Parasite Energy family-2 charge (encoding 1).
static const AnimationPackedPose _gM249ParasiteEnergyFamily2ChargeAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/m249_animation_08FF4_bank1.inc"
};

/// Packed Euler rotations for M249 Parasite Energy family-2 charge (encoding 4, eight angle units per step).
static const AnimationPackedRotation _gM249ParasiteEnergyFamily2ChargeAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m249_animation_08FF4_bank4.inc"
};

/// Charge-pose keyframes, track loops and serialized stop controls for M249 Parasite Energy family 2.
static const AnimationRecord _gM249ParasiteEnergyFamily2ChargeAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m249_animation_08FF4_records.inc"
};

/// Nineteen absolute track-start record indices and the serialized trailing zero halfword.
static const u16 _gM249ParasiteEnergyFamily2ChargeAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m249_animation_08FF4_indices.inc"
};

/// Repeating Parasite Energy charge pose for clip family 2, M249 native-bank set 43.
///
/// Holds while the attachment countdown is nonzero, between
/// `_gM249ParasiteEnergyFamily2ReleaseAnimation` and the family-2 cast clip.
/// Entry resets child tracks 1..18 without a timed blend. All nineteen rig
/// tracks loop; their serialized stop records follow the loop jumps.
/// Keep the clip loaded while any slot or cached record refers to it.
static AnimationSet _gM249ParasiteEnergyFamily2ChargeAnimation = {
    .records           = _gM249ParasiteEnergyFamily2ChargeAnimationRecords,
    .trackStartIndices = _gM249ParasiteEnergyFamily2ChargeAnimationIndices,
    .poseBanks         = {
        [M249_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM249ParasiteEnergyFamily2ChargeAnimationBank1,
        [M249_ANIMATION_PACKED_ROTATION_BANK]      = _gM249ParasiteEnergyFamily2ChargeAnimationBank4,
    },
};

/// Local translations and 1/4096-turn Euler poses for M249 Parasite Energy family-2 cast (encoding 1).
static const AnimationPackedPose _gM249ParasiteEnergyFamily2CastAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/m249_animation_09474_bank1.inc"
};

/// Packed Euler rotations for M249 Parasite Energy family-2 cast (encoding 4, eight angle units per step).
static const AnimationPackedRotation _gM249ParasiteEnergyFamily2CastAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m249_animation_09474_bank4.inc"
};

/// Keyframes and final-pose stop controls for M249 Parasite Energy family-2 cast.
static const AnimationRecord _gM249ParasiteEnergyFamily2CastAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m249_animation_09474_records.inc"
};

/// Nineteen absolute track-start record indices and the serialized trailing zero halfword.
static const u16 _gM249ParasiteEnergyFamily2CastAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m249_animation_09474_indices.inc"
};

/// Parasite Energy cast gesture for clip family 2, M249 native-bank set 44.
///
/// Starts when the attachment countdown expires, after the family-2 charge
/// clip, by resetting child tracks 1..18 without a timed blend. All nineteen
/// rig tracks stop at their final poses. Keep the clip loaded while any slot
/// or cached record refers to it.
static AnimationSet _gM249ParasiteEnergyFamily2CastAnimation = {
    .records           = _gM249ParasiteEnergyFamily2CastAnimationRecords,
    .trackStartIndices = _gM249ParasiteEnergyFamily2CastAnimationIndices,
    .poseBanks         = {
        [M249_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM249ParasiteEnergyFamily2CastAnimationBank1,
        [M249_ANIMATION_PACKED_ROTATION_BANK]      = _gM249ParasiteEnergyFamily2CastAnimationBank4,
    },
};

/// Local translations and 1/4096-turn Euler poses for M249 aiming idle (encoding 1).
static const AnimationPackedPose _gM249AimIdleAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/m249_animation_0964C_bank1.inc"
};

/// Packed Euler rotations for M249 aiming idle (encoding 4, eight angle units per step).
static const AnimationPackedRotation _gM249AimIdleAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m249_animation_0964C_bank4.inc"
};

/// Repeated aiming-pose keyframes, track loops and serialized stop controls for M249 aiming idle.
static const AnimationRecord _gM249AimIdleAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m249_animation_0964C_records.inc"
};

/// Nineteen absolute track-start record indices and the serialized trailing zero halfword.
static const u16 _gM249AimIdleAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m249_animation_0964C_indices.inc"
};

/// Stationary aiming and attack-ready pose for the M249 bank, native set 9.
///
/// Aim locomotion selects it with no movement or turn. Attack preparation
/// blends into it over one frame, or eight when entering from movement or
/// turning. All nineteen tracks loop between identical poses, holding
/// the aiming pose; child playback uses tracks 1..18. Keep the clip loaded
/// while any slot or cached record refers to it.
static AnimationSet _gM249AimIdleAnimation = {
    .records           = _gM249AimIdleAnimationRecords,
    .trackStartIndices = _gM249AimIdleAnimationIndices,
    .poseBanks         = {
        [M249_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM249AimIdleAnimationBank1,
        [M249_ANIMATION_PACKED_ROTATION_BANK]      = _gM249AimIdleAnimationBank4,
    },
};

/// Local translations and 1/4096-turn Euler poses for M249 backward or turning aim (encoding 1).
static const AnimationPackedPose _gM249AimBackwardOrTurnAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/m249_animation_09D80_bank1.inc"
};

/// Packed Euler rotations for M249 backward or turning aim (encoding 4, eight angle units per step).
static const AnimationPackedRotation _gM249AimBackwardOrTurnAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m249_animation_09D80_bank4.inc"
};

/// Keyframes, slot-1 footstep cues and loop/stop controls for M249 backward or turning aim.
static const AnimationRecord _gM249AimBackwardOrTurnAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m249_animation_09D80_records.inc"
};

/// Nineteen absolute track-start record indices and the serialized trailing zero halfword.
static const u16 _gM249AimBackwardOrTurnAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m249_animation_09D80_indices.inc"
};

/// Repeating backward-step or stationary-turn aim motion for the M249 bank, native set 13.
///
/// Aim locomotion selects it for backward input or a stationary turn, including
/// return from Parasite Energy and cancelled reloads. All nineteen rig tracks
/// loop; child playback uses tracks 1..18, with paired footstep cues on track 1.
/// Actor control supplies world movement and turning separately. Keep the
/// clip loaded while any slot or cached record refers to it.
static AnimationSet _gM249AimBackwardOrTurnAnimation = {
    .records           = _gM249AimBackwardOrTurnAnimationRecords,
    .trackStartIndices = _gM249AimBackwardOrTurnAnimationIndices,
    .poseBanks         = {
        [M249_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM249AimBackwardOrTurnAnimationBank1,
        [M249_ANIMATION_PACKED_ROTATION_BANK]      = _gM249AimBackwardOrTurnAnimationBank4,
    },
};

/// Local translations and 1/4096-turn Euler poses for M249 forward aim motion (encoding 1).
static const AnimationPackedPose _gM249AimForwardAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/m249_animation_0A828_bank1.inc"
};

/// Packed Euler rotations for M249 forward aim motion (encoding 4, eight angle units per step).
static const AnimationPackedRotation _gM249AimForwardAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m249_animation_0A828_bank4.inc"
};

/// Keyframes, slot-1 footstep cues and loop/stop controls for M249 forward aim motion.
static const AnimationRecord _gM249AimForwardAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m249_animation_0A828_records.inc"
};

/// Nineteen absolute track-start record indices and the serialized trailing zero halfword.
static const u16 _gM249AimForwardAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m249_animation_0A828_indices.inc"
};

/// Repeating forward aim motion for the M249 bank, native set 12.
///
/// Aim locomotion selects it for forward input, including return from Parasite
/// Energy and cancelled reloads. All nineteen rig tracks loop; child playback
/// uses tracks 1..18, with paired footstep cues on track 1. Actor movement
/// supplies world displacement separately. Keep the clip loaded while any
/// slot or cached record refers to it.
static AnimationSet _gM249AimForwardAnimation = {
    .records           = _gM249AimForwardAnimationRecords,
    .trackStartIndices = _gM249AimForwardAnimationIndices,
    .poseBanks         = {
        [M249_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM249AimForwardAnimationBank1,
        [M249_ANIMATION_PACKED_ROTATION_BANK]      = _gM249AimForwardAnimationBank4,
    },
};

static AnimationPackedPose _gM249Animation0CC20Bank1[63] = {
#include "assets/m249_animation_0CC20_bank1.inc"
};

static AnimationPackedRotation _gM249Animation0CC20Bank4[982] = {
#include "assets/m249_animation_0CC20_bank4.inc"
};

static AnimationRecord _gM249Animation0CC20Records[1111] = {
#include "assets/m249_animation_0CC20_records.inc"
};

static u16 _gM249Animation0CC20Indices[20] = {
#include "assets/m249_animation_0CC20_indices.inc"
};

static AnimationSet _gM249Animation0CC20 = {
    _gM249Animation0CC20Records,
    _gM249Animation0CC20Indices,
    { NULL, _gM249Animation0CC20Bank1, NULL, NULL, _gM249Animation0CC20Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM249Animation0D2BCBank1[11] = {
#include "assets/m249_animation_0D2BC_bank1.inc"
};

static AnimationPackedRotation _gM249Animation0D2BCBank4[160] = {
#include "assets/m249_animation_0D2BC_bank4.inc"
};

static AnimationRecord _gM249Animation0D2BCRecords[210] = {
#include "assets/m249_animation_0D2BC_records.inc"
};

static u16 _gM249Animation0D2BCIndices[20] = {
#include "assets/m249_animation_0D2BC_indices.inc"
};

static AnimationSet _gM249Animation0D2BC = {
    _gM249Animation0D2BCRecords,
    _gM249Animation0D2BCIndices,
    { NULL, _gM249Animation0D2BCBank1, NULL, NULL, _gM249Animation0D2BCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM249Animation0DC30Bank1[17] = {
#include "assets/m249_animation_0DC30_bank1.inc"
};

static AnimationPackedRotation _gM249Animation0DC30Bank4[244] = {
#include "assets/m249_animation_0DC30_bank4.inc"
};

static AnimationRecord _gM249Animation0DC30Records[290] = {
#include "assets/m249_animation_0DC30_records.inc"
};

static u16 _gM249Animation0DC30Indices[20] = {
#include "assets/m249_animation_0DC30_indices.inc"
};

static AnimationSet _gM249Animation0DC30 = {
    _gM249Animation0DC30Records,
    _gM249Animation0DC30Indices,
    { NULL, _gM249Animation0DC30Bank1, NULL, NULL, _gM249Animation0DC30Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM249Animation0E344Bank1[13] = {
#include "assets/m249_animation_0E344_bank1.inc"
};

static AnimationPackedRotation _gM249Animation0E344Bank4[177] = {
#include "assets/m249_animation_0E344_bank4.inc"
};

static AnimationRecord _gM249Animation0E344Records[217] = {
#include "assets/m249_animation_0E344_records.inc"
};

static u16 _gM249Animation0E344Indices[20] = {
#include "assets/m249_animation_0E344_indices.inc"
};

static AnimationSet _gM249Animation0E344 = {
    _gM249Animation0E344Records,
    _gM249Animation0E344Indices,
    { NULL, _gM249Animation0E344Bank1, NULL, NULL, _gM249Animation0E344Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM249Animation0EBFCBank1[18] = {
#include "assets/m249_animation_0EBFC_bank1.inc"
};

static AnimationPackedRotation _gM249Animation0EBFCBank4[201] = {
#include "assets/m249_animation_0EBFC_bank4.inc"
};

static AnimationRecord _gM249Animation0EBFCRecords[283] = {
#include "assets/m249_animation_0EBFC_records.inc"
};

static u16 _gM249Animation0EBFCIndices[20] = {
#include "assets/m249_animation_0EBFC_indices.inc"
};

static AnimationSet _gM249Animation0EBFC = {
    _gM249Animation0EBFCRecords,
    _gM249Animation0EBFCIndices,
    { NULL, _gM249Animation0EBFCBank1, NULL, NULL, _gM249Animation0EBFCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM249Animation0F39CBank1[16] = {
#include "assets/m249_animation_0F39C_bank1.inc"
};

static AnimationPackedRotation _gM249Animation0F39CBank4[157] = {
#include "assets/m249_animation_0F39C_bank4.inc"
};

static AnimationRecord _gM249Animation0F39CRecords[263] = {
#include "assets/m249_animation_0F39C_records.inc"
};

static u16 _gM249Animation0F39CIndices[20] = {
#include "assets/m249_animation_0F39C_indices.inc"
};

static AnimationSet _gM249Animation0F39C = {
    _gM249Animation0F39CRecords,
    _gM249Animation0F39CIndices,
    { NULL, _gM249Animation0F39CBank1, NULL, NULL, _gM249Animation0F39CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM249Animation0FD70Bank1[25] = {
#include "assets/m249_animation_0FD70_bank1.inc"
};

static AnimationPackedRotation _gM249Animation0FD70Bank4[223] = {
#include "assets/m249_animation_0FD70_bank4.inc"
};

static AnimationRecord _gM249Animation0FD70Records[311] = {
#include "assets/m249_animation_0FD70_records.inc"
};

static u16 _gM249Animation0FD70Indices[20] = {
#include "assets/m249_animation_0FD70_indices.inc"
};

static AnimationSet _gM249Animation0FD70 = {
    _gM249Animation0FD70Records,
    _gM249Animation0FD70Indices,
    { NULL, _gM249Animation0FD70Bank1, NULL, NULL, _gM249Animation0FD70Bank4, NULL, NULL, NULL },
};

AnimationBank D_m249_8012CF58 = { { {
    NULL,
    &_gM249IdleAnimation,
    &_gM249Animation0EBFC,
    &_gM249Animation0F39C,
    &_gM249Animation0FD70,
    &_gM249TurnLeftAnimation,
    &_gM249TurnRightAnimation,
    &_gM249Animation0DC30,
    &_gM249Animation0E344,
    &_gM249AimIdleAnimation,
    &_gM249Animation0D2BC,
    &_gM249Animation0D2BC,
    &_gM249AimForwardAnimation,
    &_gM249AimBackwardOrTurnAnimation,
    &_gM249Animation0CC20,
    &_gM249Animation0CC20,
    &_gM249Part4HitReactionAnimation,
    &_gM249OtherBodyHitReactionAnimation,
    &_gM249DeathAnimation,
    &_gM249UnarmedWalkAnimation,
    &_gM249Animation0CC20,
    &_gM249IdleAnimation,
    &_gM249IdleAnimation,
    &_gM249HighHealthIdleAnimation,
    &_gM249MediumHealthIdleAnimation,
    &_gM249LowHealthIdleAnimation,
    &_gM249ParasiteEnergyFamily0ReleaseAnimation,
    &_gM249ParasiteEnergyFamily0ChargeAnimation,
    &_gM249ParasiteEnergyFamily0CastAnimation,
    &_gM249ParasiteEnergyFamily1ReleaseAnimation,
    &_gM249ParasiteEnergyFamily1ChargeAnimation,
    &_gM249ParasiteEnergyFamily1CastAnimation,
    &_gM249ScriptedPresentationEnterAnimation,
    &_gM249ScriptedPresentationLeaveAnimation,
    &_gM249ScriptedPresentationEnterAnimation,
    &_gM249ScriptedPresentationLeaveAnimation,
    &_gM249StairAscentAnimation,
    &_gM249StairDescentAnimation,
    &_gM249StairAscentFinishEvenAnimation,
    &_gM249StairAscentFinishOddAnimation,
    &_gM249ItemUseAnimation,
    &_gM249IdleAnimation,
    &_gM249ParasiteEnergyFamily2ReleaseAnimation,
    &_gM249ParasiteEnergyFamily2ChargeAnimation,
    &_gM249ParasiteEnergyFamily2CastAnimation,
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
