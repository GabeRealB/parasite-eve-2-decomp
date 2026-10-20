#include "aya/game_over.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "common.h"

#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/fs_types.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"

/// Phases of the Game Over screen, in the order `_GameOverWork::phase` steps through them.
enum {
    GAME_OVER_PHASE_ENTER        = 0, // Pick the entry: darken a preserved picture first, or start from black
    GAME_OVER_PHASE_DARKEN_SCENE = 1, // Darken the preserved picture to black, logo hidden
    GAME_OVER_PHASE_REVEAL_START = 2, // First reveal frame, drawn before the display is unmasked
    GAME_OVER_PHASE_REVEAL       = 3, // Unmask the display and brighten the logo out of black
    GAME_OVER_PHASE_HOLD         = 4, // Show the logo until a Start press skips it or the hold runs out
    GAME_OVER_PHASE_DARKEN_OUT   = 5  // Darken to black, then report the screen finished
};

enum {
    /// Frames the Game Over logo is held before the screen leaves without a button press.
    GAME_OVER_HOLD_FRAMES = 0x30D,
    /// `_GameOverWork::darkness` at which the picture is fully black.
    GAME_OVER_DARKNESS_BLACK = 0xFF
};

/// State of the Game Over screen's task, allocated by it and held in `Task::work`.
///
/// The screen is a centred logo under a full-screen tile that is subtracted
/// from the picture, so `darkness` is how much brightness is removed.
typedef struct {
    s16 phase;      // Current step of the screen (`GAME_OVER_PHASE_*`)
    s16 holdFrames; // Frames the logo has been held, counted up to `GAME_OVER_HOLD_FRAMES`
    s16 field_4;    // Cleared with the block and never accessed; role unproven
    s16 darkness;   // Level the covering tile subtracts from every channel (0 picture untouched, 0xFF black)
} _GameOverWork;
STATIC_ASSERT_SIZEOF(_GameOverWork, 0x8);

/// Pixel dimensions, ordering-table displacement and texture depth of the transition packets.
enum {
    GAME_OVER_SCREEN_WIDTH        = FILE_SYSTEM_IMAGE_WIDTH,
    GAME_OVER_SCREEN_HEIGHT       = FILE_SYSTEM_IMAGE_HEIGHT,
    GAME_OVER_SCREEN_TILE_OT_BACK = 16,
    GAME_OVER_TEXTURE_DEPTH_4BIT  = 0,
};

/// Replaces the resident image workspace with a solid RGB555 red frame.
///
/// Writes all 320 by 240 pixels, with the mask bit clear. The caller must
/// finish decoding or capturing before overwriting the shared workspace;
/// this does not upload the strips or select them as the display source.
static inline void _gameOverFillRedImage(void)
{
    enum { GAME_OVER_RED_FLASH_PIXEL = 0x001F };
    u_long* imageWords;
    u16     wordIndex;
    u_long  redPixelPair;

    // Two RGB555 pixels occupy each word; all twenty strips are contiguous.
    imageWords   = gFsImageWorkspacePtr->strips[0];
    redPixelPair = (GAME_OVER_RED_FLASH_PIXEL << 16) | GAME_OVER_RED_FLASH_PIXEL;
    wordIndex    = 0;
    do {
        *imageWords++ = redPixelPair;
        wordIndex++;
    } while ((u32)wordIndex <= ARRAY_SIZE(gFsImageWorkspacePtr->strips) * ARRAY_SIZE(gFsImageWorkspacePtr->strips[0]) - 1);
}

/// Replaces the image strips with red beneath a fading additive white flash.
///
/// Starts with a fresh task at state 0. After two callbacks, fills the whole
/// image workspace with RGB555 red; the next 32 callbacks reduce the flash
/// by eight levels each. The existing strip-display source must be selected
/// by the caller. Uses no work block and releases the task when the flash ends.
static void _gameOverRedFlashTask(Task* task)
{
    enum {
        GAME_OVER_RED_FLASH_INITIALIZE = 0,
        GAME_OVER_RED_FLASH_DELAY      = 1,
        GAME_OVER_RED_FLASH_FILL_IMAGE = 2,
        GAME_OVER_RED_FLASH_FADE       = 3,
        GAME_OVER_RED_FLASH_FADE_STEP  = 8,
        GAME_OVER_RED_FLASH_END_LEVEL  = 256,
    };
    TILE*     flashTile;
    DR_TPAGE* blendCommand;
    u8        flashLevel;
    char      unusedStack[8]; // Unused local retained for the original stack frame.

    switch (task->state) {
        case GAME_OVER_RED_FLASH_INITIALIZE:
            task->killCountdown = 0;
        case GAME_OVER_RED_FLASH_DELAY:
            task->state += 1;
            break;
        case GAME_OVER_RED_FLASH_FILL_IMAGE:
            _gameOverFillRedImage();
            task->state += 1;
            break;
        case GAME_OVER_RED_FLASH_FADE:
            task->killCountdown += GAME_OVER_RED_FLASH_FADE_STEP;
            break;
    }

    if (task->killCountdown >= GAME_OVER_RED_FLASH_END_LEVEL) {
        taskKill(task);
        return;
    }

    flashTile      = gGpuPrimCursor;
    flashLevel     = ~(u8)task->killCountdown;
    gGpuPrimCursor = flashTile + 1;
    setTile(flashTile);
    setSemiTrans(flashTile, true);
    flashTile->r0 = flashLevel;
    flashTile->g0 = flashLevel;
    flashTile->b0 = flashLevel;
    flashTile->x0 = -GAME_OVER_SCREEN_WIDTH / 2;
    flashTile->y0 = -GAME_OVER_SCREEN_HEIGHT / 2;
    flashTile->w  = GAME_OVER_SCREEN_WIDTH;
    flashTile->h  = GAME_OVER_SCREEN_HEIGHT;
    addPrim(gGpuCurrentOt - GAME_OVER_SCREEN_TILE_OT_BACK, flashTile);

    blendCommand   = gGpuPrimCursor;
    gGpuPrimCursor = blendCommand + 1;
    setDrawTPage(blendCommand, false, true, getTPage(GAME_OVER_TEXTURE_DEPTH_4BIT, GPU_BLEND_ADD, 0, 0));
    addPrim(gGpuCurrentOt - GAME_OVER_SCREEN_TILE_OT_BACK, blendCommand);
}

