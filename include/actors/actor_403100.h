#ifndef ACTOR_403100_H
#define ACTOR_403100_H

#include "common.h"

#include "gameplay/3A34.h"

/// The word at `Actor403100Work::field_664`, which the overlay reads both as a
/// whole word and as four separate bytes: `func_actor_403100_8013D2A0` gates a
/// new request on `word & 0xFFFF00` (the two bytes at 0x665 / 0x666) being
/// clear, then sets those two bytes, while `func_actor_403100_8013CDC0`
/// switches on the byte at 0x666 alone. Both views are modelled explicitly.
typedef union Actor403100Req {
    /* 0x0 */ s32 word;
    struct {
        /* 0x0 */ s8 field_664;
        /* 0x1 */ s8 field_665;
        /* 0x2 */ u8 field_666; // 4-state machine ticked by func_actor_403100_8013CDC0
        /* 0x3 */ s8 field_667;
    } b;
} Actor403100Req;
STATIC_ASSERT_SIZEOF(Actor403100Req, 0x4);

/// Per-actor work block for the `actor_403100` overlay.
///
/// `func_actor_403100_80136610` allocates it with `Mem_Calloc(0x678, 0)` and
/// stores the same pointer twice: into the `Task::idMap` slot (0x1C), which an
/// enemy actor reuses for its own work block, and into the overlay-wide
/// `D_actor_403100_80155808`. It also hands `work + 0x20` and `work` to the
/// `TmdObject` at `Task::extra` (`field_1C` / `field_20`). Most of the overlay
/// reaches it through the global rather than through the task.
typedef struct Actor403100Work {
    /* 0x000 */ byte           pad_0[0x82];
    /* 0x082 */ s16            field_82;  // facing angle, stepped towards field_B2
    /* 0x084 */ byte           pad_84[0x2E];
    /* 0x0B2 */ s16            field_B2;  // target angle
    /* 0x0B4 */ s16            field_B4;  // angle offset, decayed towards 0
    /* 0x0B6 */ byte           pad_B6[0x35E];
    /* 0x414 */ GpObj          field_414; // display nodes unlinked on death
    /* 0x434 */ byte           pad_434[0x48];
    /* 0x47C */ GpObj          field_47C;
    /* 0x49C */ byte           pad_49C[0xC0];
    /* 0x55C */ GpObj          field_55C;
    /* 0x57C */ byte           pad_57C[0x18];
    /* 0x594 */ GpObj          field_594;
    /* 0x5B4 */ byte           pad_5B4[0x24];
    /* 0x5D8 */ s16            field_5D8;
    /* 0x5DA */ s16            field_5DA; // animation request kind
    /* 0x5DC */ byte           pad_5DC[0x2];
    /* 0x5DE */ s16            field_5DE; // animation id
    /* 0x5E0 */ byte           pad_5E0[0x2];
    /* 0x5E2 */ s16            field_5E2;
    /* 0x5E4 */ byte           pad_5E4[0x6];
    /* 0x5EA */ s16            field_5EA;
    /* 0x5EC */ u16            field_5EC; // per-state frame counter
    /* 0x5EE */ s16            field_5EE;
    /* 0x5F0 */ byte           pad_5F0[0x6];
    /* 0x5F6 */ s16            field_5F6;
    /* 0x5F8 */ u16            field_5F8; // state index
    /* 0x5FA */ u16            field_5FA; // sub-state index
    /* 0x5FC */ s16            field_5FC;
    /* 0x5FE */ byte           pad_5FE[0x2];
    /* 0x600 */ s16            field_600;
    /* 0x602 */ byte           pad_602[0x2];
    /* 0x604 */ s16            field_604;
    /* 0x606 */ byte           pad_606[0x2];
    /* 0x608 */ s16            field_608;
    /* 0x60A */ byte           pad_60A[0xE];
    /* 0x618 */ s16            field_618;
    /* 0x61A */ byte           pad_61A[0x2];
    /* 0x61C */ s16            field_61C;
    /* 0x61E */ byte           pad_61E[0xE];
    /* 0x62C */ s16            field_62C;
    /* 0x62E */ byte           pad_62E[0x31];
    /* 0x65F */ s8             field_65F;
    /* 0x660 */ byte           pad_660[0x4];
    /* 0x664 */ Actor403100Req field_664;
    /* 0x668 */ s8             field_668;
    /* 0x669 */ s8             field_669;
    /* 0x66A */ byte           pad_66A[0x5];
    /* 0x66F */ s8             field_66F;
    /* 0x670 */ byte           pad_670[0x8];
} Actor403100Work;
STATIC_ASSERT_SIZEOF(Actor403100Work, 0x678);

#endif // ACTOR_403100_H
