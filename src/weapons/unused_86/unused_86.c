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

/// Family-0 PE release translation/rotation poses for player tracks 0 and 1.
///
/// Translation uses model units; Euler angles use 4096 units per turn.
/// Read-only clip payloads retain their serialized placement in .data.
static const AnimationPackedPose _gUnused86PlayerPeReleaseFamily0AnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/unused_86_animation_04F38_bank1.inc"
};

/// Family-0 PE release packed Euler poses for tracks 2..18, in eight-angle-unit steps.
static const AnimationPackedRotation _gUnused86PlayerPeReleaseFamily0AnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/unused_86_animation_04F38_bank4.inc"
};

/// Family-0 PE release keyframes and final-pose stop controls.
///
/// Durations count normal-rate frames; pose offsets count four-byte words.
static const AnimationRecord _gUnused86PlayerPeReleaseFamily0AnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/unused_86_animation_04F38_records.inc"
};

/// Absolute family-0 PE release record starts for tracks 0..18, then a retained serialized halfword.
static const u16 _gUnused86PlayerPeReleaseFamily0AnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/unused_86_animation_04F38_indices.inc"
};

/// Player Parasite Energy family-0 release in the unused weapon animation bank.
///
/// Native clip 26 starts the release/charge/cast sequence for clip family 0;
/// child tracks blend into it while the attachment countdown begins.
/// Contains 19 part tracks: translation/rotation for 0 and 1, packed rotation
/// for 2..18. Keep the loaded package present while playback slots or cached
/// record pointers refer to the descriptor or its arrays.
static AnimationSet _gUnused86PlayerPeReleaseFamily0Animation = {
    .records           = _gUnused86PlayerPeReleaseFamily0AnimationRecords,
    .trackStartIndices = _gUnused86PlayerPeReleaseFamily0AnimationIndices,
    .poseBanks         = {
        [UNUSED_86_ANIMATION_TRANSLATION_ROTATION_BANK] = _gUnused86PlayerPeReleaseFamily0AnimationBank1,
        [UNUSED_86_ANIMATION_PACKED_ROTATION_BANK]      = _gUnused86PlayerPeReleaseFamily0AnimationBank4,
    },
};

/// Family-0 PE charge translation/rotation poses for tracks 0 and 1, in model units and 1/4096 turns.
static const AnimationPackedPose _gUnused86PlayerPeChargeFamily0AnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/unused_86_animation_05134_bank1.inc"
};

/// Family-0 PE charge packed Euler poses for tracks 2..18, in eight-angle-unit steps.
static const AnimationPackedRotation _gUnused86PlayerPeChargeFamily0AnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/unused_86_animation_05134_bank4.inc"
};

/// Family-0 PE charge's two one-frame keyframes, return jump and retained stop per track.
///
/// Pose offsets count four-byte words; jump targets are absolute record indices.
static const AnimationRecord _gUnused86PlayerPeChargeFamily0AnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/unused_86_animation_05134_records.inc"
};

/// Absolute family-0 PE charge record starts for tracks 0..18, then a retained serialized halfword.
static const u16 _gUnused86PlayerPeChargeFamily0AnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/unused_86_animation_05134_indices.inc"
};

/// Player Parasite Energy family-0 charge in the unused weapon animation bank.
///
/// Native clip 27 loops two poses per track while the attachment countdown
/// runs. Clip 28 replaces it when the countdown expires.
/// Contains 19 part tracks: translation/rotation for 0 and 1, packed rotation
/// for 2..18. Keep the loaded package present while playback slots or cached
/// record pointers refer to the descriptor or its arrays.
static AnimationSet _gUnused86PlayerPeChargeFamily0Animation = {
    .records           = _gUnused86PlayerPeChargeFamily0AnimationRecords,
    .trackStartIndices = _gUnused86PlayerPeChargeFamily0AnimationIndices,
    .poseBanks         = {
        [UNUSED_86_ANIMATION_TRANSLATION_ROTATION_BANK] = _gUnused86PlayerPeChargeFamily0AnimationBank1,
        [UNUSED_86_ANIMATION_PACKED_ROTATION_BANK]      = _gUnused86PlayerPeChargeFamily0AnimationBank4,
    },
};

/// Family-0 PE cast translation/rotation poses for tracks 0 and 1, in model units and 1/4096 turns.
static const AnimationPackedPose _gUnused86PlayerPeCastFamily0AnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/unused_86_animation_0540C_bank1.inc"
};

/// Family-0 PE cast packed Euler poses for tracks 2..18, in eight-angle-unit steps.
static const AnimationPackedRotation _gUnused86PlayerPeCastFamily0AnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/unused_86_animation_0540C_bank4.inc"
};

