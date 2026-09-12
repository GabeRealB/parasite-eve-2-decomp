#ifndef ACTOR_400500_H
#define ACTOR_400500_H

#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "main/task.h"

/// A `MATRIX` plus the word-wise view used to splat an identity rotation
/// with five aligned stores instead of nine halfword ones.
typedef union Actor400500Matrix {
    MATRIX mat;
    struct {
        /* 0x00 */ s32 m00_m01;
        /* 0x04 */ s32 m02_m10;
        /* 0x08 */ s32 m11_m12;
        /* 0x0C */ s32 m20_m21;
        /* 0x10 */ s16 m22;
    } ident;
} Actor400500Matrix;
STATIC_ASSERT_SIZEOF(Actor400500Matrix, 0x20);

/// View-space sample written by `func_actor_400500_8013DBCC`: the X and Z of
/// the translation `Gp_WorldToLocal` produces for one of the actor's
/// coordinate nodes. `func_actor_400500_80132C54` passes
/// `Actor400500Work::field_9A0` as the destination, so the slot lives inside
/// the work block. Only `x` and `z` are ever written; the middle halfword is
/// kept so the layout matches the sibling `Actor400600ViewPos`.
typedef struct Actor400500ViewPos {
    /* 0x0 */ s16 x;
    /* 0x2 */ s16 y;
    /* 0x4 */ s16 z;
} Actor400500ViewPos;
STATIC_ASSERT_SIZEOF(Actor400500ViewPos, 0x6);

/// Overlay of the model-root `GsCOORDINATE2` at `TmdObject::field_8`.
/// `x` / `z` are the low halfwords of `coord.t[0]` / `coord.t[2]`.
typedef struct Actor400500RootXZ {
    /* 0x00 */ byte pad_0[0x18];
    /* 0x18 */ u16  x;
    /* 0x1A */ byte pad_1A[6];
    /* 0x20 */ u16  z;
} Actor400500RootXZ;

/// Dual-width hit flags at `Actor400500Work` + 0x4C. Guards test bit 0 as a
/// halfword and then bits 0x102 as a word, the same shape as
/// `Actor341700Flags` / `ActorsShared8016974c`.
typedef union Actor400500HitFlags {
    /* 0x0 */ u32 word;
    /* 0x0 */ u16 half;
} Actor400500HitFlags;
STATIC_ASSERT_SIZEOF(Actor400500HitFlags, 0x4);

/// Prefix view of `Actor400500Work` for the dual-width flags at +0x4C.
/// That address is `slots[1].field_10` / `field_12` (0x10 into the second
/// animation slot), the same overlap `ActorsShared80168d3cWork` uses for
/// `flags_EC`.
typedef struct Actor400500HitView {
    /* 0x00 */ byte                pad[0x4C];
    /* 0x4C */ Actor400500HitFlags flags_4C;
} Actor400500HitView;

/// 0x28-byte stride overlay of `Actor400500Work` from offset 0. `anim` is
/// 0x14 bytes, so `stride[i].field_1D` is `slots[i].field_9`. Walked from
/// index 1 by `func_actor_400500_8013A8E4`.
typedef struct Actor400500AnimStride {
    /* 0x00 */ byte pad[0x1D];
    /* 0x1D */ u8   field_1D;
    /* 0x1E */ byte pad_1E[0xA];
} Actor400500AnimStride;
STATIC_ASSERT_SIZEOF(Actor400500AnimStride, 0x28);

