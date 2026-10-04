#include "boot.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libcd.h>
#include <psyq/libetc.h>
#include <psyq/libpress.h>

#include "types.h"

#include "main/cdaudio.h"
#include "cdaudio.h"
#include "main/cdaudio_types.h"
#include "main/display.h"
#include "main/display_types.h"
#include "fs.h"
#include "main/fs_types.h"
#include "main/gameflow.h"
#include "gfx.h"
#include "main/gfx_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "mapui/map_akropolis.h"

#include "mapui/map_dryfield.h"

#include "mapui/map_dryfield_full.h"

#include "mapui/map_neo_ark.h"

#include "mapui/map_shelter.h"

#include "title/title.h"

/* Define BSS before API headers to preserve first-declaration order. */
u8* GAuxHeap;

size_t GAuxHeapSize;

u8* Gpu_PrimHeapBase;

void* gMemActiveAuxHeap;

size_t Gpu_PrimHeapSize;

size_t GActiveAuxHeapSize;

static u8* Gpu_PrimHeapCanaryAddress;

CdCmdQueue gCdCmdQueue;

u8* Mem_AuxRegionBase;

size_t Mem_AuxRegionBytes;

#include "main/fs.h"
#include "main/mem.h"
#include "mem.h"

/// Fixed addresses no image defines, which main points at: the image-buffer
/// region, the work area at 0x801FD000, the load addresses of the three actor
/// slots, and 4MB into the dev kit's memory.

extern u8 D_801FD000[];

extern u8 D_80131E20[];

extern u8 D_80149E20[];

extern u8 D_80161E20[];

extern u8 D_80400000[];

// Build stamp (must stay in .rodata ahead of Boot_LoadInitialFile jtbl).
/// Early-image build stamp string @ VA 0x80012750 ("2000/05/01 19:24 ver2.49").
static const char Boot_BuildStamp[];

/// Unreferenced.
static void* Mem_UnusedTopRamPointer;

/// Unreferenced.
static void* Mem_UnusedDevKitEndPointer;

static GfxImageSlot* Gfx_ImageSlotTables[];

// Build stamp (must stay in .rodata ahead of Boot_LoadInitialFile jtbl).
/// Early-image build stamp string @ VA 0x80012750 ("2000/05/01 19:24 ver2.49").
static const char Boot_BuildStamp[] = "2000/05/01 19:24 ver2.49";

FsImgBuffers* Fs_ImgBuffers = &D_801D7000;
/// Unreferenced.
static void* Mem_UnusedTopRamPointer = D_801FD000;
void*        Fs_ActorLoadBase0       = D_80131E20;
void*        Fs_ActorLoadBase1       = D_80149E20;
void*        Fs_ActorLoadBase2       = D_80161E20;
/// Unreferenced.
static void* Mem_UnusedDevKitEndPointer = D_80400000;

/// Start of the auxiliary region while no area map is selected: the address the
/// map overlays load at, since without one their memory is free as well.
#define MEM_AUX_REGION_DEFAULT_BASE ((u8*)0x80179950)

static GfxImageSlot* Gfx_ImageSlotTables[] = {
    NULL,
    D_map_akropolis_8017A048,
    D_map_dryfield_80179A14,
    D_map_dryfield_full_801799A4,
    D_map_shelter_80179B40,
    D_map_neo_ark_80179DB8,
};

