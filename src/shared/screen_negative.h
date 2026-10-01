/* An in-place image filter over the 320x240 16-bit frame captured into
 * Fs_ImgBuffers: it turns every pixel into its inverted luminance, a grey
 * photographic negative, for scenes that freeze the picture as a negative.
 *
 * Include this header in the prologue and screen_negative_filter.inc.c at the
 * filter's position. The filter is static; a file that needs it under another
 * name as well, as actor_460200 does for its cutscene script, includes the
 * fragment a second time with screenNegativeFilter defined to that name.
 *
 * screen_negative_capture.inc.c is the task that freezes the picture: it
 * captures the frame, filters it and holds it. The task is spawned from each
 * package's own table, so the package keeps its name for it and calls the
 * inline body. Its two rectangles are the package's data, defined at their
 * own positions under the names declared here.
 */

#ifndef SRC_SHARED_SCREEN_NEGATIVE_H
#define SRC_SHARED_SCREEN_NEGATIVE_H

#include <psyq/libgpu.h>

static void screenNegativeFilter(void);

extern RECT gScreenNegativeFrameRect; /* the whole 320x240 frame */
extern RECT gScreenNegativeStripRect; /* one 16x240 strip of the shown buffer */

#endif                                /* SRC_SHARED_SCREEN_NEGATIVE_H */
