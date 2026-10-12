#include "gamemain.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libapi.h>
#include <psyq/libetc.h>
#include <psyq/libgs.h>

#include "common.h"

#include "boot.h"
#include "cdaudio.h"
#include "main/coord.h"
#include "main/display_types.h"
#include "display_types.h"
#include "main/fs.h"
#include "main/fs_types.h"
#include "fs.h"
#include "gfx.h"
#include "gpuext.h"
#include "mc.h"
#include "main/mem.h"
#include "main/game_debug.h"
#include "main/pad.h"
#include "pad.h"
#include "main/pad_types.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "sound.h"
#include "stream.h"
#include "main/task.h"
#include "task.h"
#include "text.h"
#include "main/tmd.h"
#include "main/wipsys_types.h"

/* Define BSS before API headers to preserve first-declaration order. */
/// Reusable black semitransparent rectangle for the synchronous pause backdrop.
static TILE _gGameMainPauseTile;

/// Reusable average-blend draw-mode packet for the synchronous pause backdrop.
static DR_TPAGE _gGameMainPauseDrawMode;

volatile u8 gDisplayTaskImageSourceSnapshot;

u_long gGpuFrameOtTags[2][GPU_ORDERING_TABLE_BUFFER_ENTRIES];

volatile u8 gDisplayTaskFlipControlSnapshot;

GfxCoord gGfxViewRotCoord;

GfxCoord gGfxViewOffsetCoord;

void* gGpuSystemPrimCursor;

GsOT gGpuFrameOrderingTables[2];

GfxCoord gGfxViewCoord;

u32 gRandomLcgState;

/// Decremented once per vertical blank; no consumer or countdown purpose is established.
static volatile s32 D_80070F64;

DisplayState gDisplayState;

u_long* gGpuCurrentOt;

GameMainPersistentState gGameMainPersistentState;

/// Retained 80-byte resident reservation with no observed accesses; role unproven.
static u8 D_800710C8[0x50];

#include "main/display.h"
#include "display.h"
#include "main/gamemain.h"
#include "main/gfx.h"
#include "main/random.h"

static u32 _gGameMainInitializationCount;

static s32 D_8005EC7C;

static u32 D_8005EC84[4];

static const u8 _gGameMainPauseText[];

static void _gameMainInitialize(void);

static inline void _displayPresentFrame(s32 bufferIndex);

static void _displayVSyncCallback(void);

static inline s32 _gameMainPauseBlocked(void);

static void _gameMainShowPauseScreen(s32 haltBitIndex);

static inline s32 _gameMainPaceToSceneTiming(s32 frameStartLines, s32 elapsedLines);

static void _gameMainRunLoop(void);

static void _gameMainInitGraphics(void);

static void _gameMainSpawnStartupTask(void);

static void _displayPresentGameFrame(s32 bufferIndex);

enum {
    GAME_MAIN_FIRST_INITIALIZATION           = 1,
    GAME_MAIN_HALT_CONTROLLER_DISCONNECT_BIT = 1,
    DISPLAY_INITIAL_FRAME_BUDGET_LINES       = 240,
};

/// Wrapping resident initialization count retained across soft resets.
///
/// Cleared at power-on; the first initialization increments it to one and
/// selects full graphics and boot setup. Later initializations reload the title.
static u32   _gGameMainInitializationCount     = 0;
s32          gDisplayVSyncWaitMode             = 0;
s32          gDisplayFrameBudgetLines          = DISPLAY_INITIAL_FRAME_BUDGET_LINES;
volatile s32 gDisplayPendingFlip               = 0;
volatile s32 gDisplayVSyncElapsedLines         = 0;
volatile s32 gDisplayPreviousVSyncElapsedLines = 0;
/// Retained initialized word with no observed accesses; role unproven.
static s32   D_8005EC7C         = 0;
volatile s32 gGameMainHaltFlags = 0;
/// Retained four-word data block with no observed accesses; interpretation unproven.
static u32 D_8005EC84[4] = { 0, 0x01FF03FF, 0, 0 };

/// Clears resident display state and selects the initial game-loop controls.
///
/// Discards both framebuffer environments, clocks and requests, selects NTSC,
/// holds mode requests and restores one-VBlank timing. Call before configuring
/// framebuffers, with no task or callback needing the previous state. The
/// retained halfword initialized to 1 has no established role.
static inline void _displayResetGameState(void)
{
    memFillBytes(&gDisplayState, 0, sizeof(gDisplayState));
    gDisplayState.field_120                      = 1;
    gDisplayState.region                         = MODE_NTSC;
    gDisplayState.control.flags.pendingPlayerPos = 0;
    gDisplayState.holdState                      = DISPLAY_HOLD_INITIAL;
    gDisplayState.displayOwner                   = DISPLAY_OWNER_GAME_LOOP;
    gDisplayState.pendingMode                    = DISPLAY_MODE_NONE;
    gDisplayState.frameCount                     = 0;
    gDisplayState.gameTick                       = 0;
    gDisplayState.animFrame                      = 0;
    gDisplayState.vsyncCount                     = 0;
    gDisplayState.loopTicks                      = 0;
    gDisplayState.loopCount                      = 0;
    displaySetFrameTiming(DISPLAY_TIMING_EVERY_VBLANK);
}

