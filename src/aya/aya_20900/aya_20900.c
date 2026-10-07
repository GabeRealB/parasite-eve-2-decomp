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

/// Phases of the Game Over screen, in the order `_Aya20900GameOverWork::phase` steps through them.
enum {
    AYA_20900_GAME_OVER_PHASE_ENTER        = 0, // Pick the entry: darken a preserved picture first, or start from black
    AYA_20900_GAME_OVER_PHASE_DARKEN_SCENE = 1, // Darken the preserved picture to black, logo hidden
    AYA_20900_GAME_OVER_PHASE_REVEAL_START = 2, // First reveal frame, drawn before the display is unmasked
    AYA_20900_GAME_OVER_PHASE_REVEAL       = 3, // Unmask the display and brighten the logo out of black
    AYA_20900_GAME_OVER_PHASE_HOLD         = 4, // Show the logo until a Start press skips it or the hold runs out
    AYA_20900_GAME_OVER_PHASE_DARKEN_OUT   = 5  // Darken to black, then report the screen finished
};

enum {
    /// Frames the Game Over logo is held before the screen leaves without a button press.
    AYA_20900_GAME_OVER_HOLD_FRAMES = 0x30D,
    /// `_Aya20900GameOverWork::darkness` at which the picture is fully black.
    AYA_20900_GAME_OVER_DARKNESS_BLACK = 0xFF
};

/// State of the Game Over screen's task, allocated by it and held in `Task::work`.
///
/// The screen is a centred logo under a full-screen tile that is subtracted
/// from the picture, so `darkness` is how much brightness is removed.
typedef struct {
    s16 phase;      // Current step of the screen (`AYA_20900_GAME_OVER_PHASE_*`)
    s16 holdFrames; // Frames the logo has been held, counted up to `AYA_20900_GAME_OVER_HOLD_FRAMES`
    s16 field_4;    // Cleared with the block and never accessed; role unproven
    s16 darkness;   // Level the covering tile subtracts from every channel (0 picture untouched, 0xFF black)
} _Aya20900GameOverWork;
STATIC_ASSERT_SIZEOF(_Aya20900GameOverWork, 0x8);

/// Pixel dimensions, ordering-table displacement and texture depth of the transition packets.
enum {
    AYA_20900_SCREEN_WIDTH        = FILE_SYSTEM_IMAGE_WIDTH,
    AYA_20900_SCREEN_HEIGHT       = FILE_SYSTEM_IMAGE_HEIGHT,
    AYA_20900_SCREEN_TILE_OT_BACK = 16,
    AYA_20900_TEXTURE_DEPTH_4BIT  = 0,
};

/// Fills every image strip with two RGB555 red pixels per word.
static inline void _aya20900FillRedImage(void)
{
    enum { AYA_20900_RED_FLASH_PIXEL = 0x001F };
    u_long* imageWords;
    u16     wordIndex;
    u_long  redPixelPair;

    // Two RGB555 pixels occupy each word; all twenty strips are contiguous.
    imageWords   = Fs_ImgBuffers->strips[0];
    redPixelPair = (AYA_20900_RED_FLASH_PIXEL << 16) | AYA_20900_RED_FLASH_PIXEL;
    wordIndex    = 0;
    do {
        *imageWords++ = redPixelPair;
        wordIndex++;
    } while ((u32)wordIndex <= sizeof(Fs_ImgBuffers->strips) / sizeof(*imageWords) - 1);
}

