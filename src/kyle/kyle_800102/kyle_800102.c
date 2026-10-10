#include "kyle/kyle_800102.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>

#include "gte.h"
#include "types.h"

#include "kyle/kyle.h"
#include "kyle_800102_private.h"

#include "gameplay/actor.h"
#include "gameplay/actor_render.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/loading.h"
#include "gameplay/player_actor.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/world_collision.h"

#include "main/coord.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/task_types.h"

#include "weapons/weapon.h"
#include "../../shared/grenade_shell.h"

static void _grenadeShellExit(Task* task);
static void _grenadeShellSpawn(Task* task);

#include "../../shared/grenade_shell_spawn.inc.c"

#include "../../shared/grenade_shell_fly.inc.c"

#include "../../shared/grenade_shell_blast.inc.c"

#include "../../shared/grenade_shell_exit.inc.c"
#include "gameplay/animation.h"
void kyle800102GrenadeShellTask(Task* projectileTask)
{
    const TaskFunc stateHandlers[] = {
        _grenadeShellSpawn,
        _grenadeShellFly,
        _grenadeShellBlast,
        _grenadeShellExit,
    };

    stateHandlers[projectileTask->state](projectileTask);
}

/// Initial part-local pose and parent indices copied into each body model.
static TmdBone _gKyle800102KyleMadiganBodySkeleton[] = {
#include "assets/kyle_madigan_body_skeleton.inc"
};

/// Vertex counts for the twenty consecutive part-local groups, totalling 300.
static u32 _gKyle800102KyleMadiganBodyPartVerts[] = {
#include "assets/kyle_madigan_body_partVerts.inc"
};

/// Body vertices in integer part-local coordinates, ordered by skeletal part.
static SVECTOR _gKyle800102KyleMadiganBodyVerts[] = {
#include "assets/kyle_madigan_body_verts.inc"
};

/// Body lighting normals with twelve fractional bits, indexed independently of vertices.
static SVECTOR _gKyle800102KyleMadiganBodyNormals[] = {
#include "assets/kyle_madigan_body_normals.inc"
};

/// Writable draw commands: twenty skeletal groups and a final pre-transformed group.
///
/// First model creation resolves handler slots in place; the last word is
/// `TMD_STREAM_END`. Projection commands fill the first primitive-buffer region.
static u32 _gKyle800102KyleMadiganBodyStream[] = {
#include "assets/kyle_madigan_body_stream.inc"
};

TmdSource gKyle800102KyleMadiganBody = {
    .handlersResolved = false,
    // Each half holds all textured faces and one untextured Gouraud triangle.
    .bufferHalfBytes     = (204 * sizeof(POLY_GT3)) + (193 * sizeof(POLY_GT4)) + sizeof(POLY_G3),
    .preXformRegionBytes = (80 * sizeof(POLY_GT3)) + (48 * sizeof(POLY_GT4)),
    .partCount           = ARRAY_SIZE(_gKyle800102KyleMadiganBodySkeleton),
    .partVertexCounts    = _gKyle800102KyleMadiganBodyPartVerts,
    .verts               = _gKyle800102KyleMadiganBodyVerts,
    .normals             = _gKyle800102KyleMadiganBodyNormals,
    .skeleton            = _gKyle800102KyleMadiganBodySkeleton,
    .stream              = _gKyle800102KyleMadiganBodyStream,
};

/// Identity rest transform for the hand's single root, copied into each model.
static TmdBone _gKyle800102KyleMadiganHandRightSkeleton[] = {
#include "assets/kyle_madigan_hand_right_skeleton.inc"
};

/// Vertex count for the single part-local hand group (23 vertices).
static u32 _gKyle800102KyleMadiganHandRightPartVerts[] = {
#include "assets/kyle_madigan_hand_right_partVerts.inc"
};

/// Right-hand vertices in integer coordinates local to the hand root.
static SVECTOR _gKyle800102KyleMadiganHandRightVerts[] = {
#include "assets/kyle_madigan_hand_right_verts.inc"
};

/// Hand lighting normals with twelve fractional bits, indexed independently of vertices.
static SVECTOR _gKyle800102KyleMadiganHandRightNormals[] = {
#include "assets/kyle_madigan_hand_right_normals.inc"
};

/// Writable draw commands for four textured Gouraud triangles and nineteen quads.
///
/// One skeletal group and an empty final group end in `TMD_STREAM_END`.
/// First model creation resolves the command handler slots in place.
static u32 _gKyle800102KyleMadiganHandRightStream[] = {
#include "assets/kyle_madigan_hand_right_stream.inc"
};

TmdSource gKyle800102KyleMadiganHandRight = {
    .handlersResolved = false,
    .bufferHalfBytes  = (4 * sizeof(POLY_GT3)) + (19 * sizeof(POLY_GT4)),
    // All faces transform directly from the single hand part.
    .preXformRegionBytes = 0,
    .partCount           = ARRAY_SIZE(_gKyle800102KyleMadiganHandRightSkeleton),
    .partVertexCounts    = _gKyle800102KyleMadiganHandRightPartVerts,
    .verts               = _gKyle800102KyleMadiganHandRightVerts,
    .normals             = _gKyle800102KyleMadiganHandRightNormals,
    .skeleton            = _gKyle800102KyleMadiganHandRightSkeleton,
    .stream              = _gKyle800102KyleMadiganHandRightStream,
};

/// Identity rest transform for the right hand's single root, copied into each model.
static TmdBone _gKyle800102KyleMadiganHandRightSecondPairSkeleton[] = {
#include "assets/kyle_madigan_hand_right_second_pair_skeleton.inc"
};

/// Vertex count for the single part-local right-hand group (27 vertices).
static u32 _gKyle800102KyleMadiganHandRightSecondPairPartVerts[] = {
#include "assets/kyle_madigan_hand_right_second_pair_partVerts.inc"
};

/// Right-hand vertices in integer coordinates local to the hand root.
static SVECTOR _gKyle800102KyleMadiganHandRightSecondPairVerts[] = {
#include "assets/kyle_madigan_hand_right_second_pair_verts.inc"
};