/// Initializes the resident game run at startup or after an accepted reset.
///
/// Blanks display and finishes CD-audio cancellation before replacing callbacks,
/// heaps and task/render lists. Increments the wrapping initialization count,
/// queues the corresponding startup task, restores NTSC display/audio defaults
/// and installs VBlank driver polling. One-time controller, memory-card, CD and
/// SPU setup must already be complete; prior heap allocations become invalid.
static void _gameMainInitialize(void)
{
    enum {
        GAME_MAIN_RESET_SETTLE_VBLANKS = 10,
        GAME_MAIN_INITIAL_DRAW_BUFFER  = 1,
    };
    s32 drawBufferIndex;

    // End driver activity before discarding callbacks and live allocations.
    SetDispMask(0);
    cdAudioCancelAndWait();
    ResetCallback();
    bootResetCd(BOOT_CD_RESET_DRIVE_AND_VOLUME);
    VSync(GAME_MAIN_RESET_SETTLE_VBLANKS);

    _scratchStackSetCursor(SCRATCH_STACK_CURSOR_SLOT);
    _gGameMainInitializationCount++;
    memConfigureImageMemory(GAME_STAGE_NONE, 0);
    memInitHeaps();
    taskResetDefaultList();
    actorRenderResetLists();
    _gameMainInitGraphics();

    // Reset the full display state after startup work has been queued.
    _displayResetGameState();

    gDisplayPendingFlip = 0;
    gpuClearFrameOrderingTable(0);
    gpuClearFrameOrderingTable(1);
    spuResetSystem();
    sndVolumeSetReducedMode(0);
    bootInitCdAudio();
    VSyncCallback(_displayVSyncCallback);

    drawBufferIndex          = GAME_MAIN_INITIAL_DRAW_BUFFER;
    gDisplayState.drawBuffer = drawBufferIndex;
    displayConfigureFramebuffers(DISPLAY_SETUP_DEFAULT);
    memFillBytes(gGameDebugState, 0, sizeof(*gGameDebugState));
}

void displayPresentTaskFrame(s32 bufferIndex)
{
    s32 flipMode;
    u8  savedDrawBuffer;

    flipMode = gDisplayTaskFlipControlSnapshot & DISPLAY_FLIP_MODE_MASK;
    if (flipMode != DISPLAY_FLIP_HOLD) {
        PutDrawEnv(&gDisplayState.drawEnv[bufferIndex]);
        PutDispEnv(&gDisplayState.dispEnv[bufferIndex]);
        if (flipMode == DISPLAY_FLIP_FULL) {
            if (gDisplayTaskImageSourceSnapshot != DISPLAY_IMAGE_NONE) {
                displayUploadBackgroundImage(bufferIndex);
                // Place the movie in this buffer, then restore the drawing tasks' selector.
                savedDrawBuffer          = gDisplayState.drawBuffer;
                gDisplayState.drawBuffer = bufferIndex;
                streamPresentMovieFrame();
                gDisplayState.drawBuffer = savedDrawBuffer;
            }
            DrawOTag((u_long*)gGpuFrameOrderingTables[gDisplayState.otBuffer].tag);
        } else if (gDisplayTaskImageSourceSnapshot == DISPLAY_IMAGE_TRANSITION_STRIPS) {
            displayUploadBackgroundImage(bufferIndex);
        } else if (gDisplayTaskImageSourceSnapshot == DISPLAY_IMAGE_ROOM_SLOT) {
            gfxRestoreAreaFrame(gGameSession->location.loc.stage, gGameSession->location.loc.area, bufferIndex);
        }
        // The snapshot is read as a signed byte; keep its high-bit behavior.
        if ((s8)gDisplayTaskFlipControlSnapshot < DISPLAY_FLIP_SKIP_TASK_OT) {
            DrawOTag((u_long*)gGpuTaskOrderingTables[bufferIndex].tag);
        }
    }
}

/// Presents one game-loop framebuffer and its resident ordering table.
///
/// `bufferIndex` is 0 or 1 and must also be the current `drawBuffer` for movie
/// placement. Apply its environments, upload the selected background and any
/// available movie, then submit its OT unless `skipDraw` is nonzero. GPU users
/// of reused buffers must have finished; image/movie storage must survive transfer.
static inline void _displayPresentFrame(s32 bufferIndex)
{
    PutDrawEnv(&gDisplayState.drawEnv[bufferIndex]);
    PutDispEnv(&gDisplayState.dispEnv[bufferIndex]);
    if (gDisplayState.control.flags.imageSource != DISPLAY_IMAGE_NONE) {
        displayUploadBackgroundImage(bufferIndex);
    }
    streamPresentMovieFrame();
    if (gDisplayState.skipDraw == 0) {
        DrawOTag((u_long*)gGpuFrameOrderingTables[bufferIndex].tag);
    }
}

