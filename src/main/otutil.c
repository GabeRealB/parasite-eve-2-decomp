#include "display.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libapi.h>
#include <psyq/libetc.h>
#include <psyq/libgs.h>

#include "common.h"

#include "fs.h"
#include "main/display_types.h"
#include "display_types.h"
#include "gamemain.h"
#include "main/mem.h"
#include "mem.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/stage.h"
#include "stage.h"
#include "main/task.h"
#include "main/task_types.h"

#include "gameplay/actor_render.h"
#include "gameplay/item_menu.h"
#include "gameplay/loading.h"

/* Define BSS before API headers to preserve first-declaration order. */
u8 Gpu_PrimBufStatic[0x6000];

/// Byte arena supplying both halves of task-owned presentation primitives.
static u8* _gGpuDisplayPrimBufferBase;

/// Total arena capacity in bytes, split evenly between the two frame buffers.
static s32 _gGpuDisplayPrimBufferBytes;

GsOT Gpu_OrderingTables[2];

TaskNode gTaskDisplayList;

static s32 Display_HoldMode;

static u_long Gpu_SmallOtTags[2 * GPU_SMALL_ORDERING_TABLE_ENTRIES];

#include "main/display.h"
#include "task.h"

static TaskDesc Display_MenuTaskDesc;

static void _displayFlipOtAndDrawViewActors(void);

static void _displayResumeGameLoop(void);

static void _displayFlipOtAndDrawFlaggedModels(void);

static TaskDesc Display_MenuTaskDesc = { { { TASK_BODY_NONE, 0xC0 } }, menuRootTask };

/// Selects the resident small ordering tables and primitive arena for task presentation.
///
/// Each frame slot has 64 tags and 0x3000 bytes of primitive storage. The
/// frame path initializes the selected descriptor's offset, point and tail
/// tag, clears its tags and selects its primitive half before drawing.
/// Storage remains borrowed until GPU drawing completes; packets must fit
/// within the selected half.
static __inline__ void _displayConfigureSmallTaskBuffers(void)
{
    // SDK descriptors view the packed DMA words as tags.
    Gpu_OrderingTables[0].length = GPU_SMALL_ORDERING_TABLE_DEPTH_BITS;
    Gpu_OrderingTables[0].org    = (GsOT_TAG*)Gpu_SmallOtTags;
    Gpu_OrderingTables[1].length = GPU_SMALL_ORDERING_TABLE_DEPTH_BITS;
    Gpu_OrderingTables[1].org    = (GsOT_TAG*)(Gpu_SmallOtTags + GPU_SMALL_ORDERING_TABLE_ENTRIES);
    _gGpuDisplayPrimBufferBase   = Gpu_PrimBufStatic;
    _gGpuDisplayPrimBufferBytes  = sizeof(Gpu_PrimBufStatic);
}

