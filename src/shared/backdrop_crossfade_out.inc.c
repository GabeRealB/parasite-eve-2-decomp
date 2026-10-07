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
