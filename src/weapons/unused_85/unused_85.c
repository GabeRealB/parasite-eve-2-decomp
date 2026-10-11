#include "weapons/weapon.h"

#include <psyq/libgpu.h>

#include "gameplay/animation.h"

enum {
    UNUSED_85_HELD_WEAPON_QUAD_COUNT              = 18,
    UNUSED_85_ANIMATION_TRANSLATION_ROTATION_BANK = 1,
    UNUSED_85_ANIMATION_PACKED_ROTATION_BANK      = 4,
};

// Read-only embedded payloads retain the package's fixed addresses in .data.
/// Initial identity transform and self-parented root of the held weapon's single part.
static const TmdBone _gUnused85HeldWeaponModelSkeleton[1] __attribute__((section(".data"))) = {
#include "assets/unused_85_model_0016C_skeleton.inc"
};

/// Vertex count of the held weapon's single part (20 vertices).
static const u32 _gUnused85HeldWeaponModelPartVerts[1] __attribute__((section(".data"))) = {
#include "assets/unused_85_model_0016C_partVerts.inc"
};

/// Held-weapon vertices in integer part-local coordinates, borrowed by runtime models.
static SVECTOR _gUnused85HeldWeaponModelVerts[20] = {
#include "assets/unused_85_model_0016C_verts.inc"
};

/// Held-weapon lighting normals, indexed independently of the vertices by each corner.
static SVECTOR _gUnused85HeldWeaponModelNormals[20] = {
#include "assets/unused_85_model_0016C_normals.inc"
};

/// Eighteen directly transformed GT4 quads; model creation rewrites their handler slot.
static u32 _gUnused85HeldWeaponModelStream[132] = {
#include "assets/unused_85_model_0016C_stream.inc"
};

TmdSource gUnused85HeldWeaponModel = {
    .handlersResolved    = 0,
    .bufferHalfBytes     = UNUSED_85_HELD_WEAPON_QUAD_COUNT * sizeof(POLY_GT4),
    .preXformRegionBytes = 0,
    .partCount           = ARRAY_SIZE(_gUnused85HeldWeaponModelSkeleton),
    .partVertexCounts    = _gUnused85HeldWeaponModelPartVerts,
    .verts               = _gUnused85HeldWeaponModelVerts,
    .normals             = _gUnused85HeldWeaponModelNormals,
    .skeleton            = _gUnused85HeldWeaponModelSkeleton,
    .stream              = _gUnused85HeldWeaponModelStream,
};

/// Idle translation/rotation poses for player-model tracks 0 and 1.
static const AnimationPackedPose _gUnused85IdleAnimationBank1[2] __attribute__((section(".data"))) = {
#include "assets/unused_85_animation_00530_bank1.inc"
};

/// Packed idle rotations for player-model tracks 2..18.
static const AnimationPackedRotation _gUnused85IdleAnimationBank4[8] __attribute__((section(".data"))) = {
#include "assets/unused_85_animation_00530_bank4.inc"
};

/// Idle keyframes and loop/stop controls; keyframe durations count normal-rate frames.
static const AnimationRecord _gUnused85IdleAnimationRecords[76] __attribute__((section(".data"))) = {
#include "assets/unused_85_animation_00530_records.inc"
};

/// Idle track starts for parts 0..18, followed by the retained unused export halfword.
static const u16 _gUnused85IdleAnimationIndices[20] __attribute__((section(".data"))) = {
#include "assets/unused_85_animation_00530_indices.inc"
};

/// Player standing-idle clip for unused weapon index 6.
///
/// Native-bank set 1; sets 21, 22 and 41 also select this same clip.
/// All 19 tracks borrow the embedded payloads while this package is loaded.
static AnimationSet _gUnused85IdleAnimation = {
    .records           = _gUnused85IdleAnimationRecords,
    .trackStartIndices = _gUnused85IdleAnimationIndices,
    .poseBanks         = {
        [UNUSED_85_ANIMATION_TRANSLATION_ROTATION_BANK] = _gUnused85IdleAnimationBank1,
        [UNUSED_85_ANIMATION_PACKED_ROTATION_BANK]      = _gUnused85IdleAnimationBank4,
    },
};

/// Unarmed-walk translation/rotation poses for player-model tracks 0 and 1.
static const AnimationPackedPose _gUnused85UnarmedWalkAnimationBank1[12] __attribute__((section(".data"))) = {
#include "assets/unused_85_animation_00BD4_bank1.inc"
};

/// Packed unarmed-walk rotations for player-model tracks 2..18.
static const AnimationPackedRotation _gUnused85UnarmedWalkAnimationBank4[151] __attribute__((section(".data"))) = {
#include "assets/unused_85_animation_00BD4_bank4.inc"
};

/// Unarmed-walk keyframes and loop/stop controls; durations count normal-rate frames.
static const AnimationRecord _gUnused85UnarmedWalkAnimationRecords[218] __attribute__((section(".data"))) = {
#include "assets/unused_85_animation_00BD4_records.inc"
};

/// Unarmed-walk track starts for parts 0..18 and the retained unused export halfword.
static const u16 _gUnused85UnarmedWalkAnimationIndices[20] __attribute__((section(".data"))) = {
#include "assets/unused_85_animation_00BD4_indices.inc"
};

/// Player forward-walk clip when no held-weapon task exists, for unused weapon index 6.
///
/// Native-bank set 19. Its 19 tracks borrow this package's embedded payloads.
static AnimationSet _gUnused85UnarmedWalkAnimation = {
    .records           = _gUnused85UnarmedWalkAnimationRecords,
    .trackStartIndices = _gUnused85UnarmedWalkAnimationIndices,
    .poseBanks         = {
        [UNUSED_85_ANIMATION_TRANSLATION_ROTATION_BANK] = _gUnused85UnarmedWalkAnimationBank1,
        [UNUSED_85_ANIMATION_PACKED_ROTATION_BANK]      = _gUnused85UnarmedWalkAnimationBank4,
    },
};

/// Forward-walk translation/rotation poses for player-model tracks 0 and 1.
static const AnimationPackedPose _gUnused85WalkAnimationBank1[16] __attribute__((section(".data"))) = {
#include "assets/unused_85_animation_01520_bank1.inc"
};

/// Packed forward-walk rotations for player-model tracks 2..18.
static const AnimationPackedRotation _gUnused85WalkAnimationBank4[221] __attribute__((section(".data"))) = {
#include "assets/unused_85_animation_01520_bank4.inc"
};

/// Forward-walk keyframes and loop/stop controls; durations count normal-rate frames.
static const AnimationRecord _gUnused85WalkAnimationRecords[306] __attribute__((section(".data"))) = {
#include "assets/unused_85_animation_01520_records.inc"
};

/// Forward-walk track starts for parts 0..18 and the retained unused export halfword.
static const u16 _gUnused85WalkAnimationIndices[20] __attribute__((section(".data"))) = {
#include "assets/unused_85_animation_01520_indices.inc"
};

/// Player forward-walk clip with a held weapon, for unused weapon index 6.
///
/// Native-bank set 2. Its 19 tracks borrow this package's embedded payloads.
static AnimationSet _gUnused85WalkAnimation = {
    .records           = _gUnused85WalkAnimationRecords,
    .trackStartIndices = _gUnused85WalkAnimationIndices,
    .poseBanks         = {
        [UNUSED_85_ANIMATION_TRANSLATION_ROTATION_BANK] = _gUnused85WalkAnimationBank1,
        [UNUSED_85_ANIMATION_PACKED_ROTATION_BANK]      = _gUnused85WalkAnimationBank4,
    },
};

/// Backward-walk translation/rotation poses for player-model tracks 0 and 1.
static const AnimationPackedPose _gUnused85BackwardWalkAnimationBank1[19] __attribute__((section(".data"))) = {
#include "assets/unused_85_animation_01DF8_bank1.inc"
};

/// Packed backward-walk rotations for player-model tracks 2..18.
static const AnimationPackedRotation _gUnused85BackwardWalkAnimationBank4[193] __attribute__((section(".data"))) = {
#include "assets/unused_85_animation_01DF8_bank4.inc"
};

/// Backward-walk keyframes and loop/stop controls; durations count normal-rate frames.
static const AnimationRecord _gUnused85BackwardWalkAnimationRecords[296] __attribute__((section(".data"))) = {
#include "assets/unused_85_animation_01DF8_records.inc"
};

