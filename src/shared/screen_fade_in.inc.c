/* Part of the screen fade library; see screen_fade.h. */

/// Fade from black, entry 1 of the actor's task table.
///
/// State 0 allocates the channel block and seeds all three channels at 0xFF;
/// a failed allocation kills the task. State 1 runs every frame: it draws a
/// subtractive `fadeDrawOverlay` tinted `r`/`g`/`r` (`b` is stepped but never
/// drawn), then lowers all three channels by `Task::spawnArg1`, the fade rate.
/// Once `r` has gone negative the screen is clear and the task kills itself.
void screenFadeInTask(Task* arg0)
{
    ScreenFadeWork* fade;
    ScreenFadeWork* alloc;

    fade = arg0->work;
    switch (arg0->state) {
        case 0:
            alloc      = memMalloc(sizeof(*alloc), false);
            arg0->work = alloc;
            if (alloc == NULL) {
                taskKill(arg0);
                return;
            }
            fade         = alloc;
            fade->b      = 0xFF;
            fade->g      = 0xFF;
            fade->r      = 0xFF;
            arg0->state += 1;
            /* fallthrough */
        case 1:
            fadeDrawOverlay(fade->r, fade->g, fade->r, GPU_BLEND_SUBTRACT);
            fade->r -= (u16)arg0->spawnArg1.value;
            fade->g -= (u16)arg0->spawnArg1.value;
            fade->b -= (u16)arg0->spawnArg1.value;
            if (fade->r >= 0) {
                return;
            }
            taskKill(arg0);
            break;
    }
}
