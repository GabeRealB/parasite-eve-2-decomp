#ifndef ACTOR_113100_H
#define ACTOR_113100_H

#include "common.h"

#include "gameplay/3A34.h"

/// Work block of the `actor_113100` enemy task. `func_actor_113100_80131E58`
/// (state 0) `Mem_Calloc`s 0x540 bytes and parks the pointer in the task's
/// `Task::idMap` slot -- that slot is not a `TaskIdMap` here. The same
/// function builds the display node at +0x4B8: it points `GpObj::field_C` at
/// the record that follows the node (block + 0x4D8), sets `field_18` to
/// 0x30000 and `field_1C` to 0x100, zeroes the position triple and sets
/// `flags` to 1. The exit callback `func_actor_113100_80132EF0` hands that
/// node back to `Gp_UnlinkObj`.
/// `field_53E` latches the `GameFlag_GetNibble(0xED)` result that
/// `func_actor_113100_80132F40` uses, so the setup it triggers runs only on
/// the edge where the flag turns positive and the latch is still clear.
typedef struct Actor113100Work {
    /* 0x000 */ byte  pad_0[0x4B8];
    /* 0x4B8 */ GpObj obj;
    /* 0x4D8 */ byte  pad_4D8[0x18];
    /* 0x4F0 */ s32   field_4F0;
    /* 0x4F4 */ s32   field_4F4;
    /* 0x4F8 */ s32   field_4F8;
    /* 0x4FC */ byte  pad_4FC[0x36];
    /* 0x532 */ u16   field_532;
    /* 0x534 */ byte  pad_534[0x6];
    /* 0x53A */ s16   field_53A;
    /* 0x53C */ byte  pad_53C[0x2];
    /* 0x53E */ s8    field_53E;
    /* 0x53F */ byte  pad_53F[1];
} Actor113100Work;
STATIC_ASSERT_SIZEOF(Actor113100Work, 0x540);

/// Animation preset `func_actor_113100_8013301C` builds on its stack and hands
/// to `func_actor_113100_801331E8` as message 0x7D3. That function compares
/// `field_0` against `Actor113100Work::field_476` and, when they differ,
/// latches it and re-seeds the slot tables through `Gp_AnimResetSlot` /
/// `func_800B3F84`; `field_8` selects between that path and the plain
/// `Gp_AnimTickIndex` loop, and `field_C` is passed on as the per-slot
/// argument. The trailing `field_10` is stored but never read by the callee.
typedef struct Actor113100AnimPreset {
    /* 0x00 */ s32 field_0;
    /* 0x04 */ s32 field_4;
    /* 0x08 */ s32 field_8;
    /* 0x0C */ s32 field_C;
    /* 0x10 */ s32 field_10;
} Actor113100AnimPreset;
STATIC_ASSERT_SIZEOF(Actor113100AnimPreset, 0x14);

#endif
