#include "kyle/kyle.h"

#include <psyq/sys/types.h>
#include <psyq/libgpu.h>

#include "types.h"

/// Initial part-local pose and parent indices copied into each body model.
static TmdBone _gKyle800101KyleMadiganBodySkeleton[] = {
#include "assets/kyle_madigan_body_skeleton.inc"
};

/// Vertex counts for the twenty consecutive part-local groups, totalling 300.
static u32 _gKyle800101KyleMadiganBodyPartVerts[] = {
#include "assets/kyle_madigan_body_partVerts.inc"
};

/// Body vertices in integer part-local coordinates, ordered by skeletal part.
static SVECTOR _gKyle800101KyleMadiganBodyVerts[] = {
#include "assets/kyle_madigan_body_verts.inc"
};

/// Body lighting normals with twelve fractional bits, indexed independently of vertices.
static SVECTOR _gKyle800101KyleMadiganBodyNormals[] = {
#include "assets/kyle_madigan_body_normals.inc"
};

/// Writable draw commands: twenty skeletal groups and a final pre-transformed group.
///
/// First model creation resolves handler slots in place; the last word is
/// `TMD_STREAM_END`. Projection commands fill the first primitive-buffer region.
static u32 _gKyle800101KyleMadiganBodyStream[] = {
#include "assets/kyle_madigan_body_stream.inc"
};

TmdSource gKyle800101KyleMadiganBody = {
    .handlersResolved = false,
    // Each half holds all textured faces and one untextured Gouraud triangle.
    .bufferHalfBytes     = (204 * sizeof(POLY_GT3)) + (193 * sizeof(POLY_GT4)) + sizeof(POLY_G3),
    .preXformRegionBytes = (80 * sizeof(POLY_GT3)) + (48 * sizeof(POLY_GT4)),
    .partCount           = ARRAY_SIZE(_gKyle800101KyleMadiganBodySkeleton),
    .partVertexCounts    = _gKyle800101KyleMadiganBodyPartVerts,
    .verts               = _gKyle800101KyleMadiganBodyVerts,
    .normals             = _gKyle800101KyleMadiganBodyNormals,
    .skeleton            = _gKyle800101KyleMadiganBodySkeleton,
    .stream              = _gKyle800101KyleMadiganBodyStream,
};

/// Identity rest transform for the hand's single root, copied into each model.
static TmdBone _gKyle800101KyleMadiganHandRightSkeleton[] = {
#include "assets/kyle_madigan_hand_right_skeleton.inc"
};

/// Vertex count for the single part-local hand group (23 vertices).
static u32 _gKyle800101KyleMadiganHandRightPartVerts[] = {
#include "assets/kyle_madigan_hand_right_partVerts.inc"
};

/// Right-hand vertices in integer coordinates local to the hand root.
static SVECTOR _gKyle800101KyleMadiganHandRightVerts[] = {
#include "assets/kyle_madigan_hand_right_verts.inc"
};

/// Hand lighting normals with twelve fractional bits, indexed independently of vertices.
static SVECTOR _gKyle800101KyleMadiganHandRightNormals[] = {
#include "assets/kyle_madigan_hand_right_normals.inc"
};

/// Writable draw commands for four textured Gouraud triangles and nineteen quads.
///
/// One skeletal group and an empty final group end in `TMD_STREAM_END`.
/// First model creation resolves the command handler slots in place.
static u32 _gKyle800101KyleMadiganHandRightStream[] = {
#include "assets/kyle_madigan_hand_right_stream.inc"
};

TmdSource gKyle800101KyleMadiganHandRight = {
    .handlersResolved = false,
    .bufferHalfBytes  = (4 * sizeof(POLY_GT3)) + (19 * sizeof(POLY_GT4)),
    // All faces transform directly from the single hand part.
    .preXformRegionBytes = 0,
    .partCount           = ARRAY_SIZE(_gKyle800101KyleMadiganHandRightSkeleton),
    .partVertexCounts    = _gKyle800101KyleMadiganHandRightPartVerts,
    .verts               = _gKyle800101KyleMadiganHandRightVerts,
    .normals             = _gKyle800101KyleMadiganHandRightNormals,
    .skeleton            = _gKyle800101KyleMadiganHandRightSkeleton,
    .stream              = _gKyle800101KyleMadiganHandRightStream,
};

/// Identity rest transform for the right hand's single root, copied into each model.
static TmdBone _gKyle800101KyleMadiganHandRightSecondPairSkeleton[] = {
#include "assets/kyle_madigan_hand_right_second_pair_skeleton.inc"
};

/// Vertex count for the single part-local right-hand group (27 vertices).
static u32 _gKyle800101KyleMadiganHandRightSecondPairPartVerts[] = {
#include "assets/kyle_madigan_hand_right_second_pair_partVerts.inc"
};

/// Right-hand vertices in integer coordinates local to the hand root.
static SVECTOR _gKyle800101KyleMadiganHandRightSecondPairVerts[] = {
#include "assets/kyle_madigan_hand_right_second_pair_verts.inc"
};

/// Hand lighting normals with twelve fractional bits, indexed independently of vertices.
static SVECTOR _gKyle800101KyleMadiganHandRightSecondPairNormals[] = {
#include "assets/kyle_madigan_hand_right_second_pair_normals.inc"
};

/// Writable draw commands for two textured Gouraud triangles and twenty-four quads.
///
/// One skeletal group and an empty final group end in `TMD_STREAM_END`.
/// First model creation resolves the command handler slots in place.
static u32 _gKyle800101KyleMadiganHandRightSecondPairStream[] = {
#include "assets/kyle_madigan_hand_right_second_pair_stream.inc"
};

TmdSource gKyle800101KyleMadiganHandRightSecondPair = {
    .handlersResolved = false,
    .bufferHalfBytes  = (2 * sizeof(POLY_GT3)) + (24 * sizeof(POLY_GT4)),
    // All faces transform directly from the single hand part.
    .preXformRegionBytes = 0,
    .partCount           = ARRAY_SIZE(_gKyle800101KyleMadiganHandRightSecondPairSkeleton),
    .partVertexCounts    = _gKyle800101KyleMadiganHandRightSecondPairPartVerts,
    .verts               = _gKyle800101KyleMadiganHandRightSecondPairVerts,
    .normals             = _gKyle800101KyleMadiganHandRightSecondPairNormals,
    .skeleton            = _gKyle800101KyleMadiganHandRightSecondPairSkeleton,
    .stream              = _gKyle800101KyleMadiganHandRightSecondPairStream,
};

/// Identity rest transform for the left hand's single root, copied into each model.
static TmdBone _gKyle800101KyleMadiganHandLeftFirstPairSkeleton[] = {
#include "assets/kyle_madigan_left_skeleton.inc"
};

/// Vertex count for the left hand's single part (23 vertices).
static u32 _gKyle800101KyleMadiganHandLeftFirstPairPartVerts[] = {
#include "assets/kyle_madigan_left_partVerts.inc"
};

/// Left-hand vertices in integer coordinates local to the hand root.
static SVECTOR _gKyle800101KyleMadiganHandLeftFirstPairVerts[] = {
#include "assets/kyle_madigan_left_verts.inc"
};

/// Hand lighting normals with twelve fractional bits, indexed independently of vertices.
static SVECTOR _gKyle800101KyleMadiganHandLeftFirstPairNormals[] = {
#include "assets/kyle_madigan_left_normals.inc"
};

/// Writable draw commands for four textured Gouraud triangles and nineteen quads.
///
/// One skeletal group and an empty final group end in `TMD_STREAM_END`.
/// First model creation resolves the command handler slots in place.
static u32 _gKyle800101KyleMadiganHandLeftFirstPairStream[] = {
#include "assets/kyle_madigan_left_stream.inc"
};

