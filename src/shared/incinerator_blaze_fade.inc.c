/* Part of the incinerator blaze library; see incinerator_blaze.h. */

/// Queues the opaque white screen held after the incinerator's colour ramps.
///
/// Borrows one TILE and one DR_TPAGE from the word-aligned frame arena until
/// GPU completion. Foreground OT tag -16 must be writable. The fixed screen
/// rectangle retains the draw environment's vertical shake.
static inline void _blazeQueueWhiteout(void)
{
    enum {
        BLAZE_FADE_CHANNEL_MAX   = 255,
        BLAZE_FADE_WIDTH_PIXELS  = 320,
        BLAZE_FADE_HEIGHT_PIXELS = 240,
        BLAZE_FADE_OT_INDEX      = -16,
    };
    TILE*     tile;
    DR_TPAGE* drawMode;

    tile           = gGpuPrimCursor;
    gGpuPrimCursor = tile + 1;
    setTile(tile);
    tile->r0 = BLAZE_FADE_CHANNEL_MAX;
    tile->g0 = BLAZE_FADE_CHANNEL_MAX;
    tile->b0 = BLAZE_FADE_CHANNEL_MAX;
    tile->x0 = -BLAZE_FADE_WIDTH_PIXELS / 2;
    tile->y0 = -BLAZE_FADE_HEIGHT_PIXELS / 2;
    tile->w  = BLAZE_FADE_WIDTH_PIXELS;
    tile->h  = BLAZE_FADE_HEIGHT_PIXELS;
    addPrim(gGpuCurrentOt + BLAZE_FADE_OT_INDEX, tile);

    // OT insertion prepends: the draw mode must execute before the tile.
    drawMode       = gGpuPrimCursor;
    gGpuPrimCursor = drawMode + 1;
    setDrawTPage(drawMode, false, true, 0);
    addPrim(gGpuCurrentOt + BLAZE_FADE_OT_INDEX, drawMode);
}

/// Draws the incinerator's scripted red-to-white fade and holds the whiteout.
///
/// Start a bodyless task in state 0. `spawnArg2.pointer` borrows its controller
/// task, whose work begins with `BlazeParentWork` and must remain live until the
/// wash to white finishes; `spawnArg1` is unused. The task owns an eight-byte
/// `ScreenFadeWork` primary-heap allocation until teardown. Allocation failure
/// kills the task.
///
/// `BLAZE_FADE_MESSAGE_SET_STATE` selects 2 (red +10 to 80), 3 (red +1 to 255),
/// then 4 (green and blue +8 to white), with one step per callback. Each red
/// ramp returns to state 1, holding the additive tint until the next request.
/// The white wash stops the controller's heat haze, restores the default
/// framebuffer layout and fills the complete RAM image workspace white.
/// State 5 holds an opaque 320 by 240 white tile without vertical-shake
/// compensation. Other states only draw the current tint's low channel bytes.
///
/// Drawing requires a word-aligned frame-arena reservation for one `TILE` and
/// one `DR_TPAGE`, and a writable foreground OT tag at index -16. Packet storage
/// remains borrowed until GPU completion. The task stays live after whiteout.
static void _blazeFadeTask(Task* task)
{
    enum {
        BLAZE_FADE_STATE_INIT       = 0,
        BLAZE_FADE_STATE_HOLD_TINT  = 1,
        BLAZE_FADE_STATE_FAST_RED   = 2,
        BLAZE_FADE_STATE_SLOW_RED   = 3,
        BLAZE_FADE_STATE_TO_WHITE   = 4,
        BLAZE_FADE_STATE_HOLD_WHITE = 5,
        BLAZE_FADE_FAST_RED_STEP    = 10,
        BLAZE_FADE_FAST_RED_MAX     = 80,
        BLAZE_FADE_WHITE_STEP       = 8,
        BLAZE_FADE_CHANNEL_MAX      = 255,
        BLAZE_FADE_WHITE_IMAGE_FILL = 0xFF,
    };
    ScreenFadeWork*  fadeWork;
    BlazeParentWork* parentWork;

    fadeWork = task->work;
    switch (task->state) {
        case BLAZE_FADE_STATE_INIT:
            task->work = memMalloc(sizeof(*fadeWork), false);
            if (task->work == NULL) {
                taskKill(task);
                return;
            }
            fadeWork       = task->work;
            fadeWork->b    = 0;
            fadeWork->g    = 0;
            fadeWork->r    = 0;
            task->msgTable = gBlazeFadeMessages;
            task->state   += 1;
            break;
        case BLAZE_FADE_STATE_FAST_RED:
            fadeWork->r += BLAZE_FADE_FAST_RED_STEP;
            if (fadeWork->r >= BLAZE_FADE_FAST_RED_MAX + 1) {
                fadeWork->r = BLAZE_FADE_FAST_RED_MAX;
                task->state = BLAZE_FADE_STATE_HOLD_TINT;
            }
            break;
        case BLAZE_FADE_STATE_SLOW_RED:
            fadeWork->r += 1;
            if (fadeWork->r >= BLAZE_FADE_CHANNEL_MAX + 1) {
                fadeWork->r = BLAZE_FADE_CHANNEL_MAX;
                task->state = BLAZE_FADE_STATE_HOLD_TINT;
            }
            break;
        case BLAZE_FADE_STATE_TO_WHITE:
            fadeWork->g += BLAZE_FADE_WHITE_STEP;
            fadeWork->b += BLAZE_FADE_WHITE_STEP;
            if (fadeWork->g >= BLAZE_FADE_CHANNEL_MAX + 1) {
                // Replace the heat-haze image with white before holding the whiteout.
                parentWork             = ((Task*)task->spawnArg2.pointer)->work;
                parentWork->wave.state = SCREEN_WAVE_RAMP_FINISHED;
                displayConfigureFramebuffers(DISPLAY_SETUP_DEFAULT | DISPLAY_SETUP_NO_CLEAR | DISPLAY_SETUP_KEEP_VIEW);
                memFillBytes(Fs_ImgBuffers, BLAZE_FADE_WHITE_IMAGE_FILL, sizeof(*Fs_ImgBuffers));
                fadeWork->b = BLAZE_FADE_CHANNEL_MAX;
                fadeWork->g = BLAZE_FADE_CHANNEL_MAX;
                task->state = BLAZE_FADE_STATE_HOLD_WHITE;
            }
            break;
        case BLAZE_FADE_STATE_HOLD_WHITE:
            _blazeQueueWhiteout();
            return;
    }
    fadeDrawOverlay(fadeWork->r, fadeWork->g, fadeWork->b, GPU_BLEND_ADD);
}
