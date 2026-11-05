/* The screen-wave effect: a task that redraws the rendered frame as a grid of
 * textured quads whose corners sine waves push around, for rooms and actors
 * that shake the view. _screenWaveTask builds a 10x30 grid every frame;
 * _screenWaveGridTask keeps a prebuilt, double-buffered 8x30 grid of 40x8
 * quads and only moves their corners. One wave runs per package at a time.
 *
 * Include this header in the prologue, wave.inc.c or grid.inc.c at the task's
 * position, and run.inc.c, the event callback that starts and steers the wave,
 * at its own. The package defines the task's storage at its own positions
 * (bss is laid out in first-declaration order), under these names:
 *
 *   s32                   _gScreenWaveAmplitude      current strength (32 per peak pixel)
 *   ScreenWaveCtx*        SCREEN_WAVE_ACTIVE_CONTEXT the running wave's context
 *   ScreenWaveOscillator  SCREEN_WAVE_COLUMN_OSCILLATORS[] per-column phase, offset and speed
 *   ScreenWaveOscillator  SCREEN_WAVE_ROW_OSCILLATORS[]    per-row phase, offset and speed
 *   POLY_FT4              gScreenWaveGrid[2][30][8]  the prebuilt grids (grid task)
 *   ScreenWaveCtx         SCREEN_WAVE_SPAWN_CONTEXT the context _screenWaveRun fills
 *   static TaskDesc       _gScreenWaveTaskDescs[2]   slot 0 wave recipe, then TASK_DESC_END
 *
 * The grid task's oscillators are ScreenWaveGridOscillator. shelter_b6_corridor
 * binds SCREEN_WAVE_GRID because four unread bytes follow its quads.
 */

#ifndef SCREEN_WAVE_WAVE_H
#define SCREEN_WAVE_WAVE_H

#include <engine/display.h>
#include <engine/task_types.h>

/// How a screen-wave quad treats the captured frame (`ScreenWaveCtx.modulateTexture`).
enum {
    /// Copy the frame's own colors.
    SCREEN_WAVE_TEXTURE_RAW = 0,
    /// Multiply the frame by the context's color.
    SCREEN_WAVE_MODULATE_TEXTURE = 1,
};

/// Phase of a screen-wave ramp (`ScreenWaveCtx.state`); the task starts it rising.
enum {
    /// Count `frame` up to `span` and hold it there.
    SCREEN_WAVE_RAMP_RISING = 0,
    /// Count `frame` down to zero, then finish.
    SCREEN_WAVE_RAMP_FALLING = 1,
    /// End the screen-wave task.
    SCREEN_WAVE_RAMP_FINISHED = 2,
};

/// A screen wave's ramp and tint, handed to the task as its spawn argument; strength is `frame * scale / span`.
typedef struct {
    s16 span;            // Rise length in frames
    s16 scale;           // Peak strength, 32 per pixel of displacement
    s16 state;           // Ramp phase (SCREEN_WAVE_RAMP_*)
    s16 frame;           // Position along the ramp, in frames
    u8  modulateTexture; // Nonzero to tint the frame with r, g, b
    u8  r;
    u8  g;
    u8  b;
} ScreenWaveCtx;

/// One row's or column's sine wave in `_screenWaveGridTask`'s mesh; angles are 4096 to a turn.
typedef struct {
    s16 phase;   // Advanced by `speed` each frame while actors run
    s16 offset;  // Random phase offset, set when the task starts
    s16 speed;   // Phase step per frame, 20 to 119
    s16 field_6; // Unused by the grid task
} ScreenWaveGridOscillator;

/// One grid line's sine wave in `_screenWaveTask`'s mesh: a column's pushes x, a row's y; 4096 to a turn.
typedef struct {
    s16 phase;  // Advanced by `speed` each frame
    s16 offset; // Random phase offset, set when the task starts
    s16 speed;  // Phase step per frame, 0 to 99
} ScreenWaveOscillator;

/* The package binds the wave's storage by defining these before including this header. */

/// The context `_screenWaveRun` fills and spawns the wave with; defaults to `gScreenWaveSpawnCtx`.
#ifndef SCREEN_WAVE_SPAWN_CONTEXT
#define SCREEN_WAVE_SPAWN_CONTEXT (gScreenWaveSpawnCtx)
#endif

/// The grid task's quads, `[buffer][row][column]`; defaults to `gScreenWaveGrid`, which must be the array itself.
#ifndef SCREEN_WAVE_GRID
#define SCREEN_WAVE_GRID gScreenWaveGrid
#endif

/// The running wave's context, taken from its spawn argument; defaults to `_gScreenWaveContext`.
#ifndef SCREEN_WAVE_ACTIVE_CONTEXT
#define SCREEN_WAVE_ACTIVE_CONTEXT (_gScreenWaveContext)
#endif

/// The column waves, 11 for `_screenWaveTask` and 9 for the grid task; defaults to `_gScreenWaveColumnWaves`.
#ifndef SCREEN_WAVE_COLUMN_OSCILLATORS
#define SCREEN_WAVE_COLUMN_OSCILLATORS (_gScreenWaveColumnWaves)
#endif

/// The 30 row waves; defaults to `_gScreenWaveRowWaves`.
#ifndef SCREEN_WAVE_ROW_OSCILLATORS
#define SCREEN_WAVE_ROW_OSCILLATORS (_gScreenWaveRowWaves)
#endif

static void _screenWaveTask(Task* task);
static void _screenWaveGridTask(Task* task);

static void _screenWaveRun(s32 request);

#endif /* SCREEN_WAVE_WAVE_H */
