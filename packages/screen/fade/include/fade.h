/* Full-screen fade tasks that darken or reveal the screen through a subtractive
 * overlay. Include this header in the prologue and each fragment at its
 * function's position; a package carrying out.inc.c declares its static
 * callback in its own prologue.
 */

#ifndef SCREEN_FADE_FADE_H
#define SCREEN_FADE_FADE_H

#include <engine/display.h>
#include <engine/task_types.h>
#include <psyq/sys/types.h>
#include <psyq/libgpu.h>

/// A fade task's overlay color, stepped each frame until it leaves 0..255; the screen fades draw `r` as blue too.
typedef struct {
    byte field_0[0x2]; // Unused
    s16  r;            // Signed, so a ramp down ends below zero
    s16  g;
    s16  b;
} ScreenFadeWork;

/// A fade in starts at full intensity; a fade out is complete once its red channel passes it.
enum {
    SCREEN_FADE_FULL_INTENSITY = 255,
    SCREEN_FADE_OUT_COMPLETE   = 256,
};

#ifndef SCREEN_FADE_IN_TASK
/// Name of the fade-in task in.inc.c defines; defaults to `screenFadeInTask`. For a static one, declare its
/// prototype and define this before including the header. in.inc.c needs `engine/display_types.h`, `engine/gameflow.h`,
/// `engine/mem.h` and `engine/task.h` included first.
#define SCREEN_FADE_IN_TASK screenFadeInTask
#endif

/// Reveals the screen from black, lightening a subtractive overlay by the low half of `spawnArg1` each frame.
void SCREEN_FADE_IN_TASK(Task* task);

/// `SCREEN_FADE_IN_TASK` drawing its own tile at the current draw origin instead of through `fadeDrawOverlay`.
void screenFadeInTileTask(Task* task);

/// Subtracts the task's rate, the low half of `spawnArg1`, from all three fade channels.
static inline void _screenFadeStepDown(ScreenFadeWork* fade, const Task* task)
{
    fade->r -= task->spawnArg1.halves.low;
    fade->g -= task->spawnArg1.halves.low;
    fade->b -= task->spawnArg1.halves.low;
}

/// Adds the task's rate, the low half of `spawnArg1`, to all three fade channels.
static inline void _screenFadeStepUp(ScreenFadeWork* fade, const Task* task)
{
    fade->r += task->spawnArg1.halves.low;
    fade->g += task->spawnArg1.halves.low;
    fade->b += task->spawnArg1.halves.low;
}

/// Queues a full-screen subtractive tile in the fade's color at the current draw origin, in front of the scene.
static inline void _screenFadeDrawTileOverlay(const ScreenFadeWork* fade)
{
    enum {
        SCREEN_FADE_TILE_FOREGROUND_TAG     = -16,
        SCREEN_FADE_TILE_TEXTURE_DEPTH_4BIT = 0,
    };
    u8        red;
    u8        green;
    TILE*     tile;
    DR_TPAGE* drawMode;

    red            = fade->r;
    green          = fade->g;
    tile           = gGpuPrimCursor;
    gGpuPrimCursor = tile + 1;
    setTile(tile);
    setSemiTrans(tile, true);
    tile->r0 = red;
    tile->g0 = green;
    tile->b0 = red;
    tile->x0 = -DISPLAY_SCREEN_WIDTH / 2;
    tile->y0 = -DISPLAY_SCREEN_HEIGHT / 2;
    tile->w  = DISPLAY_SCREEN_WIDTH;
    tile->h  = DISPLAY_SCREEN_HEIGHT;
    addPrim(gGpuCurrentOt + SCREEN_FADE_TILE_FOREGROUND_TAG, tile);

    // Insertion prepends: queue the mode last so the GPU applies it to the tile.
    drawMode       = gGpuPrimCursor;
    gGpuPrimCursor = drawMode + 1;
    setDrawTPage(drawMode, false, true, getTPage(SCREEN_FADE_TILE_TEXTURE_DEPTH_4BIT, GPU_BLEND_SUBTRACT, 0, 0));
    addPrim(gGpuCurrentOt + SCREEN_FADE_TILE_FOREGROUND_TAG, drawMode);
}

#endif /* SCREEN_FADE_FADE_H */