/// Queues the centred, unmodulated 120 by 24 pixel Game Over logo.
///
/// Requires its 4-bit texture at VRAM (960, 0) and palette at (0, 255),
/// and a current ordering table and primitive buffer with room for one quad.
static void _gameOverDrawLogo(void)
{
    enum { GAME_OVER_LOGO_WIDTH  = 120,
           GAME_OVER_LOGO_HEIGHT = 24 };
    POLY_FT4* logoQuad;
    s16       left;
    s16       top;
    s16       width;
    s16       height;

    left           = -GAME_OVER_LOGO_WIDTH / 2;
    logoQuad       = gGpuPrimCursor;
    gGpuPrimCursor = logoQuad + 1;
    setPolyFT4(logoQuad);
    setShadeTex(logoQuad, true);
    top    = -GAME_OVER_LOGO_HEIGHT / 2;
    width  = GAME_OVER_LOGO_WIDTH;
    height = GAME_OVER_LOGO_HEIGHT;
    setXYWH(logoQuad, left, top, width, height);
    setUVWH(logoQuad, 0, 0, width, height);
    logoQuad->clut  = getClut(0, 255);
    logoQuad->tpage = getTPage(GAME_OVER_TEXTURE_DEPTH_4BIT, GPU_BLEND_ADD, 960, 0);
    addPrim(gGpuCurrentOt, logoQuad);
}

/// Queues a full-screen subtraction of the Game Over screen's darkness level.
///
/// Borrows initialized work with darkness in 0..255 (0 unchanged, 255 black).
/// Requires a centered 320 by 240 draw area, a current ordering table with
/// sixteen preceding tags, and primitive space for one TILE and one DR_TPAGE.
/// The packets affect the scene and any logo already queued by the caller.
static inline void _gameOverDrawDarkness(const _GameOverWork* work)
{
    TILE*     darknessTile;
    DR_TPAGE* blendCommand;
    u8        darknessLevel;

    // The command precedes the tile when the ordering table is traversed.
    darknessTile   = gGpuPrimCursor;
    darknessLevel  = work->darkness;
    gGpuPrimCursor = darknessTile + 1;
    setTile(darknessTile);
    setSemiTrans(darknessTile, true);
    darknessTile->r0 = darknessLevel;
    darknessTile->g0 = darknessLevel;
    darknessTile->b0 = darknessLevel;
    darknessTile->x0 = -GAME_OVER_SCREEN_WIDTH / 2;
    darknessTile->y0 = -GAME_OVER_SCREEN_HEIGHT / 2;
    darknessTile->w  = GAME_OVER_SCREEN_WIDTH;
    darknessTile->h  = GAME_OVER_SCREEN_HEIGHT;
    addPrim(gGpuCurrentOt - GAME_OVER_SCREEN_TILE_OT_BACK, darknessTile);

    blendCommand   = gGpuPrimCursor;
    gGpuPrimCursor = blendCommand + 1;
    setDrawTPage(blendCommand, false, true, getTPage(GAME_OVER_TEXTURE_DEPTH_4BIT, GPU_BLEND_SUBTRACT, 0, 0));
    addPrim(gGpuCurrentOt - GAME_OVER_SCREEN_TILE_OT_BACK, blendCommand);
}