/// Per-actor state block for the `actor_400500` overlay.
///
/// `func_actor_400500_80135414` is the overlay's only allocator: it calls
/// `Mem_Calloc(0xA50, 0)` and stores the result in the `Task::idMap` slot
/// (0x1C), which an enemy actor reuses for its own work block, so it is *not*
/// a `TaskIdMap` here. Reach it with `(Actor400500Work*)task->idMap`. The
/// same function hands `&work->field_97C` / `&work->field_95C` to the
/// `TmdObject` at `Task::extra` (`field_1C` / `field_20`) and `work->rec0`
/// to `GpEnemy::field_54`; the size below is the allocation, not a guess.
/// `obj1`/`obj2` share `rec1`; `obj3`/`obj4` share `rec2`.
///
/// `field_A06` / `field_A08` are the state and sub-state indices the handler
/// tables walk and `field_A04` is the per-state frame counter, mirroring
/// `Actor400600Work::field_71C` / `field_71E` / `field_718`.
///
/// The block opens with the 0x14-byte animation context and eighteen 0x28-byte
/// slots. `func_actor_400500_8013DC4C` walks slots 1..17, copies the low byte
/// of `field_9F8` into each slot's `field_9`, resets them from `field_9FE`,
/// and latches that id in `field_9FC`. The second slot's `field_10` overlaps
/// `Actor400500HitView::flags_4C`.
typedef struct Actor400500Work {
    /* 0x000 */ GpAnimCtx          anim;
    /* 0x014 */ GpAnimSlot         slots[0x12];
    /* 0x2E4 */ byte               pad_2E4[0x524];
    /* 0x808 */ MATRIX             matrix_808; // model root coord, copied on the light-mode path
    /* 0x828 */ GpObj              obj0;
    /* 0x848 */ GpRec18            rec0[3];
    /* 0x890 */ GpObj              obj1;
    /* 0x8B0 */ GpObj              obj2;
    /* 0x8D0 */ GpRec18            rec1[1];
    /* 0x8E8 */ GpObj              obj3;
    /* 0x908 */ GpObj              obj4;
    /* 0x928 */ GpRec18            rec2[1];
    /* 0x940 */ byte               pad_940[8];
    /* 0x948 */ s16                field_948;
    /* 0x94A */ s16                field_94A;
    /* 0x94C */ s16                field_94C;
    /* 0x94E */ byte               pad_94E[2];
    /* 0x950 */ u16                field_950; // low half of root coord.t[0]
    /* 0x952 */ byte               pad_952[2];
    /* 0x954 */ u16                field_954; // low half of root coord.t[2]
    /* 0x956 */ byte               pad_956[0x4A];
    /* 0x9A0 */ Actor400500ViewPos field_9A0;
    /* 0x9A6 */ byte               pad_9A6[0x16];
    /* 0x9BC */ s16                field_9BC;
    /* 0x9BE */ byte               pad_9BE[2];
    /* 0x9C0 */ VECTOR             field_9C0; // own position, copied from the model root coord.t
    /* 0x9D0 */ SVECTOR            field_9D0;
    /* 0x9D8 */ SVECTOR            field_9D8; // ApplyMatrixSV dest; vz is the former field_9DC
    /* 0x9E0 */ s16                field_9E0;
    /* 0x9E2 */ s16                field_9E2;
    /* 0x9E4 */ s16                field_9E4;
    /* 0x9E6 */ byte               pad_9E6[0xA];
    /* 0x9F0 */ Task*              field_9F0[2]; // child tasks, killed on death
    /* 0x9F8 */ s16                field_9F8;    // animation speed / step scale
    /* 0x9FA */ s16                field_9FA;    // animation request kind
    /* 0x9FC */ u16                field_9FC;    // last animation id the slots were reset to
    /* 0x9FE */ s16                field_9FE;    // animation id
    /* 0xA00 */ s16                field_A00;    // blend frame; incremented as u16, passed signed to 8013DD8C
    /* 0xA02 */ s16                field_A02;    // identity scale written with the matrix copy
    /* 0xA04 */ u16                field_A04;    // per-state frame counter
    /* 0xA06 */ u16                field_A06;    // state index
    /* 0xA08 */ u16                field_A08;    // sub-state index
    /* 0xA0A */ u16                field_A0A;
    /* 0xA0C */ byte               pad_A0C[2];
    /* 0xA0E */ s16                field_A0E; // extra arg forwarded to func_800B4114, then cleared
    /* 0xA10 */ s16                field_A10;
    /* 0xA12 */ s16                field_A12;
    /* 0xA14 */ byte               pad_A14[0x2];
    /* 0xA16 */ s16                field_A16; // distance, compared to a range
    /* 0xA18 */ s16                field_A18;
    /* 0xA1A */ s16                field_A1A; // 1: sample part 0xE when heading is 0x400/0xC00
    /* 0xA1C */ u16                field_A1C; // mode; 2, 3 and 6 take the heading-0 path
    /* 0xA1E */ u16                field_A1E; // flags; bit 0x1 and bit 0x2 gate animations
    /* 0xA20 */ s16                field_A20;
    /* 0xA22 */ u16                field_A22; // frame counter used when field_A1C == 5
    /* 0xA24 */ s16                field_A24; // copied to TmdObject::field_2C
    /* 0xA26 */ u16                field_A26; // heading countdown, decremented by 0x80
    /* 0xA28 */ s16                field_A28;
    /* 0xA2A */ byte               pad_A2A[0x2];
    /* 0xA2C */ s16                field_A2C; // countdown written with message kind 1
    /* 0xA2E */ byte               pad_A2E[2];
    /* 0xA30 */ s16                field_A30; // blocks setting field_A46 to 0x80 while nonzero
    /* 0xA32 */ s16                field_A32; // heading; >>3 as u16, compared to 0 as s16
    /* 0xA34 */ s16                field_A34; // gates the field_A1A==3 sub-state write
    /* 0xA36 */ u16                field_A36; // angle, range-tested as (a - 0x300) <= 0xA00
    /* 0xA38 */ s16                field_A38;
    /* 0xA3A */ s16                field_A3A;
    /* 0xA3C */ s16                field_A3C;
    /* 0xA3E */ s16                field_A3E;
    /* 0xA40 */ s16                field_A40;
    /* 0xA42 */ s16                field_A42;
    /* 0xA44 */ byte               pad_A44[2];
    /* 0xA46 */ s8                 field_A46; // signed flag; 0x81 means active mode 1
    /* 0xA47 */ s8                 field_A47;
    /* 0xA48 */ byte               pad_A48;
    /* 0xA49 */ s8                 field_A49;
    /* 0xA4A */ s8                 field_A4A;
    /* 0xA4B */ s8                 field_A4B; // last message kind 1..4
    /* 0xA4C */ s8                 field_A4C; // set with kind 1
    /* 0xA4D */ byte               pad_A4D[0x3];
} Actor400500Work;
STATIC_ASSERT_SIZEOF(Actor400500Work, 0xA50);

void func_8004BFF8(s16 angle, MATRIX* matrix);
void ActorsShared80132c4c(MATRIX* src, MATRIX* dst);

#endif
