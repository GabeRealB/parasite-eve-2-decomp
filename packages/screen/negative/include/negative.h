/* An in-place image filter over the 320x240 16-bit frame captured into
 * gFsImageWorkspacePtr: it turns every pixel into its inverted luminance, a grey
 * photographic negative, for scenes that freeze the picture as a negative.
 *
 * Include this header in the prologue and filter.inc.c at the
 * filter's position. The filter is static; a file that needs it under another
 * name as well, as actor_460200 does for its cutscene script, includes the
 * fragment a second time with SCREEN_NEGATIVE_FILTER bound to that name.
 *
 * capture.inc.c is the task that freezes the picture: it
 * captures the frame, filters it and holds it. The task is spawned from each
 * package's own table, so the package keeps its name for it and calls the
 * inline body, and spawns it with the address of a ScreenNegativeCaptureArgs
 * block it owns. Each carrier defines its private `_gScreenNegativeFrameRect`
 * and `_gScreenNegativeStripRect` before including the capture fragment.
 */

#ifndef SCREEN_NEGATIVE_NEGATIVE_H
#define SCREEN_NEGATIVE_NEGATIVE_H

#include <psyq/libgpu.h>

#include <core/pe2.h>

/// The capture task's spawn argument: how long to hold the negative, and a flag the package sets to end it early.
typedef struct {
    u16 duration; // Frames the negative is held
    s16 done;     // Nonzero ends the hold; set by the task when it times out
} ScreenNegativeCaptureArgs;

#ifndef SCREEN_NEGATIVE_FILTER
/// Name of the static filter filter.inc.c defines and the capture task calls; defaults to `_screenNegativeFilter`.
/// filter.inc.c needs `engine/fs.h`, `engine/fs_types.h` and `common.h` included first.
#define SCREEN_NEGATIVE_FILTER _screenNegativeFilter
#endif

static void SCREEN_NEGATIVE_FILTER(void);

#endif /* SCREEN_NEGATIVE_NEGATIVE_H */