/// Advances and draws the Game Over screen once, returning 1 when it reaches black.
///
/// Borrows the task's initialized `_GameOverWork`. Preserved displays
/// darken before the logo appears; other entries reveal the logo from black.
/// Timing counts callbacks. Start skips the hold and stops MIDI sequence 0x62.
static s16 _gameOverStep(Task* task)
{
    enum {
        GAME_OVER_DARKEN_STEP     = 8,
        GAME_OVER_REVEAL_STEP     = 4,
        GAME_OVER_MIDI_SEQUENCE   = 0x62,
        GAME_OVER_MIDI_STOP_TICKS = 1,
    };
    _GameOverWork* work;
    u16            showLogo;

    work     = task->work;
    showLogo = 1;
    switch (work->phase) {
        case GAME_OVER_PHASE_ENTER:
            if (gGameSession->restartMode == GAME_SESSION_RESTART_PRESERVE_DISPLAY) {
                showLogo       = 0;
                work->darkness = 0;
                work->phase   += 1;
            } else {
                // Nothing to keep on screen: swap in a black background and start fully dark.
                memFillBytes(gFsImageWorkspacePtr, 0, sizeof(*gFsImageWorkspacePtr));
                gDisplayState.control.flags.imageSource = DISPLAY_IMAGE_STRIPS;
                work->darkness                          = GAME_OVER_DARKNESS_BLACK;
                work->phase                             = GAME_OVER_PHASE_REVEAL_START;
            }
            break;
        case GAME_OVER_PHASE_DARKEN_SCENE:
            work->darkness += GAME_OVER_DARKEN_STEP;
            if (work->darkness >= GAME_OVER_DARKNESS_BLACK) {
                // The preserved picture is hidden now; replace it with the black background.
                memFillBytes(gFsImageWorkspacePtr, 0, sizeof(*gFsImageWorkspacePtr));
                gDisplayState.control.flags.imageSource = DISPLAY_IMAGE_STRIPS;
                work->darkness                          = GAME_OVER_DARKNESS_BLACK;
                work->phase                            += 1;
            }
            showLogo = 0;
            break;
        case GAME_OVER_PHASE_REVEAL:
            SetDispMask(1);
            goto reveal;
        case GAME_OVER_PHASE_REVEAL_START:
            work->phase += 1;
        reveal:
            work->darkness -= GAME_OVER_REVEAL_STEP;
            if (work->darkness <= 0) {
                work->darkness   = 0;
                work->holdFrames = 0;
                work->phase     += 1;
            }
            break;
        case GAME_OVER_PHASE_HOLD:
            work->holdFrames += 1;
            if (work->holdFrames < GAME_OVER_HOLD_FRAMES) {
                if (padIsStartPressed() != 0) {
                    sndEvtRequestMidiStop(GAME_OVER_MIDI_SEQUENCE, GAME_OVER_MIDI_STOP_TICKS);
                    work->phase += 1;
                }
            } else {
                work->phase += 1;
            }
            break;
        case GAME_OVER_PHASE_DARKEN_OUT:
            work->darkness += GAME_OVER_DARKEN_STEP;
            if (work->darkness >= GAME_OVER_DARKNESS_BLACK) {
                gDisplayState.control.flags.imageSource = DISPLAY_IMAGE_NONE;
                return 1;
            }
            break;
    }

    if (showLogo != 0) {
        _gameOverDrawLogo();
    }

    _gameOverDrawDarkness(work);
    return 0;
}

/// Owns the Game Over screen until its final fade completes or allocation fails.
///
/// Starts at state 0, allocates a primary-heap work block and advances the
/// screen on the same callback. A finished screen requests task exit with
/// result 0; normal task teardown releases the work block. The package and
/// logo resources must remain loaded until exit has been consumed.
static void _gameOverTask(Task* task)
{
    enum { GAME_OVER_STATE_INITIALIZE = 0,
           GAME_OVER_STATE_UPDATE     = 1 };
    _GameOverWork* work;
    s32            taskState;

    taskState = task->state;
    switch (taskState) {
        case GAME_OVER_STATE_INITIALIZE:
            work       = memMalloc(sizeof(*work), false);
            task->work = work;
            if (work == NULL) {
                taskKill(task);
                return;
            }
            memFillBytes(work, 0U, sizeof(*work));
            task->state += 1;
        case GAME_OVER_STATE_UPDATE:
            if (_gameOverStep(task) != 0) {
                taskRequestKill(task, 0);
            }
            return;
    }
}

enum { GAME_OVER_PRESENTATION_TASK_PRIORITY = 0xC0 };
TaskDesc gGameOverTaskDescriptors[GAME_OVER_TASK_COUNT] = {
    { { { TASK_BODY_NONE, GAME_OVER_PRESENTATION_TASK_PRIORITY } }, _gameOverTask, { NULL } },
    { { { TASK_BODY_NONE, GAME_OVER_PRESENTATION_TASK_PRIORITY } }, NULL, { NULL } },
    { { { TASK_BODY_NONE, GAME_OVER_PRESENTATION_TASK_PRIORITY } }, NULL, { NULL } },
    { { { TASK_BODY_NONE, GAME_OVER_PRESENTATION_TASK_PRIORITY } }, _gameOverRedFlashTask, { NULL } },
};