/// Family-0 PE cast keyframes and final-pose stop controls.
///
/// Durations count normal-rate frames; pose offsets count four-byte words.
static const AnimationRecord _gUnused86PlayerPeCastFamily0AnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/unused_86_animation_0540C_records.inc"
};

/// Absolute family-0 PE cast record starts for tracks 0..18, then a retained serialized halfword.
static const u16 _gUnused86PlayerPeCastFamily0AnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/unused_86_animation_0540C_indices.inc"
};

/// Player Parasite Energy family-0 cast in the unused weapon animation bank.
///
/// Native clip 28 follows the family-0 charge when the attachment countdown
/// expires. Completion allows the interrupted locomotion or aim to resume.
/// Contains 19 part tracks: translation/rotation for 0 and 1, packed rotation
/// for 2..18. Keep the loaded package present while playback slots or cached
/// record pointers refer to the descriptor or its arrays.
static AnimationSet _gUnused86PlayerPeCastFamily0Animation = {
    .records           = _gUnused86PlayerPeCastFamily0AnimationRecords,
    .trackStartIndices = _gUnused86PlayerPeCastFamily0AnimationIndices,
    .poseBanks         = {
        [UNUSED_86_ANIMATION_TRANSLATION_ROTATION_BANK] = _gUnused86PlayerPeCastFamily0AnimationBank1,
        [UNUSED_86_ANIMATION_PACKED_ROTATION_BANK]      = _gUnused86PlayerPeCastFamily0AnimationBank4,
    },
};

/// Family-1 PE release translation/rotation poses for tracks 0 and 1, in model units and 1/4096 turns.
static const AnimationPackedPose _gUnused86PlayerPeReleaseFamily1AnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/unused_86_animation_056B0_bank1.inc"
};

/// Family-1 PE release packed Euler poses for tracks 2..18, in eight-angle-unit steps.
static const AnimationPackedRotation _gUnused86PlayerPeReleaseFamily1AnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/unused_86_animation_056B0_bank4.inc"
};

/// Family-1 PE release keyframes and final-pose stop controls.
///
/// Durations count normal-rate frames; pose offsets count four-byte words.
static const AnimationRecord _gUnused86PlayerPeReleaseFamily1AnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/unused_86_animation_056B0_records.inc"
};

/// Absolute family-1 PE release record starts for tracks 0..18, then a retained serialized halfword.
static const u16 _gUnused86PlayerPeReleaseFamily1AnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/unused_86_animation_056B0_indices.inc"
};

/// Player Parasite Energy family-1 release in the unused weapon animation bank.
///
/// Native clip 29 starts the release/charge/cast sequence for clip family 1;
/// child tracks blend into it while the attachment countdown begins.
/// Contains 19 part tracks: translation/rotation for 0 and 1, packed rotation
/// for 2..18. Keep the loaded package present while playback slots or cached
/// record pointers refer to the descriptor or its arrays.
static AnimationSet _gUnused86PlayerPeReleaseFamily1Animation = {
    .records           = _gUnused86PlayerPeReleaseFamily1AnimationRecords,
    .trackStartIndices = _gUnused86PlayerPeReleaseFamily1AnimationIndices,
    .poseBanks         = {
        [UNUSED_86_ANIMATION_TRANSLATION_ROTATION_BANK] = _gUnused86PlayerPeReleaseFamily1AnimationBank1,
        [UNUSED_86_ANIMATION_PACKED_ROTATION_BANK]      = _gUnused86PlayerPeReleaseFamily1AnimationBank4,
    },
};

/// Family-1 PE charge translation/rotation poses for tracks 0 and 1, in model units and 1/4096 turns.
static const AnimationPackedPose _gUnused86PlayerPeChargeFamily1AnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/unused_86_animation_058B0_bank1.inc"
};

/// Family-1 PE charge packed Euler poses for tracks 2..18, in eight-angle-unit steps.
static const AnimationPackedRotation _gUnused86PlayerPeChargeFamily1AnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/unused_86_animation_058B0_bank4.inc"
};

/// Family-1 PE charge's two one-frame keyframes, return jump and retained stop per track.
///
/// Pose offsets count four-byte words; jump targets are absolute record indices.
static const AnimationRecord _gUnused86PlayerPeChargeFamily1AnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/unused_86_animation_058B0_records.inc"
};

/// Absolute family-1 PE charge record starts for tracks 0..18, then a retained serialized halfword.
static const u16 _gUnused86PlayerPeChargeFamily1AnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/unused_86_animation_058B0_indices.inc"
};

