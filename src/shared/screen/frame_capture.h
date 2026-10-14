/* Shared depth-sorted frame capture for effects that sample the scene behind
 * them. Each carrier supplies its function instance's linkage and identifier.
 *
 * Include this header in the prologue and each fragment at its function's
 * position.
 */

#ifndef SRC_SHARED_FRAME_CAPTURE_H
#define SRC_SHARED_FRAME_CAPTURE_H

#include "types.h"

/// Selects this carrier's `void(s32)` frame-capture function identifier.
///
/// Defaults to the overlay-private `frameCaptureQueue` used across the two
/// actor_403600 translation units. A carrier used by only one translation unit
/// declares `_frameCaptureQueue` static before this header and binds this macro
/// to that identifier. Keep the binding through the implementation and callers.
/// Expands only to an identifier; captures no values and performs no evaluation.
#ifndef FRAME_CAPTURE_QUEUE
#define FRAME_CAPTURE_QUEUE frameCaptureQueue
#endif

/// Queues a depth-sorted copy of the current draw buffer into off-screen VRAM.
///
/// Copies the fixed 320x240 frame to VRAM (448,256) for subsequent textured
/// drawing. Requires the centered 16-bit layout: buffer 0 at (0,0), buffer 1
/// at (0,272), selected by `gDisplayState.drawBuffer`. `orderingTableSlot`
/// counts tags from `gGpuCurrentOt`, with no depth scaling, masking or clamp;
/// the selected table must contain that slot, including callers' depth bias.
/// The normal game table supports nonnegative slots through 1055.
///
/// Consumes eleven GPU packets (144 bytes) in the current frame arena. That
/// storage must remain live until drawing completes; the source buffer must
/// already contain the desired scene when the packets execute. Requires live
/// mapped-view resources and 20 aligned scratch-stack bytes, released before
/// returning. No capacity or bounds checks are performed.
///
/// Restores the current buffer's centered offset and disables mask-bit writes.
/// Restores the first view clip if its scaled, wrapped restore slot is below
/// `orderingTableSlot`, otherwise the full buffer. Leaves the left source
/// texture page selected with dithering and drawing in the display area enabled.
void FRAME_CAPTURE_QUEUE(s32 orderingTableSlot);

#endif /* SRC_SHARED_FRAME_CAPTURE_H */
