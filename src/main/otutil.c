#include "display_types.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libapi.h>
#include <psyq/libetc.h>
#include <psyq/libgs.h>

#include "common.h"

#include "fs.h"
#include "main/display_types.h"
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
u8 gDisplayStaticPrimitiveArena[DISPLAY_STATIC_PRIMITIVE_ARENA_BYTES];

/// Borrowed byte arena bound for modal task presentation.
///
/// Paired with `_gDisplayTaskPrimitiveArenaBytes`; storage belongs to the
/// resident static arena or the configured image-memory reservation. Previous
/// GPU work must finish before rebinding or reusing either region.
u8* _gDisplayTaskPrimitiveArenaBase;

/// Signed total-byte capacity of the bound display-task primitive arena.
///
/// Configured as 0x6000 or 0x10000 bytes, divided into two equal frame halves.
/// The frame path preserves signed division when deriving its byte stride.
s32 _gDisplayTaskPrimitiveArenaBytes;

GsOT gGpuTaskOrderingTables[2];

TaskNode gDisplayTaskList;

/// Selector for the unused signed hold-state resolver; no producer is present.
///
/// Values 2/3 return overrides 4/6, and other values return the display hold
/// byte. The selector values' original roles are unproven; BSS starts at zero.
s32 _gDisplayHoldStateSelector;

/// Two resident 64-word DMA tag banks for modal task presentation.
///
/// Each four-byte word carries a 24-bit DMA link and 8-bit packet length.
/// The SDK borrows each half through its `GsOT_TAG` view until drawing ends.
u_long _gDisplaySmallTaskOtTags[2 * GPU_SMALL_ORDERING_TABLE_ENTRIES];

#include "main/display.h"
#include "display.h"
#include "task.h"

static void _displayFlipOtAndDrawViewActors(void);

static void _displayResumeGameLoop(void);

static void _displayFlipOtAndDrawFlaggedModels(void);

/// Bodyless gameplay menu-root recipe queued by resident display-mode requests.
///
/// The mode request becomes spawnArg1 and spawnArg2 is zero. Gameplay menu
/// code must be loaded when the queued callback runs. The descriptor itself
/// has program lifetime and is copied synchronously when the task is spawned.
static TaskDesc _gDisplayMenuTaskDesc = {
    .header   = { .fields = { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } },
    .callback = menuRootTask,
};

/// Selects the resident small ordering tables and primitive arena for task presentation.
///
/// Each frame slot has 64 tags and 0x3000 bytes of primitive storage. The
/// frame path initializes the selected descriptor's offset, point and tail
/// tag, clears its tags and selects its primitive half before drawing.
/// Storage remains borrowed until GPU drawing completes; packets must fit
/// within the selected half.
static inline void _displayConfigureSmallTaskBuffers(void)
{
    // SDK descriptors view the packed DMA words as tags.
    gGpuTaskOrderingTables[0].length = GPU_SMALL_ORDERING_TABLE_DEPTH_BITS;
    gGpuTaskOrderingTables[0].org    = (GsOT_TAG*)_gDisplaySmallTaskOtTags;
    gGpuTaskOrderingTables[1].length = GPU_SMALL_ORDERING_TABLE_DEPTH_BITS;
    gGpuTaskOrderingTables[1].org    = (GsOT_TAG*)(_gDisplaySmallTaskOtTags + GPU_SMALL_ORDERING_TABLE_ENTRIES);
    _gDisplayTaskPrimitiveArenaBase  = gDisplayStaticPrimitiveArena;
    _gDisplayTaskPrimitiveArenaBytes = sizeof(gDisplayStaticPrimitiveArena);
}

