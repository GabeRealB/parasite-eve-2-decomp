#include "kyle/kyle.h"

#include <psyq/sys/types.h>
#include <psyq/libgpu.h>

#include "common.h"

/// Initial part-local pose and parent indices copied into each body model.
static TmdBone _gKyle800103KyleMadiganBodySkeleton[] = {
#include "assets/kyle_madigan_body_skeleton.inc"
};

/// Vertex counts for the twenty consecutive part-local groups, totalling 300.
static u32 _gKyle800103KyleMadiganBodyPartVerts[] = {
#include "assets/kyle_madigan_body_partVerts.inc"
};

/// Body vertices in integer part-local coordinates, ordered by skeletal part.
static SVECTOR _gKyle800103KyleMadiganBodyVerts[] = {
#include "assets/kyle_madigan_body_verts.inc"
};

/// Body lighting normals with twelve fractional bits, indexed independently of vertices.
static SVECTOR _gKyle800103KyleMadiganBodyNormals[] = {
#include "assets/kyle_madigan_body_normals.inc"
};

/// Writable draw commands: twenty skeletal groups and a final pre-transformed group.
///
/// First model creation resolves handler slots in place; the last word is
/// `TMD_STREAM_END`. Projection commands fill the first primitive-buffer region.
static u32 _gKyle800103KyleMadiganBodyStream[] = {
#include "assets/kyle_madigan_body_stream.inc"
};

TmdSource gKyle800103KyleMadiganBody = {
    .handlersResolved = false,
    // Each half holds all textured faces and one untextured Gouraud triangle.
    .bufferHalfBytes     = (204 * sizeof(POLY_GT3)) + (193 * sizeof(POLY_GT4)) + sizeof(POLY_G3),
    .preXformRegionBytes = (80 * sizeof(POLY_GT3)) + (48 * sizeof(POLY_GT4)),
    .partCount           = ARRAY_SIZE(_gKyle800103KyleMadiganBodySkeleton),
    .partVertexCounts    = _gKyle800103KyleMadiganBodyPartVerts,
    .verts               = _gKyle800103KyleMadiganBodyVerts,
    .normals             = _gKyle800103KyleMadiganBodyNormals,
    .skeleton            = _gKyle800103KyleMadiganBodySkeleton,
    .stream              = _gKyle800103KyleMadiganBodyStream,
};

/// Identity rest transform for the hand's single root, copied into each model.
static TmdBone _gKyle800103KyleMadiganHandRightSkeleton[] = {
#include "assets/kyle_madigan_hand_right_skeleton.inc"
};

/// Vertex count for the single part-local hand group (23 vertices).
static u32 _gKyle800103KyleMadiganHandRightPartVerts[] = {
#include "assets/kyle_madigan_hand_right_partVerts.inc"
};

/// Right-hand vertices in integer coordinates local to the hand root.
static SVECTOR _gKyle800103KyleMadiganHandRightVerts[] = {
#include "assets/kyle_madigan_hand_right_verts.inc"
};

/// Hand lighting normals with twelve fractional bits, indexed independently of vertices.
static SVECTOR _gKyle800103KyleMadiganHandRightNormals[] = {
#include "assets/kyle_madigan_hand_right_normals.inc"
};

/// Writable draw commands for four textured Gouraud triangles and nineteen quads.
///
/// One skeletal group and an empty final group end in `TMD_STREAM_END`.
/// First model creation resolves the command handler slots in place.
static u32 _gKyle800103KyleMadiganHandRightStream[] = {
#include "assets/kyle_madigan_hand_right_stream.inc"
};

TmdSource gKyle800103KyleMadiganHandRight = {
    .handlersResolved = false,
    .bufferHalfBytes  = (4 * sizeof(POLY_GT3)) + (19 * sizeof(POLY_GT4)),
    // All faces transform directly from the single hand part.
    .preXformRegionBytes = 0,
    .partCount           = ARRAY_SIZE(_gKyle800103KyleMadiganHandRightSkeleton),
    .partVertexCounts    = _gKyle800103KyleMadiganHandRightPartVerts,
    .verts               = _gKyle800103KyleMadiganHandRightVerts,
    .normals             = _gKyle800103KyleMadiganHandRightNormals,
    .skeleton            = _gKyle800103KyleMadiganHandRightSkeleton,
    .stream              = _gKyle800103KyleMadiganHandRightStream,
};

/// Identity rest transform for the right hand's single root, copied into each model.
static TmdBone _gKyle800103KyleMadiganHandRightSecondPairSkeleton[] = {
#include "assets/kyle_madigan_hand_right_second_pair_skeleton.inc"
};

/// Vertex count for the single part-local right-hand group (27 vertices).
static u32 _gKyle800103KyleMadiganHandRightSecondPairPartVerts[] = {
#include "assets/kyle_madigan_hand_right_second_pair_partVerts.inc"
};

/// Right-hand vertices in integer coordinates local to the hand root.
static SVECTOR _gKyle800103KyleMadiganHandRightSecondPairVerts[] = {
#include "assets/kyle_madigan_hand_right_second_pair_verts.inc"
};

/// Hand lighting normals with twelve fractional bits, indexed independently of vertices.
static SVECTOR _gKyle800103KyleMadiganHandRightSecondPairNormals[] = {
#include "assets/kyle_madigan_hand_right_second_pair_normals.inc"
};

/// Writable draw commands for two textured Gouraud triangles and twenty-four quads.
///
/// One skeletal group and an empty final group end in `TMD_STREAM_END`.
/// First model creation resolves the command handler slots in place.
static u32 _gKyle800103KyleMadiganHandRightSecondPairStream[] = {
#include "assets/kyle_madigan_hand_right_second_pair_stream.inc"
};

TmdSource gKyle800103KyleMadiganHandRightSecondPair = {
    .handlersResolved = false,
    .bufferHalfBytes  = (2 * sizeof(POLY_GT3)) + (24 * sizeof(POLY_GT4)),
    // All faces transform directly from the single hand part.
    .preXformRegionBytes = 0,
    .partCount           = ARRAY_SIZE(_gKyle800103KyleMadiganHandRightSecondPairSkeleton),
    .partVertexCounts    = _gKyle800103KyleMadiganHandRightSecondPairPartVerts,
    .verts               = _gKyle800103KyleMadiganHandRightSecondPairVerts,
    .normals             = _gKyle800103KyleMadiganHandRightSecondPairNormals,
    .skeleton            = _gKyle800103KyleMadiganHandRightSecondPairSkeleton,
    .stream              = _gKyle800103KyleMadiganHandRightSecondPairStream,
};

/// Identity rest transform for the left hand's single root, copied into each model.
static TmdBone _gKyle800103KyleMadiganHandLeftFirstPairSkeleton[] = {
#include "assets/kyle_madigan_left_skeleton.inc"
};