/// Replaces the image strips with red beneath a fading additive white flash.
///
/// Starts with a fresh task at state 0. After two callbacks, fills the whole
/// image workspace with RGB555 red; the next 32 callbacks reduce the flash
/// by eight levels each. The existing strip-display source must be selected
/// by the caller. Uses no work block and releases the task when the flash ends.
static void _aya20900RedFlashTask(Task* task)
{
    enum {
        AYA_20900_RED_FLASH_INITIALIZE = 0,
        AYA_20900_RED_FLASH_DELAY      = 1,
        AYA_20900_RED_FLASH_FILL_IMAGE = 2,
        AYA_20900_RED_FLASH_FADE       = 3,
        AYA_20900_RED_FLASH_FADE_STEP  = 8,
        AYA_20900_RED_FLASH_END_LEVEL  = 256,
    };
    TILE*     flashTile;
    DR_TPAGE* blendCommand;
    u8        flashLevel;
    char      unusedStack[8]; // Unused local retained for the original stack frame.

    switch (task->state) {
        case AYA_20900_RED_FLASH_INITIALIZE:
            task->killCountdown = 0;
        case AYA_20900_RED_FLASH_DELAY:
            task->state += 1;
            break;
        case AYA_20900_RED_FLASH_FILL_IMAGE:
            _aya20900FillRedImage();
            task->state += 1;
            break;
        case AYA_20900_RED_FLASH_FADE:
            task->killCountdown += AYA_20900_RED_FLASH_FADE_STEP;
            break;
    }

    if (task->killCountdown >= AYA_20900_RED_FLASH_END_LEVEL) {
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
    flashTile->x0 = -AYA_20900_SCREEN_WIDTH / 2;
    flashTile->y0 = -AYA_20900_SCREEN_HEIGHT / 2;
    flashTile->w  = AYA_20900_SCREEN_WIDTH;
    flashTile->h  = AYA_20900_SCREEN_HEIGHT;
    addPrim(gGpuCurrentOt - AYA_20900_SCREEN_TILE_OT_BACK, flashTile);

    blendCommand   = gGpuPrimCursor;
    gGpuPrimCursor = blendCommand + 1;
    setDrawTPage(blendCommand, false, true, getTPage(AYA_20900_TEXTURE_DEPTH_4BIT, GPU_BLEND_ADD, 0, 0));
    addPrim(gGpuCurrentOt - AYA_20900_SCREEN_TILE_OT_BACK, blendCommand);
}

/// Queues the centred, unmodulated 120 by 24 pixel Game Over logo.
///
/// Requires its 4-bit texture at VRAM (960, 0) and palette at (0, 255),
/// and a current ordering table and primitive buffer with room for one quad.
static void _aya20900DrawGameOverLogo(void)
{
    enum { AYA_20900_GAME_OVER_LOGO_WIDTH  = 120,
           AYA_20900_GAME_OVER_LOGO_HEIGHT = 24 };
    POLY_FT4* logoQuad;
    s16       left;
    s16       top;
    s16       width;
    s16       height;

    left           = -AYA_20900_GAME_OVER_LOGO_WIDTH / 2;
    logoQuad       = gGpuPrimCursor;
    gGpuPrimCursor = logoQuad + 1;
    setPolyFT4(logoQuad);
    setShadeTex(logoQuad, true);
    top    = -AYA_20900_GAME_OVER_LOGO_HEIGHT / 2;
    width  = AYA_20900_GAME_OVER_LOGO_WIDTH;
    height = AYA_20900_GAME_OVER_LOGO_HEIGHT;
    setXYWH(logoQuad, left, top, width, height);
    setUVWH(logoQuad, 0, 0, width, height);
    logoQuad->clut  = getClut(0, 255);
    logoQuad->tpage = getTPage(AYA_20900_TEXTURE_DEPTH_4BIT, GPU_BLEND_ADD, 960, 0);
    addPrim(gGpuCurrentOt, logoQuad);
}

/// Queues the screen's subtractive covering tile after drawing its logo.
static inline void _aya20900DrawGameOverDarkness(const _Aya20900GameOverWork* work)
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
    darknessTile->x0 = -AYA_20900_SCREEN_WIDTH / 2;
    darknessTile->y0 = -AYA_20900_SCREEN_HEIGHT / 2;
    darknessTile->w  = AYA_20900_SCREEN_WIDTH;
    darknessTile->h  = AYA_20900_SCREEN_HEIGHT;
    addPrim(gGpuCurrentOt - AYA_20900_SCREEN_TILE_OT_BACK, darknessTile);

    blendCommand   = gGpuPrimCursor;
    gGpuPrimCursor = blendCommand + 1;
    setDrawTPage(blendCommand, false, true, getTPage(AYA_20900_TEXTURE_DEPTH_4BIT, GPU_BLEND_SUBTRACT, 0, 0));
    addPrim(gGpuCurrentOt - AYA_20900_SCREEN_TILE_OT_BACK, blendCommand);
}

