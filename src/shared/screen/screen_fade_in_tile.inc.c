/* Part of the screen fade library; see screen_fade.h. */

#include "screen_fade_step_down.inc.c"
#include "screen_fade_tile_overlay.inc.c"

void screenFadeInTileTask(Task* task)
{
    enum {
        SCREEN_FADE_IN_TILE_STATE_INITIALIZE = 0,
        SCREEN_FADE_IN_TILE_STATE_RAMP       = 1,
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
            fade->b      = SCREEN_FADE_FULL_INTENSITY;
            fade->g      = SCREEN_FADE_FULL_INTENSITY;
            fade->r      = SCREEN_FADE_FULL_INTENSITY;
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
