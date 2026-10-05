#include "gamemain.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libapi.h>
#include <psyq/libetc.h>
#include <psyq/libgs.h>

#include "types.h"

#include "boot.h"
#include "cdaudio.h"
#include "main/coord.h"
#include "main/display_types.h"
#include "display_types.h"
#include "main/fs.h"
#include "main/fs_types.h"
#include "gameflow.h"
#include "gfx.h"
#include "gpuext.h"
#include "mc.h"
#include "main/mem.h"
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

enum {
    DISPLAY_BACKGROUND_WIDTH           = 320,
    DISPLAY_BACKGROUND_HEIGHT          = 240,
    DISPLAY_BACKGROUND_BUFFER_STRIDE   = 272,
    DISPLAY_BACKGROUND_ROW_BYTES       = 640,
    DISPLAY_BACKGROUND_STRIP_WIDTH     = 16,
    DISPLAY_BACKGROUND_STRIP_ROW_BYTES = 32,
    DISPLAY_BACKGROUND_STRIP_COUNT     = 20U,
};

#define GameResetScratchHead() *SCRATCH_STACK_CURSOR_SLOT = SCRATCH_STACK_CURSOR_SLOT

/* Define BSS before API headers to preserve first-declaration order. */
/// Immediate-mode TILE / DR_TPAGE scratch for the "now loading" overlay.
static TILE D_8006EC18;

static DR_TPAGE D_8006EC28;

volatile u8 D_8006EC30;

u_long Gpu_OtTags[2 * GPU_ORDERING_TABLE_BUFFER_ENTRIES];

volatile u8 D_80070E38;

GfxCoord gGfxViewRotCoord;

GfxCoord Gfx_ViewOffsetCoord;

u8* Gpu_SysPrimCursor;

GsOT Gpu_OtBuffers[2];

GfxCoord gGfxViewCoord;

u32 gRandomLcgState;

static volatile s32 D_80070F64;

DisplayState gDisplayState;

u_long* gGpuCurrentOt;

GameMainPersistentState Wip_SysFlags;

/// Unreferenced.
static u8 D_800710C8[0x50];

#include "main/display.h"
#include "display.h"
#include "main/gamemain.h"
#include "main/gfx.h"
#include "main/random.h"
#include "main/wipsys.h"

static u32 D_8005EC64;

/// Unreferenced.
static s32 D_8005EC7C;

/// Unreferenced.
static u32 D_8005EC84[4];

// Drawn by GameMain_ShowLoading (must stay in .rodata for this TU).
/// "PAUSE!" overlay text for GameMain_ShowLoading (@ VA 0x80013404).
static const u8 GameMain_PauseText[];

static void GameMain_Init(void);

/// Puts buffer `buf`'s draw and display environments, uploads the image the
/// flip wants in it, and draws that buffer's ordering table unless drawing is
/// suppressed.
static inline void _displayPresentFrame(s32 buf);

/// VSync callback: timed flip / strip load / audio tick (gamemain.c).
static void Display_VSyncCallback(void);

/// Nonzero while the game cannot be halted: a CD command is running without
/// the halt flag that permits it, the flip is holding the displayed frame, a CD
/// operation is in progress, or the display is off.
static inline s32 _gameMainPauseBlocked(void);

static void GameMain_ShowLoading(s32 arg0);

/// Holds the frame back until the stream's timing table allows it: while the
/// table is active, waits until the accumulated time reaches the current entry,
/// adds this frame's time and advances the cursor (skipping -1 entries, stopping
/// at 0). Returns the frame's elapsed time, remeasured if it waited.
static inline s32 _gameMainPaceToStream(s32 start, s32 elapsed);

static void GameMain_Loop(void);

static void Gfx_InitGraph(void);

static void GameMain_SpawnBootTask(void);

static void Display_PutEnvAndDraw(s32 arg0);

static u32   D_8005EC64          = 0;
s32          D_8005EC68          = 0;
s32          D_8005EC6C          = 0xF0;
volatile s32 Display_PendingFlip = 0;
volatile s32 D_8005EC74          = 0;
volatile s32 D_8005EC78          = 0;
/// Unreferenced.
static s32   D_8005EC7C         = 0;
volatile s32 GameMain_HaltFlags = 0;
/// Unreferenced.
static u32 D_8005EC84[4] = { 0, 0x01FF03FF, 0, 0 };