void Mem_ConfigureAuxHeap(s32 arg0, s32 arg1)
{
    GfxImageSlot* entries;
    s32           i;
    u8**          p88;
    size_t*       p90;
    u8**          p98;
    size_t        temp;

    entries = Gfx_ImageSlotTables[arg0];
    if ((gDisplayState.videoMode == DISPLAY_VIDEO_NORMAL) || (arg0 == 0)) {
        Mem_AuxRegionBase  = MEM_AUX_REGION_DEFAULT_BASE;
        Mem_AuxRegionBytes = 0x836B0;
        Gpu_PrimHeapBase   = MEM_AUX_REGION_DEFAULT_BASE;
        gMemActiveAuxHeap  = MEM_AUX_REGION_DEFAULT_BASE + 0x10000;
        GActiveAuxHeapSize = 0x4D6B0;
    } else {
        Mem_AuxRegionBase  = entries[arg1].regionBase;
        Mem_AuxRegionBytes = entries[arg1].byteExtent + 0x26000;
        Gpu_PrimHeapBase   = entries[arg1].regionBase;
        gMemActiveAuxHeap  = Gpu_PrimHeapBase + 0x10000;
        GActiveAuxHeapSize = entries[arg1].byteExtent - 0x10000;
    }
    i                = 0;
    Gpu_PrimHeapSize = 0x10000;
    GAuxHeap         = gMemActiveAuxHeap;
    GAuxHeapSize     = Mem_AuxRegionBytes - 0x10000;
    do {
        Gpu_PrimHeapBase[Gpu_PrimHeapSize - (i & 0xFF) - 1] = 0;
        i                                                  += 1;
    } while ((u32)(i & 0xFF) < 0xAU);
    p98  = &Gpu_PrimHeapCanaryAddress;
    p88  = &Gpu_PrimHeapBase;
    p90  = &Gpu_PrimHeapSize;
    temp = *p90 - 0xA;
    *p98 = *p88 + temp;
}

void Boot_LoadInitialFile(Task* task)
{
    u8          modeParam[8];
    u8          param1[8];
    u8          param2[8];
    u8          fade;
    CdCmdQueue* queue;

    queue = &gCdCmdQueue;
    switch (task->state) {
        case 0:
            Display_SetMode(DISPLAY_SETUP_DEFAULT | DISPLAY_SETUP_NO_CLEAR | DISPLAY_SETUP_KEEP_VIEW);
            modeParam[0] = CdlModeSpeed | CdlModeSize1;
            CdControlB(CdlSetmode, modeParam, NULL);
            SetDispMask(0);
            Fs_ScanIsoDirectory(1);
            gDisplayState.control.flags.imageSource = DISPLAY_IMAGE_STRIPS;
            CdCmd_Enqueue(CD_COMMAND_READ_STAGE_HEADER, NULL, NULL);
            Mem_ConfigureAuxHeap(0, 0);
            while (queue->imageLoadStatus != CD_COMMAND_IMAGE_COMPLETE) {
                CdCmd_StepVlcRebuild();
            }
            param1[3] = 0;
            param1[2] = 0;
            param1[0] = 1;
            param2[0] = 0;
            param2[1] = 0;
            param2[2] = 0;
            param2[3] = 0;
            CdCmd_Enqueue(CD_COMMAND_LOAD_FILE, param1, param2);
            task->killCountdown = 0xFF;
            fade                = task->killCountdown;
            fadeDrawOverlay(fade, fade, fade, GPU_BLEND_SUBTRACT);
            task->state++;
            break;

        case 1:
            SetDispMask(1);
            task->killCountdown -= 8;
            if (task->killCountdown <= 0) {
                task->killCountdown = 0;
                task->state++;
            }
            fade = task->killCountdown;
            fadeDrawOverlay(fade, fade, fade, GPU_BLEND_SUBTRACT);
            break;

        case 2:
            if (task->killCountdown < 0x5A) {
                task->killCountdown++;
            }
            if (CdCmd_IsIdle() == 0) {
                return;
            }
            if (task->killCountdown < 0x5A) {
                return;
            }
            task->killCountdown = 0;
            task->state++;
            break;

        case 3:
            task->killCountdown += 8;
            if (task->killCountdown >= 0x100) {
                memFillBytes(Fs_ImgBuffers, 0, sizeof(*Fs_ImgBuffers));
                task->state++;
                break;
            }
            fade = task->killCountdown;
            fadeDrawOverlay(fade, fade, fade, GPU_BLEND_SUBTRACT);
            break;

        case 4:
            Task_Spawn(0, 0xD, 0, 0);
            taskKill(task);
            SetDispMask(1);
            gDisplayState.debugMode = 0;
            break;
    }
}