TmdSource gKyle800101KyleMadiganHandLeftFirstPair = {
    .handlersResolved = false,
    .bufferHalfBytes  = (4 * sizeof(POLY_GT3)) + (19 * sizeof(POLY_GT4)),
    // All faces transform directly from the single hand part.
    .preXformRegionBytes = 0,
    .partCount           = ARRAY_SIZE(_gKyle800101KyleMadiganHandLeftFirstPairSkeleton),
    .partVertexCounts    = _gKyle800101KyleMadiganHandLeftFirstPairPartVerts,
    .verts               = _gKyle800101KyleMadiganHandLeftFirstPairVerts,
    .normals             = _gKyle800101KyleMadiganHandLeftFirstPairNormals,
    .skeleton            = _gKyle800101KyleMadiganHandLeftFirstPairSkeleton,
    .stream              = _gKyle800101KyleMadiganHandLeftFirstPairStream,
};

/// Identity rest transform for the left hand's single root, copied into each model.
static TmdBone _gKyle800101KyleMadiganHandLeftSkeleton[] = {
#include "assets/kyle_madigan_hand_left_skeleton.inc"
};

/// Vertex count for the single part-local left-hand group (27 vertices).
static u32 _gKyle800101KyleMadiganHandLeftPartVerts[] = {
#include "assets/kyle_madigan_hand_left_partVerts.inc"
};

/// Left-hand vertices in integer coordinates local to the hand root.
static SVECTOR _gKyle800101KyleMadiganHandLeftVerts[] = {
#include "assets/kyle_madigan_hand_left_verts.inc"
};

/// Hand lighting normals with twelve fractional bits, indexed independently of vertices.
static SVECTOR _gKyle800101KyleMadiganHandLeftNormals[] = {
#include "assets/kyle_madigan_hand_left_normals.inc"
};

/// Writable draw commands for two textured Gouraud triangles and twenty-four quads.
///
/// One skeletal group and an empty final group end in `TMD_STREAM_END`.
/// First model creation resolves the command handler slots in place.
static u32 _gKyle800101KyleMadiganHandLeftStream[] = {
#include "assets/kyle_madigan_hand_left_stream.inc"
};

TmdSource gKyle800101KyleMadiganHandLeft = {
    .handlersResolved = false,
    .bufferHalfBytes  = (2 * sizeof(POLY_GT3)) + (24 * sizeof(POLY_GT4)),
    // All faces transform directly from the single hand part.
    .preXformRegionBytes = 0,
    .partCount           = ARRAY_SIZE(_gKyle800101KyleMadiganHandLeftSkeleton),
    .partVertexCounts    = _gKyle800101KyleMadiganHandLeftPartVerts,
    .verts               = _gKyle800101KyleMadiganHandLeftVerts,
    .normals             = _gKyle800101KyleMadiganHandLeftNormals,
    .skeleton            = _gKyle800101KyleMadiganHandLeftSkeleton,
    .stream              = _gKyle800101KyleMadiganHandLeftStream,
};

/// Identity rest transform for the gun's single root, copied into each model.
static TmdBone _gKyle800101KyleMadiganGunSkeleton[] = {
#include "assets/kyle_madigan_gun_skeleton.inc"
};

/// Vertex count for the single part-local gun group (22 vertices).
static u32 _gKyle800101KyleMadiganGunPartVerts[] = {
#include "assets/kyle_madigan_gun_partVerts.inc"
};

/// Gun vertices in integer coordinates local to the gun root.
static SVECTOR _gKyle800101KyleMadiganGunVerts[] = {
#include "assets/kyle_madigan_gun_verts.inc"
};

/// Gun lighting normals with twelve fractional bits, indexed independently of vertices.
static SVECTOR _gKyle800101KyleMadiganGunNormals[] = {
#include "assets/kyle_madigan_gun_normals.inc"
};

/// Writable draw commands for one flat textured quad, two Gouraud triangles and nineteen Gouraud quads.
///
/// One skeletal group and an empty final group end in `TMD_STREAM_END`.
/// First model creation resolves the command handler slots in place.
static u32 _gKyle800101KyleMadiganGunStream[] = {
#include "assets/kyle_madigan_gun_stream.inc"
};

TmdSource gKyle800101KyleMadiganGun = {
    .handlersResolved = false,
    .bufferHalfBytes  = sizeof(POLY_FT4) + (2 * sizeof(POLY_GT3)) + (19 * sizeof(POLY_GT4)),
    // All faces transform directly from the single gun part.
    .preXformRegionBytes = 0,
    .partCount           = ARRAY_SIZE(_gKyle800101KyleMadiganGunSkeleton),
    .partVertexCounts    = _gKyle800101KyleMadiganGunPartVerts,
    .verts               = _gKyle800101KyleMadiganGunVerts,
    .normals             = _gKyle800101KyleMadiganGunNormals,
    .skeleton            = _gKyle800101KyleMadiganGunSkeleton,
    .stream              = _gKyle800101KyleMadiganGunStream,
};

/// Serialized encodings selecting the pose-bank formats used by these body clips.
enum {
    KYLE_800101_ANIMATION_TRANSLATION_ROTATION_BANK = 1,
    KYLE_800101_ANIMATION_PACKED_ROTATION_BANK      = 4
};

/// Idle translation/rotation keys for tracks 0 and 1, in integer coordinates and 1/4096 turns.
static AnimationPackedPose _gKyle800101KyleMadiganIdleAnimationBank1[] = {
#include "assets/kyle_800101_animation_0641C_bank1.inc"
};

/// Idle rotation-only keys for tracks 2..19, in eight-unit steps over 4096 units per turn.
static AnimationPackedRotation _gKyle800101KyleMadiganIdleAnimationBank4[] = {
#include "assets/kyle_800101_animation_0641C_bank4.inc"
};

/// Idle keyframes and loop controls; durations count whole normal-rate frames.
///
/// Pose offsets count four-byte words; control jumps use absolute record indices.
static AnimationRecord _gKyle800101KyleMadiganIdleAnimationRecords[] = {
#include "assets/kyle_800101_animation_0641C_records.inc"
};

/// Absolute record-array starts for the idle animation's twenty model-part tracks.
static u16 _gKyle800101KyleMadiganIdleAnimationIndices[] = {
#include "assets/kyle_800101_animation_0641C_indices.inc"
};
STATIC_ASSERT(ARRAY_SIZE(_gKyle800101KyleMadiganIdleAnimationIndices) == ARRAY_SIZE(_gKyle800101KyleMadiganBodySkeleton), kyle_800101_idle_animation_tracks_match_body);

/// Kyle Madigan's looping standing idle for companion resource variant 1.
///
/// Bank set 1 and its aliases share twenty tracks: 0 and 1 carry translation
/// and rotation, 2..19 rotation only. Child playback drives tracks 1..19.
/// Playback borrows the data without changing it; keep this package loaded
/// while any slot or cached record refers to the clip.
static AnimationSet _gKyle800101KyleMadiganIdleAnimation = {
    .records           = _gKyle800101KyleMadiganIdleAnimationRecords,
    .trackStartIndices = _gKyle800101KyleMadiganIdleAnimationIndices,
    .poseBanks         = {
        [KYLE_800101_ANIMATION_TRANSLATION_ROTATION_BANK] = _gKyle800101KyleMadiganIdleAnimationBank1,
        [KYLE_800101_ANIMATION_PACKED_ROTATION_BANK]      = _gKyle800101KyleMadiganIdleAnimationBank4,
    },
};

/// Left-turn translation/rotation keys for tracks 0 and 1, in integer coordinates and 1/4096 turns.
static AnimationPackedPose _gKyle800101KyleMadiganTurnLeftAnimationBank1[] = {
#include "assets/kyle_800101_animation_06C88_bank1.inc"
};

/// Left-turn rotation-only keys for tracks 2..19, in eight-unit steps over 4096 units per turn.
static AnimationPackedRotation _gKyle800101KyleMadiganTurnLeftAnimationBank4[] = {
#include "assets/kyle_800101_animation_06C88_bank4.inc"
};