/// Player Parasite Energy family-1 charge in the unused weapon animation bank.
///
/// Native clip 30 loops two poses per track while the attachment countdown
/// runs. Clip 31 replaces it when the countdown expires.
/// Contains 19 part tracks: translation/rotation for 0 and 1, packed rotation
/// for 2..18. Keep the loaded package present while playback slots or cached
/// record pointers refer to the descriptor or its arrays.
static AnimationSet _gUnused86PlayerPeChargeFamily1Animation = {
    .records           = _gUnused86PlayerPeChargeFamily1AnimationRecords,
    .trackStartIndices = _gUnused86PlayerPeChargeFamily1AnimationIndices,
    .poseBanks         = {
        [UNUSED_86_ANIMATION_TRANSLATION_ROTATION_BANK] = _gUnused86PlayerPeChargeFamily1AnimationBank1,
        [UNUSED_86_ANIMATION_PACKED_ROTATION_BANK]      = _gUnused86PlayerPeChargeFamily1AnimationBank4,
    },
};

/// Family-1 PE cast translation/rotation poses for tracks 0 and 1, in model units and 1/4096 turns.
static const AnimationPackedPose _gUnused86PlayerPeCastFamily1AnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/unused_86_animation_05C04_bank1.inc"
};

/// Family-1 PE cast packed Euler poses for tracks 2..18, in eight-angle-unit steps.
static const AnimationPackedRotation _gUnused86PlayerPeCastFamily1AnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/unused_86_animation_05C04_bank4.inc"
};

/// Family-1 PE cast keyframes and final-pose stop controls.
///
/// Durations count normal-rate frames; pose offsets count four-byte words.
static const AnimationRecord _gUnused86PlayerPeCastFamily1AnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/unused_86_animation_05C04_records.inc"
};

/// Absolute family-1 PE cast record starts for tracks 0..18, then a retained serialized halfword.
static const u16 _gUnused86PlayerPeCastFamily1AnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/unused_86_animation_05C04_indices.inc"
};

/// Player Parasite Energy family-1 cast in the unused weapon animation bank.
///
/// Native clip 31 follows the family-1 charge when the attachment countdown
/// expires. Completion allows the interrupted locomotion or aim to resume.
/// Contains 19 part tracks: translation/rotation for 0 and 1, packed rotation
/// for 2..18. Keep the loaded package present while playback slots or cached
/// record pointers refer to the descriptor or its arrays.
static AnimationSet _gUnused86PlayerPeCastFamily1Animation = {
    .records           = _gUnused86PlayerPeCastFamily1AnimationRecords,
    .trackStartIndices = _gUnused86PlayerPeCastFamily1AnimationIndices,
    .poseBanks         = {
        [UNUSED_86_ANIMATION_TRANSLATION_ROTATION_BANK] = _gUnused86PlayerPeCastFamily1AnimationBank1,
        [UNUSED_86_ANIMATION_PACKED_ROTATION_BANK]      = _gUnused86PlayerPeCastFamily1AnimationBank4,
    },
};

/// Part-4 hit translation/rotation poses for tracks 0 and 1, in model units and 1/4096 turns.
///
/// Read-only payloads retain their serialized placement in .data; pose banks are word-aligned.
static const AnimationPackedPose _gUnused86PlayerHitPart4AnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/unused_86_animation_05EB8_bank1.inc"
};

/// Part-4 hit packed Euler poses for player tracks 2..18, in eight-angle-unit steps.
static const AnimationPackedRotation _gUnused86PlayerHitPart4AnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/unused_86_animation_05EB8_bank4.inc"
};

/// Part-4 hit keyframes and stops; durations count normal-rate frames and pose offsets count four-byte words.
static const AnimationRecord _gUnused86PlayerHitPart4AnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/unused_86_animation_05EB8_records.inc"
};

/// Absolute part-4 hit record starts for tracks 0..18, followed by a retained serialized zero halfword.
static const u16 _gUnused86PlayerHitPart4AnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/unused_86_animation_05EB8_indices.inc"
};

/// Player hit reaction for the collision sphere on model part 4 in the unused weapon bank.
///
/// Native clip 16; hit region 1 selects it with a three-normal-frame blend.
/// Each of the 19 tracks ends with a stop: 0..1 carry translation/rotation,
/// 2..18 packed rotation. Keep the package loaded while any playback slot or
/// cached record pointer refers to this descriptor or its arrays.
static AnimationSet _gUnused86PlayerHitPart4Animation = {
    .records           = _gUnused86PlayerHitPart4AnimationRecords,
    .trackStartIndices = _gUnused86PlayerHitPart4AnimationIndices,
    .poseBanks         = {
        [UNUSED_86_ANIMATION_TRANSLATION_ROTATION_BANK] = _gUnused86PlayerHitPart4AnimationBank1,
        [UNUSED_86_ANIMATION_PACKED_ROTATION_BANK]      = _gUnused86PlayerHitPart4AnimationBank4,
    },
};

/// Other-body hit translation/rotation poses for tracks 0 and 1, in model units and 1/4096 turns.
static const AnimationPackedPose _gUnused86PlayerHitOtherBodyAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/unused_86_animation_061D8_bank1.inc"
};

