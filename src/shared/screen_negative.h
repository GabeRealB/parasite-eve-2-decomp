/* An in-place image filter over the 320x240 16-bit frame captured into
 * Fs_ImgBuffers: it turns every pixel into its inverted luminance, a grey
 * photographic negative, for scenes that freeze the picture as a negative.
 *
 * Include this header in the prologue and screen_negative_filter.inc.c at the
 * filter's position. The filter is static; a file that needs it under another
 * name as well, as actor_460200 does for its cutscene script, includes the
 * fragment a second time with SCREEN_NEGATIVE_FILTER bound to that name.
 *
 * screen_negative_capture.inc.c is the task that freezes the picture: it
 * captures the frame, filters it and holds it. The task is spawned from each
 * package's own table, so the package keeps its name for it and calls the
 * inline body, and spawns it with the address of a ScreenNegativeCaptureArgs
 * block it owns. Its two rectangles are the package's data, defined at their
 * own positions under the names declared here.
 */

#ifndef SRC_SHARED_SCREEN_NEGATIVE_H
#define SRC_SHARED_SCREEN_NEGATIVE_H

#include <psyq/libgpu.h>

#include "common.h"

/// What a package hands the capture task as its second spawn argument, and
/// keeps for as long as the task runs.
///
/// The task takes `duration` as it starts and clears `done`; from then on it
/// ends, and lets drawing resume, on the first frame it finds `done` nonzero.
/// It sets the flag itself when the hold runs out, and the package sets it to
/// end the freeze early.
typedef struct {
    u16 duration; // Frames the negative is held before the task ends on its own
    s16 done;     // Nonzero ends the capture (0 running, 1 timed out or cancelled)
} ScreenNegativeCaptureArgs;
STATIC_ASSERT_SIZEOF(ScreenNegativeCaptureArgs, 0x4);

#ifndef SCREEN_NEGATIVE_FILTER
/// Selects the private `void (void)` filter declared here and defined by the fragment.
///
/// The value must be a bare function identifier. The default capture-task copy
/// is `screenNegativeFilter`. For a further copy, declare its static prototype
/// in the carrier's prologue, undefine this binding and rebind it around the
/// fragment inclusion, then restore the default. The guarded header does not
/// declare further copies. This alias has no arguments, captures no values and
/// constructs no tokens. The fragment requires `main/fs.h`, `main/fs_types.h`
/// and `common.h`; capture callers must use the same binding as their filter.
#define SCREEN_NEGATIVE_FILTER screenNegativeFilter
#endif

static void SCREEN_NEGATIVE_FILTER(void);

extern RECT gScreenNegativeFrameRect; /* the whole 320x240 frame */
extern RECT gScreenNegativeStripRect; /* one 16x240 strip of the shown buffer */

#endif                                /* SRC_SHARED_SCREEN_NEGATIVE_H */