/// Backward-walk track starts for parts 0..18 and the retained unused export halfword.
static const u16 _gUnused85BackwardWalkAnimationIndices[20] __attribute__((section(".data"))) = {
#include "assets/unused_85_animation_01DF8_indices.inc"
};

/// Player backward-walk clip for unused weapon index 6.
///
/// Native-bank set 3. Its 19 tracks borrow this package's embedded payloads.
static AnimationSet _gUnused85BackwardWalkAnimation = {
    .records           = _gUnused85BackwardWalkAnimationRecords,
    .trackStartIndices = _gUnused85BackwardWalkAnimationIndices,
    .poseBanks         = {
        [UNUSED_85_ANIMATION_TRANSLATION_ROTATION_BANK] = _gUnused85BackwardWalkAnimationBank1,
        [UNUSED_85_ANIMATION_PACKED_ROTATION_BANK]      = _gUnused85BackwardWalkAnimationBank4,
    },
};

/// Run translation/rotation poses for player-model tracks 0 and 1.
static const AnimationPackedPose _gUnused85RunAnimationBank1[13] __attribute__((section(".data"))) = {
#include "assets/unused_85_animation_0252C_bank1.inc"
};

/// Packed run rotations for player-model tracks 2..18.
static const AnimationPackedRotation _gUnused85RunAnimationBank4[167] __attribute__((section(".data"))) = {
#include "assets/unused_85_animation_0252C_bank4.inc"
};

/// Run keyframes and loop/stop controls; keyframe durations count normal-rate frames.
static const AnimationRecord _gUnused85RunAnimationRecords[235] __attribute__((section(".data"))) = {
#include "assets/unused_85_animation_0252C_records.inc"
};

/// Run track starts for parts 0..18, followed by the retained unused export halfword.
static const u16 _gUnused85RunAnimationIndices[20] __attribute__((section(".data"))) = {
#include "assets/unused_85_animation_0252C_indices.inc"
};

/// Player run clip for unused weapon index 6.
///
/// Native-bank set 4. Its 19 tracks borrow this package's embedded payloads.
static AnimationSet _gUnused85RunAnimation = {
    .records           = _gUnused85RunAnimationRecords,
    .trackStartIndices = _gUnused85RunAnimationIndices,
    .poseBanks         = {
        [UNUSED_85_ANIMATION_TRANSLATION_ROTATION_BANK] = _gUnused85RunAnimationBank1,
        [UNUSED_85_ANIMATION_PACKED_ROTATION_BANK]      = _gUnused85RunAnimationBank4,
    },
};

/// Left stationary-turn translation/rotation poses for player-model tracks 0 and 1.
static const AnimationPackedPose _gUnused85NegativeTurnAnimationBank1[19] __attribute__((section(".data"))) = {
#include "assets/unused_85_animation_02D8C_bank1.inc"
};

/// Packed left stationary-turn rotations for player-model tracks 2..18.
static const AnimationPackedRotation _gUnused85NegativeTurnAnimationBank4[169] __attribute__((section(".data"))) = {
#include "assets/unused_85_animation_02D8C_bank4.inc"
};

/// Left stationary-turn keyframes and loop/stop controls; durations count normal-rate frames.
static const AnimationRecord _gUnused85NegativeTurnAnimationRecords[290] __attribute__((section(".data"))) = {
#include "assets/unused_85_animation_02D8C_records.inc"
};

/// Left stationary-turn track starts for parts 0..18 and the retained export halfword.
static const u16 _gUnused85NegativeTurnAnimationIndices[20] __attribute__((section(".data"))) = {
#include "assets/unused_85_animation_02D8C_indices.inc"
};

/// Player looping left stationary-turn clip for unused weapon index 6.
///
/// Native-bank set 5, selected by a negative turn sign while stopped.
/// Its 19 tracks borrow this package's embedded payloads while it is loaded.
static AnimationSet _gUnused85NegativeTurnAnimation = {
    .records           = _gUnused85NegativeTurnAnimationRecords,
    .trackStartIndices = _gUnused85NegativeTurnAnimationIndices,
    .poseBanks         = {
        [UNUSED_85_ANIMATION_TRANSLATION_ROTATION_BANK] = _gUnused85NegativeTurnAnimationBank1,
        [UNUSED_85_ANIMATION_PACKED_ROTATION_BANK]      = _gUnused85NegativeTurnAnimationBank4,
    },
};

/// Right stationary-turn translation/rotation poses for player-model tracks 0 and 1.
static const AnimationPackedPose _gUnused85PositiveTurnAnimationBank1[19] __attribute__((section(".data"))) = {
#include "assets/unused_85_animation_035F0_bank1.inc"
};

/// Packed right stationary-turn rotations for player-model tracks 2..18.
static const AnimationPackedRotation _gUnused85PositiveTurnAnimationBank4[170] __attribute__((section(".data"))) = {
#include "assets/unused_85_animation_035F0_bank4.inc"
};

/// Right stationary-turn keyframes and loop/stop controls; durations count normal-rate frames.
static const AnimationRecord _gUnused85PositiveTurnAnimationRecords[290] __attribute__((section(".data"))) = {
#include "assets/unused_85_animation_035F0_records.inc"
};

/// Right stationary-turn track starts for parts 0..18 and the retained export halfword.
static const u16 _gUnused85PositiveTurnAnimationIndices[20] __attribute__((section(".data"))) = {
#include "assets/unused_85_animation_035F0_indices.inc"
};

/// Player looping right stationary-turn clip for unused weapon index 6.
///
/// Native-bank set 6, selected by a positive turn sign while stopped.
/// Its 19 tracks borrow this package's embedded payloads while it is loaded.
static AnimationSet _gUnused85PositiveTurnAnimation = {
    .records           = _gUnused85PositiveTurnAnimationRecords,
    .trackStartIndices = _gUnused85PositiveTurnAnimationIndices,
    .poseBanks         = {
        [UNUSED_85_ANIMATION_TRANSLATION_ROTATION_BANK] = _gUnused85PositiveTurnAnimationBank1,
        [UNUSED_85_ANIMATION_PACKED_ROTATION_BANK]      = _gUnused85PositiveTurnAnimationBank4,
    },
};

/// Item-use translation/rotation poses for player-model tracks 0 and 1.
static const AnimationPackedPose _gUnused85ItemUseAnimationBank1[3] __attribute__((section(".data"))) = {
#include "assets/unused_85_animation_03904_bank1.inc"
};

/// Packed item-use rotations for player-model tracks 2..18.
static const AnimationPackedRotation _gUnused85ItemUseAnimationBank4[69] __attribute__((section(".data"))) = {
#include "assets/unused_85_animation_03904_bank4.inc"
};

/// Item-use keyframes and stop controls; durations count normal-rate frames.
static const AnimationRecord _gUnused85ItemUseAnimationRecords[99] __attribute__((section(".data"))) = {
#include "assets/unused_85_animation_03904_records.inc"
};

/// Item-use track starts for parts 0..18 and the retained export halfword.
static const u16 _gUnused85ItemUseAnimationIndices[20] __attribute__((section(".data"))) = {
#include "assets/unused_85_animation_03904_indices.inc"
};

/// Player one-shot item-use clip for unused weapon index 6.
///
/// Native-bank set 40. Its 19 tracks borrow this package's embedded payloads
/// while it is loaded; completion resumes aim locomotion.
static AnimationSet _gUnused85ItemUseAnimation = {
    .records           = _gUnused85ItemUseAnimationRecords,
    .trackStartIndices = _gUnused85ItemUseAnimationIndices,
    .poseBanks         = {
        [UNUSED_85_ANIMATION_TRANSLATION_ROTATION_BANK] = _gUnused85ItemUseAnimationBank1,
        [UNUSED_85_ANIMATION_PACKED_ROTATION_BANK]      = _gUnused85ItemUseAnimationBank4,
    },
};

/// Stair-ascent translation/rotation poses for player-model tracks 0 and 1.
static const AnimationPackedPose _gUnused85StairAscentAnimationBank1[14] __attribute__((section(".data"))) = {
#include "assets/unused_85_animation_04060_bank1.inc"
};

/// Packed stair-ascent rotations for player-model tracks 2..18.
static const AnimationPackedRotation _gUnused85StairAscentAnimationBank4[156] __attribute__((section(".data"))) = {
#include "assets/unused_85_animation_04060_bank4.inc"
};