/// Vertex count for the left hand's single part (23 vertices).
static u32 _gKyle800103KyleMadiganHandLeftFirstPairPartVerts[] = {
#include "assets/kyle_madigan_left_partVerts.inc"
};

/// Left-hand vertices in integer coordinates local to the hand root.
static SVECTOR _gKyle800103KyleMadiganHandLeftFirstPairVerts[] = {
#include "assets/kyle_madigan_left_verts.inc"
};

/// Hand lighting normals with twelve fractional bits, indexed independently of vertices.
static SVECTOR _gKyle800103KyleMadiganHandLeftFirstPairNormals[] = {
#include "assets/kyle_madigan_left_normals.inc"
};

/// Writable draw commands for four textured Gouraud triangles and nineteen quads.
///
/// One skeletal group and an empty final group end in `TMD_STREAM_END`.
/// First model creation resolves the command handler slots in place.
static u32 _gKyle800103KyleMadiganHandLeftFirstPairStream[] = {
#include "assets/kyle_madigan_left_stream.inc"
};

TmdSource gKyle800103KyleMadiganHandLeftFirstPair = {
    .handlersResolved = false,
    .bufferHalfBytes  = (4 * sizeof(POLY_GT3)) + (19 * sizeof(POLY_GT4)),
    // All faces transform directly from the single hand part.
    .preXformRegionBytes = 0,
    .partCount           = ARRAY_SIZE(_gKyle800103KyleMadiganHandLeftFirstPairSkeleton),
    .partVertexCounts    = _gKyle800103KyleMadiganHandLeftFirstPairPartVerts,
    .verts               = _gKyle800103KyleMadiganHandLeftFirstPairVerts,
    .normals             = _gKyle800103KyleMadiganHandLeftFirstPairNormals,
    .skeleton            = _gKyle800103KyleMadiganHandLeftFirstPairSkeleton,
    .stream              = _gKyle800103KyleMadiganHandLeftFirstPairStream,
};

/// Identity rest transform for the left hand's single root, copied into each model.
static TmdBone _gKyle800103KyleMadiganHandLeftSkeleton[] = {
#include "assets/kyle_madigan_hand_left_skeleton.inc"
};

/// Vertex count for the single part-local left-hand group (27 vertices).
static u32 _gKyle800103KyleMadiganHandLeftPartVerts[] = {
#include "assets/kyle_madigan_hand_left_partVerts.inc"
};

/// Left-hand vertices in integer coordinates local to the hand root.
static SVECTOR _gKyle800103KyleMadiganHandLeftVerts[] = {
#include "assets/kyle_madigan_hand_left_verts.inc"
};

/// Hand lighting normals with twelve fractional bits, indexed independently of vertices.
static SVECTOR _gKyle800103KyleMadiganHandLeftNormals[] = {
#include "assets/kyle_madigan_hand_left_normals.inc"
};

/// Writable draw commands for two textured Gouraud triangles and twenty-four quads.
///
/// One skeletal group and an empty final group end in `TMD_STREAM_END`.
/// First model creation resolves the command handler slots in place.
static u32 _gKyle800103KyleMadiganHandLeftStream[] = {
#include "assets/kyle_madigan_hand_left_stream.inc"
};

TmdSource gKyle800103KyleMadiganHandLeft = {
    .handlersResolved = false,
    .bufferHalfBytes  = (2 * sizeof(POLY_GT3)) + (24 * sizeof(POLY_GT4)),
    // All faces transform directly from the single hand part.
    .preXformRegionBytes = 0,
    .partCount           = ARRAY_SIZE(_gKyle800103KyleMadiganHandLeftSkeleton),
    .partVertexCounts    = _gKyle800103KyleMadiganHandLeftPartVerts,
    .verts               = _gKyle800103KyleMadiganHandLeftVerts,
    .normals             = _gKyle800103KyleMadiganHandLeftNormals,
    .skeleton            = _gKyle800103KyleMadiganHandLeftSkeleton,
    .stream              = _gKyle800103KyleMadiganHandLeftStream,
};

/// Identity rest transform for the M950's single root, copied into each model.
static TmdBone _gKyle800103KyleMadiganM950Skeleton[] = {
#include "assets/kyle_800103_model_06038_skeleton.inc"
};

/// Vertex count for the single part-local weapon group (36 vertices).
static u32 _gKyle800103KyleMadiganM950PartVerts[] = {
#include "assets/kyle_800103_model_06038_partVerts.inc"
};

/// M950 vertices in integer coordinates local to the weapon root.
static SVECTOR _gKyle800103KyleMadiganM950Verts[] = {
#include "assets/kyle_800103_model_06038_verts.inc"
};

/// Weapon lighting normals with twelve fractional bits, indexed independently of vertices.
static SVECTOR _gKyle800103KyleMadiganM950Normals[] = {
#include "assets/kyle_800103_model_06038_normals.inc"
};

/// Writable draw commands for four textured Gouraud triangles and twenty-eight quads.
///
/// One skeletal group and an empty final group end in `TMD_STREAM_END`.
/// First model creation resolves the command handler slots in place.
static u32 _gKyle800103KyleMadiganM950Stream[] = {
#include "assets/kyle_800103_model_06038_stream.inc"
};

TmdSource gKyle800103KyleMadiganM950 = {
    .handlersResolved = false,
    .bufferHalfBytes  = (4 * sizeof(POLY_GT3)) + (28 * sizeof(POLY_GT4)),
    // All faces transform directly from the single weapon part.
    .preXformRegionBytes = 0,
    .partCount           = ARRAY_SIZE(_gKyle800103KyleMadiganM950Skeleton),
    .partVertexCounts    = _gKyle800103KyleMadiganM950PartVerts,
    .verts               = _gKyle800103KyleMadiganM950Verts,
    .normals             = _gKyle800103KyleMadiganM950Normals,
    .skeleton            = _gKyle800103KyleMadiganM950Skeleton,
    .stream              = _gKyle800103KyleMadiganM950Stream,
};

/// Encoding selectors indexing the private clips' pose banks.
enum {
    KYLE_800103_ANIMATION_TRANSLATION_ROTATION_BANK = 1,
    KYLE_800103_ANIMATION_PACKED_ROTATION_BANK      = 4
};

/// Encoding-1 translation/rotation poses for the idle clip's parts 0 and 1.
static AnimationPackedPose _gKyle800103KyleMadiganIdleAnimationBank1[] = {
#include "assets/kyle_800103_animation_065F8_bank1.inc"
};

/// Encoding-4 packed rotations for the idle clip's parts 2..19.
static AnimationPackedRotation _gKyle800103KyleMadiganIdleAnimationBank4[] = {
#include "assets/kyle_800103_animation_065F8_bank4.inc"
};

/// Idle keyframes and loop/stop controls; keyframe durations count whole playback frames.
static AnimationRecord _gKyle800103KyleMadiganIdleAnimationRecords[] = {
#include "assets/kyle_800103_animation_065F8_records.inc"
};