/// Presents a due framebuffer and services the resident drivers at vertical blank.
///
/// Registered as the SDK's void(void) VSync callback. A pending buffer is 0 or 1;
/// negative values suppress presentation. The low 16 scanline-counter bits plus
/// prior callback time must exceed half the frame budget before either the game
/// or task presenter runs. An unselected presenter leaves the request pending.
/// Advances the VSync clocks and countdown, then polls CD audio, SPU audio and
/// controller input in that order. Records its elapsed scanlines for frame pacing.
static void _displayVSyncCallback(void)
{
    enum {
        DISPLAY_VSYNC_SCANLINE_MASK = 0xFFFF,
        DISPLAY_FLIP_REQUEST_NONE   = -1,
    };
    s32 startScanlines;

    startScanlines = VSync(1);
    if (gDisplayPendingFlip >= 0) {
        if (((startScanlines & DISPLAY_VSYNC_SCANLINE_MASK) + gDisplayPreviousVSyncElapsedLines) > (gDisplayFrameBudgetLines >> 1)) {
            if (gDisplayState.vsyncFlag == DISPLAY_VSYNC_GAME) {
                _displayPresentFrame(gDisplayPendingFlip);
                gDisplayPendingFlip = DISPLAY_FLIP_REQUEST_NONE;
            } else if (gDisplayState.vsyncFlag == DISPLAY_VSYNC_TASK) {
                displayPresentTaskFrame(gDisplayPendingFlip);
                gDisplayPendingFlip = DISPLAY_FLIP_REQUEST_NONE;
            }
        }
    }
    // Driver polling continues even when no frame is presented or play is paused.
    D_80070F64 -= 1;
    if (gDisplayState.displayOwner == DISPLAY_OWNER_GAME_LOOP) {
        gDisplayState.frameCount += 1;
    }
    gDisplayState.vsyncCount += 1;
    cdAudioPollDriver();
    spuRunVBlankAudioUpdate();
    padPollPort0();
    gDisplayVSyncElapsedLines = VSync(1) - (startScanlines & DISPLAY_VSYNC_SCANLINE_MASK);
}

/// NUL-terminated large-font caption for the controller-disconnect pause screen.
static const u8 _gGameMainPauseText[] = "PAUSE!";

/// Returns 1 when the loading/pause overlay and halted main-loop path are blocked.
///
/// A scene-error halt overrides the CD queue's pause block only. Task-owned
/// frame hold, an unfinished filesystem operation or GPU display blanking still
/// block pausing. Reads live state without changing it; returns 0 when permitted.
static inline s32 _gameMainPauseBlocked(void)
{
    s32 blocked;

    blocked = 0;
    if (gCdCmdQueue.blockGamePause != 0 && !(gGameMainHaltFlags & GAME_MAIN_HALT_SCENE_ERROR)) {
        blocked = 1;
    } else if (gDisplayState.vsyncFlag == DISPLAY_VSYNC_TASK && gDisplayState.control.flags.flipMode == DISPLAY_FLIP_HOLD) {
        blocked = 1;
    } else if (gFsCdOperationStatus != FILE_SYSTEM_CD_OPERATION_COMPLETE) {
        blocked = 1;
    } else if (gpuExtIsDisplayEnabled() == 0) {
        blocked = 1;
    }
    return blocked;
}

/// Darkens the current 320x240 game framebuffer for the pause overlay.
///
/// `tile` and `drawMode` borrow separate word-aligned writable packets. Requires
/// the centered game draw environment with its applied vertical shake. Black
/// average blending halves the framebuffer colour; subtracting the applied
/// shake keeps the rectangle fixed on screen. Submits the mode before the tile
/// synchronously, leaving both packets reusable and average blending active.
static inline void _gameMainDrawPauseBackdrop(TILE* tile, DR_TPAGE* drawMode)
{
    enum {
        GAME_MAIN_PAUSE_WIDTH_PIXELS  = 320,
        GAME_MAIN_PAUSE_HEIGHT_PIXELS = 240,
    };

    setDrawTPage(drawMode, true, true, getTPage(0, GPU_BLEND_AVERAGE, 0, 0));
    DrawPrim(drawMode);
    tile->x0 = -GAME_MAIN_PAUSE_WIDTH_PIXELS / 2;
    tile->w  = GAME_MAIN_PAUSE_WIDTH_PIXELS;
    tile->h  = GAME_MAIN_PAUSE_HEIGHT_PIXELS;
    // The packed black colour clears the command byte; install the tile code next.
    GPU_PRIMITIVE_COLOR_WORD(tile, 0) = 0;
    setTile(tile);
    setSemiTrans(tile, true);
    tile->y0 = -GAME_MAIN_PAUSE_HEIGHT_PIXELS / 2 - gDisplayState.vramYOffset;
    DrawPrim(tile);
}