/// Stair-ascent keyframes, footstep cues and loop/stop controls; durations count normal-rate frames.
static const AnimationRecord _gUnused85StairAscentAnimationRecords[253] __attribute__((section(".data"))) = {
#include "assets/unused_85_animation_04060_records.inc"
};

/// Stair-ascent track starts for parts 0..18 and the retained export halfword.
static const u16 _gUnused85StairAscentAnimationIndices[20] __attribute__((section(".data"))) = {
#include "assets/unused_85_animation_04060_indices.inc"
};

/// Player looping stair-ascent clip for unused weapon index 6.
///
/// Native-bank set 36; footstep cues pace scripted ascent strides.
/// Its 19 tracks borrow this package's embedded payloads while it is loaded.
static AnimationSet _gUnused85StairAscentAnimation = {
    .records           = _gUnused85StairAscentAnimationRecords,
    .trackStartIndices = _gUnused85StairAscentAnimationIndices,
    .poseBanks         = {
        [UNUSED_85_ANIMATION_TRANSLATION_ROTATION_BANK] = _gUnused85StairAscentAnimationBank1,
        [UNUSED_85_ANIMATION_PACKED_ROTATION_BANK]      = _gUnused85StairAscentAnimationBank4,
    },
};

/// Stair-descent translation/rotation poses for player-model tracks 0 and 1.
static const AnimationPackedPose _gUnused85StairDescentAnimationBank1[16] __attribute__((section(".data"))) = {
#include "assets/unused_85_animation_047E8_bank1.inc"
};

/// Packed stair-descent rotations for player-model tracks 2..18.
static const AnimationPackedRotation _gUnused85StairDescentAnimationBank4[167] __attribute__((section(".data"))) = {
#include "assets/unused_85_animation_047E8_bank4.inc"
};

/// Stair-descent keyframes, footstep cues and loop/stop controls; durations count normal-rate frames.
static const AnimationRecord _gUnused85StairDescentAnimationRecords[247] __attribute__((section(".data"))) = {
#include "assets/unused_85_animation_047E8_records.inc"
};

/// Stair-descent track starts for parts 0..18 and the retained export halfword.
static const u16 _gUnused85StairDescentAnimationIndices[20] __attribute__((section(".data"))) = {
#include "assets/unused_85_animation_047E8_indices.inc"
};

/// Player looping stair-descent clip for unused weapon index 6.
///
/// Native-bank set 37; footstep cues pace scripted descent strides.
/// Its 19 tracks borrow this package's embedded payloads while it is loaded.
static AnimationSet _gUnused85StairDescentAnimation = {
    .records           = _gUnused85StairDescentAnimationRecords,
    .trackStartIndices = _gUnused85StairDescentAnimationIndices,
    .poseBanks         = {
        [UNUSED_85_ANIMATION_TRANSLATION_ROTATION_BANK] = _gUnused85StairDescentAnimationBank1,
        [UNUSED_85_ANIMATION_PACKED_ROTATION_BANK]      = _gUnused85StairDescentAnimationBank4,
    },
};

/// Odd-count stair-ascent finish translation/rotation poses for player-model tracks 0 and 1.
static const AnimationPackedPose _gUnused85StairAscentFinishOddAnimationBank1[6] __attribute__((section(".data"))) = {
#include "assets/unused_85_animation_04ABC_bank1.inc"
};

/// Packed odd-count stair-ascent finish rotations for player-model tracks 2..18.
static const AnimationPackedRotation _gUnused85StairAscentFinishOddAnimationBank4[52] __attribute__((section(".data"))) = {
#include "assets/unused_85_animation_04ABC_bank4.inc"
};

/// Odd-count stair-ascent finish keyframes, footstep cue and stop controls; durations count normal-rate frames.
static const AnimationRecord _gUnused85StairAscentFinishOddAnimationRecords[91] __attribute__((section(".data"))) = {
#include "assets/unused_85_animation_04ABC_records.inc"
};

/// Odd-count stair-ascent finish track starts for parts 0..18 and the retained export halfword.
static const u16 _gUnused85StairAscentFinishOddAnimationIndices[20] __attribute__((section(".data"))) = {
#include "assets/unused_85_animation_04ABC_indices.inc"
};

/// Player one-shot stair-ascent finish clip for an odd original step count, for unused weapon index 6.
///
/// Native-bank set 39; the stair controller retains the initial count's parity.
/// Its 19 tracks borrow this package's embedded payloads while it is loaded.
static AnimationSet _gUnused85StairAscentFinishOddAnimation = {
    .records           = _gUnused85StairAscentFinishOddAnimationRecords,
    .trackStartIndices = _gUnused85StairAscentFinishOddAnimationIndices,
    .poseBanks         = {
        [UNUSED_85_ANIMATION_TRANSLATION_ROTATION_BANK] = _gUnused85StairAscentFinishOddAnimationBank1,
        [UNUSED_85_ANIMATION_PACKED_ROTATION_BANK]      = _gUnused85StairAscentFinishOddAnimationBank4,
    },
};

/// Even-count stair-ascent finish translation/rotation poses for player tracks 0 and 1.
static const AnimationPackedPose _gUnused85StairAscentFinishEvenAnimationBank1[7] __attribute__((section(".data"))) = {
#include "assets/unused_85_animation_04E4C_bank1.inc"
};

/// Packed even-count stair-ascent finish rotations for player tracks 2..18.
static const AnimationPackedRotation _gUnused85StairAscentFinishEvenAnimationBank4[73] __attribute__((section(".data"))) = {
#include "assets/unused_85_animation_04E4C_bank4.inc"
};

/// Even-count stair-ascent finish keyframes, footstep cue and final-pose stops.
///
/// Keyframe durations count normal-rate frames; the cue belongs to track 1.
static const AnimationRecord _gUnused85StairAscentFinishEvenAnimationRecords[114] __attribute__((section(".data"))) = {
#include "assets/unused_85_animation_04E4C_records.inc"
};

/// Even-count stair-ascent finish track starts for parts 0..18 and the retained export halfword.
static const u16 _gUnused85StairAscentFinishEvenAnimationIndices[20] __attribute__((section(".data"))) = {
#include "assets/unused_85_animation_04E4C_indices.inc"
};

/// Player one-shot stair-ascent finish clip for an even original step count, for unused weapon index 6.
///
/// Native-bank set 38; the stair controller retains the initial count's parity.
/// Its 19 tracks borrow this package's embedded payloads while it is loaded.
static AnimationSet _gUnused85StairAscentFinishEvenAnimation = {
    .records           = _gUnused85StairAscentFinishEvenAnimationRecords,
    .trackStartIndices = _gUnused85StairAscentFinishEvenAnimationIndices,
    .poseBanks         = {
        [UNUSED_85_ANIMATION_TRANSLATION_ROTATION_BANK] = _gUnused85StairAscentFinishEvenAnimationBank1,
        [UNUSED_85_ANIMATION_PACKED_ROTATION_BANK]      = _gUnused85StairAscentFinishEvenAnimationBank4,
    },
};

/// Family-0 PE release translation/rotation poses for player tracks 0 and 1.
static const AnimationPackedPose _gUnused85PeReleaseFamily0AnimationBank1[9] __attribute__((section(".data"))) = {
#include "assets/unused_85_animation_052D4_bank1.inc"
};

/// Packed family-0 PE release rotations for player tracks 2..18.
static const AnimationPackedRotation _gUnused85PeReleaseFamily0AnimationBank4[104] __attribute__((section(".data"))) = {
#include "assets/unused_85_animation_052D4_bank4.inc"
};

/// Family-0 PE release keyframes and final-pose stops; durations count normal-rate frames.
static const AnimationRecord _gUnused85PeReleaseFamily0AnimationRecords[139] __attribute__((section(".data"))) = {
#include "assets/unused_85_animation_052D4_records.inc"
};

/// Family-0 PE release track starts for parts 0..18 and the retained export halfword.
static const u16 _gUnused85PeReleaseFamily0AnimationIndices[20] __attribute__((section(".data"))) = {
#include "assets/unused_85_animation_052D4_indices.inc"
};

/// Player Parasite Energy family-0 release clip for unused weapon index 6.
///
/// Native-bank set 26, played once before charging while the attachment countdown runs.
/// Its 19 tracks borrow this package's embedded payloads while it is loaded.
static AnimationSet _gUnused85PeReleaseFamily0Animation = {
    .records           = _gUnused85PeReleaseFamily0AnimationRecords,
    .trackStartIndices = _gUnused85PeReleaseFamily0AnimationIndices,
    .poseBanks         = {
        [UNUSED_85_ANIMATION_TRANSLATION_ROTATION_BANK] = _gUnused85PeReleaseFamily0AnimationBank1,
        [UNUSED_85_ANIMATION_PACKED_ROTATION_BANK]      = _gUnused85PeReleaseFamily0AnimationBank4,
    },
};

