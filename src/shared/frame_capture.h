/* Copies the rendered frame into off-screen VRAM partway through the ordering
 * table, so later primitives (see-through or distorting enemies) can sample
 * the scene behind them. The copy is a short run of GPU drawing-environment
 * primitives queued at a chosen depth.
 *
 * Include this header in the prologue and each fragment at its function's
 * position.
 */

#ifndef SRC_SHARED_FRAME_CAPTURE_H
#define SRC_SHARED_FRAME_CAPTURE_H

#include "types.h"

void frameCaptureQueue(s32 otz);

#endif /* SRC_SHARED_FRAME_CAPTURE_H */