/// Absolute first-record indices for the idle clip's twenty model-part tracks.
static u16 _gKyle800103KyleMadiganIdleAnimationIndices[] = {
#include "assets/kyle_800103_animation_065F8_indices.inc"
};

/// Kyle Madigan's looping standing idle for companion resource variant 3.
///
/// Twenty part tracks; native playback uses parts 1..19. Banks 1 and 4 supply
/// translation/rotation for parts 0..1 and packed rotation for parts 2..19.
/// Keep this package loaded while playback or cached record pointers use it.
static AnimationSet _gKyle800103KyleMadiganIdleAnimation = {
    .records           = _gKyle800103KyleMadiganIdleAnimationRecords,
    .trackStartIndices = _gKyle800103KyleMadiganIdleAnimationIndices,
    .poseBanks         = {
        [KYLE_800103_ANIMATION_TRANSLATION_ROTATION_BANK] = _gKyle800103KyleMadiganIdleAnimationBank1,
        [KYLE_800103_ANIMATION_PACKED_ROTATION_BANK]      = _gKyle800103KyleMadiganIdleAnimationBank4,
    },
};

/// Encoding-1 translation/rotation poses for the walk clip's parts 0 and 1.
static AnimationPackedPose _gKyle800103KyleMadiganWalkAnimationBank1[] = {
#include "assets/kyle_800103_animation_06E78_bank1.inc"
};

/// Encoding-4 packed rotations for the walk clip's parts 2..19.
static AnimationPackedRotation _gKyle800103KyleMadiganWalkAnimationBank4[] = {
#include "assets/kyle_800103_animation_06E78_bank4.inc"
};

/// Walk keyframes, cue bits and loop/stop controls; keyframe durations count whole playback frames.
static AnimationRecord _gKyle800103KyleMadiganWalkAnimationRecords[] = {
#include "assets/kyle_800103_animation_06E78_records.inc"
};

/// Absolute first-record indices for the walk clip's twenty model-part tracks.
static u16 _gKyle800103KyleMadiganWalkAnimationIndices[] = {
#include "assets/kyle_800103_animation_06E78_indices.inc"
};

/// Kyle Madigan's looping walk for companion resource variant 3, with or without his weapon.
///
/// Twenty part tracks; native playback uses parts 1..19. Banks 1 and 4 supply
/// translation/rotation for parts 0..1 and packed rotation for parts 2..19.
/// Keep this package loaded while playback or cached record pointers use it.
static AnimationSet _gKyle800103KyleMadiganWalkAnimation = {
    .records           = _gKyle800103KyleMadiganWalkAnimationRecords,
    .trackStartIndices = _gKyle800103KyleMadiganWalkAnimationIndices,
    .poseBanks         = {
        [KYLE_800103_ANIMATION_TRANSLATION_ROTATION_BANK] = _gKyle800103KyleMadiganWalkAnimationBank1,
        [KYLE_800103_ANIMATION_PACKED_ROTATION_BANK]      = _gKyle800103KyleMadiganWalkAnimationBank4,
    },
};

/// Encoding-1 translation/rotation poses for the left-turn clip's parts 0 and 1.
static AnimationPackedPose _gKyle800103KyleMadiganTurnLeftAnimationBank1[] = {
#include "assets/kyle_800103_animation_076E4_bank1.inc"
};

/// Encoding-4 packed rotations for the left-turn clip's parts 2..19.
static AnimationPackedRotation _gKyle800103KyleMadiganTurnLeftAnimationBank4[] = {
#include "assets/kyle_800103_animation_076E4_bank4.inc"
};

/// Left-turn keyframes and loop/stop controls; keyframe durations count whole playback frames.
static AnimationRecord _gKyle800103KyleMadiganTurnLeftAnimationRecords[] = {
#include "assets/kyle_800103_animation_076E4_records.inc"
};

/// Absolute first-record indices for the left-turn clip's twenty model-part tracks.
static u16 _gKyle800103KyleMadiganTurnLeftAnimationIndices[] = {
#include "assets/kyle_800103_animation_076E4_indices.inc"
};

/// Kyle Madigan's looping negative-yaw turn for companion resource variant 3.
///
/// Twenty part tracks; native playback uses parts 1..19. Banks 1 and 4 supply
/// translation/rotation for parts 0..1 and packed rotation for parts 2..19.
/// Keep this package loaded while playback or cached record pointers use it.
static AnimationSet _gKyle800103KyleMadiganTurnLeftAnimation = {
    .records           = _gKyle800103KyleMadiganTurnLeftAnimationRecords,
    .trackStartIndices = _gKyle800103KyleMadiganTurnLeftAnimationIndices,
    .poseBanks         = {
        [KYLE_800103_ANIMATION_TRANSLATION_ROTATION_BANK] = _gKyle800103KyleMadiganTurnLeftAnimationBank1,
        [KYLE_800103_ANIMATION_PACKED_ROTATION_BANK]      = _gKyle800103KyleMadiganTurnLeftAnimationBank4,
    },
};

/// Encoding-1 translation/rotation poses for the right-turn clip's parts 0 and 1.
static AnimationPackedPose _gKyle800103KyleMadiganTurnRightAnimationBank1[] = {
#include "assets/kyle_800103_animation_07FB8_bank1.inc"
};

/// Encoding-4 packed rotations for the right-turn clip's parts 2..19.
static AnimationPackedRotation _gKyle800103KyleMadiganTurnRightAnimationBank4[] = {
#include "assets/kyle_800103_animation_07FB8_bank4.inc"
};

/// Right-turn keyframes and loop/stop controls; keyframe durations count whole playback frames.
static AnimationRecord _gKyle800103KyleMadiganTurnRightAnimationRecords[] = {
#include "assets/kyle_800103_animation_07FB8_records.inc"
};

/// Absolute first-record indices for the right-turn clip's twenty model-part tracks.
static u16 _gKyle800103KyleMadiganTurnRightAnimationIndices[] = {
#include "assets/kyle_800103_animation_07FB8_indices.inc"
};

/// Kyle Madigan's looping positive-yaw turn for companion resource variant 3.
///
/// Twenty part tracks; native playback uses parts 1..19. Banks 1 and 4 supply
/// translation/rotation for parts 0..1 and packed rotation for parts 2..19.
/// Keep this package loaded while playback or cached record pointers use it.
static AnimationSet _gKyle800103KyleMadiganTurnRightAnimation = {
    .records           = _gKyle800103KyleMadiganTurnRightAnimationRecords,
    .trackStartIndices = _gKyle800103KyleMadiganTurnRightAnimationIndices,
    .poseBanks         = {
        [KYLE_800103_ANIMATION_TRANSLATION_ROTATION_BANK] = _gKyle800103KyleMadiganTurnRightAnimationBank1,
        [KYLE_800103_ANIMATION_PACKED_ROTATION_BANK]      = _gKyle800103KyleMadiganTurnRightAnimationBank4,
    },
};