/// Family-0 PE charge translation/rotation poses for player tracks 0 and 1.
static const AnimationPackedPose _gUnused85PeChargeFamily0AnimationBank1[3] __attribute__((section(".data"))) = {
#include "assets/unused_85_animation_054D0_bank1.inc"
};

/// Packed family-0 PE charge rotations for player tracks 2..18.
static const AnimationPackedRotation _gUnused85PeChargeFamily0AnimationBank4[22] __attribute__((section(".data"))) = {
#include "assets/unused_85_animation_054D0_bank4.inc"
};

/// Family-0 PE charge keyframes, track loops and retained stop controls.
///
/// Each keyframe lasts one normal-rate frame; every track jumps back to its start.
static const AnimationRecord _gUnused85PeChargeFamily0AnimationRecords[76] __attribute__((section(".data"))) = {
#include "assets/unused_85_animation_054D0_records.inc"
};

/// Family-0 PE charge track starts for parts 0..18 and the retained export halfword.
static const u16 _gUnused85PeChargeFamily0AnimationIndices[20] __attribute__((section(".data"))) = {
#include "assets/unused_85_animation_054D0_indices.inc"
};

/// Player Parasite Energy family-0 charge loop for unused weapon index 6.
///
/// Native-bank set 27, repeated until the attachment countdown finishes or is cancelled.
/// Its 19 tracks borrow this package's embedded payloads while it is loaded.
static AnimationSet _gUnused85PeChargeFamily0Animation = {
    .records           = _gUnused85PeChargeFamily0AnimationRecords,
    .trackStartIndices = _gUnused85PeChargeFamily0AnimationIndices,
    .poseBanks         = {
        [UNUSED_85_ANIMATION_TRANSLATION_ROTATION_BANK] = _gUnused85PeChargeFamily0AnimationBank1,
        [UNUSED_85_ANIMATION_PACKED_ROTATION_BANK]      = _gUnused85PeChargeFamily0AnimationBank4,
    },
};

/// Family-0 PE cast translation/rotation poses for player tracks 0 and 1.
static const AnimationPackedPose _gUnused85PeCastFamily0AnimationBank1[6] __attribute__((section(".data"))) = {
#include "assets/unused_85_animation_057A8_bank1.inc"
};

/// Packed family-0 PE cast rotations for player tracks 2..18.
static const AnimationPackedRotation _gUnused85PeCastFamily0AnimationBank4[57] __attribute__((section(".data"))) = {
#include "assets/unused_85_animation_057A8_bank4.inc"
};

/// Family-0 PE cast keyframes and final-pose stops; durations count normal-rate frames.
static const AnimationRecord _gUnused85PeCastFamily0AnimationRecords[87] __attribute__((section(".data"))) = {
#include "assets/unused_85_animation_057A8_records.inc"
};

/// Family-0 PE cast track starts for parts 0..18 and the retained export halfword.
static const u16 _gUnused85PeCastFamily0AnimationIndices[20] __attribute__((section(".data"))) = {
#include "assets/unused_85_animation_057A8_indices.inc"
};

/// Player Parasite Energy family-0 cast clip for unused weapon index 6.
///
/// Native-bank set 28, played once after the attachment countdown finishes.
/// Its 19 tracks borrow this package's embedded payloads while it is loaded.
static AnimationSet _gUnused85PeCastFamily0Animation = {
    .records           = _gUnused85PeCastFamily0AnimationRecords,
    .trackStartIndices = _gUnused85PeCastFamily0AnimationIndices,
    .poseBanks         = {
        [UNUSED_85_ANIMATION_TRANSLATION_ROTATION_BANK] = _gUnused85PeCastFamily0AnimationBank1,
        [UNUSED_85_ANIMATION_PACKED_ROTATION_BANK]      = _gUnused85PeCastFamily0AnimationBank4,
    },
};

/// Family-1 PE release translation/rotation poses for player tracks 0 and 1.
static const AnimationPackedPose _gUnused85PeReleaseFamily1AnimationBank1[4] __attribute__((section(".data"))) = {
#include "assets/unused_85_animation_05A4C_bank1.inc"
};

/// Packed family-1 PE release rotations for player tracks 2..18.
static const AnimationPackedRotation _gUnused85PeReleaseFamily1AnimationBank4[55] __attribute__((section(".data"))) = {
#include "assets/unused_85_animation_05A4C_bank4.inc"
};

/// Family-1 PE release keyframes and final-pose stops; durations count normal-rate frames.
static const AnimationRecord _gUnused85PeReleaseFamily1AnimationRecords[82] __attribute__((section(".data"))) = {
#include "assets/unused_85_animation_05A4C_records.inc"
};

/// Family-1 PE release track starts for parts 0..18 and the retained export halfword.
static const u16 _gUnused85PeReleaseFamily1AnimationIndices[20] __attribute__((section(".data"))) = {
#include "assets/unused_85_animation_05A4C_indices.inc"
};

/// Player Parasite Energy family-1 release clip for unused weapon index 6.
///
/// Native-bank set 29, played once before charging while the attachment countdown runs.
/// Its 19 tracks borrow this package's embedded payloads while it is loaded.
static AnimationSet _gUnused85PeReleaseFamily1Animation = {
    .records           = _gUnused85PeReleaseFamily1AnimationRecords,
    .trackStartIndices = _gUnused85PeReleaseFamily1AnimationIndices,
    .poseBanks         = {
        [UNUSED_85_ANIMATION_TRANSLATION_ROTATION_BANK] = _gUnused85PeReleaseFamily1AnimationBank1,
        [UNUSED_85_ANIMATION_PACKED_ROTATION_BANK]      = _gUnused85PeReleaseFamily1AnimationBank4,
    },
};

/// Family-1 PE charge translation/rotation poses for player tracks 0 and 1.
static const AnimationPackedPose _gUnused85PeChargeFamily1AnimationBank1[3] __attribute__((section(".data"))) = {
#include "assets/unused_85_animation_05C4C_bank1.inc"
};

/// Packed family-1 PE charge rotations for player tracks 2..18.
static const AnimationPackedRotation _gUnused85PeChargeFamily1AnimationBank4[23] __attribute__((section(".data"))) = {
#include "assets/unused_85_animation_05C4C_bank4.inc"
};

/// Family-1 PE charge keyframes, track loops and retained stop controls.
///
/// Each keyframe lasts one normal-rate frame; every track jumps back to its start.
static const AnimationRecord _gUnused85PeChargeFamily1AnimationRecords[76] __attribute__((section(".data"))) = {
#include "assets/unused_85_animation_05C4C_records.inc"
};

/// Family-1 PE charge track starts for parts 0..18 and the retained export halfword.
static const u16 _gUnused85PeChargeFamily1AnimationIndices[20] __attribute__((section(".data"))) = {
#include "assets/unused_85_animation_05C4C_indices.inc"
};

/// Player Parasite Energy family-1 charge loop for unused weapon index 6.
///
/// Native-bank set 30, repeated until the attachment countdown finishes or is cancelled.
/// Its 19 tracks borrow this package's embedded payloads while it is loaded.
static AnimationSet _gUnused85PeChargeFamily1Animation = {
    .records           = _gUnused85PeChargeFamily1AnimationRecords,
    .trackStartIndices = _gUnused85PeChargeFamily1AnimationIndices,
    .poseBanks         = {
        [UNUSED_85_ANIMATION_TRANSLATION_ROTATION_BANK] = _gUnused85PeChargeFamily1AnimationBank1,
        [UNUSED_85_ANIMATION_PACKED_ROTATION_BANK]      = _gUnused85PeChargeFamily1AnimationBank4,
    },
};

/// Family-1 PE cast translation/rotation poses for player-model tracks 0 and 1.
///
/// Translations use model units; Euler angles use 4096 units per turn.
static const AnimationPackedPose _gUnused85PeCastFamily1AnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/unused_85_animation_05FA0_bank1.inc"
};

/// Family-1 PE cast rotations for player-model tracks 2..18, in eight-angle-unit steps.
static const AnimationPackedRotation _gUnused85PeCastFamily1AnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/unused_85_animation_05FA0_bank4.inc"
};

