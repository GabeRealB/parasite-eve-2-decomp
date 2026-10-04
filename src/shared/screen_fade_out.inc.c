/* Part of the screen fade library; see screen_fade.h. */

/// Fade task, entry 4 of the actor's task table: darkens the screen to black.
///
/// State 0 allocates the channel block and clears it; a failed allocation
/// kills the task. State 1 runs every frame: it draws a subtractive
/// `fadeDrawOverlay` tinted `r`/`g`/`r` (`b` is stepped but never drawn),
/// then raises all three channels by `Task::spawnArg1`, the fade rate. Once
/// `r` reaches 0x100 the task kills itself.
void screenFadeOutTask(Task* arg0)
{
    ScreenFadeWork* work;
    ScreenFadeWork* alloc;

    work = arg0->work;
    switch (arg0->state) {
        case 0:
            alloc      = memMalloc(sizeof(*alloc), false);
            arg0->work = alloc;
            if (alloc == NULL) {
                taskKill(arg0);
                return;
            }
            work         = alloc;
            work->b      = 0;
            work->g      = 0;
            work->r      = 0;
            arg0->state += 1;
            /* fallthrough */
        case 1:
            fadeDrawOverlay(work->r, work->g, work->r, GPU_BLEND_SUBTRACT);
            work->r += (u16)arg0->spawnArg1.value;
            work->g += (u16)arg0->spawnArg1.value;
            work->b += (u16)arg0->spawnArg1.value;
            if (work->r >= 0x100) {
                taskKill(arg0);
            }
            break;
    }
}
