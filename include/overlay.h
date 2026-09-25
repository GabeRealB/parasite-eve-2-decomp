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

#endif /* OVERLAY_H */
