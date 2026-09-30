/* The screen-wave effect: a task that redraws the rendered frame as a grid of
 * textured quads whose corners sine waves push around, used by rooms and actors
 * that shake the view.
 *
 * Include this header in the prologue and screen_wave.inc.c at the task's
 * position. The task's state belongs to the package, which declares and
 * defines it at its own positions under these names - declaring it here would
 * move it, since bss is laid out in first-declaration order:
 *
 *   s32              gScreenWaveRamp       the strength's ramp towards the peak
 *   OverlayWaveCtx*  gScreenWaveCtx        the context the task was spawned with
 *   OverlayWaveRec6  gScreenWaveColumns[]  per-column phase, offset and speed
 *   OverlayWaveRec6  gScreenWaveRows[]     per-row phase, offset and speed
 */

#ifndef SRC_SHARED_SCREEN_WAVE_H
#define SRC_SHARED_SCREEN_WAVE_H

#include "main/task_types.h"

#include "overlay.h"

void screenWaveTask(Task* arg0);

#endif /* SRC_SHARED_SCREEN_WAVE_H */