s32 displayRunTaskFrame(GsOT* unusedOrderingTables, s32 frameStartLines, s32 unusedOtBuffer)
{
/// Selects the task frame's buffers and clears its OT before task drawing.
///
/// Captures the writable display pointer and the orderingTables, firstTag,
/// halfBytes and savedOt locals. Requires configured reusable task buffers and
/// a frame index of 0 or 1. Saves the caller's OT for restoration after drawing.
/// Expands to one compound statement; undefined before this function ends.
#define DISPLAY_BEGIN_TASK_FRAME()                                                      \
    {                                                                                   \
        if (display->mdecActive == 0) {                                                 \
            display->frameBuffer ^= 1;                                                  \
        }                                                                               \
        if (display->control.flags.flipMode != DISPLAY_FLIP_HOLD) {                     \
            display->drawBuffer = display->frameBuffer;                                 \
        }                                                                               \
        orderingTables = Gpu_OrderingTables;                                            \
        GsClearOt(0, 0, &orderingTables[display->frameBuffer]);                         \
        firstTag       = (u_long*)orderingTables[display->frameBuffer].org;             \
        halfBytes      = _gGpuDisplayPrimBufferBytes;                                   \
        *firstTag      = GPU_OT_END_PRIM;                                               \
        halfBytes     /= 2;                                                             \
        savedOt        = gGpuCurrentOt;                                                 \
        gGpuCurrentOt  = (u_long*)orderingTables[display->frameBuffer].org;             \
        gGpuPrimCursor = _gGpuDisplayPrimBufferBase + display->frameBuffer * halfBytes; \
    }

    enum {
        DISPLAY_TASK_FRAME_SCANLINE_MASK = 0x7FFF,
        DISPLAY_TASK_FLIP_NONE           = -1,
        DISPLAY_TASK_FLIP_IMMEDIATE      = -2,
    };
    DisplayState* display;
    GsOT*         orderingTables;
    u_long*       savedOt;
    u_long*       firstTag;
    s32           halfBytes;

    // Borrow the task OT and its primitive half until this frame is submitted.
    display = &gDisplayState;
    DISPLAY_BEGIN_TASK_FRAME();
    taskExecList(&gTaskDisplayList);
    cdCmdService();
    if (display->mdecActive == 0) {
        DrawSync(0);
    }
    // Queue an interrupt-side flip within budget; late frames present immediately.
    if (((VSync(1) - frameStartLines) & DISPLAY_TASK_FRAME_SCANLINE_MASK) < D_8005EC6C) {
        EnterCriticalSection();
        display->vsyncFlag  = DISPLAY_VSYNC_TASK;
        Display_PendingFlip = display->frameBuffer;
        // Snapshot presentation controls for the interrupt-side flip.
        D_80070E38 = display->control.flags.flipMode;
        // This non-volatile store must remain eligible for the following call delay slot.
        *(u8*)&D_8006EC30 = display->control.flags.imageSource;
        ExitCriticalSection();
        VSync(D_8005EC68);
        if (Display_PendingFlip != DISPLAY_TASK_FLIP_NONE) {
            D_8005EC78      = 0;
            frameStartLines = VSync(1) & DISPLAY_TASK_FRAME_SCANLINE_MASK;
            displayPresentTaskFrame(display->frameBuffer);
            Display_PendingFlip = DISPLAY_TASK_FLIP_NONE;
        } else {
            D_8005EC78      = D_8005EC74;
            frameStartLines = -D_8005EC74;
        }
    } else {
        D_8005EC78          = 0;
        frameStartLines     = VSync(1) & DISPLAY_TASK_FRAME_SCANLINE_MASK;
        display->vsyncFlag  = DISPLAY_VSYNC_TASK;
        Display_PendingFlip = DISPLAY_TASK_FLIP_IMMEDIATE;
        D_80070E38          = display->control.flags.flipMode;
        *(u8*)&D_8006EC30   = display->control.flags.imageSource;
        displayPresentTaskFrame(display->frameBuffer);
        Display_PendingFlip = DISPLAY_TASK_FLIP_NONE;
    }
    gGpuCurrentOt = savedOt;
    return frameStartLines;
#undef DISPLAY_BEGIN_TASK_FRAME
}

Task* displaySpawnTask(s32 bank, TaskSpawnArg selector, TaskSpawnArg spawnArg1, TaskSpawnArg spawnArg2)
{
    DisplayState* display;
    TaskNode*     previousList;
    Task*         spawnedTask;

    display     = &gDisplayState;
    spawnedTask = NULL;
    if (display->displayOwner == DISPLAY_OWNER_GAME_LOOP) {
        // Prepare the hidden framebuffer and a fresh task-owned drawing list.
        _displayConfigureSmallTaskBuffers();
        display->frameBuffer = display->drawBuffer ^ 1;
        previousList         = taskGetActiveList();
        taskInitList(&gTaskDisplayList);
        spawnedTask = taskSpawn(bank, selector, spawnArg1, spawnArg2);
        // Commit the presentation handoff only after the task has been created.
        if (spawnedTask != NULL) {
            display->pendingMode            = DISPLAY_MODE_BARE_OT;
            display->displayOwner           = DISPLAY_OWNER_TASK;
            display->control.flags.flipMode = DISPLAY_FLIP_FULL;
        }
        taskSetActiveList(previousList);
    }
    return spawnedTask;
}

Task* displaySpawnTaskFromTable(TaskDesc* table, s32 index, TaskSpawnArg spawnArg1, TaskSpawnArg spawnArg2)
{
    DisplayState* display;
    TaskNode*     previousList;
    Task*         spawnedTask;

    display     = &gDisplayState;
    spawnedTask = NULL;
    if (display->displayOwner == DISPLAY_OWNER_GAME_LOOP) {
        // Prepare the hidden framebuffer and a fresh task-owned drawing list.
        _displayConfigureSmallTaskBuffers();
        display->frameBuffer = display->drawBuffer ^ 1;
        previousList         = taskGetActiveList();
        taskInitList(&gTaskDisplayList);
        spawnedTask = taskSpawnFromTable(table, index, spawnArg1, spawnArg2);
        // Commit the presentation handoff only after the task has been created.
        if (spawnedTask != NULL) {
            display->pendingMode            = DISPLAY_MODE_BARE_OT;
            display->displayOwner           = DISPLAY_OWNER_TASK;
            display->control.flags.flipMode = DISPLAY_FLIP_FULL;
        }
        taskSetActiveList(previousList);
    }
    return spawnedTask;
}

