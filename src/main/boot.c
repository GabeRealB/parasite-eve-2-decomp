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
#include "main/stream.h"

/// Fixed addresses no image defines, which main points at: the image-buffer
/// region, the work area at 0x801FD000, the load addresses of the three actor
/// slots, and 4MB into the dev kit's memory.

extern u8 D_801FD000[];

extern u8 D_80131E20[];

extern u8 D_80149E20[];

extern u8 D_80161E20[];

extern u8 D_80400000[];

// Build stamp (must stay in .rodata ahead of bootColdStartTask jtbl).
/// Early-image build stamp string @ VA 0x80012750 ("2000/05/01 19:24 ver2.49").
static const char Boot_BuildStamp[];

/// Unreferenced.
static void* Mem_UnusedTopRamPointer;

/// Unreferenced.
static void* Mem_UnusedDevKitEndPointer;

static GfxImageSlot* Gfx_ImageSlotTables[];

// Build stamp (must stay in .rodata ahead of bootColdStartTask jtbl).
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

/// Queues the stage-zero title startup file with normal loading and no image shift.
///
/// Borrows a writable key and four option bytes for this call. The enqueue API
/// ignores `fileKey->fileIdHundreds`; that member stays untouched, and the options
/// supply the zero hundreds component instead. X/Y offsets are zero VRAM pages
/// and rows. The CD ring must have room for one request; it copies all consumed
/// bytes before returning and retains neither pointer. Loading is asynchronous.
static inline void _bootQueueStartupFile(FsFileLoadKey* fileKey, u8 loadOptions[4])
{
    enum { BOOT_STARTUP_FILE_INDEX = 1 };

    fileKey->stage     = GAME_STAGE_NONE;
    fileKey->fileGroup = 0;
    fileKey->fileIndex = BOOT_STARTUP_FILE_INDEX;
    loadOptions[0]     = 0;
    loadOptions[1]     = CD_COMMAND_LOAD_DEFAULT;
    loadOptions[2]     = 0;
    loadOptions[3]     = 0;
    cdCmdEnqueue(CD_COMMAND_LOAD_FILE, fileKey, loadOptions);
}

/// Resets the decoder and detaches the movie ring after CD delivery has paused.
///
/// The active stream's ring must still be registered and writable. Mode zero
/// also resets the SDK callback system; the caller is rebuilding CD state.
/// Discards pending ring contents and removes its CD data/ready callbacks,
/// then clears the movie-active latch. Does not free the borrowed ring storage.
static __inline__ void _bootReleaseMovieRing(void)
{
    enum { BOOT_MOVIE_RESET_CALLBACKS_AND_DECODER = 0 };

    DecDCTReset(BOOT_MOVIE_RESET_CALLBACKS_AND_DECODER);
    StClearRing();
    StUnSetRing();
    Wip_SysFlags.movieStreamActive = false;
}

/// Clears the trailing ten bytes of the configured GPU primitive reservation.
///
/// `Gpu_PrimHeapBase` must address `Gpu_PrimHeapSize` writable bytes, with
/// at least `MEMORY_PRIMITIVE_TRAILER_BYTES` bytes in the region. Stores their
/// start in `Gpu_PrimHeapCanaryAddress`, which has no reader in the game.
static __inline__ void _memClearPrimitiveTrailer(void)
{
    u8 trailerIndex;

    for (trailerIndex = 0; trailerIndex < MEMORY_PRIMITIVE_TRAILER_BYTES; trailerIndex++) {
        Gpu_PrimHeapBase[Gpu_PrimHeapSize - trailerIndex - 1] = 0;
    }
    Gpu_PrimHeapCanaryAddress = Gpu_PrimHeapBase + (Gpu_PrimHeapSize - MEMORY_PRIMITIVE_TRAILER_BYTES);
}

