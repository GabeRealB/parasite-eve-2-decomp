#include "kyle/kyle.h"

#include <psyq/sys/types.h>
#include <psyq/libgpu.h>

#include "common.h"

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

/// Identity rest transform for the M950's single root, copied into each model.
/// Vertex count for the single part-local weapon group (36 vertices).
/// M950 vertices in integer coordinates local to the weapon root.
/// Weapon lighting normals with twelve fractional bits, indexed independently of vertices.
/// Writable draw commands for four textured Gouraud triangles and twenty-eight quads.
///
/// One skeletal group and an empty final group end in `TMD_STREAM_END`.
/// First model creation resolves the command handler slots in place.
#include "assets/characters/kyle/m950.inc"

/// Encoding selectors indexing the private clips' pose banks.
enum {
    KYLE_800103_ANIMATION_TRANSLATION_ROTATION_BANK = 1,
    KYLE_800103_ANIMATION_PACKED_ROTATION_BANK      = 4
};

#include "assets/characters/kyle/companion_3_clips.inc"

#include "assets/characters/kyle/companion_3_bank.inc"

/// The names the resident companion table gives this package's copies.
DEFINE_ALIAS(gKyleBody, gKyle800103Body);
DEFINE_ALIAS(gKyleHandRight, gKyle800103HandRight);
DEFINE_ALIAS(gKyleHandLeft2, gKyle800103HandLeft2);
DEFINE_ALIAS(gKyleHandLeft, gKyle800103HandLeft);
DEFINE_ALIAS(gKyleHandRight3, gKyle800103HandRight3);