/// Encoding-1 translation/rotation poses for the part-4 hit clip's parts 0 and 1.
static AnimationPackedPose _gKyle800103KyleMadiganDamagePart4AnimationBank1[] = {
#include "assets/kyle_800103_animation_08478_bank1.inc"
};

/// Encoding-4 packed rotations for the part-4 hit clip's parts 2..19.
static AnimationPackedRotation _gKyle800103KyleMadiganDamagePart4AnimationBank4[] = {
#include "assets/kyle_800103_animation_08478_bank4.inc"
};

/// Part-4 hit keyframes and stop controls; keyframe durations count whole playback frames.
static AnimationRecord _gKyle800103KyleMadiganDamagePart4AnimationRecords[] = {
#include "assets/kyle_800103_animation_08478_records.inc"
};

/// Absolute first-record indices for the part-4 hit clip's twenty model-part tracks.
static u16 _gKyle800103KyleMadiganDamagePart4AnimationIndices[] = {
#include "assets/kyle_800103_animation_08478_indices.inc"
};

/// Kyle Madigan's hit reaction for body part 4 in companion resource variant 3.
///
/// Twenty part tracks end in stop controls; native playback uses parts 1..19.
/// Banks 1 and 4 supply translation/rotation for parts 0..1 and packed rotation
/// for parts 2..19. Keep the package loaded while playback or cached records use it.
static AnimationSet _gKyle800103KyleMadiganDamagePart4Animation = {
    .records           = _gKyle800103KyleMadiganDamagePart4AnimationRecords,
    .trackStartIndices = _gKyle800103KyleMadiganDamagePart4AnimationIndices,
    .poseBanks         = {
        [KYLE_800103_ANIMATION_TRANSLATION_ROTATION_BANK] = _gKyle800103KyleMadiganDamagePart4AnimationBank1,
        [KYLE_800103_ANIMATION_PACKED_ROTATION_BANK]      = _gKyle800103KyleMadiganDamagePart4AnimationBank4,
    },
};

/// Encoding-1 translation/rotation poses for the other-body hit clip's parts 0 and 1.
static const AnimationPackedPose _gKyle800103KyleMadiganDamageOtherBodyAnimationBank1[] SECTION(".data") = {
#include "assets/kyle_800103_animation_08A1C_bank1.inc"
};

/// Encoding-4 packed rotations for the other-body hit clip's parts 2..19.
static const AnimationPackedRotation _gKyle800103KyleMadiganDamageOtherBodyAnimationBank4[] SECTION(".data") = {
#include "assets/kyle_800103_animation_08A1C_bank4.inc"
};

/// Other-body hit keyframes and stop controls; durations count whole playback frames.
static const AnimationRecord _gKyle800103KyleMadiganDamageOtherBodyAnimationRecords[] SECTION(".data") = {
#include "assets/kyle_800103_animation_08A1C_records.inc"
};

/// Absolute first-record indices for the other-body hit clip's twenty part tracks.
static const u16 _gKyle800103KyleMadiganDamageOtherBodyAnimationIndices[] SECTION(".data") = {
#include "assets/kyle_800103_animation_08A1C_indices.inc"
};

/// Kyle Madigan's hit reaction outside body part 4 for companion resource variant 3.
///
/// Native bank set 17 supplies twenty part tracks ending in stop controls;
/// companion playback uses parts 1..19. Banks 1 and 4 encode translation/rotation
/// for parts 0..1 and rotation for parts 2..19. The immutable arrays retain the
/// package's data-section layout. Keep this package loaded while playback or
/// cached record pointers use the clip.
static AnimationSet _gKyle800103KyleMadiganDamageOtherBodyAnimation = {
    .records           = _gKyle800103KyleMadiganDamageOtherBodyAnimationRecords,
    .trackStartIndices = _gKyle800103KyleMadiganDamageOtherBodyAnimationIndices,
    .poseBanks         = {
        [KYLE_800103_ANIMATION_TRANSLATION_ROTATION_BANK] = _gKyle800103KyleMadiganDamageOtherBodyAnimationBank1,
        [KYLE_800103_ANIMATION_PACKED_ROTATION_BANK]      = _gKyle800103KyleMadiganDamageOtherBodyAnimationBank4,
    },
};

/// Encoding-1 translation/rotation poses for the stopped clip's parts 0 and 1.
static const AnimationPackedPose _gKyle800103KyleMadiganStoppedAnimationBank1[] SECTION(".data") = {
#include "assets/kyle_800103_animation_08F8C_bank1.inc"
};

/// Encoding-4 packed rotations for the stopped clip's parts 2..19.
static const AnimationPackedRotation _gKyle800103KyleMadiganStoppedAnimationBank4[] SECTION(".data") = {
#include "assets/kyle_800103_animation_08F8C_bank4.inc"
};

/// Stopped keyframes and stop controls; durations count whole playback frames.
static const AnimationRecord _gKyle800103KyleMadiganStoppedAnimationRecords[] SECTION(".data") = {
#include "assets/kyle_800103_animation_08F8C_records.inc"
};

/// Absolute first-record indices for the stopped clip's twenty model-part tracks.
static const u16 _gKyle800103KyleMadiganStoppedAnimationIndices[] SECTION(".data") = {
#include "assets/kyle_800103_animation_08F8C_indices.inc"
};

/// Kyle Madigan's collapse into the zero-HP stopped pose for resource variant 3.
///
/// Native bank set 18 supplies twenty part tracks ending in stop controls;
/// companion playback uses parts 1..19. Banks 1 and 4 encode translation/rotation
/// for parts 0..1 and rotation for parts 2..19. Keep this package loaded while
/// playback or cached record pointers use the clip.
static AnimationSet _gKyle800103KyleMadiganStoppedAnimation = {
    .records           = _gKyle800103KyleMadiganStoppedAnimationRecords,
    .trackStartIndices = _gKyle800103KyleMadiganStoppedAnimationIndices,
    .poseBanks         = {
        [KYLE_800103_ANIMATION_TRANSLATION_ROTATION_BANK] = _gKyle800103KyleMadiganStoppedAnimationBank1,
        [KYLE_800103_ANIMATION_PACKED_ROTATION_BANK]      = _gKyle800103KyleMadiganStoppedAnimationBank4,
    },
};

/// Encoding-1 translation/rotation poses for the idle variation's parts 0 and 1.
static const AnimationPackedPose _gKyle800103KyleMadiganIdleVariationAnimationBank1[] SECTION(".data") = {
#include "assets/kyle_800103_animation_09798_bank1.inc"
};

/// Encoding-4 packed rotations for the idle variation's parts 2..19.
static const AnimationPackedRotation _gKyle800103KyleMadiganIdleVariationAnimationBank4[] SECTION(".data") = {
#include "assets/kyle_800103_animation_09798_bank4.inc"
};