/// Hand lighting normals with twelve fractional bits, indexed independently of vertices.
static SVECTOR _gKyle800102KyleMadiganHandRightSecondPairNormals[] = {
#include "assets/kyle_madigan_hand_right_second_pair_normals.inc"
};

/// Writable draw commands for two textured Gouraud triangles and twenty-four quads.
///
/// One skeletal group and an empty final group end in `TMD_STREAM_END`.
/// First model creation resolves the command handler slots in place.
static u32 _gKyle800102KyleMadiganHandRightSecondPairStream[] = {
#include "assets/kyle_madigan_hand_right_second_pair_stream.inc"
};

TmdSource gKyle800102KyleMadiganHandRightSecondPair = {
    .handlersResolved = false,
    .bufferHalfBytes  = (2 * sizeof(POLY_GT3)) + (24 * sizeof(POLY_GT4)),
    // All faces transform directly from the single hand part.
    .preXformRegionBytes = 0,
    .partCount           = ARRAY_SIZE(_gKyle800102KyleMadiganHandRightSecondPairSkeleton),
    .partVertexCounts    = _gKyle800102KyleMadiganHandRightSecondPairPartVerts,
    .verts               = _gKyle800102KyleMadiganHandRightSecondPairVerts,
    .normals             = _gKyle800102KyleMadiganHandRightSecondPairNormals,
    .skeleton            = _gKyle800102KyleMadiganHandRightSecondPairSkeleton,
    .stream              = _gKyle800102KyleMadiganHandRightSecondPairStream,
};

/// Identity rest transform for the left hand's single root, copied into each model.
static TmdBone _gKyle800102KyleMadiganHandLeftFirstPairSkeleton[] = {
#include "assets/kyle_madigan_left_skeleton.inc"
};

/// Vertex count for the left hand's single part (23 vertices).
static u32 _gKyle800102KyleMadiganHandLeftFirstPairPartVerts[] = {
#include "assets/kyle_madigan_left_partVerts.inc"
};

/// Left-hand vertices in integer coordinates local to the hand root.
static SVECTOR _gKyle800102KyleMadiganHandLeftFirstPairVerts[] = {
#include "assets/kyle_madigan_left_verts.inc"
};

/// Hand lighting normals with twelve fractional bits, indexed independently of vertices.
static SVECTOR _gKyle800102KyleMadiganHandLeftFirstPairNormals[] = {
#include "assets/kyle_madigan_left_normals.inc"
};

/// Writable draw commands for four textured Gouraud triangles and nineteen quads.
///
/// One skeletal group and an empty final group end in `TMD_STREAM_END`.
/// First model creation resolves the command handler slots in place.
static u32 _gKyle800102KyleMadiganHandLeftFirstPairStream[] = {
#include "assets/kyle_madigan_left_stream.inc"
};

TmdSource gKyle800102KyleMadiganHandLeftFirstPair = {
    .handlersResolved = false,
    .bufferHalfBytes  = (4 * sizeof(POLY_GT3)) + (19 * sizeof(POLY_GT4)),
    // All faces transform directly from the single hand part.
    .preXformRegionBytes = 0,
    .partCount           = ARRAY_SIZE(_gKyle800102KyleMadiganHandLeftFirstPairSkeleton),
    .partVertexCounts    = _gKyle800102KyleMadiganHandLeftFirstPairPartVerts,
    .verts               = _gKyle800102KyleMadiganHandLeftFirstPairVerts,
    .normals             = _gKyle800102KyleMadiganHandLeftFirstPairNormals,
    .skeleton            = _gKyle800102KyleMadiganHandLeftFirstPairSkeleton,
    .stream              = _gKyle800102KyleMadiganHandLeftFirstPairStream,
};

/// Identity rest transform for the left hand's single root, copied into each model.
static TmdBone _gKyle800102KyleMadiganHandLeftSkeleton[] = {
#include "assets/kyle_madigan_hand_left_skeleton.inc"
};

/// Vertex count for the single part-local left-hand group (27 vertices).
static u32 _gKyle800102KyleMadiganHandLeftPartVerts[] = {
#include "assets/kyle_madigan_hand_left_partVerts.inc"
};

/// Left-hand vertices in integer coordinates local to the hand root.
static SVECTOR _gKyle800102KyleMadiganHandLeftVerts[] = {
#include "assets/kyle_madigan_hand_left_verts.inc"
};

/// Hand lighting normals with twelve fractional bits, indexed independently of vertices.
static SVECTOR _gKyle800102KyleMadiganHandLeftNormals[] = {
#include "assets/kyle_madigan_hand_left_normals.inc"
};

/// Writable draw commands for two textured Gouraud triangles and twenty-four quads.
///
/// One skeletal group and an empty final group end in `TMD_STREAM_END`.
/// First model creation resolves the command handler slots in place.
static u32 _gKyle800102KyleMadiganHandLeftStream[] = {
#include "assets/kyle_madigan_hand_left_stream.inc"
};

TmdSource gKyle800102KyleMadiganHandLeft = {
    .handlersResolved = false,
    .bufferHalfBytes  = (2 * sizeof(POLY_GT3)) + (24 * sizeof(POLY_GT4)),
    // All faces transform directly from the single hand part.
    .preXformRegionBytes = 0,
    .partCount           = ARRAY_SIZE(_gKyle800102KyleMadiganHandLeftSkeleton),
    .partVertexCounts    = _gKyle800102KyleMadiganHandLeftPartVerts,
    .verts               = _gKyle800102KyleMadiganHandLeftVerts,
    .normals             = _gKyle800102KyleMadiganHandLeftNormals,
    .skeleton            = _gKyle800102KyleMadiganHandLeftSkeleton,
    .stream              = _gKyle800102KyleMadiganHandLeftStream,
};

/// Identity rest transform for the launcher's single root, copied into each model.
static TmdBone _gKyle800102KyleMadiganMm1Skeleton[] = {
#include "assets/kyle_800102_model_069CC_skeleton.inc"
};