/// Other-body hit packed Euler poses for player tracks 2..18, in eight-angle-unit steps.
static const AnimationPackedRotation _gUnused86PlayerHitOtherBodyAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/unused_86_animation_061D8_bank4.inc"
};

/// Other-body hit keyframes and stops; durations count normal-rate frames and pose offsets count four-byte words.
static const AnimationRecord _gUnused86PlayerHitOtherBodyAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/unused_86_animation_061D8_records.inc"
};

/// Absolute other-body hit record starts for tracks 0..18, followed by a retained serialized zero halfword.
static const u16 _gUnused86PlayerHitOtherBodyAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/unused_86_animation_061D8_indices.inc"
};

/// Player hit reaction for the root and part-1 collision spheres in the unused weapon bank.
///
/// Native clip 17; hit region 2 selects it with a three-normal-frame blend.
/// Each of the 19 tracks ends with a stop: 0..1 carry translation/rotation,
/// 2..18 packed rotation. Keep the package loaded while any playback slot or
/// cached record pointer refers to this descriptor or its arrays.
static AnimationSet _gUnused86PlayerHitOtherBodyAnimation = {
    .records           = _gUnused86PlayerHitOtherBodyAnimationRecords,
    .trackStartIndices = _gUnused86PlayerHitOtherBodyAnimationIndices,
    .poseBanks         = {
        [UNUSED_86_ANIMATION_TRANSLATION_ROTATION_BANK] = _gUnused86PlayerHitOtherBodyAnimationBank1,
        [UNUSED_86_ANIMATION_PACKED_ROTATION_BANK]      = _gUnused86PlayerHitOtherBodyAnimationBank4,
    },
};

/// Death translation/rotation poses for tracks 0 and 1, in model units and 1/4096 turns.
static const AnimationPackedPose _gUnused86PlayerDeathAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/unused_86_animation_0699C_bank1.inc"
};

/// Death packed Euler poses for player tracks 2..18, in eight-angle-unit steps.
static const AnimationPackedRotation _gUnused86PlayerDeathAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/unused_86_animation_0699C_bank4.inc"
};

/// Death keyframes and final-pose stops; durations count normal-rate frames and pose offsets count four-byte words.
static const AnimationRecord _gUnused86PlayerDeathAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/unused_86_animation_0699C_records.inc"
};

/// Absolute death record starts for tracks 0..18, followed by a retained serialized zero halfword.
static const u16 _gUnused86PlayerDeathAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/unused_86_animation_0699C_indices.inc"
};

/// Player death pose in the unused weapon animation bank.
///
/// Native clip 18, selected when HP reaches zero; all 19 tracks stop on their
/// final pose. Tracks 0..1 carry translation/rotation and 2..18 packed rotation.
/// Keep the package loaded while any playback slot or cached record pointer
/// refers to this descriptor or its arrays.
static AnimationSet _gUnused86PlayerDeathAnimation = {
    .records           = _gUnused86PlayerDeathAnimationRecords,
    .trackStartIndices = _gUnused86PlayerDeathAnimationIndices,
    .poseBanks         = {
        [UNUSED_86_ANIMATION_TRANSLATION_ROTATION_BANK] = _gUnused86PlayerDeathAnimationBank1,
        [UNUSED_86_ANIMATION_PACKED_ROTATION_BANK]      = _gUnused86PlayerDeathAnimationBank4,
    },
};

/// High-health idle translation/rotation poses for tracks 0 and 1, in model units and 1/4096 turns.
static const AnimationPackedPose _gUnused86PlayerHighHealthIdleAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/unused_86_animation_07C34_bank1.inc"
};

/// High-health idle packed Euler poses for player tracks 2..18, in eight-angle-unit steps.
static const AnimationPackedRotation _gUnused86PlayerHighHealthIdleAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/unused_86_animation_07C34_bank4.inc"
};

/// High-health idle keyframes, tail-loop controls and stops.
///
/// Durations count normal-rate frames; pose offsets count four-byte words, jumps absolute record indices.
static const AnimationRecord _gUnused86PlayerHighHealthIdleAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/unused_86_animation_07C34_records.inc"
};

/// Absolute high-health idle record starts for tracks 0..18, followed by a retained serialized zero halfword.
static const u16 _gUnused86PlayerHighHealthIdleAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/unused_86_animation_07C34_indices.inc"
};

