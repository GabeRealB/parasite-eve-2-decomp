#ifndef OVERLAY_H
#define OVERLAY_H

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgs.h>
#include <psyq/inline_c.h>
#include "gte.h"

#include "main/mem.h"

/*
 * Types that room and actor overlays both carry.
 *
 * Every overlay is linked on its own, so code that several of them share was
 * compiled into each one. The layouts below are the ones that code repeats in
 * rooms and actors alike and that neither the gameplay nor the main executable
 * owns; one declaration serves both families.
 */

/// Work block of a full-screen fade task, allocated eight bytes at a time and
/// kept at `Task::work`: the three colour channels the fade overlay is drawn
/// with, stepped toward white or black each frame. The channels are signed,
/// since a fade-in ends when a channel goes negative. The leading halfword is
/// never touched.
typedef struct OverlayFadeWork {
    byte pad_0[0x2];
    s16  r;
    s16  g;
    s16  b;
} OverlayFadeWork;
STATIC_ASSERT_SIZEOF(OverlayFadeWork, 0x8);

/// Ramp context of a screen-wave task, handed to the task as its spawn
/// argument. Whoever spawns the task seeds `span`, `scale` and the tint; the
/// task clears `frame` and `state`, then counts `frame` up to `span` while
/// `state` is 0 and back down to zero while it is 1, and ends itself once
/// `state` is 2.
/// The wave's amplitude is `frame * scale / span`. A nonzero `blend` shades the
/// wave mesh with `r`, `g` and `b`; zero draws the raw frame-buffer copy.
typedef struct OverlayWaveCtx {
    s16 span;
    s16 scale;
    s16 state;
    s16 frame;
    u8  blend;
    u8  r;
    u8  g;
    u8  b;
} OverlayWaveCtx;
STATIC_ASSERT_SIZEOF(OverlayWaveCtx, 0xC);

/// One row's or column's sine wave in a screen-wave mesh whose phase tables
/// are padded to eight bytes: `phase` advances by `speed` every frame, and the
/// displacement a vertex takes is the sine of its position plus `phase` and
/// the fixed `offset`.
typedef struct OverlayWaveRec {
    s16 phase;
    s16 offset;
    s16 speed;
    s16 pad_6;
} OverlayWaveRec;
STATIC_ASSERT_SIZEOF(OverlayWaveRec, 0x8);

/// The same wave record in the meshes whose phase tables are packed six bytes
/// apart.
typedef struct OverlayWaveRec6 {
    s16 phase;
    s16 offset;
    s16 speed;
} OverlayWaveRec6;
STATIC_ASSERT_SIZEOF(OverlayWaveRec6, 0x6);

/// The scratch-pad block the padded screen-wave mesh takes from
/// `G_SCRATCH_HEAD` for one frame: a copy of the row and column wave records
/// the mesh's vertices are displaced by.
typedef struct OverlayWaveScratch {
    OverlayWaveRec rows[30];
    OverlayWaveRec cols[9];
} OverlayWaveScratch;
STATIC_ASSERT_SIZEOF(OverlayWaveScratch, 0x138);

/// The scratch-pad block the world-space walk takes from `G_SCRATCH_HEAD`:
/// `coord` is the frame the walk stands on, climbing the `GsCOORDINATE2::sub`
/// parent chain until it runs out, `vec` the vector being carried up, `out`
/// the GTE result it is refreshed from after each frame, and `flag` the GTE
/// flag word.
typedef struct OverlayWalkScratch {
    GsCOORDINATE2* coord;
    SVECTOR        vec;
    s32            out[3];
    s32            pad_18;
    s32            flag;
} OverlayWalkScratch;
STATIC_ASSERT_SIZEOF(OverlayWalkScratch, 0x20);

