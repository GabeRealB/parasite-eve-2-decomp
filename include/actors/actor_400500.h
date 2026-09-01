#ifndef ACTOR_400500_H
#define ACTOR_400500_H

#include "common.h"
#include "main/task.h"

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

/// Per-actor state block for the `actor_400500` overlay.
///
/// `func_actor_400500_80135414` is the overlay's only allocator: it calls
/// `Mem_Calloc(0xA50, 0)` and stores the result in the `Task::idMap` slot
/// (0x1C), which an enemy actor reuses for its own work block, so it is *not*
/// a `TaskIdMap` here. Reach it with `(Actor400500Work*)task->idMap`. The
/// same function hands `&work->field_97C` / `&work->field_95C` to the
/// `TmdObject` at `Task::extra` (`field_1C` / `field_20`) and `work + 0x848`
/// to `GpEnemy::field_54`; the size below is the allocation, not a guess.
///
/// `field_A06` / `field_A08` are the state and sub-state indices the handler
/// tables walk and `field_A04` is the per-state frame counter, mirroring
/// `Actor400600Work::field_71C` / `field_71E` / `field_718`.
typedef struct Actor400500Work {
    /* 0x000 */ byte               pad_0[0x94A];
    /* 0x94A */ s16                field_94A;
    /* 0x94C */ s16                field_94C;
    /* 0x94E */ byte               pad_94E[0x52];
    /* 0x9A0 */ Actor400500ViewPos field_9A0;
    /* 0x9A6 */ byte               pad_9A6[0x36];
    /* 0x9DC */ s16                field_9DC;
    /* 0x9DE */ byte               pad_9DE[0x12];
    /* 0x9F0 */ Task*              field_9F0[2]; // child tasks, killed on death
    /* 0x9F8 */ s16                field_9F8;    // animation speed / step scale
    /* 0x9FA */ s16                field_9FA;    // animation request kind
    /* 0x9FC */ byte               pad_9FC[0x2];
    /* 0x9FE */ s16                field_9FE;    // animation id
    /* 0xA00 */ byte               pad_A00[0x4];
    /* 0xA04 */ u16                field_A04;    // per-state frame counter
    /* 0xA06 */ u16                field_A06;    // state index
    /* 0xA08 */ u16                field_A08;    // sub-state index
    /* 0xA0A */ u16                field_A0A;
    /* 0xA0C */ byte               pad_A0C[0x4];
    /* 0xA10 */ s16                field_A10;
    /* 0xA12 */ s16                field_A12;
    /* 0xA14 */ byte               pad_A14[0x2];
    /* 0xA16 */ s16                field_A16; // distance, compared to a range
    /* 0xA18 */ s16                field_A18;
    /* 0xA1A */ byte               pad_A1A[0x4];
    /* 0xA1E */ u16                field_A1E; // flags; bit 0x1 and bit 0x2 gate animations
    /* 0xA20 */ s16                field_A20;
    /* 0xA22 */ byte               pad_A22[0x2];
    /* 0xA24 */ s16                field_A24; // copied to TmdObject::field_2C
    /* 0xA26 */ byte               pad_A26[0x2];
    /* 0xA28 */ s16                field_A28;
    /* 0xA2A */ byte               pad_A2A[0xC];
    /* 0xA36 */ u16                field_A36; // angle, range-tested as (a - 0x300) <= 0xA00
    /* 0xA38 */ byte               pad_A38[0x4];
    /* 0xA3C */ s16                field_A3C;
    /* 0xA3E */ s16                field_A3E;
    /* 0xA40 */ s16                field_A40;
    /* 0xA42 */ s16                field_A42;
    /* 0xA44 */ byte               pad_A44[0xC];
} Actor400500Work;
STATIC_ASSERT_SIZEOF(Actor400500Work, 0xA50);

#endif
