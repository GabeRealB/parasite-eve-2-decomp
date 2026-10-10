#include "kyle/kyle.h"

#include <psyq/sys/types.h>
#include <psyq/libgpu.h>

#include "types.h"

#include "gameplay/animation.h"

/// Initial part-local pose and parent indices copied into each body model.
static TmdBone _gKyle800104KyleMadiganBodySkeleton[] = {
#include "assets/kyle_madigan_body_skeleton.inc"
};

/// Vertex counts for the twenty consecutive part-local groups, totalling 300.
static u32 _gKyle800104KyleMadiganBodyPartVerts[] = {
#include "assets/kyle_madigan_body_partVerts.inc"
};

/// Body vertices in integer part-local coordinates, ordered by skeletal part.
static SVECTOR _gKyle800104KyleMadiganBodyVerts[] = {
#include "assets/kyle_madigan_body_verts.inc"
};

/// Body lighting normals with twelve fractional bits, indexed independently of vertices.
static SVECTOR _gKyle800104KyleMadiganBodyNormals[] = {
#include "assets/kyle_madigan_body_normals.inc"
};

/// Writable draw commands: twenty skeletal groups and a final pre-transformed group.
///
/// First model creation resolves handler slots in place; the last word is
/// `TMD_STREAM_END`. Projection commands fill the first primitive-buffer region.
static u32 _gKyle800104KyleMadiganBodyStream[] = {
#include "assets/kyle_madigan_body_stream.inc"
};

TmdSource gKyle800104KyleMadiganBody = {
    .handlersResolved = false,
    // Each half holds all textured faces and one untextured Gouraud triangle.
    .bufferHalfBytes     = (204 * sizeof(POLY_GT3)) + (193 * sizeof(POLY_GT4)) + sizeof(POLY_G3),
    .preXformRegionBytes = (80 * sizeof(POLY_GT3)) + (48 * sizeof(POLY_GT4)),
    .partCount           = ARRAY_SIZE(_gKyle800104KyleMadiganBodySkeleton),
    .partVertexCounts    = _gKyle800104KyleMadiganBodyPartVerts,
    .verts               = _gKyle800104KyleMadiganBodyVerts,
    .normals             = _gKyle800104KyleMadiganBodyNormals,
    .skeleton            = _gKyle800104KyleMadiganBodySkeleton,
    .stream              = _gKyle800104KyleMadiganBodyStream,
};

/// Identity rest transform for the hand's single root, copied into each model.
static TmdBone _gKyle800104KyleMadiganHandRightSkeleton[] = {
#include "assets/kyle_madigan_hand_right_skeleton.inc"
};

/// Vertex count for the single part-local hand group (23 vertices).
static u32 _gKyle800104KyleMadiganHandRightPartVerts[] = {
#include "assets/kyle_madigan_hand_right_partVerts.inc"
};

/// Right-hand vertices in integer coordinates local to the hand root.
static SVECTOR _gKyle800104KyleMadiganHandRightVerts[] = {
#include "assets/kyle_madigan_hand_right_verts.inc"
};

/// Hand lighting normals with twelve fractional bits, indexed independently of vertices.
static SVECTOR _gKyle800104KyleMadiganHandRightNormals[] = {
#include "assets/kyle_madigan_hand_right_normals.inc"
};

/// Writable draw commands for four textured Gouraud triangles and nineteen quads.
///
/// One skeletal group and an empty final group end in `TMD_STREAM_END`.
/// First model creation resolves the command handler slots in place.
static u32 _gKyle800104KyleMadiganHandRightStream[] = {
#include "assets/kyle_madigan_hand_right_stream.inc"
};

TmdSource gKyle800104KyleMadiganHandRight = {
    .handlersResolved = false,
    .bufferHalfBytes  = (4 * sizeof(POLY_GT3)) + (19 * sizeof(POLY_GT4)),
    // All faces transform directly from the single hand part.
    .preXformRegionBytes = 0,
    .partCount           = ARRAY_SIZE(_gKyle800104KyleMadiganHandRightSkeleton),
    .partVertexCounts    = _gKyle800104KyleMadiganHandRightPartVerts,
    .verts               = _gKyle800104KyleMadiganHandRightVerts,
    .normals             = _gKyle800104KyleMadiganHandRightNormals,
    .skeleton            = _gKyle800104KyleMadiganHandRightSkeleton,
    .stream              = _gKyle800104KyleMadiganHandRightStream,
};

/// Identity rest transform for the right hand's single root, copied into each model.
static TmdBone _gKyle800104KyleMadiganHandRightSecondPairSkeleton[] = {
#include "assets/kyle_madigan_hand_right_second_pair_skeleton.inc"
};

/// Vertex count for the single part-local right-hand group (27 vertices).
static u32 _gKyle800104KyleMadiganHandRightSecondPairPartVerts[] = {
#include "assets/kyle_madigan_hand_right_second_pair_partVerts.inc"
};

/// Right-hand vertices in integer coordinates local to the hand root.
static SVECTOR _gKyle800104KyleMadiganHandRightSecondPairVerts[] = {
#include "assets/kyle_madigan_hand_right_second_pair_verts.inc"
};

/// Hand lighting normals with twelve fractional bits, indexed independently of vertices.
static SVECTOR _gKyle800104KyleMadiganHandRightSecondPairNormals[] = {
#include "assets/kyle_madigan_hand_right_second_pair_normals.inc"
};

/// Writable draw commands for two textured Gouraud triangles and twenty-four quads.
///
/// One skeletal group and an empty final group end in `TMD_STREAM_END`.
/// First model creation resolves the command handler slots in place.
static u32 _gKyle800104KyleMadiganHandRightSecondPairStream[] = {
#include "assets/kyle_madigan_hand_right_second_pair_stream.inc"
};

