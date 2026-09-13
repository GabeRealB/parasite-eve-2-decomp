#ifndef ACTOR_402200_H
#define ACTOR_402200_H

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

/// The actor's per-part attach coordinates, 0x50 apart, hanging off the
/// display object's 0x08 slot (`TmdObject::field_8`, the trailing per-part
/// `GsCOORDINATE2` array). This overlay's code reaches the root and the fourth
/// part: `func_actor_402200_80137444` parks `&field_F0` in the work block's
/// 0x65C slot, and the shared ground-quad body `ActorsShared8013806c` takes the
/// shadow quad's horizontal position from `field_F0` and its height from
/// `field_0`.
typedef struct Actor402200Coord {
    /* 0x000 */ GsCOORDINATE2 field_0;
    /* 0x050 */ byte          pad_50[0xA0];
    /* 0x0F0 */ GsCOORDINATE2 field_F0;
} Actor402200Coord;

/// Display object hung off `Actor402200::field_2C`; `field_8` is the per-part
/// coordinate array above.
typedef struct Actor402200Obj2C {
    /* 0x00 */ byte              pad_0[8];
    /* 0x08 */ Actor402200Coord* field_8;
} Actor402200Obj2C;

/// Per-instance work block the overlay's setup `func_actor_402200_80137444`
/// allocates with `Mem_Calloc(0x71C)` and parks in the 0x1C slot below (the
/// task's `Task::idMap`, which is not a `TaskIdMap` here).
///
/// `field_6E2` is the ground-shadow shade the shared ground-quad body
/// `ActorsShared8013806c` hands to `Gp_DrawEffGroundQuad`, which draws nothing
/// for a negative value: the frame code turns the calloc'd zero into -1 the
/// first time it runs, so an actor that never raises the shade casts no shadow.
///
/// `field_6F4` is the actor's phase, written and read as a signed halfword:
/// the frame handler clears it on entry, `ActorsShared801381e0` raises it to 1
/// while the remaining-enemy count is positive, and the handler branches on
/// 0 / 1 thereafter.
typedef struct Actor402200Work {
    /* 0x000 */ byte pad_0[0x6E2];
    /* 0x6E2 */ s16  field_6E2;
    /* 0x6E4 */ byte pad_6E4[0x10];
    /* 0x6F4 */ s16  field_6F4;
    /* 0x6F6 */ byte pad_6F6[0x26];
} Actor402200Work;
STATIC_ASSERT_SIZEOF(Actor402200Work, 0x71C);

/// Actor context handed to this overlay's callbacks: `field_1C` is the work
/// block above and `field_2C` the display object. Same shape as the other
/// actor overlays' contexts.
typedef struct Actor402200 {
    /* 0x00 */ byte              pad_0[0x1C];
    /* 0x1C */ Actor402200Work*  field_1C;
    /* 0x20 */ byte              pad_20[0xC];
    /* 0x2C */ Actor402200Obj2C* field_2C;
} Actor402200;

#endif