Task* taskSpawnOnDefaultList(s32 bank, TaskSpawnArg selector, TaskSpawnArg spawnArg1, TaskSpawnArg spawnArg2)
{
    TaskNode* previousList;
    Task*     spawnedTask;

    previousList = taskGetActiveList();
    taskSetActiveList(&gTaskDefaultList);
    spawnedTask = taskSpawn(bank, selector, spawnArg1, spawnArg2);
    taskSetActiveList(previousList);
    return spawnedTask;
}

Task* taskSpawnFromTableOnDefaultList(TaskDesc* table, s32 index, TaskSpawnArg spawnArg1, TaskSpawnArg spawnArg2)
{
    TaskNode* previousList;
    Task*     spawnedTask;

    previousList = taskGetActiveList();
    taskSetActiveList(&gTaskDefaultList);
    spawnedTask = taskSpawnFromTable(table, index, spawnArg1, spawnArg2);
    taskSetActiveList(previousList);
    return spawnedTask;
}

void displayResumeGameLoop(void)
{
    _displayResumeGameLoop();
}

/// Rebuilds the alternate game ordering table with cached view sprites and active models.
///
/// Requires an OT buffer index of 0 or 1, finished GPU use of the alternate
/// table, loaded view/model data and sufficient live primitive storage.
/// Composes the listed model coordinates before drawing and enables full
/// presentation. Restores the borrowed OT pointer; leaves the primitive cursor
/// and other draw-buffer selectors as the drawing operations leave them.
static void _displayFlipOtAndDrawViewActors(void)
{
    DisplayState* display;
    u_long*       savedOt;
    s32           otBuffer;

    display           = &gDisplayState;
    savedOt           = gGpuCurrentOt;
    otBuffer          = display->otBuffer ^ 1;
    display->otBuffer = otBuffer;
    _gpuBeginOt(otBuffer);
    spriteLinkViewCachedPackets();
    actorRenderComposeAndDrawActiveModels(&Gpu_OtBuffers[display->otBuffer]);
    gGpuCurrentOt                   = savedOt;
    display->control.flags.flipMode = DISPLAY_FLIP_FULL;
}

void displayAcquireMenuHold(void)
{
    DisplayState* display;

    display = &gDisplayState;
    if (display->holdState >= 0) {
        display->holdState |= DISPLAY_HOLD_ACTIVE;
        display->holdCount  = 1;
    } else {
        display->holdCount++;
    }
}

void displayReleaseMenuHold(void)
{
    DisplayState* display;

    display = &gDisplayState;
    if (display->holdState >= 0) {
        display->holdCount = 0;
    } else if (--display->holdCount == 0) {
        display->holdState &= DISPLAY_HOLD_MODE_MASK;
        display->holdCount  = 0;
    }
}

/// Returns selector 2's value 4, selector 3's value 6, or the signed hold byte.
///
/// No caller or writer of `Display_HoldMode` is present in the game. The
/// selector, override values and hold byte's low bits have unproven roles.
static s32 _displayResolveHoldState(void)
{
    switch (Display_HoldMode) {
        case 2:
            return 4;
        case 3:
            return 6;
        case 1: // The binary distinguishes one value below 2; its identity is unproven.
        default:
            return gDisplayState.holdState;
    }
}

void displayInitTaskBuffers(void)
{
    _displayConfigureSmallTaskBuffers();
}

