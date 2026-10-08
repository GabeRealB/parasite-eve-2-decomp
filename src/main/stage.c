#include "main/stage.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libpress.h>

#include "common.h"

#include "main/coord.h"
#include "main/display.h"
#include "display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "fs.h"
#include "main/fs_types.h"
#include "fs_types.h"
#include "main/game_debug_types.h"
#include "gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "mem.h"
#include "main/pad.h"
#include "main/pad_types.h"
#include "main/session.h"
#include "main/session_types.h"
#include "stage.h"
#include "main/stream.h"
#include "stream.h"
#include "main/task.h"
#include "task.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"

#include "gameplay/actor_render.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/loading.h"
#include "gameplay/world_collision.h"

/// Vertical spacing of the two 240-row VRAM framebuffers, in rows.
enum { DISPLAY_FRAMEBUFFER_STRIDE_ROWS = 272 };

/// Ordering-table contents while a mode task owns the frame.
///
/// A view transition also uses this byte as its step selector. Zero skips the
/// intermediate step; any other value takes it. Kind 7 selects no extra draw.
enum {
    STAGE_TRANSITION_NONE      = 0,
    STAGE_TRANSITION_FILTERED  = 1,    // Priority-filtered tasks, view sprites and flagged actors
    STAGE_TRANSITION_ACTORS    = 2,    // View sprites and active actors
    STAGE_TRANSITION_TASKS     = 3,    // Full default task list
    STAGE_TRANSITION_KIND_7    = 7,
    STAGE_TRANSITION_TASKS_ALT = 0x20, // Same ordering-table contents as the full task list
};

/// Bits of the fade-overlay flag byte.
enum {
    STAGE_FADE_ADDITIVE   = 0x01, // Add the grey tile; otherwise subtract it
    STAGE_FADE_FRONT      = 0x02, // Link the tile at the front of the ordering table
    STAGE_FADE_SKIP       = 0x80, // Skip one fade step, then clear this bit
    STAGE_FADE_SKIP_CLEAR = 0x7F, // Flag byte with the skip bit removed
};

/// Step stored when a fade is started with a zero step, and the level that fills a channel.
enum {
    STAGE_FADE_DEFAULT_STEP = 0x20,
    STAGE_FADE_OPAQUE       = 0xFF,
};

/// Work the transition task still has to do. The ending request is the sign bit.
enum {
    STAGE_REQUEST_FILE_LOAD  = 0x08000000, // Run the file-load transition
    STAGE_REQUEST_KEEP_VIEW  = 0x10000000, // View transition reuses the current view
    STAGE_REQUEST_CAPTURE    = 0x20000000, // Store the current framebuffer
    STAGE_REQUEST_TRANSITION = 0x40000000, // Change to the pending view
};

/// Active view-transition or file-load request, and a view change that keeps the current view.
enum {
    STAGE_REQUEST_BUSY            = STAGE_REQUEST_TRANSITION | STAGE_REQUEST_FILE_LOAD,
    STAGE_REQUEST_KEEP_TRANSITION = STAGE_REQUEST_TRANSITION | STAGE_REQUEST_KEEP_VIEW,
};

/// Ending request, the sign bit of the request word.
#define STAGE_REQUEST_ENDING 0x80000000u

/// Resident state for a stage transition, the grey fade overlay and one queued mode task.
///
/// `Stage_Ctx` addresses the single `Stage_Context` object. A mode request
/// stores a task descriptor and its two spawn words until the CD queue is idle,
/// then spawns that descriptor's entry 0. The three unaccessed bytes have no
/// established role.
typedef struct {
    TaskDesc*    taskDesc;           // Descriptor table; the mode task spawns entry 0
    s32          spawnArg1;          // First payload word, copied to the spawned task
    TaskSpawnArg spawnArg2;          // Second payload word, copied to the spawned task
    u32          entryMode;          // Presentation mode (0 reload, 1 keep, 3 hold, 4 draw actors, 0x100 grey capture)
    byte         unknown_10;         // Role unproven; no direct access
    u8           transitionKind;     // OT contents and view-transition step (0 none, 1 filtered, 2 actors, 3 tasks, 7 kind 7, 0x20 tasks)
    u8           loadBuffersCleared; // 1 after the file-load transition clears both framebuffers; 0 after a view transition stores its image
    u8           fullOtReady;        // 0 small ordering table, 1 after the full table is initialized
    u8           largePrimBuf;       // 0 static primitive buffer, 1 the 0x10000 heap buffer
    u8           otFlipArmed;        // 1 after a keep-resources spawn, so a later flip may change the transition kind
    byte         unknown_16;         // Role unproven; no direct access
    u8           fadeLevel;          // Current overlay grey, clamped to 0..fadeMax
    u8           fadeStep;           // Signed per-tick step held in a byte; the stepper sign-extends it and negation zero-extends it
    u8           fadeFlags;          // Overlay bits (1 additive, 2 front of the OT, 0x80 skip one step)
    u8           fadeMax;            // Level at which the fade is complete; STAGE_FADE_OPAQUE until lowered
    byte         unknown_1b;         // Role unproven; no direct access
    u32          requestFlags;       // Pending work (file load, keep view, capture, view transition, ending)
    s32          pendingView;        // View slot copied into the session when a view transition starts
    s32          heldFrameBuffer;    // Framebuffer index captured for the current step; the step waits until presentation leaves it
    u32          transitionStep;     // Step of the view transition or the file-load transition
    u8           loadFileKey[8];     // File-load command block; the enqueue reads bytes 0, 2 and 3, and nothing extracted writes it
    u8           loadFileArgs[4];    // Four argument bytes of that command; nothing extracted writes them
} StageCtx;
STATIC_ASSERT_SIZEOF(StageCtx, 0x38);

static StageCtx Stage_Context;

static s32 D_8007A358;

static u16 D_8007A35C;

static u16 D_8007A35E;

static void* D_8007A360;

static u8* Mdec_DecodeBase;

static StreamSceneImageHeader* Stage_CdEntry;

/// Resident storage for diagnostic controls, input overrides and loading status.
///
/// Shared through `Pad_RemapState` and cleared at main-loop initialization;
/// its lifetime spans gameplay and room overlay loads.
static GameDebugState _gGameDebugStateStorage;

/// Active stage/flow context pointer.
static StageCtx* Stage_Ctx;

static TaskDesc Display_ModeTaskDesc;

static const TaskFuncTable6 Display_TaskStates;

void func_80701470(Task* arg0);

static void _stageStepFadeOverlay(void);

static s32 _stageStepFileLoadTransition(Task* unused);

static Task* _stageSpawnModeTask(void);

static void Display_TransitionTask(Task* task);

static void _stageFlipOtAndRedraw(s32 unused);

static void _gfxInvertCapturedFrameGray(void);

static s32 _stageRequestCurrentViewTransition(void);

static void _stageSetTransitionKindAndRedraw(u8 transitionKind);

static void _stageSuspendCdAndSpawnModeTask(Task* task);

static void _stageWaitCdAndSpawnModeTask(Task* task);

static void _stageBeginModeExitLoad(Task* task);

static void _stageWaitModeExitLoad(Task* task);

static void _stageResumeMovieAndFinishModeTask(Task* task);

static void Display_DispatchTaskTable(Task* task);

static __inline__ void _mdecFinishImageDecode(void);

static void _mdecStepSceneImageDecode(void);

static void _mdecStepStandaloneImageDecode(void);

static void _mdecImageStripCallback(void);

// resolved decode base (mdecRequestSceneImageDecode)
// matched gCdCmdQueue.sceneImageHeaders entry

/// Active stage/flow context pointer.
static StageCtx* Stage_Ctx            = &Stage_Context;
static TaskDesc  Display_ModeTaskDesc = { { { TASK_BODY_NONE, 0 } }, Display_DispatchTaskTable };
GameDebugState*  Pad_RemapState       = &_gGameDebugStateStorage;
TaskDesc         D_800626AC[]         = {
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, taskDebugLaunchCallback },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, func_80701470 },
};

