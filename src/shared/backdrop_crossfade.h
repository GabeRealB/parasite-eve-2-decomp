/* A cross-fade between a still backdrop parked in off-screen VRAM and the live
 * frame. The backdrop task redraws the displayed frame buffer as two SPRTs
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

void crossfadeDrawLive(s32 shade);
void crossfadeOutState(Task* task);
void crossfadeSetTpage(s32 tpage, s16 arg1);

/* Defined by each package. */
void crossfadeDrawBackdrop(s32 shade);

#endif /* SRC_SHARED_BACKDROP_CROSSFADE_H */
