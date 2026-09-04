#ifndef ROOMS_ACROPOLIS_SANCTUARY_H
#define ROOMS_ACROPOLIS_SANCTUARY_H

#include "common.h"

#include "gameplay/3CD8.h"
#include "main/task.h"
#include "rooms/room_common.h"

/// 0xC work block of the sanctuary's cutscene task, hung off the `Task::idMap`
/// slot (0x1C) -- that slot is *not* a `TaskIdMap` here, it is the
/// `Mem_Calloc(0xC)` block `func_acropolis_sanctuary_8017DA40` allocates and
/// zeroes before publishing the owning task in
/// `D_acropolis_sanctuary_80186C90`. Reach it with `(AcsCutsceneWork*)task->idMap`.
///
/// `target` is the slot-3 task the block's messages are addressed to, captured
/// once from `Game_GetPtrSlot(3)`. `phase` is the script step the driver in
/// `func_acropolis_sanctuary_8017DA40` runs -- it only acts on phase 2, and
/// then only while `step` is still 0, bumping `step` once the scene has been
/// dispatched so it fires exactly once.
typedef struct AcsCutsceneWork {
    /* 0x0 */ Task* target;
    /* 0x4 */ u16   phase;
    /* 0x6 */ u16   step;
    /* 0x8 */ s32   field_8;
} AcsCutsceneWork;
STATIC_ASSERT_SIZEOF(AcsCutsceneWork, 0xC);

/// Scratch payload `func_acropolis_sanctuary_8017DA40` builds on its own stack
/// for the slot-3 messages it sends: `rec` is the 0x14-byte record msgs 0x3E8
/// (weapon) and 0x3F4 take, `place` the position + Euler rotation msg 0x3E9
/// takes. One buffer serves both because the task only ever has one message in
/// flight, and the union is what makes the 0x18-byte frame slot the two share
/// explicit.
typedef union AcsMsgArg {
    /* 0x0 */ GpRec14       rec;
    /* 0x0 */ RoomPlacement place;
} AcsMsgArg;
STATIC_ASSERT_SIZEOF(AcsMsgArg, 0x18);

/// One corner of an `AcsQuad`, laid out like an `SVECTOR` but read unsigned:
/// every consumer either copies the component into an `SVECTOR` verbatim or
/// subtracts it from a value that is truncated back to 16 bits, so the sign of
/// the load never reaches the result.
typedef struct AcsQuadCorner {
    /* 0x0 */ u16 vx;
    /* 0x2 */ u16 vy;
    /* 0x4 */ u16 vz;
    /* 0x6 */ u16 pad;
} AcsQuadCorner;
STATIC_ASSERT_SIZEOF(AcsQuadCorner, 8);

/// A size class of the sanctuary's mosaic effect: the four corner offsets a
/// tile of that class is drawn with. `D_acropolis_sanctuary_80182710` holds two
/// of them, a small one and a double-sized one, and `AcsTile::quad` picks
/// between them. `func_acropolis_sanctuary_8017E134` only needs `corner[0]`,
/// the origin the tile's grid position is measured from.
typedef struct AcsQuad {
    /* 0x0 */ AcsQuadCorner corner[4];
} AcsQuad;
STATIC_ASSERT_SIZEOF(AcsQuad, 0x20);

/// One tile of the sanctuary's mosaic, from the 72-entry table at
/// `D_acropolis_sanctuary_80182320`. `row` and `col` are grid coordinates that
/// `func_acropolis_sanctuary_8017E134` scales by 1145/128 and 2147/256 into the
/// spawn offset, and `quad` selects the tile's size class in
/// `D_acropolis_sanctuary_80182710`.
typedef struct AcsTile {
    /* 0x0 */ s16 field_0;
    /* 0x2 */ s16 field_2;
    /* 0x4 */ s16 row;
    /* 0x6 */ s16 col;
    /* 0x8 */ s16 field_8;
    /* 0xA */ s16 field_A;
    /* 0xC */ s16 quad;
} AcsTile;
STATIC_ASSERT_SIZEOF(AcsTile, 0xE);

#endif