s32 displayRunTaskFrame(GsOT* unusedOrderingTables, s32 frameStartLines, s32 unusedOtBuffer)
{
/// Selects and clears a display-task ordering table and borrows its primitive half.
///
/// `displayState` is a stable writable `DisplayState` pointer, evaluated repeatedly;
/// `previousOt` is a u_long* lvalue in caller storage, assigned once. Inputs must
/// have no side effects and must not name the macro's `orderingTables`, `firstTag`
/// or `halfBytes`.
/// Requires frame index 0/1, two configured task OTs and a live arena split into
/// equal, word-aligned byte halves. Prior GPU work on the selected storage must
/// have finished.
/// MDEC holds the frame index; display hold preserves the draw-buffer selector.
/// Saves the previous global OT for the caller to restore after submission;
/// the primitive cursor selects a half using signed division of the arena's
/// total byte capacity by two.
/// Captures no caller locals. Expands to one compound statement and performs
/// no arena-capacity check; packets must remain live through GPU completion.
#define DISPLAY_BEGIN_TASK_FRAME(displayState, previousOt)                                          \
    {                                                                                               \
        GsOT*   orderingTables;                                                                     \
        u_long* firstTag;                                                                           \
        s32     halfBytes;                                                                          \
        if ((displayState)->mdecActive == 0) {                                                      \
            (displayState)->frameBuffer ^= 1;                                                       \
        }                                                                                           \
        if ((displayState)->control.flags.flipMode != DISPLAY_FLIP_HOLD) {                          \
            (displayState)->drawBuffer = (displayState)->frameBuffer;                               \
        }                                                                                           \
        orderingTables = gGpuTaskOrderingTables;                                                    \
        GsClearOt(0, 0, &orderingTables[(displayState)->frameBuffer]);                              \
        firstTag       = (u_long*)orderingTables[(displayState)->frameBuffer].org;                  \
        halfBytes      = _gDisplayTaskPrimitiveArenaBytes;                                          \
        *firstTag      = GPU_DMA_CHAIN_END;                                                         \
        halfBytes     /= 2;                                                                         \
        (previousOt)   = gGpuCurrentOt;                                                             \
        gGpuCurrentOt  = (u_long*)orderingTables[(displayState)->frameBuffer].org;                  \
        gGpuPrimCursor = _gDisplayTaskPrimitiveArenaBase + (displayState)->frameBuffer * halfBytes; \
    }

    enum {
        DISPLAY_TASK_FRAME_SCANLINE_MASK = 0x7FFF,
        DISPLAY_TASK_FLIP_NONE           = -1,
        DISPLAY_TASK_FLIP_IMMEDIATE      = -2,
    };
    DisplayState* display;
    u_long*       savedOt;

    // Borrow the task OT and its primitive half until this frame is submitted.
    display = &gDisplayState;
    DISPLAY_BEGIN_TASK_FRAME(display, savedOt);
    taskExecList(&gDisplayTaskList);
    cdCmdService();
    if (display->mdecActive == 0) {
        DrawSync(0);
    }
    // Queue an interrupt-side flip within budget; late frames present immediately.
    if (((VSync(1) - frameStartLines) & DISPLAY_TASK_FRAME_SCANLINE_MASK) < gDisplayFrameBudgetLines) {
        EnterCriticalSection();
        display->vsyncFlag  = DISPLAY_VSYNC_TASK;
        gDisplayPendingFlip = display->frameBuffer;
        // Snapshot presentation controls for the interrupt-side flip.
        gDisplayTaskFlipControlSnapshot = display->control.flags.flipMode;
        // This non-volatile store must remain eligible for the following call delay slot.
        *(u8*)&gDisplayTaskImageSourceSnapshot = display->control.flags.imageSource;
        ExitCriticalSection();
        VSync(gDisplayVSyncWaitMode);
        if (gDisplayPendingFlip != DISPLAY_TASK_FLIP_NONE) {
            gDisplayPreviousVSyncElapsedLines = 0;
            frameStartLines                   = VSync(1) & DISPLAY_TASK_FRAME_SCANLINE_MASK;
            displayPresentTaskFrame(display->frameBuffer);
            gDisplayPendingFlip = DISPLAY_TASK_FLIP_NONE;
        } else {
            gDisplayPreviousVSyncElapsedLines = gDisplayVSyncElapsedLines;
            frameStartLines                   = -gDisplayVSyncElapsedLines;
        }
    } else {
        gDisplayPreviousVSyncElapsedLines      = 0;
        frameStartLines                        = VSync(1) & DISPLAY_TASK_FRAME_SCANLINE_MASK;
        display->vsyncFlag                     = DISPLAY_VSYNC_TASK;
        gDisplayPendingFlip                    = DISPLAY_TASK_FLIP_IMMEDIATE;
        gDisplayTaskFlipControlSnapshot        = display->control.flags.flipMode;
        *(u8*)&gDisplayTaskImageSourceSnapshot = display->control.flags.imageSource;
        displayPresentTaskFrame(display->frameBuffer);
        gDisplayPendingFlip = DISPLAY_TASK_FLIP_NONE;
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
        taskInitList(&gDisplayTaskList);
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
        taskInitList(&gDisplayTaskList);
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
    actorRenderComposeAndDrawActiveModels(&gGpuFrameOrderingTables[display->otBuffer]);
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
/// No caller or writer of `_gDisplayHoldStateSelector` is present in the game. The
/// selector, override values and hold byte's low bits have unproven roles.
static s32 _displayResolveHoldState(void)
{
    switch (_gDisplayHoldStateSelector) {
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
                displayQueueModeTask(&_gDisplayMenuTaskDesc, modeRequest, 0, STAGE_ENTRY_RELOAD);
            } else {
                displayQueueModeTask(&_gDisplayMenuTaskDesc, DISPLAY_MODE_MAP, 0, STAGE_ENTRY_RELOAD);
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
    memConfigureImageMemory(session->location.stage, session->location.area);
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
    DisplayState* display;
    u_long*       savedOt;
    s32           otBuffer;

    display           = &gDisplayState;
    savedOt           = gGpuCurrentOt;
    otBuffer          = display->otBuffer ^ 1;
    display->otBuffer = otBuffer;
    // Tasks and models share the game's depth base after its reserved foreground tags.
    gGpuCurrentOt = gGpuFrameOtTags[otBuffer];
    gpuClearFrameOrderingTable(display->otBuffer);
    gGpuCurrentOt = gGpuCurrentOt + GPU_ORDERING_TABLE_RESERVED_ENTRIES;
    taskExecListForPriority(&gTaskDefaultList, TASK_PRIORITY_REDRAW);
    actorRenderComposeAndDrawFlaggedModels(&gGpuFrameOrderingTables[display->otBuffer]);
    gGpuCurrentOt                   = savedOt;
    display->control.flags.flipMode = DISPLAY_FLIP_FULL;
}

void gpuInitTaskOrderingTables(void)
{
    GsOT*         orderingTables;
    DisplayState* display;
    u_long*       firstTag;

    orderingTables           = gGpuTaskOrderingTables;
    orderingTables->length   = GPU_ORDERING_TABLE_DEPTH_BITS;
    orderingTables->org      = (GsOT_TAG*)gGpuFrameOtTags[0];
    orderingTables[1].length = GPU_ORDERING_TABLE_DEPTH_BITS;
    orderingTables[1].org    = (GsOT_TAG*)gGpuFrameOtTags[1];
    display                  = &gDisplayState;
    // Select tag zero as the task depth base, without the game loop's foreground bias.
    GsClearOt(0, 0, &orderingTables[display->frameBuffer]);
    firstTag      = (u_long*)orderingTables[display->frameBuffer].org;
    *firstTag     = GPU_DMA_CHAIN_END;
    gGpuCurrentOt = firstTag;
}

void displayUseHeapTaskPrimitiveBuffer(void)
{
    _gDisplayTaskPrimitiveArenaBytes = MEMORY_PRIMITIVE_HEAP_BYTES;
    _gDisplayTaskPrimitiveArenaBase  = gGpuPrimitiveBufferBase;
}

void displayUseStaticTaskPrimitiveBuffer(void)
{
    _gDisplayTaskPrimitiveArenaBase  = gDisplayStaticPrimitiveArena;
    _gDisplayTaskPrimitiveArenaBytes = sizeof(gDisplayStaticPrimitiveArena);
}