TmdSource gKyle800104KyleMadiganHandRightSecondPair = {
    .handlersResolved = false,
    .bufferHalfBytes  = (2 * sizeof(POLY_GT3)) + (24 * sizeof(POLY_GT4)),
    // All faces transform directly from the single hand part.
    .preXformRegionBytes = 0,
    .partCount           = ARRAY_SIZE(_gKyle800104KyleMadiganHandRightSecondPairSkeleton),
    .partVertexCounts    = _gKyle800104KyleMadiganHandRightSecondPairPartVerts,
    .verts               = _gKyle800104KyleMadiganHandRightSecondPairVerts,
    .normals             = _gKyle800104KyleMadiganHandRightSecondPairNormals,
    .skeleton            = _gKyle800104KyleMadiganHandRightSecondPairSkeleton,
    .stream              = _gKyle800104KyleMadiganHandRightSecondPairStream,
};

/// Identity rest transform for the left hand's single root, copied into each model.
static TmdBone _gKyle800104KyleMadiganHandLeftFirstPairSkeleton[] = {
#include "assets/kyle_madigan_left_skeleton.inc"
};

/// Vertex count for the left hand's single part (23 vertices).
static u32 _gKyle800104KyleMadiganHandLeftFirstPairPartVerts[] = {
#include "assets/kyle_madigan_left_partVerts.inc"
};

/// Left-hand vertices in integer coordinates local to the hand root.
static SVECTOR _gKyle800104KyleMadiganHandLeftFirstPairVerts[] = {
#include "assets/kyle_madigan_left_verts.inc"
};

/// Hand lighting normals with twelve fractional bits, indexed independently of vertices.
static SVECTOR _gKyle800104KyleMadiganHandLeftFirstPairNormals[] = {
#include "assets/kyle_madigan_left_normals.inc"
};

/// Writable draw commands for four textured Gouraud triangles and nineteen quads.
///
/// One skeletal group and an empty final group end in `TMD_STREAM_END`.
/// First model creation resolves the command handler slots in place.
static u32 _gKyle800104KyleMadiganHandLeftFirstPairStream[] = {
#include "assets/kyle_madigan_left_stream.inc"
};

TmdSource gKyle800104KyleMadiganHandLeftFirstPair = {
    .handlersResolved = false,
    .bufferHalfBytes  = (4 * sizeof(POLY_GT3)) + (19 * sizeof(POLY_GT4)),
    // All faces transform directly from the single hand part.
    .preXformRegionBytes = 0,
    .partCount           = ARRAY_SIZE(_gKyle800104KyleMadiganHandLeftFirstPairSkeleton),
    .partVertexCounts    = _gKyle800104KyleMadiganHandLeftFirstPairPartVerts,
    .verts               = _gKyle800104KyleMadiganHandLeftFirstPairVerts,
    .normals             = _gKyle800104KyleMadiganHandLeftFirstPairNormals,
    .skeleton            = _gKyle800104KyleMadiganHandLeftFirstPairSkeleton,
    .stream              = _gKyle800104KyleMadiganHandLeftFirstPairStream,
};

/// Identity rest transform for the left hand's single root, copied into each model.
static TmdBone _gKyle800104KyleMadiganHandLeftSkeleton[] = {
#include "assets/kyle_madigan_hand_left_skeleton.inc"
};

/// Vertex count for the single part-local left-hand group (27 vertices).
static u32 _gKyle800104KyleMadiganHandLeftPartVerts[] = {
#include "assets/kyle_madigan_hand_left_partVerts.inc"
};

/// Left-hand vertices in integer coordinates local to the hand root.
static SVECTOR _gKyle800104KyleMadiganHandLeftVerts[] = {
#include "assets/kyle_madigan_hand_left_verts.inc"
};

/// Hand lighting normals with twelve fractional bits, indexed independently of vertices.
static SVECTOR _gKyle800104KyleMadiganHandLeftNormals[] = {
#include "assets/kyle_madigan_hand_left_normals.inc"
};

/// Writable draw commands for two textured Gouraud triangles and twenty-four quads.
///
/// One skeletal group and an empty final group end in `TMD_STREAM_END`.
/// First model creation resolves the command handler slots in place.
static u32 _gKyle800104KyleMadiganHandLeftStream[] = {
#include "assets/kyle_madigan_hand_left_stream.inc"
};

TmdSource gKyle800104KyleMadiganHandLeft = {
    .handlersResolved = false,
    .bufferHalfBytes  = (2 * sizeof(POLY_GT3)) + (24 * sizeof(POLY_GT4)),
    // All faces transform directly from the single hand part.
    .preXformRegionBytes = 0,
    .partCount           = ARRAY_SIZE(_gKyle800104KyleMadiganHandLeftSkeleton),
    .partVertexCounts    = _gKyle800104KyleMadiganHandLeftPartVerts,
    .verts               = _gKyle800104KyleMadiganHandLeftVerts,
    .normals             = _gKyle800104KyleMadiganHandLeftNormals,
    .skeleton            = _gKyle800104KyleMadiganHandLeftSkeleton,
    .stream              = _gKyle800104KyleMadiganHandLeftStream,
};

/// Identity rest transform for the gun's single root, copied into each model.
static TmdBone _gKyle800104KyleMadiganGunSkeleton[] = {
#include "assets/kyle_800104_model_062A8_skeleton.inc"
};

/// Vertex count for the single part-local gun group (78 vertices).
static u32 _gKyle800104KyleMadiganGunPartVerts[] = {
#include "assets/kyle_800104_model_062A8_partVerts.inc"
};

/// Gun vertices in integer coordinates local to the gun root.
static SVECTOR _gKyle800104KyleMadiganGunVerts[] = {
#include "assets/kyle_800104_model_062A8_verts.inc"
};