/// Advances and draws the Game Over screen once, returning 1 when it reaches black.
///
/// Borrows the task's initialized `_Aya20900GameOverWork`. Preserved displays
/// darken before the logo appears; other entries reveal the logo from black.
/// Timing counts callbacks. Start skips the hold and stops MIDI sequence 0x62.
static s16 _aya20900StepGameOver(Task* task)
{
    enum {
        AYA_20900_GAME_OVER_DARKEN_STEP     = 8,
        AYA_20900_GAME_OVER_REVEAL_STEP     = 4,
        AYA_20900_GAME_OVER_MIDI_SEQUENCE   = 0x62,
        AYA_20900_GAME_OVER_MIDI_STOP_TICKS = 1,
    };
    _Aya20900GameOverWork* work;
    u16                    showLogo;

    work     = task->work;
    showLogo = 1;
    switch (work->phase) {
        case AYA_20900_GAME_OVER_PHASE_ENTER:
            if (gGameSession->restartMode == GAME_SESSION_RESTART_PRESERVE_DISPLAY) {
                showLogo       = 0;
                work->darkness = 0;
                work->phase   += 1;
            } else {
                // Nothing to keep on screen: swap in a black background and start fully dark.
                memFillBytes(Fs_ImgBuffers, 0, sizeof(*Fs_ImgBuffers));
                gDisplayState.control.flags.imageSource = DISPLAY_IMAGE_STRIPS;
                work->darkness                          = AYA_20900_GAME_OVER_DARKNESS_BLACK;
                work->phase                             = AYA_20900_GAME_OVER_PHASE_REVEAL_START;
            }
            break;
        case AYA_20900_GAME_OVER_PHASE_DARKEN_SCENE:
            work->darkness += AYA_20900_GAME_OVER_DARKEN_STEP;
            if (work->darkness >= AYA_20900_GAME_OVER_DARKNESS_BLACK) {
                // The preserved picture is hidden now; replace it with the black background.
                memFillBytes(Fs_ImgBuffers, 0, sizeof(*Fs_ImgBuffers));
                gDisplayState.control.flags.imageSource = DISPLAY_IMAGE_STRIPS;
                work->darkness                          = AYA_20900_GAME_OVER_DARKNESS_BLACK;
                work->phase                            += 1;
            }
            showLogo = 0;
            break;
        case AYA_20900_GAME_OVER_PHASE_REVEAL:
            SetDispMask(1);
            goto reveal;
        case AYA_20900_GAME_OVER_PHASE_REVEAL_START:
            work->phase += 1;
        reveal:
            work->darkness -= AYA_20900_GAME_OVER_REVEAL_STEP;
            if (work->darkness <= 0) {
                work->darkness   = 0;
                work->holdFrames = 0;
                work->phase     += 1;
            }
            break;
        case AYA_20900_GAME_OVER_PHASE_HOLD:
            work->holdFrames += 1;
            if (work->holdFrames < AYA_20900_GAME_OVER_HOLD_FRAMES) {
                if (padIsStartPressed() != 0) {
                    sndEvtRequestMidiStop(AYA_20900_GAME_OVER_MIDI_SEQUENCE, AYA_20900_GAME_OVER_MIDI_STOP_TICKS);
                    work->phase += 1;
                }
            } else {
                work->phase += 1;
            }
            break;
        case AYA_20900_GAME_OVER_PHASE_DARKEN_OUT:
            work->darkness += AYA_20900_GAME_OVER_DARKEN_STEP;
            if (work->darkness >= AYA_20900_GAME_OVER_DARKNESS_BLACK) {
                gDisplayState.control.flags.imageSource = DISPLAY_IMAGE_NONE;
                return 1;
            }
            break;
    }

    if (showLogo != 0) {
        _aya20900DrawGameOverLogo();
    }

    _aya20900DrawGameOverDarkness(work);
    return 0;
}

/// Owns the Game Over screen until its final fade completes or allocation fails.
///
/// Starts at state 0, allocates a primary-heap work block and advances the
/// screen on the same callback. A finished screen requests task exit with
/// result 0; normal task teardown releases the work block. The package and
/// logo resources must remain loaded until exit has been consumed.
static void _aya20900GameOverTask(Task* task)
{
    enum { AYA_20900_GAME_OVER_TASK_INITIALIZE = 0,
           AYA_20900_GAME_OVER_TASK_UPDATE     = 1 };
    _Aya20900GameOverWork* work;
    s32                    taskState;

    taskState = task->state;
    switch (taskState) {
        case AYA_20900_GAME_OVER_TASK_INITIALIZE:
            work       = memMalloc(sizeof(*work), false);
            task->work = work;
            if (work == NULL) {
                taskKill(task);
                return;
            }
            memFillBytes(work, 0U, sizeof(*work));
            task->state += 1;
        case AYA_20900_GAME_OVER_TASK_UPDATE:
            if (_aya20900StepGameOver(task) != 0) {
                taskRequestKill(task, 0);
            }
            return;
    }
}

/// The package's task descriptors, spawned by gameplay from entry 0.
TaskDesc D_aya_20900_80115D9C[4] = {
    { { { TASK_BODY_NONE, 0xC0 } }, _aya20900GameOverTask, { NULL } },
    { { { TASK_BODY_NONE, 0xC0 } }, NULL, { NULL } },
    { { { TASK_BODY_NONE, 0xC0 } }, NULL, { NULL } },
    { { { TASK_BODY_NONE, 0xC0 } }, _aya20900RedFlashTask, { NULL } },
};