// VSync countdown

static void GameMain_Init(void)
{
    s32 flag; // The indirection is required.

    SetDispMask(0);
    Boot_WaitCdAudioReady();
    ResetCallback();
    Boot_ResetCd(1);
    VSync(10);

    GameResetScratchHead();
    D_8005EC64++;
    memConfigureImageMemory(GAME_STAGE_NONE, 0);
    Mem_Init();
    taskResetDefaultList();
    Tmd_InitLists();
    Gfx_InitGraph();

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

    Display_PendingFlip = 0;
    gpuClearFrameOrderingTable(0);
    gpuClearFrameOrderingTable(1);
    Spu_WaitDma();
    Snd_SetMutedVolumes(0);
    Boot_InitCdAudio();
    VSyncCallback(Display_VSyncCallback);

    flag                     = 1;
    gDisplayState.drawBuffer = flag;
    Display_SetMode(DISPLAY_SETUP_DEFAULT);
    memFillBytes(Pad_RemapState, 0, sizeof(*Pad_RemapState));
}

void Display_FlipDraw(s32 bufferIndex)
{
    s32 mode;
    u8  savedDrawBuffer;

    mode = D_80070E38 & DISPLAY_FLIP_MODE_MASK;
    if (mode != DISPLAY_FLIP_HOLD) {
        PutDrawEnv(&gDisplayState.drawEnv[bufferIndex]);
        PutDispEnv(&gDisplayState.dispEnv[bufferIndex]);
        if (mode == DISPLAY_FLIP_FULL) {
            if (D_8006EC30 != DISPLAY_IMAGE_NONE) {
                Display_LoadImageStrips(bufferIndex);
                savedDrawBuffer          = gDisplayState.drawBuffer;
                gDisplayState.drawBuffer = bufferIndex;
                Stream_PresentFrame();
                gDisplayState.drawBuffer = savedDrawBuffer;
            }
            DrawOTag((u_long*)Gpu_OtBuffers[gDisplayState.otBuffer].tag);
        } else if (D_8006EC30 == DISPLAY_IMAGE_TRANSITION_STRIPS) {
            Display_LoadImageStrips(bufferIndex);
        } else if (D_8006EC30 == DISPLAY_IMAGE_ROOM_SLOT) {
            gfxRestoreAreaFrame(gGameSession->location.loc.stage, gGameSession->location.loc.area, bufferIndex);
        }
        if ((s8)D_80070E38 < DISPLAY_FLIP_SKIP_TASK_OT) {
            DrawOTag(Gpu_OrderingTables[bufferIndex].tag);
        }
    }
}

/// Puts buffer `buf`'s draw and display environments, uploads the image the
/// flip wants in it, and draws that buffer's ordering table unless drawing is
/// suppressed.
static inline void _displayPresentFrame(s32 buf)
{
    PutDrawEnv(&gDisplayState.drawEnv[buf]);
    PutDispEnv(&gDisplayState.dispEnv[buf]);
    if (gDisplayState.control.flags.imageSource != DISPLAY_IMAGE_NONE) {
        Display_LoadImageStrips(buf);
    }
    Stream_PresentFrame();
    if (gDisplayState.skipDraw == 0) {
        DrawOTag((u_long*)Gpu_OtBuffers[buf].tag);
    }
}

/// VSync callback: timed flip / strip load / audio tick (gamemain.c).
static void Display_VSyncCallback(void)
{
    s32 start;

    start = VSync(1);
    if (Display_PendingFlip >= 0) {
        if (((start & 0xFFFF) + D_8005EC78) > (D_8005EC6C >> 1)) {
            if (gDisplayState.vsyncFlag == DISPLAY_VSYNC_GAME) {
                _displayPresentFrame(Display_PendingFlip);
                Display_PendingFlip = -1;
            } else if (gDisplayState.vsyncFlag == DISPLAY_VSYNC_TASK) {
                Display_FlipDraw(Display_PendingFlip);
                Display_PendingFlip = -1;
            }
        }
    }
    D_80070F64 -= 1;
    if (gDisplayState.displayOwner == DISPLAY_OWNER_GAME_LOOP) {
        gDisplayState.frameCount += 1;
    }
    gDisplayState.vsyncCount += 1;
    CdAudio_Tick();
    Audio_IrqFrameWork();
    Pad_PollControllers();
    D_8005EC74 = VSync(1) - (start & 0xFFFF);
}

