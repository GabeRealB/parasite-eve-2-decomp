#include <psyq/sys/types.h>

#include "types.h"

#include "gameplay/animation.h"

/// Encoding indices for these player clips' translation/rotation and rotation-only banks.
enum {
    UNUSED_86_ANIMATION_TRANSLATION_ROTATION_BANK = 1,
    UNUSED_86_ANIMATION_PACKED_ROTATION_BANK      = 4,
};

/// Idle translation/rotation keyframes for player tracks 0 and 1.
///
/// Translation uses model units; Euler angles use 4096 units per turn.
/// Read-only clip payloads retain their serialized placement in .data.
static const AnimationPackedPose _gUnused86PlayerIdleAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/unused_86_animation_00194_bank1.inc"
};

/// Idle packed Euler keyframes for player tracks 2..18, in eight-angle-unit steps.
static const AnimationPackedRotation _gUnused86PlayerIdleAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/unused_86_animation_00194_bank4.inc"
};

/// Idle keyframes and controls; durations count normal-rate frames and pose offsets count four-byte words.
static const AnimationRecord _gUnused86PlayerIdleAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/unused_86_animation_00194_records.inc"
};

/// Absolute idle record starts for tracks 0..18, followed by the retained serialized halfword.
static const u16 _gUnused86PlayerIdleAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/unused_86_animation_00194_indices.inc"
};

/// Standing idle for the player using the unused weapon animation bank.
///
/// Native clip 1; entries 21, 22 and 41 also select this pose. Contains 19 part
/// tracks: translation/rotation for tracks 0 and 1, packed rotation for 2..18.
/// The loaded weapon package must remain present while any playback slot
/// refers to this set or its arrays.
static AnimationSet _gUnused86PlayerIdleAnimation = {
    .records           = _gUnused86PlayerIdleAnimationRecords,
    .trackStartIndices = _gUnused86PlayerIdleAnimationIndices,
    .poseBanks         = {
        [UNUSED_86_ANIMATION_TRANSLATION_ROTATION_BANK] = _gUnused86PlayerIdleAnimationBank1,
        [UNUSED_86_ANIMATION_PACKED_ROTATION_BANK]      = _gUnused86PlayerIdleAnimationBank4,
    },
};

/// Unarmed-walk translation/rotation keyframes for tracks 0 and 1, in model units and 1/4096 turns.
static const AnimationPackedPose _gUnused86PlayerUnarmedWalkAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/unused_86_animation_00838_bank1.inc"
};

/// Unarmed-walk packed Euler keyframes for player tracks 2..18, in eight-angle-unit steps.
static const AnimationPackedRotation _gUnused86PlayerUnarmedWalkAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/unused_86_animation_00838_bank4.inc"
};

/// Unarmed-walk keyframes and controls; durations count normal-rate frames and pose offsets count four-byte words.
static const AnimationRecord _gUnused86PlayerUnarmedWalkAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/unused_86_animation_00838_records.inc"
};

/// Absolute unarmed-walk record starts for tracks 0..18, followed by the retained serialized halfword.
static const u16 _gUnused86PlayerUnarmedWalkAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/unused_86_animation_00838_indices.inc"
};

/// Forward walk without an attached weapon model, in the unused weapon animation bank.
///
/// Native clip 19. Contains 19 part tracks: translation/rotation for tracks
/// 0 and 1, packed rotation for 2..18. The loaded weapon package must remain
/// present while any playback slot refers to this set or its arrays.
static AnimationSet _gUnused86PlayerUnarmedWalkAnimation = {
    .records           = _gUnused86PlayerUnarmedWalkAnimationRecords,
    .trackStartIndices = _gUnused86PlayerUnarmedWalkAnimationIndices,
    .poseBanks         = {
        [UNUSED_86_ANIMATION_TRANSLATION_ROTATION_BANK] = _gUnused86PlayerUnarmedWalkAnimationBank1,
        [UNUSED_86_ANIMATION_PACKED_ROTATION_BANK]      = _gUnused86PlayerUnarmedWalkAnimationBank4,
    },
};

/// Walk translation/rotation keyframes for tracks 0 and 1, in model units and 1/4096 turns.
static const AnimationPackedPose _gUnused86PlayerWalkAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/unused_86_animation_01184_bank1.inc"
};

/// Walk packed Euler keyframes for player tracks 2..18, in eight-angle-unit steps.
static const AnimationPackedRotation _gUnused86PlayerWalkAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/unused_86_animation_01184_bank4.inc"
};

/// Walk keyframes and controls; durations count normal-rate frames and pose offsets count four-byte words.
static const AnimationRecord _gUnused86PlayerWalkAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/unused_86_animation_01184_records.inc"
};

/// Absolute walk record starts for tracks 0..18, followed by the retained serialized halfword.
static const u16 _gUnused86PlayerWalkAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/unused_86_animation_01184_indices.inc"
};

/// Forward walk with an attached weapon model, in the unused weapon animation bank.
///
/// Native clip 2. Contains 19 part tracks: translation/rotation for tracks
/// 0 and 1, packed rotation for 2..18. The loaded weapon package must remain
/// present while any playback slot refers to this set or its arrays.
static AnimationSet _gUnused86PlayerWalkAnimation = {
    .records           = _gUnused86PlayerWalkAnimationRecords,
    .trackStartIndices = _gUnused86PlayerWalkAnimationIndices,
    .poseBanks         = {
        [UNUSED_86_ANIMATION_TRANSLATION_ROTATION_BANK] = _gUnused86PlayerWalkAnimationBank1,
        [UNUSED_86_ANIMATION_PACKED_ROTATION_BANK]      = _gUnused86PlayerWalkAnimationBank4,
    },
};

