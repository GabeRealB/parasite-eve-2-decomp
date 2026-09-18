#ifndef ROOMS_SHARED_80182078_H
#define ROOMS_SHARED_80182078_H

#include "common.h"

#include "main/mem.h"
#include "main/session.h"

#include <psyq/libgs.h>
#include <psyq/inline_c.h>
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
/// `coord.t[2]`. `kind` is the record's `key` high halfword, `angle[]` the
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

/// `mvmva 1, 0, 0, 0, 0`: rotate V0 by the rotation matrix and add the
/// translation vector. The `inline_c.h` macro of that name assembles to a
/// different word, so spell the instruction out.
#define gte_rtv0tr_real() __asm__ volatile("nop; nop; .word 0x4A480012")
/// `gpf 1`: scale IR1..3 by IR0. Same reason as above for spelling out the word.
#define gte_gpf12_real() __asm__ volatile("nop; nop; .word 0x4B98003D")

/// Carries `v` from the local frame `coord` up the `GsCOORDINATE2::sub` parent
/// chain into world space, using a 0x20 scratch block from `G_SCRATCH_HEAD`.
static __inline__ void RoomsShared80182078ToWorld(GsCOORDINATE2* coord, SVECTOR* v)
{
    RoomsShared80182078Walk* blk;

    {
        register GsCOORDINATE2* parent asm("v0");
        parent                                                                                              = coord;
        ((RoomsShared80182078Walk*)((u8*)*(void**)G_SCRATCH_HEAD - sizeof(RoomsShared80182078Walk)))->coord = parent;
    }
    {
        register u8* tmp asm("v0");
        tmp = (u8*)*(void**)G_SCRATCH_HEAD - sizeof(RoomsShared80182078Walk);
        blk = (RoomsShared80182078Walk*)tmp;
    }
    blk->vec.vx = v->vx;
    blk->vec.vy = v->vy;
    blk->vec.vz = v->vz;

    *(void**)G_SCRATCH_HEAD = blk;
    while (blk->coord != NULL) {
        gte_SetTransMatrix(&blk->coord->coord);
        gte_SetRotMatrix(&blk->coord->coord);
        gte_ldv0(&blk->vec);
        gte_rtv0tr_real();
        gte_stlvnl(blk->out);
        gte_stflg(&blk->flag);
        blk->vec.vx = *(u16*)&blk->out[0];
        blk->vec.vy = *(u16*)&blk->out[1];
        blk->vec.vz = *(u16*)&blk->out[2];
        blk->coord  = blk->coord->sub;
    }
    v->vx = blk->vec.vx;
    v->vy = blk->vec.vy;
    v->vz = blk->vec.vz;

    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + sizeof(RoomsShared80182078Walk);
}
static __inline__ void RoomsShared80182078ToWorld2(GsCOORDINATE2* coord, SVECTOR* v)
{
    RoomsShared80182078Walk* blk;

    blk         = (RoomsShared80182078Walk*)((u8*)*(void**)G_SCRATCH_HEAD - sizeof(RoomsShared80182078Walk));
    blk->coord  = coord;
    blk->vec.vx = v->vx;
    blk->vec.vy = v->vy;
    blk->vec.vz = v->vz;

    *(void**)G_SCRATCH_HEAD = blk;
    while (blk->coord != NULL) {
        gte_SetTransMatrix(&blk->coord->coord);
        gte_SetRotMatrix(&blk->coord->coord);
        gte_ldv0(&blk->vec);
        gte_rtv0tr_real();
        gte_stlvnl(blk->out);
        gte_stflg(&blk->flag);
        blk->vec.vx = *(u16*)&blk->out[0];
        blk->vec.vy = *(u16*)&blk->out[1];
        blk->vec.vz = *(u16*)&blk->out[2];
        blk->coord  = blk->coord->sub;
    }
    v->vx = blk->vec.vx;
    v->vy = blk->vec.vy;
    v->vz = blk->vec.vz;

    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + sizeof(RoomsShared80182078Walk);
}

#endif // ROOMS_SHARED_80182078_H