/// Idle-variation keyframes and loop/stop controls; durations count whole playback frames.
static const AnimationRecord _gKyle800103KyleMadiganIdleVariationAnimationRecords[] SECTION(".data") = {
#include "assets/kyle_800103_animation_09798_records.inc"
};

/// Absolute first-record indices for the idle variation's twenty model-part tracks.
static const u16 _gKyle800103KyleMadiganIdleVariationAnimationIndices[] SECTION(".data") = {
#include "assets/kyle_800103_animation_09798_indices.inc"
};

/// Kyle Madigan's looping idle variation for companion resource variant 3.
///
/// Native bank set 23 supplies twenty tracks with an entry followed by looping
/// poses; companion playback uses parts 1..19. Banks 1 and 4 encode
/// translation/rotation for parts 0..1 and rotation for parts 2..19. Keep this
/// package loaded while playback or cached record pointers use the clip.
static AnimationSet _gKyle800103KyleMadiganIdleVariationAnimation = {
    .records           = _gKyle800103KyleMadiganIdleVariationAnimationRecords,
    .trackStartIndices = _gKyle800103KyleMadiganIdleVariationAnimationIndices,
    .poseBanks         = {
        [KYLE_800103_ANIMATION_TRANSLATION_ROTATION_BANK] = _gKyle800103KyleMadiganIdleVariationAnimationBank1,
        [KYLE_800103_ANIMATION_PACKED_ROTATION_BANK]      = _gKyle800103KyleMadiganIdleVariationAnimationBank4,
    },
};

/// Encoding-1 translation/rotation poses for the kneeling clip's parts 0 and 1.
static const AnimationPackedPose _gKyle800103KyleMadiganKneelAnimationBank1[] SECTION(".data") = {
#include "assets/kyle_800103_animation_09C68_bank1.inc"
};

/// Encoding-4 packed rotations for the kneeling clip's parts 2..19.
static const AnimationPackedRotation _gKyle800103KyleMadiganKneelAnimationBank4[] SECTION(".data") = {
#include "assets/kyle_800103_animation_09C68_bank4.inc"
};

/// Kneeling keyframes and stop controls; durations count whole playback frames.
static const AnimationRecord _gKyle800103KyleMadiganKneelAnimationRecords[] SECTION(".data") = {
#include "assets/kyle_800103_animation_09C68_records.inc"
};

/// Absolute first-record indices for the kneeling clip's twenty model-part tracks.
static const u16 _gKyle800103KyleMadiganKneelAnimationIndices[] SECTION(".data") = {
#include "assets/kyle_800103_animation_09C68_indices.inc"
};

/// Kyle Madigan's transition from standing to one knee for resource variant 3.
///
/// Native bank set 32 supplies twenty part tracks ending in stop controls;
/// companion playback uses parts 1..19. Banks 1 and 4 encode translation/rotation
/// for parts 0..1 and rotation for parts 2..19. Keep this package loaded while
/// playback or cached record pointers use the clip.
static AnimationSet _gKyle800103KyleMadiganKneelAnimation = {
    .records           = _gKyle800103KyleMadiganKneelAnimationRecords,
    .trackStartIndices = _gKyle800103KyleMadiganKneelAnimationIndices,
    .poseBanks         = {
        [KYLE_800103_ANIMATION_TRANSLATION_ROTATION_BANK] = _gKyle800103KyleMadiganKneelAnimationBank1,
        [KYLE_800103_ANIMATION_PACKED_ROTATION_BANK]      = _gKyle800103KyleMadiganKneelAnimationBank4,
    },
};

/// Encoding-1 translation/rotation poses for the rise from kneeling's parts 0 and 1.
static const AnimationPackedPose _gKyle800103KyleMadiganRiseFromKneelAnimationBank1[] SECTION(".data") = {
#include "assets/kyle_800103_animation_0A1C0_bank1.inc"
};

/// Encoding-4 packed rotations for the rise from kneeling's parts 2..19.
static const AnimationPackedRotation _gKyle800103KyleMadiganRiseFromKneelAnimationBank4[] SECTION(".data") = {
#include "assets/kyle_800103_animation_0A1C0_bank4.inc"
};

/// Rise-from-kneeling keyframes and stop controls; durations count whole playback frames.
static const AnimationRecord _gKyle800103KyleMadiganRiseFromKneelAnimationRecords[] SECTION(".data") = {
#include "assets/kyle_800103_animation_0A1C0_records.inc"
};

/// Absolute first-record indices for the rise from kneeling's twenty part tracks.
static const u16 _gKyle800103KyleMadiganRiseFromKneelAnimationIndices[] SECTION(".data") = {
#include "assets/kyle_800103_animation_0A1C0_indices.inc"
};

/// Kyle Madigan's return from one knee to standing for companion resource variant 3.
///
/// Native bank set 33 supplies twenty part tracks ending in stop controls;
/// companion playback uses parts 1..19. Banks 1 and 4 encode translation/rotation
/// for parts 0..1 and rotation for parts 2..19. Keep this package loaded while
/// playback or cached record pointers use the clip.
static AnimationSet _gKyle800103KyleMadiganRiseFromKneelAnimation = {
    .records           = _gKyle800103KyleMadiganRiseFromKneelAnimationRecords,
    .trackStartIndices = _gKyle800103KyleMadiganRiseFromKneelAnimationIndices,
    .poseBanks         = {
        [KYLE_800103_ANIMATION_TRANSLATION_ROTATION_BANK] = _gKyle800103KyleMadiganRiseFromKneelAnimationBank1,
        [KYLE_800103_ANIMATION_PACKED_ROTATION_BANK]      = _gKyle800103KyleMadiganRiseFromKneelAnimationBank4,
    },
};

/// Encoding-1 translation/rotation poses for the combat hold's parts 0 and 1.
static const AnimationPackedPose _gKyle800103KyleMadiganCombatHoldAnimationBank1[] SECTION(".data") = {
#include "assets/kyle_800103_animation_0A3B0_bank1.inc"
};

/// Encoding-4 packed rotations for the combat hold's parts 2..19.
static const AnimationPackedRotation _gKyle800103KyleMadiganCombatHoldAnimationBank4[] SECTION(".data") = {
#include "assets/kyle_800103_animation_0A3B0_bank4.inc"
};

/// Repeated combat-hold keyframes and loop/stop controls; durations count playback frames.
static const AnimationRecord _gKyle800103KyleMadiganCombatHoldAnimationRecords[] SECTION(".data") = {
#include "assets/kyle_800103_animation_0A3B0_records.inc"
};

/// Absolute first-record indices for the combat hold's twenty model-part tracks.
static const u16 _gKyle800103KyleMadiganCombatHoldAnimationIndices[] SECTION(".data") = {
#include "assets/kyle_800103_animation_0A3B0_indices.inc"
};

