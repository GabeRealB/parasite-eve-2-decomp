/* Part of the screen negative library; see screen_negative.h. */

/// The capture task: state 0 seeds the countdown from the `ScreenNegativeCaptureArgs`
/// duration and copies the displayed frame into `Fs_ImgBuffers` - twenty
/// 16-pixel strips from the shown buffer (`gScreenNegativeStripRect`), or the
/// whole of `gScreenNegativeFrameRect` at once while `gDisplayState.debugMode`
/// is negative - and stops drawing; state 1 waits for the transfer and runs
/// `SCREEN_NEGATIVE_FILTER`; state 2 holds the frozen negative until the
/// countdown runs out or `done` is set, then resumes drawing and ends.
static inline void screenNegativeCaptureTask(Task* task)
{
    ScreenNegativeCaptureArgs* args;
    s32                        i;
    u_long*                    strip;

    args = task->spawnArg2.pointer;
    if (D_801156F9 == 0) {
        switch (task->state) {
            case 0:
                args->done          = 0;
                task->killCountdown = args->duration;
                if (gDisplayState.drawBuffer != 0) {
                    gScreenNegativeStripRect.y = 0;
                } else {
                    gScreenNegativeStripRect.y = 0x110;
                }
                if (gDisplayState.debugMode < 0) {
                    StoreImage(&gScreenNegativeFrameRect, Fs_ImgBuffers->strips[0]);
                } else {
                    strip = Fs_ImgBuffers->strips[0];
                    for (i = 0; i < FILE_SYSTEM_IMAGE_STRIP_COUNT; i++) {
                        gScreenNegativeStripRect.x = i * FILE_SYSTEM_IMAGE_STRIP_WIDTH;
                        StoreImage(&gScreenNegativeStripRect, strip);
                        strip += FILE_SYSTEM_IMAGE_STRIP_WORDS;
                    }
                }
                gDisplayState.skipDraw = 1;
                task->state++;
                break;
            case 1:
                DrawSync(0);
                SCREEN_NEGATIVE_FILTER();
                task->state++;
                break;
            case 2:
                if (--task->killCountdown <= 0) {
                    args->done = 1;
                }
                if (args->done != 0) {
                    taskKill(task);
                    gDisplayState.skipDraw = 0;
                }
                break;
        }
    }
}
