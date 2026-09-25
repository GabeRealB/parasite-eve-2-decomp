#ifndef OVERLAY_H
#define OVERLAY_H

#include "common.h"

#include <psyq/libgte.h>

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

#endif /* OVERLAY_H */