/// Left-turn keyframes and loop controls; durations count whole normal-rate frames.
///
/// Pose offsets count four-byte words; control jumps use absolute record indices.
static AnimationRecord _gKyle800101KyleMadiganTurnLeftAnimationRecords[] = {
#include "assets/kyle_800101_animation_06C88_records.inc"
};

/// Absolute record-array starts for the left-turn animation's twenty model-part tracks.
static u16 _gKyle800101KyleMadiganTurnLeftAnimationIndices[] = {
#include "assets/kyle_800101_animation_06C88_indices.inc"
};
STATIC_ASSERT(ARRAY_SIZE(_gKyle800101KyleMadiganTurnLeftAnimationIndices) == ARRAY_SIZE(_gKyle800101KyleMadiganBodySkeleton), kyle_800101_turn_left_animation_tracks_match_body);

/// Kyle Madigan's looping left-turn animation for companion resource variant 1.
///
/// Bank set 5 supplies twenty tracks: 0 and 1 carry translation and rotation,
/// 2..19 rotation only. Child playback drives tracks 1..19 while the actor
/// turns its root toward a negative yaw difference.
/// Playback borrows the data without changing it; keep this package loaded
/// while any slot or cached record refers to the clip.
static AnimationSet _gKyle800101KyleMadiganTurnLeftAnimation = {
    .records           = _gKyle800101KyleMadiganTurnLeftAnimationRecords,
    .trackStartIndices = _gKyle800101KyleMadiganTurnLeftAnimationIndices,
    .poseBanks         = {
        [KYLE_800101_ANIMATION_TRANSLATION_ROTATION_BANK] = _gKyle800101KyleMadiganTurnLeftAnimationBank1,
        [KYLE_800101_ANIMATION_PACKED_ROTATION_BANK]      = _gKyle800101KyleMadiganTurnLeftAnimationBank4,
    },
};

/// Right-turn translation/rotation keys for tracks 0 and 1, in integer coordinates and 1/4096 turns.
static AnimationPackedPose _gKyle800101KyleMadiganTurnRightAnimationBank1[] = {
#include "assets/kyle_800101_animation_0755C_bank1.inc"
};

/// Right-turn rotation-only keys for tracks 2..19, in eight-unit steps over 4096 units per turn.
static AnimationPackedRotation _gKyle800101KyleMadiganTurnRightAnimationBank4[] = {
#include "assets/kyle_800101_animation_0755C_bank4.inc"
};

/// Right-turn keyframes and loop controls; durations count whole normal-rate frames.
///
/// Pose offsets count four-byte words; control jumps use absolute record indices.
static AnimationRecord _gKyle800101KyleMadiganTurnRightAnimationRecords[] = {
#include "assets/kyle_800101_animation_0755C_records.inc"
};

/// Absolute record-array starts for the right-turn animation's twenty model-part tracks.
static u16 _gKyle800101KyleMadiganTurnRightAnimationIndices[] = {
#include "assets/kyle_800101_animation_0755C_indices.inc"
};
STATIC_ASSERT(ARRAY_SIZE(_gKyle800101KyleMadiganTurnRightAnimationIndices) == ARRAY_SIZE(_gKyle800101KyleMadiganBodySkeleton), kyle_800101_turn_right_animation_tracks_match_body);

/// Kyle Madigan's looping right-turn animation for companion resource variant 1.
///
/// Bank set 6 supplies twenty tracks: 0 and 1 carry translation and rotation,
/// 2..19 rotation only. Child playback drives tracks 1..19 while the actor
/// turns its root toward a positive yaw difference.
/// Playback borrows the data without changing it; keep this package loaded
/// while any slot or cached record refers to the clip.
static AnimationSet _gKyle800101KyleMadiganTurnRightAnimation = {
    .records           = _gKyle800101KyleMadiganTurnRightAnimationRecords,
    .trackStartIndices = _gKyle800101KyleMadiganTurnRightAnimationIndices,
    .poseBanks         = {
        [KYLE_800101_ANIMATION_TRANSLATION_ROTATION_BANK] = _gKyle800101KyleMadiganTurnRightAnimationBank1,
        [KYLE_800101_ANIMATION_PACKED_ROTATION_BANK]      = _gKyle800101KyleMadiganTurnRightAnimationBank4,
    },
};

/// Part-4 hit translation/rotation keys for tracks 0 and 1, in integer coordinates and 1/4096 turns.
static AnimationPackedPose _gKyle800101KyleMadiganPart4HitAnimationBank1[] = {
#include "assets/kyle_800101_animation_07A1C_bank1.inc"
};

/// Part-4 hit rotation-only keys for tracks 2..19, in eight-unit steps over 4096 units per turn.
static AnimationPackedRotation _gKyle800101KyleMadiganPart4HitAnimationBank4[] = {
#include "assets/kyle_800101_animation_07A1C_bank4.inc"
};

/// Part-4 hit keyframes and stop controls; durations count whole normal-rate frames.
///
/// Pose offsets count four-byte words; stop records retain the final keyframe.
static AnimationRecord _gKyle800101KyleMadiganPart4HitAnimationRecords[] = {
#include "assets/kyle_800101_animation_07A1C_records.inc"
};

/// Absolute record-array starts for the part-4 hit animation's twenty model-part tracks.
static u16 _gKyle800101KyleMadiganPart4HitAnimationIndices[] = {
#include "assets/kyle_800101_animation_07A1C_indices.inc"
};
STATIC_ASSERT(ARRAY_SIZE(_gKyle800101KyleMadiganPart4HitAnimationIndices) == ARRAY_SIZE(_gKyle800101KyleMadiganBodySkeleton), kyle_800101_part4_hit_animation_tracks_match_body);

/// Kyle Madigan's non-looping reaction to a hit on the sphere attached to body part 4.
///
/// Bank set 16 supplies twenty tracks: 0 and 1 carry translation and rotation,
/// 2..19 rotation only. Child playback drives tracks 1..19 and holds their
/// final poses until recovery selects another clip.
/// Playback borrows the data without changing it; keep this package loaded
/// while any slot or cached record refers to the clip.
static AnimationSet _gKyle800101KyleMadiganPart4HitAnimation = {
    .records           = _gKyle800101KyleMadiganPart4HitAnimationRecords,
    .trackStartIndices = _gKyle800101KyleMadiganPart4HitAnimationIndices,
    .poseBanks         = {
        [KYLE_800101_ANIMATION_TRANSLATION_ROTATION_BANK] = _gKyle800101KyleMadiganPart4HitAnimationBank1,
        [KYLE_800101_ANIMATION_PACKED_ROTATION_BANK]      = _gKyle800101KyleMadiganPart4HitAnimationBank4,
    },
};

/// Other-body hit translation/rotation keys for tracks 0 and 1, in integer coordinates and 1/4096 turns.
static AnimationPackedPose _gKyle800101KyleMadiganOtherBodyHitAnimationBank1[] = {
#include "assets/kyle_800101_animation_07FC0_bank1.inc"
};

/// Other-body hit rotation-only keys for tracks 2..19, in eight-unit steps over 4096 units per turn.
static AnimationPackedRotation _gKyle800101KyleMadiganOtherBodyHitAnimationBank4[] = {
#include "assets/kyle_800101_animation_07FC0_bank4.inc"
};

/// Other-body hit keyframes and stop controls; durations count whole normal-rate frames.
///
/// Pose offsets count four-byte words; stop records retain the final keyframe.
static AnimationRecord _gKyle800101KyleMadiganOtherBodyHitAnimationRecords[] = {
#include "assets/kyle_800101_animation_07FC0_records.inc"
};