s32 displayDispatchModeRequest(s32 modeRequest)
{
    enum {
        DISPLAY_MENU_REQUEST_ATTACHMENTS = 0x42,
        DISPLAY_MENU_FADE_MAX            = 0x20,
        DISPLAY_MENU_FADE_STEP_PER_TICK  = 8,
        DISPLAY_MAP_FADE_STEP_PER_TICK   = 0x20,
        DISPLAY_DEMO_FADE_STEP_PER_TICK  = 0x10,
        DISPLAY_MODE_FADE_MAX            = 0xFF,
    };

    if (modeRequest >= DISPLAY_MODE_MENU_FIRST) {
        if (modeRequest < DISPLAY_MODE_MENU_LIMIT) {
            // Clear the old request while queueing the menu, then publish its selector.
            gDisplayState.pendingMode = DISPLAY_MODE_NONE;
            // Retain the map's constant argument: merging these calls changes the binary.
            if (modeRequest != DISPLAY_MODE_MAP) {
                displayQueueModeTask(&Display_MenuTaskDesc, modeRequest, 0, STAGE_ENTRY_RELOAD);
            } else {
                displayQueueModeTask(&Display_MenuTaskDesc, DISPLAY_MODE_MAP, 0, STAGE_ENTRY_RELOAD);
            }
            gDisplayState.pendingMode = modeRequest;
            if (gDisplayState.demoScene != DISPLAY_DEMO_NONE) {
                stageSetFadeMax(DISPLAY_MODE_FADE_MAX);
                stageConfigureFade(0, 0, DISPLAY_DEMO_FADE_STEP_PER_TICK, true);
            } else if (modeRequest != DISPLAY_MENU_REQUEST_ATTACHMENTS) {
                if (modeRequest == DISPLAY_MODE_MAP) {
                    stageSetFadeMax(DISPLAY_MODE_FADE_MAX);
                    stageConfigureFade(0, 0, DISPLAY_MAP_FADE_STEP_PER_TICK, true);
                } else {
                    stageSetFadeMax(DISPLAY_MENU_FADE_MAX);
                    stageConfigureFade(0, 0, DISPLAY_MENU_FADE_STEP_PER_TICK, true);
                }
            }
        }
        stageStartModeController();
    }
    return 0;
}

/// Restores session image memory and returns presentation to the game loop.
static void _displayResumeGameLoop(void)
{
    GameSession* session;

    session = gGameSession;
    memConfigureImageMemory(session->location.loc.stage, session->location.loc.area);
    gDisplayState.displayOwner = DISPLAY_OWNER_GAME_LOOP;
    gDisplayState.pendingMode  = DISPLAY_MODE_NONE;
}

/// Rebuilds the alternate game OT with priority-0x62 tasks and flagged models.
///
/// Retained standalone entry with no callers. Requires OT index 0 or 1,
/// finished GPU use of the alternate tags and live task/model resources with
/// sufficient primitive storage. Restores the borrowed OT pointer and enables
/// full presentation; drawing determines the final primitive cursor.
static void _displayFlipOtAndDrawFlaggedModels(void)
{
    enum { DISPLAY_REDRAW_TASK_PRIORITY = 0x62 };
    DisplayState* display;
    u_long*       savedOt;
    s32           otBuffer;

    display           = &gDisplayState;
    savedOt           = gGpuCurrentOt;
    otBuffer          = display->otBuffer ^ 1;
    display->otBuffer = otBuffer;
    // Tasks and models share the game's depth base after its reserved foreground tags.
    gGpuCurrentOt = Gpu_OtTags + otBuffer * GPU_ORDERING_TABLE_BUFFER_ENTRIES;
    gpuClearFrameOrderingTable(display->otBuffer);
    gGpuCurrentOt = gGpuCurrentOt + GPU_ORDERING_TABLE_RESERVED_ENTRIES;
    taskExecListForPriority(&gTaskDefaultList, DISPLAY_REDRAW_TASK_PRIORITY);
    actorRenderComposeAndDrawFlaggedModels(&Gpu_OtBuffers[display->otBuffer]);
    gGpuCurrentOt                   = savedOt;
    display->control.flags.flipMode = DISPLAY_FLIP_FULL;
}

void gpuInitTaskOrderingTables(void)
{
    GsOT*         orderingTables;
    DisplayState* display;
    u_long*       firstTag;

    orderingTables           = Gpu_OrderingTables;
    orderingTables->length   = GPU_ORDERING_TABLE_DEPTH_BITS;
    orderingTables->org      = (GsOT_TAG*)Gpu_OtTags;
    orderingTables[1].length = GPU_ORDERING_TABLE_DEPTH_BITS;
    orderingTables[1].org    = (GsOT_TAG*)(Gpu_OtTags + GPU_ORDERING_TABLE_BUFFER_ENTRIES);
    display                  = &gDisplayState;
    // Select tag zero as the task depth base, without the game loop's foreground bias.
    GsClearOt(0, 0, &orderingTables[display->frameBuffer]);
    firstTag      = (u_long*)orderingTables[display->frameBuffer].org;
    *firstTag     = GPU_OT_END_PRIM;
    gGpuCurrentOt = firstTag;
}

void displayUseHeapTaskPrimitiveBuffer(void)
{
    _gGpuDisplayPrimBufferBytes = MEMORY_PRIMITIVE_HEAP_BYTES;
    _gGpuDisplayPrimBufferBase  = Gpu_PrimHeapBase;
}

void displayUseStaticTaskPrimitiveBuffer(void)
{
    _gGpuDisplayPrimBufferBase  = Gpu_PrimBufStatic;
    _gGpuDisplayPrimBufferBytes = sizeof(Gpu_PrimBufStatic);
}
