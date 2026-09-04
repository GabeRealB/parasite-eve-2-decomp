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

/// One quad of the sanctuary's blocker cage, from the four-entry table at
/// `D_acropolis_sanctuary_801822AC` (the template) and its live copy hanging
/// off `AcsBlockerSet::edges`. The quad is given as two index pairs into
/// `AcsBlockerSet::corners` -- `bottom0` / `bottom1` are the low corners of the
/// two posts it spans and `top0` / `top1` the high ones -- plus the index of
/// its facing direction in `AcsBlockerSet::normals`.
/// `func_acropolis_sanctuary_8017DD78` sets `field_A` to 1 on every quad it
/// copies, which is what arms the cage.
typedef struct AcsBlockerEdge {
    /* 0x0 */ s16 bottom0;
    /* 0x2 */ s16 bottom1;
    /* 0x4 */ s16 top0;
    /* 0x6 */ s16 top1;
    /* 0x8 */ s16 normal;
    /* 0xA */ s16 field_A;
} AcsBlockerEdge;
STATIC_ASSERT_SIZEOF(AcsBlockerEdge, 0xC);

/// The sanctuary's blocker cage: four unit-length facing directions, eight
/// corner points (four posts, low corner then high corner) and the four quads
/// that join them. The overlay holds two of these -- the template at
/// `D_acropolis_sanctuary_801822EC` and the live set at
/// `D_acropolis_sanctuary_80183568`, whose arrays are longer because the live
/// set is also used for the room's other geometry.
/// `func_acropolis_sanctuary_8017DD78` copies the template's first four
/// normals, eight corners and four quads into the live set.
typedef struct AcsBlockerSet {
    /* 0x00 */ s32             field_0;
    /* 0x04 */ SVECTOR*        normals;
    /* 0x08 */ SVECTOR*        corners;
    /* 0x0C */ AcsBlockerEdge* edges;
    /* 0x10 */ s16**           field_10;
    /* 0x14 */ s32             field_14;
    /* 0x18 */ s32             field_18;
    /* 0x1C */ s16             field_1C;
    /* 0x1E */ s16             field_1E;
    /* 0x20 */ s16             field_20;
    /* 0x22 */ s16             field_22;
} AcsBlockerSet;
STATIC_ASSERT_SIZEOF(AcsBlockerSet, 0x24);

/// The offset `func_acropolis_sanctuary_8017DD78` adds to every corner of the
/// blocker cage once it has been copied. Both variants are positive, and the
/// components are read back unsigned because only the low 16 bits of the sum
/// reach the `s16` corner they are added to.
typedef struct AcsBlockerShift {
    /* 0x0 */ u16 vx;
    /* 0x2 */ u16 vy;
    /* 0x4 */ u16 vz;
} AcsBlockerShift;

/// Per-frame scratch the sanctuary's flame sprite builds at `G_SCRATCH_HEAD`:
/// `pos` is the packed `SVECTOR` fed to RTPS, `sx` / `sy` the projected screen
/// point, `otz` the depth (`SZ3 >> 2`) the ordering-table slot is taken from
/// and `half` the sprite's half-extent, `field_24 * 0x27 / otz`, so the flame
/// shrinks with distance.
typedef struct AcsSprayScratch {
    /* 0x00 */ s32     otz;
    /* 0x04 */ s32     half;
    /* 0x08 */ SVECTOR pos;
    /* 0x10 */ u16     sx;
    /* 0x12 */ u16     sy;
} AcsSprayScratch;
STATIC_ASSERT_SIZEOF(AcsSprayScratch, 0x14);

/// One grey level per sprite variant, indexed by the variant the flame task
/// picked out of `Task::spawnArg1` (bits 8..9). The overlay holds two of these,
/// the base level `D_acropolis_sanctuary_8017D5D8` and the per-frame flicker
/// amplitude `D_acropolis_sanctuary_8017D5DC`; both are copied onto the stack
/// so the variant index can subscript them.
typedef struct AcsSpriteLevels {
    /* 0x0 */ u8 v[3];
} AcsSpriteLevels;

#endif
