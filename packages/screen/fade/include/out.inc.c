/* Part of the screen fade library; see fade.h. */

#include "fade.h"

/// Fades the screen to black, darkening a subtractive overlay by the low half of `spawnArg1` each frame.
static void _screenFadeOutTask(Task* task)
{
    enum {
        SCREEN_FADE_OUT_STATE_INITIALIZE = 0,
        SCREEN_FADE_OUT_STATE_RAMP       = 1,
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
            if (fade->r >= SCREEN_FADE_OUT_COMPLETE) {
                taskKill(task);
            }
            break;
    }
}
