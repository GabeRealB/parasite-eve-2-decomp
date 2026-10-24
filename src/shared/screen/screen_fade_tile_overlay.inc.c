/* Shared fade overlay; include after overlay.h, main/display_types.h and main/gfx.h. */
#ifndef SRC_SHARED_SCREEN_FADE_TILE_OVERLAY_INC_C
#define SRC_SHARED_SCREEN_FADE_TILE_OVERLAY_INC_C

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

#endif /* SRC_SHARED_SCREEN_FADE_TILE_OVERLAY_INC_C */
