#ifndef ACTOR_101500_H
#define ACTOR_101500_H

#include "common.h"

/// The actor's animation work area. `field_352` is the pose the actor asks
/// for, `field_354` the pose its slots were last re-queued for and
/// `field_356` the frame count accumulated while the two agree:
/// `func_actor_101500_80134778` re-seeds the slots from the per-state id table
/// when they differ and ticks them while they match.
typedef struct Actor101500Work {
    /* 0x000 */ byte pad_0[0x352];
    /* 0x352 */ u16  field_352;
    /* 0x354 */ s16  field_354;
    /* 0x356 */ u16  field_356;
} Actor101500Work;

typedef struct Actor101500 {
    /* 0x00 */ byte             pad_0[0x1C];
    /* 0x1C */ Actor101500Work* field_1C;
} Actor101500;

/// Per-state animation id handed to `func_800B4114`, indexed by `field_352`.
extern s16 D_actor_101500_8013BE70[];

#endif
