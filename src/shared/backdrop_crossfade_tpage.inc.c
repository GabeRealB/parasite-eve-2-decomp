/* Part of the backdrop crossfade library; see backdrop_crossfade.h. */

/// Queues the direct-colour texture page and additive blend mode for a crossfade sprite.
///
/// `vramX` and `vramY` are pixel coordinates within 1024x512 VRAM, rounded
/// down to a 64-column, 256-row page origin. Enables drawing into the display
/// area and disables dithering. Insert the sprite into the crossfade OT tag
/// before calling: head insertion makes this command execute before it.
/// Requires room for one `DR_TPAGE` in the current frame's packet arena;
/// the GPU borrows that packet until the frame completes.
static void _crossfadeSetTpage(s32 vramX, s16 vramY)
{
    enum {
        CROSSFADE_TEXTURE_DEPTH_DIRECT = 2, // RGB555 with a semitransparency bit
        CROSSFADE_TEXTURE_PAGE_X_MASK  = 0x3C0,
    };
    DR_TPAGE* pageCommand;
    s32       vramRow;

    // Promote the row once before the SDK macro's repeated bit operations.
    vramRow        = vramY;
    pageCommand    = gGpuPrimCursor;
    gGpuPrimCursor = pageCommand + 1;
    setDrawTPage(pageCommand, true, false, getTPage(CROSSFADE_TEXTURE_DEPTH_DIRECT, GPU_BLEND_ADD, vramX & CROSSFADE_TEXTURE_PAGE_X_MASK, vramRow));
    addPrim(gGpuCurrentOt + CROSSFADE_ORDERING_TABLE_SLOT, pageCommand);
}
