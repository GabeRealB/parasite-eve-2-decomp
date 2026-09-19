#ifndef ACTORS_SHARED_80133F64_H
#define ACTORS_SHARED_80133F64_H

#include "common.h"

#include "gameplay/1BC.h"
#include "main/task.h"

/// Work block the actors sharing this body hang off the task's `work` slot
/// (0x1C), which is not a `TaskIdMap` here. The carriers allocate blocks of
/// different sizes, so only the prefix this body reaches is described.
///
/// 0xC..0x784 is six back-to-back animation blocks, each a `GpAnimCtx`
/// followed by its own `GpAnimSlot[N]` and an N-entry 0x10-byte table -- the
/// three argument groups the carrier's setup routine hands to `func_800B3F84`,
/// which is what anchors every offset here. They pair up (0/1, 2/3, 4/5) with
/// eight slots in the first pair and four in the others. `field_7BE` is the
/// `GpAnimSlot.rate` the resets seed with and `field_7BC` the animation id.
typedef struct ActorsShared80133f64Work {
    /* 0x000 */ byte       pad_0[0xC];
    /* 0x00C */ GpAnimCtx  anim0;
    /* 0x020 */ GpAnimSlot slots0[8];
    /* 0x160 */ byte       aux0[0x80]; // GpAnimCtx.poses, one 0x10-byte record per slot
    /* 0x1E0 */ GpAnimCtx  anim1;
    /* 0x1F4 */ GpAnimSlot slots1[8];
    /* 0x334 */ byte       aux1[0x80];
    /* 0x3B4 */ GpAnimCtx  anim2;
    /* 0x3C8 */ GpAnimSlot slots2[4];
    /* 0x468 */ byte       aux2[0x40];
    /* 0x4A8 */ GpAnimCtx  anim3;
    /* 0x4BC */ GpAnimSlot slots3[4];
    /* 0x55C */ byte       aux3[0x40];
    /* 0x59C */ GpAnimCtx  anim4;
    /* 0x5B0 */ GpAnimSlot slots4[4];
    /* 0x650 */ byte       aux4[0x40];
    /* 0x690 */ GpAnimCtx  anim5;
    /* 0x6A4 */ GpAnimSlot slots5[4];
    /* 0x744 */ byte       aux5[0x40];
    /* 0x784 */ byte       pad_784[0x38];
    /* 0x7BC */ s16        field_7BC;
    /* 0x7BE */ s16        field_7BE;
    /* 0x7C0 */ s16        field_7C0;
    /* 0x7C2 */ byte       pad_7C2[0x2];
} ActorsShared80133f64Work;
STATIC_ASSERT_SIZEOF(ActorsShared80133f64Work, 0x7C4);

/// Re-seed the three animation pairs: for each pair, stamp the pending
/// `GpAnimSlot.rate` onto the even context's slots while resetting the odd
/// context's slots to the pending animation id.
void ActorsShared80133f64(Task* task);

#endif
