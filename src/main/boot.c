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
#include "main/areas.h"
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

/// Image-memory lengths in bytes; the full region ends at 0x801FD000.
enum {
    MEMORY_IMAGE_REGION_EXTENSION_BYTES = 0x26000, // Beyond the slot extent: image buffers and the following bytes up to 0x801FD000
    MEMORY_IMAGE_DEFAULT_REGION_BYTES   = 0x836B0, // From the default base at 0x80179950 to the region's end
    MEMORY_PRIMITIVE_TRAILER_BYTES      = 10,
};

/// VRAM y coordinate of the second 320x240 frame, including the 32-line gap.
enum {
    GRAPHICS_AREA_FRAME_SECOND_Y = FILE_SYSTEM_IMAGE_HEIGHT + 32,
};

/// Clears the primitive reservation's last ten bytes and records their start.
static __inline__ void _memClearPrimitiveTrailer(void)
{
    s32 trailerIndex;

    trailerIndex = 0;
    do {
        Gpu_PrimHeapBase[Gpu_PrimHeapSize - (trailerIndex & 0xFF) - 1] = 0;
        trailerIndex                                                  += 1;
    } while ((u32)(trailerIndex & 0xFF) < (u32)MEMORY_PRIMITIVE_TRAILER_BYTES);
    Gpu_PrimHeapCanaryAddress = Gpu_PrimHeapBase + (Gpu_PrimHeapSize - MEMORY_PRIMITIVE_TRAILER_BYTES);
}

/// Reserves a fixed workspace after the captured frame and saves its heap views.
static __inline__ void _memConfigureCapturedFrameWorkspace(const GfxImageSlot* areaSlots, s32 areaId, s32 auxHeapOffsetBytes)
{
    u8*     workspaceBase;
    size_t  auxHeapBytes;
    size_t  capturedFrameBytes;
    size_t* activeHeapSizeSlot;

    capturedFrameBytes = sizeof(FsImgBuffers);
    do {
        Gpu_PrimHeapSize = MEMORY_PRIMITIVE_HEAP_BYTES;
    } while (0);
    activeHeapSizeSlot  = &GActiveAuxHeapSize;
    auxHeapBytes        = MEMORY_PRIMITIVE_HEAP_BYTES - auxHeapOffsetBytes;
    *activeHeapSizeSlot = auxHeapBytes;
    Mem_AuxRegionBytes  = MEMORY_PRIMITIVE_HEAP_BYTES;
    GAuxHeapSize        = auxHeapBytes;

    workspaceBase     = areaSlots[areaId].regionBase + capturedFrameBytes;
    Gpu_PrimHeapBase  = workspaceBase;
    gMemActiveAuxHeap = workspaceBase + auxHeapOffsetBytes;
    Mem_AuxRegionBase = workspaceBase;
    GAuxHeap          = workspaceBase + auxHeapOffsetBytes;
}

void memConfigureImageMemory(s32 stageId, s32 areaId)
{
    const GfxImageSlot* areaSlots;

    areaSlots = Gfx_ImageSlotTables[stageId];
    // Initially keep the active heap below the resident image workspace.
    if ((gDisplayState.videoMode == DISPLAY_VIDEO_NORMAL) || (stageId == GAME_STAGE_NONE)) {
        Mem_AuxRegionBase  = MEM_AUX_REGION_DEFAULT_BASE;
        Mem_AuxRegionBytes = MEMORY_IMAGE_DEFAULT_REGION_BYTES;
        Gpu_PrimHeapBase   = MEM_AUX_REGION_DEFAULT_BASE;
        gMemActiveAuxHeap  = MEM_AUX_REGION_DEFAULT_BASE + MEMORY_PRIMITIVE_HEAP_BYTES;
        GActiveAuxHeapSize = MEMORY_IMAGE_DEFAULT_REGION_BYTES - MEMORY_IMAGE_REGION_EXTENSION_BYTES - MEMORY_PRIMITIVE_HEAP_BYTES;
    } else {
        Mem_AuxRegionBase  = areaSlots[areaId].regionBase;
        Mem_AuxRegionBytes = areaSlots[areaId].byteExtent + MEMORY_IMAGE_REGION_EXTENSION_BYTES;
        Gpu_PrimHeapBase   = areaSlots[areaId].regionBase;
        gMemActiveAuxHeap  = Gpu_PrimHeapBase + MEMORY_PRIMITIVE_HEAP_BYTES;
        GActiveAuxHeapSize = areaSlots[areaId].byteExtent - MEMORY_PRIMITIVE_HEAP_BYTES;
    }

    // Save the larger heap view that also reclaims the image workspace.
    Gpu_PrimHeapSize = MEMORY_PRIMITIVE_HEAP_BYTES;
    GAuxHeap         = gMemActiveAuxHeap;
    GAuxHeapSize     = Mem_AuxRegionBytes - MEMORY_PRIMITIVE_HEAP_BYTES;
    _memClearPrimitiveTrailer();
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
            memConfigureImageMemory(GAME_STAGE_NONE, 0);
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

void gfxCaptureAreaFrame(s32 stageId, s32 areaId, s32 bufferIndex, s32 auxHeapOffsetBytes)
{
    RECT                frameRect;
    const GfxImageSlot* areaSlots;

    areaSlots   = Gfx_ImageSlotTables[stageId];
    frameRect.x = 0;
    if (bufferIndex != 0) {
        frameRect.y = 0;
    } else {
        frameRect.y = GRAPHICS_AREA_FRAME_SECOND_Y;
    }
    frameRect.w = FILE_SYSTEM_IMAGE_WIDTH;
    frameRect.h = FILE_SYSTEM_IMAGE_HEIGHT;
    // Finish the capture before moving the heaps beyond its pixels.
    StoreImage(&frameRect, (u_long*)areaSlots[areaId].regionBase);
    DrawSync(0);

    _memConfigureCapturedFrameWorkspace(areaSlots, areaId, auxHeapOffsetBytes);
}

void gfxRestoreAreaFrame(s32 stageId, s32 areaId, s32 bufferIndex)
{
    RECT                frameRect;
    const GfxImageSlot* areaSlots;

    areaSlots = Gfx_ImageSlotTables[stageId];
    if (bufferIndex == 0) {
        frameRect.y = 0;
    } else {
        frameRect.y = GRAPHICS_AREA_FRAME_SECOND_Y;
    }
    frameRect.w = FILE_SYSTEM_IMAGE_WIDTH;
    frameRect.h = FILE_SYSTEM_IMAGE_HEIGHT;
    frameRect.x = 0;
    LoadImage(&frameRect, (u_long*)areaSlots[areaId].regionBase);
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
                memConfigureImageMemory(GAME_STAGE_NONE, 0);
                taskSpawnFromTable(Title_TaskDescs, 0, 0, 0);
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