/// Vertex count for the single part-local launcher group (52 vertices).
static u32 _gKyle800102KyleMadiganMm1PartVerts[] = {
#include "assets/kyle_800102_model_069CC_partVerts.inc"
};

/// Launcher vertices in integer coordinates local to its root.
static SVECTOR _gKyle800102KyleMadiganMm1Verts[] = {
#include "assets/kyle_800102_model_069CC_verts.inc"
};

/// Launcher lighting normals with twelve fractional bits, indexed independently of vertices.
static SVECTOR _gKyle800102KyleMadiganMm1Normals[] = {
#include "assets/kyle_800102_model_069CC_normals.inc"
};

/// Writable draw commands for four textured Gouraud triangles and forty-one quads.
///
/// One skeletal group and an empty final group end in `TMD_STREAM_END`.
/// First model creation resolves the command handler slots in place.
static u32 _gKyle800102KyleMadiganMm1Stream[] = {
#include "assets/kyle_800102_model_069CC_stream.inc"
};

TmdSource gKyle800102KyleMadiganMm1 = {
    .handlersResolved = false,
    .bufferHalfBytes  = (4 * sizeof(POLY_GT3)) + (41 * sizeof(POLY_GT4)),
    // All faces transform directly from the single launcher part.
    .preXformRegionBytes = 0,
    .partCount           = ARRAY_SIZE(_gKyle800102KyleMadiganMm1Skeleton),
    .partVertexCounts    = _gKyle800102KyleMadiganMm1PartVerts,
    .verts               = _gKyle800102KyleMadiganMm1Verts,
    .normals             = _gKyle800102KyleMadiganMm1Normals,
    .skeleton            = _gKyle800102KyleMadiganMm1Skeleton,
    .stream              = _gKyle800102KyleMadiganMm1Stream,
};

/// Encoding-indexed pose banks used by this package's body clips.
enum {
    KYLE_800102_ANIMATION_TRANSLATION_ROTATION_BANK = 1,
    KYLE_800102_ANIMATION_PACKED_ROTATION_BANK      = 4
};

/// Idle translation/rotation poses: integer local XYZ and angles in 1/4096 turns.
static AnimationPackedPose _gKyle800102KyleMadiganIdleAnimationBank1[] = {
#include "assets/kyle_800102_animation_070F8_bank1.inc"
};

/// Idle rotation-only poses, packed as signed 11/10/11-bit angles in eight-unit steps.
static AnimationPackedRotation _gKyle800102KyleMadiganIdleAnimationBank4[] = {
#include "assets/kyle_800102_animation_070F8_bank4.inc"
};

/// Idle keyframes and loop controls; durations count whole normal-rate frames.
static AnimationRecord _gKyle800102KyleMadiganIdleAnimationRecords[] = {
#include "assets/kyle_800102_animation_070F8_records.inc"
};

/// Absolute record starts for the body's twenty part tracks during idle.
static u16 _gKyle800102KyleMadiganIdleAnimationIndices[] = {
#include "assets/kyle_800102_animation_070F8_indices.inc"
};

/// Kyle Madigan's looping standing idle for companion resource variant 2.
///
/// Bank set 1 and its aliases share twenty body-part tracks.
/// Playback borrows these records and word-aligned pose banks; keep the
/// package loaded while any playback slot or cached record refers to them.
static AnimationSet _gKyle800102KyleMadiganIdleAnimation = {
    .records           = _gKyle800102KyleMadiganIdleAnimationRecords,
    .trackStartIndices = _gKyle800102KyleMadiganIdleAnimationIndices,
    .poseBanks         = {
        [KYLE_800102_ANIMATION_TRANSLATION_ROTATION_BANK] = _gKyle800102KyleMadiganIdleAnimationBank1,
        [KYLE_800102_ANIMATION_PACKED_ROTATION_BANK]      = _gKyle800102KyleMadiganIdleAnimationBank4,
    },
};

/// Unarmed-walk translation/rotation poses: integer local XYZ and angles in 1/4096 turns.
static AnimationPackedPose _gKyle800102KyleMadiganUnarmedWalkAnimationBank1[] = {
#include "assets/kyle_800102_animation_07978_bank1.inc"
};

/// Unarmed-walk rotation-only poses, packed as signed 11/10/11-bit angles in eight-unit steps.
static AnimationPackedRotation _gKyle800102KyleMadiganUnarmedWalkAnimationBank4[] = {
#include "assets/kyle_800102_animation_07978_bank4.inc"
};

/// Unarmed-walk keyframes and loop controls; durations count whole normal-rate frames.
static AnimationRecord _gKyle800102KyleMadiganUnarmedWalkAnimationRecords[] = {
#include "assets/kyle_800102_animation_07978_records.inc"
};

/// Absolute record starts for the body's twenty part tracks during unarmed walking.
static u16 _gKyle800102KyleMadiganUnarmedWalkAnimationIndices[] = {
#include "assets/kyle_800102_animation_07978_indices.inc"
};

/// Kyle Madigan's looping unarmed walk for companion resource variant 2.
///
/// Bank set 19 supplies twenty body-part tracks for scripted walking after
/// the held weapon is removed. Playback borrows these records and word-aligned
/// pose banks; keep the package loaded while any slot or cached record uses them.
static AnimationSet _gKyle800102KyleMadiganUnarmedWalkAnimation = {
    .records           = _gKyle800102KyleMadiganUnarmedWalkAnimationRecords,
    .trackStartIndices = _gKyle800102KyleMadiganUnarmedWalkAnimationIndices,
    .poseBanks         = {
        [KYLE_800102_ANIMATION_TRANSLATION_ROTATION_BANK] = _gKyle800102KyleMadiganUnarmedWalkAnimationBank1,
        [KYLE_800102_ANIMATION_PACKED_ROTATION_BANK]      = _gKyle800102KyleMadiganUnarmedWalkAnimationBank4,
    },
};

