#ifndef ACTOR_215100_H
#define ACTOR_215100_H

#include "common.h"

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

#endif
