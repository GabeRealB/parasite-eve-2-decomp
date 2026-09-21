#ifndef ACTOR_202300_H
#define ACTOR_202300_H

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "gameplay/3FB8.h"
#include "main/tmd.h"

/// Per-instance work block this overlay's task holds in the 0x1C slot of
/// `Actor202300`. `field_694` is the current animation id, `field_698` the
/// frame counter the lunge tick compares against the per-animation start
/// frame table `D_actor_202300_8014DD64`, and `field_6A8` the state that tick
/// switches on.
typedef struct Actor202300Work {
    /* 0x000 */ byte pad_0[0x47C];
    /// Body node whose 0xC000 flags the lunge raises on the wind-up and
    /// recovery, then drops once `field_6B2` reports contact.
    /* 0x47C */ GpObj field_47C;
    /* 0x49C */ byte  pad_49C[0x1F8];
    /* 0x694 */ s16   field_694;
    /* 0x696 */ byte  pad_696[2];
    /* 0x698 */ s16   field_698;
    /* 0x69A */ byte  pad_69A[2];
    /* 0x69C */ s16   field_69C;
    /* 0x69E */ s16   field_69E;
    /* 0x6A0 */ byte  pad_6A0[2];
    /// Facing angle the lunge steers `field_6A4` towards.
    /* 0x6A2 */ u16  field_6A2;
    /* 0x6A4 */ s16  field_6A4;
    /* 0x6A6 */ s16  field_6A6;
    /* 0x6A8 */ s16  field_6A8;
    /* 0x6AA */ byte pad_6AA[4];
    /* 0x6AE */ s16  field_6AE;
    /* 0x6B0 */ byte pad_6B0[2];
    /// Raised by the collision node once the lunge connects; the state
    /// handlers check it to break out of the approach cycle.
    /* 0x6B2 */ s16  field_6B2;
    /* 0x6B4 */ byte pad_6B4[2];
    /* 0x6B6 */ s16  field_6B6;
    /* 0x6B8 */ byte pad_6B8[8];
    /// Shift count for the idle-to-lunge draw: each cycle widens the LCG mask
    /// by one bit, so the enemy grows less likely to lunge again.
    /* 0x6C0 */ s16  field_6C0;
    /* 0x6C2 */ s16  field_6C2;
    /* 0x6C4 */ s16  field_6C4;
    /* 0x6C6 */ byte pad_6C6[0x16];
    /* 0x6DC */ s16  field_6DC;
} Actor202300Work;

/// Actor context handed to this overlay's callbacks: `field_1C` is the work
/// block above and `field_2C` the display object whose `coords` is the
/// per-part coordinate array.
typedef struct Actor202300 {
    /* 0x00 */ byte             pad_0[0x1C];
    /* 0x1C */ Actor202300Work* field_1C;
    /* 0x20 */ byte             pad_20[0xC];
    /* 0x2C */ TmdObject*       field_2C;
} Actor202300;

#endif // ACTOR_202300_H