/// Gun lighting normals with twelve fractional bits, indexed independently of vertices.
static SVECTOR _gKyle800104KyleMadiganGunNormals[] = {
#include "assets/kyle_800104_model_062A8_normals.inc"
};

/// Writable draw commands for two textured Gouraud triangles and 73 quads.
///
/// One skeletal group and an empty final group end in `TMD_STREAM_END`.
/// First model creation resolves the command handler slots in place.
static u32 _gKyle800104KyleMadiganGunStream[] = {
#include "assets/kyle_800104_model_062A8_stream.inc"
};

TmdSource gKyle800104KyleMadiganGun = {
    .handlersResolved = false,
    .bufferHalfBytes  = (2 * sizeof(POLY_GT3)) + (73 * sizeof(POLY_GT4)),
    // All faces transform directly from the single gun part.
    .preXformRegionBytes = 0,
    .partCount           = ARRAY_SIZE(_gKyle800104KyleMadiganGunSkeleton),
    .partVertexCounts    = _gKyle800104KyleMadiganGunPartVerts,
    .verts               = _gKyle800104KyleMadiganGunVerts,
    .normals             = _gKyle800104KyleMadiganGunNormals,
    .skeleton            = _gKyle800104KyleMadiganGunSkeleton,
    .stream              = _gKyle800104KyleMadiganGunStream,
};

/// Serialized encodings selecting the two pose-bank formats used by these clips.
enum {
    KYLE_800104_ANIMATION_TRANSLATION_ROTATION_BANK = 1,
    KYLE_800104_ANIMATION_PACKED_ROTATION_BANK      = 4
};

/// Idle translation/rotation keys for tracks 0 and 1: integer positions and 1/4096-turn angles.
static AnimationPackedPose _gKyle800104KyleMadiganIdleAnimationBank1[] = {
#include "assets/kyle_800104_animation_06D24_bank1.inc"
};

/// Idle rotation-only keys for tracks 2..19, in eight-unit steps over 4096 units per turn.
static AnimationPackedRotation _gKyle800104KyleMadiganIdleAnimationBank4[] = {
#include "assets/kyle_800104_animation_06D24_bank4.inc"
};

/// Idle keyframes and controls: pose offsets count four-byte words; durations count normal-rate frames.
static AnimationRecord _gKyle800104KyleMadiganIdleAnimationRecords[] = {
#include "assets/kyle_800104_animation_06D24_records.inc"
};

/// Absolute record-array starts for the idle animation's twenty skeletal tracks (0..19).
static u16 _gKyle800104KyleMadiganIdleAnimationIndices[] = {
#include "assets/kyle_800104_animation_06D24_indices.inc"
};

/// Kyle Madigan's looping idle animation.
///
/// Twenty tracks: 0 and 1 carry translation and rotation, 2..19 rotation only.
/// Child playback drives tracks 1..19; keep this package loaded while slots
/// borrow the descriptor, records, indices or pose banks.
/// Several other bank slots reuse this idle clip.
static AnimationSet _gKyle800104KyleMadiganIdleAnimation = {
    .records           = _gKyle800104KyleMadiganIdleAnimationRecords,
    .trackStartIndices = _gKyle800104KyleMadiganIdleAnimationIndices,
    .poseBanks         = {
        [KYLE_800104_ANIMATION_TRANSLATION_ROTATION_BANK] = _gKyle800104KyleMadiganIdleAnimationBank1,
        [KYLE_800104_ANIMATION_PACKED_ROTATION_BANK]      = _gKyle800104KyleMadiganIdleAnimationBank4,
    },
};

/// Unarmed walk translation/rotation keys for tracks 0 and 1: integer positions and 1/4096-turn angles.
static AnimationPackedPose _gKyle800104KyleMadiganUnarmedWalkAnimationBank1[] = {
#include "assets/kyle_800104_animation_075A4_bank1.inc"
};

/// Unarmed walk rotation-only keys for tracks 2..19, in eight-unit steps over 4096 units per turn.
static AnimationPackedRotation _gKyle800104KyleMadiganUnarmedWalkAnimationBank4[] = {
#include "assets/kyle_800104_animation_075A4_bank4.inc"
};

/// Unarmed walk keyframes and controls: pose offsets count four-byte words; durations count normal-rate frames.
static AnimationRecord _gKyle800104KyleMadiganUnarmedWalkAnimationRecords[] = {
#include "assets/kyle_800104_animation_075A4_records.inc"
};

/// Absolute record-array starts for the unarmed walk animation's twenty skeletal tracks (0..19).
static u16 _gKyle800104KyleMadiganUnarmedWalkAnimationIndices[] = {
#include "assets/kyle_800104_animation_075A4_indices.inc"
};

/// Kyle Madigan's looping walk animation when no held weapon model is equipped.
///
/// Twenty tracks: 0 and 1 carry translation and rotation, 2..19 rotation only.
/// Child playback drives tracks 1..19; keep this package loaded while slots
/// borrow the descriptor, records, indices or pose banks.
static AnimationSet _gKyle800104KyleMadiganUnarmedWalkAnimation = {
    .records           = _gKyle800104KyleMadiganUnarmedWalkAnimationRecords,
    .trackStartIndices = _gKyle800104KyleMadiganUnarmedWalkAnimationIndices,
    .poseBanks         = {
        [KYLE_800104_ANIMATION_TRANSLATION_ROTATION_BANK] = _gKyle800104KyleMadiganUnarmedWalkAnimationBank1,
        [KYLE_800104_ANIMATION_PACKED_ROTATION_BANK]      = _gKyle800104KyleMadiganUnarmedWalkAnimationBank4,
    },
};

/// Left-turn translation/rotation keys for tracks 0 and 1: integer positions and 1/4096-turn angles.
static AnimationPackedPose _gKyle800104KyleMadiganTurnLeftAnimationBank1[] = {
#include "assets/kyle_800104_animation_07E10_bank1.inc"
};

