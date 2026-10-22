#include "kyle/kyle.h"

#include <psyq/sys/types.h>
#include <psyq/libgpu.h>

#include "types.h"

/// Initial part-local pose and parent indices copied into each body model.
/// Vertex counts for the twenty consecutive part-local groups, totalling 300.
/// Body vertices in integer part-local coordinates, ordered by skeletal part.
/// Body lighting normals with twelve fractional bits, indexed independently of vertices.
/// Writable draw commands: twenty skeletal groups and a final pre-transformed group.
///
/// First model creation resolves handler slots in place; the last word is
/// `TMD_STREAM_END`. Projection commands fill the first primitive-buffer region.
#include "assets/characters/kyle/body.inc"

/// Identity rest transform for the hand's single root, copied into each model.
/// Vertex count for the single part-local hand group (23 vertices).
/// Right-hand vertices in integer coordinates local to the hand root.
/// Hand lighting normals with twelve fractional bits, indexed independently of vertices.
/// Writable draw commands for four textured Gouraud triangles and nineteen quads.
///
/// One skeletal group and an empty final group end in `TMD_STREAM_END`.
/// First model creation resolves the command handler slots in place.
#include "assets/characters/kyle/hand_right.inc"

/// Identity rest transform for the right hand's single root, copied into each model.
/// Vertex count for the single part-local right-hand group (27 vertices).
/// Right-hand vertices in integer coordinates local to the hand root.
/// Hand lighting normals with twelve fractional bits, indexed independently of vertices.
/// Writable draw commands for two textured Gouraud triangles and twenty-four quads.
///
/// One skeletal group and an empty final group end in `TMD_STREAM_END`.
/// First model creation resolves the command handler slots in place.
#include "assets/characters/kyle/hand_right_3.inc"

/// Identity rest transform for the left hand's single root, copied into each model.
/// Vertex count for the left hand's single part (23 vertices).
/// Left-hand vertices in integer coordinates local to the hand root.
/// Hand lighting normals with twelve fractional bits, indexed independently of vertices.
/// Writable draw commands for four textured Gouraud triangles and nineteen quads.
///
/// One skeletal group and an empty final group end in `TMD_STREAM_END`.
/// First model creation resolves the command handler slots in place.
#include "assets/characters/kyle/hand_left_2.inc"

/// Identity rest transform for the left hand's single root, copied into each model.
/// Vertex count for the single part-local left-hand group (27 vertices).
/// Left-hand vertices in integer coordinates local to the hand root.
/// Hand lighting normals with twelve fractional bits, indexed independently of vertices.
/// Writable draw commands for two textured Gouraud triangles and twenty-four quads.
///
/// One skeletal group and an empty final group end in `TMD_STREAM_END`.
/// First model creation resolves the command handler slots in place.
#include "assets/characters/kyle/hand_left.inc"

/// Identity rest transform for the gun's single root, copied into each model.
/// Vertex count for the single part-local gun group (78 vertices).
/// Gun vertices in integer coordinates local to the gun root.
/// Gun lighting normals with twelve fractional bits, indexed independently of vertices.
/// Writable draw commands for two textured Gouraud triangles and 73 quads.
///
/// One skeletal group and an empty final group end in `TMD_STREAM_END`.
/// First model creation resolves the command handler slots in place.
#include "assets/characters/kyle/m4a1.inc"

/// Serialized encodings selecting the two pose-bank formats used by these clips.
enum {
    KYLE_800104_ANIMATION_TRANSLATION_ROTATION_BANK = 1,
    KYLE_800104_ANIMATION_PACKED_ROTATION_BANK      = 4
};

#include "assets/characters/kyle/companion_4_clips.inc"

/// Native set indices for these clips; the combat-exit descriptor also has a second slot.
enum {
    KYLE_800104_ANIMATION_WALK_SET              = 2,
    KYLE_800104_ANIMATION_RUN_SET               = 4,
    KYLE_800104_ANIMATION_COMBAT_EXIT_SET       = 8,
    KYLE_800104_ANIMATION_COMBAT_EXIT_ALIAS_SET = 35,
};

#include "assets/characters/kyle/companion_4_bank.inc"

/// The names the resident companion table gives this package's copies.
DEFINE_ALIAS(gKyleBody, gKyle800104Body);
DEFINE_ALIAS(gKyleHandRight, gKyle800104HandRight);
DEFINE_ALIAS(gKyleHandLeft2, gKyle800104HandLeft2);
DEFINE_ALIAS(gKyleHandLeft, gKyle800104HandLeft);
DEFINE_ALIAS(gKyleHandRight3, gKyle800104HandRight3);
