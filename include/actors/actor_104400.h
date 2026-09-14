#ifndef ACTOR_104400_H
#define ACTOR_104400_H

#include "common.h"

#include "gameplay/3A34.h"

/// Status flags at `Actor104400Work` + 0xEC, read through two widths.
///
/// Guards test bit 0 as a halfword and then bits 0x102 as a word
/// (`Actor04400_Fn06618` is the out-of-line copy of the test), the same
/// shape as `Actor341700Flags`.
typedef union Actor104400Flags {
    /* 0x0 */ u32 word;
    /* 0x0 */ u16 half;
} Actor104400Flags;
STATIC_ASSERT_SIZEOF(Actor104400Flags, 0x4);

/// Per-actor state block for the `actor_104400` overlay's enemy.
///
/// `Actor04400_Fn00B24` and `Actor04400_Fn00D3C` both allocate it with
/// `Mem_Calloc(0x454, 0)` and store it in the `Task::idMap` slot (0x1C), so
/// the size below is the allocation, not a guess: this actor reuses that
/// pointer field for its own work block and it is *not* a `TaskIdMap` here.
/// Reach it with `(Actor104400Work*)task->idMap`.
///
/// The layout mirrors the sibling `actor_341700` / `actor_342400` overlays,
/// whose work blocks are the same size and drive the same state machine:
/// `field_420` / `field_422` are the state and sub-state indices the handler
/// table walks, `field_412` is the per-state frame counter, and
/// `field_414` .. `field_426` are the animation request the actor hands to
/// its player. The three `GpObj` nodes are the display objects
/// `Actor04400_Fn08A40` hands back to `Gp_UnlinkObj`.
typedef struct Actor104400Work {
    /* 0x000 */ MATRIX           matrix_0; // model root coord, copied out on the kill path
    /* 0x020 */ byte             pad_20[0x5A];
    /* 0x07A */ s16              field_7A; // heading
    /* 0x07C */ byte             pad_7C[0xC];
    /* 0x088 */ s16              field_88; // x of the vector turned towards
    /* 0x08A */ s16              field_8A;
    /* 0x08C */ s16              field_8C; // z of the vector turned towards
    /* 0x08E */ byte             pad_8E[0x5E];
    /* 0x0EC */ Actor104400Flags flags_EC;
    /* 0x0F0 */ byte             pad_F0[0x1BC];
    /* 0x2AC */ GpObj            obj_2AC;
    /* 0x2CC */ GpObj            obj_2CC;
    /* 0x2EC */ byte             pad_2EC[0xC0];
    /* 0x3AC */ GpObj            obj_3AC;
    /* 0x3CC */ byte             pad_3CC[0x46];
    /* 0x412 */ u16              field_412; // per-state frame counter
    /* 0x414 */ s16              field_414; // animation request kind
    /* 0x416 */ byte             pad_416[0x2];
    /* 0x418 */ s16              field_418; // animation id
    /* 0x41A */ byte             pad_41A[0x2];
    /* 0x41C */ s16              field_41C; // animation speed / step scale
    /* 0x41E */ s16              field_41E;
    /* 0x420 */ u16              field_420; // state index
    /* 0x422 */ u16              field_422; // sub-state index
    /* 0x424 */ s16              field_424; // yaw added to model parts 3..5, a third each
    /* 0x426 */ s16              field_426;
    /* 0x428 */ byte             pad_428[0x8];
    /* 0x430 */ s16              field_430;
    /* 0x432 */ byte             pad_432[0x6];
    /* 0x438 */ s16              field_438; // 1 on the death path
    /* 0x43A */ byte             pad_43A[0xE];
    /* 0x448 */ s16              field_448;
    /* 0x44A */ byte             pad_44A[0x5];
    /* 0x44F */ u8               field_44F;
    /* 0x450 */ byte             pad_450[0x4];
} Actor104400Work;
STATIC_ASSERT_SIZEOF(Actor104400Work, 0x454);

#endif