/// Left-turn translation/rotation poses: integer local XYZ and angles in 1/4096 turns.
static AnimationPackedPose _gKyle800102KyleMadiganTurnLeftAnimationBank1[] = {
#include "assets/kyle_800102_animation_081E4_bank1.inc"
};

/// Left-turn rotation-only poses, packed as signed 11/10/11-bit angles in eight-unit steps.
static AnimationPackedRotation _gKyle800102KyleMadiganTurnLeftAnimationBank4[] = {
#include "assets/kyle_800102_animation_081E4_bank4.inc"
};

/// Left-turn keyframes and loop controls; durations count whole normal-rate frames.
static AnimationRecord _gKyle800102KyleMadiganTurnLeftAnimationRecords[] = {
#include "assets/kyle_800102_animation_081E4_records.inc"
};

/// Absolute record starts for the body's twenty part tracks during left turns.
static u16 _gKyle800102KyleMadiganTurnLeftAnimationIndices[] = {
#include "assets/kyle_800102_animation_081E4_indices.inc"
};

/// Kyle Madigan's looping left turn for companion resource variant 2.
///
/// Bank set 5 supplies twenty body-part tracks while the actor turns through
/// negative yaw. Playback borrows these records and word-aligned pose banks;
/// keep the package loaded while any slot or cached record refers to them.
static AnimationSet _gKyle800102KyleMadiganTurnLeftAnimation = {
    .records           = _gKyle800102KyleMadiganTurnLeftAnimationRecords,
    .trackStartIndices = _gKyle800102KyleMadiganTurnLeftAnimationIndices,
    .poseBanks         = {
        [KYLE_800102_ANIMATION_TRANSLATION_ROTATION_BANK] = _gKyle800102KyleMadiganTurnLeftAnimationBank1,
        [KYLE_800102_ANIMATION_PACKED_ROTATION_BANK]      = _gKyle800102KyleMadiganTurnLeftAnimationBank4,
    },
};

/// Right-turn translation/rotation poses: integer local XYZ and angles in 1/4096 turns.
static AnimationPackedPose _gKyle800102KyleMadiganTurnRightAnimationBank1[] = {
#include "assets/kyle_800102_animation_08AB8_bank1.inc"
};

/// Right-turn rotation-only poses, packed as signed 11/10/11-bit angles in eight-unit steps.
static AnimationPackedRotation _gKyle800102KyleMadiganTurnRightAnimationBank4[] = {
#include "assets/kyle_800102_animation_08AB8_bank4.inc"
};

/// Right-turn keyframes and loop controls; durations count whole normal-rate frames.
static AnimationRecord _gKyle800102KyleMadiganTurnRightAnimationRecords[] = {
#include "assets/kyle_800102_animation_08AB8_records.inc"
};

/// Absolute record starts for the body's twenty part tracks during right turns.
static u16 _gKyle800102KyleMadiganTurnRightAnimationIndices[] = {
#include "assets/kyle_800102_animation_08AB8_indices.inc"
};

/// Kyle Madigan's looping right turn for companion resource variant 2.
///
/// Bank set 6 supplies twenty body-part tracks while the actor turns through
/// positive yaw. Playback borrows these records and word-aligned pose banks;
/// keep the package loaded while any slot or cached record refers to them.
static AnimationSet _gKyle800102KyleMadiganTurnRightAnimation = {
    .records           = _gKyle800102KyleMadiganTurnRightAnimationRecords,
    .trackStartIndices = _gKyle800102KyleMadiganTurnRightAnimationIndices,
    .poseBanks         = {
        [KYLE_800102_ANIMATION_TRANSLATION_ROTATION_BANK] = _gKyle800102KyleMadiganTurnRightAnimationBank1,
        [KYLE_800102_ANIMATION_PACKED_ROTATION_BANK]      = _gKyle800102KyleMadiganTurnRightAnimationBank4,
    },
};

/// Head-hit translation/rotation poses: integer local XYZ and angles in 1/4096 turns.
static const AnimationPackedPose _gKyle800102KyleMadiganHeadHitAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/kyle_800102_animation_08F78_bank1.inc"
};

/// Head-hit rotation-only poses, packed as signed 11/10/11-bit angles in eight-unit steps.
static const AnimationPackedRotation _gKyle800102KyleMadiganHeadHitAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/kyle_800102_animation_08F78_bank4.inc"
};

/// Head-hit keyframes and stop controls; durations count whole normal-rate frames.
static const AnimationRecord _gKyle800102KyleMadiganHeadHitAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/kyle_800102_animation_08F78_records.inc"
};

/// Absolute record starts for the body's twenty part tracks during head-hit recovery.
static const u16 _gKyle800102KyleMadiganHeadHitAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/kyle_800102_animation_08F78_indices.inc"
};

STATIC_ASSERT(ARRAY_SIZE(_gKyle800102KyleMadiganHeadHitAnimationIndices) == ARRAY_SIZE(_gKyle800102KyleMadiganBodySkeleton), kyle800102HeadHitTrackCount);

/// Kyle Madigan's head-hit reaction for companion resource variant 2.
///
/// Bank set 16 supplies twenty body-part tracks when the head's collision sphere
/// is hit. Tracks 0 and 1 carry translation/rotation; tracks 2..19 carry rotation
/// only. Playback borrows the records and word-aligned pose banks in this
/// package; keep it loaded while any slot or cached record refers to them.
static AnimationSet _gKyle800102KyleMadiganHeadHitAnimation = {
    .records           = _gKyle800102KyleMadiganHeadHitAnimationRecords,
    .trackStartIndices = _gKyle800102KyleMadiganHeadHitAnimationIndices,
    .poseBanks         = {
        [KYLE_800102_ANIMATION_TRANSLATION_ROTATION_BANK] = _gKyle800102KyleMadiganHeadHitAnimationBank1,
        [KYLE_800102_ANIMATION_PACKED_ROTATION_BANK]      = _gKyle800102KyleMadiganHeadHitAnimationBank4,
    },
};