void Boot_WaitCdAudioReady(void)
{
    CdAudio_Begin();
    while (CdAudio_Phase.stopStep != CD_AUDIO_STOP_STEP_DONE) {
    }
}

void Boot_InitCdAudio(void)
{
    CdAudio_Init();
}

void Gfx_StoreImageSlot(s32 arg0, s32 arg1, s32 arg2, s32 arg3)
{
    RECT          rect;
    GfxImageSlot* entries;
    u8*           ptr;
    size_t        size;
    size_t        imgBufSize;
    size_t*       pSize;

    entries = Gfx_ImageSlotTables[arg0];
    rect.x  = 0;
    if (arg2 != 0) {
        rect.y = 0;
    } else {
        rect.y = 0x110;
    }
    rect.w = 0x140;
    rect.h = 0xF0;
    StoreImage(&rect, (u_long*)entries[arg1].regionBase);
    DrawSync(0);

    imgBufSize = 0x25800;
    do {
        Gpu_PrimHeapSize = 0x10000;
    } while (0);
    pSize              = &GActiveAuxHeapSize;
    size               = 0x10000 - arg3;
    *pSize             = size;
    Mem_AuxRegionBytes = 0x10000;
    GAuxHeapSize       = size;

    ptr               = entries[arg1].regionBase + imgBufSize;
    Gpu_PrimHeapBase  = ptr;
    gMemActiveAuxHeap = ptr + arg3;
    Mem_AuxRegionBase = ptr;
    GAuxHeap          = ptr + arg3;
}

void Gfx_LoadImageSlot(s32 arg0, s32 arg1, s32 arg2)
{
    RECT          rect;
    GfxImageSlot* entries;

    entries = Gfx_ImageSlotTables[arg0];
    if (arg2 == 0) {
        rect.y = 0;
    } else {
        rect.y = 0x110;
    }
    rect.w = 0x140;
    rect.h = 0xF0;
    rect.x = 0;
    LoadImage(&rect, (u_long*)entries[arg1].regionBase);
}

void Boot_InitCd(void)
{
    u8 param[8];

    CdInit();
    param[0] = CdlModeSpeed;
    CdControlB(CdlSetmode, param, NULL);
    CdAudio_Init();
    CdCmd_ClearQueue();
}

void Boot_ResetCd(s32 mode)
{
    u8 ctrlParam[8];

    CdFlush();
    VSync(3);
    CdControlB(CdlPause, NULL, NULL);
    if (Wip_SysFlags.movieStreamActive != 0) {
        DecDCTReset(0);
        StClearRing();
        StUnSetRing();
        Wip_SysFlags.movieStreamActive = 0;
    }
    CdReset(mode);
    ctrlParam[0] = CdlModeSpeed;
    CdControlB(CdlSetmode, ctrlParam, NULL);
    CdCmd_ClearQueue();
}

void Boot_LoadTask(Task* task)
{
    u8  modeParam[8];
    u8  param1[8];
    u8  param2[8];
    s32 state;

    state = task->state;
    switch (state) {
        case 0:
            modeParam[0] = CdlModeSpeed | CdlModeSize1;
            CdControlB(CdlSetmode, modeParam, NULL);
            SetDispMask(0);
            param1[3] = 0;
            param1[2] = 0;
            param1[0] = 1;
            param2[0] = 0;
            param2[1] = 0;
            param2[2] = 0;
            param2[3] = 0;
            CdCmd_Enqueue(CD_COMMAND_LOAD_FILE, param1, param2);
            task->state = task->state + 1;
            return;
        case 1:
            if (CdCmd_IsIdle() != 0) {
                SetDispMask(1);
                Mem_ConfigureAuxHeap(0, 0);
                Task_SpawnFromTable(Title_TaskDescs, 0, 0, 0);
                taskKill(task);
                gDisplayState.debugMode = 0;
            }
            return;
    }
}

void Boot_DispatchCdCmd(void)
{
    CdCmd_Dispatch();
}

bool Fs_StageCdfIsAvailable(u32 stageIdx)
{
    return Fs_StageCdfSectors[(u8)stageIdx] != 0;
}
