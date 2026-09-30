/* The screen-wave effect: a task that redraws the rendered frame as a grid of
 * textured quads whose corners sine waves push around, used by rooms and actors
 * that shake the view. It comes in two versions: screenWaveTask emits a 10x30
 * grid from the primitive cursor every frame, while screenWaveGridTask keeps a
 * prebuilt, double-buffered 8x30 grid of 40x8 quads and only moves their
 * corners.
 *
 * Include this header in the prologue and screen_wave.inc.c or
 * screen_wave_grid.inc.c at the task's position. The task's state belongs to
 * the package, which declares and defines it at its own positions under these
 * names - declaring it here would move it, since bss is laid out in
 * first-declaration order:
 *
 *   s32              gScreenWaveRamp       the strength's ramp towards the peak
 *   OverlayWaveCtx*  gScreenWaveCtx        the context the task was spawned with
 *   OverlayWaveRec6  gScreenWaveColumns[]  per-column phase, offset and speed
 *   OverlayWaveRec6  gScreenWaveRows[]     per-row phase, offset and speed
 *   POLY_FT4         gScreenWaveGrid[2][30][8]  the prebuilt grids (grid task)
 *
 * The grid task's records are OverlayWaveRec rather than OverlayWaveRec6. A
 * package that keeps the grid inside a larger object names it through
 * SCREEN_WAVE_GRID before including this header.
 */

#ifndef SRC_SHARED_SCREEN_WAVE_H
#define SRC_SHARED_SCREEN_WAVE_H

#include "main/task_types.h"

#include "overlay.h"

#ifndef SCREEN_WAVE_GRID
#define SCREEN_WAVE_GRID gScreenWaveGrid
#endif

void screenWaveTask(Task* arg0);
void screenWaveGridTask(Task* arg0);

#endif /* SRC_SHARED_SCREEN_WAVE_H */
