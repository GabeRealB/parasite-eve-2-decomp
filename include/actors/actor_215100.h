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

/// Work block the actor's spawn handler allocates (0x4F8 bytes, cleared) and
/// hangs off `Task::work`, which is not a `TaskIdMap` here.
///
/// The head holds the light and colour matrices the actor's model and its
/// attached sub-model draw with, and the animation context with its twenty
/// slots. `state` selects what the step body does next: 1 reseeds the slots
/// from `animId` with `animArg`, 2 resets them to `animId`, and both then
/// advance to 3, which ticks the slots. `appliedAnimId` records the clip the
/// slots were last seeded with. The placement opcodes cache the root yaw in
/// `yaw`; the "walk to" opcode leaves the remaining distance, in steps of 12,
/// in `travel`, which the step body counts down while clip 4 plays.
/// `field_4F0` is the task of the enemy spawned alongside this one, whose
/// model the visibility opcode drives together with the actor's own.
typedef struct Actor215100Work {
    /* 0x000 */ MATRIX     light;
    /* 0x020 */ MATRIX     color;
    /* 0x040 */ GpAnimCtx  anim;
    /* 0x054 */ GpAnimSlot slots[0x14];
    /* 0x374 */ byte       field_374;
    /* 0x375 */ byte       pad_375[0x13F];
    /* 0x4B4 */ s16        state;
    /* 0x4B6 */ s16        appliedAnimId;
    /* 0x4B8 */ s16        animId;
    /* 0x4BA */ s16        field_4BA;
    /* 0x4BC */ byte       pad_4BC[0x2A];
    /* 0x4E6 */ u16        yaw;
    /* 0x4E8 */ byte       pad_4E8[0x2];
    /* 0x4EA */ s16        travel;
    /* 0x4EC */ s16        animArg;
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

#endif
