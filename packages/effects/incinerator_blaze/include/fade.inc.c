/* Part of the incinerator blaze library; see incinerator_blaze.h. */

#include <screen/fade/fade.h>
#include <screen/wave/wave.h>

/// Queues the opaque full-screen white tile the blaze fade ends on.
static inline void _blazeQueueWhiteout(void)
{
    enum {
        BLAZE_FADE_CHANNEL_MAX = 255,
        BLAZE_FADE_OT_INDEX    = -16,
    };
    TILE*     tile;
    DR_TPAGE* drawMode;

    tile           = gGpuPrimCursor;
    gGpuPrimCursor = tile + 1;
    setTile(tile);
    setRGB0(tile, BLAZE_FADE_CHANNEL_MAX, BLAZE_FADE_CHANNEL_MAX, BLAZE_FADE_CHANNEL_MAX);
    setXY0(tile, -DISPLAY_SCREEN_WIDTH / 2, -DISPLAY_SCREEN_HEIGHT / 2);
    setWH(tile, DISPLAY_SCREEN_WIDTH, DISPLAY_SCREEN_HEIGHT);
    addPrim(gGpuCurrentOt + BLAZE_FADE_OT_INDEX, tile);

    // OT insertion prepends: the draw mode must execute before the tile.
    drawMode       = gGpuPrimCursor;
    gGpuPrimCursor = drawMode + 1;
    setDrawTPage(drawMode, false, true, 0);
    addPrim(gGpuCurrentOt + BLAZE_FADE_OT_INDEX, drawMode);
}

/// Tints the screen with an additive wash its spawner steps through by message: state 2 ramps red quickly to 80, 3
/// slowly to 255, and 4 washes green and blue up to white, then stops the spawner's heat haze and holds a white screen.
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
            task->msgTable = _gBlazeFadeMessages;
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
                memFillBytes(gFsImageWorkspacePtr, BLAZE_FADE_WHITE_IMAGE_FILL, sizeof(*gFsImageWorkspacePtr));
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
