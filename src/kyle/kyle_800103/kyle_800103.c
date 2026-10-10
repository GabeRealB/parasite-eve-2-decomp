#include "kyle/kyle.h"

#include <psyq/sys/types.h>
#include <psyq/libgpu.h>

#include "types.h"

#include "gameplay/animation.h"

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

static AnimationPackedPose _gKyle800103Animation08A1CBank1[10] = {
#include "assets/kyle_800103_animation_08A1C_bank1.inc"
};

static AnimationPackedRotation _gKyle800103Animation08A1CBank4[131] = {
#include "assets/kyle_800103_animation_08A1C_bank4.inc"
};

static AnimationRecord _gKyle800103Animation08A1CRecords[180] = {
#include "assets/kyle_800103_animation_08A1C_records.inc"
};

static u16 _gKyle800103Animation08A1CIndices[20] = {
#include "assets/kyle_800103_animation_08A1C_indices.inc"
};

static AnimationSet _gKyle800103Animation08A1C = {
    _gKyle800103Animation08A1CRecords,
    _gKyle800103Animation08A1CIndices,
    { NULL, _gKyle800103Animation08A1CBank1, NULL, NULL, _gKyle800103Animation08A1CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gKyle800103Animation08F8CBank1[11] = {
#include "assets/kyle_800103_animation_08F8C_bank1.inc"
};

static AnimationPackedRotation _gKyle800103Animation08F8CBank4[127] = {
#include "assets/kyle_800103_animation_08F8C_bank4.inc"
};

static AnimationRecord _gKyle800103Animation08F8CRecords[168] = {
#include "assets/kyle_800103_animation_08F8C_records.inc"
};

static u16 _gKyle800103Animation08F8CIndices[20] = {
#include "assets/kyle_800103_animation_08F8C_indices.inc"
};

static AnimationSet _gKyle800103Animation08F8C = {
    _gKyle800103Animation08F8CRecords,
    _gKyle800103Animation08F8CIndices,
    { NULL, _gKyle800103Animation08F8CBank1, NULL, NULL, _gKyle800103Animation08F8CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gKyle800103Animation09798Bank1[10] = {
#include "assets/kyle_800103_animation_09798_bank1.inc"
};

static AnimationPackedRotation _gKyle800103Animation09798Bank4[181] = {
#include "assets/kyle_800103_animation_09798_bank4.inc"
};

static AnimationRecord _gKyle800103Animation09798Records[284] = {
#include "assets/kyle_800103_animation_09798_records.inc"
};

static u16 _gKyle800103Animation09798Indices[20] = {
#include "assets/kyle_800103_animation_09798_indices.inc"
};

static AnimationSet _gKyle800103Animation09798 = {
    _gKyle800103Animation09798Records,
    _gKyle800103Animation09798Indices,
    { NULL, _gKyle800103Animation09798Bank1, NULL, NULL, _gKyle800103Animation09798Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gKyle800103Animation09C68Bank1[9] = {
#include "assets/kyle_800103_animation_09C68_bank1.inc"
};

static AnimationPackedRotation _gKyle800103Animation09C68Bank4[113] = {
#include "assets/kyle_800103_animation_09C68_bank4.inc"
};

static AnimationRecord _gKyle800103Animation09C68Records[148] = {
#include "assets/kyle_800103_animation_09C68_records.inc"
};

static u16 _gKyle800103Animation09C68Indices[20] = {
#include "assets/kyle_800103_animation_09C68_indices.inc"
};

static AnimationSet _gKyle800103Animation09C68 = {
    _gKyle800103Animation09C68Records,
    _gKyle800103Animation09C68Indices,
    { NULL, _gKyle800103Animation09C68Bank1, NULL, NULL, _gKyle800103Animation09C68Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gKyle800103Animation0A1C0Bank1[8] = {
#include "assets/kyle_800103_animation_0A1C0_bank1.inc"
};

static AnimationPackedRotation _gKyle800103Animation0A1C0Bank4[129] = {
#include "assets/kyle_800103_animation_0A1C0_bank4.inc"
};

static AnimationRecord _gKyle800103Animation0A1C0Records[169] = {
#include "assets/kyle_800103_animation_0A1C0_records.inc"
};

static u16 _gKyle800103Animation0A1C0Indices[20] = {
#include "assets/kyle_800103_animation_0A1C0_indices.inc"
};

static AnimationSet _gKyle800103Animation0A1C0 = {
    _gKyle800103Animation0A1C0Records,
    _gKyle800103Animation0A1C0Indices,
    { NULL, _gKyle800103Animation0A1C0Bank1, NULL, NULL, _gKyle800103Animation0A1C0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gKyle800103Animation0A3B0Bank1[2] = {
#include "assets/kyle_800103_animation_0A3B0_bank1.inc"
};

static AnimationPackedRotation _gKyle800103Animation0A3B0Bank4[18] = {
#include "assets/kyle_800103_animation_0A3B0_bank4.inc"
};

static AnimationRecord _gKyle800103Animation0A3B0Records[80] = {
#include "assets/kyle_800103_animation_0A3B0_records.inc"
};

static u16 _gKyle800103Animation0A3B0Indices[20] = {
#include "assets/kyle_800103_animation_0A3B0_indices.inc"
};

static AnimationSet _gKyle800103Animation0A3B0 = {
    _gKyle800103Animation0A3B0Records,
    _gKyle800103Animation0A3B0Indices,
    { NULL, _gKyle800103Animation0A3B0Bank1, NULL, NULL, _gKyle800103Animation0A3B0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gKyle800103Animation0AB4CBank1[12] = {
#include "assets/kyle_800103_animation_0AB4C_bank1.inc"
};

static AnimationPackedRotation _gKyle800103Animation0AB4CBank4[165] = {
#include "assets/kyle_800103_animation_0AB4C_bank4.inc"
};

static AnimationRecord _gKyle800103Animation0AB4CRecords[266] = {
#include "assets/kyle_800103_animation_0AB4C_records.inc"
};

static u16 _gKyle800103Animation0AB4CIndices[20] = {
#include "assets/kyle_800103_animation_0AB4C_indices.inc"
};

static AnimationSet _gKyle800103Animation0AB4C = {
    _gKyle800103Animation0AB4CRecords,
    _gKyle800103Animation0AB4CIndices,
    { NULL, _gKyle800103Animation0AB4CBank1, NULL, NULL, _gKyle800103Animation0AB4CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gKyle800103Animation0B55CBank1[16] = {
#include "assets/kyle_800103_animation_0B55C_bank1.inc"
};

static AnimationPackedRotation _gKyle800103Animation0B55CBank4[250] = {
#include "assets/kyle_800103_animation_0B55C_bank4.inc"
};

static AnimationRecord _gKyle800103Animation0B55CRecords[326] = {
#include "assets/kyle_800103_animation_0B55C_records.inc"
};

static u16 _gKyle800103Animation0B55CIndices[20] = {
#include "assets/kyle_800103_animation_0B55C_indices.inc"
};

static AnimationSet _gKyle800103Animation0B55C = {
    _gKyle800103Animation0B55CRecords,
    _gKyle800103Animation0B55CIndices,
    { NULL, _gKyle800103Animation0B55CBank1, NULL, NULL, _gKyle800103Animation0B55CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gKyle800103Animation0BE4CBank1[12] = {
#include "assets/kyle_800103_animation_0BE4C_bank1.inc"
};

static AnimationPackedRotation _gKyle800103Animation0BE4CBank4[235] = {
#include "assets/kyle_800103_animation_0BE4C_bank4.inc"
};

static AnimationRecord _gKyle800103Animation0BE4CRecords[281] = {
#include "assets/kyle_800103_animation_0BE4C_records.inc"
};

static u16 _gKyle800103Animation0BE4CIndices[20] = {
#include "assets/kyle_800103_animation_0BE4C_indices.inc"
};

static AnimationSet _gKyle800103Animation0BE4C = {
    _gKyle800103Animation0BE4CRecords,
    _gKyle800103Animation0BE4CIndices,
    { NULL, _gKyle800103Animation0BE4CBank1, NULL, NULL, _gKyle800103Animation0BE4CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gKyle800103Animation0C0D4Bank1[2] = {
#include "assets/kyle_800103_animation_0C0D4_bank1.inc"
};

static AnimationPackedRotation _gKyle800103Animation0C0D4Bank4[47] = {
#include "assets/kyle_800103_animation_0C0D4_bank4.inc"
};

static AnimationRecord _gKyle800103Animation0C0D4Records[89] = {
#include "assets/kyle_800103_animation_0C0D4_records.inc"
};

static u16 _gKyle800103Animation0C0D4Indices[20] = {
#include "assets/kyle_800103_animation_0C0D4_indices.inc"
};

static AnimationSet _gKyle800103Animation0C0D4 = {
    _gKyle800103Animation0C0D4Records,
    _gKyle800103Animation0C0D4Indices,
    { NULL, _gKyle800103Animation0C0D4Bank1, NULL, NULL, _gKyle800103Animation0C0D4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gKyle800103Animation0C56CBank1[8] = {
#include "assets/kyle_800103_animation_0C56C_bank1.inc"
};

static AnimationPackedRotation _gKyle800103Animation0C56CBank4[106] = {
#include "assets/kyle_800103_animation_0C56C_bank4.inc"
};

static AnimationRecord _gKyle800103Animation0C56CRecords[144] = {
#include "assets/kyle_800103_animation_0C56C_records.inc"
};

static u16 _gKyle800103Animation0C56CIndices[20] = {
#include "assets/kyle_800103_animation_0C56C_indices.inc"
};

static AnimationSet _gKyle800103Animation0C56C = {
    _gKyle800103Animation0C56CRecords,
    _gKyle800103Animation0C56CIndices,
    { NULL, _gKyle800103Animation0C56CBank1, NULL, NULL, _gKyle800103Animation0C56CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gKyle800103Animation0CA78Bank1[10] = {
#include "assets/kyle_800103_animation_0CA78_bank1.inc"
};

static AnimationPackedRotation _gKyle800103Animation0CA78Bank4[119] = {
#include "assets/kyle_800103_animation_0CA78_bank4.inc"
};

static AnimationRecord _gKyle800103Animation0CA78Records[154] = {
#include "assets/kyle_800103_animation_0CA78_records.inc"
};

static u16 _gKyle800103Animation0CA78Indices[20] = {
#include "assets/kyle_800103_animation_0CA78_indices.inc"
};

static AnimationSet _gKyle800103Animation0CA78 = {
    _gKyle800103Animation0CA78Records,
    _gKyle800103Animation0CA78Indices,
    { NULL, _gKyle800103Animation0CA78Bank1, NULL, NULL, _gKyle800103Animation0CA78Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gKyle800103Animation0D47CBank1[17] = {
#include "assets/kyle_800103_animation_0D47C_bank1.inc"
};

static AnimationPackedRotation _gKyle800103Animation0D47CBank4[246] = {
#include "assets/kyle_800103_animation_0D47C_bank4.inc"
};

static AnimationRecord _gKyle800103Animation0D47CRecords[324] = {
#include "assets/kyle_800103_animation_0D47C_records.inc"
};

static u16 _gKyle800103Animation0D47CIndices[20] = {
#include "assets/kyle_800103_animation_0D47C_indices.inc"
};

static AnimationSet _gKyle800103Animation0D47C = {
    _gKyle800103Animation0D47CRecords,
    _gKyle800103Animation0D47CIndices,
    { NULL, _gKyle800103Animation0D47CBank1, NULL, NULL, _gKyle800103Animation0D47CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gKyle800103Animation0DBE4Bank1[12] = {
#include "assets/kyle_800103_animation_0DBE4_bank1.inc"
};

static AnimationPackedRotation _gKyle800103Animation0DBE4Bank4[167] = {
#include "assets/kyle_800103_animation_0DBE4_bank4.inc"
};

static AnimationRecord _gKyle800103Animation0DBE4Records[251] = {
#include "assets/kyle_800103_animation_0DBE4_records.inc"
};

static u16 _gKyle800103Animation0DBE4Indices[20] = {
#include "assets/kyle_800103_animation_0DBE4_indices.inc"
};

static AnimationSet _gKyle800103Animation0DBE4 = {
    _gKyle800103Animation0DBE4Records,
    _gKyle800103Animation0DBE4Indices,
    { NULL, _gKyle800103Animation0DBE4Bank1, NULL, NULL, _gKyle800103Animation0DBE4Bank4, NULL, NULL, NULL },
};

AnimationBank D_kyle_800103_8017567C = { { {
    NULL,
    &_gKyle800103KyleMadiganIdleAnimation,
    &_gKyle800103KyleMadiganWalkAnimation,
    &_gKyle800103Animation0DBE4,
    &_gKyle800103Animation0D47C,
    &_gKyle800103KyleMadiganTurnLeftAnimation,
    &_gKyle800103KyleMadiganTurnRightAnimation,
    &_gKyle800103Animation0C56C,
    &_gKyle800103Animation0CA78,
    &_gKyle800103Animation0A3B0,
    &_gKyle800103Animation0C0D4,
    &_gKyle800103KyleMadiganIdleAnimation,
    &_gKyle800103Animation0B55C,
    &_gKyle800103Animation0AB4C,
    &_gKyle800103Animation0BE4C,
    &_gKyle800103KyleMadiganIdleAnimation,
    &_gKyle800103KyleMadiganDamagePart4Animation,
    &_gKyle800103Animation08A1C,
    &_gKyle800103Animation08F8C,
    &_gKyle800103KyleMadiganWalkAnimation,
    &_gKyle800103Animation0BE4C,
    &_gKyle800103KyleMadiganIdleAnimation,
    &_gKyle800103KyleMadiganIdleAnimation,
    &_gKyle800103Animation09798,
    &_gKyle800103KyleMadiganIdleAnimation,
    &_gKyle800103KyleMadiganIdleAnimation,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    &_gKyle800103Animation09C68,
    &_gKyle800103Animation0A1C0,
    &_gKyle800103Animation0C56C,
    &_gKyle800103Animation0CA78,
    &_gKyle800103KyleMadiganIdleAnimation,
    &_gKyle800103KyleMadiganIdleAnimation,
    &_gKyle800103KyleMadiganIdleAnimation,
    &_gKyle800103KyleMadiganIdleAnimation,
    &_gKyle800103KyleMadiganIdleAnimation,
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
    NULL,
    NULL,
    NULL,
    NULL,
} } };
