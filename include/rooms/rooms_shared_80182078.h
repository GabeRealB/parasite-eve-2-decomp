#ifndef ROOMS_SHARED_80182078_H
#define ROOMS_SHARED_80182078_H

#include "common.h"

#include "main/session.h"

#include <psyq/libgs.h>
#include <psyq/libgte.h>

/// 0x20 scratch block the shared coordinate walk takes from
/// `G_SCRATCH_HEAD`. `coord` is the frame the walk is currently standing on
/// (it climbs the `GsCOORDINATE2::sub` parent chain until NULL), `vec` is the
/// vector being carried up into world space, and `out` receives the `MVMVA`
/// result (`MAC1..3`) that is fed back into `vec` each step. `flag` takes the
/// GTE flag register so the block matches what the code stores.
typedef struct RoomsShared80182078Walk {
    /* 0x00 */ GsCOORDINATE2* coord;
    /* 0x04 */ SVECTOR        vec;
    /* 0x0C */ s32            out[3];
    /* 0x18 */ s32            pad_18;
    /* 0x1C */ s32            flag;
} RoomsShared80182078Walk;
STATIC_ASSERT_SIZEOF(RoomsShared80182078Walk, 0x20);

/// 0xE4 scratch block `RoomsShared80182078` takes from
/// `G_SCRATCH_HEAD` while it nudges a coordinate frame away from the
/// obstacles recorded in a `GpRec18` table. `m` is the working matrix handed
/// to `Gfx_RotMatrixY` / `Gfx_MatrixCol2`. `eye` is the frame's own world
/// position and `aim` the world point one unit (0x1000) in front of it, both
/// produced by walking the parent chain; `delta` is the scratch difference fed
/// to `ratan2` and later the GPF-scaled push applied to `coord.t[0]` /
/// `coord.t[2]`. `kind` is the record's `field_4` high halfword, `angle[]` the
/// per-record bearing relative to the facing direction (0x7FFE marks "no more
/// records", 0x7FFF "record does not count"), `i` / `j` the two loop counters,
/// `diff` the wrapped bearing difference between a pair of records and `hit`
/// the value the function returns.
typedef struct RoomsShared80182078Scratch {
    /* 0x00 */ MATRIX  m;
    /* 0x20 */ byte    pad_20[0x80];
    /* 0xA0 */ SVECTOR delta;
    /* 0xA8 */ SVECTOR eye;
    /* 0xB0 */ SVECTOR aim;
    /* 0xB8 */ s32     kind;
    /* 0xBC */ s16     angle[0x10];
    /* 0xDC */ s16     i;
    /* 0xDE */ s16     j;
    /* 0xE0 */ s16     diff;
    /* 0xE2 */ s16     hit;
} RoomsShared80182078Scratch;
STATIC_ASSERT_SIZEOF(RoomsShared80182078Scratch, 0xE4);

/// Nudges a coordinate frame away from the obstacles recorded in a `GpRec18`
/// table: it takes the frame's world position and the point one unit in front
/// of it, sorts the records by bearing, and where two of them close to within
/// 0x400 pushes the frame `push` units along the bisector. Returns 1 when a
/// push was applied. Six acropolis rooms carry this body.
s32 RoomsShared80182078(GsCOORDINATE2* coord, GpRec18* recs, s16 count, s16 push);

#endif // ROOMS_SHARED_80182078_H
