#include "common.h"

#include <psyq/libapi.h>
#include <psyq/libetc.h>
#include <psyq/inline_c.h>

#include "main/unknown_syms.h"
#include "main/boot.h"
#include "main/cdaudio.h"
#include "main/fs.h"
#include "main/gamemain.h"
#include "main/gfx.h"
#include "main/gpuext.h"
#include "main/pad.h"
#include "main/text.h"
#include "main/tmd.h"
#include "main/wipsys.h"

void GameMain_Init(void)
{
    s32 flag; // The indirection is required.

    SetDispMask(0);
    Boot_WaitCdAudioReady();
    ResetCallback();
    Boot_ResetCd(1);
    VSync(10);

    GameResetScratchHead();
    D_8005EC64++;
    Mem_ConfigureAuxHeap(0, 0);
    Mem_Init();
    Task_ResetDefaultList();
    Tmd_InitLists();
    Gfx_InitGraph();

    Mem_Set(&gDisplayState, 0, sizeof(gDisplayState));
    gDisplayState.field_120                    = 1;
    gDisplayState.region                       = 0;
    gDisplayState.at100.flags.pendingPlayerPos = 0;
    gDisplayState.holdState                    = -1;
    gDisplayState.displayOwner                 = 0;
    gDisplayState.pendingMode                  = 0;
    gDisplayState.frameCount                   = 0;
    gDisplayState.gameTick                     = 0;
    gDisplayState.animFrame                    = 0;
    gDisplayState.vsyncCount                   = 0;
    gDisplayState.field_10                     = 0;
    gDisplayState.loopCount                    = 0;
    GameMain_SetFrameTiming(0);

    Display_PendingFlip = 0;
    Gpu_ClearOTag(0);
    Gpu_ClearOTag(1);
    Spu_WaitDma();
    Snd_SetMutedVolumes(0);
    Boot_InitCdAudio();
    VSyncCallback(Display_VSyncCallback);

    flag                     = 1;
    gDisplayState.drawBuffer = flag;
    Display_SetMode(0x1010);
    Mem_Set(Pad_RemapState, 0, 0x1C);
}

void func_80020058(void);

