#ifndef ACTORS_SHARED_80163354_H
#define ACTORS_SHARED_80163354_H

#include "common.h"

#include "main/task.h"

/// 0x90-byte scratchpad frame `ActorsShared80163354` carves off
/// `G_SCRATCH_HEAD` to draw a textured quad between two model parts: both
/// parts' view-space matrices, their positions, the four widened corners and
/// `RotTransPers4`'s outputs.
typedef struct ActorsShared80163354Scratch {
    /* 0x00 */ MATRIX  firstMatrix;  // first part's `workm` in view space
    /* 0x20 */ MATRIX  secondMatrix; // second part's `workm` in view space
    /* 0x40 */ SVECTOR first;
    /* 0x48 */ SVECTOR second;
    /* 0x50 */ SVECTOR corner0;
    /* 0x58 */ SVECTOR corner1;
    /* 0x60 */ SVECTOR corner2;
    /* 0x68 */ SVECTOR corner3;
    /* 0x70 */ s32     screen0;
    /* 0x74 */ s32     screen1;
    /* 0x78 */ s32     screen2;
    /* 0x7C */ s32     screen3;
    /* 0x80 */ s32     perspective;
    /* 0x84 */ s32     flags;
    /* 0x88 */ s32     depth;
    /* 0x8C */ s16     halfX; // half the X separation, pulls the corners together
    /* 0x8E */ s16     halfZ;
} ActorsShared80163354Scratch;
STATIC_ASSERT_SIZEOF(ActorsShared80163354Scratch, 0x90);

/// Draws a semi-transparent textured quad between model parts `firstJoint` and
/// `secondJoint`: the segment joining them in view space is widened by `width`
/// either side at height `height`, pulled in by half its length at both ends,
/// and shaded grey `shade`. Shared by `actor_341700` and `actor_342400`.
void ActorsShared80163354(Task* task, s16 firstJoint, s16 secondJoint, s16 width, s32 height, u8 shade);

#endif