/// Kyle Madigan's fixed held-weapon pose between combat decisions for resource variant 3.
///
/// Native bank set 9 repeats one pose per part through twenty looping tracks;
/// companion playback uses parts 1..19. Banks 1 and 4 encode translation/rotation
/// for parts 0..1 and rotation for parts 2..19. Keep this package loaded while
/// playback or cached record pointers use the clip.
static AnimationSet _gKyle800103KyleMadiganCombatHoldAnimation = {
    .records           = _gKyle800103KyleMadiganCombatHoldAnimationRecords,
    .trackStartIndices = _gKyle800103KyleMadiganCombatHoldAnimationIndices,
    .poseBanks         = {
        [KYLE_800103_ANIMATION_TRANSLATION_ROTATION_BANK] = _gKyle800103KyleMadiganCombatHoldAnimationBank1,
        [KYLE_800103_ANIMATION_PACKED_ROTATION_BANK]      = _gKyle800103KyleMadiganCombatHoldAnimationBank4,
    },
};

/// Encoding-1 translation/rotation poses for native clip 13's parts 0 and 1.
static const AnimationPackedPose _gKyle800103Animation0AB4CBank1[] SECTION(".data") = {
#include "assets/kyle_800103_animation_0AB4C_bank1.inc"
};

/// Encoding-4 packed rotations for native clip 13's parts 2..19.
static const AnimationPackedRotation _gKyle800103Animation0AB4CBank4[] SECTION(".data") = {
#include "assets/kyle_800103_animation_0AB4C_bank4.inc"
};

/// Native clip 13's looping keyframes, cue bits and retained terminal stop records.
///
/// Keyframe durations count normal-rate frames; part 1 carries both cue bits.
static const AnimationRecord _gKyle800103Animation0AB4CRecords[] SECTION(".data") = {
#include "assets/kyle_800103_animation_0AB4C_records.inc"
};

/// Absolute first-record indices for native clip 13's twenty model-part tracks.
static const u16 _gKyle800103Animation0AB4CIndices[] SECTION(".data") = {
#include "assets/kyle_800103_animation_0AB4C_indices.inc"
};

/// Kyle Madigan's looping native clip 13 for companion resource variant 3.
///
/// The motion role is unproven. Twenty part tracks use translation/rotation
/// for parts 0..1 and packed rotation for parts 2..19; companion playback uses
/// parts 1..19. Keep this package loaded while playback or cached record
/// pointers use the clip.
static AnimationSet _gKyle800103Animation0AB4C = {
    .records           = _gKyle800103Animation0AB4CRecords,
    .trackStartIndices = _gKyle800103Animation0AB4CIndices,
    .poseBanks         = {
        [KYLE_800103_ANIMATION_TRANSLATION_ROTATION_BANK] = _gKyle800103Animation0AB4CBank1,
        [KYLE_800103_ANIMATION_PACKED_ROTATION_BANK]      = _gKyle800103Animation0AB4CBank4,
    },
};

/// Encoding-1 translation/rotation poses for the combat approach's parts 0 and 1.
static const AnimationPackedPose _gKyle800103KyleMadiganCombatApproachAnimationBank1[] SECTION(".data") = {
#include "assets/kyle_800103_animation_0B55C_bank1.inc"
};

/// Encoding-4 packed rotations for the combat approach's parts 2..19.
static const AnimationPackedRotation _gKyle800103KyleMadiganCombatApproachAnimationBank4[] SECTION(".data") = {
#include "assets/kyle_800103_animation_0B55C_bank4.inc"
};

/// Combat-approach keyframes, cue bits and loop/stop controls; durations count normal-rate frames.
static const AnimationRecord _gKyle800103KyleMadiganCombatApproachAnimationRecords[] SECTION(".data") = {
#include "assets/kyle_800103_animation_0B55C_records.inc"
};

/// Absolute first-record indices for the combat approach's twenty model-part tracks.
static const u16 _gKyle800103KyleMadiganCombatApproachAnimationIndices[] SECTION(".data") = {
#include "assets/kyle_800103_animation_0B55C_indices.inc"
};

/// Kyle Madigan's looping forward combat approach for companion resource variant 3.
///
/// Native set 12 accompanies movement toward a target or the player. Twenty
/// part tracks use translation/rotation for parts 0..1 and packed rotation for
/// parts 2..19; companion playback uses parts 1..19. Keep this package loaded
/// while playback or cached record pointers use the clip.
static AnimationSet _gKyle800103KyleMadiganCombatApproachAnimation = {
    .records           = _gKyle800103KyleMadiganCombatApproachAnimationRecords,
    .trackStartIndices = _gKyle800103KyleMadiganCombatApproachAnimationIndices,
    .poseBanks         = {
        [KYLE_800103_ANIMATION_TRANSLATION_ROTATION_BANK] = _gKyle800103KyleMadiganCombatApproachAnimationBank1,
        [KYLE_800103_ANIMATION_PACKED_ROTATION_BANK]      = _gKyle800103KyleMadiganCombatApproachAnimationBank4,
    },
};

/// Encoding-1 translation/rotation poses for the M950 reload's parts 0 and 1.
static const AnimationPackedPose _gKyle800103KyleMadiganM950ReloadAnimationBank1[] SECTION(".data") = {
#include "assets/kyle_800103_animation_0BE4C_bank1.inc"
};

/// Encoding-4 packed rotations for the M950 reload's parts 2..19.
static const AnimationPackedRotation _gKyle800103KyleMadiganM950ReloadAnimationBank4[] SECTION(".data") = {
#include "assets/kyle_800103_animation_0BE4C_bank4.inc"
};

/// M950-reload keyframes and stop controls, without cue bits; durations count normal-rate frames.
static const AnimationRecord _gKyle800103KyleMadiganM950ReloadAnimationRecords[] SECTION(".data") = {
#include "assets/kyle_800103_animation_0BE4C_records.inc"
};

/// Absolute first-record indices for the M950 reload's twenty model-part tracks.
static const u16 _gKyle800103KyleMadiganM950ReloadAnimationIndices[] SECTION(".data") = {
#include "assets/kyle_800103_animation_0BE4C_indices.inc"
};

/// Kyle Madigan's M950 reload for companion resource variant 3.
///
/// Native sets 14 and 20 share this finite clip; native reload waits on part 1.
/// Twenty part tracks use translation/rotation for parts 0..1 and packed
/// rotation for parts 2..19; companion playback uses parts 1..19. Keep this
/// package loaded while playback or cached record pointers use the clip.
static AnimationSet _gKyle800103KyleMadiganM950ReloadAnimation = {
    .records           = _gKyle800103KyleMadiganM950ReloadAnimationRecords,
    .trackStartIndices = _gKyle800103KyleMadiganM950ReloadAnimationIndices,
    .poseBanks         = {
        [KYLE_800103_ANIMATION_TRANSLATION_ROTATION_BANK] = _gKyle800103KyleMadiganM950ReloadAnimationBank1,
        [KYLE_800103_ANIMATION_PACKED_ROTATION_BANK]      = _gKyle800103KyleMadiganM950ReloadAnimationBank4,
    },
};