/// Body-hit translation/rotation poses: integer local XYZ and angles in 1/4096 turns.
static const AnimationPackedPose _gKyle800102KyleMadiganBodyHitAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/kyle_800102_animation_0951C_bank1.inc"
};

/// Body-hit rotation-only poses, packed as signed 11/10/11-bit angles in eight-unit steps.
static const AnimationPackedRotation _gKyle800102KyleMadiganBodyHitAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/kyle_800102_animation_0951C_bank4.inc"
};

/// Body-hit keyframes and stop controls; durations count whole normal-rate frames.
static const AnimationRecord _gKyle800102KyleMadiganBodyHitAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/kyle_800102_animation_0951C_records.inc"
};

/// Absolute record starts for the body's twenty part tracks during body-hit recovery.
static const u16 _gKyle800102KyleMadiganBodyHitAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/kyle_800102_animation_0951C_indices.inc"
};

STATIC_ASSERT(ARRAY_SIZE(_gKyle800102KyleMadiganBodyHitAnimationIndices) == ARRAY_SIZE(_gKyle800102KyleMadiganBodySkeleton), kyle800102BodyHitTrackCount);

/// Kyle Madigan's hit reaction for body contacts outside the head, in resource variant 2.
///
/// Bank set 17 supplies twenty body-part tracks. Tracks 0 and 1 carry
/// translation/rotation; tracks 2..19 carry rotation only. Playback borrows
/// the records and word-aligned pose banks in this package; keep it loaded
/// while any slot or cached record refers to them.
static AnimationSet _gKyle800102KyleMadiganBodyHitAnimation = {
    .records           = _gKyle800102KyleMadiganBodyHitAnimationRecords,
    .trackStartIndices = _gKyle800102KyleMadiganBodyHitAnimationIndices,
    .poseBanks         = {
        [KYLE_800102_ANIMATION_TRANSLATION_ROTATION_BANK] = _gKyle800102KyleMadiganBodyHitAnimationBank1,
        [KYLE_800102_ANIMATION_PACKED_ROTATION_BANK]      = _gKyle800102KyleMadiganBodyHitAnimationBank4,
    },
};

/// Collapse translation/rotation poses: integer local XYZ and angles in 1/4096 turns.
static const AnimationPackedPose _gKyle800102KyleMadiganCollapseAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/kyle_800102_animation_09A8C_bank1.inc"
};

/// Collapse rotation-only poses, packed as signed 11/10/11-bit angles in eight-unit steps.
static const AnimationPackedRotation _gKyle800102KyleMadiganCollapseAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/kyle_800102_animation_09A8C_bank4.inc"
};

/// Collapse keyframes and stop controls that hold the final pose; durations count normal-rate frames.
static const AnimationRecord _gKyle800102KyleMadiganCollapseAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/kyle_800102_animation_09A8C_records.inc"
};

/// Absolute record starts for the body's twenty part tracks during collapse.
static const u16 _gKyle800102KyleMadiganCollapseAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/kyle_800102_animation_09A8C_indices.inc"
};

STATIC_ASSERT(ARRAY_SIZE(_gKyle800102KyleMadiganCollapseAnimationIndices) == ARRAY_SIZE(_gKyle800102KyleMadiganBodySkeleton), kyle800102CollapseTrackCount);

/// Kyle Madigan's collapse into a held fallen pose for companion resource variant 2.
///
/// Bank set 18 supplies twenty body-part tracks when companion HP reaches zero.
/// Tracks 0 and 1 carry translation/rotation; tracks 2..19 carry rotation only.
/// Playback borrows the records and word-aligned pose banks in this package;
/// keep it loaded while any slot or cached record refers to them.
static AnimationSet _gKyle800102KyleMadiganCollapseAnimation = {
    .records           = _gKyle800102KyleMadiganCollapseAnimationRecords,
    .trackStartIndices = _gKyle800102KyleMadiganCollapseAnimationIndices,
    .poseBanks         = {
        [KYLE_800102_ANIMATION_TRANSLATION_ROTATION_BANK] = _gKyle800102KyleMadiganCollapseAnimationBank1,
        [KYLE_800102_ANIMATION_PACKED_ROTATION_BANK]      = _gKyle800102KyleMadiganCollapseAnimationBank4,
    },
};

/// Idle-variation translation/rotation poses: integer local XYZ and angles in 1/4096 turns.
static const AnimationPackedPose _gKyle800102KyleMadiganIdleVariationAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/kyle_800102_animation_0A298_bank1.inc"
};

/// Idle-variation rotation-only poses, packed as signed 11/10/11-bit angles in eight-unit steps.
static const AnimationPackedRotation _gKyle800102KyleMadiganIdleVariationAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/kyle_800102_animation_0A298_bank4.inc"
};

/// Idle-variation keyframes and loop controls; durations count whole normal-rate frames.
static const AnimationRecord _gKyle800102KyleMadiganIdleVariationAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/kyle_800102_animation_0A298_records.inc"
};

/// Absolute record starts for the body's twenty part tracks in the idle variation.
static const u16 _gKyle800102KyleMadiganIdleVariationAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/kyle_800102_animation_0A298_indices.inc"
};

STATIC_ASSERT(ARRAY_SIZE(_gKyle800102KyleMadiganIdleVariationAnimationIndices) == ARRAY_SIZE(_gKyle800102KyleMadiganBodySkeleton), kyle800102IdleVariationTrackCount);

/// Kyle Madigan's looping alternate idle for companion resource variant 2.
///
/// Bank set 23 supplies twenty body-part tracks after several idle decisions.
/// Tracks 0 and 1 carry translation/rotation; tracks 2..19 carry rotation only.
/// Playback borrows the records and word-aligned pose banks in this package;
/// keep it loaded while any slot or cached record refers to them.
static AnimationSet _gKyle800102KyleMadiganIdleVariationAnimation = {
    .records           = _gKyle800102KyleMadiganIdleVariationAnimationRecords,
    .trackStartIndices = _gKyle800102KyleMadiganIdleVariationAnimationIndices,
    .poseBanks         = {
        [KYLE_800102_ANIMATION_TRANSLATION_ROTATION_BANK] = _gKyle800102KyleMadiganIdleVariationAnimationBank1,
        [KYLE_800102_ANIMATION_PACKED_ROTATION_BANK]      = _gKyle800102KyleMadiganIdleVariationAnimationBank4,
    },
};