// Drawn by GameMain_ShowLoading (must stay in .rodata for this TU).
/// "PAUSE!" overlay text for GameMain_ShowLoading (@ VA 0x80013404).
static const u8 GameMain_PauseText[] = "PAUSE!";

/// Nonzero while the game cannot be halted: a CD command is running without
/// the halt flag that permits it, the flip is holding the displayed frame, a CD
/// operation is in progress, or the display is off.
static inline s32 _gameMainPauseBlocked(void)
{
    s32 blocked;

    blocked = 0;
    if (gCdCmdQueue.blockGamePause != 0 && !(GameMain_HaltFlags & 8)) {
        blocked = 1;
    } else if (gDisplayState.vsyncFlag == DISPLAY_VSYNC_TASK && gDisplayState.control.flags.flipMode == DISPLAY_FLIP_HOLD) {
        blocked = 1;
    } else if (Fs_CdOpStatus != 0xFF) {
        blocked = 1;
    } else if (GpuExt_IsDisplayEnabled() == 0) {
        blocked = 1;
    }
    return blocked;
}

static void GameMain_ShowLoading(s32 arg0)
{
    TextDrawReq req;
    TILE*       tile;
    DR_TPAGE*   dr;
    s32         buf;

    if (!(GameMain_HaltFlags & ~1)) {
        if (!_gameMainPauseBlocked()) {
            GameMain_HaltFlags |= 1 << arg0;
            tile                = &D_8006EC18;
            dr                  = &D_8006EC28;
            if (arg0 == 1) {
                SndEvt_EnqueueTypeD();
            }
            setlen(dr, 1);
            dr->code[0] = 0xE1000600;
            DrawPrim(dr);

            tile->x0                          = -0xA0;
            tile->w                           = 0x140;
            tile->h                           = 0xF0;
            GPU_PRIMITIVE_COLOR_WORD(tile, 0) = 0;
            setlen(tile, 3);
            setcode(tile, 0x62);
            tile->y0 = -0x78 - gDisplayState.vramYOffset;
            DrawPrim(tile);

            req.x          = 0;
            req.otIndex    = 4;
            req.colorRgb   = 0x37A78;
            req.glyphTable = TEXT_GLYPH_TABLE_LARGE;
            req.alignment  = TEXT_ALIGNMENT_CENTER;
            req.drawMode   = TEXT_DRAW_IMMEDIATE;
            req.y          = 6 - gDisplayState.vramYOffset;
            textDrawString(&req, GameMain_PauseText);

            buf = gDisplayState.drawBuffer ^ 1;
            PutDrawEnv(&gDisplayState.drawEnv[buf]);
            PutDispEnv(&gDisplayState.dispEnv[buf]);

            EnterCriticalSection();
            Display_PendingFlip = -1;
            ExitCriticalSection();
        }
    }
}

/// Holds the frame back until the stream's timing table allows it: while the
/// table is active, waits until the accumulated time reaches the current entry,
/// adds this frame's time and advances the cursor (skipping -1 entries, stopping
/// at 0). Returns the frame's elapsed time, remeasured if it waited.
static inline s32 _gameMainPaceToStream(s32 start, s32 elapsed)
{
    CdCmdQueue* q;

    q = &gCdCmdQueue;
    if (q->paceToSceneTiming != 0) {
        if (elapsed + q->timingElapsedLines < *q->timingCursor) {
            while (((VSync(1) - start) & 0x7FFF) + q->timingElapsedLines < *q->timingCursor) {
            }
            elapsed                = (VSync(1) - start) & 0x7FFF;
            q->timingElapsedLines += elapsed;
        } else {
            q->timingElapsedLines += elapsed;
        }
        if (*q->timingCursor != CD_COMMAND_TIMING_END) {
            q->timingCursor++;
            if (*q->timingCursor == CD_COMMAND_TIMING_SKIP) {
                q->timingCursor++;
            }
        }
    }
    return elapsed;
}

