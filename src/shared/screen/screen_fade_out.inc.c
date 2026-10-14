/* Part of the screen fade library; see screen_fade.h. */

/// Advances a fade-out ramp by the task's unsigned 16-bit intensity increment.
///
/// Requires writable `fade` and readable `task` storage. The low halfword of
/// `task->spawnArg1` is 0..65535 intensity units per update; the high halfword
/// is ignored. Red, green and blue are updated in that order, each reading the
/// rate separately. Additions promote to int, then narrow to signed 16 bits
/// without clamping: values can wrap negative, and zero preserves them.
/// The caller draws before stepping and tests the stored red value afterwards;
/// this helper neither draws nor ends the task and leaves other storage intact.
static inline void _screenFadeStepUp(ScreenFadeWork* fade, const Task* task)
{
    fade->r += task->spawnArg1.halves.low;
    fade->g += task->spawnArg1.halves.low;
    fade->b += task->spawnArg1.halves.low;
}

/// Darkens the screen by increasing a subtractive full-screen overlay each update.
///
/// Start at state 0 with no owned work. The unsigned low 16 bits of `spawnArg1`
/// supply intensity units added per update; zero holds the task indefinitely.
/// The task owns its primary-heap `ScreenFadeWork` until teardown; allocation
/// failure kills it. State 1 draws before advancing the ramp; initialization
/// seeds all channels to zero and also draws and steps in that update.
/// All channels narrow to signed 16 bits without clamping, while drawing uses
/// their low red/green/red bytes. Completion tests stored red >= 256, so rates
/// large enough to wrap the signed counter need not finish on that update.
/// Drawing requires the frame arena and foreground OT tag used by `fadeDrawOverlay`.
static void _screenFadeOutTask(Task* task)
{
    enum {
        SCREEN_FADE_OUT_STATE_INITIALIZE = 0,
        SCREEN_FADE_OUT_STATE_RAMP       = 1,
        SCREEN_FADE_OUT_END_INTENSITY    = 256,
    };
    ScreenFadeWork* fade;
    ScreenFadeWork* allocatedFade;

    fade = task->work;
    switch (task->state) {
        case SCREEN_FADE_OUT_STATE_INITIALIZE:
            allocatedFade = memMalloc(sizeof(*allocatedFade), false);
            task->work    = allocatedFade;
            if (allocatedFade == NULL) {
                taskKill(task);
                return;
            }
            fade         = allocatedFade;
            fade->b      = 0;
            fade->g      = 0;
            fade->r      = 0;
            task->state += 1;
            // Queue the initial zero overlay and advance the ramp in the same update.
            /* fallthrough */
        case SCREEN_FADE_OUT_STATE_RAMP:
            fadeDrawOverlay(fade->r, fade->g, fade->r, GPU_BLEND_SUBTRACT);
            _screenFadeStepUp(fade, task);
            if (fade->r >= SCREEN_FADE_OUT_END_INTENSITY) {
                taskKill(task);
            }
            break;
    }
}