/// Absolute record-array starts for the other-body hit animation's twenty model-part tracks.
static u16 _gKyle800101KyleMadiganOtherBodyHitAnimationIndices[] = {
#include "assets/kyle_800101_animation_07FC0_indices.inc"
};
STATIC_ASSERT(ARRAY_SIZE(_gKyle800101KyleMadiganOtherBodyHitAnimationIndices) == ARRAY_SIZE(_gKyle800101KyleMadiganBodySkeleton), kyle_800101_other_body_hit_animation_tracks_match_body);

/// Kyle Madigan's non-looping reaction to a hit on either other motion sphere.
///
/// Bank set 17 supplies twenty tracks: 0 and 1 carry translation and rotation,
/// 2..19 rotation only. Child playback drives tracks 1..19 and holds their
/// final poses until recovery selects another clip.
/// Playback borrows the data without changing it; keep this package loaded
/// while any slot or cached record refers to the clip.
static AnimationSet _gKyle800101KyleMadiganOtherBodyHitAnimation = {
    .records           = _gKyle800101KyleMadiganOtherBodyHitAnimationRecords,
    .trackStartIndices = _gKyle800101KyleMadiganOtherBodyHitAnimationIndices,
    .poseBanks         = {
        [KYLE_800101_ANIMATION_TRANSLATION_ROTATION_BANK] = _gKyle800101KyleMadiganOtherBodyHitAnimationBank1,
        [KYLE_800101_ANIMATION_PACKED_ROTATION_BANK]      = _gKyle800101KyleMadiganOtherBodyHitAnimationBank4,
    },
};

/// Stopped-pose translation/rotation keys for tracks 0 and 1, in integer coordinates and 1/4096 turns.
static AnimationPackedPose _gKyle800101KyleMadiganStoppedAnimationBank1[] = {
#include "assets/kyle_800101_animation_08530_bank1.inc"
};

/// Stopped-pose rotation-only keys for tracks 2..19, in eight-unit steps over 4096 units per turn.
static AnimationPackedRotation _gKyle800101KyleMadiganStoppedAnimationBank4[] = {
#include "assets/kyle_800101_animation_08530_bank4.inc"
};

/// Stopped-pose keyframes and stop controls; durations count whole normal-rate frames.
///
/// Pose offsets count four-byte words; stop records retain the final keyframe.
static AnimationRecord _gKyle800101KyleMadiganStoppedAnimationRecords[] = {
#include "assets/kyle_800101_animation_08530_records.inc"
};

/// Absolute record-array starts for the stopped animation's twenty model-part tracks.
static u16 _gKyle800101KyleMadiganStoppedAnimationIndices[] = {
#include "assets/kyle_800101_animation_08530_indices.inc"
};
STATIC_ASSERT(ARRAY_SIZE(_gKyle800101KyleMadiganStoppedAnimationIndices) == ARRAY_SIZE(_gKyle800101KyleMadiganBodySkeleton), kyle_800101_stopped_animation_tracks_match_body);

/// Kyle Madigan's non-looping stopped/death animation for companion resource variant 1.
///
/// Bank set 18 supplies twenty tracks: 0 and 1 carry translation and rotation,
/// 2..19 rotation only. Child playback drives tracks 1..19 and holds their
/// final poses; the stopped damage state does not return to idle.
/// Playback borrows the data without changing it; keep this package loaded
/// while any slot or cached record refers to the clip.
static AnimationSet _gKyle800101KyleMadiganStoppedAnimation = {
    .records           = _gKyle800101KyleMadiganStoppedAnimationRecords,
    .trackStartIndices = _gKyle800101KyleMadiganStoppedAnimationIndices,
    .poseBanks         = {
        [KYLE_800101_ANIMATION_TRANSLATION_ROTATION_BANK] = _gKyle800101KyleMadiganStoppedAnimationBank1,
        [KYLE_800101_ANIMATION_PACKED_ROTATION_BANK]      = _gKyle800101KyleMadiganStoppedAnimationBank4,
    },
};

/// Idle variation's translation/rotation keys for tracks 0 and 1, in integer coordinates and 1/4096 turns.
static AnimationPackedPose _gKyle800101KyleMadiganIdleVariationAnimationBank1[] = {
#include "assets/kyle_800101_animation_08D3C_bank1.inc"
};

/// Idle variation's rotation-only keys for tracks 2..19, in eight-unit steps over 4096 units per turn.
static AnimationPackedRotation _gKyle800101KyleMadiganIdleVariationAnimationBank4[] = {
#include "assets/kyle_800101_animation_08D3C_bank4.inc"
};

/// Idle variation's keyframes and loop controls; durations count whole normal-rate frames.
///
/// Pose offsets count four-byte words; control jumps use absolute record indices.
static AnimationRecord _gKyle800101KyleMadiganIdleVariationAnimationRecords[] = {
#include "assets/kyle_800101_animation_08D3C_records.inc"
};

/// Absolute record-array starts for the idle variation's twenty model-part tracks.
static u16 _gKyle800101KyleMadiganIdleVariationAnimationIndices[] = {
#include "assets/kyle_800101_animation_08D3C_indices.inc"
};
STATIC_ASSERT(ARRAY_SIZE(_gKyle800101KyleMadiganIdleVariationAnimationIndices) == ARRAY_SIZE(_gKyle800101KyleMadiganBodySkeleton), kyle_800101_idle_variation_animation_tracks_match_body);

/// Kyle Madigan's looping idle variation for companion resource variant 1.
///
/// Bank set 23 plays after several idle decisions without following the player.
/// Twenty tracks supply translation/rotation for parts 0 and 1 and rotation
/// only for parts 2..19; child playback drives tracks 1..19.
/// Playback borrows the data without changing it; keep this package loaded
/// while any slot or cached record refers to the clip.
static AnimationSet _gKyle800101KyleMadiganIdleVariationAnimation = {
    .records           = _gKyle800101KyleMadiganIdleVariationAnimationRecords,
    .trackStartIndices = _gKyle800101KyleMadiganIdleVariationAnimationIndices,
    .poseBanks         = {
        [KYLE_800101_ANIMATION_TRANSLATION_ROTATION_BANK] = _gKyle800101KyleMadiganIdleVariationAnimationBank1,
        [KYLE_800101_ANIMATION_PACKED_ROTATION_BANK]      = _gKyle800101KyleMadiganIdleVariationAnimationBank4,
    },
};

/// Bank-set 32 translation/rotation keys for tracks 0 and 1, in integer coordinates and 1/4096 turns.
static AnimationPackedPose _gKyle800101Animation0920CBank1[] = {
#include "assets/kyle_800101_animation_0920C_bank1.inc"
};

/// Bank-set 32 rotation-only keys for tracks 2..19, in eight-unit steps over 4096 units per turn.
static AnimationPackedRotation _gKyle800101Animation0920CBank4[] = {
#include "assets/kyle_800101_animation_0920C_bank4.inc"
};

/// Bank-set 32 keyframes and terminal stops; durations count whole normal-rate frames.
///
/// Pose offsets count four-byte words; each stop retains its track's final pose.
static AnimationRecord _gKyle800101Animation0920CRecords[] = {
#include "assets/kyle_800101_animation_0920C_records.inc"
};

/// Absolute record-array starts for bank set 32's twenty model-part tracks.
static u16 _gKyle800101Animation0920CIndices[] = {
#include "assets/kyle_800101_animation_0920C_indices.inc"
};
STATIC_ASSERT(ARRAY_SIZE(_gKyle800101Animation0920CIndices) == ARRAY_SIZE(_gKyle800101KyleMadiganBodySkeleton), kyle_800101_set32_animation_tracks_match_body);