static void GameMain_Loop(void)
{
    CdCmdQueue* cdQueue;
    s32         frameStart;
    s32         gameBuffer;
    s32         elapsedLines;
    s8          pendingShakeY;

    frameStart             = 0;
    GameMain_HaltFlags     = 0;
    gDisplayState.otBuffer = gDisplayState.drawBuffer;

    for (;;) {
        if (gDisplayState.gameMode == DISPLAY_GAME_RESTART ||
            (gDisplayState.gameMode == DISPLAY_GAME_ACTIVE && gDisplayState.cdBusy == DISPLAY_CD_IDLE && gDisplayState.gameRunning != 0 &&
             Pad_CheckSpecialCombo() != 0)) {
            GameMain_Init();
            GameMain_HaltFlags = 0;
        }
        GameResetScratchHead();
        Pad_UpdatePort0();

        if (gPadStates[0].inputFormat == PAD_INPUT_FORMAT_UNAVAILABLE && gPadStates[0].inputBlockPolls == 0 && gDisplayState.gameRunning != 0 &&
            gDisplayState.suppressDisconnectPause == 0 && gDisplayState.gameMode == DISPLAY_GAME_ACTIVE) {
            GameMain_ShowLoading(1);
        } else if (GameMain_HaltFlags & 2) {
            SndEvt_EnqueueTypeE();
            GameMain_HaltFlags &= ~2;
        }

        Snd_PollAsync(0);

        if (GameMain_HaltFlags != 0 && !_gameMainPauseBlocked()) {
            VSync(0);
            frameStart = VSync(1) & 0x7FFF;
            Boot_DispatchCdCmd();
            continue;
        }

        // Clocks differ: CD file loads freeze play time, while paced loop time continues.
        gDisplayState.loopCount++;
        gDisplayState.loopTicks++;
        if (gDisplayState.displayOwner == DISPLAY_OWNER_GAME_LOOP && gDisplayState.pendingMode != DISPLAY_MODE_NONE &&
            ((s8)gDisplayState.pendingMode < DISPLAY_MODE_NONE || gDisplayState.holdState >= 0)) {
            Display_DispatchModeId(gDisplayState.pendingMode);
        }
        if (gDisplayState.displayOwner != DISPLAY_OWNER_GAME_LOOP) {
            frameStart = Display_FrameFlipDraw(Gpu_OtBuffers, frameStart, gDisplayState.otBuffer);
            continue;
        }

        gameBuffer                = gDisplayState.otBuffer ^ 1;
        gDisplayState.pendingMode = DISPLAY_MODE_NONE;
        cdQueue                   = &gCdCmdQueue;
        gDisplayState.otBuffer    = gameBuffer;
        gDisplayState.drawBuffer  = gDisplayState.otBuffer;
        gDisplayState.animFrame++;
        if (cdQueue->pausePlayClock == 0) {
            gDisplayState.gameTick += 1 + (D_8005EC68 >> 1);
        }
        gDisplayState.loopTicks += D_8005EC68 >> 1;
        gpuBeginOt(gameBuffer);

        Gpu_SysPrimCursor = Gpu_PrimBufStatic + gDisplayState.otBuffer * (s32)(sizeof(Gpu_PrimBufStatic) / 2);
        gGpuPrimCursor    = Gpu_PrimHeapBase + gDisplayState.otBuffer * (Gpu_PrimHeapSize >> 1);
        taskExecDefaultList();

        if (gDisplayState.displayOwner != DISPLAY_OWNER_GAME_LOOP) {
            continue;
        }

        Boot_DispatchCdCmd();

        if (gDisplayState.height == 480) {
            Display_PendingFlip = -1;
            VSync(0);
            ResetGraph(1);
            _displayPresentFrame(gDisplayState.otBuffer);
            frameStart = 0;
            continue;
        }

        DrawSync(0);
        elapsedLines = _gameMainPaceToStream(frameStart, (VSync(1) - frameStart) & 0x7FFF);

        if (elapsedLines < D_8005EC6C) {
            gDisplayState.vsyncFlag = DISPLAY_VSYNC_GAME;
            Display_PendingFlip     = gDisplayState.otBuffer;
            VSync(D_8005EC68);
            if (Display_PendingFlip != -1) {
                D_8005EC78 = 0;
                frameStart = VSync(1) & 0x7FFF;
                _displayPresentFrame(gDisplayState.otBuffer);
                Display_PendingFlip = -1;
            } else {
                D_8005EC78 = D_8005EC74;
                frameStart = -D_8005EC74;
            }
        } else {
            gDisplayState.vsyncFlag = DISPLAY_VSYNC_GAME;
            Display_PendingFlip     = -2;
            D_8005EC78              = 0;
            frameStart              = VSync(1) & 0x7FFF;
            _displayPresentFrame(gDisplayState.otBuffer);
            Display_PendingFlip = -1;
        }

        pendingShakeY                   = gDisplayState.shakeY;
        gDisplayState.vramYOffset       = pendingShakeY;
        gDisplayState.drawEnv[0].ofs[1] = pendingShakeY + 0x78;
        gDisplayState.drawEnv[1].ofs[1] = pendingShakeY + 0x188;
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
    /// Expands to several statements; use only at these standalone call sites.
#define GRAPHICS_RESET_VIEW_COORD(node, parentCoord, offsetZ) \
    gfxSetRotIdentity(&(node).coord);                         \
    (node).parent       = (parentCoord);                      \
    (node).coord.t[0]   = 0;                                  \
    (node).coord.t[1]   = 0;                                  \
    (node).coord.t[2]   = (offsetZ);                          \
    (node).composeStamp = GRAPHICS_COORD_DIRTY

    // World nodes inherit view translation, then view rotation, then the root offset.
    GRAPHICS_RESET_VIEW_COORD(Gfx_ViewOffsetCoord, NULL, GRAPHICS_DEFAULT_VIEW_OFFSET_Z);
    GRAPHICS_RESET_VIEW_COORD(gGfxViewRotCoord, &Gfx_ViewOffsetCoord, 0);
    GRAPHICS_RESET_VIEW_COORD(gGfxViewCoord, &gGfxViewRotCoord, 0);
#undef GRAPHICS_RESET_VIEW_COORD

    gte_SetGeomScreen(GRAPHICS_DEFAULT_PROJECTION_DISTANCE);

    gfxSetRotIdentity(&GsWSMATRIX);
}

void Display_LoadImageStrips(s32 bufferIndex)
{
    RECT rect;
    s32  bufferY = bufferIndex;
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
            rect.w = DISPLAY_BACKGROUND_WIDTH;
            rect.h = DISPLAY_BACKGROUND_HEIGHT - gDisplayState.vramYOffset;
            LoadImage(&rect, Fs_ImgBuffers->strips[0]);
            return;
        }
        if (bufferIndex == 0) {
            rect.y = 0;
        } else {
            rect.y = DISPLAY_BACKGROUND_BUFFER_STRIDE;
        }
        rect.x      = 0;
        rect.w      = DISPLAY_BACKGROUND_WIDTH;
        offsetYByte = gDisplayState.vramYOffset;
        rect.h      = offsetYByte + DISPLAY_BACKGROUND_HEIGHT;
        LoadImage(&rect, (u_long*)((u8*)Fs_ImgBuffers + ((-offsetYByte) * DISPLAY_BACKGROUND_ROW_BYTES)));
        return;
    }
    sourceRowOffsetBytes = 0;
    // Each strip is one 16-pixel column of the image workspace.
    if (gCdCmdQueue.imageLayout == FILE_SYSTEM_IMAGE_STRIPS) {
        bufferY *= DISPLAY_BACKGROUND_BUFFER_STRIDE;
        rect.w   = DISPLAY_BACKGROUND_STRIP_WIDTH;
        offsetY  = gDisplayState.vramYOffset;
        rect.y   = bufferY;
        rect.h   = DISPLAY_BACKGROUND_HEIGHT;
        if (offsetY > 0) {
            rect.h -= offsetY;
            rect.y  = bufferY + offsetY;
        } else if (offsetY < 0) {
            sourceRowOffsetBytes = (-offsetY) * DISPLAY_BACKGROUND_STRIP_ROW_BYTES;
            rect.h               = offsetY + DISPLAY_BACKGROUND_HEIGHT;
        }
        stripIndex = 0;
        do {
            stripIndex16 = stripIndex & 0xFFFF;
            rect.x       = stripIndex16 * DISPLAY_BACKGROUND_STRIP_WIDTH;
            LoadImage(&rect, (u_long*)((u8*)Fs_ImgBuffers->strips[stripIndex16] + sourceRowOffsetBytes));
            stripIndex++;
        } while ((u32)(stripIndex & 0xFFFF) < DISPLAY_BACKGROUND_STRIP_COUNT);
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
        D_8005EC68               = 0;
        D_8005EC6C               = DISPLAY_FRAME_LINES_ONE;
    } else if (timingMode == DISPLAY_TIMING_TWO_VBLANKS) {
        gDisplayState.frameTicks = 2;
        D_8005EC68               = 2;
        D_8005EC6C               = DISPLAY_FRAME_LINES_TWO;
    } else if (timingMode == DISPLAY_TIMING_THREE_VBLANKS) {
        gDisplayState.frameTicks = 3;
        D_8005EC68               = 3;
        D_8005EC6C               = DISPLAY_FRAME_LINES_THREE;
    }
}