/// Left-turn rotation-only keys for tracks 2..19, in eight-unit steps over 4096 units per turn.
static AnimationPackedRotation _gKyle800104KyleMadiganTurnLeftAnimationBank4[] = {
#include "assets/kyle_800104_animation_07E10_bank4.inc"
};

/// Left-turn keyframes and controls: pose offsets count four-byte words; durations count normal-rate frames.
static AnimationRecord _gKyle800104KyleMadiganTurnLeftAnimationRecords[] = {
#include "assets/kyle_800104_animation_07E10_records.inc"
};

/// Absolute record-array starts for the left turn animation's twenty skeletal tracks (0..19).
static u16 _gKyle800104KyleMadiganTurnLeftAnimationIndices[] = {
#include "assets/kyle_800104_animation_07E10_indices.inc"
};

/// Kyle Madigan's looping left-turn animation.
///
/// Twenty tracks: 0 and 1 carry translation and rotation, 2..19 rotation only.
/// Child playback drives tracks 1..19; keep this package loaded while slots
/// borrow the descriptor, records, indices or pose banks.
static AnimationSet _gKyle800104KyleMadiganTurnLeftAnimation = {
    .records           = _gKyle800104KyleMadiganTurnLeftAnimationRecords,
    .trackStartIndices = _gKyle800104KyleMadiganTurnLeftAnimationIndices,
    .poseBanks         = {
        [KYLE_800104_ANIMATION_TRANSLATION_ROTATION_BANK] = _gKyle800104KyleMadiganTurnLeftAnimationBank1,
        [KYLE_800104_ANIMATION_PACKED_ROTATION_BANK]      = _gKyle800104KyleMadiganTurnLeftAnimationBank4,
    },
};

/// Right-turn translation/rotation keys for tracks 0 and 1: integer positions and 1/4096-turn angles.
static AnimationPackedPose _gKyle800104KyleMadiganTurnRightAnimationBank1[] = {
#include "assets/kyle_800104_animation_086E4_bank1.inc"
};

/// Right-turn rotation-only keys for tracks 2..19, in eight-unit steps over 4096 units per turn.
static AnimationPackedRotation _gKyle800104KyleMadiganTurnRightAnimationBank4[] = {
#include "assets/kyle_800104_animation_086E4_bank4.inc"
};

/// Right-turn keyframes and controls: pose offsets count four-byte words; durations count normal-rate frames.
static AnimationRecord _gKyle800104KyleMadiganTurnRightAnimationRecords[] = {
#include "assets/kyle_800104_animation_086E4_records.inc"
};

/// Absolute record-array starts for the right turn animation's twenty skeletal tracks (0..19).
static u16 _gKyle800104KyleMadiganTurnRightAnimationIndices[] = {
#include "assets/kyle_800104_animation_086E4_indices.inc"
};

/// Kyle Madigan's looping right-turn animation.
///
/// Twenty tracks: 0 and 1 carry translation and rotation, 2..19 rotation only.
/// Child playback drives tracks 1..19; keep this package loaded while slots
/// borrow the descriptor, records, indices or pose banks.
static AnimationSet _gKyle800104KyleMadiganTurnRightAnimation = {
    .records           = _gKyle800104KyleMadiganTurnRightAnimationRecords,
    .trackStartIndices = _gKyle800104KyleMadiganTurnRightAnimationIndices,
    .poseBanks         = {
        [KYLE_800104_ANIMATION_TRANSLATION_ROTATION_BANK] = _gKyle800104KyleMadiganTurnRightAnimationBank1,
        [KYLE_800104_ANIMATION_PACKED_ROTATION_BANK]      = _gKyle800104KyleMadiganTurnRightAnimationBank4,
    },
};

/// Part-4 hit translation/rotation keys for tracks 0 and 1: integer positions and 1/4096-turn angles.
static AnimationPackedPose _gKyle800104KyleMadiganPart4HitAnimationBank1[] = {
#include "assets/kyle_800104_animation_08BA4_bank1.inc"
};

/// Part-4 hit rotation-only keys for tracks 2..19, in eight-unit steps over 4096 units per turn.
static AnimationPackedRotation _gKyle800104KyleMadiganPart4HitAnimationBank4[] = {
#include "assets/kyle_800104_animation_08BA4_bank4.inc"
};

/// Part-4 hit keyframes and controls: pose offsets count four-byte words; durations count normal-rate frames.
static AnimationRecord _gKyle800104KyleMadiganPart4HitAnimationRecords[] = {
#include "assets/kyle_800104_animation_08BA4_records.inc"
};

/// Absolute record-array starts for the part-4 hit animation's twenty skeletal tracks (0..19).
static u16 _gKyle800104KyleMadiganPart4HitAnimationIndices[] = {
#include "assets/kyle_800104_animation_08BA4_indices.inc"
};

/// Kyle Madigan's non-looping hit reaction for collision on body part 4.
///
/// Twenty tracks: 0 and 1 carry translation and rotation, 2..19 rotation only.
/// Child playback drives tracks 1..19; keep this package loaded while slots
/// borrow the descriptor, records, indices or pose banks.
static AnimationSet _gKyle800104KyleMadiganPart4HitAnimation = {
    .records           = _gKyle800104KyleMadiganPart4HitAnimationRecords,
    .trackStartIndices = _gKyle800104KyleMadiganPart4HitAnimationIndices,
    .poseBanks         = {
        [KYLE_800104_ANIMATION_TRANSLATION_ROTATION_BANK] = _gKyle800104KyleMadiganPart4HitAnimationBank1,
        [KYLE_800104_ANIMATION_PACKED_ROTATION_BANK]      = _gKyle800104KyleMadiganPart4HitAnimationBank4,
    },
};