/// Encoding-1 translation/rotation poses for the M950 shot's parts 0 and 1.
static const AnimationPackedPose _gKyle800103KyleMadiganM950FireAnimationBank1[] SECTION(".data") = {
#include "assets/kyle_800103_animation_0C0D4_bank1.inc"
};

/// Encoding-4 packed rotations for the M950 shot's parts 2..19.
static const AnimationPackedRotation _gKyle800103KyleMadiganM950FireAnimationBank4[] SECTION(".data") = {
#include "assets/kyle_800103_animation_0C0D4_bank4.inc"
};

/// M950-shot keyframes and stop controls; durations count normal-rate frames.
static const AnimationRecord _gKyle800103KyleMadiganM950FireAnimationRecords[] SECTION(".data") = {
#include "assets/kyle_800103_animation_0C0D4_records.inc"
};

/// Absolute first-record indices for the M950 shot's twenty model-part tracks.
static const u16 _gKyle800103KyleMadiganM950FireAnimationIndices[] SECTION(".data") = {
#include "assets/kyle_800103_animation_0C0D4_indices.inc"
};

/// Kyle Madigan's M950 shot and recoil for companion resource variant 3.
///
/// Native set 10 restarts on each shot of a burst; attack completion waits on
/// part 8. Twenty part tracks use translation/rotation for parts 0..1 and
/// packed rotation for parts 2..19; companion playback uses parts 1..19. Keep
/// this package loaded while playback or cached record pointers use the clip.
static AnimationSet _gKyle800103KyleMadiganM950FireAnimation = {
    .records           = _gKyle800103KyleMadiganM950FireAnimationRecords,
    .trackStartIndices = _gKyle800103KyleMadiganM950FireAnimationIndices,
    .poseBanks         = {
        [KYLE_800103_ANIMATION_TRANSLATION_ROTATION_BANK] = _gKyle800103KyleMadiganM950FireAnimationBank1,
        [KYLE_800103_ANIMATION_PACKED_ROTATION_BANK]      = _gKyle800103KyleMadiganM950FireAnimationBank4,
    },
};

/// Encoding-1 translation/rotation poses for combat entry's parts 0 and 1.
static const AnimationPackedPose _gKyle800103KyleMadiganCombatEntryAnimationBank1[] SECTION(".data") = {
#include "assets/kyle_800103_animation_0C56C_bank1.inc"
};

/// Encoding-4 packed rotations for combat entry's parts 2..19.
static const AnimationPackedRotation _gKyle800103KyleMadiganCombatEntryAnimationBank4[] SECTION(".data") = {
#include "assets/kyle_800103_animation_0C56C_bank4.inc"
};

/// Combat-entry keyframes and stop controls; durations count normal-rate frames.
static const AnimationRecord _gKyle800103KyleMadiganCombatEntryAnimationRecords[] SECTION(".data") = {
#include "assets/kyle_800103_animation_0C56C_records.inc"
};

/// Absolute first-record indices for combat entry's twenty model-part tracks.
static const u16 _gKyle800103KyleMadiganCombatEntryAnimationIndices[] SECTION(".data") = {
#include "assets/kyle_800103_animation_0C56C_indices.inc"
};

/// Kyle Madigan's weapon-raising combat entry for companion resource variant 3.
///
/// Native sets 7 and 34 share this finite transition from idle to the held
/// weapon pose. Twenty part tracks use translation/rotation for parts 0..1
/// and packed rotation for parts 2..19; companion playback uses parts 1..19.
/// Keep this package loaded while playback or cached record pointers use it.
static AnimationSet _gKyle800103KyleMadiganCombatEntryAnimation = {
    .records           = _gKyle800103KyleMadiganCombatEntryAnimationRecords,
    .trackStartIndices = _gKyle800103KyleMadiganCombatEntryAnimationIndices,
    .poseBanks         = {
        [KYLE_800103_ANIMATION_TRANSLATION_ROTATION_BANK] = _gKyle800103KyleMadiganCombatEntryAnimationBank1,
        [KYLE_800103_ANIMATION_PACKED_ROTATION_BANK]      = _gKyle800103KyleMadiganCombatEntryAnimationBank4,
    },
};

/// Encoding-1 translation/rotation poses for combat exit's parts 0 and 1.
static const AnimationPackedPose _gKyle800103KyleMadiganCombatExitAnimationBank1[] SECTION(".data") = {
#include "assets/kyle_800103_animation_0CA78_bank1.inc"
};

/// Encoding-4 packed rotations for combat exit's parts 2..19.
static const AnimationPackedRotation _gKyle800103KyleMadiganCombatExitAnimationBank4[] SECTION(".data") = {
#include "assets/kyle_800103_animation_0CA78_bank4.inc"
};

/// Combat-exit keyframes and stop controls; durations count normal-rate frames.
static const AnimationRecord _gKyle800103KyleMadiganCombatExitAnimationRecords[] SECTION(".data") = {
#include "assets/kyle_800103_animation_0CA78_records.inc"
};

/// Absolute first-record indices for combat exit's twenty model-part tracks.
static const u16 _gKyle800103KyleMadiganCombatExitAnimationIndices[] SECTION(".data") = {
#include "assets/kyle_800103_animation_0CA78_indices.inc"
};

/// Kyle Madigan's weapon-lowering combat exit for companion resource variant 3.
///
/// Native sets 8 and 35 share this finite transition from the held weapon pose
/// to idle. Twenty part tracks use translation/rotation for parts 0..1 and
/// packed rotation for parts 2..19; companion playback uses parts 1..19. Keep
/// this package loaded while playback or cached record pointers use the clip.
static AnimationSet _gKyle800103KyleMadiganCombatExitAnimation = {
    .records           = _gKyle800103KyleMadiganCombatExitAnimationRecords,
    .trackStartIndices = _gKyle800103KyleMadiganCombatExitAnimationIndices,
    .poseBanks         = {
        [KYLE_800103_ANIMATION_TRANSLATION_ROTATION_BANK] = _gKyle800103KyleMadiganCombatExitAnimationBank1,
        [KYLE_800103_ANIMATION_PACKED_ROTATION_BANK]      = _gKyle800103KyleMadiganCombatExitAnimationBank4,
    },
};

/// Run translation/rotation keys for tracks 0 and 1.
///
/// Translation uses integer model units; rotation uses 4096 units per turn.
static const AnimationPackedPose _gKyle800103KyleMadiganRunAnimationBank1[] SECTION(".data") = {
#include "assets/kyle_800103_animation_0D47C_bank1.inc"
};

/// Run rotations for tracks 2..19, in eight-unit steps over 4096 units per turn.
static const AnimationPackedRotation _gKyle800103KyleMadiganRunAnimationBank4[] SECTION(".data") = {
#include "assets/kyle_800103_animation_0D47C_bank4.inc"
};