/// Family-1 PE cast keyframes and stop controls; durations count normal-rate frames.
static const AnimationRecord _gUnused85PeCastFamily1AnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/unused_85_animation_05FA0_records.inc"
};

/// Family-1 PE cast absolute record starts for player-model parts 0..18.
///
/// The twentieth halfword is retained export data, not another track.
static const u16 _gUnused85PeCastFamily1AnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/unused_85_animation_05FA0_indices.inc"
};

/// Player Parasite Energy family-1 cast clip for unused weapon index 6.
///
/// Native-bank set 31, started when the family-1 charge countdown finishes.
/// All 19 tracks stop at their final poses and borrow the loaded package's data.
static AnimationSet _gUnused85PeCastFamily1Animation = {
    .records           = _gUnused85PeCastFamily1AnimationRecords,
    .trackStartIndices = _gUnused85PeCastFamily1AnimationIndices,
    .poseBanks         = {
        [UNUSED_85_ANIMATION_TRANSLATION_ROTATION_BANK] = _gUnused85PeCastFamily1AnimationBank1,
        [UNUSED_85_ANIMATION_PACKED_ROTATION_BANK]      = _gUnused85PeCastFamily1AnimationBank4,
    },
};

/// Model-part-4 hit-reaction translation/rotation poses for player-model tracks 0 and 1.
///
/// Translations use model units; Euler angles use 4096 units per turn.
static const AnimationPackedPose _gUnused85Part4HitReactionAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/unused_85_animation_06254_bank1.inc"
};

/// Model-part-4 hit-reaction rotations for player-model tracks 2..18, in eight-angle-unit steps.
static const AnimationPackedRotation _gUnused85Part4HitReactionAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/unused_85_animation_06254_bank4.inc"
};

/// Model-part-4 hit-reaction keyframes and stop controls; durations count normal-rate frames.
static const AnimationRecord _gUnused85Part4HitReactionAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/unused_85_animation_06254_records.inc"
};

/// Model-part-4 hit-reaction absolute record starts for player-model parts 0..18.
///
/// The twentieth halfword is retained export data, not another track.
static const u16 _gUnused85Part4HitReactionAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/unused_85_animation_06254_indices.inc"
};

/// Player hit-reaction clip for contacts on model part 4, for unused weapon index 6.
///
/// Native-bank set 16, selected by hit region 1 with a three-frame blend.
/// All 19 tracks stop at their final poses and borrow the loaded package's data.
static AnimationSet _gUnused85Part4HitReactionAnimation = {
    .records           = _gUnused85Part4HitReactionAnimationRecords,
    .trackStartIndices = _gUnused85Part4HitReactionAnimationIndices,
    .poseBanks         = {
        [UNUSED_85_ANIMATION_TRANSLATION_ROTATION_BANK] = _gUnused85Part4HitReactionAnimationBank1,
        [UNUSED_85_ANIMATION_PACKED_ROTATION_BANK]      = _gUnused85Part4HitReactionAnimationBank4,
    },
};

/// Other-body hit-reaction translation/rotation poses for player-model tracks 0 and 1.
///
/// Translations use model units; Euler angles use 4096 units per turn.
static const AnimationPackedPose _gUnused85OtherBodyHitReactionAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/unused_85_animation_06574_bank1.inc"
};

/// Other-body hit-reaction rotations for player-model tracks 2..18, in eight-angle-unit steps.
static const AnimationPackedRotation _gUnused85OtherBodyHitReactionAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/unused_85_animation_06574_bank4.inc"
};

/// Other-body hit-reaction keyframes and stop controls; durations count normal-rate frames.
static const AnimationRecord _gUnused85OtherBodyHitReactionAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/unused_85_animation_06574_records.inc"
};

/// Other-body hit-reaction absolute record starts for player-model parts 0..18.
///
/// The twentieth halfword is retained export data, not another track.
static const u16 _gUnused85OtherBodyHitReactionAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/unused_85_animation_06574_indices.inc"
};

/// Player hit-reaction clip for contacts outside model part 4, for unused weapon index 6.
///
/// Native-bank set 17, selected for the other-body hit region with a three-frame blend.
/// All 19 tracks stop at their final poses and borrow the loaded package's data.
static AnimationSet _gUnused85OtherBodyHitReactionAnimation = {
    .records           = _gUnused85OtherBodyHitReactionAnimationRecords,
    .trackStartIndices = _gUnused85OtherBodyHitReactionAnimationIndices,
    .poseBanks         = {
        [UNUSED_85_ANIMATION_TRANSLATION_ROTATION_BANK] = _gUnused85OtherBodyHitReactionAnimationBank1,
        [UNUSED_85_ANIMATION_PACKED_ROTATION_BANK]      = _gUnused85OtherBodyHitReactionAnimationBank4,
    },
};

/// Stopped-pose translation/rotation poses for player-model tracks 0 and 1.
///
/// Translations use model units; Euler angles use 4096 units per turn.
static const AnimationPackedPose _gUnused85StoppedPoseAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/unused_85_animation_06D38_bank1.inc"
};

/// Stopped-pose rotations for player-model tracks 2..18, in eight-angle-unit steps.
static const AnimationPackedRotation _gUnused85StoppedPoseAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/unused_85_animation_06D38_bank4.inc"
};

/// Stopped-pose keyframes and stop controls; durations count normal-rate frames.
static const AnimationRecord _gUnused85StoppedPoseAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/unused_85_animation_06D38_records.inc"
};

/// Stopped-pose absolute record starts for player-model parts 0..18.
///
/// The twentieth halfword is retained export data, not another track.
static const u16 _gUnused85StoppedPoseAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/unused_85_animation_06D38_indices.inc"
};

/// Player stopped-pose clip for unused weapon index 6.
///
/// Native-bank set 18, selected when damage mode stops movement and turning.
/// All 19 tracks stop at their final poses and borrow the loaded package's data.
static AnimationSet _gUnused85StoppedPoseAnimation = {
    .records           = _gUnused85StoppedPoseAnimationRecords,
    .trackStartIndices = _gUnused85StoppedPoseAnimationIndices,
    .poseBanks         = {
        [UNUSED_85_ANIMATION_TRANSLATION_ROTATION_BANK] = _gUnused85StoppedPoseAnimationBank1,
        [UNUSED_85_ANIMATION_PACKED_ROTATION_BANK]      = _gUnused85StoppedPoseAnimationBank4,
    },
};

/// Healthy-idle translation/rotation poses for player-model tracks 0 and 1.
///
/// Translations use model units; Euler angles use 4096 units per turn.
static const AnimationPackedPose _gUnused85HealthyIdleAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/unused_85_animation_07FD0_bank1.inc"
};

/// Healthy-idle rotations for player-model tracks 2..18, in eight-angle-unit steps.
static const AnimationPackedRotation _gUnused85HealthyIdleAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/unused_85_animation_07FD0_bank4.inc"
};

/// Healthy-idle keyframes and loop/stop controls; durations count normal-rate frames.
///
/// Forward playback takes each track's backward jump before its retained stop record.
static const AnimationRecord _gUnused85HealthyIdleAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/unused_85_animation_07FD0_records.inc"
};

/// Healthy-idle absolute record starts for player-model parts 0..18.
///
/// The twentieth halfword is retained export data, not another track.
static const u16 _gUnused85HealthyIdleAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/unused_85_animation_07FD0_indices.inc"
};

/// Player idle loop above half maximum HP, for unused weapon index 6.
///
/// Native-bank set 23, blended in after 300 direction-free ticks.
/// All 19 tracks enter repeating tails and borrow the loaded package's data.
static AnimationSet _gUnused85HealthyIdleAnimation = {
    .records           = _gUnused85HealthyIdleAnimationRecords,
    .trackStartIndices = _gUnused85HealthyIdleAnimationIndices,
    .poseBanks         = {
        [UNUSED_85_ANIMATION_TRANSLATION_ROTATION_BANK] = _gUnused85HealthyIdleAnimationBank1,
        [UNUSED_85_ANIMATION_PACKED_ROTATION_BANK]      = _gUnused85HealthyIdleAnimationBank4,
    },
};

/// Critical-health idle translation/rotation poses for player-model tracks 0 and 1.
///
/// Translations use model units; Euler angles use 4096 units per turn.
static const AnimationPackedPose _gUnused85CriticalHealthIdleAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/unused_85_animation_08B48_bank1.inc"
};