/// Other-body hit translation/rotation keys: integer positions and 1/4096-turn angles.
static const AnimationPackedPose _gKyle800104KyleMadiganOtherBodyHitAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/kyle_800104_animation_09148_bank1.inc"
};

/// Other-body hit rotation-only keys, in eight-unit steps over 4096 units per turn.
static const AnimationPackedRotation _gKyle800104KyleMadiganOtherBodyHitAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/kyle_800104_animation_09148_bank4.inc"
};

/// Other-body hit keyframes and stop controls; pose offsets count words, durations normal-rate frames.
static const AnimationRecord _gKyle800104KyleMadiganOtherBodyHitAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/kyle_800104_animation_09148_records.inc"
};

/// Absolute record-array starts for the other-body hit animation's twenty tracks (0..19).
static const u16 _gKyle800104KyleMadiganOtherBodyHitAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/kyle_800104_animation_09148_indices.inc"
};
STATIC_ASSERT(ARRAY_SIZE(_gKyle800104KyleMadiganOtherBodyHitAnimationIndices) == ARRAY_SIZE(_gKyle800104KyleMadiganBodySkeleton), kyle800104_other_body_hit_track_count);

/// Kyle Madigan's non-looping hit reaction for collision on body parts other than part 4.
///
/// Tracks 0 and 1 encode translation and rotation; 2..19 encode rotation only.
/// Native damage playback blends child tracks 1..19, then returns to idle.
/// Keep this package loaded while playback borrows the descriptor or its data.
static AnimationSet _gKyle800104KyleMadiganOtherBodyHitAnimation = {
    .records           = _gKyle800104KyleMadiganOtherBodyHitAnimationRecords,
    .trackStartIndices = _gKyle800104KyleMadiganOtherBodyHitAnimationIndices,
    .poseBanks         = {
        [KYLE_800104_ANIMATION_TRANSLATION_ROTATION_BANK] = _gKyle800104KyleMadiganOtherBodyHitAnimationBank1,
        [KYLE_800104_ANIMATION_PACKED_ROTATION_BANK]      = _gKyle800104KyleMadiganOtherBodyHitAnimationBank4,
    },
};

/// Death translation/rotation keys: integer positions and 1/4096-turn angles.
static const AnimationPackedPose _gKyle800104KyleMadiganDeathAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/kyle_800104_animation_096B8_bank1.inc"
};

/// Death rotation-only keys, in eight-unit steps over 4096 units per turn.
static const AnimationPackedRotation _gKyle800104KyleMadiganDeathAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/kyle_800104_animation_096B8_bank4.inc"
};

/// Death keyframes and stop controls; pose offsets count words, durations normal-rate frames.
static const AnimationRecord _gKyle800104KyleMadiganDeathAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/kyle_800104_animation_096B8_records.inc"
};

/// Absolute record-array starts for the death animation's twenty tracks (0..19).
static const u16 _gKyle800104KyleMadiganDeathAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/kyle_800104_animation_096B8_indices.inc"
};
STATIC_ASSERT(ARRAY_SIZE(_gKyle800104KyleMadiganDeathAnimationIndices) == ARRAY_SIZE(_gKyle800104KyleMadiganBodySkeleton), kyle800104_death_track_count);

/// Kyle Madigan's non-looping death animation, retaining its final pose.
///
/// Tracks 0 and 1 encode translation and rotation; 2..19 encode rotation only.
/// At zero HP, native or scripted behavior resets child tracks 1..19 to this
/// clip and enters the stopped damage state. Keep this package loaded while
/// playback borrows the descriptor or its data.
static AnimationSet _gKyle800104KyleMadiganDeathAnimation = {
    .records           = _gKyle800104KyleMadiganDeathAnimationRecords,
    .trackStartIndices = _gKyle800104KyleMadiganDeathAnimationIndices,
    .poseBanks         = {
        [KYLE_800104_ANIMATION_TRANSLATION_ROTATION_BANK] = _gKyle800104KyleMadiganDeathAnimationBank1,
        [KYLE_800104_ANIMATION_PACKED_ROTATION_BANK]      = _gKyle800104KyleMadiganDeathAnimationBank4,
    },
};

/// Idle-variation translation/rotation keys: integer positions and 1/4096-turn angles.
static const AnimationPackedPose _gKyle800104KyleMadiganIdleVariationAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/kyle_800104_animation_09EC4_bank1.inc"
};

/// Idle-variation rotation-only keys, in eight-unit steps over 4096 units per turn.
static const AnimationPackedRotation _gKyle800104KyleMadiganIdleVariationAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/kyle_800104_animation_09EC4_bank4.inc"
};

/// Idle-variation keyframes and loop controls; pose offsets count words, durations normal-rate frames.
static const AnimationRecord _gKyle800104KyleMadiganIdleVariationAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/kyle_800104_animation_09EC4_records.inc"
};

/// Absolute record-array starts for the idle variation's twenty tracks (0..19).
static const u16 _gKyle800104KyleMadiganIdleVariationAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/kyle_800104_animation_09EC4_indices.inc"
};
STATIC_ASSERT(ARRAY_SIZE(_gKyle800104KyleMadiganIdleVariationAnimationIndices) == ARRAY_SIZE(_gKyle800104KyleMadiganBodySkeleton), kyle800104_idle_variation_track_count);

/// Kyle Madigan's looping variation of the native idle animation.
///
/// Tracks 0 and 1 encode translation and rotation; 2..19 encode rotation only.
/// Idle decisions select this clip for child tracks 1..19 once per idle spell.
/// Keep this package loaded while playback borrows the descriptor or its data.
static AnimationSet _gKyle800104KyleMadiganIdleVariationAnimation = {
    .records           = _gKyle800104KyleMadiganIdleVariationAnimationRecords,
    .trackStartIndices = _gKyle800104KyleMadiganIdleVariationAnimationIndices,
    .poseBanks         = {
        [KYLE_800104_ANIMATION_TRANSLATION_ROTATION_BANK] = _gKyle800104KyleMadiganIdleVariationAnimationBank1,
        [KYLE_800104_ANIMATION_PACKED_ROTATION_BANK]      = _gKyle800104KyleMadiganIdleVariationAnimationBank4,
    },
};