void gpuClearFrameOrderingTable(s16 bufferIndex)
{
    u_long* tableStart = Gpu_OtTags + bufferIndex * GPU_ORDERING_TABLE_BUFFER_ENTRIES;
    ClearOTagR(tableStart, GPU_ORDERING_TABLE_BUFFER_ENTRIES);
    // End at the first frame tag instead of following the SDK's trailing packets.
    *tableStart = GPU_OT_END_PRIM;
}

static void Gfx_InitGraph(void)
{
    RECT    rect;
    GsOT*   otCtx;
    u_long* ot;
    s32     depth;

    if (D_8005EC64 == 1) {
        ResetGraph(0);
    }

    rect.x = 0;
    rect.y = 0;
    rect.w = 0x140;
    rect.h = 0x200;
    ClearImage(&rect, 0, 0, 0);
    DrawSync(0);
    InitGeom();

    // The tables exceed `1 << length` entries, so `tag` is set here rather than by `GsClearOt`.
    otCtx           = Gpu_OtBuffers;
    depth           = GPU_ORDERING_TABLE_DEPTH_BITS;
    otCtx->length   = depth;
    ot              = Gpu_OtTags;
    otCtx->tag      = (GsOT_TAG*)(ot + GPU_ORDERING_TABLE_BUFFER_ENTRIES - 1);
    otCtx->org      = (GsOT_TAG*)ot;
    otCtx[1].length = depth;
    otCtx[1].org    = (GsOT_TAG*)(ot + GPU_ORDERING_TABLE_BUFFER_ENTRIES);
    otCtx[1].tag    = (GsOT_TAG*)(ot + 2 * GPU_ORDERING_TABLE_BUFFER_ENTRIES - 1);
    GameMain_SpawnBootTask();
    gfxResetView();
    Gpu_InitDefaultLights();
    gDisplayState.control.flags.imageSource = DISPLAY_IMAGE_NONE;
}

