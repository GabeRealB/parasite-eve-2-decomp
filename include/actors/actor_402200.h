#ifndef ACTOR_402200_H
#define ACTOR_402200_H

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "gameplay/1BC.h"

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
///
/// `field_6EA` is a pending tint request: `func_actor_402200_80137FB0` reads it
/// once a frame and, while it is 1 or 2, hands the display object the matching
/// translate (0, 0, 0x400 or full 0xFFF) and clears it, so each request is
/// consumed on the frame it is raised.
///
/// `field_718` arms a one-shot vocal cue and `field_71A` is its frame counter.
/// While the flag is clear the body does nothing; once it is set the counter
/// runs up, plays the actor's cue at 0x14, and at 0x5F asks the scene for
/// message 0x3ED - clearing the flag and sending 0x3F1 instead if the scene
/// refuses it.
typedef struct Actor402200Work {
    /* 0x000 */ byte pad_0[0x6E2];
    /* 0x6E2 */ s16  field_6E2;
    /* 0x6E4 */ byte pad_6E4[6];
    /* 0x6EA */ s16  field_6EA;
    /* 0x6EC */ byte pad_6EC[8];
    /* 0x6F4 */ s16  field_6F4;
    /* 0x6F6 */ byte pad_6F6[0x22];
    /* 0x718 */ s16  field_718;
    /* 0x71A */ s16  field_71A;
} Actor402200Work;
STATIC_ASSERT_SIZEOF(Actor402200Work, 0x71C);

/// Actor context handed to this overlay's callbacks: `field_1C` is the work
/// block above, `field_20` the `GpEnemy` the spawner left in the task's
/// `Task::spawnArg2` slot, and `field_2C` the display object. Same shape as the
/// other actor overlays' contexts.
typedef struct Actor402200 {
    /* 0x00 */ byte              pad_0[0x1C];
    /* 0x1C */ Actor402200Work*  field_1C;
    /* 0x20 */ GpEnemy*          field_20;
    /* 0x24 */ byte              pad_24[8];
    /* 0x2C */ Actor402200Obj2C* field_2C;
} Actor402200;

#endif
