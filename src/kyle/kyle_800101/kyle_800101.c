#include "kyle/kyle.h"

#include <psyq/sys/types.h>
#include <psyq/libgpu.h>

#include "types.h"

#include "gameplay/animation.h"

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

static AnimationPackedPose _gKyle800101Animation09B48Bank1[2] = {
#include "assets/kyle_800101_animation_09B48_bank1.inc"
};

static AnimationPackedRotation _gKyle800101Animation09B48Bank4[18] = {
#include "assets/kyle_800101_animation_09B48_bank4.inc"
};

static AnimationRecord _gKyle800101Animation09B48Records[80] = {
#include "assets/kyle_800101_animation_09B48_records.inc"
};

static u16 _gKyle800101Animation09B48Indices[20] = {
#include "assets/kyle_800101_animation_09B48_indices.inc"
};

static AnimationSet _gKyle800101Animation09B48 = {
    _gKyle800101Animation09B48Records,
    _gKyle800101Animation09B48Indices,
    { NULL, _gKyle800101Animation09B48Bank1, NULL, NULL, _gKyle800101Animation09B48Bank4, NULL, NULL, NULL },
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

static AnimationPackedPose _gKyle800101Animation0B6F0Bank1[18] = {
#include "assets/kyle_800101_animation_0B6F0_bank1.inc"
};

static AnimationPackedRotation _gKyle800101Animation0B6F0Bank4[258] = {
#include "assets/kyle_800101_animation_0B6F0_bank4.inc"
};

static AnimationRecord _gKyle800101Animation0B6F0Records[337] = {
#include "assets/kyle_800101_animation_0B6F0_records.inc"
};

static u16 _gKyle800101Animation0B6F0Indices[20] = {
#include "assets/kyle_800101_animation_0B6F0_indices.inc"
};

static AnimationSet _gKyle800101Animation0B6F0 = {
    _gKyle800101Animation0B6F0Records,
    _gKyle800101Animation0B6F0Indices,
    { NULL, _gKyle800101Animation0B6F0Bank1, NULL, NULL, _gKyle800101Animation0B6F0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gKyle800101Animation0B968Bank1[2] = {
#include "assets/kyle_800101_animation_0B968_bank1.inc"
};

static AnimationPackedRotation _gKyle800101Animation0B968Bank4[50] = {
#include "assets/kyle_800101_animation_0B968_bank4.inc"
};

static AnimationRecord _gKyle800101Animation0B968Records[82] = {
#include "assets/kyle_800101_animation_0B968_records.inc"
};

static u16 _gKyle800101Animation0B968Indices[20] = {
#include "assets/kyle_800101_animation_0B968_indices.inc"
};

static AnimationSet _gKyle800101Animation0B968 = {
    _gKyle800101Animation0B968Records,
    _gKyle800101Animation0B968Indices,
    { NULL, _gKyle800101Animation0B968Bank1, NULL, NULL, _gKyle800101Animation0B968Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gKyle800101Animation0BD70Bank1[10] = {
#include "assets/kyle_800101_animation_0BD70_bank1.inc"
};

static AnimationPackedRotation _gKyle800101Animation0BD70Bank4[85] = {
#include "assets/kyle_800101_animation_0BD70_bank4.inc"
};

static AnimationRecord _gKyle800101Animation0BD70Records[123] = {
#include "assets/kyle_800101_animation_0BD70_records.inc"
};

static u16 _gKyle800101Animation0BD70Indices[20] = {
#include "assets/kyle_800101_animation_0BD70_indices.inc"
};

static AnimationSet _gKyle800101Animation0BD70 = {
    _gKyle800101Animation0BD70Records,
    _gKyle800101Animation0BD70Indices,
    { NULL, _gKyle800101Animation0BD70Bank1, NULL, NULL, _gKyle800101Animation0BD70Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gKyle800101Animation0C24CBank1[8] = {
#include "assets/kyle_800101_animation_0C24C_bank1.inc"
};

static AnimationPackedRotation _gKyle800101Animation0C24CBank4[114] = {
#include "assets/kyle_800101_animation_0C24C_bank4.inc"
};

static AnimationRecord _gKyle800101Animation0C24CRecords[153] = {
#include "assets/kyle_800101_animation_0C24C_records.inc"
};

static u16 _gKyle800101Animation0C24CIndices[20] = {
#include "assets/kyle_800101_animation_0C24C_indices.inc"
};

static AnimationSet _gKyle800101Animation0C24C = {
    _gKyle800101Animation0C24CRecords,
    _gKyle800101Animation0C24CIndices,
    { NULL, _gKyle800101Animation0C24CBank1, NULL, NULL, _gKyle800101Animation0C24CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gKyle800101Animation0CACCBank1[11] = {
#include "assets/kyle_800101_animation_0CACC_bank1.inc"
};

static AnimationPackedRotation _gKyle800101Animation0CACCBank4[201] = {
#include "assets/kyle_800101_animation_0CACC_bank4.inc"
};

static AnimationRecord _gKyle800101Animation0CACCRecords[290] = {
#include "assets/kyle_800101_animation_0CACC_records.inc"
};

static u16 _gKyle800101Animation0CACCIndices[20] = {
#include "assets/kyle_800101_animation_0CACC_indices.inc"
};

static AnimationSet _gKyle800101Animation0CACC = {
    _gKyle800101Animation0CACCRecords,
    _gKyle800101Animation0CACCIndices,
    { NULL, _gKyle800101Animation0CACCBank1, NULL, NULL, _gKyle800101Animation0CACCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gKyle800101Animation0D4D0Bank1[17] = {
#include "assets/kyle_800101_animation_0D4D0_bank1.inc"
};

static AnimationPackedRotation _gKyle800101Animation0D4D0Bank4[246] = {
#include "assets/kyle_800101_animation_0D4D0_bank4.inc"
};

static AnimationRecord _gKyle800101Animation0D4D0Records[324] = {
#include "assets/kyle_800101_animation_0D4D0_records.inc"
};

static u16 _gKyle800101Animation0D4D0Indices[20] = {
#include "assets/kyle_800101_animation_0D4D0_indices.inc"
};

static AnimationSet _gKyle800101Animation0D4D0 = {
    _gKyle800101Animation0D4D0Records,
    _gKyle800101Animation0D4D0Indices,
    { NULL, _gKyle800101Animation0D4D0Bank1, NULL, NULL, _gKyle800101Animation0D4D0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gKyle800101Animation0DC38Bank1[12] = {
#include "assets/kyle_800101_animation_0DC38_bank1.inc"
};

static AnimationPackedRotation _gKyle800101Animation0DC38Bank4[167] = {
#include "assets/kyle_800101_animation_0DC38_bank4.inc"
};

static AnimationRecord _gKyle800101Animation0DC38Records[251] = {
#include "assets/kyle_800101_animation_0DC38_records.inc"
};

static u16 _gKyle800101Animation0DC38Indices[20] = {
#include "assets/kyle_800101_animation_0DC38_indices.inc"
};

static AnimationSet _gKyle800101Animation0DC38 = {
    _gKyle800101Animation0DC38Records,
    _gKyle800101Animation0DC38Indices,
    { NULL, _gKyle800101Animation0DC38Bank1, NULL, NULL, _gKyle800101Animation0DC38Bank4, NULL, NULL, NULL },
};

AnimationBank D_kyle_800101_801756D0 = { { {
    NULL,
    &_gKyle800101KyleMadiganIdleAnimation,
    &_gKyle800101Animation0CACC,
    &_gKyle800101Animation0DC38,
    &_gKyle800101Animation0D4D0,
    &_gKyle800101KyleMadiganTurnLeftAnimation,
    &_gKyle800101KyleMadiganTurnRightAnimation,
    &_gKyle800101Animation0BD70,
    &_gKyle800101Animation0C24C,
    &_gKyle800101KyleMadiganCombatHoldAnimation,
    &_gKyle800101Animation0B968,
    &_gKyle800101KyleMadiganIdleAnimation,
    &_gKyle800101KyleMadiganApproachAnimation,
    &_gKyle800101Animation0A2C4,
    &_gKyle800101Animation0B6F0,
    &_gKyle800101KyleMadiganIdleAnimation,
    &_gKyle800101KyleMadiganPart4HitAnimation,
    &_gKyle800101KyleMadiganOtherBodyHitAnimation,
    &_gKyle800101KyleMadiganStoppedAnimation,
    &_gKyle800101Animation0CACC,
    &_gKyle800101Animation0B6F0,
    &_gKyle800101KyleMadiganIdleAnimation,
    &_gKyle800101KyleMadiganIdleAnimation,
    &_gKyle800101KyleMadiganIdleVariationAnimation,
    &_gKyle800101KyleMadiganIdleAnimation,
    &_gKyle800101KyleMadiganIdleAnimation,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    &_gKyle800101Animation0920C,
    &_gKyle800101Animation09764,
    &_gKyle800101Animation0BD70,
    &_gKyle800101Animation0C24C,
    &_gKyle800101KyleMadiganIdleAnimation,
    &_gKyle800101KyleMadiganIdleAnimation,
    &_gKyle800101KyleMadiganIdleAnimation,
    &_gKyle800101KyleMadiganIdleAnimation,
    &_gKyle800101KyleMadiganIdleAnimation,
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