/// Kyle Madigan's non-looping bank-set 32 clip for companion resource variant 1.
///
/// Twenty tracks supply translation/rotation for parts 0 and 1 and rotation
/// only for parts 2..19. The visual role and a companion caller are unproven.
/// Playback borrows the data without changing it; keep this package loaded
/// while any slot or cached record refers to the clip.
static AnimationSet _gKyle800101Animation0920C = {
    .records           = _gKyle800101Animation0920CRecords,
    .trackStartIndices = _gKyle800101Animation0920CIndices,
    .poseBanks         = {
        [KYLE_800101_ANIMATION_TRANSLATION_ROTATION_BANK] = _gKyle800101Animation0920CBank1,
        [KYLE_800101_ANIMATION_PACKED_ROTATION_BANK]      = _gKyle800101Animation0920CBank4,
    },
};

/// Bank-set 33 translation/rotation keys for tracks 0 and 1, in integer coordinates and 1/4096 turns.
static AnimationPackedPose _gKyle800101Animation09764Bank1[] = {
#include "assets/kyle_800101_animation_09764_bank1.inc"
};

/// Bank-set 33 rotation-only keys for tracks 2..19, in eight-unit steps over 4096 units per turn.
static AnimationPackedRotation _gKyle800101Animation09764Bank4[] = {
#include "assets/kyle_800101_animation_09764_bank4.inc"
};

/// Bank-set 33 keyframes and terminal stops; durations count whole normal-rate frames.
///
/// Pose offsets count four-byte words; each stop retains its track's final pose.
static AnimationRecord _gKyle800101Animation09764Records[] = {
#include "assets/kyle_800101_animation_09764_records.inc"
};

/// Absolute record-array starts for bank set 33's twenty model-part tracks.
static u16 _gKyle800101Animation09764Indices[] = {
#include "assets/kyle_800101_animation_09764_indices.inc"
};
STATIC_ASSERT(ARRAY_SIZE(_gKyle800101Animation09764Indices) == ARRAY_SIZE(_gKyle800101KyleMadiganBodySkeleton), kyle_800101_set33_animation_tracks_match_body);

/// Kyle Madigan's non-looping bank-set 33 clip for companion resource variant 1.
///
/// Twenty tracks supply translation/rotation for parts 0 and 1 and rotation
/// only for parts 2..19. The visual role and a companion caller are unproven.
/// Playback borrows the data without changing it; keep this package loaded
/// while any slot or cached record refers to the clip.
static AnimationSet _gKyle800101Animation09764 = {
    .records           = _gKyle800101Animation09764Records,
    .trackStartIndices = _gKyle800101Animation09764Indices,
    .poseBanks         = {
        [KYLE_800101_ANIMATION_TRANSLATION_ROTATION_BANK] = _gKyle800101Animation09764Bank1,
        [KYLE_800101_ANIMATION_PACKED_ROTATION_BANK]      = _gKyle800101Animation09764Bank4,
    },
};

/// Combat hold's translation/rotation keys for tracks 0 and 1, in integer coordinates and 1/4096 turns.
static AnimationPackedPose _gKyle800101KyleMadiganCombatHoldAnimationBank1[] = {
#include "assets/kyle_800101_animation_09958_bank1.inc"
};

/// Combat hold's rotation-only keys for tracks 2..19, in eight-unit steps over 4096 units per turn.
static AnimationPackedRotation _gKyle800101KyleMadiganCombatHoldAnimationBank4[] = {
#include "assets/kyle_800101_animation_09958_bank4.inc"
};

/// Combat hold's keyframes and loop controls; durations count whole normal-rate frames.
///
/// Pose offsets count four-byte words; control jumps use absolute record indices.
static AnimationRecord _gKyle800101KyleMadiganCombatHoldAnimationRecords[] = {
#include "assets/kyle_800101_animation_09958_records.inc"
};

/// Absolute record-array starts for the combat hold's twenty model-part tracks.
static u16 _gKyle800101KyleMadiganCombatHoldAnimationIndices[] = {
#include "assets/kyle_800101_animation_09958_indices.inc"
};
STATIC_ASSERT(ARRAY_SIZE(_gKyle800101KyleMadiganCombatHoldAnimationIndices) == ARRAY_SIZE(_gKyle800101KyleMadiganBodySkeleton), kyle_800101_combat_hold_animation_tracks_match_body);

/// Kyle Madigan's looping weapon-aim hold for companion resource variant 1.
///
/// Bank set 9 holds aim after combat entry and between attacks or movement.
/// Twenty tracks supply translation/rotation for parts 0 and 1 and rotation
/// only for parts 2..19; child playback drives tracks 1..19.
/// Playback borrows the data without changing it; keep this package loaded
/// while any slot or cached record refers to the clip.
static AnimationSet _gKyle800101KyleMadiganCombatHoldAnimation = {
    .records           = _gKyle800101KyleMadiganCombatHoldAnimationRecords,
    .trackStartIndices = _gKyle800101KyleMadiganCombatHoldAnimationIndices,
    .poseBanks         = {
        [KYLE_800101_ANIMATION_TRANSLATION_ROTATION_BANK] = _gKyle800101KyleMadiganCombatHoldAnimationBank1,
        [KYLE_800101_ANIMATION_PACKED_ROTATION_BANK]      = _gKyle800101KyleMadiganCombatHoldAnimationBank4,
    },
};

/// Translation/rotation poses for tracks 0 and 1 of the unreferenced clip.
///
/// Translation uses integer model units; rotation uses 4096 units per turn.
static const AnimationPackedPose _gKyle800101Animation09B48Bank1[] SECTION(".data") = {
#include "assets/kyle_800101_animation_09B48_bank1.inc"
};

/// Fixed rotations for tracks 2..19, in eight-unit steps over 4096 units per turn.
static const AnimationPackedRotation _gKyle800101Animation09B48Bank4[] SECTION(".data") = {
#include "assets/kyle_800101_animation_09B48_bank4.inc"
};

/// Two identical one-frame keys, a backward jump and a terminal stop per track.
///
/// Pose offsets count four-byte words; jumps use absolute record indices.
static const AnimationRecord _gKyle800101Animation09B48Records[] SECTION(".data") = {
#include "assets/kyle_800101_animation_09B48_records.inc"
};

/// Absolute record-array starts for the unreferenced clip's twenty model-part tracks.
static const u16 _gKyle800101Animation09B48Indices[] SECTION(".data") = {
#include "assets/kyle_800101_animation_09B48_indices.inc"
};
STATIC_ASSERT(ARRAY_SIZE(_gKyle800101Animation09B48Indices) == ARRAY_SIZE(_gKyle800101KyleMadiganBodySkeleton), kyle_800101_unreferenced_animation_tracks_match_body);

/// An unreferenced fixed looping pose for Kyle Madigan's twenty-part body.
///
/// No native bank entry selects this descriptor; its intended role is unproven.
/// Tracks 0 and 1 use translation/rotation, and tracks 2..19 use packed rotations.
/// Playback reads the package-owned data; retain it while a slot borrows the clip.
static AnimationSet _gKyle800101Animation09B48 = {
    .records           = _gKyle800101Animation09B48Records,
    .trackStartIndices = _gKyle800101Animation09B48Indices,
    .poseBanks         = {
        [KYLE_800101_ANIMATION_TRANSLATION_ROTATION_BANK] = _gKyle800101Animation09B48Bank1,
        [KYLE_800101_ANIMATION_PACKED_ROTATION_BANK]      = _gKyle800101Animation09B48Bank4,
    },
};

/// Bank-set 13 translation/rotation keys for tracks 0 and 1, in integer coordinates and 1/4096 turns.
static AnimationPackedPose _gKyle800101Animation0A2C4Bank1[] = {
#include "assets/kyle_800101_animation_0A2C4_bank1.inc"
};

/// Bank-set 13 rotation-only keys for tracks 2..19, in eight-unit steps over 4096 units per turn.
static AnimationPackedRotation _gKyle800101Animation0A2C4Bank4[] = {
#include "assets/kyle_800101_animation_0A2C4_bank4.inc"
};

