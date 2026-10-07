/* A cross-fade between a still backdrop parked in off-screen VRAM and the live
 * frame. The backdrop task redraws the current draw buffer as two SPRTs
 * (192 and 128 wide) at one shade and the stored backdrop semi-transparently
 * at the complementary shade, each followed by a 15-bit tpage for its VRAM
 * source, in OT slot 8. Its fade-out state trades the two shades eight levels
 * a frame.
 *
 * Include this header in the prologue and each fragment at its function's
 * position.
 */

#ifndef SRC_SHARED_BACKDROP_CROSSFADE_H
#define SRC_SHARED_BACKDROP_CROSSFADE_H

#include "types.h"

#include "main/task_types.h"

/// Shared ordering tag and RGB modulation scale for backdrop crossfade packets.
enum {
    CROSSFADE_ORDERING_TABLE_SLOT = 8,
    CROSSFADE_SHADE_UNITY         = 0x80, // Textured RGB modulation at original brightness
};

static void _crossfadeDrawLive(s32 shade);
void        crossfadeOutState(Task* task);
static void _crossfadeSetTpage(s32 vramX, s16 vramY);

/* Defined by each package. */
void crossfadeDrawBackdrop(s32 shade);

#endif /* SRC_SHARED_BACKDROP_CROSSFADE_H */
