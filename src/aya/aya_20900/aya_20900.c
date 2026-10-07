#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "common.h"

#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
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
    AYA_20900_GAME_OVER_PHASE_HOLD         = 4, // Show the logo until a pad press skips it or the hold runs out
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

void        func_aya_20900_8011578C(Task* arg0);
static void func_aya_20900_80115948(void);
static s32  func_aya_20900_80115A14(Task* arg0);
void        func_aya_20900_80115CFC(Task* arg0);

void func_aya_20900_8011578C(Task* arg0)
{
    TILE*     p;
    DR_TPAGE* dr;
    u32*      buf;
    u32       i;
    u32       val;
    u8        color;
    char      pad[8];

    switch (arg0->state) {
        case 0:
            arg0->killCountdown = 0;
        case 1:
            arg0->state += 1;
            break;
        case 2:
            buf = (u32*)Fs_ImgBuffers->strips[0];
            val = 0x1F001F;
            i   = 0;
            do {
                *buf++ = val;
                i++;
            } while ((i & 0xFFFF) <= (u32)(FILE_SYSTEM_IMAGE_STRIP_COUNT * FILE_SYSTEM_IMAGE_STRIP_WORDS - 1));
            arg0->state += 1;
            break;
        case 3:
            arg0->killCountdown += 8;
            break;
    }

    if (arg0->killCountdown >= 0x100) {
        taskKill(arg0);
        return;
    }

    p              = gGpuPrimCursor;
    color          = ~(u8)arg0->killCountdown;
    gGpuPrimCursor = p + 1;
    setlen(p, 3);
    setcode(p, 0x62);
    p->r0 = color;
    p->g0 = color;
    p->b0 = color;
    p->x0 = -0xA0;
    p->y0 = -0x78;
    p->w  = 0x140;
    p->h  = 0xF0;
    addPrim(gGpuCurrentOt - 0x10, p);

    dr             = gGpuPrimCursor;
    gGpuPrimCursor = dr + 1;
    setlen(dr, 1);
    dr->code[0] = 0xE1000000 | 0x220;
    addPrim(gGpuCurrentOt - 0x10, dr);
}

static void func_aya_20900_80115948(void)
{
    POLY_FT4* p;
    s16       x;
    s16       y;
    s16       w;
    s16       h;

    x              = -0x3C;
    p              = gGpuPrimCursor;
    gGpuPrimCursor = p + 1;
    setlen(p, 9);
    setcode(p, 0x2D);
    y = -0xC;
    w = 0x78;
    h = 0x18;
    setXYWH(p, x, y, w, h);
    setUVWH(p, 0, 0, w, h);
    p->clut  = 0x3FC0;
    p->tpage = 0x2F;
    addPrim(gGpuCurrentOt, p);
}

static s32 func_aya_20900_80115A14(Task* arg0)
{
    _Aya20900GameOverWork* work;
    TILE*                  p;
    DR_TPAGE*              dr;
    s32                    showLogo;
    u8                     color;

    work     = arg0->work;
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
            work->darkness += 8;
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
            work->darkness -= 4;
            if (work->darkness <= 0) {
                work->darkness   = 0;
                work->holdFrames = 0;
                work->phase     += 1;
            }
            break;
        case AYA_20900_GAME_OVER_PHASE_HOLD:
            work->holdFrames += 1;
            if (work->holdFrames < AYA_20900_GAME_OVER_HOLD_FRAMES) {
                if (Pad_CheckFlag800() != 0) {
                    sndEvtRequestMidiStop(0x62, 1);
                    work->phase += 1;
                }
            } else {
                work->phase += 1;
            }
            break;
        case AYA_20900_GAME_OVER_PHASE_DARKEN_OUT:
            work->darkness += 8;
            if (work->darkness >= AYA_20900_GAME_OVER_DARKNESS_BLACK) {
                gDisplayState.control.flags.imageSource = DISPLAY_IMAGE_NONE;
                return 1;
            }
            break;
    }

    if (showLogo & 0xFFFF) {
        func_aya_20900_80115948();
    }

    p              = gGpuPrimCursor;
    color          = work->darkness;
    gGpuPrimCursor = p + 1;
    setlen(p, 3);
    setcode(p, 0x62);
    p->r0 = color;
    p->g0 = color;
    p->b0 = color;
    p->x0 = -0xA0;
    p->y0 = -0x78;
    p->w  = 0x140;
    p->h  = 0xF0;
    addPrim(gGpuCurrentOt - 0x10, p);

    dr             = gGpuPrimCursor;
    gGpuPrimCursor = dr + 1;
    setlen(dr, 1);
    dr->code[0] = 0xE1000000 | 0x240;
    addPrim(gGpuCurrentOt - 0x10, dr);
    return 0;
}

void func_aya_20900_80115CFC(Task* arg0)
{
    _Aya20900GameOverWork* work;
    s32                    temp_v1;

    temp_v1 = arg0->state;
    switch (temp_v1) { /* irregular */
        case 0:
            work       = memMalloc(sizeof(*work), false);
            arg0->work = work;
            if (work == NULL) {
                taskKill(arg0);
                return;
            }
            memFillBytes(work, 0U, sizeof(*work));
            arg0->state += 1;
        case 1:
            if ((func_aya_20900_80115A14(arg0) << 0x10) != 0) {
                taskRequestKill(arg0, 0);
            }
            return;
    }
}

/// The package's task descriptors, spawned by gameplay from entry 0.
TaskDesc D_aya_20900_80115D9C[4] = {
    { { { TASK_BODY_NONE, 0xC0 } }, func_aya_20900_80115CFC, { NULL } },
    { { { TASK_BODY_NONE, 0xC0 } }, NULL, { NULL } },
    { { { TASK_BODY_NONE, 0xC0 } }, NULL, { NULL } },
    { { { TASK_BODY_NONE, 0xC0 } }, func_aya_20900_8011578C, { NULL } },
};