/// Backward-walk translation/rotation keyframes for tracks 0 and 1, in model units and 1/4096 turns.
static const AnimationPackedPose _gUnused86PlayerBackwardWalkAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/unused_86_animation_01A5C_bank1.inc"
};

/// Backward-walk packed Euler keyframes for player tracks 2..18, in eight-angle-unit steps.
static const AnimationPackedRotation _gUnused86PlayerBackwardWalkAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/unused_86_animation_01A5C_bank4.inc"
};

/// Backward-walk keyframes and controls; durations count normal-rate frames and pose offsets count four-byte words.
static const AnimationRecord _gUnused86PlayerBackwardWalkAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/unused_86_animation_01A5C_records.inc"
};

/// Absolute backward-walk record starts for tracks 0..18, followed by the retained serialized halfword.
static const u16 _gUnused86PlayerBackwardWalkAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/unused_86_animation_01A5C_indices.inc"
};

/// Backward walk for the player using the unused weapon animation bank.
///
/// Native clip 3. Contains 19 part tracks: translation/rotation for tracks
/// 0 and 1, packed rotation for 2..18. The loaded weapon package must remain
/// present while any playback slot refers to this set or its arrays.
static AnimationSet _gUnused86PlayerBackwardWalkAnimation = {
    .records           = _gUnused86PlayerBackwardWalkAnimationRecords,
    .trackStartIndices = _gUnused86PlayerBackwardWalkAnimationIndices,
    .poseBanks         = {
        [UNUSED_86_ANIMATION_TRANSLATION_ROTATION_BANK] = _gUnused86PlayerBackwardWalkAnimationBank1,
        [UNUSED_86_ANIMATION_PACKED_ROTATION_BANK]      = _gUnused86PlayerBackwardWalkAnimationBank4,
    },
};

/// Run translation/rotation keyframes for tracks 0 and 1, in model units and 1/4096 turns.
static const AnimationPackedPose _gUnused86PlayerRunAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/unused_86_animation_02190_bank1.inc"
};

/// Run packed Euler keyframes for player tracks 2..18, in eight-angle-unit steps.
static const AnimationPackedRotation _gUnused86PlayerRunAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/unused_86_animation_02190_bank4.inc"
};

/// Run keyframes and controls; durations count normal-rate frames and pose offsets count four-byte words.
static const AnimationRecord _gUnused86PlayerRunAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/unused_86_animation_02190_records.inc"
};

/// Absolute run record starts for tracks 0..18, followed by the retained serialized halfword.
static const u16 _gUnused86PlayerRunAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/unused_86_animation_02190_indices.inc"
};

/// Forward run for the player using the unused weapon animation bank.
///
/// Native clip 4. Contains 19 part tracks: translation/rotation for tracks
/// 0 and 1, packed rotation for 2..18. The loaded weapon package must remain
/// present while any playback slot refers to this set or its arrays.
static AnimationSet _gUnused86PlayerRunAnimation = {
    .records           = _gUnused86PlayerRunAnimationRecords,
    .trackStartIndices = _gUnused86PlayerRunAnimationIndices,
    .poseBanks         = {
        [UNUSED_86_ANIMATION_TRANSLATION_ROTATION_BANK] = _gUnused86PlayerRunAnimationBank1,
        [UNUSED_86_ANIMATION_PACKED_ROTATION_BANK]      = _gUnused86PlayerRunAnimationBank4,
    },
};

/// Left-turn translation/rotation keyframes for tracks 0 and 1, in model units and 1/4096 turns.
static const AnimationPackedPose _gUnused86PlayerTurnLeftAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/unused_86_animation_029F0_bank1.inc"
};

/// Left-turn packed Euler keyframes for player tracks 2..18, in eight-angle-unit steps.
static const AnimationPackedRotation _gUnused86PlayerTurnLeftAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/unused_86_animation_029F0_bank4.inc"
};

/// Left-turn keyframes and controls; durations count normal-rate frames and pose offsets count four-byte words.
static const AnimationRecord _gUnused86PlayerTurnLeftAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/unused_86_animation_029F0_records.inc"
};

/// Absolute left-turn record starts for tracks 0..18, followed by the retained serialized halfword.
static const u16 _gUnused86PlayerTurnLeftAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/unused_86_animation_029F0_indices.inc"
};

/// Stationary left turn for the player using the unused weapon animation bank.
///
/// Native clip 5, also selected for scripted turns toward a negative yaw delta.
/// Contains 19 part tracks: translation/rotation for tracks 0 and 1, packed
/// rotation for 2..18. The loaded weapon package must remain present while
/// any playback slot refers to this set or its arrays.
static AnimationSet _gUnused86PlayerTurnLeftAnimation = {
    .records           = _gUnused86PlayerTurnLeftAnimationRecords,
    .trackStartIndices = _gUnused86PlayerTurnLeftAnimationIndices,
    .poseBanks         = {
        [UNUSED_86_ANIMATION_TRANSLATION_ROTATION_BANK] = _gUnused86PlayerTurnLeftAnimationBank1,
        [UNUSED_86_ANIMATION_PACKED_ROTATION_BANK]      = _gUnused86PlayerTurnLeftAnimationBank4,
    },
};

/// Right-turn translation/rotation keyframes for tracks 0 and 1, in model units and 1/4096 turns.
///
/// Read-only clip payloads retain their serialized placement in .data.
static const AnimationPackedPose _gUnused86PlayerTurnRightAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/unused_86_animation_03254_bank1.inc"
};