/// Draws and holds the pause screen when the main-loop pause gates allow it.
///
/// `haltBitIndex` selects a bit in `gGameMainHaltFlags` (0..30 for the signed
/// shift). The current caller selects bit 1 for a disconnected controller; the
/// main loop releases audio ducking when its disconnect-pause condition clears.
/// Any halt bit except bit 0 prevents another pause. Requires the 320x240 game
/// environments and large font to be ready; reuses synchronous private GPU
/// packets without OT storage.
/// Failed gates leave flags, audio and display untouched.
static void _gameMainShowPauseScreen(s32 haltBitIndex)
{
    enum {
        GAME_MAIN_PAUSE_EXEMPT_HALT_MASK  = 1,
        GAME_MAIN_PAUSE_TEXT_BASELINE_Y   = 6,
        GAME_MAIN_PAUSE_TEXT_OT_INDEX     = 4,
        GAME_MAIN_PAUSE_TEXT_COLOR_RGB    = 0x037A78,
        GAME_MAIN_PAUSE_FLIP_REQUEST_NONE = -1,
    };
    TextDrawReq textRequest;
    TILE*       tile;
    DR_TPAGE*   drawMode;
    s32         displayBufferIndex;

    if (!(gGameMainHaltFlags & ~GAME_MAIN_PAUSE_EXEMPT_HALT_MASK)) {
        if (!_gameMainPauseBlocked()) {
            gGameMainHaltFlags |= 1 << haltBitIndex;
            tile                = &_gGameMainPauseTile;
            drawMode            = &_gGameMainPauseDrawMode;
            if (haltBitIndex == GAME_MAIN_HALT_CONTROLLER_DISCONNECT_BIT) {
                sndEvtRequestScriptDuckAcquire();
            }
            // Black average blending dims the current draw buffer; cancel shake.
            _gameMainDrawPauseBackdrop(tile, drawMode);

            textRequest.x          = 0;
            textRequest.otIndex    = GAME_MAIN_PAUSE_TEXT_OT_INDEX;
            textRequest.colorRgb   = GAME_MAIN_PAUSE_TEXT_COLOR_RGB;
            textRequest.glyphTable = TEXT_GLYPH_TABLE_LARGE;
            textRequest.alignment  = TEXT_ALIGNMENT_CENTER;
            textRequest.drawMode   = TEXT_DRAW_IMMEDIATE;
            textRequest.y          = GAME_MAIN_PAUSE_TEXT_BASELINE_Y - gDisplayState.vramYOffset;
            textDrawString(&textRequest, _gGameMainPauseText);

            // Hold the just-drawn image while redirecting drawing to its partner.
            displayBufferIndex = gDisplayState.drawBuffer ^ 1;
            PutDrawEnv(&gDisplayState.drawEnv[displayBufferIndex]);
            PutDispEnv(&gDisplayState.dispEnv[displayBufferIndex]);

            EnterCriticalSection();
            gDisplayPendingFlip = GAME_MAIN_PAUSE_FLIP_REQUEST_NONE;
            ExitCriticalSection();
        }
    }
}

/// Advances the scene's scanline-deadline cursor, retaining its zero terminator.
///
/// `queue` borrows aligned u32 timing words from the live scene reservation.
/// Unless the current word is zero, advances one word and skips at most one
/// following 0xFFFFFFFF marker. Requires the current and any examined following
/// word to be readable, and the resulting cursor to remain usable by scene pacing.
/// Does not change the buffer contents or accumulated scanline time.
static inline void _gameMainAdvanceSceneTimingCursor(CdCmdQueue* queue)
{
    if (*queue->timingCursor != CD_COMMAND_TIMING_END) {
        queue->timingCursor++;
        if (*queue->timingCursor == CD_COMMAND_TIMING_SKIP) {
            queue->timingCursor++;
        }
    }
}

/// Waits for the current scene timing deadline and accounts for this loop's time.
///
/// `frameStartLines` is the VSync(1) scanline-counter origin, possibly negative
/// to compensate for callback time. `elapsedLines` is its already measured
/// nonnegative difference modulo 32768. Returns that difference, remeasured
/// after waiting when needed. Inactive scene pacing leaves timing state intact.
///
/// Active pacing borrows a readable, aligned u32 timing cursor for this call.
/// Deadlines count accumulated scanlines; comparisons and accumulation are
/// unsigned. Advance one deadline, skip at most one following 0xFFFFFFFF word,
/// and stay on a zero terminator while continuing to accumulate elapsed time.
/// Each accessed word must fit the scene's live timing reservation. Deadlines
/// must be reachable before this loop's 15-bit elapsed counter wraps.
static inline s32 _gameMainPaceToSceneTiming(s32 frameStartLines, s32 elapsedLines)
{
    enum { GAME_MAIN_ELAPSED_SCANLINE_MASK = 0x7FFF };
    CdCmdQueue* queue;

    queue = &gCdCmdQueue;
    if (queue->paceToSceneTiming != 0) {
        if (elapsedLines + queue->timingElapsedLines < *queue->timingCursor) {
            while (((VSync(1) - frameStartLines) & GAME_MAIN_ELAPSED_SCANLINE_MASK) + queue->timingElapsedLines < *queue->timingCursor) {
            }
            elapsedLines               = (VSync(1) - frameStartLines) & GAME_MAIN_ELAPSED_SCANLINE_MASK;
            queue->timingElapsedLines += elapsedLines;
        } else {
            queue->timingElapsedLines += elapsedLines;
        }
        // Zero terminates advancement, rather than disabling time accounting.
        _gameMainAdvanceSceneTimingCursor(queue);
    }
    return elapsedLines;
}