/// Bank-set 13 keyframes, cue bits and loop controls; durations count whole normal-rate frames.
///
/// Pose offsets count four-byte words; control jumps use absolute record indices.
static AnimationRecord _gKyle800101Animation0A2C4Records[] = {
#include "assets/kyle_800101_animation_0A2C4_records.inc"
};

/// Absolute record-array starts for bank set 13's twenty model-part tracks.
static u16 _gKyle800101Animation0A2C4Indices[] = {
#include "assets/kyle_800101_animation_0A2C4_indices.inc"
};
STATIC_ASSERT(ARRAY_SIZE(_gKyle800101Animation0A2C4Indices) == ARRAY_SIZE(_gKyle800101KyleMadiganBodySkeleton), kyle_800101_set13_animation_tracks_match_body);

/// Kyle Madigan's looping bank-set 13 clip for companion resource variant 1.
///
/// Twenty tracks supply translation/rotation for parts 0 and 1 and rotation
/// only for parts 2..19. Track 1 carries cue bits 1 and 2; the companion role
/// and a caller are unproven.
/// Playback borrows the data without changing it; keep this package loaded
/// while any slot or cached record refers to the clip.
static AnimationSet _gKyle800101Animation0A2C4 = {
    .records           = _gKyle800101Animation0A2C4Records,
    .trackStartIndices = _gKyle800101Animation0A2C4Indices,
    .poseBanks         = {
        [KYLE_800101_ANIMATION_TRANSLATION_ROTATION_BANK] = _gKyle800101Animation0A2C4Bank1,
        [KYLE_800101_ANIMATION_PACKED_ROTATION_BANK]      = _gKyle800101Animation0A2C4Bank4,
    },
};

/// Approach translation/rotation keys for tracks 0 and 1, in integer coordinates and 1/4096 turns.
static AnimationPackedPose _gKyle800101KyleMadiganApproachAnimationBank1[] = {
#include "assets/kyle_800101_animation_0AC7C_bank1.inc"
};

/// Approach rotation-only keys for tracks 2..19, in eight-unit steps over 4096 units per turn.
static AnimationPackedRotation _gKyle800101KyleMadiganApproachAnimationBank4[] = {
#include "assets/kyle_800101_animation_0AC7C_bank4.inc"
};

/// Approach keyframes, footstep cues and loop controls; durations count whole normal-rate frames.
///
/// Pose offsets count four-byte words; control jumps use absolute record indices.
static AnimationRecord _gKyle800101KyleMadiganApproachAnimationRecords[] = {
#include "assets/kyle_800101_animation_0AC7C_records.inc"
};

/// Absolute record-array starts for the approach animation's twenty model-part tracks.
static u16 _gKyle800101KyleMadiganApproachAnimationIndices[] = {
#include "assets/kyle_800101_animation_0AC7C_indices.inc"
};
STATIC_ASSERT(ARRAY_SIZE(_gKyle800101KyleMadiganApproachAnimationIndices) == ARRAY_SIZE(_gKyle800101KyleMadiganBodySkeleton), kyle_800101_approach_animation_tracks_match_body);

/// Kyle Madigan's looping armed approach run for companion resource variant 1.
///
/// Bank set 12 plays while running toward the target or player; track 1 carries
/// the footstep cues. Twenty tracks supply translation/rotation for parts 0 and
/// 1 and rotation only for parts 2..19; child playback drives tracks 1..19.
/// Playback borrows the data without changing it; keep this package loaded
/// while any slot or cached record refers to the clip.
static AnimationSet _gKyle800101KyleMadiganApproachAnimation = {
    .records           = _gKyle800101KyleMadiganApproachAnimationRecords,
    .trackStartIndices = _gKyle800101KyleMadiganApproachAnimationIndices,
    .poseBanks         = {
        [KYLE_800101_ANIMATION_TRANSLATION_ROTATION_BANK] = _gKyle800101KyleMadiganApproachAnimationBank1,
        [KYLE_800101_ANIMATION_PACKED_ROTATION_BANK]      = _gKyle800101KyleMadiganApproachAnimationBank4,
    },
};

/// Reload translation/rotation keys for tracks 0 and 1, in integer coordinates and 1/4096 turns.
static AnimationPackedPose _gKyle800101KyleMadiganReloadAnimationBank1[] = {
#include "assets/kyle_800101_animation_0B6F0_bank1.inc"
};

/// Reload rotation-only keys for tracks 2..19, in eight-unit steps over 4096 units per turn.
static AnimationPackedRotation _gKyle800101KyleMadiganReloadAnimationBank4[] = {
#include "assets/kyle_800101_animation_0B6F0_bank4.inc"
};

/// Reload keyframes and stop controls; durations count whole normal-rate frames.
///
/// Pose offsets count four-byte words; stop controls retain the last keyframe.
static AnimationRecord _gKyle800101KyleMadiganReloadAnimationRecords[] = {
#include "assets/kyle_800101_animation_0B6F0_records.inc"
};

/// Absolute record-array starts for the reload animation's twenty model-part tracks.
static u16 _gKyle800101KyleMadiganReloadAnimationIndices[] = {
#include "assets/kyle_800101_animation_0B6F0_indices.inc"
};
STATIC_ASSERT(ARRAY_SIZE(_gKyle800101KyleMadiganReloadAnimationIndices) == ARRAY_SIZE(_gKyle800101KyleMadiganBodySkeleton), kyle_800101_reload_animation_tracks_match_body);

/// Kyle Madigan's P229 reload animation for companion resource variant 1.
///
/// Bank set 14 and its alias 20 share a stopping clip. Native reload playback
/// drives child tracks 1..19 and refreshes the companion's attack allowance.
/// Tracks 0 and 1 carry translation/rotation, 2..19 rotation only.
/// Playback borrows the data without changing it; keep this package loaded
/// while any slot or cached record refers to the clip.
static AnimationSet _gKyle800101KyleMadiganReloadAnimation = {
    .records           = _gKyle800101KyleMadiganReloadAnimationRecords,
    .trackStartIndices = _gKyle800101KyleMadiganReloadAnimationIndices,
    .poseBanks         = {
        [KYLE_800101_ANIMATION_TRANSLATION_ROTATION_BANK] = _gKyle800101KyleMadiganReloadAnimationBank1,
        [KYLE_800101_ANIMATION_PACKED_ROTATION_BANK]      = _gKyle800101KyleMadiganReloadAnimationBank4,
    },
};

/// Single-shot translation/rotation keys for tracks 0 and 1, in integer coordinates and 1/4096 turns.
static AnimationPackedPose _gKyle800101KyleMadiganSingleShotAnimationBank1[] = {
#include "assets/kyle_800101_animation_0B968_bank1.inc"
};

/// Single-shot rotation-only keys for tracks 2..19, in eight-unit steps over 4096 units per turn.
static AnimationPackedRotation _gKyle800101KyleMadiganSingleShotAnimationBank4[] = {
#include "assets/kyle_800101_animation_0B968_bank4.inc"
};

/// Single-shot keyframes and stop controls; durations count whole normal-rate frames.
///
/// Pose offsets count four-byte words; stop controls retain the last keyframe.
static AnimationRecord _gKyle800101KyleMadiganSingleShotAnimationRecords[] = {
#include "assets/kyle_800101_animation_0B968_records.inc"
};

/// Absolute record-array starts for the single-shot animation's twenty model-part tracks.
static u16 _gKyle800101KyleMadiganSingleShotAnimationIndices[] = {
#include "assets/kyle_800101_animation_0B968_indices.inc"
};
STATIC_ASSERT(ARRAY_SIZE(_gKyle800101KyleMadiganSingleShotAnimationIndices) == ARRAY_SIZE(_gKyle800101KyleMadiganBodySkeleton), kyle_800101_single_shot_animation_tracks_match_body);