/// Slot-32 translation/rotation keys: integer positions and 1/4096-turn angles; clip role unproven.
static const AnimationPackedPose _gKyle800104Animation0A394Bank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/kyle_800104_animation_0A394_bank1.inc"
};

/// Slot-32 rotation-only keys, in eight-unit steps over 4096 units per turn; clip role unproven.
static const AnimationPackedRotation _gKyle800104Animation0A394Bank4[] __attribute__((section(".data"))) = {
#include "assets/kyle_800104_animation_0A394_bank4.inc"
};

/// Slot-32 keyframes and stop controls; word pose offsets and normal-rate frame durations; clip role unproven.
static const AnimationRecord _gKyle800104Animation0A394Records[] __attribute__((section(".data"))) = {
#include "assets/kyle_800104_animation_0A394_records.inc"
};

/// Absolute record-array starts for slot 32's twenty tracks (0..19); clip role unproven.
static const u16 _gKyle800104Animation0A394Indices[] __attribute__((section(".data"))) = {
#include "assets/kyle_800104_animation_0A394_indices.inc"
};
STATIC_ASSERT(ARRAY_SIZE(_gKyle800104Animation0A394Indices) == ARRAY_SIZE(_gKyle800104KyleMadiganBodySkeleton), kyle800104_slot32_track_count);

/// Non-looping clip in Kyle Madigan's bank slot 32; its role is unproven.
///
/// Tracks 0 and 1 encode translation and rotation; 2..19 encode rotation only.
/// Keep this package loaded while playback borrows the descriptor or its data.
static AnimationSet _gKyle800104Animation0A394 = {
    .records           = _gKyle800104Animation0A394Records,
    .trackStartIndices = _gKyle800104Animation0A394Indices,
    .poseBanks         = {
        [KYLE_800104_ANIMATION_TRANSLATION_ROTATION_BANK] = _gKyle800104Animation0A394Bank1,
        [KYLE_800104_ANIMATION_PACKED_ROTATION_BANK]      = _gKyle800104Animation0A394Bank4,
    },
};

/// Slot-33 translation/rotation keys: integer positions and 1/4096-turn angles; clip role unproven.
static const AnimationPackedPose _gKyle800104Animation0A8ECBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/kyle_800104_animation_0A8EC_bank1.inc"
};

/// Slot-33 rotation-only keys, in eight-unit steps over 4096 units per turn; clip role unproven.
static const AnimationPackedRotation _gKyle800104Animation0A8ECBank4[] __attribute__((section(".data"))) = {
#include "assets/kyle_800104_animation_0A8EC_bank4.inc"
};

/// Slot-33 keyframes and stop controls; word pose offsets and normal-rate frame durations; clip role unproven.
static const AnimationRecord _gKyle800104Animation0A8ECRecords[] __attribute__((section(".data"))) = {
#include "assets/kyle_800104_animation_0A8EC_records.inc"
};

/// Absolute record-array starts for slot 33's twenty tracks (0..19); clip role unproven.
static const u16 _gKyle800104Animation0A8ECIndices[] __attribute__((section(".data"))) = {
#include "assets/kyle_800104_animation_0A8EC_indices.inc"
};
STATIC_ASSERT(ARRAY_SIZE(_gKyle800104Animation0A8ECIndices) == ARRAY_SIZE(_gKyle800104KyleMadiganBodySkeleton), kyle800104_slot33_track_count);

/// Non-looping clip in Kyle Madigan's bank slot 33; its role is unproven.
///
/// Tracks 0 and 1 encode translation and rotation; 2..19 encode rotation only.
/// Keep this package loaded while playback borrows the descriptor or its data.
static AnimationSet _gKyle800104Animation0A8EC = {
    .records           = _gKyle800104Animation0A8ECRecords,
    .trackStartIndices = _gKyle800104Animation0A8ECIndices,
    .poseBanks         = {
        [KYLE_800104_ANIMATION_TRANSLATION_ROTATION_BANK] = _gKyle800104Animation0A8ECBank1,
        [KYLE_800104_ANIMATION_PACKED_ROTATION_BANK]      = _gKyle800104Animation0A8ECBank4,
    },
};

/// Combat-hold translation/rotation keys: integer positions and 1/4096-turn angles.
static const AnimationPackedPose _gKyle800104KyleMadiganCombatHoldAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/kyle_800104_animation_0AADC_bank1.inc"
};

/// Combat-hold rotation-only keys, in eight-unit steps over 4096 units per turn.
static const AnimationPackedRotation _gKyle800104KyleMadiganCombatHoldAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/kyle_800104_animation_0AADC_bank4.inc"
};

/// Combat-hold keyframes and loop controls; pose offsets count words, durations normal-rate frames.
static const AnimationRecord _gKyle800104KyleMadiganCombatHoldAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/kyle_800104_animation_0AADC_records.inc"
};

/// Absolute record-array starts for the combat hold's twenty tracks (0..19).
static const u16 _gKyle800104KyleMadiganCombatHoldAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/kyle_800104_animation_0AADC_indices.inc"
};
STATIC_ASSERT(ARRAY_SIZE(_gKyle800104KyleMadiganCombatHoldAnimationIndices) == ARRAY_SIZE(_gKyle800104KyleMadiganBodySkeleton), kyle800104_combat_hold_track_count);