/// Begins a game frame in the opposite ordering-table and primitive-buffer halves.
///
/// Requires `otBuffer` to be 0 or 1 and initialized, word-aligned arenas with
/// previous GPU use of the selected halves complete. Counts one animation frame;
/// the play clock adds 1 + (VSync wait argument >> 1) unless CD loading pauses it.
/// The loop clock adds only the pacing term here; its base tick is counted by
/// the caller. These differ from frameTicks. Resets both byte-addressed cursors.
/// The static arena has two equal halves; the heap reservation must also support
/// two aligned halves, with any odd trailing byte excluded by its right shift.
/// Drawing tasks borrow these halves until GPU completion and must fit in them.
static inline void _gameMainBeginFrame(void)
{
    CdCmdQueue* cdQueue;
    s32         bufferIndex;

    bufferIndex               = gDisplayState.otBuffer ^ 1;
    gDisplayState.pendingMode = DISPLAY_MODE_NONE;
    cdQueue                   = &gCdCmdQueue;
    gDisplayState.otBuffer    = bufferIndex;
    gDisplayState.drawBuffer  = gDisplayState.otBuffer;
    // Animation frames, play time and paced loop time use separate clocks.
    gDisplayState.animFrame++;
    if (cdQueue->pausePlayClock == 0) {
        gDisplayState.gameTick += 1 + (gDisplayVSyncWaitMode >> 1);
    }
    gDisplayState.loopTicks += gDisplayVSyncWaitMode >> 1;
    _gpuBeginOt(bufferIndex);

    // Select matching halves of the system-packet and heap-packet arenas.
    gGpuSystemPrimCursor = gDisplayStaticPrimitiveArena + gDisplayState.otBuffer * (s32)(sizeof(gDisplayStaticPrimitiveArena) / 2);
    gGpuPrimCursor       = gGpuPrimitiveBufferBase + gDisplayState.otBuffer * (gGpuPrimitiveBufferBytes >> 1);
}

/// Dispatches resident game and task-owned frames indefinitely.
///
/// Requires initialized drivers, game tasks, framebuffer environments and both
/// OT/primitive buffers. Each iteration resets the scratch stack, ending every
/// reservation from the previous iteration. Game tasks must keep packets within
/// their selected buffer half and retain them until GPU completion before reuse.
///
/// Permitted halts service input, asynchronous callbacks and CD work without
/// advancing loop clocks. Other iterations may hand presentation to the display task
/// list. Game frames advance their own animation/play clocks and alternate OTs;
/// scanline pacing uses 15-bit elapsed counts and compensates for callback time.
/// Restarts rebuild the run while retaining the one-time driver setup and flags.
static void _gameMainRunLoop(void)
{
    enum {
        GAME_MAIN_ELAPSED_SCANLINE_MASK    = 0x7FFF,
        GAME_MAIN_FLIP_REQUEST_NONE        = -1,
        GAME_MAIN_FLIP_REQUEST_IMMEDIATE   = -2,
        GAME_MAIN_VSYNC_WAIT_NEXT          = 0,
        GAME_MAIN_VSYNC_SCANLINE_COUNTER   = 1,
        GAME_MAIN_GPU_WAIT_COMPLETE        = 0,
        GAME_MAIN_GRAPH_RESET_QUEUE        = 1,
        GAME_MAIN_FRAMEBUFFER_FULL_HEIGHT  = 480,
        GAME_MAIN_FRAMEBUFFER_HALF_HEIGHT  = 240,
        GAME_MAIN_FRAMEBUFFER_VERTICAL_GAP = 32,
    };
    s32 frameStartLines;
    s32 elapsedLines;
    s8  pendingShakeY;

    frameStartLines        = 0;
    gGameMainHaltFlags     = 0;
    gDisplayState.otBuffer = gDisplayState.drawBuffer;

    for (;;) {
        if (gDisplayState.gameMode == DISPLAY_GAME_RESTART ||
            (gDisplayState.gameMode == DISPLAY_GAME_ACTIVE && gDisplayState.cdBusy == DISPLAY_CD_IDLE && gDisplayState.gameRunning != 0 &&
             padCheckSoftResetCombo() != 0)) {
            _gameMainInitialize();
            gGameMainHaltFlags = 0;
        }
        // Scratch borrowers cannot carry a reservation into the next iteration.
        _scratchStackSetCursor(SCRATCH_STACK_CURSOR_SLOT);
        padUpdatePort0();

        if (gPadStates[0].inputFormat == PAD_INPUT_FORMAT_UNAVAILABLE && gPadStates[0].inputBlockPolls == 0 && gDisplayState.gameRunning != 0 &&
            gDisplayState.suppressDisconnectPause == 0 && gDisplayState.gameMode == DISPLAY_GAME_ACTIVE) {
            _gameMainShowPauseScreen(GAME_MAIN_HALT_CONTROLLER_DISCONNECT_BIT);
        } else if (gGameMainHaltFlags & GAME_MAIN_HALT_CONTROLLER_DISCONNECT) {
            sndEvtRequestScriptDuckRelease();
            gGameMainHaltFlags &= ~GAME_MAIN_HALT_CONTROLLER_DISCONNECT;
        }

        asyncCbPollMainLoop(0);

        if (gGameMainHaltFlags != 0 && !_gameMainPauseBlocked()) {
            VSync(GAME_MAIN_VSYNC_WAIT_NEXT);
            frameStartLines = VSync(GAME_MAIN_VSYNC_SCANLINE_COUNTER) & GAME_MAIN_ELAPSED_SCANLINE_MASK;
            cdCmdService();
            continue;
        }

        // Clocks differ: CD file loads freeze play time, while paced loop time continues.
        gDisplayState.loopCount++;
        gDisplayState.loopTicks++;
        if (gDisplayState.displayOwner == DISPLAY_OWNER_GAME_LOOP && gDisplayState.pendingMode != DISPLAY_MODE_NONE &&
            ((s8)gDisplayState.pendingMode < DISPLAY_MODE_NONE || gDisplayState.holdState >= 0)) {
            displayDispatchModeRequest(gDisplayState.pendingMode);
        }
        if (gDisplayState.displayOwner != DISPLAY_OWNER_GAME_LOOP) {
            frameStartLines = displayRunTaskFrame(gGpuFrameOrderingTables, frameStartLines, gDisplayState.otBuffer);
            continue;
        }

        _gameMainBeginFrame();
        taskExecDefaultList();

        if (gDisplayState.displayOwner != DISPLAY_OWNER_GAME_LOOP) {
            continue;
        }

        cdCmdService();

        // A 480-line frame shares one VRAM region and bypasses timed flips.
        if (gDisplayState.height == GAME_MAIN_FRAMEBUFFER_FULL_HEIGHT) {
            gDisplayPendingFlip = GAME_MAIN_FLIP_REQUEST_NONE;
            VSync(GAME_MAIN_VSYNC_WAIT_NEXT);
            ResetGraph(GAME_MAIN_GRAPH_RESET_QUEUE);
            _displayPresentFrame(gDisplayState.otBuffer);
            frameStartLines = 0;
            continue;
        }

        DrawSync(GAME_MAIN_GPU_WAIT_COMPLETE);
        elapsedLines = _gameMainPaceToSceneTiming(frameStartLines, (VSync(GAME_MAIN_VSYNC_SCANLINE_COUNTER) - frameStartLines) & GAME_MAIN_ELAPSED_SCANLINE_MASK);

        // The callback consumes timely flips; late or unconsumed frames submit here.
        if (elapsedLines < gDisplayFrameBudgetLines) {
            gDisplayState.vsyncFlag = DISPLAY_VSYNC_GAME;
            gDisplayPendingFlip     = gDisplayState.otBuffer;
            VSync(gDisplayVSyncWaitMode);
            if (gDisplayPendingFlip != GAME_MAIN_FLIP_REQUEST_NONE) {
                gDisplayPreviousVSyncElapsedLines = 0;
                frameStartLines                   = VSync(GAME_MAIN_VSYNC_SCANLINE_COUNTER) & GAME_MAIN_ELAPSED_SCANLINE_MASK;
                _displayPresentFrame(gDisplayState.otBuffer);
                gDisplayPendingFlip = GAME_MAIN_FLIP_REQUEST_NONE;
            } else {
                gDisplayPreviousVSyncElapsedLines = gDisplayVSyncElapsedLines;
                frameStartLines                   = -gDisplayVSyncElapsedLines;
            }
        } else {
            gDisplayState.vsyncFlag           = DISPLAY_VSYNC_GAME;
            gDisplayPendingFlip               = GAME_MAIN_FLIP_REQUEST_IMMEDIATE;
            gDisplayPreviousVSyncElapsedLines = 0;
            frameStartLines                   = VSync(GAME_MAIN_VSYNC_SCANLINE_COUNTER) & GAME_MAIN_ELAPSED_SCANLINE_MASK;
            _displayPresentFrame(gDisplayState.otBuffer);
            gDisplayPendingFlip = GAME_MAIN_FLIP_REQUEST_NONE;
        }

        // Apply the request to the next image crop and both centered draw origins.
        pendingShakeY                   = gDisplayState.shakeY;
        gDisplayState.vramYOffset       = pendingShakeY;
        gDisplayState.drawEnv[0].ofs[1] = pendingShakeY + GAME_MAIN_FRAMEBUFFER_HALF_HEIGHT / 2;
        gDisplayState.drawEnv[1].ofs[1] = pendingShakeY + GAME_MAIN_FRAMEBUFFER_HALF_HEIGHT + GAME_MAIN_FRAMEBUFFER_HALF_HEIGHT / 2 + GAME_MAIN_FRAMEBUFFER_VERTICAL_GAP;
    }
}

