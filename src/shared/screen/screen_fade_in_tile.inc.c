/* Part of the screen fade library; see screen_fade.h. */

#include "screen_fade_step_down.inc.c"

/// Queues a 320x240 subtractive fade tile at the current draw origin.
///
/// `fade` supplies readable signed channels; their low red/green/red bytes
/// become the tile colour, while blue and the ramp values are left untouched.
/// Covers x = -160..159 and y = -120..119 before the draw origin is added,
/// including any screen shake applied there.
///
/// Requires a word-aligned `gGpuPrimCursor` with
/// `sizeof(TILE) + sizeof(DR_TPAGE)` writable bytes and tag -16 in
/// `gGpuCurrentOt`. Reserves both packets without checking capacity; their
/// storage must remain live until GPU drawing completes. The draw mode runs
/// before the tile and remains active afterwards: subtractive blending,
/// dithering enabled, displayed-area drawing disabled, and a 4-bit texture
/// page at (0, 0). The untextured tile does not sample that page.
static inline void _screenFadeDrawTileOverlay(const ScreenFadeWork* fade)
{
    enum {
        SCREEN_FADE_TILE_WIDTH_PIXELS       = 320,
        SCREEN_FADE_TILE_HEIGHT_PIXELS      = 240,
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
    tile->x0 = -SCREEN_FADE_TILE_WIDTH_PIXELS / 2;
    tile->y0 = -SCREEN_FADE_TILE_HEIGHT_PIXELS / 2;
    tile->w  = SCREEN_FADE_TILE_WIDTH_PIXELS;
    tile->h  = SCREEN_FADE_TILE_HEIGHT_PIXELS;
    addPrim(gGpuCurrentOt + SCREEN_FADE_TILE_FOREGROUND_TAG, tile);

    // Insertion prepends: queue the mode last so the GPU applies it to the tile.
    drawMode       = gGpuPrimCursor;
    gGpuPrimCursor = drawMode + 1;
    setDrawTPage(drawMode, false, true, getTPage(SCREEN_FADE_TILE_TEXTURE_DEPTH_4BIT, GPU_BLEND_SUBTRACT, 0, 0));
    addPrim(gGpuCurrentOt + SCREEN_FADE_TILE_FOREGROUND_TAG, drawMode);
}

void screenFadeInTileTask(Task* task)
{
    enum {
        SCREEN_FADE_IN_TILE_STATE_INITIALIZE = 0,
        SCREEN_FADE_IN_TILE_STATE_RAMP       = 1,
        SCREEN_FADE_IN_TILE_MAX_INTENSITY    = 255,
    };
    ScreenFadeWork* fade;
    ScreenFadeWork* allocatedFade;

    fade = task->work;
    switch (task->state) {
        case SCREEN_FADE_IN_TILE_STATE_INITIALIZE:
            allocatedFade = memMalloc(sizeof(*allocatedFade), false);
            task->work    = allocatedFade;
            if (allocatedFade == NULL) {
                taskKill(task);
                break;
            }
            fade         = allocatedFade;
            fade->b      = SCREEN_FADE_IN_TILE_MAX_INTENSITY;
            fade->g      = SCREEN_FADE_IN_TILE_MAX_INTENSITY;
            fade->r      = SCREEN_FADE_IN_TILE_MAX_INTENSITY;
            task->state += 1;
            // Draw immediately so initialization does not expose an unfaded frame.
            /* fallthrough */
        case SCREEN_FADE_IN_TILE_STATE_RAMP:
            _screenFadeDrawTileOverlay(fade);
            _screenFadeStepDown(fade, task);
            if (fade->r < 0) {
                taskKill(task);
            }
            break;
    }
}