/// Right-turn packed Euler keyframes for tracks 2..18, in eight-angle-unit steps.
static const AnimationPackedRotation _gUnused86PlayerTurnRightAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/unused_86_animation_03254_bank4.inc"
};

/// Right-turn keyframes and loop/stop controls; durations count normal-rate frames and pose offsets count four-byte words.
static const AnimationRecord _gUnused86PlayerTurnRightAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/unused_86_animation_03254_records.inc"
};

/// Absolute right-turn record starts for tracks 0..18, followed by a retained serialized zero halfword.
static const u16 _gUnused86PlayerTurnRightAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/unused_86_animation_03254_indices.inc"
};

/// Stationary right turn for the player using the unused weapon animation bank.
///
/// Native clip 6, also selected for scripted turns toward a nonnegative yaw
/// delta. Contains 19 part tracks: translation/rotation for tracks 0 and 1,
/// packed rotation for 2..18. The clip loops after its opening poses. The loaded
/// weapon package must remain present while playback refers to this set or its arrays.
static AnimationSet _gUnused86PlayerTurnRightAnimation = {
    .records           = _gUnused86PlayerTurnRightAnimationRecords,
    .trackStartIndices = _gUnused86PlayerTurnRightAnimationIndices,
    .poseBanks         = {
        [UNUSED_86_ANIMATION_TRANSLATION_ROTATION_BANK] = _gUnused86PlayerTurnRightAnimationBank1,
        [UNUSED_86_ANIMATION_PACKED_ROTATION_BANK]      = _gUnused86PlayerTurnRightAnimationBank4,
    },
};

/// Item-use translation/rotation keyframes for tracks 0 and 1, in model units and 1/4096 turns.
static const AnimationPackedPose _gUnused86PlayerItemUseAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/unused_86_animation_03568_bank1.inc"
};

/// Item-use packed Euler keyframes for tracks 2..18, in eight-angle-unit steps.
static const AnimationPackedRotation _gUnused86PlayerItemUseAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/unused_86_animation_03568_bank4.inc"
};

/// Item-use keyframes and stop controls; durations count normal-rate frames and pose offsets count four-byte words.
static const AnimationRecord _gUnused86PlayerItemUseAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/unused_86_animation_03568_records.inc"
};

/// Absolute item-use record starts for tracks 0..18, followed by a retained serialized zero halfword.
static const u16 _gUnused86PlayerItemUseAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/unused_86_animation_03568_indices.inc"
};

/// Item-use presentation for the player using the unused weapon animation bank.
///
/// Native clip 40. Plays once before returning to aim locomotion; the animation
/// does not apply the item. Contains 19 part tracks: translation/rotation for
/// tracks 0 and 1, packed rotation for 2..18. The loaded weapon package must
/// remain present while playback refers to this set or its arrays.
static AnimationSet _gUnused86PlayerItemUseAnimation = {
    .records           = _gUnused86PlayerItemUseAnimationRecords,
    .trackStartIndices = _gUnused86PlayerItemUseAnimationIndices,
    .poseBanks         = {
        [UNUSED_86_ANIMATION_TRANSLATION_ROTATION_BANK] = _gUnused86PlayerItemUseAnimationBank1,
        [UNUSED_86_ANIMATION_PACKED_ROTATION_BANK]      = _gUnused86PlayerItemUseAnimationBank4,
    },
};

/// Stair-ascent translation/rotation keyframes for tracks 0 and 1, in model units and 1/4096 turns.
static const AnimationPackedPose _gUnused86PlayerStairAscentAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/unused_86_animation_03CC4_bank1.inc"
};

/// Stair-ascent packed Euler keyframes for tracks 2..18, in eight-angle-unit steps.
static const AnimationPackedRotation _gUnused86PlayerStairAscentAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/unused_86_animation_03CC4_bank4.inc"
};

/// Stair-ascent keyframes, footstep cues and loop/stop controls.
///
/// Durations count normal-rate frames; pose offsets count four-byte words.
static const AnimationRecord _gUnused86PlayerStairAscentAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/unused_86_animation_03CC4_records.inc"
};

/// Absolute stair-ascent record starts for tracks 0..18, followed by a retained serialized zero halfword.
static const u16 _gUnused86PlayerStairAscentAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/unused_86_animation_03CC4_indices.inc"
};

/// Looping stair ascent for the player using the unused weapon animation bank.
///
/// Native clip 36. Track 1's footstep cues gate the scripted stair strides;
/// finishing the flight selects a separate clip by the original step-count parity.
/// Contains 19 part tracks: translation/rotation for tracks 0 and 1, packed
/// rotation for 2..18. The loaded weapon package must remain present while
/// playback refers to this set or its arrays.
static AnimationSet _gUnused86PlayerStairAscentAnimation = {
    .records           = _gUnused86PlayerStairAscentAnimationRecords,
    .trackStartIndices = _gUnused86PlayerStairAscentAnimationIndices,
    .poseBanks         = {
        [UNUSED_86_ANIMATION_TRANSLATION_ROTATION_BANK] = _gUnused86PlayerStairAscentAnimationBank1,
        [UNUSED_86_ANIMATION_PACKED_ROTATION_BANK]      = _gUnused86PlayerStairAscentAnimationBank4,
    },
};

/// Stair-descent translation/rotation keyframes for tracks 0 and 1, in model units and 1/4096 turns.
static const AnimationPackedPose _gUnused86PlayerStairDescentAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/unused_86_animation_0444C_bank1.inc"
};