/// Configures the primitive and auxiliary heap views after an area's captured frame.
///
/// `areaSlot` must be nonempty, with a word-aligned `regionBase` providing
/// `sizeof(FsImgBuffers)` bytes of captured pixels followed by
/// `MEMORY_PRIMITIVE_HEAP_BYTES` writable bytes. The slot's `byteExtent` is
/// not the frame length. Previous allocations and GPU work in the workspace
/// must have ended before it is repurposed.
///
/// `auxHeapOffsetBytes` is in [0, `MEMORY_PRIMITIVE_HEAP_BYTES`]. The active
/// and saved auxiliary heap are the suffix beginning at that byte offset;
/// the saved whole-region view and the primitive reservation both cover the
/// full workspace. The auxiliary suffix overlaps primitives unless empty.
/// These borrowed regions require separate heap3 and primitive-cursor setup.
static __inline__ void _memConfigureCapturedFrameWorkspace(const GfxImageSlot* areaSlot, s32 auxHeapOffsetBytes)
{
    u8* workspaceBase;

    // Save both the complete workspace and its auxiliary suffix for heap selection.
    Gpu_PrimHeapSize   = MEMORY_PRIMITIVE_HEAP_BYTES;
    GActiveAuxHeapSize = MEMORY_PRIMITIVE_HEAP_BYTES - auxHeapOffsetBytes;
    Mem_AuxRegionBytes = MEMORY_PRIMITIVE_HEAP_BYTES;
    GAuxHeapSize       = GActiveAuxHeapSize;

    workspaceBase     = areaSlot->regionBase + sizeof(FsImgBuffers);
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

void bootColdStartTask(Task* task)
{
    enum {
        BOOT_COLD_START_DECODE_IMAGE = 0,
        BOOT_COLD_START_FADE_IN      = 1,
        BOOT_COLD_START_HOLD_IMAGE   = 2,
        BOOT_COLD_START_FADE_OUT     = 3,
        BOOT_COLD_START_START_TITLE  = 4,
        BOOT_COLD_START_HOLD_FRAMES  = 90,
        BOOT_FADE_FULL_BRIGHTNESS    = 255,
        BOOT_FADE_FINISHED           = 256,
        BOOT_FADE_BRIGHTNESS_STEP    = 8,
        BOOT_RESIDENT_TASK_BANK      = 0,
        BOOT_START_TITLE_TASK_SLOT   = 13,
        BOOT_SCAN_WITH_STARTUP_IMAGE = 1,
    };
    u8            driveMode[8]; // Only byte 0 is sent; the retained scratch extent preserves stack placement.
    FsFileLoadKey fileKey;
    u8            loadOptions[sizeof(gCdCmdQueue.entries[0].args)];
    u8            brightness;
    CdCmdQueue*   queue;

    queue = &gCdCmdQueue;
    switch (task->state) {
        case BOOT_COLD_START_DECODE_IMAGE:
            // Discover the mounted library and decode the separate startup image.
            displayConfigureFramebuffers(DISPLAY_SETUP_DEFAULT | DISPLAY_SETUP_NO_CLEAR | DISPLAY_SETUP_KEEP_VIEW);
            driveMode[0] = CdlModeSpeed | CdlModeSize1;
            CdControlB(CdlSetmode, driveMode, NULL);
            SetDispMask(0);
            fsScanIsoDirectory(BOOT_SCAN_WITH_STARTUP_IMAGE);
            gDisplayState.control.flags.imageSource = DISPLAY_IMAGE_STRIPS;
            // This opcode ignores the key/options copied from low RAM at address zero.
            cdCmdEnqueue(CD_COMMAND_READ_STAGE_HEADER, NULL, NULL);
            memConfigureImageMemory(GAME_STAGE_NONE, 0);
            while (queue->imageLoadStatus != CD_COMMAND_IMAGE_COMPLETE) {
                mdecStepImageDecode();
            }
            _bootQueueStartupFile(&fileKey, loadOptions);
            task->killCountdown = BOOT_FADE_FULL_BRIGHTNESS;
            brightness          = task->killCountdown;
            fadeDrawOverlay(brightness, brightness, brightness, GPU_BLEND_SUBTRACT);
            task->state++;
            break;

        case BOOT_COLD_START_FADE_IN:
            SetDispMask(1);
            task->killCountdown -= BOOT_FADE_BRIGHTNESS_STEP;
            if (task->killCountdown <= 0) {
                task->killCountdown = 0;
                task->state++;
            }
            brightness = task->killCountdown;
            fadeDrawOverlay(brightness, brightness, brightness, GPU_BLEND_SUBTRACT);
            break;

        case BOOT_COLD_START_HOLD_IMAGE:
            // The image stays visible until both its minimum hold and file loading finish.
            if (task->killCountdown < BOOT_COLD_START_HOLD_FRAMES) {
                task->killCountdown++;
            }
            if (cdCmdIsIdle() == 0) {
                return;
            }
            if (task->killCountdown < BOOT_COLD_START_HOLD_FRAMES) {
                return;
            }
            task->killCountdown = 0;
            task->state++;
            break;

        case BOOT_COLD_START_FADE_OUT:
            task->killCountdown += BOOT_FADE_BRIGHTNESS_STEP;
            if (task->killCountdown >= BOOT_FADE_FINISHED) {
                memFillBytes(Fs_ImgBuffers, 0, sizeof(*Fs_ImgBuffers));
                task->state++;
                break;
            }
            brightness = task->killCountdown;
            fadeDrawOverlay(brightness, brightness, brightness, GPU_BLEND_SUBTRACT);
            break;

        case BOOT_COLD_START_START_TITLE:
            taskSpawn(BOOT_RESIDENT_TASK_BANK, BOOT_START_TITLE_TASK_SLOT, 0, 0);
            taskKill(task);
            SetDispMask(1);
            gDisplayState.debugMode = 0;
            break;
    }
}

void cdAudioCancelAndWait(void)
{
    cdAudioCancel();
    // The VBlank driver must finish the stop before callbacks are reset.
    while (CdAudio_Phase.stopStep != CD_AUDIO_STOP_STEP_DONE) {
    }
}

void bootInitCdAudio(void)
{
    cdAudioInit();
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

    _memConfigureCapturedFrameWorkspace(&areaSlots[areaId], auxHeapOffsetBytes);
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

void bootInitCd(void)
{
    u8 driveMode;

    CdInit();
    driveMode = CdlModeSpeed;
    CdControlB(CdlSetmode, &driveMode, NULL);
    cdAudioInit();
    cdCmdResetState();
}

void bootResetCd(s32 resetMode)
{
    enum { BOOT_CD_RESET_SETTLE_VBLANKS = 3 };
    u8 driveMode;

    // Stop sector delivery before resetting the decoder or detaching its ring.
    CdFlush();
    VSync(BOOT_CD_RESET_SETTLE_VBLANKS);
    CdControlB(CdlPause, NULL, NULL);
    if (Wip_SysFlags.movieStreamActive != 0) {
        _bootReleaseMovieRing();
    }
    CdReset(resetMode);
    driveMode = CdlModeSpeed;
    CdControlB(CdlSetmode, &driveMode, NULL);
    cdCmdResetState();
}

void bootReloadTitleTask(Task* task)
{
    enum {
        BOOT_TITLE_RELOAD_QUEUE       = 0,
        BOOT_TITLE_RELOAD_WAIT        = 1,
        BOOT_TITLE_STARTUP_DESCRIPTOR = 0,
    };
    u8            driveMode[8]; // Only byte 0 is sent; the retained scratch extent preserves stack placement.
    FsFileLoadKey fileKey;
    u8            loadOptions[sizeof(gCdCmdQueue.entries[0].args)];
    s32           state;

    state = task->state;
    switch (state) {
        case BOOT_TITLE_RELOAD_QUEUE:
            driveMode[0] = CdlModeSpeed | CdlModeSize1;
            CdControlB(CdlSetmode, driveMode, NULL);
            SetDispMask(0);
            _bootQueueStartupFile(&fileKey, loadOptions);
            task->state = task->state + 1;
            return;
        case BOOT_TITLE_RELOAD_WAIT:
            if (cdCmdIsIdle() != 0) {
                SetDispMask(1);
                memConfigureImageMemory(GAME_STAGE_NONE, 0);
                taskSpawnFromTable(Title_TaskDescs, BOOT_TITLE_STARTUP_DESCRIPTOR, 0, 0);
                taskKill(task);
                gDisplayState.debugMode = 0;
            }
            return;
    }
}

void cdCmdService(void)
{
    cdCmdDispatch();
}

bool fsIsStageCdfAvailable(u32 stageIndex)
{
    enum { FILE_SYSTEM_STAGE_CDF_ABSENT = 0 };

    return Fs_StageCdfSectors[(u8)stageIndex] != FILE_SYSTEM_STAGE_CDF_ABSENT;
}