static const TaskFuncTable6 Display_TaskStates = { {
    _stageSuspendCdAndSpawnModeTask,
    _stageWaitCdAndSpawnModeTask,
    Display_TransitionTask,
    _stageBeginModeExitLoad,
    _stageWaitModeExitLoad,
    _stageResumeMovieAndFinishModeTask,
} };

/// Links the current grey fade tile behind its add/subtract draw-mode command.
///
/// Appends a 320x240 semitransparent TILE and DR_TPAGE to the current primitive
/// arena, which must have room for both. Uses depth zero or the last entry of
/// the selected small/full ordering table. The tile's origin compensates the
/// display's draw-origin Y offset; the draw command executes before the tile.
static __inline__ void _stageAppendFadeOverlay(void)
{
    enum {
        STAGE_FADE_SEMITRANSPARENT_TILE_CODE = 0x62,
        STAGE_FADE_TILE_COMMAND_WORDS        = (sizeof(TILE) - sizeof(u_long)) / sizeof(u_long),
    };
    s32       otIndex;
    s32       drawOriginYOffset;
    u8        fadeLevel;
    TILE*     tile;
    DR_TPAGE* drawModePacket;

    otIndex = 0;
    if (Stage_Ctx->fadeFlags & STAGE_FADE_FRONT) {
        otIndex = (1 << GPU_ORDERING_TABLE_DEPTH_BITS) - 1;
        if (Stage_Ctx->fullOtReady == 0) {
            otIndex = GPU_SMALL_ORDERING_TABLE_ENTRIES - 1;
        }
    }

    tile              = gGpuPrimCursor;
    gGpuPrimCursor    = tile + 1;
    drawOriginYOffset = gDisplayState.vramYOffset;
    setlen(tile, STAGE_FADE_TILE_COMMAND_WORDS);
    setcode(tile, STAGE_FADE_SEMITRANSPARENT_TILE_CODE);
    tile->x0  = -FILE_SYSTEM_IMAGE_WIDTH / 2;
    tile->y0  = -FILE_SYSTEM_IMAGE_HEIGHT / 2 - drawOriginYOffset;
    tile->w   = FILE_SYSTEM_IMAGE_WIDTH;
    tile->h   = FILE_SYSTEM_IMAGE_HEIGHT;
    fadeLevel = Stage_Ctx->fadeLevel;
    tile->b0  = fadeLevel;
    tile->g0  = fadeLevel;
    tile->r0  = fadeLevel;

    drawModePacket = gGpuPrimCursor;
    gGpuPrimCursor = drawModePacket + 1;
    if (!(Stage_Ctx->fadeFlags & STAGE_FADE_ADDITIVE)) {
        setlen(drawModePacket, ARRAY_SIZE(drawModePacket->code));
        drawModePacket->code[0] = _get_mode(0, 1, getTPage(0, GPU_BLEND_SUBTRACT, 0, 0));
    } else {
        setlen(drawModePacket, ARRAY_SIZE(drawModePacket->code));
        drawModePacket->code[0] = _get_mode(0, 1, getTPage(0, GPU_BLEND_ADD, 0, 0));
    }

    addPrim(&gGpuCurrentOt[otIndex], tile);
    addPrim(&gGpuCurrentOt[otIndex], drawModePacket);
}

/// Advances the stage fade by nominal frame ticks and links its grey blend tile.
///
/// The skip bit suppresses both phases once. Holding presentation stops level
/// changes but still draws a nonzero overlay. The byte step is signed and the
/// level saturates at zero or fadeMax. Requires room for a TILE and DR_TPAGE in
/// the primitive arena and the ordering table selected by fullOtReady.
static void _stageStepFadeOverlay(void)
{
    StageCtx* stage;
    s32       signedStep;
    s32       nextLevel;
    s32       levelDelta;
    u8        maxLevel;

    stage = Stage_Ctx;
    if ((s8)stage->fadeFlags & STAGE_FADE_SKIP) {
        stage->fadeFlags = stage->fadeFlags & STAGE_FADE_SKIP_CLEAR;
        return;
    }

    // Hold freezes the level; the overlay still participates in drawing.
    if (gDisplayState.control.flags.flipMode != DISPLAY_FLIP_HOLD) {
        signedStep = (s8)stage->fadeStep;
        if (signedStep != 0) {
            levelDelta = signedStep * gDisplayState.frameTicks;
            nextLevel  = stage->fadeLevel;
            nextLevel  = nextLevel + levelDelta;
            if (nextLevel <= 0) {
                stage->fadeLevel    = 0;
                Stage_Ctx->fadeStep = 0;
            } else {
                maxLevel = stage->fadeMax;
                if (nextLevel >= (s32)maxLevel) {
                    stage->fadeLevel    = maxLevel;
                    Stage_Ctx->fadeStep = 0;
                } else {
                    stage->fadeLevel = (u8)nextLevel;
                }
            }
        }
    }

    // Link the blend command ahead of the tile at the selected ordering depth.
    if (Stage_Ctx->fadeLevel != 0) {
        _stageAppendFadeOverlay();
    }
}

/// Steps a queued file load while replacing and clearing the room frame.
///
/// Hides presentation, waits for the CD queue and a framebuffer change, then
/// captures the new room frame before clearing both VRAM buffers. Completion
/// selects task-only drawing with transition strips and consumes the request.
/// The task argument is unused and the retained return value is always 1.
static s32 _stageStepFileLoadTransition(Task* unused)
{
/// Clears and synchronizes both 320x240 VRAM framebuffers before publishing the latch.
///
/// Captures the writable RECT local frameRect and gDisplayState. The framebuffer
/// index must be 0 or 1. Takes no arguments; expands to a compound statement and
/// is undefined after this function. Each reset remains necessary across SDK calls.
#define STAGE_CLEAR_LOAD_FRAMEBUFFERS()                                                  \
    {                                                                                    \
        frameRect.x = 0;                                                                 \
        frameRect.w = FILE_SYSTEM_IMAGE_WIDTH;                                           \
        frameRect.h = FILE_SYSTEM_IMAGE_HEIGHT;                                          \
        frameRect.y = (gDisplayState.frameBuffer ^ 1) * DISPLAY_FRAMEBUFFER_STRIDE_ROWS; \
        ClearImage(&frameRect, 0, 0, 0);                                                 \
        frameRect.x = 0;                                                                 \
        frameRect.w = FILE_SYSTEM_IMAGE_WIDTH;                                           \
        frameRect.h = FILE_SYSTEM_IMAGE_HEIGHT;                                          \
        frameRect.y = gDisplayState.frameBuffer * DISPLAY_FRAMEBUFFER_STRIDE_ROWS;       \
        ClearImage(&frameRect, 0, 0, 0);                                                 \
        DrawSync(0);                                                                     \
    }
    enum {
        STAGE_FILE_LOAD_BEGIN        = 0,
        STAGE_FILE_LOAD_WAIT_QUEUE   = 1,
        STAGE_FILE_LOAD_WAIT_FRAME   = 2,
        STAGE_FILE_LOAD_RESTORE_DRAW = 3,
    };
    RECT frameRect;

    switch (Stage_Ctx->transitionStep) {
        case STAGE_FILE_LOAD_BEGIN:
            SetDispMask(0);
            Stage_Ctx->heldFrameBuffer = gDisplayState.frameBuffer;
            gDisplayState.keepGraphics = 1;
            gfxRestoreAreaFrame(gGameSession->location.loc.stage, gGameSession->location.loc.area, gDisplayState.frameBuffer);
            gDisplayState.control.flags.flipMode = DISPLAY_FLIP_HOLD;
            Stage_Ctx->transitionStep            = Stage_Ctx->transitionStep + 1;
            break;
        case STAGE_FILE_LOAD_WAIT_QUEUE:
            if (cdCmdIsIdle() & 0xFFFF) {
                cdCmdEnqueue(CD_COMMAND_LOAD_FILE, Stage_Ctx->loadFileKey, Stage_Ctx->loadFileArgs);
                Stage_Ctx->transitionStep = Stage_Ctx->transitionStep + 1;
            }
            break;
        case STAGE_FILE_LOAD_WAIT_FRAME:
            if ((cdCmdIsIdle() & 0xFFFF) && (gDisplayState.frameBuffer != Stage_Ctx->heldFrameBuffer)) {
                gfxCaptureAreaFrame(gGameSession->location.loc.stage, gGameSession->location.loc.area, gDisplayState.frameBuffer,
                                    MEMORY_PRIMITIVE_HEAP_BYTES);
                memInitAuxHeap();
                STAGE_CLEAR_LOAD_FRAMEBUFFERS();
                Stage_Ctx->loadBuffersCleared = 1;
                Stage_Ctx->transitionStep     = Stage_Ctx->transitionStep + 1;
            }
            break;
        case STAGE_FILE_LOAD_RESTORE_DRAW:
            gDisplayState.control.flags.flipMode    = DISPLAY_FLIP_TASK_ONLY;
            gDisplayState.control.flags.imageSource = DISPLAY_IMAGE_TRANSITION_STRIPS;
            Stage_Ctx->transitionStep               = Stage_Ctx->transitionStep + 1;
            /* fallthrough */
        default:
            SetDispMask(1);
            Stage_Ctx->requestFlags = Stage_Ctx->requestFlags & ~STAGE_REQUEST_FILE_LOAD;
            break;
    }
    return 1;
}
#undef STAGE_CLEAR_LOAD_FRAMEBUFFERS

