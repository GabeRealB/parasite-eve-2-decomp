/* Copies the scene drawn so far to off-screen VRAM, for effects that sample
 * what is behind them. Include this header in the prologue and
 * frame_capture.inc.c at its function's position.
 */

#ifndef SCREEN_FRAME_CAPTURE_FRAME_CAPTURE_H
#define SCREEN_FRAME_CAPTURE_FRAME_CAPTURE_H

#include <core/pe2.h>

#include <engine/display.h>

/// Name of the capture function; defaults to `frameCaptureQueue`. A package that keeps it to one unit declares
/// `_frameCaptureQueue` static and defines this to it before including the header.
#ifndef FRAME_CAPTURE_QUEUE
#define FRAME_CAPTURE_QUEUE frameCaptureQueue
#endif

/// Queues, at ordering-table slot `orderingTableSlot`, a copy of the draw buffer to VRAM (448,256), so it captures
/// what is drawn behind that depth; drawing returns to the draw buffer afterwards.
void FRAME_CAPTURE_QUEUE(s32 orderingTableSlot);

#endif /* SCREEN_FRAME_CAPTURE_FRAME_CAPTURE_H */
