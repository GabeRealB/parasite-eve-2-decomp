/* Part of the screen fade library; see screen_fade.h. */

#include "screen_fade_step_down.inc.c"

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
            task->state += SCREEN_FADE_IN_STATE_RAMP - SCREEN_FADE_IN_STATE_INITIALIZE;
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
