/* Part of the screen fade library; see screen_fade.h. */

#include "screen_fade_step_down.inc.c"

/// Reveals the screen by reducing a subtractive full-screen overlay each update.
///
/// Start at state 0 with no owned work. The low 16 bits of `spawnArg1` are
/// intensity units removed per update. A zero rate holds the overlay
/// indefinitely. The task owns its primary-heap `ScreenFadeWork` until task
/// teardown; allocation failure kills it.
/// All channels start at 255 and are stored as signed 16-bit values, but the
/// overlay uses the low bytes of red/green/red. The initializing update also
/// draws and steps the ramp. Each subtraction narrows back to signed 16 bits;
/// the task ends when the stored red value is negative.
void SCREEN_FADE_IN_TASK(Task* task)
{
    enum {
        SCREEN_FADE_IN_STATE_INITIALIZE = 0,
        SCREEN_FADE_IN_STATE_RAMP       = 1,
        SCREEN_FADE_IN_MAX_INTENSITY    = 255,
    };
    ScreenFadeWork* fade;
    ScreenFadeWork* allocatedFade;

    fade = task->work;
    switch (task->state) {
        case SCREEN_FADE_IN_STATE_INITIALIZE:
            allocatedFade = memMalloc(sizeof(*allocatedFade), false);
            task->work    = allocatedFade;
            if (allocatedFade == NULL) {
                taskKill(task);
                return;
            }
            fade         = allocatedFade;
            fade->b      = SCREEN_FADE_IN_MAX_INTENSITY;
            fade->g      = SCREEN_FADE_IN_MAX_INTENSITY;
            fade->r      = SCREEN_FADE_IN_MAX_INTENSITY;
            task->state += 1;
            // Draw immediately so initialization does not expose an unfaded frame.
            /* fallthrough */
        case SCREEN_FADE_IN_STATE_RAMP:
            fadeDrawOverlay(fade->r, fade->g, fade->r, GPU_BLEND_SUBTRACT);
            _screenFadeStepDown(fade, task);
            if (fade->r >= 0) {
                return;
            }
            taskKill(task);
            break;
    }
}
