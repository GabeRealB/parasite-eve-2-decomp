/* Part of the screen negative library; see screen_negative.h. */

/// Captures and filters a frame, then holds scene drawing until release.
///
/// `spawnArg2.pointer` borrows writable `ScreenNegativeCaptureArgs` through
/// task completion. Duration is an unsigned tick count copied at capture;
/// the copy narrows to the signed task halfword, so use 0..32767 for an ordinary
/// duration (zero ends on the first hold tick). Larger values retain halfword
/// wrap. Initialization clears done, and the hold ends on expiry or a later nonzero
/// done flag. A nonzero script gate pauses every phase, including release.
/// Requires exclusive use of the complete resident RGB555 image workspace
/// through transfer/filtering and the carrier's capture rectangles and filter.
/// Normal capture stores twenty 16x240 strips from the displayed buffer;
/// negative debug mode stores the fixed whole-frame rectangle contiguously.
/// Upload and frozen-frame drawing belong to the scene's other tasks.
static inline void _screenNegativeCaptureTask(Task* task)
{
    enum {
        SCREEN_NEGATIVE_CAPTURE_START   = 0,
        SCREEN_NEGATIVE_CAPTURE_FILTER  = 1,
        SCREEN_NEGATIVE_CAPTURE_HOLD    = 2,
        SCREEN_NEGATIVE_SECOND_BUFFER_Y = 272
    };
    ScreenNegativeCaptureArgs* args;
    s32                        stripIndex;
    u_long(*strip)[FILE_SYSTEM_IMAGE_STRIP_WORDS];

    args = task->spawnArg2.pointer;
    if (D_801156F9 == 0) {
        switch (task->state) {
            case SCREEN_NEGATIVE_CAPTURE_START:
                args->done          = 0;
                task->killCountdown = args->duration;
                if (gDisplayState.drawBuffer != 0) {
                    gScreenNegativeStripRect.y = 0;
                } else {
                    gScreenNegativeStripRect.y = SCREEN_NEGATIVE_SECOND_BUFFER_Y;
                }
                if (gDisplayState.debugMode < 0) {
                    StoreImage(&gScreenNegativeFrameRect, Fs_ImgBuffers->strips[0]);
                } else {
                    // Traverse complete strip arrays within the resident frame.
                    strip = Fs_ImgBuffers->strips;
                    for (stripIndex = 0; stripIndex < FILE_SYSTEM_IMAGE_STRIP_COUNT; stripIndex++) {
                        gScreenNegativeStripRect.x = stripIndex * FILE_SYSTEM_IMAGE_STRIP_WIDTH;
                        StoreImage(&gScreenNegativeStripRect, *strip);
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