/// Kyle Madigan's P229 single-shot animation for companion resource variant 1.
///
/// Bank set 10 plays with the shot's collision, sound and muzzle flash; attack
/// playback waits for child slot 8 to settle before beginning its cooldown.
/// Twenty stopping tracks supply translation/rotation for parts 0 and 1 and
/// rotation only for parts 2..19; child playback drives tracks 1..19.
/// Playback borrows the data without changing it; keep this package loaded
/// while any slot or cached record refers to the clip.
static AnimationSet _gKyle800101KyleMadiganSingleShotAnimation = {
    .records           = _gKyle800101KyleMadiganSingleShotAnimationRecords,
    .trackStartIndices = _gKyle800101KyleMadiganSingleShotAnimationIndices,
    .poseBanks         = {
        [KYLE_800101_ANIMATION_TRANSLATION_ROTATION_BANK] = _gKyle800101KyleMadiganSingleShotAnimationBank1,
        [KYLE_800101_ANIMATION_PACKED_ROTATION_BANK]      = _gKyle800101KyleMadiganSingleShotAnimationBank4,
    },
};

/// Combat-entry translation/rotation keys for tracks 0 and 1, in integer coordinates and 1/4096 turns.
static AnimationPackedPose _gKyle800101KyleMadiganCombatEntryAnimationBank1[] = {
#include "assets/kyle_800101_animation_0BD70_bank1.inc"
};

/// Combat-entry rotation-only keys for tracks 2..19, in eight-unit steps over 4096 units per turn.
static AnimationPackedRotation _gKyle800101KyleMadiganCombatEntryAnimationBank4[] = {
#include "assets/kyle_800101_animation_0BD70_bank4.inc"
};

/// Combat-entry keyframes and stop controls; durations count whole normal-rate frames.
///
/// Pose offsets count four-byte words; stop controls retain the last keyframe.
static AnimationRecord _gKyle800101KyleMadiganCombatEntryAnimationRecords[] = {
#include "assets/kyle_800101_animation_0BD70_records.inc"
};

/// Absolute record-array starts for the combat-entry animation's twenty model-part tracks.
static u16 _gKyle800101KyleMadiganCombatEntryAnimationIndices[] = {
#include "assets/kyle_800101_animation_0BD70_indices.inc"
};
STATIC_ASSERT(ARRAY_SIZE(_gKyle800101KyleMadiganCombatEntryAnimationIndices) == ARRAY_SIZE(_gKyle800101KyleMadiganBodySkeleton), kyle_800101_combat_entry_animation_tracks_match_body);

/// Kyle Madigan's combat-entry animation for companion resource variant 1.
///
/// Bank set 7 and its alias 34 share a stopping clip. Native combat entry
/// plays it after facing the target, then switches to the combat-hold clip.
/// Twenty tracks supply translation/rotation for parts 0 and 1 and rotation
/// only for parts 2..19; child playback drives tracks 1..19.
/// Playback borrows the data without changing it; keep this package loaded
/// while any slot or cached record refers to the clip.
static AnimationSet _gKyle800101KyleMadiganCombatEntryAnimation = {
    .records           = _gKyle800101KyleMadiganCombatEntryAnimationRecords,
    .trackStartIndices = _gKyle800101KyleMadiganCombatEntryAnimationIndices,
    .poseBanks         = {
        [KYLE_800101_ANIMATION_TRANSLATION_ROTATION_BANK] = _gKyle800101KyleMadiganCombatEntryAnimationBank1,
        [KYLE_800101_ANIMATION_PACKED_ROTATION_BANK]      = _gKyle800101KyleMadiganCombatEntryAnimationBank4,
    },
};

/// Combat-exit translation/rotation keys for tracks 0 and 1, in integer coordinates and 1/4096 turns.
static AnimationPackedPose _gKyle800101KyleMadiganCombatExitAnimationBank1[] = {
#include "assets/kyle_800101_animation_0C24C_bank1.inc"
};

/// Combat-exit rotation-only keys for tracks 2..19, in eight-unit steps over 4096 units per turn.
static AnimationPackedRotation _gKyle800101KyleMadiganCombatExitAnimationBank4[] = {
#include "assets/kyle_800101_animation_0C24C_bank4.inc"
};

/// Combat-exit keyframes and stop controls; durations count whole normal-rate frames.
///
/// Pose offsets count four-byte words; stop controls retain the last keyframe.
static AnimationRecord _gKyle800101KyleMadiganCombatExitAnimationRecords[] = {
#include "assets/kyle_800101_animation_0C24C_records.inc"
};

/// Absolute record-array starts for the combat-exit animation's twenty model-part tracks.
static u16 _gKyle800101KyleMadiganCombatExitAnimationIndices[] = {
#include "assets/kyle_800101_animation_0C24C_indices.inc"
};
STATIC_ASSERT(ARRAY_SIZE(_gKyle800101KyleMadiganCombatExitAnimationIndices) == ARRAY_SIZE(_gKyle800101KyleMadiganBodySkeleton), kyle_800101_combat_exit_animation_tracks_match_body);

/// Kyle Madigan's combat-exit animation for companion resource variant 1.
///
/// Bank set 8 and its alias 35 share a stopping clip. Native combat exit
/// clears lock-on and plays it before returning to idle.
/// Twenty tracks supply translation/rotation for parts 0 and 1 and rotation
/// only for parts 2..19; child playback drives tracks 1..19.
/// Playback borrows the data without changing it; keep this package loaded
/// while any slot or cached record refers to the clip.
static AnimationSet _gKyle800101KyleMadiganCombatExitAnimation = {
    .records           = _gKyle800101KyleMadiganCombatExitAnimationRecords,
    .trackStartIndices = _gKyle800101KyleMadiganCombatExitAnimationIndices,
    .poseBanks         = {
        [KYLE_800101_ANIMATION_TRANSLATION_ROTATION_BANK] = _gKyle800101KyleMadiganCombatExitAnimationBank1,
        [KYLE_800101_ANIMATION_PACKED_ROTATION_BANK]      = _gKyle800101KyleMadiganCombatExitAnimationBank4,
    },
};

/// Walk translation/rotation keys for tracks 0 and 1, in integer coordinates and 1/4096 turns.
static AnimationPackedPose _gKyle800101KyleMadiganWalkAnimationBank1[] = {
#include "assets/kyle_800101_animation_0CACC_bank1.inc"
};

/// Walk rotation-only keys for tracks 2..19, in eight-unit steps over 4096 units per turn.
static AnimationPackedRotation _gKyle800101KyleMadiganWalkAnimationBank4[] = {
#include "assets/kyle_800101_animation_0CACC_bank4.inc"
};

/// Walk keyframes, footstep cues and loop controls; durations count whole normal-rate frames.
///
/// Pose offsets count four-byte words; control jumps use absolute record indices.
static AnimationRecord _gKyle800101KyleMadiganWalkAnimationRecords[] = {
#include "assets/kyle_800101_animation_0CACC_records.inc"
};

/// Absolute record-array starts for the walk animation's twenty model-part tracks.
static u16 _gKyle800101KyleMadiganWalkAnimationIndices[] = {
#include "assets/kyle_800101_animation_0CACC_indices.inc"
};
STATIC_ASSERT(ARRAY_SIZE(_gKyle800101KyleMadiganWalkAnimationIndices) == ARRAY_SIZE(_gKyle800101KyleMadiganBodySkeleton), kyle_800101_walk_animation_tracks_match_body);

/// Kyle Madigan's looping walk animation for companion resource variant 1.
///
/// Bank sets 2 and 19 share this clip for native following, scan/wander movement
/// and scripted movement with or without equipment; track 1 carries footstep cues.
/// Twenty tracks supply translation/rotation for parts 0 and 1 and rotation
/// only for parts 2..19; child playback drives tracks 1..19.
/// Playback borrows the data without changing it; keep this package loaded
/// while any slot or cached record refers to the clip.
static AnimationSet _gKyle800101KyleMadiganWalkAnimation = {
    .records           = _gKyle800101KyleMadiganWalkAnimationRecords,
    .trackStartIndices = _gKyle800101KyleMadiganWalkAnimationIndices,
    .poseBanks         = {
        [KYLE_800101_ANIMATION_TRANSLATION_ROTATION_BANK] = _gKyle800101KyleMadiganWalkAnimationBank1,
        [KYLE_800101_ANIMATION_PACKED_ROTATION_BANK]      = _gKyle800101KyleMadiganWalkAnimationBank4,
    },
};

