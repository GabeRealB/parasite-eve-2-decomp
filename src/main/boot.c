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
#include "main/gamemain.h"

#include "mapui/map_akropolis.h"

#include "mapui/map_dryfield.h"

#include "mapui/map_dryfield_full.h"

#include "mapui/map_neo_ark.h"

#include "mapui/map_shelter.h"

#include "title/title.h"

/* Define BSS before API headers to preserve first-declaration order. */
void* gMemSavedAuxHeap;

size_t gMemSavedAuxHeapBytes;

u8* gGpuPrimitiveBufferBase;

void* gMemActiveAuxHeap;

size_t gGpuPrimitiveBufferBytes;

size_t gMemActiveAuxHeapBytes;

/// Saved start of the primitive reservation's ten zeroed trailer bytes.
///
/// Updated when image memory is configured. No reader or trailer check is
/// established; ending setup can replace the arena without updating this pointer.
u8* _gMemPrimitiveTrailerStart;

CdCmdQueue gCdCmdQueue;

void* gMemSavedImageRegion;

size_t gMemSavedImageRegionBytes;

#include "main/fs.h"
#include "main/mem.h"
#include "mem.h"
#include "main/stream.h"

/// Exclusive end byte address of the default saved image-memory region.
///
/// The default base plus its saved byte extent reaches this address.
/// Initializes only `_gMemUnusedImageRegionEnd`, which has no reader; memory
/// configuration computes its extents separately. The typed byte pointer
/// denotes a boundary and neither reserves storage nor permits a byte read.
#define MEMORY_IMAGE_REGION_END ((u8*)0x801FD000)

extern u8 D_80400000[];

/// Fixed byte address of actor work buffer 0 (actor-overlay slot 1).
///
/// Initializes `gFsActorBuffer0Base`. Actor packages, scene/movie workspaces
/// and GPU packets share this RAM; previous users must finish before reuse.
/// The address specifies no object layout, capacity or reservation.
#define FILE_SYSTEM_ACTOR_BUFFER_0_BASE ((u8*)0x80131E20)

/// Fixed byte address of actor work buffer 1 (actor-overlay slot 2).
///
/// Initializes `gFsActorBuffer1Base`. Loaded packages, scene/movie workspaces
/// and GPU packets reuse this RAM; previous users must finish before reuse.
/// This address specifies no capacity or reservation.
#define FILE_SYSTEM_ACTOR_BUFFER_1_BASE ((u8*)0x80149E20)

/// Fixed byte address of actor work buffer 2 (actor-overlay slot 3).
///
/// Initializes `gFsActorBuffer2Base`. Actor loads, scene/movie workspaces,
/// serialized demo state and GPU packets share this RAM; prior users must finish
/// before reuse. The address specifies no object layout, capacity or reservation.
#define FILE_SYSTEM_ACTOR_BUFFER_2_BASE ((u8*)0x80161E20)

static const char _gBootBuildStamp[sizeof("2000/05/01 19:24 ver2.49")];

static u8* _gMemUnusedImageRegionEnd;

static u8* _gMemUnusedDevKitAddress;

static const GfxImageSlot* _gGfxImageSlotTablesByStage[6];

/// Retained NUL-terminated ASCII build identification for the resident image.
///
/// No code reader is established. Its bytes precede the boot state jump table
/// in rodata and must retain that position.
static const char _gBootBuildStamp[sizeof("2000/05/01 19:24 ver2.49")] = "2000/05/01 19:24 ver2.49";

FsImgBuffers* gFsImageWorkspacePtr = &gFsImageWorkspace;
/// Retained unused byte pointer to the exclusive image-memory region end.
///
/// No reader or active allocation/stack-bound role is established.
static u8* _gMemUnusedImageRegionEnd = MEMORY_IMAGE_REGION_END;
u8*        gFsActorBuffer0Base       = FILE_SYSTEM_ACTOR_BUFFER_0_BASE;
u8*        gFsActorBuffer1Base       = FILE_SYSTEM_ACTOR_BUFFER_1_BASE;
u8*        gFsActorBuffer2Base       = FILE_SYSTEM_ACTOR_BUFFER_2_BASE;
/// Retained unused byte pointer to development RAM address 0x80400000.
///
/// No reader or allocation-end role is established.
static u8* _gMemUnusedDevKitAddress = D_80400000;

/// Fixed byte base of the default shared image-memory region.
///
/// Normal video mode or stage zero selects this base for the complete saved
/// region and GPU primitives. The active auxiliary heap begins 0x10000 bytes
/// later. This address is also the map-overlay load base: occupants and pending
/// CPU/GPU/decoder users must finish before the region is reclaimed. The macro
/// specifies an address only, with neither an object layout nor an allocation.
#define MEMORY_DEFAULT_IMAGE_REGION_BASE ((u8*)0x80179950)

