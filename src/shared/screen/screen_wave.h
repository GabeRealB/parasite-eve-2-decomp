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
 *   static TaskDesc       _gScreenWaveTaskDescs[2]   slot 0 wave recipe, then TASK_DESC_END
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

#include "main/display.h"
#include "main/task_types.h"

#include "overlay.h"

/// Writable ramp and tint context used for scripted wave spawns and later phase requests.
///
/// Bind a parenthesized, side-effect-free, addressable `ScreenWaveCtx` lvalue
/// before including this header and retain it through screen_wave_run.inc.c.
/// Every expansion must select the same object, including later event requests.
/// The default names `gScreenWaveSpawnCtx`, defined in the submarine tunnel's
/// second translation unit and declared in its private header. The reservoir
/// selects its static `_gScreenWaveSpawnCtx`. Storage declarations stay in the
/// carrier or its private header at their existing BSS positions.
///
/// A non-positive request writes span, peak strength and RGB modulation, then
/// passes the object's address as spawn argument 2; it does not copy the context.
/// The wave's seed tick resets frame and phase. Positive requests write the
/// low signed-halfword phase of this same context, so send them after that tick.
/// Expansion occurs once per field store and once for the spawn address. The
/// object must remain writable through the final drawing tick, including the
/// tick that kills the task; only one wave may borrow it at a time. This binding
/// is needed only by carriers of the event-request fragment.
#ifndef SCREEN_WAVE_SPAWN_CONTEXT
#define SCREEN_WAVE_SPAWN_CONTEXT (gScreenWaveSpawnCtx)
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

/// Writable pointer slot for the screen-wave renderer's borrowed ramp and tint context.
///
/// Bind a side-effect-free, assignable `ScreenWaveCtx*` lvalue before including
/// this header and keep the binding through the renderer fragment. The default
/// names the carrier's static `_gScreenWaveContext`; actor_205200, the submarine
/// tunnel and the garbage incinerator bind overlay-private `gScreenWaveContext`
/// storage defined in another translation unit. Declare the slot in the carrier
/// or its private header at the existing BSS position.
///
/// The seed tick replaces the slot with `task->spawnArg2.pointer`, resets its
/// frame and starts a rising ramp. The borrowed context must be non-null, have
/// positive `span` and remain writable through the final drawing tick, including
/// the tick that kills the task. Only one wave may use a package's slot at a time.
/// Teardown leaves the pointer in place; its value does not indicate a live task.
/// The grid renderer fixes tint at initialization; the per-frame renderer reads
/// tint on each draw. Expansion is repeated for pointer assignment and field
/// access, so a replacement expression must be parenthesized and have no side effects.
#ifndef SCREEN_WAVE_ACTIVE_CONTEXT
#define SCREEN_WAVE_ACTIVE_CONTEXT (_gScreenWaveContext)
#endif

/// Writable sine-wave array controlling horizontal displacement at vertical mesh edges.
///
/// Bind a side-effect-free, parenthesized array lvalue before including this
/// header and retain it through the selected renderer fragment. The default
/// names the carrier's static `_gScreenWaveColumnWaves`; actor_205200, the
/// submarine tunnel and the garbage incinerator bind overlay-private
/// `gScreenWaveColumnWaves` defined in another translation unit. Declare the
/// storage in the carrier or its private header at the existing BSS position.
///
/// `_screenWaveTask` requires `ScreenWaveOscillator` elements 0..10 for its
/// ten quad columns. `_screenWaveGridTask` requires word-aligned
/// `ScreenWaveGridOscillator` elements 0..8 for its eight quad columns; it
/// snapshots only each record's first four bytes (`phase` and `offset`).
/// Extra declared elements are outside the active mesh, not additional waves.
///
/// The seed tick initializes each active record; drawing ticks add `speed`
/// to its signed-halfword `phase`, with 4096 angle units per turn. The grid
/// renderer pauses this advance while actors are frozen; the per-frame
/// renderer always advances. Expansion is repeated for reads, writes and
/// address-taking, so it must have no side effects. Storage must remain
/// writable through the final drawing tick, with only one wave using it at a time.
#ifndef SCREEN_WAVE_COLUMN_OSCILLATORS
#define SCREEN_WAVE_COLUMN_OSCILLATORS (_gScreenWaveColumnWaves)
#endif

/// Writable sine-wave array controlling vertical displacement along horizontal mesh edges.
///
/// Bind a side-effect-free, parenthesized array lvalue before including this
/// header and retain it through the selected renderer fragment. The default
/// names the carrier's static `_gScreenWaveRowWaves`; actor_205200, the
/// submarine tunnel and the garbage incinerator bind overlay-private
/// `gScreenWaveRowWaves` defined in another translation unit. Declare storage
/// in the carrier or its private header at the existing BSS position.
///
/// Both renderers require elements 0..29, one for each horizontal edge below
/// the pinned top edge; no oscillator is indexed at -1. `_screenWaveTask`
/// requires `ScreenWaveOscillator` elements. `_screenWaveGridTask` requires
/// word-aligned `ScreenWaveGridOscillator` elements and snapshots only their
/// first four bytes (`phase` and `offset`), leaving speed and the unknown tail
/// uncopied. Extra declared elements are outside the active mesh.
///
/// The seed tick initializes each active phase, offset and speed. Drawing
/// ticks add speed to the signed-halfword phase, with 4096 angle units per
/// turn; the grid renderer pauses this advance while actors are frozen, and
/// the per-frame renderer always advances. Expansion is repeated for reads,
/// writes and address-taking, so it must have no side effects. Storage must
/// remain writable through the final drawing tick, with only one wave using
/// it at a time.
#ifndef SCREEN_WAVE_ROW_OSCILLATORS
#define SCREEN_WAVE_ROW_OSCILLATORS (_gScreenWaveRowWaves)
#endif

static void _screenWaveTask(Task* task);
static void _screenWaveGridTask(Task* task);

static void _screenWaveRun(s32 request);

#endif /* SRC_SHARED_SCREEN_WAVE_H */