/// Player long-idle clip above half maximum HP in the unused weapon bank.
///
/// Native clip 23, selected after 300 direction-free normal-locomotion ticks
/// with a five-normal-frame blend. All 19 tracks loop after their opening
/// sequence: 0..1 carry translation/rotation, 2..18 packed rotation. Keep the
/// package loaded while any playback slot or cached record pointer refers to
/// this descriptor or its arrays.
static AnimationSet _gUnused86PlayerHighHealthIdleAnimation = {
    .records           = _gUnused86PlayerHighHealthIdleAnimationRecords,
    .trackStartIndices = _gUnused86PlayerHighHealthIdleAnimationIndices,
    .poseBanks         = {
        [UNUSED_86_ANIMATION_TRANSLATION_ROTATION_BANK] = _gUnused86PlayerHighHealthIdleAnimationBank1,
        [UNUSED_86_ANIMATION_PACKED_ROTATION_BANK]      = _gUnused86PlayerHighHealthIdleAnimationBank4,
    },
};

/// Low-health idle translation/rotation poses for tracks 0 and 1, in model units and 1/4096 turns.
static const AnimationPackedPose _gUnused86PlayerLowHealthIdleAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/unused_86_animation_087AC_bank1.inc"
};

/// Low-health idle packed Euler poses for player tracks 2..18, in eight-angle-unit steps.
static const AnimationPackedRotation _gUnused86PlayerLowHealthIdleAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/unused_86_animation_087AC_bank4.inc"
};

/// Low-health idle keyframes, tail-loop controls and stops, also used for paralysis.
///
/// Durations count normal-rate frames; pose offsets count four-byte words, jumps absolute record indices.
static const AnimationRecord _gUnused86PlayerLowHealthIdleAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/unused_86_animation_087AC_records.inc"
};

/// Absolute low-health idle record starts for tracks 0..18, followed by a retained serialized zero halfword.
static const u16 _gUnused86PlayerLowHealthIdleAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/unused_86_animation_087AC_indices.inc"
};

/// Player long-idle clip at or below quarter maximum HP in the unused weapon bank.
///
/// Native clip 25, selected after 300 direction-free normal-locomotion ticks
/// with a five-normal-frame blend. Paralysis uses the same clip on all child
/// tracks with a six-normal-frame blend. All 19 tracks
/// loop after their opening sequence: 0..1 carry translation/rotation, 2..18
/// packed rotation. Keep the package loaded while any playback slot or cached
/// record pointer refers to this descriptor or its arrays.
static AnimationSet _gUnused86PlayerLowHealthIdleAnimation = {
    .records           = _gUnused86PlayerLowHealthIdleAnimationRecords,
    .trackStartIndices = _gUnused86PlayerLowHealthIdleAnimationIndices,
    .poseBanks         = {
        [UNUSED_86_ANIMATION_TRANSLATION_ROTATION_BANK] = _gUnused86PlayerLowHealthIdleAnimationBank1,
        [UNUSED_86_ANIMATION_PACKED_ROTATION_BANK]      = _gUnused86PlayerLowHealthIdleAnimationBank4,
    },
};

/// Mid-health idle translation/rotation poses for tracks 0 and 1, in model units and 1/4096 turns.
static const AnimationPackedPose _gUnused86PlayerMidHealthIdleAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/unused_86_animation_08EC8_bank1.inc"
};

/// Mid-health idle packed Euler poses for player tracks 2..18, in eight-angle-unit steps.
static const AnimationPackedRotation _gUnused86PlayerMidHealthIdleAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/unused_86_animation_08EC8_bank4.inc"
};

/// Mid-health idle keyframes, tail-loop controls and stops.
///
/// Durations count normal-rate frames; pose offsets count four-byte words, jumps absolute record indices.
static const AnimationRecord _gUnused86PlayerMidHealthIdleAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/unused_86_animation_08EC8_records.inc"
};

/// Absolute mid-health idle record starts for tracks 0..18, followed by a retained serialized zero halfword.
static const u16 _gUnused86PlayerMidHealthIdleAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/unused_86_animation_08EC8_indices.inc"
};

/// Player long-idle clip above quarter and at or below half maximum HP in the unused weapon bank.
///
/// Native clip 24, selected after 300 direction-free normal-locomotion ticks
/// with a five-normal-frame blend. All 19 tracks loop after their opening
/// sequence: 0..1 carry translation/rotation, 2..18 packed rotation. Keep the
/// package loaded while any playback slot or cached record pointer refers to
/// this descriptor or its arrays.
static AnimationSet _gUnused86PlayerMidHealthIdleAnimation = {
    .records           = _gUnused86PlayerMidHealthIdleAnimationRecords,
    .trackStartIndices = _gUnused86PlayerMidHealthIdleAnimationIndices,
    .poseBanks         = {
        [UNUSED_86_ANIMATION_TRANSLATION_ROTATION_BANK] = _gUnused86PlayerMidHealthIdleAnimationBank1,
        [UNUSED_86_ANIMATION_PACKED_ROTATION_BANK]      = _gUnused86PlayerMidHealthIdleAnimationBank4,
    },
};