/// Run translation/rotation keys for tracks 0 and 1, in integer coordinates and 1/4096 turns.
static AnimationPackedPose _gKyle800101KyleMadiganRunAnimationBank1[] = {
#include "assets/kyle_800101_animation_0D4D0_bank1.inc"
};

/// Run rotation-only keys for tracks 2..19, in eight-unit steps over 4096 units per turn.
static AnimationPackedRotation _gKyle800101KyleMadiganRunAnimationBank4[] = {
#include "assets/kyle_800101_animation_0D4D0_bank4.inc"
};

/// Run keyframes, footstep cues and loop controls; durations count whole normal-rate frames.
///
/// Pose offsets count four-byte words; control jumps use absolute record indices.
static AnimationRecord _gKyle800101KyleMadiganRunAnimationRecords[] = {
#include "assets/kyle_800101_animation_0D4D0_records.inc"
};

/// Absolute record-array starts for the run animation's twenty model-part tracks.
static u16 _gKyle800101KyleMadiganRunAnimationIndices[] = {
#include "assets/kyle_800101_animation_0D4D0_indices.inc"
};
STATIC_ASSERT(ARRAY_SIZE(_gKyle800101KyleMadiganRunAnimationIndices) == ARRAY_SIZE(_gKyle800101KyleMadiganBodySkeleton), kyle_800101_run_animation_tracks_match_body);

/// Kyle Madigan's looping run animation for companion resource variant 1.
///
/// Bank set 4 plays during distant following, retreat and obstacle scanning;
/// track 1 carries footstep cues. Twenty tracks supply translation/rotation for
/// parts 0 and 1 and rotation only for parts 2..19; child playback drives tracks 1..19.
/// Playback borrows the data without changing it; keep this package loaded
/// while any slot or cached record refers to the clip.
static AnimationSet _gKyle800101KyleMadiganRunAnimation = {
    .records           = _gKyle800101KyleMadiganRunAnimationRecords,
    .trackStartIndices = _gKyle800101KyleMadiganRunAnimationIndices,
    .poseBanks         = {
        [KYLE_800101_ANIMATION_TRANSLATION_ROTATION_BANK] = _gKyle800101KyleMadiganRunAnimationBank1,
        [KYLE_800101_ANIMATION_PACKED_ROTATION_BANK]      = _gKyle800101KyleMadiganRunAnimationBank4,
    },
};

/// Native clip 3's translation/rotation keys for tracks 0 and 1.
///
/// Translation uses integer model units; rotation uses 4096 units per turn.
static const AnimationPackedPose _gKyle800101Animation0DC38Bank1[] SECTION(".data") = {
#include "assets/kyle_800101_animation_0DC38_bank1.inc"
};

/// Native clip 3's rotations for tracks 2..19, in eight-unit steps over 4096 units per turn.
static const AnimationPackedRotation _gKyle800101Animation0DC38Bank4[] SECTION(".data") = {
#include "assets/kyle_800101_animation_0DC38_bank4.inc"
};

/// Native clip 3's keyframes, footstep cues and loop controls, in normal-rate frames.
///
/// Pose offsets count four-byte words; jumps use absolute record indices.
static const AnimationRecord _gKyle800101Animation0DC38Records[] SECTION(".data") = {
#include "assets/kyle_800101_animation_0DC38_records.inc"
};

/// Absolute record-array starts for native clip 3's twenty model-part tracks.
static const u16 _gKyle800101Animation0DC38Indices[] SECTION(".data") = {
#include "assets/kyle_800101_animation_0DC38_indices.inc"
};
STATIC_ASSERT(ARRAY_SIZE(_gKyle800101Animation0DC38Indices) == ARRAY_SIZE(_gKyle800101KyleMadiganBodySkeleton), kyle_800101_set3_animation_tracks_match_body);

/// Kyle Madigan's looping native clip 3 for companion resource variant 1.
///
/// Track 1 carries footstep cues; the motion's direction and a companion caller
/// are unproven. Twenty tracks encode translation/rotation for parts 0 and 1
/// and rotation only for parts 2..19. Keep this package loaded while playback
/// or cached records use the clip.
static AnimationSet _gKyle800101Animation0DC38 = {
    .records           = _gKyle800101Animation0DC38Records,
    .trackStartIndices = _gKyle800101Animation0DC38Indices,
    .poseBanks         = {
        [KYLE_800101_ANIMATION_TRANSLATION_ROTATION_BANK] = _gKyle800101Animation0DC38Bank1,
        [KYLE_800101_ANIMATION_PACKED_ROTATION_BANK]      = _gKyle800101Animation0DC38Bank4,
    },
};

AnimationSet* gKyle800101KyleMadiganAnimationBank[ANIMATION_BANK_SET_CAPACITY] = {
    [1]  = &_gKyle800101KyleMadiganIdleAnimation,
    [2]  = &_gKyle800101KyleMadiganWalkAnimation,
    [3]  = &_gKyle800101Animation0DC38,
    [4]  = &_gKyle800101KyleMadiganRunAnimation,
    [5]  = &_gKyle800101KyleMadiganTurnLeftAnimation,
    [6]  = &_gKyle800101KyleMadiganTurnRightAnimation,
    [7]  = &_gKyle800101KyleMadiganCombatEntryAnimation,
    [8]  = &_gKyle800101KyleMadiganCombatExitAnimation,
    [9]  = &_gKyle800101KyleMadiganCombatHoldAnimation,
    [10] = &_gKyle800101KyleMadiganSingleShotAnimation,
    [11] = &_gKyle800101KyleMadiganIdleAnimation,
    [12] = &_gKyle800101KyleMadiganApproachAnimation,
    [13] = &_gKyle800101Animation0A2C4,
    [14] = &_gKyle800101KyleMadiganReloadAnimation,
    [15] = &_gKyle800101KyleMadiganIdleAnimation,
    [16] = &_gKyle800101KyleMadiganPart4HitAnimation,
    [17] = &_gKyle800101KyleMadiganOtherBodyHitAnimation,
    [18] = &_gKyle800101KyleMadiganStoppedAnimation,
    [19] = &_gKyle800101KyleMadiganWalkAnimation,
    [20] = &_gKyle800101KyleMadiganReloadAnimation,
    [21] = &_gKyle800101KyleMadiganIdleAnimation,
    [22] = &_gKyle800101KyleMadiganIdleAnimation,
    [23] = &_gKyle800101KyleMadiganIdleVariationAnimation,
    [24] = &_gKyle800101KyleMadiganIdleAnimation,
    [25] = &_gKyle800101KyleMadiganIdleAnimation,
    [32] = &_gKyle800101Animation0920C,
    [33] = &_gKyle800101Animation09764,
    [34] = &_gKyle800101KyleMadiganCombatEntryAnimation,
    [35] = &_gKyle800101KyleMadiganCombatExitAnimation,
    [36] = &_gKyle800101KyleMadiganIdleAnimation,
    [37] = &_gKyle800101KyleMadiganIdleAnimation,
    [38] = &_gKyle800101KyleMadiganIdleAnimation,
    [39] = &_gKyle800101KyleMadiganIdleAnimation,
    [40] = &_gKyle800101KyleMadiganIdleAnimation,
    // Unlisted base slots are empty; the writable extension starts zeroed.
    [ANIMATION_BANK_BASE_SET_COUNT] = NULL,
};
