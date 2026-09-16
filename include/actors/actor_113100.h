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
/// `field_534` is a `Task_SpawnFromTable` result this same setup stores, and
/// `field_53C` is the mode byte the 0x7DB handler writes and
/// `func_actor_113100_80132104` switches on.
/// `field_52A` is the heading the turn-to-face body `func_actor_113100_801330E8`
/// steps the root coordinate's yaw toward, 0x40 a frame, snapping to it once
/// within 0x41, and `field_477` is the animation id that body puts in the
/// preset it sends when it snaps. Both reset `field_530` / `field_532` there.
typedef struct Actor113100Work {
    /* 0x000 */ byte  pad_0[0x477];
    /* 0x477 */ s8    field_477;
    /* 0x478 */ byte  pad_478[0x40];
    /* 0x4B8 */ GpObj obj;
    /* 0x4D8 */ byte  pad_4D8[0x18];
    /* 0x4F0 */ s32   field_4F0;
    /* 0x4F4 */ s32   field_4F4;
    /* 0x4F8 */ s32   field_4F8;
    /* 0x4FC */ byte  pad_4FC[0x2E];
    /* 0x52A */ u16   field_52A;
    /* 0x52C */ byte  pad_52C[0x4];
    /* 0x530 */ s16   field_530;
    /* 0x532 */ u16   field_532;
    /* 0x534 */ Task* field_534;
    /* 0x538 */ s16   field_538;
    /* 0x53A */ s16   field_53A;
    /* 0x53C */ u8    field_53C;
    /* 0x53D */ byte  pad_53D[1];
    /* 0x53E */ s8    field_53E;
    /* 0x53F */ byte  pad_53F[1];
} Actor113100Work;
STATIC_ASSERT_SIZEOF(Actor113100Work, 0x540);

/// Payload the sender of message 0x7DB passes as `Gp_DispatchMsg`'s `arg2`;
/// the same 4-byte record as `Actor342400Msg7DB`, whose halfword at 0x2 is the
/// only part this overlay's handler reads.
typedef struct Actor113100Msg7DB {
    /* 0x0 */ u8  field_0;
    /* 0x1 */ u8  field_1;
    /* 0x2 */ u16 field_2;
} Actor113100Msg7DB;
STATIC_ASSERT_SIZEOF(Actor113100Msg7DB, 0x4);

/// Message 0x7DB handler, listed in `D_actor_113100_80144338` after the 0x7D3 /
/// 0x7D5 / 0x7DD ones. The payload halfword selects one of four actions: 0 and
/// 1 clear and raise bit 0x80 of the child task's `TmdObject::field_C`, the
/// deferred-kill flag that decides whether the model is drawn; 2 and 3 set the
/// work block's `field_53C` mode byte to 1 and 0. Nothing reads the opcode
/// itself, hence `msgId`.
s32 func_actor_113100_801333B8(Task* task, s32 msgId, Actor113100Msg7DB* msg);

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

/// A `MATRIX`'s word-wise view, for the identity splat
/// `func_actor_113100_801330E8` writes over the root coordinate before
/// `RotMatrix` refills the 3x3: five aligned stores rather than nine halfword
/// ones (the same shape as `Actor141000MatWords`, whose comment on the idiom is
/// the fuller one).
typedef struct Actor113100MatWords {
    /* 0x00 */ s32 m00_m01;
    /* 0x04 */ s32 m02_m10;
    /* 0x08 */ s32 m11_m12;
    /* 0x0C */ s32 m20_m21;
    /* 0x10 */ s16 m22;
} Actor113100MatWords;
STATIC_ASSERT_SIZEOF(Actor113100MatWords, 0x14);

/// The four main-body handlers, dispatched by `Actor113100Work::field_532`.
extern TaskFuncTable4 D_actor_113100_80131E48;

#endif