/// Scripted presentation entry translations in model units and Euler angles in 1/4096 turns for tracks 0..1.
///
/// Read-only clip payloads retain their serialized placement in .data.
static const AnimationPackedPose _gUnused86PlayerScriptedPresentationEnterAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/unused_86_animation_09348_bank1.inc"
};

/// Scripted presentation entry Euler keyframes for tracks 2..18, in eight-angle-unit steps.
static const AnimationPackedRotation _gUnused86PlayerScriptedPresentationEnterAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/unused_86_animation_09348_bank4.inc"
};

/// Scripted presentation entry keyframes and track stops.
///
/// Durations count normal-rate frames; pose offsets count four-byte words.
static const AnimationRecord _gUnused86PlayerScriptedPresentationEnterAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/unused_86_animation_09348_records.inc"
};

/// Absolute presentation entry record starts for tracks 0..18, followed by a retained serialized zero halfword.
static const u16 _gUnused86PlayerScriptedPresentationEnterAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/unused_86_animation_09348_indices.inc"
};

/// Player presentation before a scripted weapon event in the unused weapon animation bank.
///
/// Native clip 32, also stored at entry 34. The zero presentation variant
/// selects this clip before the event's command. All 19 tracks terminate:
/// tracks 0..1 use translation/rotation and 2..18 packed rotation.
/// Keep the package loaded while a slot or cached record pointer refers to
/// this descriptor or its arrays.
static AnimationSet _gUnused86PlayerScriptedPresentationEnterAnimation = {
    .records           = _gUnused86PlayerScriptedPresentationEnterAnimationRecords,
    .trackStartIndices = _gUnused86PlayerScriptedPresentationEnterAnimationIndices,
    .poseBanks         = {
        [UNUSED_86_ANIMATION_TRANSLATION_ROTATION_BANK] = _gUnused86PlayerScriptedPresentationEnterAnimationBank1,
        [UNUSED_86_ANIMATION_PACKED_ROTATION_BANK]      = _gUnused86PlayerScriptedPresentationEnterAnimationBank4,
    },
};

/// Scripted presentation exit translations in model units and Euler angles in 1/4096 turns for tracks 0..1.
///
/// Read-only clip payloads retain their serialized placement in .data.
static const AnimationPackedPose _gUnused86PlayerScriptedPresentationExitAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/unused_86_animation_09520_bank1.inc"
};

/// Scripted presentation exit Euler keyframes for tracks 2..18, in eight-angle-unit steps.
static const AnimationPackedRotation _gUnused86PlayerScriptedPresentationExitAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/unused_86_animation_09520_bank4.inc"
};

/// Scripted presentation exit keyframes and track stops.
///
/// Durations count normal-rate frames; pose offsets count four-byte words.
static const AnimationRecord _gUnused86PlayerScriptedPresentationExitAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/unused_86_animation_09520_records.inc"
};

/// Absolute presentation exit record starts for tracks 0..18, followed by a retained serialized zero halfword.
static const u16 _gUnused86PlayerScriptedPresentationExitAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/unused_86_animation_09520_indices.inc"
};

/// Player presentation after a scripted weapon event in the unused weapon animation bank.
///
/// Native clip 33, also stored at entry 35. A nonzero presentation variant
/// selects this clip after the event's command. All 19 tracks terminate:
/// tracks 0..1 use translation/rotation and 2..18 packed rotation.
/// Keep the package loaded while a slot or cached record pointer refers to
/// this descriptor or its arrays.
static AnimationSet _gUnused86PlayerScriptedPresentationExitAnimation = {
    .records           = _gUnused86PlayerScriptedPresentationExitAnimationRecords,
    .trackStartIndices = _gUnused86PlayerScriptedPresentationExitAnimationIndices,
    .poseBanks         = {
        [UNUSED_86_ANIMATION_TRANSLATION_ROTATION_BANK] = _gUnused86PlayerScriptedPresentationExitAnimationBank1,
        [UNUSED_86_ANIMATION_PACKED_ROTATION_BANK]      = _gUnused86PlayerScriptedPresentationExitAnimationBank4,
    },
};

/// PE family 2 release translations in model units and Euler angles in 1/4096 turns for tracks 0..1.
///
/// Read-only clip payloads retain their serialized placement in .data.
static const AnimationPackedPose _gUnused86PlayerPeReleaseFamily2AnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/unused_86_animation_09A84_bank1.inc"
};

/// PE family 2 release Euler keyframes for tracks 2..18, in eight-angle-unit steps.
static const AnimationPackedRotation _gUnused86PlayerPeReleaseFamily2AnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/unused_86_animation_09A84_bank4.inc"
};

/// PE family 2 release keyframes and track stops.
///
/// Durations count normal-rate frames; pose offsets count four-byte words.
static const AnimationRecord _gUnused86PlayerPeReleaseFamily2AnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/unused_86_animation_09A84_records.inc"
};

