#ifndef ACTOR_102300_H
#define ACTOR_102300_H

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3FB8.h"
#include "main/tmd.h"

/// Per-instance work block this overlay's task holds in the 0x1C slot of
/// `Actor102300`, the same shape the other enemy overlays give theirs:
/// `field_694` is the current animation id, `field_698` the frame counter the
/// state handlers compare against the per-animation start frame table
/// `D_actor_102300_80135D64`, and `field_6A8` the state the frame dispatcher
/// switches on.
typedef struct Actor102300Work {
    /* 0x000 */ byte pad_0[0x5E4];
    /// Collision/proximity list node, the slot the lunge raises bit 0x8000 of
    /// and parks its `Gp_PackPair` entry in.
    /* 0x5E4 */ GpObj field_5E4;
    /* 0x604 */ byte  pad_604[0x90];
    /* 0x694 */ s16   field_694;
    /* 0x696 */ byte  pad_696[2];
    /* 0x698 */ s16   field_698;
    /* 0x69A */ byte  pad_69A[2];
    /* 0x69C */ s16   field_69C;
    /* 0x69E */ s16   field_69E;
    /* 0x6A0 */ byte  pad_6A0[4];
    /* 0x6A4 */ s16   field_6A4;
    /* 0x6A6 */ s16   field_6A6;
    /* 0x6A8 */ s16   field_6A8;
    /* 0x6AA */ byte  pad_6AA[0x3A];
} Actor102300Work;
STATIC_ASSERT_SIZEOF(Actor102300Work, 0x6E4);

/// Actor context handed to this overlay's callbacks: `field_1C` is the work
/// block above, `field_20` the `GpEnemy` the spawner left in the task's
/// `Task::spawnArg2` slot, and `field_2C` the display object whose `field_8`
/// is the per-part coordinate array. Same shape as the other actor overlays'
/// contexts.
typedef struct Actor102300 {
    /* 0x00 */ byte             pad_0[0x1C];
    /* 0x1C */ Actor102300Work* field_1C;
    /* 0x20 */ GpEnemy*         field_20;
    /* 0x24 */ byte             pad_24[8];
    /* 0x2C */ TmdObject*       field_2C;
    /* 0x30 */ s32              field_30;
} Actor102300;

#endif // ACTOR_102300_H
