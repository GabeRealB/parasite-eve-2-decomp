/* Part of the backdrop crossfade library; see backdrop_crossfade.h. */

/// Queues an opaque redraw of the current draw buffer for a backdrop crossfade.
///
/// Requires the centered 320x240 framebuffer layout, with buffer 0 at VRAM
/// (0, 0) and buffer 1 at (0, 272), selected by `gDisplayState.drawBuffer`.
/// `shade` modulates all RGB channels: 0 is black, 0x80 is unchanged;
/// callers use 0..0x80, and wider values retain only their low byte.
/// Reserves two `SPRT`s and two `DR_TPAGE`s (56 bytes) in the current frame's
/// arena and links them into the crossfade OT tag. The source frame must
/// already be drawn when these packets execute; the GPU borrows the packet
/// storage until the frame finishes. No capacity or layout checks are made.
static void _crossfadeDrawLive(s32 shade)
{
    enum {
        CROSSFADE_LEFT_STRIP_WIDTH_PIXELS = 192,
        CROSSFADE_LOWER_BUFFER_PAGE_Y     = 256,
        CROSSFADE_LOWER_BUFFER_TEXTURE_V  = 16,
    };
    SPRT* sprite;
    s16   pageOriginY;
    u8    textureU;
    u8    textureV;

    /// Reserves one opaque live-frame sprite with equal RGB modulation bytes.
    ///
    /// Requires a disjoint writable `SPRT*` lvalue and a stable integer shade;
    /// both expressions are evaluated repeatedly. Shade stores retain the low
    /// byte (0 black, 128 neutral). Captures `gGpuPrimCursor`, advancing it by
    /// sizeof(SPRT); the frame arena must have that much space through GPU use.
    /// Geometry, texture and OT linkage are supplied afterwards. One statement.
#define CROSSFADE_ALLOCATE_LIVE_SPRITE(spritePacket, shadeValue) \
    (void)({                                                     \
        (spritePacket) = gGpuPrimCursor;                         \
        gGpuPrimCursor = (spritePacket) + 1;                     \
        setSprt(spritePacket);                                   \
        (spritePacket)->r0 = (shadeValue);                       \
        (spritePacket)->g0 = (shadeValue);                       \
        (spritePacket)->b0 = (shadeValue);                       \
    })

    // The lower framebuffer starts sixteen rows into its texture page.
    if (gDisplayState.drawBuffer == 0) {
        pageOriginY = 0;
        textureU    = 0;
        textureV    = 0;
    } else {
        pageOriginY = CROSSFADE_LOWER_BUFFER_PAGE_Y;
        textureU    = 0;
        textureV    = CROSSFADE_LOWER_BUFFER_TEXTURE_V;
    }

    // Split at a 64-column page boundary to keep both strips within 256 texels.
    CROSSFADE_ALLOCATE_LIVE_SPRITE(sprite, shade);
    sprite->u0   = textureU;
    sprite->v0   = textureV;
    sprite->x0   = -DISPLAY_SCREEN_WIDTH / 2;
    sprite->y0   = -DISPLAY_SCREEN_HEIGHT / 2;
    sprite->clut = 0;
    sprite->w    = CROSSFADE_LEFT_STRIP_WIDTH_PIXELS;
    sprite->h    = DISPLAY_SCREEN_HEIGHT;
    addPrim(gGpuCurrentOt + CROSSFADE_ORDERING_TABLE_SLOT, sprite);
    _crossfadeSetTpage(0, pageOriginY);

    CROSSFADE_ALLOCATE_LIVE_SPRITE(sprite, shade);
    sprite->x0   = CROSSFADE_LEFT_STRIP_WIDTH_PIXELS - DISPLAY_SCREEN_WIDTH / 2;
    sprite->u0   = textureU;
    sprite->v0   = textureV;
    sprite->y0   = -DISPLAY_SCREEN_HEIGHT / 2;
    sprite->clut = 0;
    sprite->w    = DISPLAY_SCREEN_WIDTH - CROSSFADE_LEFT_STRIP_WIDTH_PIXELS;
    sprite->h    = DISPLAY_SCREEN_HEIGHT;
    addPrim(gGpuCurrentOt + CROSSFADE_ORDERING_TABLE_SLOT, sprite);
    _crossfadeSetTpage(CROSSFADE_LEFT_STRIP_WIDTH_PIXELS, pageOriginY);
#undef CROSSFADE_ALLOCATE_LIVE_SPRITE
}