/// Absolute PE family 2 release record starts for tracks 0..18, followed by a retained serialized zero halfword.
static const u16 _gUnused86PlayerPeReleaseFamily2AnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/unused_86_animation_09A84_indices.inc"
};

/// Player Parasite Energy release for clip family 2 in the unused weapon animation bank.
///
/// Native clip 42 starts the release with a six-normal-frame blend. Family 2
/// covers attachment IDs below 601 whose decimal tens digit is 3. All 19
/// tracks terminate: 0..1 use translation/rotation, 2..18 packed rotation.
/// Keep the package loaded while a slot or cached record pointer refers to
/// this descriptor or its arrays.
static AnimationSet _gUnused86PlayerPeReleaseFamily2Animation = {
    .records           = _gUnused86PlayerPeReleaseFamily2AnimationRecords,
    .trackStartIndices = _gUnused86PlayerPeReleaseFamily2AnimationIndices,
    .poseBanks         = {
        [UNUSED_86_ANIMATION_TRANSLATION_ROTATION_BANK] = _gUnused86PlayerPeReleaseFamily2AnimationBank1,
        [UNUSED_86_ANIMATION_PACKED_ROTATION_BANK]      = _gUnused86PlayerPeReleaseFamily2AnimationBank4,
    },
};

/// PE family 2 charge translations in model units and Euler angles in 1/4096 turns for tracks 0..1.
///
/// Read-only clip payloads retain their serialized placement in .data.
static const AnimationPackedPose _gUnused86PlayerPeChargeFamily2AnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/unused_86_animation_09C78_bank1.inc"
};

/// PE family 2 charge Euler keyframes for tracks 2..18, in eight-angle-unit steps.
static const AnimationPackedRotation _gUnused86PlayerPeChargeFamily2AnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/unused_86_animation_09C78_bank4.inc"
};

/// PE family 2 charge keyframes, track loop jumps and retained stops.
///
/// Durations count normal-rate frames, pose offsets four-byte words and jumps absolute record indices.
static const AnimationRecord _gUnused86PlayerPeChargeFamily2AnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/unused_86_animation_09C78_records.inc"
};

/// Absolute PE family 2 charge record starts for tracks 0..18, followed by a retained serialized zero halfword.
static const u16 _gUnused86PlayerPeChargeFamily2AnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/unused_86_animation_09C78_indices.inc"
};

/// Player Parasite Energy charge loop for clip family 2 in the unused weapon animation bank.
///
/// Native clip 43 holds the charge until the attachment countdown reaches zero.
/// Family 2 covers attachment IDs below 601 whose decimal tens digit is 3.
/// All 19 tracks loop: 0..1 use translation/rotation, 2..18 packed rotation.
/// Keep the package loaded while a slot or cached record pointer refers to
/// this descriptor or its arrays.
static AnimationSet _gUnused86PlayerPeChargeFamily2Animation = {
    .records           = _gUnused86PlayerPeChargeFamily2AnimationRecords,
    .trackStartIndices = _gUnused86PlayerPeChargeFamily2AnimationIndices,
    .poseBanks         = {
        [UNUSED_86_ANIMATION_TRANSLATION_ROTATION_BANK] = _gUnused86PlayerPeChargeFamily2AnimationBank1,
        [UNUSED_86_ANIMATION_PACKED_ROTATION_BANK]      = _gUnused86PlayerPeChargeFamily2AnimationBank4,
    },
};

/// PE family 2 cast translations in model units and Euler angles in 1/4096 turns for tracks 0..1.
///
/// Read-only clip payloads retain their serialized placement in .data.
static const AnimationPackedPose _gUnused86PlayerPeCastFamily2AnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/unused_86_animation_0A0F8_bank1.inc"
};

/// PE family 2 cast Euler keyframes for tracks 2..18, in eight-angle-unit steps.
static const AnimationPackedRotation _gUnused86PlayerPeCastFamily2AnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/unused_86_animation_0A0F8_bank4.inc"
};

/// PE family 2 cast keyframes and track stops.
///
/// Durations count normal-rate frames; pose offsets count four-byte words.
static const AnimationRecord _gUnused86PlayerPeCastFamily2AnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/unused_86_animation_0A0F8_records.inc"
};

/// Absolute PE family 2 cast record starts for tracks 0..18, followed by a retained serialized zero halfword.
static const u16 _gUnused86PlayerPeCastFamily2AnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/unused_86_animation_0A0F8_indices.inc"
};