/// The scratch-pad block of the push that steers a coordinate frame between
/// the obstacles in a `GpRec18` contact table. `eye` is the frame's world
/// position and `aim` the world point one unit ahead of it; `angle` holds each
/// record's bearing relative to the facing, 0x7FFE ending the list and 0x7FFF
/// marking a record that does not count. `kind` is the high half of a record's key,
/// `diff` the wrapped difference between two bearings, and `delta` first the
/// offset handed to `ratan2`, then the push added to the frame. `m` is the
/// working rotation, `i` and `j` the loop counters, and `hit` whether a push
/// was applied.
typedef struct OverlayBisectorScratch {
    MATRIX  m;
    byte    pad_20[0x80];
    SVECTOR delta;
    SVECTOR eye;
    SVECTOR aim;
    s32     kind;
    s16     angle[0x10];
    s16     i;
    s16     j;
    s16     diff;
    s16     hit;
} OverlayBisectorScratch;
STATIC_ASSERT_SIZEOF(OverlayBisectorScratch, 0xE4);

/// Carries `v` from the frame of `coord` up the parent chain into world
/// space, walking in an `OverlayWalkScratch` taken from the scratch pad.
static __inline__ void overlayToWorld(GsCOORDINATE2* coord, SVECTOR* v)
{
    OverlayWalkScratch* blk;

    {
        register GsCOORDINATE2* parent asm("v0");
        parent                                                                                    = coord;
        ((OverlayWalkScratch*)((u8*)*(void**)G_SCRATCH_HEAD - sizeof(OverlayWalkScratch)))->coord = parent;
    }
    {
        register u8* tmp asm("v0");
        tmp = (u8*)*(void**)G_SCRATCH_HEAD - sizeof(OverlayWalkScratch);
        blk = (OverlayWalkScratch*)tmp;
    }
    blk->vec.vx = v->vx;
    blk->vec.vy = v->vy;
    blk->vec.vz = v->vz;

    *(void**)G_SCRATCH_HEAD = blk;
    while (blk->coord != NULL) {
        gte_SetTransMatrix(&blk->coord->coord);
        gte_SetRotMatrix(&blk->coord->coord);
        gte_ldv0(&blk->vec);
        gte_rtv0tr();
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

    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + sizeof(OverlayWalkScratch);
}

/// The walk of `overlayToWorld` without its register bindings. The callers
/// were compiled from both spellings, and each site matches only its own.
static __inline__ void overlayToWorld2(GsCOORDINATE2* coord, SVECTOR* v)
{
    OverlayWalkScratch* blk;

    blk         = (OverlayWalkScratch*)((u8*)*(void**)G_SCRATCH_HEAD - sizeof(OverlayWalkScratch));
    blk->coord  = coord;
    blk->vec.vx = v->vx;
    blk->vec.vy = v->vy;
    blk->vec.vz = v->vz;

    *(void**)G_SCRATCH_HEAD = blk;
    while (blk->coord != NULL) {
        gte_SetTransMatrix(&blk->coord->coord);
        gte_SetRotMatrix(&blk->coord->coord);
        gte_ldv0(&blk->vec);
        gte_rtv0tr();
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

    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + sizeof(OverlayWalkScratch);
}

/// The scratch-pad block of an in-radius test on the XZ plane: the two
/// offsets and the radius, each squared in place before `dx + dz` is compared
/// with `r`.
typedef struct OverlayRangeScratch {
    s32 dx;
    s32 dz;
    s32 r;
} OverlayRangeScratch;
STATIC_ASSERT_SIZEOF(OverlayRangeScratch, 0xC);

/// Whether the XZ offset `d` reaches at least `r` from its origin.
static __inline__ s32 overlayOutOfRange(SVECTOR* d, s16 r)
{
    u8*                  head;
    OverlayRangeScratch* blk;
    s32                  ret;

    head                                      = *(u8**)G_SCRATCH_HEAD;
    ((OverlayRangeScratch*)(head - 0xC))->dx  = d->vx;
    blk                                       = (OverlayRangeScratch*)(head - 0xC);
    blk->dz                                   = d->vz;
    blk->r                                    = r;
    ((OverlayRangeScratch*)(head - 0xC))->dx *= ((OverlayRangeScratch*)(head - 0xC))->dx;
    *(OverlayRangeScratch**)G_SCRATCH_HEAD    = blk;
    blk->dz                                  *= blk->dz;
    blk->r                                   *= blk->r;
    *(u8**)G_SCRATCH_HEAD                     = head;
    ret                                       = ((OverlayRangeScratch*)(head - 0xC))->dx + blk->dz >= blk->r;
    return ret;
}

#endif /* OVERLAY_H */