/// Critical-health idle rotations for player-model tracks 2..18, in eight-angle-unit steps.
static const AnimationPackedRotation _gUnused85CriticalHealthIdleAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/unused_85_animation_08B48_bank4.inc"
};

/// Critical-health idle keyframes and loop/stop controls; durations count normal-rate frames.
///
/// Forward playback takes each track's backward jump before its retained stop record.
static const AnimationRecord _gUnused85CriticalHealthIdleAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/unused_85_animation_08B48_records.inc"
};

/// Critical-health idle absolute record starts for player-model parts 0..18.
///
/// The twentieth halfword is retained export data, not another track.
static const u16 _gUnused85CriticalHealthIdleAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/unused_85_animation_08B48_indices.inc"
};

/// Player idle loop at or below one quarter maximum HP, for unused weapon index 6.
///
/// Native-bank set 25, blended in after 300 direction-free ticks.
/// Paralysis also selects this set with a six-frame blend of child slots 1..18.
/// All 19 tracks enter repeating tails and borrow the loaded package's data.
static AnimationSet _gUnused85CriticalHealthIdleAnimation = {
    .records           = _gUnused85CriticalHealthIdleAnimationRecords,
    .trackStartIndices = _gUnused85CriticalHealthIdleAnimationIndices,
    .poseBanks         = {
        [UNUSED_85_ANIMATION_TRANSLATION_ROTATION_BANK] = _gUnused85CriticalHealthIdleAnimationBank1,
        [UNUSED_85_ANIMATION_PACKED_ROTATION_BANK]      = _gUnused85CriticalHealthIdleAnimationBank4,
    },
};

/// Medium-health idle translation/rotation poses for player-model tracks 0 and 1.
///
/// Translations use model units; Euler angles use 4096 units per turn.
static const AnimationPackedPose _gUnused85MediumHealthIdleAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/unused_85_animation_09264_bank1.inc"
};

/// Medium-health idle rotations for player-model tracks 2..18, in eight-angle-unit steps.
static const AnimationPackedRotation _gUnused85MediumHealthIdleAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/unused_85_animation_09264_bank4.inc"
};

/// Medium-health idle keyframes and loop/stop controls; durations count normal-rate frames.
///
/// Forward playback takes each track's backward jump before its retained stop record.
static const AnimationRecord _gUnused85MediumHealthIdleAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/unused_85_animation_09264_records.inc"
};

/// Medium-health idle absolute record starts for player-model parts 0..18.
///
/// The twentieth halfword is retained export data, not another track.
static const u16 _gUnused85MediumHealthIdleAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/unused_85_animation_09264_indices.inc"
};

/// Player idle loop above one quarter and at or below one half maximum HP, for unused weapon index 6.
///
/// Native-bank set 24, blended in after 300 direction-free ticks.
/// All 19 tracks enter repeating tails and borrow the loaded package's data.
static AnimationSet _gUnused85MediumHealthIdleAnimation = {
    .records           = _gUnused85MediumHealthIdleAnimationRecords,
    .trackStartIndices = _gUnused85MediumHealthIdleAnimationIndices,
    .poseBanks         = {
        [UNUSED_85_ANIMATION_TRANSLATION_ROTATION_BANK] = _gUnused85MediumHealthIdleAnimationBank1,
        [UNUSED_85_ANIMATION_PACKED_ROTATION_BANK]      = _gUnused85MediumHealthIdleAnimationBank4,
    },
};

/// Scripted-entry translation/rotation poses for player-model tracks 0 and 1.
///
/// Translations use model units; Euler angles use 4096 units per turn.
static const AnimationPackedPose _gUnused85ScriptedPresentationEnterAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/unused_85_animation_096E4_bank1.inc"
};

/// Scripted-entry rotations for player-model tracks 2..18, in eight-angle-unit steps.
static const AnimationPackedRotation _gUnused85ScriptedPresentationEnterAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/unused_85_animation_096E4_bank4.inc"
};

/// Scripted-entry keyframes and stop controls; durations count normal-rate frames.
static const AnimationRecord _gUnused85ScriptedPresentationEnterAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/unused_85_animation_096E4_records.inc"
};

/// Scripted-entry absolute record starts for player-model parts 0..18.
///
/// The twentieth halfword is retained export data, not another track.
static const u16 _gUnused85ScriptedPresentationEnterAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/unused_85_animation_096E4_indices.inc"
};

/// Player presentation entering a scripted event, for unused weapon index 6.
///
/// Native-bank set 32; set 34 selects the same clip.
/// All 19 tracks stop at their final pose and borrow the loaded package's data.
static AnimationSet _gUnused85ScriptedPresentationEnterAnimation = {
    .records           = _gUnused85ScriptedPresentationEnterAnimationRecords,
    .trackStartIndices = _gUnused85ScriptedPresentationEnterAnimationIndices,
    .poseBanks         = {
        [UNUSED_85_ANIMATION_TRANSLATION_ROTATION_BANK] = _gUnused85ScriptedPresentationEnterAnimationBank1,
        [UNUSED_85_ANIMATION_PACKED_ROTATION_BANK]      = _gUnused85ScriptedPresentationEnterAnimationBank4,
    },
};

/// Scripted-exit translation/rotation poses for player-model tracks 0 and 1.
///
/// Translations use model units; Euler angles use 4096 units per turn.
static const AnimationPackedPose _gUnused85ScriptedPresentationLeaveAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/unused_85_animation_098BC_bank1.inc"
};

/// Scripted-exit rotations for player-model tracks 2..18, in eight-angle-unit steps.
static const AnimationPackedRotation _gUnused85ScriptedPresentationLeaveAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/unused_85_animation_098BC_bank4.inc"
};

/// Scripted-exit keyframes and stop controls; durations count normal-rate frames.
static const AnimationRecord _gUnused85ScriptedPresentationLeaveAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/unused_85_animation_098BC_records.inc"
};

/// Scripted-exit absolute record starts for player-model parts 0..18.
///
/// The twentieth halfword is retained export data, not another track.
static const u16 _gUnused85ScriptedPresentationLeaveAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/unused_85_animation_098BC_indices.inc"
};

/// Player presentation leaving a scripted event, for unused weapon index 6.
///
/// Native-bank set 33; set 35 selects the same clip.
/// All 19 tracks stop at their final pose and borrow the loaded package's data.
static AnimationSet _gUnused85ScriptedPresentationLeaveAnimation = {
    .records           = _gUnused85ScriptedPresentationLeaveAnimationRecords,
    .trackStartIndices = _gUnused85ScriptedPresentationLeaveAnimationIndices,
    .poseBanks         = {
        [UNUSED_85_ANIMATION_TRANSLATION_ROTATION_BANK] = _gUnused85ScriptedPresentationLeaveAnimationBank1,
        [UNUSED_85_ANIMATION_PACKED_ROTATION_BANK]      = _gUnused85ScriptedPresentationLeaveAnimationBank4,
    },
};

/// PE-family-2 release translation/rotation poses for player-model tracks 0 and 1.
///
/// Translations use model units; Euler angles use 4096 units per turn.
static const AnimationPackedPose _gUnused85PeReleaseFamily2AnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/unused_85_animation_09E20_bank1.inc"
};

/// PE-family-2 release rotations for player-model tracks 2..18, in eight-angle-unit steps.
static const AnimationPackedRotation _gUnused85PeReleaseFamily2AnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/unused_85_animation_09E20_bank4.inc"
};

/// PE-family-2 release keyframes and stop controls; durations count normal-rate frames.
static const AnimationRecord _gUnused85PeReleaseFamily2AnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/unused_85_animation_09E20_records.inc"
};

/// PE-family-2 release absolute record starts for player-model parts 0..18.
///
/// The twentieth halfword is retained export data, not another track.
static const u16 _gUnused85PeReleaseFamily2AnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/unused_85_animation_09E20_indices.inc"
};

/// Player Parasite Energy release clip for family 2 and unused weapon index 6.
///
/// Native-bank set 42, played before the charge loop with a six-frame blend.
/// Family 2 serves spell IDs below 601 whose decimal tens digit is 3.
/// All 19 tracks stop at their final pose and borrow the loaded package's data.
static AnimationSet _gUnused85PeReleaseFamily2Animation = {
    .records           = _gUnused85PeReleaseFamily2AnimationRecords,
    .trackStartIndices = _gUnused85PeReleaseFamily2AnimationIndices,
    .poseBanks         = {
        [UNUSED_85_ANIMATION_TRANSLATION_ROTATION_BANK] = _gUnused85PeReleaseFamily2AnimationBank1,
        [UNUSED_85_ANIMATION_PACKED_ROTATION_BANK]      = _gUnused85PeReleaseFamily2AnimationBank4,
    },
};