void gfxResetView(void)
{
    enum {
        GRAPHICS_DEFAULT_VIEW_OFFSET_Z       = 0x8000,
        GRAPHICS_DEFAULT_PROJECTION_DISTANCE = 0x400,
    };

    /// Resets a view node's local transform and parent, and invalidates its cache.
    ///
    /// `node` is a writable GfxCoord lvalue without side effects, used repeatedly.
    /// `parentCoord` and `offsetZ` are evaluated once; the parent is borrowed and
    /// the signed Z offset counts game-coordinate units. X and Y become zero.
    /// Uses `gfxSetRotIdentity` and `GRAPHICS_COORD_DIRTY`; captures no locals.
    /// The node remains caller-owned; its parent's lifetime must cover later
    /// composition. The cached composed matrix is retained until rebuilt.
    /// Expands to a compound statement; use standalone in a braced body.
    /// Defined only around this three-node reset.
#define GRAPHICS_RESET_VIEW_COORD(node, parentCoord, offsetZ) \
    {                                                         \
        gfxSetRotIdentity(&(node).coord);                     \
        (node).parent       = (parentCoord);                  \
        (node).coord.t[0]   = 0;                              \
        (node).coord.t[1]   = 0;                              \
        (node).coord.t[2]   = (offsetZ);                      \
        (node).composeStamp = GRAPHICS_COORD_DIRTY;           \
    }

    // World nodes inherit view translation, then view rotation, then the root offset.
    GRAPHICS_RESET_VIEW_COORD(gGfxViewOffsetCoord, NULL, GRAPHICS_DEFAULT_VIEW_OFFSET_Z);
    GRAPHICS_RESET_VIEW_COORD(gGfxViewRotCoord, &gGfxViewOffsetCoord, 0);
    GRAPHICS_RESET_VIEW_COORD(gGfxViewCoord, &gGfxViewRotCoord, 0);
#undef GRAPHICS_RESET_VIEW_COORD

    gte_SetGeomScreen(GRAPHICS_DEFAULT_PROJECTION_DISTANCE);

    gfxSetRotIdentity(&GsWSMATRIX);
}