/// Kyle Madigan's looping held-weapon pose between combat actions.
///
/// Tracks 0 and 1 encode translation and rotation; 2..19 encode rotation only.
/// Child tracks 1..19 hold the ready pose after combat entry, between attacks
/// and at the start of a rifle/Pyke attack. Keep this package loaded while
/// playback borrows the descriptor or its data.
static AnimationSet _gKyle800104KyleMadiganCombatHoldAnimation = {
    .records           = _gKyle800104KyleMadiganCombatHoldAnimationRecords,
    .trackStartIndices = _gKyle800104KyleMadiganCombatHoldAnimationIndices,
    .poseBanks         = {
        [KYLE_800104_ANIMATION_TRANSLATION_ROTATION_BANK] = _gKyle800104KyleMadiganCombatHoldAnimationBank1,
        [KYLE_800104_ANIMATION_PACKED_ROTATION_BANK]      = _gKyle800104KyleMadiganCombatHoldAnimationBank4,
    },
};

static AnimationPackedPose _gKyle800104Animation0B214Bank1[13] = {
#include "assets/kyle_800104_animation_0B214_bank1.inc"
};

static AnimationPackedRotation _gKyle800104Animation0B214Bank4[167] = {
#include "assets/kyle_800104_animation_0B214_bank4.inc"
};

static AnimationRecord _gKyle800104Animation0B214Records[236] = {
#include "assets/kyle_800104_animation_0B214_records.inc"
};

static u16 _gKyle800104Animation0B214Indices[20] = {
#include "assets/kyle_800104_animation_0B214_indices.inc"
};