/// Run keyframes, footstep cues and loop controls, in normal-rate frames.
///
/// Pose offsets count four-byte words; jumps use absolute record indices.
static const AnimationRecord _gKyle800103KyleMadiganRunAnimationRecords[] SECTION(".data") = {
#include "assets/kyle_800103_animation_0D47C_records.inc"
};

/// Absolute record-array starts for the run's twenty model-part tracks.
static const u16 _gKyle800103KyleMadiganRunAnimationIndices[] SECTION(".data") = {
#include "assets/kyle_800103_animation_0D47C_indices.inc"
};
STATIC_ASSERT(ARRAY_SIZE(_gKyle800103KyleMadiganRunAnimationIndices) == ARRAY_SIZE(_gKyle800103KyleMadiganBodySkeleton), kyle_800103_run_animation_tracks_match_body);

/// Kyle Madigan's looping run animation for companion resource variant 3.
///
/// Native bank slot 4 supplies distant following, retreat and obstacle scanning.
/// Track 1 carries footstep cues; twenty tracks encode translation/rotation for
/// parts 0 and 1 and rotation only for parts 2..19. Companion body playback uses
/// tracks 1..19. Keep this package loaded while playback or cached records use it.
static AnimationSet _gKyle800103KyleMadiganRunAnimation = {
    .records           = _gKyle800103KyleMadiganRunAnimationRecords,
    .trackStartIndices = _gKyle800103KyleMadiganRunAnimationIndices,
    .poseBanks         = {
        [KYLE_800103_ANIMATION_TRANSLATION_ROTATION_BANK] = _gKyle800103KyleMadiganRunAnimationBank1,
        [KYLE_800103_ANIMATION_PACKED_ROTATION_BANK]      = _gKyle800103KyleMadiganRunAnimationBank4,
    },
};

/// Native clip 3's translation/rotation keys for tracks 0 and 1.
///
/// Translation uses integer model units; rotation uses 4096 units per turn.
static const AnimationPackedPose _gKyle800103Animation0DBE4Bank1[] SECTION(".data") = {
#include "assets/kyle_800103_animation_0DBE4_bank1.inc"
};

/// Native clip 3's rotations for tracks 2..19, in eight-unit steps over 4096 units per turn.
static const AnimationPackedRotation _gKyle800103Animation0DBE4Bank4[] SECTION(".data") = {
#include "assets/kyle_800103_animation_0DBE4_bank4.inc"
};

/// Native clip 3's keyframes, footstep cues and loop controls, in normal-rate frames.
///
/// Pose offsets count four-byte words; jumps use absolute record indices.
static const AnimationRecord _gKyle800103Animation0DBE4Records[] SECTION(".data") = {
#include "assets/kyle_800103_animation_0DBE4_records.inc"
};

/// Absolute record-array starts for native clip 3's twenty model-part tracks.
static const u16 _gKyle800103Animation0DBE4Indices[] SECTION(".data") = {
#include "assets/kyle_800103_animation_0DBE4_indices.inc"
};
STATIC_ASSERT(ARRAY_SIZE(_gKyle800103Animation0DBE4Indices) == ARRAY_SIZE(_gKyle800103KyleMadiganBodySkeleton), kyle_800103_set3_animation_tracks_match_body);

/// Kyle Madigan's looping native clip 3 for companion resource variant 3.
///
/// Track 1 carries footstep cues; the motion's direction and a companion caller
/// are unproven. Twenty tracks encode translation/rotation for parts 0 and 1
/// and rotation only for parts 2..19. Keep this package loaded while playback
/// or cached records use the clip.
static AnimationSet _gKyle800103Animation0DBE4 = {
    .records           = _gKyle800103Animation0DBE4Records,
    .trackStartIndices = _gKyle800103Animation0DBE4Indices,
    .poseBanks         = {
        [KYLE_800103_ANIMATION_TRANSLATION_ROTATION_BANK] = _gKyle800103Animation0DBE4Bank1,
        [KYLE_800103_ANIMATION_PACKED_ROTATION_BANK]      = _gKyle800103Animation0DBE4Bank4,
    },
};

AnimationBank gKyle800103KyleMadiganAnimationBank = {
    .table = {
        .sets = {
            [1]  = &_gKyle800103KyleMadiganIdleAnimation,
            [2]  = &_gKyle800103KyleMadiganWalkAnimation,
            [3]  = &_gKyle800103Animation0DBE4,
            [4]  = &_gKyle800103KyleMadiganRunAnimation,
            [5]  = &_gKyle800103KyleMadiganTurnLeftAnimation,
            [6]  = &_gKyle800103KyleMadiganTurnRightAnimation,
            [7]  = &_gKyle800103KyleMadiganCombatEntryAnimation,
            [8]  = &_gKyle800103KyleMadiganCombatExitAnimation,
            [9]  = &_gKyle800103KyleMadiganCombatHoldAnimation,
            [10] = &_gKyle800103KyleMadiganM950FireAnimation,
            [11] = &_gKyle800103KyleMadiganIdleAnimation,
            [12] = &_gKyle800103KyleMadiganCombatApproachAnimation,
            [13] = &_gKyle800103Animation0AB4C,
            [14] = &_gKyle800103KyleMadiganM950ReloadAnimation,
            [15] = &_gKyle800103KyleMadiganIdleAnimation,
            [16] = &_gKyle800103KyleMadiganDamagePart4Animation,
            [17] = &_gKyle800103KyleMadiganDamageOtherBodyAnimation,
            [18] = &_gKyle800103KyleMadiganStoppedAnimation,
            [19] = &_gKyle800103KyleMadiganWalkAnimation,
            [20] = &_gKyle800103KyleMadiganM950ReloadAnimation,
            [21] = &_gKyle800103KyleMadiganIdleAnimation,
            [22] = &_gKyle800103KyleMadiganIdleAnimation,
            [23] = &_gKyle800103KyleMadiganIdleVariationAnimation,
            [24] = &_gKyle800103KyleMadiganIdleAnimation,
            [25] = &_gKyle800103KyleMadiganIdleAnimation,
            [32] = &_gKyle800103KyleMadiganKneelAnimation,
            [33] = &_gKyle800103KyleMadiganRiseFromKneelAnimation,
            [34] = &_gKyle800103KyleMadiganCombatEntryAnimation,
            [35] = &_gKyle800103KyleMadiganCombatExitAnimation,
            [36] = &_gKyle800103KyleMadiganIdleAnimation,
            [37] = &_gKyle800103KyleMadiganIdleAnimation,
            [38] = &_gKyle800103KyleMadiganIdleAnimation,
            [39] = &_gKyle800103KyleMadiganIdleAnimation,
            [40] = &_gKyle800103KyleMadiganIdleAnimation,
            // Unlisted base slots are empty; the writable extension starts zeroed.
            [ANIMATION_BANK_BASE_SET_COUNT] = NULL,
        },
    },
};
