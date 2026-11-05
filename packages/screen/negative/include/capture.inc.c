/* Part of the screen negative library; see negative.h. */

/// Captures the displayed frame into the image workspace, turns it into a negative and stops scene drawing until the
/// `ScreenNegativeCaptureArgs` in `spawnArg2` says to end; pauses while event scripts are paused.
static inline void _screenNegativeCaptureTask(Task* task)
{
    enum {
        SCREEN_NEGATIVE_CAPTURE_START  = 0,
        SCREEN_NEGATIVE_CAPTURE_FILTER = 1,
        SCREEN_NEGATIVE_CAPTURE_HOLD   = 2,
    };
    ScreenNegativeCaptureArgs* args;
    s32                        stripIndex;
    u_long(*strip)[FILE_SYSTEM_IMAGE_STRIP_WORDS];

    args = task->spawnArg2.pointer;
    if (gEvsPlaybackPaused == 0) {
        switch (task->state) {
            case SCREEN_NEGATIVE_CAPTURE_START:
                args->done          = 0;
                task->killCountdown = args->duration;
                if (gDisplayState.drawBuffer != 0) {
                    _gScreenNegativeStripRect.y = 0;
                } else {
                    _gScreenNegativeStripRect.y = DISPLAY_BUFFER_STRIDE_ROWS;
                }
                if (gDisplayState.debugMode < 0) {
                    StoreImage(&_gScreenNegativeFrameRect, gFsImageWorkspacePtr->strips[0]);
                } else {
                    // Traverse complete strip arrays within the resident frame.
                    strip = gFsImageWorkspacePtr->strips;
                    for (stripIndex = 0; stripIndex < FILE_SYSTEM_IMAGE_STRIP_COUNT; stripIndex++) {
                        _gScreenNegativeStripRect.x = stripIndex * FILE_SYSTEM_IMAGE_STRIP_WIDTH;
                        StoreImage(&_gScreenNegativeStripRect, *strip);
                        strip++;
                    }
                }
                gDisplayState.skipDraw = 1;
                task->state++;
                break;
            case SCREEN_NEGATIVE_CAPTURE_FILTER:
                DrawSync(0);
                SCREEN_NEGATIVE_FILTER();
                task->state++;
                break;
            case SCREEN_NEGATIVE_CAPTURE_HOLD:
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