/// Kneeling translation/rotation poses: integer local XYZ and angles in 1/4096 turns.
static const AnimationPackedPose _gKyle800102KyleMadiganKneelAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/kyle_800102_animation_0A768_bank1.inc"
};

/// Kneeling rotation-only poses, packed as signed 11/10/11-bit angles in eight-unit steps.
static const AnimationPackedRotation _gKyle800102KyleMadiganKneelAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/kyle_800102_animation_0A768_bank4.inc"
};

/// Kneeling keyframes and stop controls that hold the final pose; durations count normal-rate frames.
static const AnimationRecord _gKyle800102KyleMadiganKneelAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/kyle_800102_animation_0A768_records.inc"
};

/// Absolute record starts for the body's twenty part tracks while kneeling down.
static const u16 _gKyle800102KyleMadiganKneelAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/kyle_800102_animation_0A768_indices.inc"
};

STATIC_ASSERT(ARRAY_SIZE(_gKyle800102KyleMadiganKneelAnimationIndices) == ARRAY_SIZE(_gKyle800102KyleMadiganBodySkeleton), kyle800102KneelTrackCount);

/// Kyle Madigan's transition from standing to one knee for companion resource variant 2.
///
/// Bank set 32 supplies twenty body-part tracks for scripted playback, paired
/// with `_gKyle800102KyleMadiganRiseFromKneelAnimation`. Tracks 0 and 1 carry
/// translation/rotation; tracks 2..19 carry rotation only. Playback borrows
/// the records and word-aligned pose banks in this package; keep it loaded
/// while any slot or cached record refers to them.
static AnimationSet _gKyle800102KyleMadiganKneelAnimation = {
    .records           = _gKyle800102KyleMadiganKneelAnimationRecords,
    .trackStartIndices = _gKyle800102KyleMadiganKneelAnimationIndices,
    .poseBanks         = {
        [KYLE_800102_ANIMATION_TRANSLATION_ROTATION_BANK] = _gKyle800102KyleMadiganKneelAnimationBank1,
        [KYLE_800102_ANIMATION_PACKED_ROTATION_BANK]      = _gKyle800102KyleMadiganKneelAnimationBank4,
    },
};

/// Rising translation/rotation poses: integer local XYZ and angles in 1/4096 turns.
static const AnimationPackedPose _gKyle800102KyleMadiganRiseFromKneelAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/kyle_800102_animation_0ACC0_bank1.inc"
};

/// Rising rotation-only poses, packed as signed 11/10/11-bit angles in eight-unit steps.
static const AnimationPackedRotation _gKyle800102KyleMadiganRiseFromKneelAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/kyle_800102_animation_0ACC0_bank4.inc"
};

/// Rising keyframes and stop controls that hold the final pose; durations count normal-rate frames.
static const AnimationRecord _gKyle800102KyleMadiganRiseFromKneelAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/kyle_800102_animation_0ACC0_records.inc"
};

/// Absolute record starts for the body's twenty part tracks while rising from one knee.
static const u16 _gKyle800102KyleMadiganRiseFromKneelAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/kyle_800102_animation_0ACC0_indices.inc"
};

STATIC_ASSERT(ARRAY_SIZE(_gKyle800102KyleMadiganRiseFromKneelAnimationIndices) == ARRAY_SIZE(_gKyle800102KyleMadiganBodySkeleton), kyle800102RiseFromKneelTrackCount);

/// Kyle Madigan's return from one knee to standing for companion resource variant 2.
///
/// Bank set 33 supplies twenty body-part tracks for scripted playback, paired
/// with `_gKyle800102KyleMadiganKneelAnimation`. Tracks 0 and 1 carry
/// translation/rotation; tracks 2..19 carry rotation only. Playback borrows
/// the records and word-aligned pose banks in this package; keep it loaded
/// while any slot or cached record refers to them.
static AnimationSet _gKyle800102KyleMadiganRiseFromKneelAnimation = {
    .records           = _gKyle800102KyleMadiganRiseFromKneelAnimationRecords,
    .trackStartIndices = _gKyle800102KyleMadiganRiseFromKneelAnimationIndices,
    .poseBanks         = {
        [KYLE_800102_ANIMATION_TRANSLATION_ROTATION_BANK] = _gKyle800102KyleMadiganRiseFromKneelAnimationBank1,
        [KYLE_800102_ANIMATION_PACKED_ROTATION_BANK]      = _gKyle800102KyleMadiganRiseFromKneelAnimationBank4,
    },
};

static AnimationPackedPose _gKyle800102Animation0AEB0Bank1[2] = {
#include "assets/kyle_800102_animation_0AEB0_bank1.inc"
};

static AnimationPackedRotation _gKyle800102Animation0AEB0Bank4[18] = {
#include "assets/kyle_800102_animation_0AEB0_bank4.inc"
};

static AnimationRecord _gKyle800102Animation0AEB0Records[80] = {
#include "assets/kyle_800102_animation_0AEB0_records.inc"
};

static u16 _gKyle800102Animation0AEB0Indices[20] = {
#include "assets/kyle_800102_animation_0AEB0_indices.inc"
};

