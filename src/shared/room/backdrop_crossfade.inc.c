/* Part of the backdrop crossfade library; see backdrop_crossfade.h. */

/// Fades the saved backdrop into the live frame by eight modulation levels per callback.
///
/// `task` is the live backdrop task; its callback-owned `killCountdown` holds
/// the saved image's RGB shade, in 0..`CROSSFADE_SHADE_UNITY` (128 unchanged).
/// The preceding state initializes it to 128. Subtraction wraps to 16 bits
/// before a signed clamp at zero; reaching zero advances `state` and still
/// queues that callback's fully live frame. Dispatch must then leave this state.
///
/// Requires a completed backdrop capture and the live frame rendered before
/// the crossfade ordering tag executes. Queues four `SPRT`s and four `DR_TPAGE`s
/// (112 bytes) in the current frame's packet arena, borrowed by the GPU until
/// frame completion. The carrier supplies the saved image's VRAM layout.
static void _crossfadeOutState(Task* task)
{
    enum {
        CROSSFADE_OUT_SHADE_STEP = 8,
    };
    u16 nextBackdropShade;

    nextBackdropShade   = (u16)task->killCountdown - CROSSFADE_OUT_SHADE_STEP;
    task->killCountdown = nextBackdropShade;
    if ((s16)nextBackdropShade <= 0) {
        task->killCountdown = 0;
        task->state++;
    }

    // Head insertion draws the opaque live frame before the additive saved image.
    _crossfadeDrawBackdrop(task->killCountdown);
    _crossfadeDrawLive(CROSSFADE_SHADE_UNITY - task->killCountdown);
}

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