/// Stair-descent packed Euler keyframes for tracks 2..18, in eight-angle-unit steps.
static const AnimationPackedRotation _gUnused86PlayerStairDescentAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/unused_86_animation_0444C_bank4.inc"
};

/// Stair-descent keyframes, footstep cues and loop/stop controls.
///
/// Durations count normal-rate frames; pose offsets count four-byte words.
static const AnimationRecord _gUnused86PlayerStairDescentAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/unused_86_animation_0444C_records.inc"
};

/// Absolute stair-descent record starts for tracks 0..18, followed by a retained serialized zero halfword.
static const u16 _gUnused86PlayerStairDescentAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/unused_86_animation_0444C_indices.inc"
};

/// Looping stair descent for the player using the unused weapon animation bank.
///
/// Native clip 37. Track 1's footstep cues gate the scripted stair strides;
/// completing the final stride returns to idle. Contains 19 part tracks:
/// translation/rotation for tracks 0 and 1, packed rotation for 2..18. The loaded
/// weapon package must remain present while playback refers to this set or its arrays.
static AnimationSet _gUnused86PlayerStairDescentAnimation = {
    .records           = _gUnused86PlayerStairDescentAnimationRecords,
    .trackStartIndices = _gUnused86PlayerStairDescentAnimationIndices,
    .poseBanks         = {
        [UNUSED_86_ANIMATION_TRANSLATION_ROTATION_BANK] = _gUnused86PlayerStairDescentAnimationBank1,
        [UNUSED_86_ANIMATION_PACKED_ROTATION_BANK]      = _gUnused86PlayerStairDescentAnimationBank4,
    },
};

/// Translation/rotation keyframes for finishing an ascent with an odd step count.
///
/// Tracks 0 and 1, in model units and 1/4096 turns.
static const AnimationPackedPose _gUnused86PlayerStairAscentFinishOddAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/unused_86_animation_04720_bank1.inc"
};

/// Packed Euler keyframes for finishing an ascent with an odd step count.
///
/// Tracks 2..18, in eight-angle-unit steps.
static const AnimationPackedRotation _gUnused86PlayerStairAscentFinishOddAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/unused_86_animation_04720_bank4.inc"
};

/// Keyframes, footstep cues and stop controls for finishing an ascent with an odd step count.
///
/// Durations count normal-rate frames; pose offsets count four-byte words.
static const AnimationRecord _gUnused86PlayerStairAscentFinishOddAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/unused_86_animation_04720_records.inc"
};

/// Absolute record starts for finishing an ascent with an odd step count.
///
/// Tracks 0..18, followed by a retained serialized zero halfword.
static const u16 _gUnused86PlayerStairAscentFinishOddAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/unused_86_animation_04720_indices.inc"
};

/// Finishing stride after a player stair ascent with an odd original step count.
///
/// Native clip 39 in the unused weapon animation bank. Plays once while the
/// player moves onto level ground, then returns to idle. Contains 19 part tracks:
/// translation/rotation for tracks 0 and 1, packed rotation for 2..18. The loaded
/// weapon package must remain present while playback refers to this set or its arrays.
static AnimationSet _gUnused86PlayerStairAscentFinishOddAnimation = {
    .records           = _gUnused86PlayerStairAscentFinishOddAnimationRecords,
    .trackStartIndices = _gUnused86PlayerStairAscentFinishOddAnimationIndices,
    .poseBanks         = {
        [UNUSED_86_ANIMATION_TRANSLATION_ROTATION_BANK] = _gUnused86PlayerStairAscentFinishOddAnimationBank1,
        [UNUSED_86_ANIMATION_PACKED_ROTATION_BANK]      = _gUnused86PlayerStairAscentFinishOddAnimationBank4,
    },
};

/// Translation/rotation keyframes for finishing an ascent with an even step count.
///
/// Tracks 0 and 1, in model units and 1/4096 turns.
static const AnimationPackedPose _gUnused86PlayerStairAscentFinishEvenAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/unused_86_animation_04AB0_bank1.inc"
};

/// Packed Euler keyframes for finishing an ascent with an even step count.
///
/// Tracks 2..18, in eight-angle-unit steps.
static const AnimationPackedRotation _gUnused86PlayerStairAscentFinishEvenAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/unused_86_animation_04AB0_bank4.inc"
};

/// Keyframes, footstep cues and stop controls for finishing an ascent with an even step count.
///
/// Durations count normal-rate frames; pose offsets count four-byte words.
static const AnimationRecord _gUnused86PlayerStairAscentFinishEvenAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/unused_86_animation_04AB0_records.inc"
};

/// Absolute record starts for finishing an ascent with an even step count.
///
/// Tracks 0..18, followed by a retained serialized zero halfword.
static const u16 _gUnused86PlayerStairAscentFinishEvenAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/unused_86_animation_04AB0_indices.inc"
};

/// Finishing stride after a player stair ascent with an even original step count.
///
/// Native clip 38 in the unused weapon animation bank. Plays once while the
/// player moves onto level ground, then returns to idle. Contains 19 part tracks:
/// translation/rotation for tracks 0 and 1, packed rotation for 2..18. The loaded
/// weapon package must remain present while playback refers to this set or its arrays.
static AnimationSet _gUnused86PlayerStairAscentFinishEvenAnimation = {
    .records           = _gUnused86PlayerStairAscentFinishEvenAnimationRecords,
    .trackStartIndices = _gUnused86PlayerStairAscentFinishEvenAnimationIndices,
    .poseBanks         = {
        [UNUSED_86_ANIMATION_TRANSLATION_ROTATION_BANK] = _gUnused86PlayerStairAscentFinishEvenAnimationBank1,
        [UNUSED_86_ANIMATION_PACKED_ROTATION_BANK]      = _gUnused86PlayerStairAscentFinishEvenAnimationBank4,
    },
};

