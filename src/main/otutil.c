#include "display.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libapi.h>
#include <psyq/libetc.h>
#include <psyq/libgs.h>

#include "common.h"

#include "boot.h"
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

static void Display_FlipOt(void);

static void _displayResumeGameLoop(void);

static void Display_FlipOtAlt(void);

static TaskDesc Display_MenuTaskDesc = { { { TASK_BODY_NONE, 0xC0 } }, Gp_MenuRootTask };

/// Binds two 64-tag task ordering tables and the static primitive arena.
///
/// Previous GPU users must have finished. Only the storage bindings change;
/// tags are cleared and the primitive cursor is selected by the frame path.
static __inline__ void _displayConfigureSmallTaskBuffers(void)
{
    GsOT* orderingTables;

    orderingTables              = Gpu_OrderingTables;
    orderingTables->length      = GPU_SMALL_ORDERING_TABLE_DEPTH_BITS;
    orderingTables->org         = (GsOT_TAG*)Gpu_SmallOtTags;
    orderingTables[1].length    = GPU_SMALL_ORDERING_TABLE_DEPTH_BITS;
    orderingTables[1].org       = (GsOT_TAG*)(Gpu_SmallOtTags + GPU_SMALL_ORDERING_TABLE_ENTRIES);
    _gGpuDisplayPrimBufferBase  = Gpu_PrimBufStatic;
    _gGpuDisplayPrimBufferBytes = sizeof(Gpu_PrimBufStatic);
}

s32 Display_FrameFlipDraw(GsOT* otBufs, s32 frameStart, s32 unused3)
{
    DisplayState* display;
    GsOT*         orderingTables;
    u_long*       savedOt;
    u_long*       firstTag;
    s32           halfBytes;
    s32           noPendingFlip;

    display = &gDisplayState;
    if (display->mdecActive == 0) {
        display->frameBuffer ^= 1;
    }
    if (display->control.flags.flipMode != DISPLAY_FLIP_HOLD) {
        display->drawBuffer = display->frameBuffer;
    }
    orderingTables = Gpu_OrderingTables;
    GsClearOt(0, 0, &orderingTables[display->frameBuffer]);
    firstTag       = orderingTables[display->frameBuffer].org;
    halfBytes      = _gGpuDisplayPrimBufferBytes;
    *firstTag      = GPU_OT_END_PRIM;
    halfBytes     /= 2;
    savedOt        = gGpuCurrentOt;
    gGpuCurrentOt  = orderingTables[display->frameBuffer].org;
    gGpuPrimCursor = _gGpuDisplayPrimBufferBase + display->frameBuffer * halfBytes;
    taskExecList(&gTaskDisplayList);
    Boot_DispatchCdCmd();
    if (display->mdecActive == 0) {
        DrawSync(0);
    }
    if (((VSync(1) - frameStart) & 0x7FFF) < D_8005EC6C) {
        EnterCriticalSection();
        display->vsyncFlag  = DISPLAY_VSYNC_TASK;
        Display_PendingFlip = display->frameBuffer;
        // Snapshot presentation controls for the interrupt-side flip.
        D_80070E38 = display->control.flags.flipMode;
        // This non-volatile store must remain eligible for the following call delay slot.
        *(u8*)&D_8006EC30 = display->control.flags.imageSource;
        ExitCriticalSection();
        VSync(D_8005EC68);
        noPendingFlip = -1;
        if (Display_PendingFlip != noPendingFlip) {
            D_8005EC78 = 0;
            frameStart = VSync(1) & 0x7FFF;
            Display_FlipDraw(display->frameBuffer);
            Display_PendingFlip = noPendingFlip;
        } else {
            D_8005EC78 = D_8005EC74;
            frameStart = -D_8005EC74;
        }
    } else {
        D_8005EC78          = 0;
        frameStart          = VSync(1) & 0x7FFF;
        display->vsyncFlag  = DISPLAY_VSYNC_TASK;
        Display_PendingFlip = -2;
        D_80070E38          = display->control.flags.flipMode;
        *(u8*)&D_8006EC30   = display->control.flags.imageSource;
        Display_FlipDraw(display->frameBuffer);
        Display_PendingFlip = -1;
    }
    gGpuCurrentOt = savedOt;
    return frameStart;
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

static void Display_FlipOt(void)
{
    DisplayState* temp;
    u_long*       saved;
    s32           buf;

    temp           = &gDisplayState;
    saved          = gGpuCurrentOt;
    buf            = temp->otBuffer ^ 1;
    temp->otBuffer = buf;
    gpuBeginOt(buf);
    spriteLinkViewCachedPackets();
    actorRenderComposeAndDrawActiveModels(&Gpu_OtBuffers[temp->otBuffer]);
    gGpuCurrentOt                = saved;
    temp->control.flags.flipMode = DISPLAY_FLIP_FULL;
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

s32 Display_DispatchModeId(s32 arg0)
{
    if (arg0 >= DISPLAY_MODE_MENU_FIRST) {
        if (arg0 < DISPLAY_MODE_MENU_LIMIT) {
            gDisplayState.pendingMode = DISPLAY_MODE_NONE;
            if (arg0 != 0x43) {
                displayQueueModeTask(&Display_MenuTaskDesc, arg0, 0, STAGE_ENTRY_RELOAD);
            } else {
                displayQueueModeTask(&Display_MenuTaskDesc, 0x43, 0, STAGE_ENTRY_RELOAD);
            }
            gDisplayState.pendingMode = arg0;
            if (gDisplayState.demoScene != DISPLAY_DEMO_NONE) {
                Stage_SetFadeMax(0xFF);
                Stage_SetFadeRate(0, 0, 0x10, 1);
            } else if (arg0 != 0x42) {
                if (arg0 == 0x43) {
                    Stage_SetFadeMax(0xFF);
                    Stage_SetFadeRate(0, 0, 0x20, 1);
                } else {
                    Stage_SetFadeMax(0x20);
                    Stage_SetFadeRate(0, 0, 8, 1);
                }
            }
        }
        Stage_InitOtAndSpawn();
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

static void Display_FlipOtAlt(void)
{
    DisplayState* temp;
    u_long*       saved;
    s32           buf;

    temp           = &gDisplayState;
    saved          = gGpuCurrentOt;
    buf            = temp->otBuffer ^ 1;
    temp->otBuffer = buf;
    gGpuCurrentOt  = Gpu_OtTags + buf * GPU_ORDERING_TABLE_BUFFER_ENTRIES;
    gpuClearFrameOrderingTable(temp->otBuffer);
    gGpuCurrentOt = gGpuCurrentOt + GPU_ORDERING_TABLE_RESERVED_ENTRIES;
    taskExecListForPriority(&gTaskDefaultList, 0x62);
    Gp_DrawActorTmdFlagged(&Gpu_OtBuffers[temp->otBuffer]);
    gGpuCurrentOt                = saved;
    temp->control.flags.flipMode = DISPLAY_FLIP_FULL;
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
