#ifndef INCLUDE_ACTORS_ACTORS_SHARED_80163354_H
#define INCLUDE_ACTORS_ACTORS_SHARED_80163354_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

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
    /* 0x70 */ long    screen0;
    /* 0x74 */ long    screen1;
    /* 0x78 */ long    screen2;
    /* 0x7C */ long    screen3;
    /* 0x80 */ long    perspective;
    /* 0x84 */ long    flags;
    /* 0x88 */ s32     depth;
    /* 0x8C */ s16     halfX; // half the X separation, pulls the corners together
    /* 0x8E */ s16     halfZ;
} ActorsShared80163354Scratch;
STATIC_ASSERT_SIZEOF(ActorsShared80163354Scratch, 0x90);

#endif // INCLUDE_ACTORS_ACTORS_SHARED_80163354_H