/// PE-family-2 charge translation/rotation poses for player-model tracks 0 and 1.
///
/// Translations use model units; Euler angles use 4096 units per turn.
static const AnimationPackedPose _gUnused85PeChargeFamily2AnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/unused_85_animation_0A014_bank1.inc"
};

/// PE-family-2 charge rotations for player-model tracks 2..18, in eight-angle-unit steps.
static const AnimationPackedRotation _gUnused85PeChargeFamily2AnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/unused_85_animation_0A014_bank4.inc"
};

/// PE-family-2 charge keyframes and loop/stop controls; durations count normal-rate frames.
///
/// Forward playback takes each track's backward jump before its retained stop record.
static const AnimationRecord _gUnused85PeChargeFamily2AnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/unused_85_animation_0A014_records.inc"
};

/// PE-family-2 charge absolute record starts for player-model parts 0..18.
///
/// The twentieth halfword is retained export data, not another track.
static const u16 _gUnused85PeChargeFamily2AnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/unused_85_animation_0A014_indices.inc"
};

/// Player Parasite Energy charge loop for family 2 and unused weapon index 6.
///
/// Native-bank set 43, reset after release and played until the attachment duration reaches zero.
/// Family 2 serves spell IDs below 601 whose decimal tens digit is 3.
/// All 19 tracks loop and borrow the loaded package's data.
static AnimationSet _gUnused85PeChargeFamily2Animation = {
    .records           = _gUnused85PeChargeFamily2AnimationRecords,
    .trackStartIndices = _gUnused85PeChargeFamily2AnimationIndices,
    .poseBanks         = {
        [UNUSED_85_ANIMATION_TRANSLATION_ROTATION_BANK] = _gUnused85PeChargeFamily2AnimationBank1,
        [UNUSED_85_ANIMATION_PACKED_ROTATION_BANK]      = _gUnused85PeChargeFamily2AnimationBank4,
    },
};

/// PE-family-2 cast translation/rotation poses for player-model tracks 0 and 1.
///
/// Translations use model units; Euler angles use 4096 units per turn.
static const AnimationPackedPose _gUnused85PeCastFamily2AnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/unused_85_animation_0A494_bank1.inc"
};

/// PE-family-2 cast rotations for player-model tracks 2..18, in eight-angle-unit steps.
static const AnimationPackedRotation _gUnused85PeCastFamily2AnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/unused_85_animation_0A494_bank4.inc"
};

/// PE-family-2 cast keyframes and stop controls; durations count normal-rate frames.
static const AnimationRecord _gUnused85PeCastFamily2AnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/unused_85_animation_0A494_records.inc"
};

/// PE-family-2 cast absolute record starts for player-model parts 0..18.
///
/// The twentieth halfword is retained export data, not another track.
static const u16 _gUnused85PeCastFamily2AnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/unused_85_animation_0A494_indices.inc"
};

/// Player Parasite Energy cast clip for family 2 and unused weapon index 6.
///
/// Native-bank set 44, reset when the charge countdown reaches zero.
/// Family 2 serves spell IDs below 601 whose decimal tens digit is 3.
/// All 19 tracks stop at their final pose and borrow the loaded package's data.
static AnimationSet _gUnused85PeCastFamily2Animation = {
    .records           = _gUnused85PeCastFamily2AnimationRecords,
    .trackStartIndices = _gUnused85PeCastFamily2AnimationIndices,
    .poseBanks         = {
        [UNUSED_85_ANIMATION_TRANSLATION_ROTATION_BANK] = _gUnused85PeCastFamily2AnimationBank1,
        [UNUSED_85_ANIMATION_PACKED_ROTATION_BANK]      = _gUnused85PeCastFamily2AnimationBank4,
    },
};

/// Aim-idle translations in model units and rotations in 1/4096 turns for tracks 0 and 1.
static const AnimationPackedPose _gUnused85AimIdleAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/unused_85_animation_0A670_bank1.inc"
};

/// Packed aim-idle Euler rotations in eight-unit angle steps for tracks 2..18.
static const AnimationPackedRotation _gUnused85AimIdleAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/unused_85_animation_0A670_bank4.inc"
};

/// Aim-idle keyframes and loop/stop controls; durations count normal-rate frames.
static const AnimationRecord _gUnused85AimIdleAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/unused_85_animation_0A670_records.inc"
};

/// Absolute aim-idle record starts for tracks 0..18 and the retained unused export halfword.
static const u16 _gUnused85AimIdleAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/unused_85_animation_0A670_indices.inc"
};

/// Player stationary aim pose for unused weapon index 6.
///
/// Native-bank set 9 loops each of its 19 tracks on a fixed pose, without cues.
/// Child playback uses tracks 1..18. The set and payloads are borrowed while
/// this package remains loaded.
static AnimationSet _gUnused85AimIdleAnimation = {
    .records           = _gUnused85AimIdleAnimationRecords,
    .trackStartIndices = _gUnused85AimIdleAnimationIndices,
    .poseBanks         = {
        [UNUSED_85_ANIMATION_TRANSLATION_ROTATION_BANK] = _gUnused85AimIdleAnimationBank1,
        [UNUSED_85_ANIMATION_PACKED_ROTATION_BANK]      = _gUnused85AimIdleAnimationBank4,
    },
};

/// Backward/turn aim translations in model units and rotations in 1/4096 turns for tracks 0 and 1.
static const AnimationPackedPose _gUnused85AimBackwardOrTurnAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/unused_85_animation_0AD60_bank1.inc"
};

/// Packed backward/turn aim Euler rotations in eight-unit angle steps for tracks 2..18.
static const AnimationPackedRotation _gUnused85AimBackwardOrTurnAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/unused_85_animation_0AD60_bank4.inc"
};

/// Backward/turn aim keyframes and loop/stop controls, with footstep cues on track 1.
///
/// Durations count normal-rate frames; pose offsets count four-byte words.
static const AnimationRecord _gUnused85AimBackwardOrTurnAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/unused_85_animation_0AD60_records.inc"
};

/// Absolute backward/turn aim record starts for tracks 0..18 and the retained unused export halfword.
static const u16 _gUnused85AimBackwardOrTurnAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/unused_85_animation_0AD60_indices.inc"
};

/// Player backward movement or stationary turning while aiming with unused weapon index 6.
///
/// Native-bank set 13 loops all 19 tracks. Child playback uses tracks 1..18,
/// with track 1 supplying alternating footstep cues. The set and payloads are
/// borrowed while this package remains loaded.
static AnimationSet _gUnused85AimBackwardOrTurnAnimation = {
    .records           = _gUnused85AimBackwardOrTurnAnimationRecords,
    .trackStartIndices = _gUnused85AimBackwardOrTurnAnimationIndices,
    .poseBanks         = {
        [UNUSED_85_ANIMATION_TRANSLATION_ROTATION_BANK] = _gUnused85AimBackwardOrTurnAnimationBank1,
        [UNUSED_85_ANIMATION_PACKED_ROTATION_BANK]      = _gUnused85AimBackwardOrTurnAnimationBank4,
    },
};

/// Forward-aim translations in model units and rotations in 1/4096 turns for tracks 0 and 1.
static const AnimationPackedPose _gUnused85AimForwardAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/unused_85_animation_0B94C_bank1.inc"
};

/// Packed forward-aim Euler rotations in eight-unit angle steps for tracks 2..18.
static const AnimationPackedRotation _gUnused85AimForwardAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/unused_85_animation_0B94C_bank4.inc"
};

/// Forward-aim keyframes and loop/stop controls, with footstep cues on track 1.
///
/// Durations count normal-rate frames; pose offsets count four-byte words.
static const AnimationRecord _gUnused85AimForwardAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/unused_85_animation_0B94C_records.inc"
};

/// Absolute forward-aim record starts for tracks 0..18 and the retained unused export halfword.
static const u16 _gUnused85AimForwardAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/unused_85_animation_0B94C_indices.inc"
};

