#ifndef ACTOR_215100_H
#define ACTOR_215100_H

#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/1BC.h"
#include "main/task.h"

/// One entry of the caption script the `actor_215100` overlay plays back.
///
/// `func_actor_215100_8014B2B8` picks a script out of the table at
/// `D_actor_215100_8015E650`, parks it in `D_actor_215100_8015E658` and walks it
/// with `func_actor_215100_8014C418`, which scans forward 0xC bytes at a time
/// until `field_8` is the `-1` terminator or `field_5` matches the requested
/// key. `func_actor_215100_8014B0D4` renders the entry that
/// `D_actor_215100_8015E662` currently indexes: `field_8` is the line id it
/// hands to `func_actor_215100_8014B3C8` (and to the metric helpers
/// `func_actor_215100_8014BDFC` / `_8014C06C` / `_8014C298`), `field_0` plus
/// bit 0x10 of `field_1` make up that call's flag word, and bit 0 of `field_4`
/// suppresses the auto-advance timer in `func_actor_215100_8014BEE8`.
typedef struct Actor215100Caption {
    /* 0x0 */ u8   field_0; // low byte of the flag word passed to _8014B3C8
    /* 0x1 */ u8   field_1; // only bit 0x10 is read; it becomes bit 0x100 of that word
    /* 0x2 */ byte pad_2[0x2];
    /* 0x4 */ u8   field_4; // bit 0 holds the caption on screen
    /* 0x5 */ u8   field_5; // key matched against D_actor_215100_8015E666
    /* 0x6 */ byte pad_6[0x2];
    /* 0x8 */ s32  field_8; // line id; -1 terminates the script
} Actor215100Caption;
STATIC_ASSERT_SIZEOF(Actor215100Caption, 0xC);

/// Work block this overlay hangs off its task's `Task::work` slot (0x1C),
/// which is not a `TaskIdMap` here. Only the prefix the animation script opcode
/// `func_actor_215100_8014CCE0` and `ActorsShared8014c874` reach is
/// described.
///
/// `state` selects the start path `ActorsShared8014c874` takes: 1 runs
/// the animated one and 2 the plain one, and the same layout appears in
/// `Actor150400Work` thirty-six bytes lower (`state` at 0x47C there), so the two
/// overlays carry separate structs.
typedef struct Actor215100Work {
    /* 0x000 */ MATRIX     light;
    /* 0x020 */ MATRIX     color;
    /* 0x040 */ GpAnimCtx  anim;
    /* 0x054 */ GpAnimSlot slots[0x14];
    /* 0x374 */ byte       field_374;
    /* 0x375 */ byte       pad_375[0x13F];
    /* 0x4B4 */ s16        state;
    /* 0x4B6 */ byte       pad_4B6[0x2];
    /* 0x4B8 */ u16        animId;
    /* 0x4BA */ s16        field_4BA;
    /* 0x4BC */ byte       pad_4BC[0x30];
    /* 0x4EC */ u16        animArg;
    /* 0x4EE */ byte       pad_4EE[0x2];
    /* 0x4F0 */ Task*      field_4F0;
    /* 0x4F4 */ GpEnemy*   enemy;
} Actor215100Work;
STATIC_ASSERT_SIZEOF(Actor215100Work, 0x4F8);

/// Argument block of the script opcode `func_actor_215100_8014CCE0`
/// implements: which animation to play, and how.
typedef struct Actor215100AnimArgs {
    /* 0x0 */ byte pad_0[4];
    /* 0x4 */ s32  animId;
    /* 0x8 */ s32  withArg;
    /* 0xC */ u16  animArg;
} Actor215100AnimArgs;

/// Eight-byte character appearance record `func_actor_215100_8014AA54` parks in
/// `D_actor_215100_8015E678` before it starts the actor's caption script.
///
/// That function copies its argument here whole and then only reads `field_5`:
/// non-zero means the character has already been committed, so it returns 2 and
/// leaves the record alone. The bytes are otherwise opaque to decompiled code
/// except through `func_actor_215100_8014A5C0`, which copies `field_0`,
/// `field_2` and `field_3` out one at a time into the task it spawns.
typedef struct Actor215100CharRec {
    /* 0x0 */ u8 field_0;
    /* 0x1 */ u8 field_1;
    /* 0x2 */ u8 field_2;
    /* 0x3 */ u8 field_3;
    /* 0x4 */ u8 field_4;
    /* 0x5 */ u8 field_5; // non-zero: the character is already committed
    /* 0x6 */ u8 field_6;
    /* 0x7 */ u8 field_7;
} Actor215100CharRec;
STATIC_ASSERT_SIZEOF(Actor215100CharRec, 0x8);

/// One entry of the caption schedule `func_actor_215100_8014AFAC` scans while
/// the actor waits to be talked to.
///
/// The entry whose window contains the session's caption clock
/// (`GameSession::field_120`, which that function ticks down once the caption
/// system goes idle) names the script to start and the line key to start it at:
/// it is taken when `field_0 * 30 >= clock` and `field_4 * 30 < clock`, and the
/// table is ordered by descending `field_0`, so the first match wins. A
/// `field_0` of -1 terminates the scan. The table itself lives in the overlay's
/// trailing data (`D_actor_215100_80154514`), not in this unit.
typedef struct Actor215100CapWindow {
    /* 0x0 */ s32 field_0; // window upper bound, x30; -1 terminates the table
    /* 0x4 */ s32 field_4; // window lower bound, x30
    /* 0x8 */ s32 field_8; // caption script index, the `func_actor_215100_8014B2B8` arg0
    /* 0xC */ s32 field_C; // the line key to start that script at, its arg1
} Actor215100CapWindow;
STATIC_ASSERT_SIZEOF(Actor215100CapWindow, 0x10);

/// `Task` as this overlay's caption actor reads it in
/// `func_actor_215100_8014AFAC`: the dispatcher index, and the low half of
/// `Task::spawnArg1` — the task's own line delay, handed to
/// `func_actor_215100_8014B2B8` as its `arg2`. The rest of the overlay passes
/// the whole `Task` around; the same two-field view is `Actor444000` in
/// actor_444000.
typedef struct Actor215100 {
    /* 0x00 */ byte pad_0[0x30];
    /* 0x30 */ s32  state;
    /* 0x34 */ s16  spawnArg1Lo;
} Actor215100;

s32 func_actor_215100_8014CCE0(Task* task, s32 arg1, Actor215100AnimArgs* args);

#endif
