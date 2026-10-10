/* The screen-wave effect: a task that redraws the rendered frame as a grid of
 * textured quads whose corners sine waves push around, used by rooms and actors
 * that shake the view. It comes in two versions: _screenWaveTask emits a 10x30
 * grid from the primitive cursor every frame, while _screenWaveGridTask keeps a
 * prebuilt, double-buffered 8x30 grid of 40x8 quads and only moves their
 * corners.
 *
 * Each included function has static per-carrier linkage.
 * Only one wave task may be active per package, since its context, strength
 * and oscillators share the package's globals. The task borrows its spawn
 * context, which must remain live through its final drawing tick.
 *
 * Include this header in the prologue and screen_wave.inc.c or
 * screen_wave_grid.inc.c at the task's position; screen_wave_run.inc.c, the
 * event callback that starts and steers the wave, goes at its own position.
 * The task's storage belongs to the package, which declares and defines it
 * at its own positions under these
 * names - declaring it here would move it, since bss is laid out in
 * first-declaration order:
 *
 *   s32                   _gScreenWaveAmplitude      current strength (32 per peak pixel)
 *   ScreenWaveCtx*        SCREEN_WAVE_ACTIVE_CONTEXT borrowed active ramp context
 *   ScreenWaveOscillator  SCREEN_WAVE_COLUMN_OSCILLATORS[] per-column phase, offset and speed
 *   ScreenWaveOscillator  SCREEN_WAVE_ROW_OSCILLATORS[]    per-row phase, offset and speed
 *   POLY_FT4              gScreenWaveGrid[2][30][8]  the prebuilt grids (grid task)
 *   ScreenWaveCtx         SCREEN_WAVE_SPAWN_CONTEXT the context _screenWaveRun fills
 *   TaskDesc              gScreenWaveTaskDesc[]      the wave task _screenWaveRun spawns
 *
 * The grid task's records are the eight-byte ScreenWaveGridOscillator rather
 * than ScreenWaveOscillator.
 * `SCREEN_WAVE_GRID` is the quad array the grid task indexes, as
 * `[buffer][row][column]`: two frame buffers, 30 rows and 8 columns of 40 by 8
 * quads. It defaults to `gScreenWaveGrid`. A package whose object is larger
 * than that array defines `SCREEN_WAVE_GRID` as the array before including this
 * header. `shelter_b6_corridor` is the one that does: four unread bytes follow
 * its quads, filling the gap before the next object, which starts on an
 * eight-byte boundary. The task takes the address of row 1 and then steps a
 * row index from -1 through 28, so every one of the 30 rows is covered.
 */

#ifndef SRC_SHARED_SCREEN_WAVE_H
#define SRC_SHARED_SCREEN_WAVE_H

#include "main/task_types.h"

#include "overlay.h"

/// Persistent ScreenWaveCtx lvalue seeded and steered by `_screenWaveRun`.
///
/// Defaults to `gScreenWaveSpawnCtx`, shared between the tunnel's translation
/// units. The reservoir binds its TU-local `_gScreenWaveSpawnCtx` before this
/// header. Declare storage in the carrier at its existing BSS position; the
/// wave task borrows its address through the final draw, so overlapping spawns
/// are forbidden. The replacement is a side-effect-free, addressable lvalue,
/// used once for each field store and once when taking its spawn address.
#ifndef SCREEN_WAVE_SPAWN_CONTEXT
#define SCREEN_WAVE_SPAWN_CONTEXT gScreenWaveSpawnCtx
#endif

/// Quad array `_screenWaveGridTask` indexes, `[buffer][row][column]`.
///
/// Defaults to `gScreenWaveGrid` when that object is the array. A package
/// whose object continues past the array defines this as the array member
/// before including the header. The replacement is used as
/// `&SCREEN_WAVE_GRID[buffer][row]`, so it must be the array itself.
#ifndef SCREEN_WAVE_GRID
#define SCREEN_WAVE_GRID gScreenWaveGrid
#endif

/// Borrowed ScreenWaveCtx* lvalue holding the renderer's active task context.
///
/// Defaults to TU-local `_gScreenWaveContext`. Split overlays bind their
/// externally linked `gScreenWaveContext` before this header. The replacement
/// must be side-effect-free; the task repeatedly reads and writes its fields.
#ifndef SCREEN_WAVE_ACTIVE_CONTEXT
#define SCREEN_WAVE_ACTIVE_CONTEXT _gScreenWaveContext
#endif

/// Addressable column-oscillator array used by the selected wave renderer.
///
/// Defaults to TU-local `_gScreenWaveColumnWaves`; split overlays bind
/// `gScreenWaveColumnWaves`. Use ScreenWaveOscillator with at least 11 entries
/// for the per-frame task, or ScreenWaveGridOscillator with at least 9 for the
/// prebuilt-grid task. Expansion is indexed repeatedly and must have no side effects.
#ifndef SCREEN_WAVE_COLUMN_OSCILLATORS
#define SCREEN_WAVE_COLUMN_OSCILLATORS _gScreenWaveColumnWaves
#endif

/// Addressable row-oscillator array containing at least 30 active entries.
///
/// Defaults to TU-local `_gScreenWaveRowWaves`; split overlays bind
/// `gScreenWaveRowWaves`. Its element type must match the column array's type.
/// Expansion is indexed repeatedly and must have no side effects.
#ifndef SCREEN_WAVE_ROW_OSCILLATORS
#define SCREEN_WAVE_ROW_OSCILLATORS _gScreenWaveRowWaves
#endif

static void _screenWaveTask(Task* task);
static void _screenWaveGridTask(Task* task);

static void _screenWaveRun(s32 request);

#endif /* SRC_SHARED_SCREEN_WAVE_H */