void displayUploadBackgroundImage(s32 bufferIndex)
{
    enum {
        DISPLAY_BACKGROUND_BUFFER_STRIDE    = 272,
        DISPLAY_BACKGROUND_STRIP_ROW_BYTES  = FILE_SYSTEM_IMAGE_STRIP_WIDTH * 2,
        DISPLAY_BACKGROUND_STRIP_INDEX_MASK = 0xFFFF,
    };
    RECT rect;
    s32  framebufferY = bufferIndex;
    s32  stripIndex;
    s32  sourceRowOffsetBytes;
    s32  stripIndex16;
    s32  offsetY;
    s8   offsetYByte;

    // Crop contiguous pixels at the top or bottom without extending the image workspace.
    if (gCdCmdQueue.imageLayout == FILE_SYSTEM_IMAGE_CONTIGUOUS) {
        if (gDisplayState.vramYOffset >= 0) {
            if (bufferIndex != 0) {
                rect.y = gDisplayState.vramYOffset + DISPLAY_BACKGROUND_BUFFER_STRIDE;
            } else {
                rect.y = gDisplayState.vramYOffset;
            }
            rect.x = 0;
            rect.w = FILE_SYSTEM_IMAGE_WIDTH;
            rect.h = FILE_SYSTEM_IMAGE_HEIGHT - gDisplayState.vramYOffset;
            LoadImage(&rect, gFsImageWorkspacePtr->strips[0]);
            return;
        }
        if (bufferIndex == 0) {
            rect.y = 0;
        } else {
            rect.y = DISPLAY_BACKGROUND_BUFFER_STRIDE;
        }
        rect.x      = 0;
        rect.w      = FILE_SYSTEM_IMAGE_WIDTH;
        offsetYByte = gDisplayState.vramYOffset;
        rect.h      = offsetYByte + FILE_SYSTEM_IMAGE_HEIGHT;
        // Contiguous rows span the workspace's nominal column boundaries.
        LoadImage(&rect, (u_long*)((u8*)gFsImageWorkspacePtr + ((-offsetYByte) * FILE_SYSTEM_IMAGE_ROW_BYTES)));
        return;
    }
    sourceRowOffsetBytes = 0;
    // Each strip is one 16-pixel column of the image workspace.
    if (gCdCmdQueue.imageLayout == FILE_SYSTEM_IMAGE_STRIPS) {
        framebufferY *= DISPLAY_BACKGROUND_BUFFER_STRIDE;
        rect.w        = FILE_SYSTEM_IMAGE_STRIP_WIDTH;
        offsetY       = gDisplayState.vramYOffset;
        rect.y        = framebufferY;
        rect.h        = FILE_SYSTEM_IMAGE_HEIGHT;
        if (offsetY > 0) {
            rect.h -= offsetY;
            rect.y  = framebufferY + offsetY;
        } else if (offsetY < 0) {
            sourceRowOffsetBytes = (-offsetY) * DISPLAY_BACKGROUND_STRIP_ROW_BYTES;
            rect.h               = offsetY + FILE_SYSTEM_IMAGE_HEIGHT;
        }
        stripIndex = 0;
        do {
            stripIndex16 = stripIndex & DISPLAY_BACKGROUND_STRIP_INDEX_MASK;
            rect.x       = stripIndex16 * FILE_SYSTEM_IMAGE_STRIP_WIDTH;
            LoadImage(&rect, (u_long*)((u8*)gFsImageWorkspacePtr->strips[stripIndex16] + sourceRowOffsetBytes));
            stripIndex++;
        } while ((u32)(stripIndex & DISPLAY_BACKGROUND_STRIP_INDEX_MASK) < (u32)ARRAY_SIZE(gFsImageWorkspacePtr->strips));
    }
}

void displaySetFrameTiming(s32 timingMode)
{
    enum {
        DISPLAY_FRAME_LINES_ONE   = 262,
        DISPLAY_FRAME_LINES_TWO   = 525,
        DISPLAY_FRAME_LINES_THREE = 787,
    };

    // Keep animation ticks, the SDK wait argument and scanline budgets distinct.
    if (timingMode == DISPLAY_TIMING_EVERY_VBLANK) {
        gDisplayState.frameTicks = 1;
        gDisplayVSyncWaitMode    = 0;
        gDisplayFrameBudgetLines = DISPLAY_FRAME_LINES_ONE;
    } else if (timingMode == DISPLAY_TIMING_TWO_VBLANKS) {
        gDisplayState.frameTicks = 2;
        gDisplayVSyncWaitMode    = 2;
        gDisplayFrameBudgetLines = DISPLAY_FRAME_LINES_TWO;
    } else if (timingMode == DISPLAY_TIMING_THREE_VBLANKS) {
        gDisplayState.frameTicks = 3;
        gDisplayVSyncWaitMode    = 3;
        gDisplayFrameBudgetLines = DISPLAY_FRAME_LINES_THREE;
    }
}

