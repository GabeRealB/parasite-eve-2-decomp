#ifndef ACTOR_215100_H
#define ACTOR_215100_H

#include "common.h"

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

/// Work block this overlay hangs off its task's `Task::idMap` slot (0x1C),
/// which is not a `TaskIdMap` here. Only the prefix the animation script opcode
/// `func_actor_215100_8014CCE0` and `func_actor_215100_8014C874` reach is
/// described.
///
/// `state` selects the start path `func_actor_215100_8014C874` takes: 1 runs
/// the animated one and 2 the plain one, and the same layout appears in
/// `Actor150400Work` thirty-six bytes lower (`state` at 0x47C there), so the two
/// overlays carry separate structs.
typedef struct Actor215100Work {
    /* 0x000 */ byte pad_0[0x4B4];
    /* 0x4B4 */ s16  state;
    /* 0x4B6 */ byte pad_4B6[0x2];
    /* 0x4B8 */ u16  animId;
    /* 0x4BA */ s16  field_4BA;
    /* 0x4BC */ byte pad_4BC[0x30];
    /* 0x4EC */ u16  animArg;
} Actor215100Work;

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

void func_actor_215100_8014C874(Task* task);

s32 func_actor_215100_8014CCE0(Task* task, s32 arg1, Actor215100AnimArgs* args);

#endif