/// Player forward movement while aiming with unused weapon index 6.
///
/// Native-bank set 12 loops all 19 tracks. Child playback uses tracks 1..18,
/// with track 1 supplying alternating footstep cues. The set and payloads are
/// borrowed while this package remains loaded.
static AnimationSet _gUnused85AimForwardAnimation = {
    .records           = _gUnused85AimForwardAnimationRecords,
    .trackStartIndices = _gUnused85AimForwardAnimationIndices,
    .poseBanks         = {
        [UNUSED_85_ANIMATION_TRANSLATION_ROTATION_BANK] = _gUnused85AimForwardAnimationBank1,
        [UNUSED_85_ANIMATION_PACKED_ROTATION_BANK]      = _gUnused85AimForwardAnimationBank4,
    },
};

/// Reload translations in model units and rotations in 1/4096 turns for tracks 0 and 1.
static const AnimationPackedPose _gUnused85ReloadAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/unused_85_animation_0C274_bank1.inc"
};

/// Packed reload Euler rotations in eight-unit angle steps for tracks 2..18.
static const AnimationPackedRotation _gUnused85ReloadAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/unused_85_animation_0C274_bank4.inc"
};

/// Reload keyframes and stop controls, with the ammunition-loading cue on track 1.
///
/// Durations count normal-rate frames; pose offsets count four-byte words.
static const AnimationRecord _gUnused85ReloadAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/unused_85_animation_0C274_records.inc"
};

/// Absolute reload record starts for tracks 0..18 and the retained unused export halfword.
static const u16 _gUnused85ReloadAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/unused_85_animation_0C274_indices.inc"
};

/// Player reload clip shared by both load selections and battle end for unused weapon index 6.
///
/// Native-bank sets 14, 15 and 20 select this clip. Its 19 tracks stop at their
/// final poses; child playback uses tracks 1..18 and track 1 supplies the load cue.
/// The set and payloads are borrowed while this package remains loaded.
static AnimationSet _gUnused85ReloadAnimation = {
    .records           = _gUnused85ReloadAnimationRecords,
    .trackStartIndices = _gUnused85ReloadAnimationIndices,
    .poseBanks         = {
        [UNUSED_85_ANIMATION_TRANSLATION_ROTATION_BANK] = _gUnused85ReloadAnimationBank1,
        [UNUSED_85_ANIMATION_PACKED_ROTATION_BANK]      = _gUnused85ReloadAnimationBank4,
    },
};

/// Firing translations in model units and rotations in 1/4096 turns for tracks 0 and 1.
static const AnimationPackedPose _gUnused85FireAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/unused_85_animation_0C644_bank1.inc"
};

/// Packed firing Euler rotations in eight-unit angle steps for tracks 2..18.
static const AnimationPackedRotation _gUnused85FireAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/unused_85_animation_0C644_bank4.inc"
};

/// Firing keyframes and stop controls without cues; durations count normal-rate frames.
static const AnimationRecord _gUnused85FireAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/unused_85_animation_0C644_records.inc"
};

/// Absolute firing record starts for tracks 0..18 and the retained unused export halfword.
static const u16 _gUnused85FireAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/unused_85_animation_0C644_indices.inc"
};

/// Retained player firing clip for unused weapon index 6.
///
/// Native-bank sets 10 and 11 share this clip, but the weapon's empty attack
/// handler does not select it. All 19 tracks stop at their final poses without
/// cues. Child playback uses tracks 1..18 and borrows the set and payloads
/// while this package remains loaded.
static AnimationSet _gUnused85FireAnimation = {
    .records           = _gUnused85FireAnimationRecords,
    .trackStartIndices = _gUnused85FireAnimationIndices,
    .poseBanks         = {
        [UNUSED_85_ANIMATION_TRANSLATION_ROTATION_BANK] = _gUnused85FireAnimationBank1,
        [UNUSED_85_ANIMATION_PACKED_ROTATION_BANK]      = _gUnused85FireAnimationBank4,
    },
};

/// Aim-entry translations in model units and rotations in 1/4096 turns for tracks 0 and 1.
static const AnimationPackedPose _gUnused85AimEntryAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/unused_85_animation_0CCC0_bank1.inc"
};

/// Packed aim-entry Euler rotations in eight-unit angle steps for tracks 2..18.
static const AnimationPackedRotation _gUnused85AimEntryAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/unused_85_animation_0CCC0_bank4.inc"
};

/// Aim-entry keyframes and stop controls without cues; durations count normal-rate frames.
static const AnimationRecord _gUnused85AimEntryAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/unused_85_animation_0CCC0_records.inc"
};

/// Absolute aim-entry record starts for tracks 0..18 and the retained unused export halfword.
static const u16 _gUnused85AimEntryAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/unused_85_animation_0CCC0_indices.inc"
};

/// Player transition into the aiming stance for unused weapon index 6.
///
/// Native-bank set 7 stops all 19 tracks at their final poses without cues.
/// Child playback uses tracks 1..18. The set and payloads are borrowed while
/// this package remains loaded.
static AnimationSet _gUnused85AimEntryAnimation = {
    .records           = _gUnused85AimEntryAnimationRecords,
    .trackStartIndices = _gUnused85AimEntryAnimationIndices,
    .poseBanks         = {
        [UNUSED_85_ANIMATION_TRANSLATION_ROTATION_BANK] = _gUnused85AimEntryAnimationBank1,
        [UNUSED_85_ANIMATION_PACKED_ROTATION_BANK]      = _gUnused85AimEntryAnimationBank4,
    },
};

static AnimationPackedPose _gUnused85Animation0D28CBank1[10] = {
#include "assets/unused_85_animation_0D28C_bank1.inc"
};

static AnimationPackedRotation _gUnused85Animation0D28CBank4[145] = {
#include "assets/unused_85_animation_0D28C_bank4.inc"
};

static AnimationRecord _gUnused85Animation0D28CRecords[176] = {
#include "assets/unused_85_animation_0D28C_records.inc"
};

static u16 _gUnused85Animation0D28CIndices[20] = {
#include "assets/unused_85_animation_0D28C_indices.inc"
};

static AnimationSet _gUnused85Animation0D28C = {
    _gUnused85Animation0D28CRecords,
    _gUnused85Animation0D28CIndices,
    { NULL, _gUnused85Animation0D28CBank1, NULL, NULL, _gUnused85Animation0D28CBank4, NULL, NULL, NULL },
};

AnimationBank D_unused_85_8012A474 = { { {
    NULL,
    &_gUnused85IdleAnimation,
    &_gUnused85WalkAnimation,
    &_gUnused85BackwardWalkAnimation,
    &_gUnused85RunAnimation,
    &_gUnused85NegativeTurnAnimation,
    &_gUnused85PositiveTurnAnimation,
    &_gUnused85AimEntryAnimation,
    &_gUnused85Animation0D28C,
    &_gUnused85AimIdleAnimation,
    &_gUnused85FireAnimation,
    &_gUnused85FireAnimation,
    &_gUnused85AimForwardAnimation,
    &_gUnused85AimBackwardOrTurnAnimation,
    &_gUnused85ReloadAnimation,
    &_gUnused85ReloadAnimation,
    &_gUnused85Part4HitReactionAnimation,
    &_gUnused85OtherBodyHitReactionAnimation,
    &_gUnused85StoppedPoseAnimation,
    &_gUnused85UnarmedWalkAnimation,
    &_gUnused85ReloadAnimation,
    &_gUnused85IdleAnimation,
    &_gUnused85IdleAnimation,
    &_gUnused85HealthyIdleAnimation,
    &_gUnused85MediumHealthIdleAnimation,
    &_gUnused85CriticalHealthIdleAnimation,
    &_gUnused85PeReleaseFamily0Animation,
    &_gUnused85PeChargeFamily0Animation,
    &_gUnused85PeCastFamily0Animation,
    &_gUnused85PeReleaseFamily1Animation,
    &_gUnused85PeChargeFamily1Animation,
    &_gUnused85PeCastFamily1Animation,
    &_gUnused85ScriptedPresentationEnterAnimation,
    &_gUnused85ScriptedPresentationLeaveAnimation,
    &_gUnused85ScriptedPresentationEnterAnimation,
    &_gUnused85ScriptedPresentationLeaveAnimation,
    &_gUnused85StairAscentAnimation,
    &_gUnused85StairDescentAnimation,
    &_gUnused85StairAscentFinishEvenAnimation,
    &_gUnused85StairAscentFinishOddAnimation,
    &_gUnused85ItemUseAnimation,
    &_gUnused85IdleAnimation,
    &_gUnused85PeReleaseFamily2Animation,
    &_gUnused85PeChargeFamily2Animation,
    &_gUnused85PeCastFamily2Animation,
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