static AnimationSet _gKyle800102Animation0AEB0 = {
    _gKyle800102Animation0AEB0Records,
    _gKyle800102Animation0AEB0Indices,
    { NULL, _gKyle800102Animation0AEB0Bank1, NULL, NULL, _gKyle800102Animation0AEB0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gKyle800102Animation0B620Bank1[13] = {
#include "assets/kyle_800102_animation_0B620_bank1.inc"
};

static AnimationPackedRotation _gKyle800102Animation0B620Bank4[175] = {
#include "assets/kyle_800102_animation_0B620_bank4.inc"
};

static AnimationRecord _gKyle800102Animation0B620Records[242] = {
#include "assets/kyle_800102_animation_0B620_records.inc"
};

static u16 _gKyle800102Animation0B620Indices[20] = {
#include "assets/kyle_800102_animation_0B620_indices.inc"
};

static AnimationSet _gKyle800102Animation0B620 = {
    _gKyle800102Animation0B620Records,
    _gKyle800102Animation0B620Indices,
    { NULL, _gKyle800102Animation0B620Bank1, NULL, NULL, _gKyle800102Animation0B620Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gKyle800102Animation0C030Bank1[16] = {
#include "assets/kyle_800102_animation_0C030_bank1.inc"
};

static AnimationPackedRotation _gKyle800102Animation0C030Bank4[250] = {
#include "assets/kyle_800102_animation_0C030_bank4.inc"
};

static AnimationRecord _gKyle800102Animation0C030Records[326] = {
#include "assets/kyle_800102_animation_0C030_records.inc"
};

static u16 _gKyle800102Animation0C030Indices[20] = {
#include "assets/kyle_800102_animation_0C030_indices.inc"
};

static AnimationSet _gKyle800102Animation0C030 = {
    _gKyle800102Animation0C030Records,
    _gKyle800102Animation0C030Indices,
    { NULL, _gKyle800102Animation0C030Bank1, NULL, NULL, _gKyle800102Animation0C030Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gKyle800102Animation0CCD8Bank1[23] = {
#include "assets/kyle_800102_animation_0CCD8_bank1.inc"
};

static AnimationPackedRotation _gKyle800102Animation0CCD8Bank4[303] = {
#include "assets/kyle_800102_animation_0CCD8_bank4.inc"
};

static AnimationRecord _gKyle800102Animation0CCD8Records[418] = {
#include "assets/kyle_800102_animation_0CCD8_records.inc"
};

static u16 _gKyle800102Animation0CCD8Indices[20] = {
#include "assets/kyle_800102_animation_0CCD8_indices.inc"
};

static AnimationSet _gKyle800102Animation0CCD8 = {
    _gKyle800102Animation0CCD8Records,
    _gKyle800102Animation0CCD8Indices,
    { NULL, _gKyle800102Animation0CCD8Bank1, NULL, NULL, _gKyle800102Animation0CCD8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gKyle800102Animation0D084Bank1[5] = {
#include "assets/kyle_800102_animation_0D084_bank1.inc"
};

static AnimationPackedRotation _gKyle800102Animation0D084Bank4[82] = {
#include "assets/kyle_800102_animation_0D084_bank4.inc"
};

static AnimationRecord _gKyle800102Animation0D084Records[118] = {
#include "assets/kyle_800102_animation_0D084_records.inc"
};

static u16 _gKyle800102Animation0D084Indices[20] = {
#include "assets/kyle_800102_animation_0D084_indices.inc"
};

static AnimationSet _gKyle800102Animation0D084 = {
    _gKyle800102Animation0D084Records,
    _gKyle800102Animation0D084Indices,
    { NULL, _gKyle800102Animation0D084Bank1, NULL, NULL, _gKyle800102Animation0D084Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gKyle800102Animation0D648Bank1[10] = {
#include "assets/kyle_800102_animation_0D648_bank1.inc"
};

static AnimationPackedRotation _gKyle800102Animation0D648Bank4[139] = {
#include "assets/kyle_800102_animation_0D648_bank4.inc"
};

static AnimationRecord _gKyle800102Animation0D648Records[180] = {
#include "assets/kyle_800102_animation_0D648_records.inc"
};

static u16 _gKyle800102Animation0D648Indices[20] = {
#include "assets/kyle_800102_animation_0D648_indices.inc"
};

static AnimationSet _gKyle800102Animation0D648 = {
    _gKyle800102Animation0D648Records,
    _gKyle800102Animation0D648Indices,
    { NULL, _gKyle800102Animation0D648Bank1, NULL, NULL, _gKyle800102Animation0D648Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gKyle800102Animation0DCBCBank1[12] = {
#include "assets/kyle_800102_animation_0DCBC_bank1.inc"
};

static AnimationPackedRotation _gKyle800102Animation0DCBCBank4[158] = {
#include "assets/kyle_800102_animation_0DCBC_bank4.inc"
};

static AnimationRecord _gKyle800102Animation0DCBCRecords[199] = {
#include "assets/kyle_800102_animation_0DCBC_records.inc"
};

static u16 _gKyle800102Animation0DCBCIndices[20] = {
#include "assets/kyle_800102_animation_0DCBC_indices.inc"
};

static AnimationSet _gKyle800102Animation0DCBC = {
    _gKyle800102Animation0DCBCRecords,
    _gKyle800102Animation0DCBCIndices,
    { NULL, _gKyle800102Animation0DCBCBank1, NULL, NULL, _gKyle800102Animation0DCBCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gKyle800102Animation0E550Bank1[11] = {
#include "assets/kyle_800102_animation_0E550_bank1.inc"
};

static AnimationPackedRotation _gKyle800102Animation0E550Bank4[206] = {
#include "assets/kyle_800102_animation_0E550_bank4.inc"
};

static AnimationRecord _gKyle800102Animation0E550Records[290] = {
#include "assets/kyle_800102_animation_0E550_records.inc"
};

static u16 _gKyle800102Animation0E550Indices[20] = {
#include "assets/kyle_800102_animation_0E550_indices.inc"
};

static AnimationSet _gKyle800102Animation0E550 = {
    _gKyle800102Animation0E550Records,
    _gKyle800102Animation0E550Indices,
    { NULL, _gKyle800102Animation0E550Bank1, NULL, NULL, _gKyle800102Animation0E550Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gKyle800102Animation0ECA4Bank1[12] = {
#include "assets/kyle_800102_animation_0ECA4_bank1.inc"
};

static AnimationPackedRotation _gKyle800102Animation0ECA4Bank4[165] = {
#include "assets/kyle_800102_animation_0ECA4_bank4.inc"
};

static AnimationRecord _gKyle800102Animation0ECA4Records[248] = {
#include "assets/kyle_800102_animation_0ECA4_records.inc"
};

static u16 _gKyle800102Animation0ECA4Indices[20] = {
#include "assets/kyle_800102_animation_0ECA4_indices.inc"
};

static AnimationSet _gKyle800102Animation0ECA4 = {
    _gKyle800102Animation0ECA4Records,
    _gKyle800102Animation0ECA4Indices,
    { NULL, _gKyle800102Animation0ECA4Bank1, NULL, NULL, _gKyle800102Animation0ECA4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gKyle800102Animation0F850Bank1[17] = {
#include "assets/kyle_800102_animation_0F850_bank1.inc"
};

static AnimationPackedRotation _gKyle800102Animation0F850Bank4[298] = {
#include "assets/kyle_800102_animation_0F850_bank4.inc"
};

static AnimationRecord _gKyle800102Animation0F850Records[378] = {
#include "assets/kyle_800102_animation_0F850_records.inc"
};

static u16 _gKyle800102Animation0F850Indices[20] = {
#include "assets/kyle_800102_animation_0F850_indices.inc"
};

static AnimationSet _gKyle800102Animation0F850 = {
    _gKyle800102Animation0F850Records,
    _gKyle800102Animation0F850Indices,
    { NULL, _gKyle800102Animation0F850Bank1, NULL, NULL, _gKyle800102Animation0F850Bank4, NULL, NULL, NULL },
};

AnimationBank D_kyle_800102_801772E8 = { { {
    NULL,
    &_gKyle800102KyleMadiganIdleAnimation,
    &_gKyle800102Animation0E550,
    &_gKyle800102Animation0ECA4,
    &_gKyle800102Animation0F850,
    &_gKyle800102KyleMadiganTurnLeftAnimation,
    &_gKyle800102KyleMadiganTurnRightAnimation,
    &_gKyle800102Animation0D648,
    &_gKyle800102Animation0DCBC,
    &_gKyle800102Animation0AEB0,
    &_gKyle800102Animation0D084,
    &_gKyle800102KyleMadiganIdleAnimation,
    &_gKyle800102Animation0C030,
    &_gKyle800102Animation0B620,
    &_gKyle800102Animation0CCD8,
    &_gKyle800102KyleMadiganIdleAnimation,
    &_gKyle800102KyleMadiganHeadHitAnimation,
    &_gKyle800102KyleMadiganBodyHitAnimation,
    &_gKyle800102KyleMadiganCollapseAnimation,
    &_gKyle800102KyleMadiganUnarmedWalkAnimation,
    &_gKyle800102Animation0CCD8,
    &_gKyle800102KyleMadiganIdleAnimation,
    &_gKyle800102KyleMadiganIdleAnimation,
    &_gKyle800102KyleMadiganIdleVariationAnimation,
    &_gKyle800102KyleMadiganIdleAnimation,
    &_gKyle800102KyleMadiganIdleAnimation,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    &_gKyle800102KyleMadiganKneelAnimation,
    &_gKyle800102KyleMadiganRiseFromKneelAnimation,
    &_gKyle800102Animation0D648,
    &_gKyle800102Animation0DCBC,
    &_gKyle800102KyleMadiganIdleAnimation,
    &_gKyle800102KyleMadiganIdleAnimation,
    &_gKyle800102KyleMadiganIdleAnimation,
    &_gKyle800102KyleMadiganIdleAnimation,
    &_gKyle800102KyleMadiganIdleAnimation,
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

SVECTOR gGrenadeShellMuzzleOffsets[2] = {
    { 0, 0x1E0, 0x80, 0 },
    { 0, 0x220, 0x80, 0 },
};

u16 gGrenadeShellBlastRadii[4] = { 0x1F4, 0x4B0, 0x7D0, 0 };

u8 gGrenadeShellSpeeds[4] = { 0x0C, 0x08, 0, 0 };

/// Identity rest transform for the shell's single root, copied into each model.
static TmdBone _gKyle800102Mm1GrenadeShellSkeleton[] = {
#include "assets/kyle_800102_model_0FA78_skeleton.inc"
};

/// Vertex count for the single part-local shell group (8 vertices).
static u32 _gKyle800102Mm1GrenadeShellPartVerts[] = {
#include "assets/kyle_800102_model_0FA78_partVerts.inc"
};

/// Shell vertices in integer coordinates local to its root.
static SVECTOR _gKyle800102Mm1GrenadeShellVerts[] = {
#include "assets/kyle_800102_model_0FA78_verts.inc"
};

/// Shell lighting normals with twelve fractional bits, indexed independently of vertices.
static SVECTOR _gKyle800102Mm1GrenadeShellNormals[] = {
#include "assets/kyle_800102_model_0FA78_normals.inc"
};

/// Writable draw commands for six textured Gouraud quads.
///
/// One skeletal group and an empty final group end in `TMD_STREAM_END`.
/// First model creation resolves the command handler slots in place.
static u32 _gKyle800102Mm1GrenadeShellStream[] = {
#include "assets/kyle_800102_model_0FA78_stream.inc"
};

TmdSource gKyle800102Mm1GrenadeShell = {
    .handlersResolved = false,
    .bufferHalfBytes  = 6 * sizeof(POLY_GT4),
    // All faces transform directly from the single shell part.
    .preXformRegionBytes = 0,
    .partCount           = ARRAY_SIZE(_gKyle800102Mm1GrenadeShellSkeleton),
    .partVertexCounts    = _gKyle800102Mm1GrenadeShellPartVerts,
    .verts               = _gKyle800102Mm1GrenadeShellVerts,
    .normals             = _gKyle800102Mm1GrenadeShellNormals,
    .skeleton            = _gKyle800102Mm1GrenadeShellSkeleton,
    .stream              = _gKyle800102Mm1GrenadeShellStream,
};
