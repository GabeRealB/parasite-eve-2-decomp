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

static AnimationPackedPose _gUnused85Animation04E4CBank1[7] = {
#include "assets/unused_85_animation_04E4C_bank1.inc"
};

static AnimationPackedRotation _gUnused85Animation04E4CBank4[73] = {
#include "assets/unused_85_animation_04E4C_bank4.inc"
};

static AnimationRecord _gUnused85Animation04E4CRecords[114] = {
#include "assets/unused_85_animation_04E4C_records.inc"
};

static u16 _gUnused85Animation04E4CIndices[20] = {
#include "assets/unused_85_animation_04E4C_indices.inc"
};

static AnimationSet _gUnused85Animation04E4C = {
    _gUnused85Animation04E4CRecords,
    _gUnused85Animation04E4CIndices,
    { NULL, _gUnused85Animation04E4CBank1, NULL, NULL, _gUnused85Animation04E4CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gUnused85Animation052D4Bank1[9] = {
#include "assets/unused_85_animation_052D4_bank1.inc"
};

static AnimationPackedRotation _gUnused85Animation052D4Bank4[104] = {
#include "assets/unused_85_animation_052D4_bank4.inc"
};

static AnimationRecord _gUnused85Animation052D4Records[139] = {
#include "assets/unused_85_animation_052D4_records.inc"
};

static u16 _gUnused85Animation052D4Indices[20] = {
#include "assets/unused_85_animation_052D4_indices.inc"
};

static AnimationSet _gUnused85Animation052D4 = {
    _gUnused85Animation052D4Records,
    _gUnused85Animation052D4Indices,
    { NULL, _gUnused85Animation052D4Bank1, NULL, NULL, _gUnused85Animation052D4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gUnused85Animation054D0Bank1[3] = {
#include "assets/unused_85_animation_054D0_bank1.inc"
};

static AnimationPackedRotation _gUnused85Animation054D0Bank4[22] = {
#include "assets/unused_85_animation_054D0_bank4.inc"
};

static AnimationRecord _gUnused85Animation054D0Records[76] = {
#include "assets/unused_85_animation_054D0_records.inc"
};

static u16 _gUnused85Animation054D0Indices[20] = {
#include "assets/unused_85_animation_054D0_indices.inc"
};

static AnimationSet _gUnused85Animation054D0 = {
    _gUnused85Animation054D0Records,
    _gUnused85Animation054D0Indices,
    { NULL, _gUnused85Animation054D0Bank1, NULL, NULL, _gUnused85Animation054D0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gUnused85Animation057A8Bank1[6] = {
#include "assets/unused_85_animation_057A8_bank1.inc"
};

static AnimationPackedRotation _gUnused85Animation057A8Bank4[57] = {
#include "assets/unused_85_animation_057A8_bank4.inc"
};

static AnimationRecord _gUnused85Animation057A8Records[87] = {
#include "assets/unused_85_animation_057A8_records.inc"
};

static u16 _gUnused85Animation057A8Indices[20] = {
#include "assets/unused_85_animation_057A8_indices.inc"
};

static AnimationSet _gUnused85Animation057A8 = {
    _gUnused85Animation057A8Records,
    _gUnused85Animation057A8Indices,
    { NULL, _gUnused85Animation057A8Bank1, NULL, NULL, _gUnused85Animation057A8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gUnused85Animation05A4CBank1[4] = {
#include "assets/unused_85_animation_05A4C_bank1.inc"
};

static AnimationPackedRotation _gUnused85Animation05A4CBank4[55] = {
#include "assets/unused_85_animation_05A4C_bank4.inc"
};

static AnimationRecord _gUnused85Animation05A4CRecords[82] = {
#include "assets/unused_85_animation_05A4C_records.inc"
};

static u16 _gUnused85Animation05A4CIndices[20] = {
#include "assets/unused_85_animation_05A4C_indices.inc"
};

static AnimationSet _gUnused85Animation05A4C = {
    _gUnused85Animation05A4CRecords,
    _gUnused85Animation05A4CIndices,
    { NULL, _gUnused85Animation05A4CBank1, NULL, NULL, _gUnused85Animation05A4CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gUnused85Animation05C4CBank1[3] = {
#include "assets/unused_85_animation_05C4C_bank1.inc"
};

static AnimationPackedRotation _gUnused85Animation05C4CBank4[23] = {
#include "assets/unused_85_animation_05C4C_bank4.inc"
};

static AnimationRecord _gUnused85Animation05C4CRecords[76] = {
#include "assets/unused_85_animation_05C4C_records.inc"
};

static u16 _gUnused85Animation05C4CIndices[20] = {
#include "assets/unused_85_animation_05C4C_indices.inc"
};

static AnimationSet _gUnused85Animation05C4C = {
    _gUnused85Animation05C4CRecords,
    _gUnused85Animation05C4CIndices,
    { NULL, _gUnused85Animation05C4CBank1, NULL, NULL, _gUnused85Animation05C4CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gUnused85Animation05FA0Bank1[8] = {
#include "assets/unused_85_animation_05FA0_bank1.inc"
};

static AnimationPackedRotation _gUnused85Animation05FA0Bank4[68] = {
#include "assets/unused_85_animation_05FA0_bank4.inc"
};

static AnimationRecord _gUnused85Animation05FA0Records[101] = {
#include "assets/unused_85_animation_05FA0_records.inc"
};

static u16 _gUnused85Animation05FA0Indices[20] = {
#include "assets/unused_85_animation_05FA0_indices.inc"
};

static AnimationSet _gUnused85Animation05FA0 = {
    _gUnused85Animation05FA0Records,
    _gUnused85Animation05FA0Indices,
    { NULL, _gUnused85Animation05FA0Bank1, NULL, NULL, _gUnused85Animation05FA0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gUnused85Animation06254Bank1[5] = {
#include "assets/unused_85_animation_06254_bank1.inc"
};

static AnimationPackedRotation _gUnused85Animation06254Bank4[55] = {
#include "assets/unused_85_animation_06254_bank4.inc"
};

static AnimationRecord _gUnused85Animation06254Records[83] = {
#include "assets/unused_85_animation_06254_records.inc"
};

static u16 _gUnused85Animation06254Indices[20] = {
#include "assets/unused_85_animation_06254_indices.inc"
};

static AnimationSet _gUnused85Animation06254 = {
    _gUnused85Animation06254Records,
    _gUnused85Animation06254Indices,
    { NULL, _gUnused85Animation06254Bank1, NULL, NULL, _gUnused85Animation06254Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gUnused85Animation06574Bank1[6] = {
#include "assets/unused_85_animation_06574_bank1.inc"
};

static AnimationPackedRotation _gUnused85Animation06574Bank4[66] = {
#include "assets/unused_85_animation_06574_bank4.inc"
};

static AnimationRecord _gUnused85Animation06574Records[96] = {
#include "assets/unused_85_animation_06574_records.inc"
};

static u16 _gUnused85Animation06574Indices[20] = {
#include "assets/unused_85_animation_06574_indices.inc"
};

static AnimationSet _gUnused85Animation06574 = {
    _gUnused85Animation06574Records,
    _gUnused85Animation06574Indices,
    { NULL, _gUnused85Animation06574Bank1, NULL, NULL, _gUnused85Animation06574Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gUnused85Animation06D38Bank1[18] = {
#include "assets/unused_85_animation_06D38_bank1.inc"
};

static AnimationPackedRotation _gUnused85Animation06D38Bank4[184] = {
#include "assets/unused_85_animation_06D38_bank4.inc"
};

static AnimationRecord _gUnused85Animation06D38Records[239] = {
#include "assets/unused_85_animation_06D38_records.inc"
};

static u16 _gUnused85Animation06D38Indices[20] = {
#include "assets/unused_85_animation_06D38_indices.inc"
};

static AnimationSet _gUnused85Animation06D38 = {
    _gUnused85Animation06D38Records,
    _gUnused85Animation06D38Indices,
    { NULL, _gUnused85Animation06D38Bank1, NULL, NULL, _gUnused85Animation06D38Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gUnused85Animation07FD0Bank1[29] = {
#include "assets/unused_85_animation_07FD0_bank1.inc"
};

static AnimationPackedRotation _gUnused85Animation07FD0Bank4[450] = {
#include "assets/unused_85_animation_07FD0_bank4.inc"
};

static AnimationRecord _gUnused85Animation07FD0Records[633] = {
#include "assets/unused_85_animation_07FD0_records.inc"
};

static u16 _gUnused85Animation07FD0Indices[20] = {
#include "assets/unused_85_animation_07FD0_indices.inc"
};

static AnimationSet _gUnused85Animation07FD0 = {
    _gUnused85Animation07FD0Records,
    _gUnused85Animation07FD0Indices,
    { NULL, _gUnused85Animation07FD0Bank1, NULL, NULL, _gUnused85Animation07FD0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gUnused85Animation08B48Bank1[12] = {
#include "assets/unused_85_animation_08B48_bank1.inc"
};

static AnimationPackedRotation _gUnused85Animation08B48Bank4[266] = {
#include "assets/unused_85_animation_08B48_bank4.inc"
};

static AnimationRecord _gUnused85Animation08B48Records[412] = {
#include "assets/unused_85_animation_08B48_records.inc"
};

static u16 _gUnused85Animation08B48Indices[20] = {
#include "assets/unused_85_animation_08B48_indices.inc"
};

static AnimationSet _gUnused85Animation08B48 = {
    _gUnused85Animation08B48Records,
    _gUnused85Animation08B48Indices,
    { NULL, _gUnused85Animation08B48Bank1, NULL, NULL, _gUnused85Animation08B48Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gUnused85Animation09264Bank1[9] = {
#include "assets/unused_85_animation_09264_bank1.inc"
};

static AnimationPackedRotation _gUnused85Animation09264Bank4[144] = {
#include "assets/unused_85_animation_09264_bank4.inc"
};

static AnimationRecord _gUnused85Animation09264Records[264] = {
#include "assets/unused_85_animation_09264_records.inc"
};

static u16 _gUnused85Animation09264Indices[20] = {
#include "assets/unused_85_animation_09264_indices.inc"
};

static AnimationSet _gUnused85Animation09264 = {
    _gUnused85Animation09264Records,
    _gUnused85Animation09264Indices,
    { NULL, _gUnused85Animation09264Bank1, NULL, NULL, _gUnused85Animation09264Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gUnused85Animation096E4Bank1[6] = {
#include "assets/unused_85_animation_096E4_bank1.inc"
};

static AnimationPackedRotation _gUnused85Animation096E4Bank4[107] = {
#include "assets/unused_85_animation_096E4_bank4.inc"
};

static AnimationRecord _gUnused85Animation096E4Records[143] = {
#include "assets/unused_85_animation_096E4_records.inc"
};

static u16 _gUnused85Animation096E4Indices[20] = {
#include "assets/unused_85_animation_096E4_indices.inc"
};

static AnimationSet _gUnused85Animation096E4 = {
    _gUnused85Animation096E4Records,
    _gUnused85Animation096E4Indices,
    { NULL, _gUnused85Animation096E4Bank1, NULL, NULL, _gUnused85Animation096E4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gUnused85Animation098BCBank1[3] = {
#include "assets/unused_85_animation_098BC_bank1.inc"
};

static AnimationPackedRotation _gUnused85Animation098BCBank4[32] = {
#include "assets/unused_85_animation_098BC_bank4.inc"
};

static AnimationRecord _gUnused85Animation098BCRecords[57] = {
#include "assets/unused_85_animation_098BC_records.inc"
};

static u16 _gUnused85Animation098BCIndices[20] = {
#include "assets/unused_85_animation_098BC_indices.inc"
};

static AnimationSet _gUnused85Animation098BC = {
    _gUnused85Animation098BCRecords,
    _gUnused85Animation098BCIndices,
    { NULL, _gUnused85Animation098BCBank1, NULL, NULL, _gUnused85Animation098BCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gUnused85Animation09E20Bank1[11] = {
#include "assets/unused_85_animation_09E20_bank1.inc"
};

static AnimationPackedRotation _gUnused85Animation09E20Bank4[125] = {
#include "assets/unused_85_animation_09E20_bank4.inc"
};

static AnimationRecord _gUnused85Animation09E20Records[167] = {
#include "assets/unused_85_animation_09E20_records.inc"
};

static u16 _gUnused85Animation09E20Indices[20] = {
#include "assets/unused_85_animation_09E20_indices.inc"
};

static AnimationSet _gUnused85Animation09E20 = {
    _gUnused85Animation09E20Records,
    _gUnused85Animation09E20Indices,
    { NULL, _gUnused85Animation09E20Bank1, NULL, NULL, _gUnused85Animation09E20Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gUnused85Animation0A014Bank1[3] = {
#include "assets/unused_85_animation_0A014_bank1.inc"
};

static AnimationPackedRotation _gUnused85Animation0A014Bank4[20] = {
#include "assets/unused_85_animation_0A014_bank4.inc"
};

static AnimationRecord _gUnused85Animation0A014Records[76] = {
#include "assets/unused_85_animation_0A014_records.inc"
};

static u16 _gUnused85Animation0A014Indices[20] = {
#include "assets/unused_85_animation_0A014_indices.inc"
};

static AnimationSet _gUnused85Animation0A014 = {
    _gUnused85Animation0A014Records,
    _gUnused85Animation0A014Indices,
    { NULL, _gUnused85Animation0A014Bank1, NULL, NULL, _gUnused85Animation0A014Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gUnused85Animation0A494Bank1[8] = {
#include "assets/unused_85_animation_0A494_bank1.inc"
};

static AnimationPackedRotation _gUnused85Animation0A494Bank4[105] = {
#include "assets/unused_85_animation_0A494_bank4.inc"
};

static AnimationRecord _gUnused85Animation0A494Records[139] = {
#include "assets/unused_85_animation_0A494_records.inc"
};

static u16 _gUnused85Animation0A494Indices[20] = {
#include "assets/unused_85_animation_0A494_indices.inc"
};

static AnimationSet _gUnused85Animation0A494 = {
    _gUnused85Animation0A494Records,
    _gUnused85Animation0A494Indices,
    { NULL, _gUnused85Animation0A494Bank1, NULL, NULL, _gUnused85Animation0A494Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gUnused85Animation0A670Bank1[2] = {
#include "assets/unused_85_animation_0A670_bank1.inc"
};

static AnimationPackedRotation _gUnused85Animation0A670Bank4[17] = {
#include "assets/unused_85_animation_0A670_bank4.inc"
};

static AnimationRecord _gUnused85Animation0A670Records[76] = {
#include "assets/unused_85_animation_0A670_records.inc"
};

static u16 _gUnused85Animation0A670Indices[20] = {
#include "assets/unused_85_animation_0A670_indices.inc"
};

static AnimationSet _gUnused85Animation0A670 = {
    _gUnused85Animation0A670Records,
    _gUnused85Animation0A670Indices,
    { NULL, _gUnused85Animation0A670Bank1, NULL, NULL, _gUnused85Animation0A670Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gUnused85Animation0AD60Bank1[12] = {
#include "assets/unused_85_animation_0AD60_bank1.inc"
};

static AnimationPackedRotation _gUnused85Animation0AD60Bank4[164] = {
#include "assets/unused_85_animation_0AD60_bank4.inc"
};

static AnimationRecord _gUnused85Animation0AD60Records[224] = {
#include "assets/unused_85_animation_0AD60_records.inc"
};

static u16 _gUnused85Animation0AD60Indices[20] = {
#include "assets/unused_85_animation_0AD60_indices.inc"
};

static AnimationSet _gUnused85Animation0AD60 = {
    _gUnused85Animation0AD60Records,
    _gUnused85Animation0AD60Indices,
    { NULL, _gUnused85Animation0AD60Bank1, NULL, NULL, _gUnused85Animation0AD60Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gUnused85Animation0B94CBank1[19] = {
#include "assets/unused_85_animation_0B94C_bank1.inc"
};

static AnimationPackedRotation _gUnused85Animation0B94CBank4[304] = {
#include "assets/unused_85_animation_0B94C_bank4.inc"
};

static AnimationRecord _gUnused85Animation0B94CRecords[382] = {
#include "assets/unused_85_animation_0B94C_records.inc"
};

static u16 _gUnused85Animation0B94CIndices[20] = {
#include "assets/unused_85_animation_0B94C_indices.inc"
};

static AnimationSet _gUnused85Animation0B94C = {
    _gUnused85Animation0B94CRecords,
    _gUnused85Animation0B94CIndices,
    { NULL, _gUnused85Animation0B94CBank1, NULL, NULL, _gUnused85Animation0B94CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gUnused85Animation0C274Bank1[15] = {
#include "assets/unused_85_animation_0C274_bank1.inc"
};

static AnimationPackedRotation _gUnused85Animation0C274Bank4[231] = {
#include "assets/unused_85_animation_0C274_bank4.inc"
};

static AnimationRecord _gUnused85Animation0C274Records[290] = {
#include "assets/unused_85_animation_0C274_records.inc"
};

static u16 _gUnused85Animation0C274Indices[20] = {
#include "assets/unused_85_animation_0C274_indices.inc"
};

static AnimationSet _gUnused85Animation0C274 = {
    _gUnused85Animation0C274Records,
    _gUnused85Animation0C274Indices,
    { NULL, _gUnused85Animation0C274Bank1, NULL, NULL, _gUnused85Animation0C274Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gUnused85Animation0C644Bank1[6] = {
#include "assets/unused_85_animation_0C644_bank1.inc"
};

static AnimationPackedRotation _gUnused85Animation0C644Bank4[81] = {
#include "assets/unused_85_animation_0C644_bank4.inc"
};

static AnimationRecord _gUnused85Animation0C644Records[125] = {
#include "assets/unused_85_animation_0C644_records.inc"
};

static u16 _gUnused85Animation0C644Indices[20] = {
#include "assets/unused_85_animation_0C644_indices.inc"
};

static AnimationSet _gUnused85Animation0C644 = {
    _gUnused85Animation0C644Records,
    _gUnused85Animation0C644Indices,
    { NULL, _gUnused85Animation0C644Bank1, NULL, NULL, _gUnused85Animation0C644Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gUnused85Animation0CCC0Bank1[11] = {
#include "assets/unused_85_animation_0CCC0_bank1.inc"
};

static AnimationPackedRotation _gUnused85Animation0CCC0Bank4[161] = {
#include "assets/unused_85_animation_0CCC0_bank4.inc"
};

static AnimationRecord _gUnused85Animation0CCC0Records[201] = {
#include "assets/unused_85_animation_0CCC0_records.inc"
};

static u16 _gUnused85Animation0CCC0Indices[20] = {
#include "assets/unused_85_animation_0CCC0_indices.inc"
};

static AnimationSet _gUnused85Animation0CCC0 = {
    _gUnused85Animation0CCC0Records,
    _gUnused85Animation0CCC0Indices,
    { NULL, _gUnused85Animation0CCC0Bank1, NULL, NULL, _gUnused85Animation0CCC0Bank4, NULL, NULL, NULL },
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
    &_gUnused85Animation0CCC0,
    &_gUnused85Animation0D28C,
    &_gUnused85Animation0A670,
    &_gUnused85Animation0C644,
    &_gUnused85Animation0C644,
    &_gUnused85Animation0B94C,
    &_gUnused85Animation0AD60,
    &_gUnused85Animation0C274,
    &_gUnused85Animation0C274,
    &_gUnused85Animation06254,
    &_gUnused85Animation06574,
    &_gUnused85Animation06D38,
    &_gUnused85UnarmedWalkAnimation,
    &_gUnused85Animation0C274,
    &_gUnused85IdleAnimation,
    &_gUnused85IdleAnimation,
    &_gUnused85Animation07FD0,
    &_gUnused85Animation09264,
    &_gUnused85Animation08B48,
    &_gUnused85Animation052D4,
    &_gUnused85Animation054D0,
    &_gUnused85Animation057A8,
    &_gUnused85Animation05A4C,
    &_gUnused85Animation05C4C,
    &_gUnused85Animation05FA0,
    &_gUnused85Animation096E4,
    &_gUnused85Animation098BC,
    &_gUnused85Animation096E4,
    &_gUnused85Animation098BC,
    &_gUnused85StairAscentAnimation,
    &_gUnused85StairDescentAnimation,
    &_gUnused85Animation04E4C,
    &_gUnused85StairAscentFinishOddAnimation,
    &_gUnused85ItemUseAnimation,
    &_gUnused85IdleAnimation,
    &_gUnused85Animation09E20,
    &_gUnused85Animation0A014,
    &_gUnused85Animation0A494,
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
