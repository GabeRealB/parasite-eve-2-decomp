/* An in-place image filter over the 320x240 16-bit frame captured into
 * Fs_ImgBuffers: it turns every pixel into its inverted luminance, a grey
 * photographic negative, for scenes that freeze the picture as a negative.
 *
 * Include this header in the prologue and screen_negative_filter.inc.c at the
 * filter's position. The filter is static; a file that needs it under another
 * name as well, as actor_460200 does for its cutscene script, includes the
 * fragment a second time with screenNegativeFilter defined to that name.
 */

#ifndef SRC_SHARED_SCREEN_NEGATIVE_H
#define SRC_SHARED_SCREEN_NEGATIVE_H

static void screenNegativeFilter(void);

#endif /* SRC_SHARED_SCREEN_NEGATIVE_H */