void Display_FlipDraw(s32 arg0)
{
    s32 mode;
    u8  saved;

    mode = D_80070E38 & 0xF;
    if (mode != 2) {
        PutDrawEnv(&gDisplayState.drawEnv[arg0]);
        PutDispEnv(&gDisplayState.dispEnv[arg0]);
        if (mode == 0) {
            if (D_8006EC30 != 0) {
                Display_LoadImageStrips(arg0);
                saved                    = gDisplayState.drawBuffer;
                gDisplayState.drawBuffer = arg0;
                func_80020058();
                gDisplayState.drawBuffer = saved;
            }
            DrawOTag(Gpu_OtBuffers[gDisplayState.otBuffer].lastTag);
        } else if (D_8006EC30 == 2) {
            Display_LoadImageStrips(arg0);
        } else if (D_8006EC30 == 3) {
            Gfx_LoadImageSlot(gGameSession->at4.loc.stage, gGameSession->at4.loc.area, arg0);
        }
        if ((s8)D_80070E38 < 0x10) {
            DrawOTag(Gpu_OrderingTables[arg0].tag);
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
    if (gDisplayState.at100.flags.imageSource != 0) {
        Display_LoadImageStrips(buf);
    }
    func_80020058();
    if (gDisplayState.skipDraw == 0) {
        DrawOTag(Gpu_OtBuffers[buf].lastTag);
    }
}

void Display_VSyncCallback(void)
{
    s32 start;

    start = VSync(1);
    if (Display_PendingFlip >= 0) {
        if (((start & 0xFFFF) + D_8005EC78) > (D_8005EC6C >> 1)) {
            if (gDisplayState.vsyncFlag == 0) {
                _displayPresentFrame(Display_PendingFlip);
                Display_PendingFlip = -1;
            } else if (gDisplayState.vsyncFlag == 1) {
                Display_FlipDraw(Display_PendingFlip);
                Display_PendingFlip = -1;
            }
        }
    }
    D_80070F64 -= 1;
    if (gDisplayState.displayOwner == 0) {
        gDisplayState.frameCount += 1;
    }
    gDisplayState.vsyncCount += 1;
    CdAudio_Tick();
    Audio_IrqFrameWork();
    func_8002C1D8();
    D_8005EC74 = VSync(1) - (start & 0xFFFF);
}

// Drawn by GameMain_ShowLoading (must stay in .rodata for this TU).
const u8 GameMain_PauseText[] = "PAUSE!";

/// Nonzero while the game cannot be halted: a CD command is running without
/// the halt flag that permits it, the flip is holding the displayed frame, a CD
/// operation is in progress, or the display is off.
static inline s32 _gameMainPauseBlocked(void)
{
    s32 blocked;

    blocked = 0;
    if (CdCmd_Queue.field_244 != 0 && !(GameMain_HaltFlags & 8)) {
        blocked = 1;
    } else if (gDisplayState.vsyncFlag == 1 && gDisplayState.at100.flags.flipMode == 2) {
        blocked = 1;
    } else if (Fs_CdOpStatus != 0xFF) {
        blocked = 1;
    } else if (GpuExt_IsDisplayEnabled() == 0) {
        blocked = 1;
    }
    return blocked;
}

void GameMain_ShowLoading(s32 arg0)
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

            tile->x0                 = -0xA0;
            tile->w                  = 0x140;
            tile->h                  = 0xF0;
            PRIM_COLOR_WORD(tile, 0) = 0;
            setlen(tile, 3);
            setcode(tile, 0x62);
            tile->y0 = -0x78 - gDisplayState.vramYOffset;
            DrawPrim(tile);

            req.x          = 0;
            req.otIndex    = 4;
            req.field_8    = 0x37A78;
            req.glyphTable = 4;
            req.centerMode = 1;
            req.field_E    = 0x10;
            req.y          = 6 - gDisplayState.vramYOffset;
            func_8002E53C(&req, GameMain_PauseText);

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

    q = &CdCmd_Queue;
    if (q->field_240 != 0) {
        if (elapsed + q->field_1A0 < *q->field_19C) {
            while (((VSync(1) - start) & 0x7FFF) + q->field_1A0 < *q->field_19C) {
            }
            elapsed       = (VSync(1) - start) & 0x7FFF;
            q->field_1A0 += elapsed;
        } else {
            q->field_1A0 += elapsed;
        }
        if (*q->field_19C != 0) {
            q->field_19C++;
            if (*q->field_19C == -1) {
                q->field_19C++;
            }
        }
    }
    return elapsed;
}

void GameMain_Loop(void)
{
    CdCmdQueue* cq;
    s32         start;
    s32         buf;
    s32         elapsed;
    s8          shake;

    start                  = 0;
    GameMain_HaltFlags     = 0;
    gDisplayState.otBuffer = gDisplayState.drawBuffer;

    for (;;) {
        if (gDisplayState.gameMode == 1 ||
            (gDisplayState.gameMode == 0 && gDisplayState.cdBusy == 0 && gDisplayState.gameRunning != 0 &&
             Pad_CheckSpecialCombo() != 0)) {
            GameMain_Init();
            GameMain_HaltFlags = 0;
        }
        GameResetScratchHead();
        Pad_UpdatePort0();

        if (Pad_States[0].status == 0xFF && Pad_States[0].cooldown == 0 && gDisplayState.gameRunning != 0 &&
            gDisplayState.loadBusy == 0 && gDisplayState.gameMode == 0) {
            GameMain_ShowLoading(1);
        } else if (GameMain_HaltFlags & 2) {
            SndEvt_EnqueueTypeE();
            GameMain_HaltFlags &= ~2;
        }

        Snd_PollAsync(0);

        if (GameMain_HaltFlags != 0 && !_gameMainPauseBlocked()) {
            VSync(0);
            start = VSync(1) & 0x7FFF;
            Boot_DispatchCdCmd();
            continue;
        }

        gDisplayState.loopCount++;
        gDisplayState.field_10++;
        if (gDisplayState.displayOwner == 0 && gDisplayState.pendingMode != 0 &&
            ((s8)gDisplayState.pendingMode < 0 || gDisplayState.holdState >= 0)) {
            Display_DispatchModeId(gDisplayState.pendingMode);
        }
        if (gDisplayState.displayOwner != 0) {
            start = Display_FrameFlipDraw(Gpu_OtBuffers, start, gDisplayState.otBuffer);
            continue;
        }

        buf                       = gDisplayState.otBuffer ^ 1;
        gDisplayState.pendingMode = 0;
        cq                        = &CdCmd_Queue;
        gDisplayState.otBuffer    = buf;
        gDisplayState.drawBuffer  = gDisplayState.otBuffer;
        gDisplayState.animFrame++;
        if (cq->field_222 == 0) {
            gDisplayState.gameTick += 1 + (D_8005EC68 >> 1);
        }
        gDisplayState.field_10 += D_8005EC68 >> 1;
        gpuBeginOt(buf);

        Gpu_SysPrimCursor = Gpu_PrimBufStatic + gDisplayState.otBuffer * 0x3000;
        gGpuPrimCursor    = (u8*)(Gpu_PrimHeapBase + gDisplayState.otBuffer * (Gpu_PrimHeapSize >> 1));
        Task_ExecDefaultList();

        if (gDisplayState.displayOwner != 0) {
            continue;
        }

        Boot_DispatchCdCmd();

        if (gDisplayState.height == 480) {
            Display_PendingFlip = -1;
            VSync(0);
            ResetGraph(1);
            _displayPresentFrame(gDisplayState.otBuffer);
            start = 0;
            continue;
        }

        DrawSync(0);
        elapsed = _gameMainPaceToStream(start, (VSync(1) - start) & 0x7FFF);

        if (elapsed < D_8005EC6C) {
            gDisplayState.vsyncFlag = 0;
            Display_PendingFlip     = gDisplayState.otBuffer;
            VSync(D_8005EC68);
            if (Display_PendingFlip != -1) {
                D_8005EC78 = 0;
                start      = VSync(1) & 0x7FFF;
                _displayPresentFrame(gDisplayState.otBuffer);
                Display_PendingFlip = -1;
            } else {
                D_8005EC78 = D_8005EC74;
                start      = -D_8005EC74;
            }
        } else {
            gDisplayState.vsyncFlag = 0;
            Display_PendingFlip     = -2;
            D_8005EC78              = 0;
            start                   = VSync(1) & 0x7FFF;
            _displayPresentFrame(gDisplayState.otBuffer);
            Display_PendingFlip = -1;
        }

        shake                           = gDisplayState.shakeY;
        gDisplayState.vramYOffset       = shake;
        gDisplayState.drawEnv[0].ofs[1] = shake + 0x78;
        gDisplayState.drawEnv[1].ofs[1] = shake + 0x188;
    }
}

void Gfx_InitCoordinateTrees(void)
{
    gfxSetRotIdentity(&Gfx_ViewOffsetCoord.coord);
    Gfx_ViewOffsetCoord.sub        = NULL;
    Gfx_ViewOffsetCoord.coord.t[0] = 0;
    Gfx_ViewOffsetCoord.coord.t[1] = 0;
    Gfx_ViewOffsetCoord.coord.t[2] = 0x8000;
    Gfx_ViewOffsetCoord.flg        = 0;

    gfxSetRotIdentity(&gGfxViewRotCoord.coord);
    gGfxViewRotCoord.sub        = &Gfx_ViewOffsetCoord;
    gGfxViewRotCoord.coord.t[0] = 0;
    gGfxViewRotCoord.coord.t[1] = 0;
    gGfxViewRotCoord.coord.t[2] = 0;
    gGfxViewRotCoord.flg        = 0;

    gfxSetRotIdentity(&gGfxViewCoord.coord);
    gGfxViewCoord.sub        = &gGfxViewRotCoord;
    gGfxViewCoord.coord.t[0] = 0;
    gGfxViewCoord.coord.t[1] = 0;
    gGfxViewCoord.coord.t[2] = 0;
    gGfxViewCoord.flg        = 0;

    gte_SetGeomScreen(0x400);

    gfxSetRotIdentity(&GsWSMATRIX);
}

void Display_LoadImageStrips(s32 arg0)
{
    RECT rect;
    s32  var_s0;
    s32  var_s1;
    s32  temp_v1;
    s32  field;
    s8   yoff;

    if (CdCmd_Queue.field_21C == 0) {
        if (gDisplayState.vramYOffset >= 0) {
            if (arg0 != 0) {
                rect.y = gDisplayState.vramYOffset + 0x110;
            } else {
                rect.y = gDisplayState.vramYOffset;
            }
            rect.x = 0;
            rect.w = 0x140;
            rect.h = 0xF0 - gDisplayState.vramYOffset;
            LoadImage(&rect, (u_long*)Fs_ImgBuffers);
            return;
        }
        if (arg0 == 0) {
            rect.y = 0;
        } else {
            rect.y = 0x110;
        }
        rect.x = 0;
        rect.w = 0x140;
        yoff   = gDisplayState.vramYOffset;
        rect.h = yoff + 0xF0;
        LoadImage(&rect, (u_long*)((u8*)Fs_ImgBuffers + ((-yoff) * 0x280)));
        return;
    }
    var_s1 = 0;
    if (CdCmd_Queue.field_21C == 1) {
        arg0  *= 0x110;
        rect.w = 0x10;
        field  = gDisplayState.vramYOffset;
        rect.y = arg0;
        rect.h = 0xF0;
        if (field > 0) {
            rect.h -= field;
            rect.y  = arg0 + field;
        } else if (field < 0) {
            var_s1 = (-field) * 0x20;
            rect.h = field + 0xF0;
        }
        var_s0 = 0;
        do {
            temp_v1 = var_s0 & 0xFFFF;
            rect.x  = temp_v1 * 0x10;
            LoadImage(&rect, (u_long*)((u8*)Fs_ImgBuffers + (temp_v1 * 0x1E00) + var_s1));
            var_s0++;
        } while ((u32)(var_s0 & 0xFFFF) < 0x14U);
    }
}

void GameMain_SetFrameTiming(s32 arg0)
{
    if (arg0 == 0) {
        gDisplayState.frameTicks = 1;
        D_8005EC68               = 0;
        D_8005EC6C               = 0x106;
    } else if (arg0 == 1) {
        gDisplayState.frameTicks = 2;
        D_8005EC68               = 2;
        D_8005EC6C               = 0x20D;
    } else if (arg0 == 2) {
        gDisplayState.frameTicks = 3;
        D_8005EC68               = 3;
        D_8005EC6C               = 0x313;
    }
}

void Gpu_ClearOTag(s16 tableIdx)
{
    u_long* tableStart = Gpu_OtTags + tableIdx * GPU_OT_ENTRIES;
    ClearOTagR(tableStart, GPU_OT_ENTRIES);
    *tableStart = GPU_OT_END_PRIM;
}

void Gfx_InitGraph(void)
{
    RECT      rect;
    GpuOtBuf* otCtx;
    u_long*   ot;
    s32       depth;

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

    otCtx            = Gpu_OtBuffers;
    depth            = 0xA;
    otCtx->depth     = depth;
    ot               = Gpu_OtTags;
    otCtx->lastTag   = ot + GPU_OT_ENTRIES - 1;
    otCtx->ot        = ot;
    otCtx[1].depth   = depth;
    otCtx[1].ot      = ot + GPU_OT_ENTRIES;
    otCtx[1].lastTag = ot + 2 * GPU_OT_ENTRIES - 1;
    GameMain_SpawnBootTask();
    Gfx_InitCoordinateTrees();
    Gpu_InitDefaultLights();
    gDisplayState.at100.flags.imageSource = 0;
}

void GameMain_SpawnBootTask(void)
{
    if (D_8005EC64 == 1) {
        Task_Spawn(0, 0x1F, 0, 0);
    } else {
        Task_Spawn(0, 0x20, 0, 0);
    }
}

void Display_PutEnvAndDraw(s32 arg0)
{
    PutDrawEnv(&gDisplayState.drawEnv[arg0]);
    PutDispEnv(&gDisplayState.dispEnv[arg0]);
    if (gDisplayState.at100.flags.imageSource != 0) {
        Display_LoadImageStrips(arg0);
    }
    func_80020058();
    if (gDisplayState.skipDraw == 0) {
        DrawOTag(Gpu_OtBuffers[arg0].lastTag);
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
    Mem_Set(&Wip_SysFlags, 0, sizeof(Wip_SysFlags));
    D_8005EC64 = 0;
    GameMain_Init();
    GameMain_Loop();
}

u32 GameMain_GetResetCount(void)
{
    return D_8005EC64;
}