/// Map-owned image-memory rows indexed directly by stage ID 0..5.
///
/// Stage zero has no row and is used only with the default-memory path.
/// Other stages borrow their loaded map's records, indexed by that stage's
/// area ID: row extents are 22, 39, 39, 50 and 34. Capture/restore and slot
/// memory selection require a nonempty record; holes are not usable storage.
/// Stages 2..5 retain a parking-region record at area zero, of unproven role.
static const GfxImageSlot* _gGfxImageSlotTablesByStage[6] = {
    NULL,
    gMapAkropolisImageSlots,
    gMapDryfieldImageSlots,
    gMapDryfieldFullImageSlots,
    gMapShelterImageSlotsByArea,
    gMapNeoArkImageSlotsByArea,
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
static inline void _bootReleaseMovieRing(void)
{
    enum { BOOT_MOVIE_RESET_CALLBACKS_AND_DECODER = 0 };

    DecDCTReset(BOOT_MOVIE_RESET_CALLBACKS_AND_DECODER);
    StClearRing();
    StUnSetRing();
    gGameMainPersistentState.movieStreamActive = false;
}

/// Clears the trailing ten bytes of the configured GPU primitive reservation.
///
/// `gGpuPrimitiveBufferBase` must address `gGpuPrimitiveBufferBytes` writable bytes, with
/// at least `MEMORY_PRIMITIVE_TRAILER_BYTES` bytes in the region. Stores their
/// start in `_gMemPrimitiveTrailerStart`, which has no reader in the game.
static inline void _memClearPrimitiveTrailer(void)
{
    u8 trailerIndex;

    for (trailerIndex = 0; trailerIndex < MEMORY_PRIMITIVE_TRAILER_BYTES; trailerIndex++) {
        gGpuPrimitiveBufferBase[gGpuPrimitiveBufferBytes - trailerIndex - 1] = 0;
    }
    _gMemPrimitiveTrailerStart = gGpuPrimitiveBufferBase + (gGpuPrimitiveBufferBytes - MEMORY_PRIMITIVE_TRAILER_BYTES);
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
static inline void _memConfigureCapturedFrameWorkspace(const GfxImageSlot* areaSlot, s32 auxHeapOffsetBytes)
{
    u8* workspaceBase;

    // Save both the complete workspace and its auxiliary suffix for heap selection.
    gGpuPrimitiveBufferBytes  = MEMORY_PRIMITIVE_HEAP_BYTES;
    gMemActiveAuxHeapBytes    = MEMORY_PRIMITIVE_HEAP_BYTES - auxHeapOffsetBytes;
    gMemSavedImageRegionBytes = MEMORY_PRIMITIVE_HEAP_BYTES;
    gMemSavedAuxHeapBytes     = gMemActiveAuxHeapBytes;

    workspaceBase           = areaSlot->regionBase + sizeof(FsImgBuffers);
    gGpuPrimitiveBufferBase = workspaceBase;
    gMemActiveAuxHeap       = workspaceBase + auxHeapOffsetBytes;
    gMemSavedImageRegion    = workspaceBase;
    gMemSavedAuxHeap        = workspaceBase + auxHeapOffsetBytes;
}

void memConfigureImageMemory(s32 stageId, s32 areaId)
{
    const GfxImageSlot* areaSlots;

    areaSlots = _gGfxImageSlotTablesByStage[stageId];
    // Initially keep the active heap below the resident image workspace.
    if ((gDisplayState.videoMode == DISPLAY_VIDEO_NORMAL) || (stageId == GAME_STAGE_NONE)) {
        gMemSavedImageRegion      = MEMORY_DEFAULT_IMAGE_REGION_BASE;
        gMemSavedImageRegionBytes = MEMORY_IMAGE_DEFAULT_REGION_BYTES;
        gGpuPrimitiveBufferBase   = MEMORY_DEFAULT_IMAGE_REGION_BASE;
        gMemActiveAuxHeap         = MEMORY_DEFAULT_IMAGE_REGION_BASE + MEMORY_PRIMITIVE_HEAP_BYTES;
        gMemActiveAuxHeapBytes    = MEMORY_IMAGE_DEFAULT_REGION_BYTES - MEMORY_IMAGE_REGION_EXTENSION_BYTES - MEMORY_PRIMITIVE_HEAP_BYTES;
    } else {
        gMemSavedImageRegion      = areaSlots[areaId].regionBase;
        gMemSavedImageRegionBytes = areaSlots[areaId].byteExtent + MEMORY_IMAGE_REGION_EXTENSION_BYTES;
        gGpuPrimitiveBufferBase   = areaSlots[areaId].regionBase;
        gMemActiveAuxHeap         = gGpuPrimitiveBufferBase + MEMORY_PRIMITIVE_HEAP_BYTES;
        gMemActiveAuxHeapBytes    = areaSlots[areaId].byteExtent - MEMORY_PRIMITIVE_HEAP_BYTES;
    }

    // Save the larger heap view that also reclaims the image workspace.
    gGpuPrimitiveBufferBytes = MEMORY_PRIMITIVE_HEAP_BYTES;
    gMemSavedAuxHeap         = gMemActiveAuxHeap;
    gMemSavedAuxHeapBytes    = gMemSavedImageRegionBytes - MEMORY_PRIMITIVE_HEAP_BYTES;
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
                memFillBytes(gFsImageWorkspacePtr, 0, sizeof(*gFsImageWorkspacePtr));
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
    while (gCdAudioProgress.stopStep != CD_AUDIO_STOP_STEP_DONE) {
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

    areaSlots   = _gGfxImageSlotTablesByStage[stageId];
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

    areaSlots = _gGfxImageSlotTablesByStage[stageId];
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
    if (gGameMainPersistentState.movieStreamActive != 0) {
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
                taskSpawnFromTable(gTitleStartupTaskDescriptors, BOOT_TITLE_STARTUP_DESCRIPTOR, 0, 0);
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

    return gFsStageCdfBaseSectors[(u8)stageIndex] != FILE_SYSTEM_STAGE_CDF_ABSENT;
}