static AnimationPackedPose _gUnused86Animation04F38Bank1[9] = {
#include "assets/unused_86_animation_04F38_bank1.inc"
};

static AnimationPackedRotation _gUnused86Animation04F38Bank4[104] = {
#include "assets/unused_86_animation_04F38_bank4.inc"
};

static AnimationRecord _gUnused86Animation04F38Records[139] = {
#include "assets/unused_86_animation_04F38_records.inc"
};

static u16 _gUnused86Animation04F38Indices[20] = {
#include "assets/unused_86_animation_04F38_indices.inc"
};

static AnimationSet _gUnused86Animation04F38 = {
    _gUnused86Animation04F38Records,
    _gUnused86Animation04F38Indices,
    { NULL, _gUnused86Animation04F38Bank1, NULL, NULL, _gUnused86Animation04F38Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gUnused86Animation05134Bank1[3] = {
#include "assets/unused_86_animation_05134_bank1.inc"
};

static AnimationPackedRotation _gUnused86Animation05134Bank4[22] = {
#include "assets/unused_86_animation_05134_bank4.inc"
};

static AnimationRecord _gUnused86Animation05134Records[76] = {
#include "assets/unused_86_animation_05134_records.inc"
};

static u16 _gUnused86Animation05134Indices[20] = {
#include "assets/unused_86_animation_05134_indices.inc"
};

static AnimationSet _gUnused86Animation05134 = {
    _gUnused86Animation05134Records,
    _gUnused86Animation05134Indices,
    { NULL, _gUnused86Animation05134Bank1, NULL, NULL, _gUnused86Animation05134Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gUnused86Animation0540CBank1[6] = {
#include "assets/unused_86_animation_0540C_bank1.inc"
};

static AnimationPackedRotation _gUnused86Animation0540CBank4[57] = {
#include "assets/unused_86_animation_0540C_bank4.inc"
};

static AnimationRecord _gUnused86Animation0540CRecords[87] = {
#include "assets/unused_86_animation_0540C_records.inc"
};

static u16 _gUnused86Animation0540CIndices[20] = {
#include "assets/unused_86_animation_0540C_indices.inc"
};

static AnimationSet _gUnused86Animation0540C = {
    _gUnused86Animation0540CRecords,
    _gUnused86Animation0540CIndices,
    { NULL, _gUnused86Animation0540CBank1, NULL, NULL, _gUnused86Animation0540CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gUnused86Animation056B0Bank1[4] = {
#include "assets/unused_86_animation_056B0_bank1.inc"
};

static AnimationPackedRotation _gUnused86Animation056B0Bank4[55] = {
#include "assets/unused_86_animation_056B0_bank4.inc"
};

static AnimationRecord _gUnused86Animation056B0Records[82] = {
#include "assets/unused_86_animation_056B0_records.inc"
};

static u16 _gUnused86Animation056B0Indices[20] = {
#include "assets/unused_86_animation_056B0_indices.inc"
};

static AnimationSet _gUnused86Animation056B0 = {
    _gUnused86Animation056B0Records,
    _gUnused86Animation056B0Indices,
    { NULL, _gUnused86Animation056B0Bank1, NULL, NULL, _gUnused86Animation056B0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gUnused86Animation058B0Bank1[3] = {
#include "assets/unused_86_animation_058B0_bank1.inc"
};

static AnimationPackedRotation _gUnused86Animation058B0Bank4[23] = {
#include "assets/unused_86_animation_058B0_bank4.inc"
};

static AnimationRecord _gUnused86Animation058B0Records[76] = {
#include "assets/unused_86_animation_058B0_records.inc"
};

static u16 _gUnused86Animation058B0Indices[20] = {
#include "assets/unused_86_animation_058B0_indices.inc"
};

static AnimationSet _gUnused86Animation058B0 = {
    _gUnused86Animation058B0Records,
    _gUnused86Animation058B0Indices,
    { NULL, _gUnused86Animation058B0Bank1, NULL, NULL, _gUnused86Animation058B0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gUnused86Animation05C04Bank1[8] = {
#include "assets/unused_86_animation_05C04_bank1.inc"
};

static AnimationPackedRotation _gUnused86Animation05C04Bank4[68] = {
#include "assets/unused_86_animation_05C04_bank4.inc"
};

static AnimationRecord _gUnused86Animation05C04Records[101] = {
#include "assets/unused_86_animation_05C04_records.inc"
};

static u16 _gUnused86Animation05C04Indices[20] = {
#include "assets/unused_86_animation_05C04_indices.inc"
};

static AnimationSet _gUnused86Animation05C04 = {
    _gUnused86Animation05C04Records,
    _gUnused86Animation05C04Indices,
    { NULL, _gUnused86Animation05C04Bank1, NULL, NULL, _gUnused86Animation05C04Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gUnused86Animation05EB8Bank1[5] = {
#include "assets/unused_86_animation_05EB8_bank1.inc"
};

static AnimationPackedRotation _gUnused86Animation05EB8Bank4[55] = {
#include "assets/unused_86_animation_05EB8_bank4.inc"
};

static AnimationRecord _gUnused86Animation05EB8Records[83] = {
#include "assets/unused_86_animation_05EB8_records.inc"
};

static u16 _gUnused86Animation05EB8Indices[20] = {
#include "assets/unused_86_animation_05EB8_indices.inc"
};

static AnimationSet _gUnused86Animation05EB8 = {
    _gUnused86Animation05EB8Records,
    _gUnused86Animation05EB8Indices,
    { NULL, _gUnused86Animation05EB8Bank1, NULL, NULL, _gUnused86Animation05EB8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gUnused86Animation061D8Bank1[6] = {
#include "assets/unused_86_animation_061D8_bank1.inc"
};

static AnimationPackedRotation _gUnused86Animation061D8Bank4[66] = {
#include "assets/unused_86_animation_061D8_bank4.inc"
};

static AnimationRecord _gUnused86Animation061D8Records[96] = {
#include "assets/unused_86_animation_061D8_records.inc"
};

static u16 _gUnused86Animation061D8Indices[20] = {
#include "assets/unused_86_animation_061D8_indices.inc"
};

static AnimationSet _gUnused86Animation061D8 = {
    _gUnused86Animation061D8Records,
    _gUnused86Animation061D8Indices,
    { NULL, _gUnused86Animation061D8Bank1, NULL, NULL, _gUnused86Animation061D8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gUnused86Animation0699CBank1[18] = {
#include "assets/unused_86_animation_0699C_bank1.inc"
};

static AnimationPackedRotation _gUnused86Animation0699CBank4[184] = {
#include "assets/unused_86_animation_0699C_bank4.inc"
};

static AnimationRecord _gUnused86Animation0699CRecords[239] = {
#include "assets/unused_86_animation_0699C_records.inc"
};

static u16 _gUnused86Animation0699CIndices[20] = {
#include "assets/unused_86_animation_0699C_indices.inc"
};

static AnimationSet _gUnused86Animation0699C = {
    _gUnused86Animation0699CRecords,
    _gUnused86Animation0699CIndices,
    { NULL, _gUnused86Animation0699CBank1, NULL, NULL, _gUnused86Animation0699CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gUnused86Animation07C34Bank1[29] = {
#include "assets/unused_86_animation_07C34_bank1.inc"
};

static AnimationPackedRotation _gUnused86Animation07C34Bank4[450] = {
#include "assets/unused_86_animation_07C34_bank4.inc"
};

static AnimationRecord _gUnused86Animation07C34Records[633] = {
#include "assets/unused_86_animation_07C34_records.inc"
};

static u16 _gUnused86Animation07C34Indices[20] = {
#include "assets/unused_86_animation_07C34_indices.inc"
};

static AnimationSet _gUnused86Animation07C34 = {
    _gUnused86Animation07C34Records,
    _gUnused86Animation07C34Indices,
    { NULL, _gUnused86Animation07C34Bank1, NULL, NULL, _gUnused86Animation07C34Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gUnused86Animation087ACBank1[12] = {
#include "assets/unused_86_animation_087AC_bank1.inc"
};

static AnimationPackedRotation _gUnused86Animation087ACBank4[266] = {
#include "assets/unused_86_animation_087AC_bank4.inc"
};

static AnimationRecord _gUnused86Animation087ACRecords[412] = {
#include "assets/unused_86_animation_087AC_records.inc"
};

static u16 _gUnused86Animation087ACIndices[20] = {
#include "assets/unused_86_animation_087AC_indices.inc"
};

static AnimationSet _gUnused86Animation087AC = {
    _gUnused86Animation087ACRecords,
    _gUnused86Animation087ACIndices,
    { NULL, _gUnused86Animation087ACBank1, NULL, NULL, _gUnused86Animation087ACBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gUnused86Animation08EC8Bank1[9] = {
#include "assets/unused_86_animation_08EC8_bank1.inc"
};

static AnimationPackedRotation _gUnused86Animation08EC8Bank4[144] = {
#include "assets/unused_86_animation_08EC8_bank4.inc"
};

static AnimationRecord _gUnused86Animation08EC8Records[264] = {
#include "assets/unused_86_animation_08EC8_records.inc"
};

static u16 _gUnused86Animation08EC8Indices[20] = {
#include "assets/unused_86_animation_08EC8_indices.inc"
};

static AnimationSet _gUnused86Animation08EC8 = {
    _gUnused86Animation08EC8Records,
    _gUnused86Animation08EC8Indices,
    { NULL, _gUnused86Animation08EC8Bank1, NULL, NULL, _gUnused86Animation08EC8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gUnused86Animation09348Bank1[6] = {
#include "assets/unused_86_animation_09348_bank1.inc"
};

static AnimationPackedRotation _gUnused86Animation09348Bank4[107] = {
#include "assets/unused_86_animation_09348_bank4.inc"
};

static AnimationRecord _gUnused86Animation09348Records[143] = {
#include "assets/unused_86_animation_09348_records.inc"
};

static u16 _gUnused86Animation09348Indices[20] = {
#include "assets/unused_86_animation_09348_indices.inc"
};

static AnimationSet _gUnused86Animation09348 = {
    _gUnused86Animation09348Records,
    _gUnused86Animation09348Indices,
    { NULL, _gUnused86Animation09348Bank1, NULL, NULL, _gUnused86Animation09348Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gUnused86Animation09520Bank1[3] = {
#include "assets/unused_86_animation_09520_bank1.inc"
};

static AnimationPackedRotation _gUnused86Animation09520Bank4[32] = {
#include "assets/unused_86_animation_09520_bank4.inc"
};

static AnimationRecord _gUnused86Animation09520Records[57] = {
#include "assets/unused_86_animation_09520_records.inc"
};

static u16 _gUnused86Animation09520Indices[20] = {
#include "assets/unused_86_animation_09520_indices.inc"
};

static AnimationSet _gUnused86Animation09520 = {
    _gUnused86Animation09520Records,
    _gUnused86Animation09520Indices,
    { NULL, _gUnused86Animation09520Bank1, NULL, NULL, _gUnused86Animation09520Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gUnused86Animation09A84Bank1[11] = {
#include "assets/unused_86_animation_09A84_bank1.inc"
};

static AnimationPackedRotation _gUnused86Animation09A84Bank4[125] = {
#include "assets/unused_86_animation_09A84_bank4.inc"
};

static AnimationRecord _gUnused86Animation09A84Records[167] = {
#include "assets/unused_86_animation_09A84_records.inc"
};

static u16 _gUnused86Animation09A84Indices[20] = {
#include "assets/unused_86_animation_09A84_indices.inc"
};

static AnimationSet _gUnused86Animation09A84 = {
    _gUnused86Animation09A84Records,
    _gUnused86Animation09A84Indices,
    { NULL, _gUnused86Animation09A84Bank1, NULL, NULL, _gUnused86Animation09A84Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gUnused86Animation09C78Bank1[3] = {
#include "assets/unused_86_animation_09C78_bank1.inc"
};

static AnimationPackedRotation _gUnused86Animation09C78Bank4[20] = {
#include "assets/unused_86_animation_09C78_bank4.inc"
};

static AnimationRecord _gUnused86Animation09C78Records[76] = {
#include "assets/unused_86_animation_09C78_records.inc"
};

static u16 _gUnused86Animation09C78Indices[20] = {
#include "assets/unused_86_animation_09C78_indices.inc"
};

static AnimationSet _gUnused86Animation09C78 = {
    _gUnused86Animation09C78Records,
    _gUnused86Animation09C78Indices,
    { NULL, _gUnused86Animation09C78Bank1, NULL, NULL, _gUnused86Animation09C78Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gUnused86Animation0A0F8Bank1[8] = {
#include "assets/unused_86_animation_0A0F8_bank1.inc"
};

static AnimationPackedRotation _gUnused86Animation0A0F8Bank4[105] = {
#include "assets/unused_86_animation_0A0F8_bank4.inc"
};

static AnimationRecord _gUnused86Animation0A0F8Records[139] = {
#include "assets/unused_86_animation_0A0F8_records.inc"
};

static u16 _gUnused86Animation0A0F8Indices[20] = {
#include "assets/unused_86_animation_0A0F8_indices.inc"
};

static AnimationSet _gUnused86Animation0A0F8 = {
    _gUnused86Animation0A0F8Records,
    _gUnused86Animation0A0F8Indices,
    { NULL, _gUnused86Animation0A0F8Bank1, NULL, NULL, _gUnused86Animation0A0F8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gUnused86Animation0A2D4Bank1[2] = {
#include "assets/unused_86_animation_0A2D4_bank1.inc"
};

static AnimationPackedRotation _gUnused86Animation0A2D4Bank4[17] = {
#include "assets/unused_86_animation_0A2D4_bank4.inc"
};

static AnimationRecord _gUnused86Animation0A2D4Records[76] = {
#include "assets/unused_86_animation_0A2D4_records.inc"
};

static u16 _gUnused86Animation0A2D4Indices[20] = {
#include "assets/unused_86_animation_0A2D4_indices.inc"
};

static AnimationSet _gUnused86Animation0A2D4 = {
    _gUnused86Animation0A2D4Records,
    _gUnused86Animation0A2D4Indices,
    { NULL, _gUnused86Animation0A2D4Bank1, NULL, NULL, _gUnused86Animation0A2D4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gUnused86Animation0A9C4Bank1[12] = {
#include "assets/unused_86_animation_0A9C4_bank1.inc"
};

static AnimationPackedRotation _gUnused86Animation0A9C4Bank4[164] = {
#include "assets/unused_86_animation_0A9C4_bank4.inc"
};

static AnimationRecord _gUnused86Animation0A9C4Records[224] = {
#include "assets/unused_86_animation_0A9C4_records.inc"
};

static u16 _gUnused86Animation0A9C4Indices[20] = {
#include "assets/unused_86_animation_0A9C4_indices.inc"
};

static AnimationSet _gUnused86Animation0A9C4 = {
    _gUnused86Animation0A9C4Records,
    _gUnused86Animation0A9C4Indices,
    { NULL, _gUnused86Animation0A9C4Bank1, NULL, NULL, _gUnused86Animation0A9C4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gUnused86Animation0B5B0Bank1[19] = {
#include "assets/unused_86_animation_0B5B0_bank1.inc"
};

static AnimationPackedRotation _gUnused86Animation0B5B0Bank4[304] = {
#include "assets/unused_86_animation_0B5B0_bank4.inc"
};

static AnimationRecord _gUnused86Animation0B5B0Records[382] = {
#include "assets/unused_86_animation_0B5B0_records.inc"
};

static u16 _gUnused86Animation0B5B0Indices[20] = {
#include "assets/unused_86_animation_0B5B0_indices.inc"
};

static AnimationSet _gUnused86Animation0B5B0 = {
    _gUnused86Animation0B5B0Records,
    _gUnused86Animation0B5B0Indices,
    { NULL, _gUnused86Animation0B5B0Bank1, NULL, NULL, _gUnused86Animation0B5B0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gUnused86Animation0BED8Bank1[15] = {
#include "assets/unused_86_animation_0BED8_bank1.inc"
};

static AnimationPackedRotation _gUnused86Animation0BED8Bank4[231] = {
#include "assets/unused_86_animation_0BED8_bank4.inc"
};

static AnimationRecord _gUnused86Animation0BED8Records[290] = {
#include "assets/unused_86_animation_0BED8_records.inc"
};

static u16 _gUnused86Animation0BED8Indices[20] = {
#include "assets/unused_86_animation_0BED8_indices.inc"
};

static AnimationSet _gUnused86Animation0BED8 = {
    _gUnused86Animation0BED8Records,
    _gUnused86Animation0BED8Indices,
    { NULL, _gUnused86Animation0BED8Bank1, NULL, NULL, _gUnused86Animation0BED8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gUnused86Animation0C2A8Bank1[6] = {
#include "assets/unused_86_animation_0C2A8_bank1.inc"
};

static AnimationPackedRotation _gUnused86Animation0C2A8Bank4[81] = {
#include "assets/unused_86_animation_0C2A8_bank4.inc"
};

static AnimationRecord _gUnused86Animation0C2A8Records[125] = {
#include "assets/unused_86_animation_0C2A8_records.inc"
};

static u16 _gUnused86Animation0C2A8Indices[20] = {
#include "assets/unused_86_animation_0C2A8_indices.inc"
};

static AnimationSet _gUnused86Animation0C2A8 = {
    _gUnused86Animation0C2A8Records,
    _gUnused86Animation0C2A8Indices,
    { NULL, _gUnused86Animation0C2A8Bank1, NULL, NULL, _gUnused86Animation0C2A8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gUnused86Animation0C924Bank1[11] = {
#include "assets/unused_86_animation_0C924_bank1.inc"
};

static AnimationPackedRotation _gUnused86Animation0C924Bank4[161] = {
#include "assets/unused_86_animation_0C924_bank4.inc"
};

static AnimationRecord _gUnused86Animation0C924Records[201] = {
#include "assets/unused_86_animation_0C924_records.inc"
};

static u16 _gUnused86Animation0C924Indices[20] = {
#include "assets/unused_86_animation_0C924_indices.inc"
};

static AnimationSet _gUnused86Animation0C924 = {
    _gUnused86Animation0C924Records,
    _gUnused86Animation0C924Indices,
    { NULL, _gUnused86Animation0C924Bank1, NULL, NULL, _gUnused86Animation0C924Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gUnused86Animation0CEF0Bank1[10] = {
#include "assets/unused_86_animation_0CEF0_bank1.inc"
};

static AnimationPackedRotation _gUnused86Animation0CEF0Bank4[145] = {
#include "assets/unused_86_animation_0CEF0_bank4.inc"
};

static AnimationRecord _gUnused86Animation0CEF0Records[176] = {
#include "assets/unused_86_animation_0CEF0_records.inc"
};

static u16 _gUnused86Animation0CEF0Indices[20] = {
#include "assets/unused_86_animation_0CEF0_indices.inc"
};

static AnimationSet _gUnused86Animation0CEF0 = {
    _gUnused86Animation0CEF0Records,
    _gUnused86Animation0CEF0Indices,
    { NULL, _gUnused86Animation0CEF0Bank1, NULL, NULL, _gUnused86Animation0CEF0Bank4, NULL, NULL, NULL },
};

AnimationBank D_unused_86_8012A0D8 = { { {
    NULL,
    &_gUnused86PlayerIdleAnimation,
    &_gUnused86PlayerWalkAnimation,
    &_gUnused86PlayerBackwardWalkAnimation,
    &_gUnused86PlayerRunAnimation,
    &_gUnused86PlayerTurnLeftAnimation,
    &_gUnused86PlayerTurnRightAnimation,
    &_gUnused86Animation0C924,
    &_gUnused86Animation0CEF0,
    &_gUnused86Animation0A2D4,
    &_gUnused86Animation0C2A8,
    &_gUnused86Animation0C2A8,
    &_gUnused86Animation0B5B0,
    &_gUnused86Animation0A9C4,
    &_gUnused86Animation0BED8,
    &_gUnused86Animation0BED8,
    &_gUnused86Animation05EB8,
    &_gUnused86Animation061D8,
    &_gUnused86Animation0699C,
    &_gUnused86PlayerUnarmedWalkAnimation,
    &_gUnused86Animation0BED8,
    &_gUnused86PlayerIdleAnimation,
    &_gUnused86PlayerIdleAnimation,
    &_gUnused86Animation07C34,
    &_gUnused86Animation08EC8,
    &_gUnused86Animation087AC,
    &_gUnused86Animation04F38,
    &_gUnused86Animation05134,
    &_gUnused86Animation0540C,
    &_gUnused86Animation056B0,
    &_gUnused86Animation058B0,
    &_gUnused86Animation05C04,
    &_gUnused86Animation09348,
    &_gUnused86Animation09520,
    &_gUnused86Animation09348,
    &_gUnused86Animation09520,
    &_gUnused86PlayerStairAscentAnimation,
    &_gUnused86PlayerStairDescentAnimation,
    &_gUnused86PlayerStairAscentFinishEvenAnimation,
    &_gUnused86PlayerStairAscentFinishOddAnimation,
    &_gUnused86PlayerItemUseAnimation,
    &_gUnused86PlayerIdleAnimation,
    &_gUnused86Animation09A84,
    &_gUnused86Animation09C78,
    &_gUnused86Animation0A0F8,
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