/// Player Parasite Energy cast for clip family 2 in the unused weapon animation bank.
///
/// Native clip 44 starts after the attachment countdown reaches zero. Family 2
/// covers attachment IDs below 601 whose decimal tens digit is 3. All 19
/// tracks terminate: 0..1 use translation/rotation, 2..18 packed rotation.
/// Keep the package loaded while a slot or cached record pointer refers to
/// this descriptor or its arrays.
static AnimationSet _gUnused86PlayerPeCastFamily2Animation = {
    .records           = _gUnused86PlayerPeCastFamily2AnimationRecords,
    .trackStartIndices = _gUnused86PlayerPeCastFamily2AnimationIndices,
    .poseBanks         = {
        [UNUSED_86_ANIMATION_TRANSLATION_ROTATION_BANK] = _gUnused86PlayerPeCastFamily2AnimationBank1,
        [UNUSED_86_ANIMATION_PACKED_ROTATION_BANK]      = _gUnused86PlayerPeCastFamily2AnimationBank4,
    },
};

/// Aim idle translations in model units and Euler angles in 1/4096 turns for tracks 0..1.
///
/// Read-only clip payloads retain their serialized placement in .data.
static const AnimationPackedPose _gUnused86PlayerAimIdleAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/unused_86_animation_0A2D4_bank1.inc"
};

/// Aim idle Euler keyframes for tracks 2..18, in eight-angle-unit steps.
static const AnimationPackedRotation _gUnused86PlayerAimIdleAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/unused_86_animation_0A2D4_bank4.inc"
};

/// Aim idle keyframes, track loop jumps and retained stops.
///
/// Durations count normal-rate frames, pose offsets four-byte words and jumps absolute record indices.
static const AnimationRecord _gUnused86PlayerAimIdleAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/unused_86_animation_0A2D4_records.inc"
};

/// Absolute aim idle record starts for tracks 0..18, followed by a retained serialized zero halfword.
static const u16 _gUnused86PlayerAimIdleAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/unused_86_animation_0A2D4_indices.inc"
};

/// Player stationary aiming loop in the unused weapon animation bank.
///
/// Native clip 9 is selected while aiming without movement or turning.
/// All 19 tracks loop: 0..1 use translation/rotation, 2..18 packed rotation.
/// Keep the package loaded while a slot or cached record pointer refers to
/// this descriptor or its arrays.
static AnimationSet _gUnused86PlayerAimIdleAnimation = {
    .records           = _gUnused86PlayerAimIdleAnimationRecords,
    .trackStartIndices = _gUnused86PlayerAimIdleAnimationIndices,
    .poseBanks         = {
        [UNUSED_86_ANIMATION_TRANSLATION_ROTATION_BANK] = _gUnused86PlayerAimIdleAnimationBank1,
        [UNUSED_86_ANIMATION_PACKED_ROTATION_BANK]      = _gUnused86PlayerAimIdleAnimationBank4,
    },
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
    &_gUnused86PlayerAimIdleAnimation,
    &_gUnused86Animation0C2A8,
    &_gUnused86Animation0C2A8,
    &_gUnused86Animation0B5B0,
    &_gUnused86Animation0A9C4,
    &_gUnused86Animation0BED8,
    &_gUnused86Animation0BED8,
    &_gUnused86PlayerHitPart4Animation,
    &_gUnused86PlayerHitOtherBodyAnimation,
    &_gUnused86PlayerDeathAnimation,
    &_gUnused86PlayerUnarmedWalkAnimation,
    &_gUnused86Animation0BED8,
    &_gUnused86PlayerIdleAnimation,
    &_gUnused86PlayerIdleAnimation,
    &_gUnused86PlayerHighHealthIdleAnimation,
    &_gUnused86PlayerMidHealthIdleAnimation,
    &_gUnused86PlayerLowHealthIdleAnimation,
    &_gUnused86PlayerPeReleaseFamily0Animation,
    &_gUnused86PlayerPeChargeFamily0Animation,
    &_gUnused86PlayerPeCastFamily0Animation,
    &_gUnused86PlayerPeReleaseFamily1Animation,
    &_gUnused86PlayerPeChargeFamily1Animation,
    &_gUnused86PlayerPeCastFamily1Animation,
    &_gUnused86PlayerScriptedPresentationEnterAnimation,
    &_gUnused86PlayerScriptedPresentationExitAnimation,
    &_gUnused86PlayerScriptedPresentationEnterAnimation,
    &_gUnused86PlayerScriptedPresentationExitAnimation,
    &_gUnused86PlayerStairAscentAnimation,
    &_gUnused86PlayerStairDescentAnimation,
    &_gUnused86PlayerStairAscentFinishEvenAnimation,
    &_gUnused86PlayerStairAscentFinishOddAnimation,
    &_gUnused86PlayerItemUseAnimation,
    &_gUnused86PlayerIdleAnimation,
    &_gUnused86PlayerPeReleaseFamily2Animation,
    &_gUnused86PlayerPeChargeFamily2Animation,
    &_gUnused86PlayerPeCastFamily2Animation,
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