void gpuClearFrameOrderingTable(s16 bufferIndex)
{
    u_long* tableStart = gGpuFrameOtTags[bufferIndex];
    ClearOTagR(tableStart, GPU_ORDERING_TABLE_BUFFER_ENTRIES);
    // End at the first frame tag instead of following the SDK's trailing packets.
    *tableStart = GPU_DMA_CHAIN_END;
}

/// Binds both game ordering-table descriptors to their resident DMA tag buffers.
///
/// Each 1088-word buffer starts at `org` and submits from its final word, with
/// ten-bit sorting depth. The extra tags surround the 1024 depth-sorting slots;
/// the SDK bitfield pointers view the same packed words used by libgpu. Prior
/// GPU users must have finished. Leaves descriptor offset/point fields intact;
/// allocates no storage, clears no tags and selects no current ordering table.
static inline void _gpuInitGameOrderingTableDescriptors(void)
{
    GsOT*   orderingTables;
    u_long* tags;
    s32     depthBits;

    orderingTables           = gGpuFrameOrderingTables;
    depthBits                = GPU_ORDERING_TABLE_DEPTH_BITS;
    orderingTables->length   = depthBits;
    tags                     = gGpuFrameOtTags[0];
    orderingTables->tag      = (GsOT_TAG*)(tags + GPU_ORDERING_TABLE_BUFFER_ENTRIES - 1);
    orderingTables->org      = (GsOT_TAG*)tags;
    orderingTables[1].length = depthBits;
    orderingTables[1].org    = (GsOT_TAG*)gGpuFrameOtTags[1];
    orderingTables[1].tag    = (GsOT_TAG*)(gGpuFrameOtTags[1] + ARRAY_SIZE(gGpuFrameOtTags[1]) - 1);
}

/// Restores GPU/GTE and resident view defaults and queues game startup.
///
/// Called after the initialization count advances and the default task list is
/// reset. Resets the GPU only on the first initialization, clears the 320x512
/// VRAM framebuffer strip synchronously and prepares both game OT descriptors.
/// The caller subsequently clears display state and ordering-table contents.
static void _gameMainInitGraphics(void)
{
    enum {
        GAME_MAIN_GRAPH_RESET_FULL        = 0,
        GAME_MAIN_FRAMEBUFFER_STRIP_WIDTH = 320,
        GAME_MAIN_VRAM_HEIGHT_PIXELS      = 512,
    };
    RECT framebufferStrip;

    if (_gGameMainInitializationCount == GAME_MAIN_FIRST_INITIALIZATION) {
        ResetGraph(GAME_MAIN_GRAPH_RESET_FULL);
    }

    framebufferStrip.x = 0;
    framebufferStrip.y = 0;
    framebufferStrip.w = GAME_MAIN_FRAMEBUFFER_STRIP_WIDTH;
    framebufferStrip.h = GAME_MAIN_VRAM_HEIGHT_PIXELS;
    ClearImage(&framebufferStrip, 0, 0, 0);
    DrawSync(0);
    InitGeom();

    // The tables exceed `1 << length` entries, so `tag` is set here rather than by `GsClearOt`.
    _gpuInitGameOrderingTableDescriptors();
    _gameMainSpawnStartupTask();
    gfxResetView();
    gfxResetDefaultLights();
    gDisplayState.control.flags.imageSource = DISPLAY_IMAGE_NONE;
}

/// Queues the first-boot loader or the shorter title reload used after resets.
///
/// Uses bank 0 of the default task list, which must already be reset and have
/// capacity. Initialization count 1 selects ISO/header discovery and the boot
/// presentation; other counts reload the title directly. Supplies zero spawn
/// arguments and leaves ownership of the returned task with the task system.
static void _gameMainSpawnStartupTask(void)
{
    enum {
        GAME_MAIN_STARTUP_TASK_BANK         = 0,
        GAME_MAIN_STARTUP_TASK_FIRST_BOOT   = 0x1F,
        GAME_MAIN_STARTUP_TASK_RELOAD_TITLE = 0x20,
    };

    if (_gGameMainInitializationCount == GAME_MAIN_FIRST_INITIALIZATION) {
        taskSpawn(GAME_MAIN_STARTUP_TASK_BANK, GAME_MAIN_STARTUP_TASK_FIRST_BOOT, 0, 0);
    } else {
        taskSpawn(GAME_MAIN_STARTUP_TASK_BANK, GAME_MAIN_STARTUP_TASK_RELOAD_TITLE, 0, 0);
    }
}

/// Standalone game-frame presenter retained in the resident image.
///
/// Follows `_displayPresentFrame`'s buffer and transfer-lifetime contract.
/// No current code calls this entry; it shares the inline presenter's behavior.
static void _displayPresentGameFrame(s32 bufferIndex)
{
    _displayPresentFrame(bufferIndex);
}

void gameMainRun(void)
{
    // Establish scratch and hardware drivers before building any game tasks.
    _scratchStackSetCursor(SCRATCH_STACK_CURSOR_SLOT);
    ResetCallback();
    SetVideoMode(MODE_NTSC);
    spuInitSystem();
    mcInit();
    padInit();
    bootInitCd();
    // Only power-on startup clears the state retained across soft resets.
    memFillBytes(&gGameMainPersistentState, 0, sizeof(gGameMainPersistentState));
    _gGameMainInitializationCount = 0;
    _gameMainInitialize();
    _gameMainRunLoop();
}

u32 gameMainGetInitializationCount(void)
{
    return _gGameMainInitializationCount;
}