static void GameMain_SpawnBootTask(void)
{
    if (D_8005EC64 == 1) {
        Task_Spawn(0, 0x1F, 0, 0);
    } else {
        Task_Spawn(0, 0x20, 0, 0);
    }
}

static void Display_PutEnvAndDraw(s32 arg0)
{
    PutDrawEnv(&gDisplayState.drawEnv[arg0]);
    PutDispEnv(&gDisplayState.dispEnv[arg0]);
    if (gDisplayState.control.flags.imageSource != DISPLAY_IMAGE_NONE) {
        Display_LoadImageStrips(arg0);
    }
    Stream_PresentFrame();
    if (gDisplayState.skipDraw == 0) {
        DrawOTag((u_long*)Gpu_OtBuffers[arg0].tag);
    }
}

// TODO
void GameMain(void)
{
    GameResetScratchHead();
    ResetCallback();
    SetVideoMode(MODE_NTSC);
    Spu_Init();
    Mc_InitLib();
    Pad_Init();
    Boot_InitCd();
    memFillBytes(&Wip_SysFlags, 0, sizeof(Wip_SysFlags));
    D_8005EC64 = 0;
    GameMain_Init();
    GameMain_Loop();
}

u32 GameMain_GetResetCount(void)
{
    return D_8005EC64;
}