static AnimationSet _gKyle800104Animation0B214 = {
    _gKyle800104Animation0B214Records,
    _gKyle800104Animation0B214Indices,
    { NULL, _gKyle800104Animation0B214Bank1, NULL, NULL, _gKyle800104Animation0B214Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gKyle800104Animation0BC24Bank1[16] = {
#include "assets/kyle_800104_animation_0BC24_bank1.inc"
};

static AnimationPackedRotation _gKyle800104Animation0BC24Bank4[250] = {
#include "assets/kyle_800104_animation_0BC24_bank4.inc"
};

static AnimationRecord _gKyle800104Animation0BC24Records[326] = {
#include "assets/kyle_800104_animation_0BC24_records.inc"
};

static u16 _gKyle800104Animation0BC24Indices[20] = {
#include "assets/kyle_800104_animation_0BC24_indices.inc"
};

static AnimationSet _gKyle800104Animation0BC24 = {
    _gKyle800104Animation0BC24Records,
    _gKyle800104Animation0BC24Indices,
    { NULL, _gKyle800104Animation0BC24Bank1, NULL, NULL, _gKyle800104Animation0BC24Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gKyle800104Animation0C56CBank1[16] = {
#include "assets/kyle_800104_animation_0C56C_bank1.inc"
};

static AnimationPackedRotation _gKyle800104Animation0C56CBank4[232] = {
#include "assets/kyle_800104_animation_0C56C_bank4.inc"
};

static AnimationRecord _gKyle800104Animation0C56CRecords[294] = {
#include "assets/kyle_800104_animation_0C56C_records.inc"
};

static u16 _gKyle800104Animation0C56CIndices[20] = {
#include "assets/kyle_800104_animation_0C56C_indices.inc"
};

static AnimationSet _gKyle800104Animation0C56C = {
    _gKyle800104Animation0C56CRecords,
    _gKyle800104Animation0C56CIndices,
    { NULL, _gKyle800104Animation0C56CBank1, NULL, NULL, _gKyle800104Animation0C56CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gKyle800104Animation0C860Bank1[4] = {
#include "assets/kyle_800104_animation_0C860_bank1.inc"
};

static AnimationPackedRotation _gKyle800104Animation0C860Bank4[56] = {
#include "assets/kyle_800104_animation_0C860_bank4.inc"
};

static AnimationRecord _gKyle800104Animation0C860Records[101] = {
#include "assets/kyle_800104_animation_0C860_records.inc"
};

static u16 _gKyle800104Animation0C860Indices[20] = {
#include "assets/kyle_800104_animation_0C860_indices.inc"
};

static AnimationSet _gKyle800104Animation0C860 = {
    _gKyle800104Animation0C860Records,
    _gKyle800104Animation0C860Indices,
    { NULL, _gKyle800104Animation0C860Bank1, NULL, NULL, _gKyle800104Animation0C860Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gKyle800104Animation0CB60Bank1[4] = {
#include "assets/kyle_800104_animation_0CB60_bank1.inc"
};

static AnimationPackedRotation _gKyle800104Animation0CB60Bank4[48] = {
#include "assets/kyle_800104_animation_0CB60_bank4.inc"
};

static AnimationRecord _gKyle800104Animation0CB60Records[112] = {
#include "assets/kyle_800104_animation_0CB60_records.inc"
};

static u16 _gKyle800104Animation0CB60Indices[20] = {
#include "assets/kyle_800104_animation_0CB60_indices.inc"
};

static AnimationSet _gKyle800104Animation0CB60 = {
    _gKyle800104Animation0CB60Records,
    _gKyle800104Animation0CB60Indices,
    { NULL, _gKyle800104Animation0CB60Bank1, NULL, NULL, _gKyle800104Animation0CB60Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gKyle800104Animation0D07CBank1[8] = {
#include "assets/kyle_800104_animation_0D07C_bank1.inc"
};

static AnimationPackedRotation _gKyle800104Animation0D07CBank4[123] = {
#include "assets/kyle_800104_animation_0D07C_bank4.inc"
};

static AnimationRecord _gKyle800104Animation0D07CRecords[160] = {
#include "assets/kyle_800104_animation_0D07C_records.inc"
};

static u16 _gKyle800104Animation0D07CIndices[20] = {
#include "assets/kyle_800104_animation_0D07C_indices.inc"
};

static AnimationSet _gKyle800104Animation0D07C = {
    _gKyle800104Animation0D07CRecords,
    _gKyle800104Animation0D07CIndices,
    { NULL, _gKyle800104Animation0D07CBank1, NULL, NULL, _gKyle800104Animation0D07CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gKyle800104Animation0D5F0Bank1[11] = {
#include "assets/kyle_800104_animation_0D5F0_bank1.inc"
};

static AnimationPackedRotation _gKyle800104Animation0D5F0Bank4[129] = {
#include "assets/kyle_800104_animation_0D5F0_bank4.inc"
};

static AnimationRecord _gKyle800104Animation0D5F0Records[167] = {
#include "assets/kyle_800104_animation_0D5F0_records.inc"
};

static u16 _gKyle800104Animation0D5F0Indices[20] = {
#include "assets/kyle_800104_animation_0D5F0_indices.inc"
};

static AnimationSet _gKyle800104Animation0D5F0 = {
    _gKyle800104Animation0D5F0Records,
    _gKyle800104Animation0D5F0Indices,
    { NULL, _gKyle800104Animation0D5F0Bank1, NULL, NULL, _gKyle800104Animation0D5F0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gKyle800104Animation0DE84Bank1[11] = {
#include "assets/kyle_800104_animation_0DE84_bank1.inc"
};

static AnimationPackedRotation _gKyle800104Animation0DE84Bank4[206] = {
#include "assets/kyle_800104_animation_0DE84_bank4.inc"
};

static AnimationRecord _gKyle800104Animation0DE84Records[290] = {
#include "assets/kyle_800104_animation_0DE84_records.inc"
};

static u16 _gKyle800104Animation0DE84Indices[20] = {
#include "assets/kyle_800104_animation_0DE84_indices.inc"
};

static AnimationSet _gKyle800104Animation0DE84 = {
    _gKyle800104Animation0DE84Records,
    _gKyle800104Animation0DE84Indices,
    { NULL, _gKyle800104Animation0DE84Bank1, NULL, NULL, _gKyle800104Animation0DE84Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gKyle800104Animation0E5D8Bank1[12] = {
#include "assets/kyle_800104_animation_0E5D8_bank1.inc"
};

static AnimationPackedRotation _gKyle800104Animation0E5D8Bank4[165] = {
#include "assets/kyle_800104_animation_0E5D8_bank4.inc"
};

static AnimationRecord _gKyle800104Animation0E5D8Records[248] = {
#include "assets/kyle_800104_animation_0E5D8_records.inc"
};

static u16 _gKyle800104Animation0E5D8Indices[20] = {
#include "assets/kyle_800104_animation_0E5D8_indices.inc"
};

static AnimationSet _gKyle800104Animation0E5D8 = {
    _gKyle800104Animation0E5D8Records,
    _gKyle800104Animation0E5D8Indices,
    { NULL, _gKyle800104Animation0E5D8Bank1, NULL, NULL, _gKyle800104Animation0E5D8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gKyle800104Animation0F184Bank1[17] = {
#include "assets/kyle_800104_animation_0F184_bank1.inc"
};

static AnimationPackedRotation _gKyle800104Animation0F184Bank4[298] = {
#include "assets/kyle_800104_animation_0F184_bank4.inc"
};

static AnimationRecord _gKyle800104Animation0F184Records[378] = {
#include "assets/kyle_800104_animation_0F184_records.inc"
};

static u16 _gKyle800104Animation0F184Indices[20] = {
#include "assets/kyle_800104_animation_0F184_indices.inc"
};

static AnimationSet _gKyle800104Animation0F184 = {
    _gKyle800104Animation0F184Records,
    _gKyle800104Animation0F184Indices,
    { NULL, _gKyle800104Animation0F184Bank1, NULL, NULL, _gKyle800104Animation0F184Bank4, NULL, NULL, NULL },
};

AnimationBank D_kyle_800104_80176C1C = { { {
    NULL,
    &_gKyle800104KyleMadiganIdleAnimation,
    &_gKyle800104Animation0DE84,
    &_gKyle800104Animation0E5D8,
    &_gKyle800104Animation0F184,
    &_gKyle800104KyleMadiganTurnLeftAnimation,
    &_gKyle800104KyleMadiganTurnRightAnimation,
    &_gKyle800104Animation0D07C,
    &_gKyle800104Animation0D5F0,
    &_gKyle800104KyleMadiganCombatHoldAnimation,
    &_gKyle800104Animation0C860,
    &_gKyle800104Animation0CB60,
    &_gKyle800104Animation0BC24,
    &_gKyle800104Animation0B214,
    &_gKyle800104Animation0C56C,
    &_gKyle800104KyleMadiganIdleAnimation,
    &_gKyle800104KyleMadiganPart4HitAnimation,
    &_gKyle800104KyleMadiganOtherBodyHitAnimation,
    &_gKyle800104KyleMadiganDeathAnimation,
    &_gKyle800104KyleMadiganUnarmedWalkAnimation,
    &_gKyle800104Animation0C56C,
    &_gKyle800104KyleMadiganIdleAnimation,
    &_gKyle800104KyleMadiganIdleAnimation,
    &_gKyle800104KyleMadiganIdleVariationAnimation,
    &_gKyle800104KyleMadiganIdleAnimation,
    &_gKyle800104KyleMadiganIdleAnimation,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    &_gKyle800104Animation0A394,
    &_gKyle800104Animation0A8EC,
    &_gKyle800104Animation0D07C,
    &_gKyle800104Animation0D5F0,
    &_gKyle800104KyleMadiganIdleAnimation,
    &_gKyle800104KyleMadiganIdleAnimation,
    &_gKyle800104KyleMadiganIdleAnimation,
    &_gKyle800104KyleMadiganIdleAnimation,
    &_gKyle800104KyleMadiganIdleAnimation,
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