/// Applies the mode-entry correction and retires the player's shared contacts.
///
/// Requires the live player model and work. Correction reads only six entries;
/// clearing follows the final-entry marker in the shared eighteen-entry table.
static __inline__ void _stageResolvePlayerContacts(void)
{
    enum { STAGE_PLAYER_RESPONSE_CONTACTS = 6 };
    Task*      playerTask;
    GameActor* player;
    GfxCoord*  rootCoord;
    s32        rootResponseEnabled;

    playerTask          = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    player              = playerTask->work;
    rootResponseEnabled = player->collisionEnableMask & (1 << GAME_ACTOR_BODY_ROOT);
    rootCoord           = playerTask->extra.tmd->coords;
    if (rootResponseEnabled) {
        worldCollisionApplyResponsePushback(rootCoord, player->collisionMotionContexts[GAME_ACTOR_BODY_ROOT].contacts,
                                            STAGE_PLAYER_RESPONSE_CONTACTS, &player->surfaceClass);
    }
    worldCollisionClearContacts(player->collisionContacts);
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Spawns entry zero of the queued mode's task table and prepares its presentation.
///
/// Returns the spawned task or NULL. Presentation changes only after success;
/// the caller still advances on failure. Keep/hold/actor modes retain resources,
/// while other modes capture the area's frame and reset the auxiliary heap.
/// Actor drawing and capture paths require the live player task, its GameActor
/// work, model root and initialized contact table; they correct the first six
/// contacts when root response is enabled, clear the shared table and invalidate
/// the root transform.
static Task* _stageSpawnModeTask(void)
{
    enum { STAGE_MODE_TASK_ENTRY = 0 };
    Task*            modeTask;
    GameLocationKey* location;

    modeTask = taskSpawnFromTable(Stage_Ctx->taskDesc, STAGE_MODE_TASK_ENTRY, Stage_Ctx->spawnArg1, Stage_Ctx->spawnArg2);
    if (modeTask != NULL) {
        switch (Stage_Ctx->entryMode) {
            case STAGE_ENTRY_DRAW_ACTORS:
                Stage_Ctx->transitionKind = STAGE_TRANSITION_ACTORS;
                _stageResolvePlayerContacts();
                // fallthrough
            case STAGE_ENTRY_KEEP:
            case STAGE_ENTRY_HOLD:
                Stage_Ctx->otFlipArmed = 1;
                if (Stage_Ctx->entryMode == STAGE_ENTRY_HOLD) {
                    gDisplayState.control.flags.flipMode    = DISPLAY_FLIP_HOLD;
                    gDisplayState.control.flags.imageSource = DISPLAY_IMAGE_NONE;
                } else {
                    gDisplayState.control.flags.flipMode    = DISPLAY_FLIP_FULL;
                    gDisplayState.control.flags.imageSource = DISPLAY_IMAGE_STRIPS;
                }
                break;
            case STAGE_ENTRY_GRAY_CAPTURE:
            case STAGE_ENTRY_RELOAD_FORCED:
            default:
                location = &gGameSession->location.loc;
                gpuResetAndInvalidateModelBuffers();
                gfxCaptureAreaFrame(location->stage, location->area, gDisplayState.drawBuffer, MEMORY_PRIMITIVE_HEAP_BYTES);
                if (Stage_Ctx->entryMode == STAGE_ENTRY_GRAY_CAPTURE) {
                    _gfxInvertCapturedFrameGray();
                }
                memInitAuxHeap();
                gDisplayState.control.flags.flipMode    = DISPLAY_FLIP_TASK_ONLY;
                gDisplayState.control.flags.imageSource = DISPLAY_IMAGE_ROOM_SLOT;
                _stageResolvePlayerContacts();
                break;
        }
    }
    return modeTask;
}

static void Display_TransitionTask(Task* task)
{
    u32          flags;
    s32          state;
    GameSession* ed;
    StageCtx*    stage;
    s32          flag;
    s32          kind;
    s32          disp;

    // View transition, then file load, the ending request, then a capture.
    flags = Stage_Ctx->requestFlags;
    if (flags & STAGE_REQUEST_TRANSITION) {
        padStartInputBlock(0);
        Stage_Ctx->otFlipArmed = 0;
        state                  = Stage_Ctx->transitionStep;
        switch (state) {
            case 0:
                Stage_Ctx->heldFrameBuffer           = gDisplayState.frameBuffer;
                gGameSession->location.loc.view      = Stage_Ctx->pendingView;
                Stage_Ctx->entryMode                 = STAGE_ENTRY_RELOAD;
                gDisplayState.control.flags.flipMode = DISPLAY_FLIP_HOLD;
                memConfigureImageMemory(gGameSession->location.loc.stage, gGameSession->location.loc.area);
                if (!(Stage_Ctx->requestFlags & STAGE_REQUEST_KEEP_VIEW)) {
                    (gameGetTaskSlot(GAME_TASK_SLOT_VIEW_GATE))->spawnArg1.value = gGameSession->location.loc.view;
                    ResetGraph(1);
                    gpuClearFrameOrderingTable(0);
                    gpuClearFrameOrderingTable(1);
                    memInitAuxHeap();
                    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = gGameSession->location.loc.view;
                    padStartInputBlock(0);
                    viewQueueCurrentCamera(VIEW_PACKET_LIST_NONE);
                    gGameSession->viewReady = 0;
                    taskSpawn(0, 0x1E, 2, 0);
                } else {
                    tmdResetAuxHeapAndRestoreBuffers();
                    gGameSession->viewReady = 1;
                }
                Stage_Ctx->transitionStep = Stage_Ctx->transitionStep + 1;
                break;
            case 1:
                ed   = gGameSession;
                flag = ed->viewReady;
                if (flag == 1) {
                    disp  = gDisplayState.frameBuffer;
                    stage = Stage_Ctx;
                    if (disp == stage->heldFrameBuffer) {
                        kind          = stage->transitionKind;
                        ed->viewReady = 0;
                        if (kind == STAGE_TRANSITION_NONE) {
                            task->killCountdown       = flag;
                            Stage_Ctx->transitionStep = Stage_Ctx->transitionStep + 2;
                        } else {
                            Stage_Ctx->transitionStep = Stage_Ctx->transitionStep + 1;
                        }
                    }
                    cdCmdRequestSuspend();
                }
                break;
            case 2:
                gDisplayState.otBuffer = gDisplayState.frameBuffer;
                _stageFlipOtAndRedraw(0);
                Stage_Ctx->fadeFlags                 = Stage_Ctx->fadeFlags | STAGE_FADE_SKIP;
                gDisplayState.control.flags.flipMode = gDisplayState.control.flags.flipMode | DISPLAY_FLIP_SKIP_TASK_OT;
                task->killCountdown                  = 3;
                Stage_Ctx->transitionStep            = Stage_Ctx->transitionStep + 1;
                break;
            case 3:
                gDisplayState.control.flags.flipMode = DISPLAY_FLIP_HOLD;
                task->killCountdown                  = task->killCountdown - 1;
                if (task->killCountdown == 0) {
                    gpuResetAndInvalidateModelBuffers();
                    gfxCaptureAreaFrame(gGameSession->location.loc.stage, gGameSession->location.loc.area,
                                        gDisplayState.frameBuffer, MEMORY_PRIMITIVE_HEAP_BYTES);
                    memInitAuxHeap();
                    Stage_Ctx->loadBuffersCleared = 0;
                    // The ending request is the sign bit.
                    if ((s32)Stage_Ctx->requestFlags < 0) {
                        padClearInputBlock(0);
                        task->state = task->state + 1;
                        _stageBeginModeExitLoad(task);
                        return;
                    }
                    Stage_Ctx->transitionStep = Stage_Ctx->transitionStep + 1;
                }
                break;
            case 4:
                gDisplayState.control.flags.flipMode    = DISPLAY_FLIP_TASK_ONLY;
                gDisplayState.control.flags.imageSource = DISPLAY_IMAGE_ROOM_SLOT;
                Stage_Ctx->transitionStep               = Stage_Ctx->transitionStep + 1;
                break;
            case 5:
                padClearInputBlock(0);
                Stage_Ctx->requestFlags = Stage_Ctx->requestFlags & ~STAGE_REQUEST_TRANSITION;
                break;
        }
    } else if (flags & STAGE_REQUEST_FILE_LOAD) {
        _stageStepFileLoadTransition(task);
    } else if ((s32)flags < 0) {
        task->state = task->state + 1;
        _stageBeginModeExitLoad(task);
    } else if (flags & STAGE_REQUEST_CAPTURE) {
        gfxCaptureAreaFrame(gGameSession->location.loc.stage, gGameSession->location.loc.area, gDisplayState.frameBuffer,
                            MEMORY_PRIMITIVE_HEAP_BYTES);
        Stage_Ctx->requestFlags = Stage_Ctx->requestFlags & ~STAGE_REQUEST_CAPTURE;
    }

    if (Stage_Ctx->entryMode == STAGE_ENTRY_DRAW_ACTORS) {
        _stageFlipOtAndRedraw(0);
    }
}

/// Rebuilds the alternate game OT using the stage transition's draw selection.
///
/// Kinds 3 and 0x20 execute the default task list, 2 draws cached view sprites
/// and active models, and 1 executes priority-0x62 tasks then draws the cached
/// sprites and flagged models. Other kinds leave the cleared OT empty.
/// Requires initialized stage/view/model data, OT index 0 or 1, finished GPU
/// use of the alternate table and enough primitive storage for the selected
/// draws. Enables full presentation and selects the current framebuffer for
/// drawing. Restores the borrowed OT pointer after dispatch.
/// The argument is ignored; its zero-valued call setup is present in the binary.
static void _stageFlipOtAndRedraw(s32 unused)
{
    enum { STAGE_REDRAW_TASK_PRIORITY = 0x62 };
    DisplayState* display;
    u_long*       savedOt;
    s32           otBuffer;
    u32           transitionKind;

    display = &gDisplayState;
    savedOt = gGpuCurrentOt;
    // Draw through the alternate game OT while retaining the caller's task OT.
    otBuffer          = display->otBuffer ^ 1;
    display->otBuffer = otBuffer;
    _gpuBeginOt(otBuffer);
    display->control.flags.flipMode = DISPLAY_FLIP_FULL;
    display->drawBuffer             = display->frameBuffer;
    transitionKind                  = Stage_Ctx->transitionKind;
    switch (transitionKind) {
        case STAGE_TRANSITION_TASKS:
        case STAGE_TRANSITION_TASKS_ALT:
            taskExecDefaultList(&gTaskDefaultList);
            break;
        case STAGE_TRANSITION_ACTORS:
            spriteLinkViewCachedPackets();
            actorRenderComposeAndDrawActiveModels(&Gpu_OtBuffers[display->otBuffer]);
            break;
        case STAGE_TRANSITION_FILTERED:
            taskExecListForPriority(&gTaskDefaultList, STAGE_REDRAW_TASK_PRIORITY);
            spriteLinkViewCachedPackets();
            actorRenderComposeAndDrawFlaggedModels(&Gpu_OtBuffers[display->otBuffer]);
            break;
    }
    gGpuCurrentOt = savedOt;
}

/// Replaces four RGB555 pixels in two writable words with inverted grey, clearing bit 15.
///
/// The two distinct word-aligned words each contain two little-endian pixels.
/// Grey is 31 - floor((3R + 4G + B) / 8), replicated into the three channels.
/// All four input mask bits are ignored and cleared in the result.
static __inline__ void _gfxInvertFourRgb555PixelsGray(u32* firstWord, u32* secondWord)
{
    // RGB555 channel masks and the 3:4:1 weighting of red, green and blue.
    enum {
        GRAPHICS_GRAY_PIXEL_PAIR_CHANNELS = 0x001F001F,
        GRAPHICS_GRAY_PIXEL_PAIR_GREEN    = GRAPHICS_GRAY_PIXEL_PAIR_CHANNELS << 5,
        GRAPHICS_GRAY_FOUR_BYTE_CHANNELS  = 0x1F1F1F1F,
        GRAPHICS_GRAY_ODD_BYTE_CHANNELS   = 0x1F001F00,
        GRAPHICS_GRAY_RED_WEIGHT          = 3,
        GRAPHICS_GRAY_GREEN_WEIGHT        = 4,
        GRAPHICS_GRAY_WEIGHT_SHIFT        = 3,
    };
    u32 secondPixels;
    u32 firstPixels;
    u32 grayChannels;
    u32 packedChannels;

    // Each byte holds one pixel's floor((3R + 4G + B) / 8), then 31 minus that value.
    secondPixels     = *secondWord;
    firstPixels      = *firstWord;
    packedChannels   = secondPixels & GRAPHICS_GRAY_PIXEL_PAIR_CHANNELS;
    packedChannels <<= 8;
    packedChannels  |= firstPixels & GRAPHICS_GRAY_PIXEL_PAIR_CHANNELS;
    grayChannels     = packedChannels * GRAPHICS_GRAY_RED_WEIGHT;
    packedChannels   = secondPixels & GRAPHICS_GRAY_PIXEL_PAIR_GREEN;
    packedChannels <<= 3;
    firstPixels    >>= 5;
    packedChannels  |= firstPixels & GRAPHICS_GRAY_PIXEL_PAIR_CHANNELS;
    grayChannels    += packedChannels * GRAPHICS_GRAY_GREEN_WEIGHT;
    secondPixels   >>= 2;
    packedChannels   = secondPixels & GRAPHICS_GRAY_ODD_BYTE_CHANNELS;
    firstPixels    >>= 5;
    packedChannels  |= firstPixels & GRAPHICS_GRAY_PIXEL_PAIR_CHANNELS;
    grayChannels    += packedChannels;
    grayChannels     = (grayChannels >> GRAPHICS_GRAY_WEIGHT_SHIFT) & GRAPHICS_GRAY_FOUR_BYTE_CHANNELS;
    grayChannels     = GRAPHICS_GRAY_FOUR_BYTE_CHANNELS - grayChannels;

    // Replicate each five-bit grey channel into R, G and B; the mask bit stays clear.
    firstPixels    = grayChannels & GRAPHICS_GRAY_PIXEL_PAIR_CHANNELS;
    firstPixels   |= (firstPixels << 10) | (firstPixels << 5);
    secondPixels   = grayChannels & GRAPHICS_GRAY_ODD_BYTE_CHANNELS;
    secondPixels >>= 8;
    secondPixels  |= (secondPixels << 10) | (secondPixels << 5);
    *firstWord     = firstPixels;
    *secondWord    = secondPixels;
}

/// Converts the area's captured RAM frame to inverted grey in place.
///
/// Call after `gfxCaptureAreaFrame` has completed its GPU readback and placed
/// `Gpu_PrimHeapBase` immediately after the word-aligned 320x240 RGB555 frame.
/// Processes every pixel, clearing its mask bit; frame storage must remain
/// writable and unavailable for other transfers throughout the conversion.
static void _gfxInvertCapturedFrameGray(void)
{
    // Keep the signed loop comparison while deriving its complete frame extent.
    enum { GRAPHICS_CAPTURE_PIXEL_GROUP_COUNT = (s32)(sizeof(FsImgBuffers) / (2 * sizeof(u32))) };

    s32  pixelGroup;
    u32* firstWord;
    u32* secondWord;

    firstWord  = (u32*)(Gpu_PrimHeapBase - sizeof(FsImgBuffers));
    secondWord = firstWord + 1;
    for (pixelGroup = 0; pixelGroup < GRAPHICS_CAPTURE_PIXEL_GROUP_COUNT; pixelGroup++) {
        _gfxInvertFourRgb555PixelsGray(firstWord, secondWord);
        secondWord += 2;
        firstWord  += 2;
    }
}

void Stage_InitOtAndSpawn(void)
{
    DisplayState* temp;

    displayInitTaskBuffers();
    temp                         = &gDisplayState;
    temp->displayOwner           = DISPLAY_OWNER_TRANSITION;
    temp->control.flags.flipMode = DISPLAY_FLIP_HOLD;
    temp->frameBuffer            = temp->otBuffer ^ 1;
    taskInitList(&gTaskDisplayList);
    taskSpawnFromTable(&Display_ModeTaskDesc, 0, 0, 0);
}

s32 stageRequestModeTaskExit(void)
{
    Stage_Ctx->requestFlags |= STAGE_REQUEST_ENDING;
    return 0;
}

s32 stageRequestViewTransition(s32 view, s32 transitionKind)
{
    StageCtx* stage;
    s32       transitionMask;

    transitionMask = STAGE_REQUEST_TRANSITION;
    if (!(Stage_Ctx->requestFlags & transitionMask)) {
        padStartInputBlock(0);
        stage                  = Stage_Ctx;
        stage->pendingView     = view;
        stage->heldFrameBuffer = 0;
        stage->transitionStep  = 0;
        stage->transitionKind  = transitionKind;
        stage->requestFlags   |= transitionMask;
    }
    return gGameSession->location.loc.view;
}

s32 stageRequestViewTransitionAndModeExit(s32 view)
{
    enum { STAGE_VIEW_TRANSITION_BUSY = -1 };
    StageCtx* stage;
    s32       transitionMask;
    s32       previousView;

    transitionMask = STAGE_REQUEST_TRANSITION;
    previousView   = STAGE_VIEW_TRANSITION_BUSY;
    if (!(Stage_Ctx->requestFlags & transitionMask)) {
        padStartInputBlock(0);
        stage                    = Stage_Ctx;
        stage->pendingView       = view;
        stage->heldFrameBuffer   = 0;
        stage->transitionStep    = 0;
        stage->transitionKind    = STAGE_TRANSITION_KIND_7;
        stage->requestFlags     |= transitionMask;
        previousView             = gGameSession->location.loc.view;
        Stage_Ctx->requestFlags |= STAGE_REQUEST_ENDING;
    }
    return previousView;
}

s32 stageRequestFrameCapture(void)
{
    Stage_Ctx->requestFlags |= STAGE_REQUEST_CAPTURE;
    return 0;
}

s32 stageConfigureFade(s32 decreasing, s32 additiveBlend, s32 stepPerTick, s32 frontOfOt)
{
    if (stepPerTick == 0) {
        Stage_Ctx->fadeStep = STAGE_FADE_DEFAULT_STEP;
    } else {
        Stage_Ctx->fadeStep = stepPerTick;
    }
    if (decreasing != 0) {
        Stage_Ctx->fadeStep = -Stage_Ctx->fadeStep;
    }
    Stage_Ctx->fadeFlags = 0;
    if (additiveBlend != 0) {
        Stage_Ctx->fadeFlags |= STAGE_FADE_ADDITIVE;
    }
    if (frontOfOt != 0) {
        Stage_Ctx->fadeFlags |= STAGE_FADE_FRONT;
    }
    return 0;
}

s32 stageGetFadeStatus(void)
{
    StageCtx* stage;
    u8        fadeLevel;

    stage     = Stage_Ctx;
    fadeLevel = stage->fadeLevel;
    if (fadeLevel == 0) {
        return STAGE_FADE_CLEAR;
    }
    if (fadeLevel >= stage->fadeMax) {
        return STAGE_FADE_AT_MAX;
    }
    return STAGE_FADE_BETWEEN;
}

s32 stageIsTransitionPending(void)
{
    return (Stage_Ctx->requestFlags & STAGE_REQUEST_BUSY) != 0;
}

void stageEnsureTaskOrderingTables(void)
{
    if (Stage_Ctx->fullOtReady == 0) {
        gpuInitTaskOrderingTables();
        Stage_Ctx->fullOtReady = 1;
    }
}

void stageEnsureHeapTaskPrimitiveBuffer(void)
{
    if (Stage_Ctx->largePrimBuf == 0) {
        displayUseHeapTaskPrimitiveBuffer();
        Stage_Ctx->largePrimBuf = 1;
    }
}

void stageReleaseTaskPrimitiveBuffer(void)
{
    if (Stage_Ctx->largePrimBuf == 1) {
        displayUseStaticTaskPrimitiveBuffer();
        Stage_Ctx->largePrimBuf = 0;
    }
}

void stageSetFadeMax(u8 maxLevel)
{
    Stage_Ctx->fadeMax = maxLevel;
}

void displaySetTaskDrawMode(s32 drawMode)
{
    switch (drawMode) {
        case DISPLAY_TASK_DRAW_CLEAR:
            gDisplayState.control.flags.flipMode    = DISPLAY_FLIP_TASK_ONLY;
            gDisplayState.control.flags.imageSource = DISPLAY_IMAGE_NONE;
            displaySetClearColor(0, 0, 0);
            return;
        case DISPLAY_TASK_DRAW_ROOM:
            gDisplayState.control.flags.flipMode    = drawMode;
            gDisplayState.control.flags.imageSource = DISPLAY_IMAGE_ROOM_SLOT;
            displaySetClearColor(DISPLAY_CLEAR_DISABLED, 0, 0);
            return;
        case DISPLAY_TASK_DRAW_TRANSITION:
            gDisplayState.control.flags.flipMode    = DISPLAY_FLIP_TASK_ONLY;
            gDisplayState.control.flags.imageSource = DISPLAY_IMAGE_TRANSITION_STRIPS;
            displaySetClearColor(DISPLAY_CLEAR_DISABLED, 0, 0);
            return;
        case DISPLAY_TASK_DRAW_HOLD:
            gDisplayState.control.flags.flipMode = DISPLAY_FLIP_HOLD;
            return;
    }
}

/// Requests a current-view transition that redraws the default task list.
///
/// With no view transition pending, retains the live view resources and resets
/// the transition's held-frame selector and step. Leaves an existing request
/// untouched, does not block controller input, and always returns zero.
static s32 _stageRequestCurrentViewTransition(void)
{
    enum { STAGE_VIEW_TRANSITION_INITIAL_STEP = 0 };
    StageCtx* stage;
    u32       requestFlags;
    s32       currentView;

    stage        = Stage_Ctx;
    requestFlags = stage->requestFlags;
    if (!(requestFlags & STAGE_REQUEST_TRANSITION)) {
        stage->requestFlags    = requestFlags | STAGE_REQUEST_KEEP_TRANSITION;
        currentView            = gGameSession->location.loc.view;
        stage->heldFrameBuffer = 0;
        stage->transitionStep  = STAGE_VIEW_TRANSITION_INITIAL_STEP;
        stage->transitionKind  = STAGE_TRANSITION_TASKS;
        stage->pendingView     = currentView;
    }
    return 0;
}

Task* displayQueueModeTask(TaskDesc* descriptor, s32 spawnArg1, TaskSpawnArg spawnArg2, s32 entryMode)
{
    StageCtx* stage;

    if (gDisplayState.pendingMode != DISPLAY_MODE_NONE) {
        return NULL;
    }

    MEM_CLEAR(Stage_Ctx, sizeof(*Stage_Ctx));

    stage            = Stage_Ctx;
    stage->taskDesc  = descriptor;
    stage->spawnArg1 = spawnArg1;
    stage->spawnArg2 = spawnArg2;
    stage->entryMode = entryMode;
    // A default request in the Acropolis plaza keeps the current resources.
    if (entryMode == STAGE_ENTRY_RELOAD) {
        if ((GAME_LOCATION_WORD(gGameSession->location.loc) & GAME_LOCATION_STAGE_AREA_MASK) ==
            GAME_LOCATION_KEY(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_PLAZA, 0, 0)) {
            stage->entryMode = STAGE_ENTRY_KEEP;
        }
    }
    Stage_Ctx->fadeMax        = STAGE_FADE_OPAQUE;
    gDisplayState.pendingMode = DISPLAY_MODE_DESCRIPTOR;
    return NULL;
}

s32 stageGetLoadBuffersCleared(void)
{
    return Stage_Ctx->loadBuffersCleared;
}

/// Selects transition OT contents and redraws only while the keep-resources arm is set.
///
/// Accepts the byte-sized STAGE_TRANSITION_* draw selections; other values
/// produce an empty game OT. The arm is set after a keep/hold/actor mode task
/// spawns and cleared by view-transition processing. An unarmed call changes
/// nothing. Requires the same drawing resources as `_stageFlipOtAndRedraw`.
static void _stageSetTransitionKindAndRedraw(u8 transitionKind)
{
    StageCtx* stage;

    stage = Stage_Ctx;
    if (stage->otFlipArmed == 1) {
        stage->transitionKind = transitionKind;
        _stageFlipOtAndRedraw(0);
    }
}

void stageResetFadeLevel(void)
{
    Stage_Ctx->fadeLevel = 0;
    Stage_Ctx->fadeMax   = STAGE_FADE_OPAQUE;
}

/// Requests CD-head suspension before handing presentation to the queued mode task.
///
/// A requested or ongoing suspension selects the wait state. An empty or
/// scene/audio ring head permits spawning immediately and skips that state.
/// Leaves one controller poll blocked after either successful or failed spawn.
static void _stageSuspendCdAndSpawnModeTask(Task* task)
{
    padStartInputBlock(0);
    if (cdCmdRequestSuspend() != 0) {
        task->state += 1;
    } else {
        gPadStates[0].inputBlockPolls = 1;
        _stageSpawnModeTask();
        task->state += 2;
    }
}

/// Spawns the queued mode task once the CD ring is idle or has a scene/audio head.
///
/// Blocks controller input while waiting, then leaves one poll blocked and
/// enters the transition state even if task allocation failed.
static void _stageWaitCdAndSpawnModeTask(Task* task)
{
    padStartInputBlock(0);
    if (cdCmdIsIdleOrSceneAudioPending() != 0) {
        gPadStates[0].inputBlockPolls = 1;
        _stageSpawnModeTask();
        task->state += 1;
    }
}

/// Starts the current view's resource reload before the mode task resumes playback.
///
/// Holds presentation. Modes 1, 3 and 4 retain image/model/sprite resources;
/// other modes reconfigure image memory and restore model/view-sprite buffers.
/// Enqueues the current-view load/seek and polls its wait state immediately.
/// Requires valid session/view resources and space in the CD request ring.
static void _stageBeginModeExitLoad(Task* task)
{
    u32 entryMode;

    gDisplayState.control.flags.flipMode = DISPLAY_FLIP_HOLD;
    entryMode                            = Stage_Ctx->entryMode;
    // Modes 1, 3 and 4 keep the room's current resources.
    if (entryMode >= STAGE_ENTRY_DRAW_ACTORS + 1U || (entryMode < STAGE_ENTRY_HOLD && entryMode != STAGE_ENTRY_KEEP)) {
        memConfigureImageMemory(gGameSession->location.loc.stage, gGameSession->location.loc.area);
        tmdResetAuxHeapAndRestoreBuffers();
        spriteAllocateViewCachedPackets();
    }
    cdCmdEnqueueDisplayResource(0, 0, CD_COMMAND_DISPLAY_LOAD_SEEK_CURRENT_VIEW);
    task->state = (s32)(task->state + 1);
    _stageWaitModeExitLoad(task);
}

/// Advances past the mode-exit reload once CD work permits movie resumption.
///
/// A queued scene-audio command also permits progress; it need not have finished.
static void _stageWaitModeExitLoad(Task* task)
{
    if (cdCmdIsIdleOrSceneAudioPending() != 0) {
        task->state += 1;
    }
}

/// Resumes a suspended movie before returning presentation to the game loop.
///
/// Waits for its first frame or retirement, then consumes the display-mode
/// request, selects image strips without clearing and exits the mode task.
static void _stageResumeMovieAndFinishModeTask(Task* task)
{
    if (cdCmdResumeSuspendedMovie() != 0) {
        gDisplayState.displayOwner              = DISPLAY_OWNER_GAME_LOOP;
        gDisplayState.pendingMode               = DISPLAY_MODE_NONE;
        gDisplayState.control.flags.imageSource = DISPLAY_IMAGE_STRIPS;
        displaySetClearColor(DISPLAY_CLEAR_DISABLED, 0, 0);
        taskCallExit(task);
    }
}

static void Display_DispatchTaskTable(Task* task)
{
    TaskFuncTable6 sp;

    sp = Display_TaskStates;
    sp.funcs[task->state](task);
    _stageStepFadeOverlay();
}

void mdecRequestSceneImageDecode(const u8* viewId)
{
    u16         headerIndex;
    u16         headerFound;
    s16         bufferKind;
    s32         requestedView;
    s32         imageDataOffset;
    CdCmdQueue* queue;

    queue         = &gCdCmdQueue;
    headerIndex   = 0;
    headerFound   = 0;
    requestedView = *viewId;
    for (; headerIndex < ARRAY_SIZE(queue->sceneImageHeaders); headerIndex++) {
        if (requestedView == queue->sceneImageHeaders[headerIndex].viewId) {
            headerFound = 1;
            break;
        }
    }
    if (!headerFound || queue->scenePayloadLoading != 0) {
        queue->imageDecodePending = 1;
        queue->imageLoadStatus    = CD_COMMAND_IMAGE_PENDING;
        queue->imageDecodeStep    = CD_COMMAND_IMAGE_WAIT_HEADER;
        return;
    }

    // Payload bytes follow any VLC and timing reservations in a shared actor arena.
    Stage_CdEntry = &queue->sceneImageHeaders[headerIndex];
    bufferKind    = Stage_CdEntry->bufferKind;
    switch (bufferKind) {
        case STREAM_SCENE_BUFFER_DECODE:
            Mdec_DecodeBase = queue->decodeBuffer;
            break;
        case STREAM_SCENE_BUFFER_ACTOR_0:
            Mdec_DecodeBase = (u8*)Fs_ActorLoadBase0;
            if (queue->sceneStream->data.scene.vlcBufferKind == STREAM_VLC_BUFFER_ACTOR_0) {
                Mdec_DecodeBase = (u8*)Fs_ActorLoadBase0 + STREAM_VLC_TABLE_BYTES;
            }
            if (queue->sceneStream->control.scene.timingBufferKind == STREAM_TIMING_BUFFER_ACTOR_0) {
                Mdec_DecodeBase = Mdec_DecodeBase + queue->sceneStream->data.scene.timingBufferBytes;
            }
            gGameSession->field_7C = 0;
            break;
        case STREAM_SCENE_BUFFER_ACTOR_1:
            Mdec_DecodeBase = (u8*)Fs_ActorLoadBase1;
            if (queue->sceneStream->data.scene.vlcBufferKind == STREAM_VLC_BUFFER_ACTOR_1) {
                Mdec_DecodeBase = (u8*)Fs_ActorLoadBase1 + STREAM_VLC_TABLE_BYTES;
            }
            if (queue->sceneStream->control.scene.timingBufferKind == STREAM_TIMING_BUFFER_ACTOR_1) {
                Mdec_DecodeBase = Mdec_DecodeBase + queue->sceneStream->data.scene.timingBufferBytes;
            }
            gGameSession->field_7E = 0;
            break;
        case STREAM_SCENE_BUFFER_ACTOR_2:
            Mdec_DecodeBase = (u8*)Fs_ActorLoadBase2;
            if (queue->sceneStream->data.scene.vlcBufferKind == STREAM_VLC_BUFFER_ACTOR_2) {
                Mdec_DecodeBase = (u8*)Fs_ActorLoadBase2 + STREAM_VLC_TABLE_BYTES;
            }
            if (queue->sceneStream->control.scene.timingBufferKind == STREAM_TIMING_BUFFER_ACTOR_2) {
                Mdec_DecodeBase = Mdec_DecodeBase + queue->sceneStream->data.scene.timingBufferBytes;
            }
            gGameSession->field_80 = 0;
            break;
        case STREAM_SCENE_BUFFER_EXTERNAL:
            Mdec_DecodeBase = queue->externalScenePayloadBuffer;
            break;
    }
    imageDataOffset           = Stage_CdEntry->imageDataOffset;
    D_8007A35C                = 0;
    queue->imageDecodePending = 1;
    queue->imageLoadStatus    = CD_COMMAND_IMAGE_PENDING;
    queue->imageDecodeStep    = CD_COMMAND_IMAGE_START;
    D_8007A360                = Mdec_DecodeBase + imageDataOffset;
}

/// Releases image-decode bookkeeping and restores model buffers when requested.
///
/// Also used on the scene decoder's timeout path. This marks the image complete
/// and invalidates scene-payload reuse; it does not stop MDEC or remove its callback.
static __inline__ void _mdecFinishImageDecode(void)
{
    CdCmdQueue* queue = &gCdCmdQueue;

    if (gDisplayState.keepGraphics == 0) {
        tmdResetAuxHeapAndRestoreBuffers();
    }
    queue->imageLoadStatus      = CD_COMMAND_IMAGE_COMPLETE;
    queue->imageDecodePending   = 0;
    D_8007A35C                  = 0;
    queue->imageDecodeStep      = CD_COMMAND_IMAGE_START;
    queue->scenePayloadReusable = 0;
}

/// Steps a cached scene image decode, its optional image uploads and timing copy.
///
/// Retries header selection from the live view, starts VLC/MDEC, then waits for
/// all twenty output strips before consuming byte-offset payload records. Header
/// and output waits finish after 91 polls as well. Requires aligned, in-bounds
/// payload offsets and live image, VLC, timing and auxiliary workspaces. The VLC
/// expansion is unrestricted; this function does not check its output capacity.
/// Timeout completion uses the cached header and does not stop MDEC DMA itself.
static void _mdecStepSceneImageDecode(void)
{
    enum {
        MDEC_SCENE_TIMEOUT_POLLS         = 91,
        MDEC_VLC_UNLIMITED_OUTPUT        = 0,
        STREAM_SCENE_STRIP_X_SHIFT_PAGES = -8,
        STREAM_SCENE_CHUNK_Y_SHIFT_ROWS  = -3,
    };
    CdCmdQueue* queue;
    u16         imageIndex;
    s32         stripUploadResult;

    queue = &gCdCmdQueue;
    switch (queue->imageDecodeStep) {
        case CD_COMMAND_IMAGE_WAIT_HEADER:
            mdecRequestSceneImageDecode(&gGameSession->location.loc.view);
            if ((u32)++D_8007A358 >= MDEC_SCENE_TIMEOUT_POLLS) {
                D_8007A358 = 0;
                gpuResetAndInvalidateModelBuffers();
                if (Stage_CdEntry->bufferKind == STREAM_SCENE_BUFFER_DECODE) {
                    queue->decodeBufferBytes = queue->nextDecodeBufferBytes;
                }
                _mdecFinishImageDecode();
            }
            break;
        case CD_COMMAND_IMAGE_START:
            gpuResetAndInvalidateModelBuffers();
            queue->mdecOutputPending = 1;
            if (queue->sceneVlcTableMode == STREAM_SCENE_VLC_IMAGE_BUFFER) {
                DecDCTvlcBuild((u16*)((u8*)Fs_ImgBuffers + FILE_SYSTEM_IMAGE_VLC_OFFSET));
                queue->rebuildImageVlcTable = 0;
                queue->vlcTable             = (u16*)((u8*)Fs_ImgBuffers + FILE_SYSTEM_IMAGE_VLC_OFFSET);
            }
            DecDCTReset(0);
            DecDCTvlcSize2(MDEC_VLC_UNLIMITED_OUTPUT);
            DecDCTvlc2(D_8007A360, gMemActiveAuxHeap,
                       queue->vlcTable);
            D_8007A35E = 1;
            DecDCToutCallback(_mdecImageStripCallback);
            DecDCTin(gMemActiveAuxHeap, queue->imageMdecMode);
            queue->imageMdecMode = MDEC_IMAGE_MODE_RGB16;
            DecDCTout(Fs_ImgBuffers->strips[0], FILE_SYSTEM_IMAGE_STRIP_WORDS);
            D_8007A358 = 0;
            queue->imageDecodeStep++;
            /* fallthrough */
        case CD_COMMAND_IMAGE_WAIT_OUTPUT:
            if (queue->mdecOutputPending == 0) {
                // Upload the payload's optional strip lists and image chunks.
                for (imageIndex = 0; imageIndex < ARRAY_SIZE(Stage_CdEntry->stripListOffsets); imageIndex++) {
                    if (Stage_CdEntry->stripListOffsets[imageIndex] != 0) {
                        if (Stage_CdEntry->relocateStripLists[imageIndex] != 0) {
                            Fs_ChunkMode    = 2;
                            D5B498_8006C233 = STREAM_SCENE_STRIP_X_SHIFT_PAGES;
                        }
                        fsBeginImageColumns((FsImageColumn*)(Mdec_DecodeBase + Stage_CdEntry->stripListOffsets[imageIndex]));
                        // Both upload polls are retained; a retry restarts this column stream.
                        while (fsUploadImageStrips(FILE_SYSTEM_IMAGE_STRIPS_RESIDENT_INPUT) != FILE_SYSTEM_IMAGE_STRIPS_COMPLETE) {
                            stripUploadResult = fsUploadImageStrips(FILE_SYSTEM_IMAGE_STRIPS_RESIDENT_INPUT);
                            if (stripUploadResult == FILE_SYSTEM_IMAGE_STRIPS_COMPLETE) {
                                break;
                            }
                            if (stripUploadResult == FILE_SYSTEM_IMAGE_STRIPS_RETRY) {
                                fsBeginImageColumns((FsImageColumn*)(Mdec_DecodeBase + Stage_CdEntry->stripListOffsets[imageIndex]));
                            }
                        }
                        Fs_ChunkMode    = 0;
                        D5B498_8006C233 = 0;
                    }
                }
                for (imageIndex = 0; imageIndex < ARRAY_SIZE(Stage_CdEntry->imageChunkOffsets); imageIndex++) {
                    if (Stage_CdEntry->imageChunkOffsets[imageIndex] != 0) {
                        if (Stage_CdEntry->relocateImageChunks[imageIndex] != 0) {
                            Fs_ChunkMode    = 2;
                            D5B498_8006C234 = STREAM_SCENE_CHUNK_Y_SHIFT_ROWS;
                        }
                        while (fsUploadImageChunk((const FsImageChunk*)(Mdec_DecodeBase + Stage_CdEntry->imageChunkOffsets[imageIndex]), 1)) {
                        }
                        Fs_ChunkMode    = 0;
                        D5B498_8006C234 = 0;
                    }
                }
                queue->imageLayout = FILE_SYSTEM_IMAGE_STRIPS;
                if (Stage_CdEntry->bufferKind == STREAM_SCENE_BUFFER_DECODE) {
                    queue->decodeBufferBytes = queue->nextDecodeBufferBytes;
                }
                // Refresh timing data before releasing the completed decode operation.
                if (queue->sceneStream->control.scene.timingBufferKind != STREAM_TIMING_BUFFER_NONE) {
                    memCopyBytes(&Mdec_DecodeBase[Stage_CdEntry->timingDataOffset], queue->timingBuffer,
                                 Stage_CdEntry->timingDataBytes);
                }
                _mdecFinishImageDecode();
            } else if ((u32)++D_8007A358 >= MDEC_SCENE_TIMEOUT_POLLS) {
                D_8007A358 = 0;
                gpuResetAndInvalidateModelBuffers();
                if (Stage_CdEntry->bufferKind == STREAM_SCENE_BUFFER_DECODE) {
                    queue->decodeBufferBytes = queue->nextDecodeBufferBytes;
                }
                _mdecFinishImageDecode();
            }
            break;
    }
}

/// Steps a standalone BS image decode and converts its strips to a contiguous frame.
///
/// Expands VLC into the selected auxiliary region, then lets MDEC callbacks fill
/// twenty 16x240 RGB555 strips. Once complete, VRAM assembles those strips and
/// reads back one 320x240 frame into the same RAM workspace. The optional display
/// preservation copies a 480-word-wide rectangle, sufficient for a 320-pixel
/// RGB24 movie. Input, VLC and output workspaces must stay valid until consumed;
/// the auxiliary region must fit the complete expanded command stream.
static void _mdecStepStandaloneImageDecode(void)
{
    enum {
        MDEC_VLC_UNLIMITED_OUTPUT    = 0,
        MDEC_PRESERVED_DISPLAY_WIDTH = FILE_SYSTEM_IMAGE_WIDTH * 3 / 2,
    };
    RECT          frameRect;
    u16           stripCursor;
    CdCmdQueue*   queue;
    DisplayState* display;

    queue = &gCdCmdQueue;
    switch (queue->imageDecodeStep) {
        case CD_COMMAND_IMAGE_START:
            gpuResetAndInvalidateModelBuffers();
            queue->mdecOutputPending = 1;
            DecDCTReset(0);
            DecDCTvlcSize2(MDEC_VLC_UNLIMITED_OUTPUT);
            DecDCTvlc2((u_long*)D_8007A360, gMemActiveAuxHeap,
                       (u_short*)((u8*)Fs_ImgBuffers + FILE_SYSTEM_IMAGE_VLC_OFFSET));
            D_8007A35E = 1;
            DecDCToutCallback(_mdecImageStripCallback);
            DecDCTin(gMemActiveAuxHeap, queue->imageMdecMode);
            queue->imageMdecMode = MDEC_IMAGE_MODE_RGB16;
            DecDCTout(Fs_ImgBuffers->strips[0], FILE_SYSTEM_IMAGE_STRIP_WORDS);
            queue->imageDecodeStep += 1;
            /* fallthrough */
        case CD_COMMAND_IMAGE_WAIT_OUTPUT:
            stripCursor = 0;
            if (queue->mdecOutputPending == 0) {
                frameRect.w = FILE_SYSTEM_IMAGE_STRIP_WIDTH;
                frameRect.h = FILE_SYSTEM_IMAGE_HEIGHT;
                // Assemble the decoded columns in the other framebuffer, then read back rows.
                frameRect.y = (gDisplayState.drawBuffer ^ 1) * DISPLAY_FRAMEBUFFER_STRIDE_ROWS;
                do {
                    frameRect.x = stripCursor * FILE_SYSTEM_IMAGE_STRIP_WIDTH;
                    LoadImage(&frameRect, Fs_ImgBuffers->strips[stripCursor]);
                    stripCursor++;
                } while ((u32)stripCursor < ARRAY_SIZE(Fs_ImgBuffers->strips));
                frameRect.w = FILE_SYSTEM_IMAGE_WIDTH;
                frameRect.x = 0;
                frameRect.h = FILE_SYSTEM_IMAGE_HEIGHT;
                display     = &gDisplayState;
                frameRect.y = (display->drawBuffer ^ 1) * DISPLAY_FRAMEBUFFER_STRIDE_ROWS;
                StoreImage(&frameRect, Fs_ImgBuffers->strips[0]);
                if (queue->preserveDisplayAfterDecode != 0) {
                    frameRect.x = 0;
                    frameRect.w = MDEC_PRESERVED_DISPLAY_WIDTH;
                    frameRect.h = FILE_SYSTEM_IMAGE_HEIGHT;
                    frameRect.y = display->drawBuffer * DISPLAY_FRAMEBUFFER_STRIDE_ROWS;
                    MoveImage(&frameRect, 0, (display->drawBuffer ^ 1) * DISPLAY_FRAMEBUFFER_STRIDE_ROWS);
                    queue->preserveDisplayAfterDecode = 0;
                } else {
                    ClearImage(&frameRect, 0, 0, 0);
                }
                queue->imageLayout = FILE_SYSTEM_IMAGE_CONTIGUOUS;
                _mdecFinishImageDecode();
            }
            return;
    }
}

void mdecStepImageDecode(void)
{
    CdCmdQueue* queue;

    queue = &gCdCmdQueue;
    if (queue->scenePayloadAvailable == 0) {
        if (queue->rebuildImageVlcTable != 0) {
            DecDCTvlcBuild((u16*)((u8*)Fs_ImgBuffers + FILE_SYSTEM_IMAGE_VLC_OFFSET));
            queue->rebuildImageVlcTable = 0;
        }
        if ((queue->imageDecodePending != 0) && (queue->rebuildImageVlcTable == 0)) {
            _mdecStepStandaloneImageDecode();
        }
    } else if (queue->imageDecodePending != 0) {
        _mdecStepSceneImageDecode();
    }
}

void mdecRequestImageDecode(u_long* bitstream)
{
    CdCmdQueue* queue;

    D_8007A35C                = 0;
    queue                     = &gCdCmdQueue;
    queue->imageDecodePending = 1;
    queue->imageLoadStatus    = CD_COMMAND_IMAGE_PENDING;
    queue->imageDecodeStep    = CD_COMMAND_IMAGE_START;
    D_8007A360                = bitstream;
    D_8007A358                = 0;
}

void mdecRequestImageVlcRebuild(void)
{
    gCdCmdQueue.rebuildImageVlcTable = true;
}

/// Chains image-output DMA strips and releases the pending-output latch at the last one.
///
/// Runs as the no-argument MDEC output callback. The image start path sets the
/// strip-width multiplier to 1, giving twenty output transfers of 1920 words.
/// The workspace and queue state must remain live through callback removal.
static void _mdecImageStripCallback(void)
{
    s32         stripCount;
    CdCmdQueue* queue;

    stripCount = FILE_SYSTEM_IMAGE_WIDTH / (D_8007A35E * FILE_SYSTEM_IMAGE_STRIP_WIDTH);
    queue      = &gCdCmdQueue;
    if (D_8007A35C == stripCount - 1) {
        queue->mdecOutputPending = 0;
        DecDCToutCallback(NULL);
    } else {
        D_8007A35C = D_8007A35C + 1;
        DecDCTout(Fs_ImgBuffers->strips[D_8007A35C], FILE_SYSTEM_IMAGE_STRIP_WORDS);
    }
}

void taskExitCallback(Task* task)
{
    taskCallExit(task);
}
