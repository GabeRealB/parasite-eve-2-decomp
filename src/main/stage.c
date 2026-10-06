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
#include "gameplay/player_actor.h"
#include "gameplay/world_collision.h"

/// Presentation mode stored for a queued mode task.
///
/// Modes 1, 3 and 4 keep the room's current resources. Mode 0, mode 2 and every
/// mode from 5 up reload them. A default request in the Acropolis plaza is
/// stored as mode 1.
enum {
    STAGE_ENTRY_RELOAD       = 0,     // Capture the frame, reset, and reload room resources
    STAGE_ENTRY_KEEP         = 1,     // Keep resources; full flip and image strips
    STAGE_ENTRY_HOLD         = 3,     // Keep resources; hold the flip and draw no image
    STAGE_ENTRY_DRAW_ACTORS  = 4,     // Keep resources, draw active actors, flip during a view transition
    STAGE_ENTRY_GRAY_CAPTURE = 0x100, // Reload path, then invert the captured RAM image to grey
};

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

static void Display_StepFadeOverlay(void);

static s32 Display_TransitionLoad(Task* unused);

static Task* Display_SpawnFromMode(void);

static void Display_TransitionTask(Task* task);

static void Display_FlipOtAndDispatch(s32 unused);

static void _gfxInvertCapturedFrameGray(void);

static s32 Stage_BeginTransitionKind3(void);

static void Stage_SetModeAndFlip(u8 arg0);

static void Stage_WaitCdActivate(Task* task);

static void Stage_WaitCdAndSpawn(Task* task);

static void Display_TaskLoadStep(Task* task);

static void Stage_WaitCdEntry(Task* task);

static void Stage_FinishCdFollowUp(Task* task);

static void Display_DispatchTaskTable(Task* task);

static __inline__ void mdecFinishDecode(void);

/// imageDecodeStep state machine: start DCT, apply work-lists / image chunks, complete.
static void Mdec_ProcessDecode(void);

static void Mdec_DecodeToVram(void);

static void Mdec_StripCallback(void);

// resolved decode base (Mdec_ResolveStreamBuffer)
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
    Stage_WaitCdActivate,
    Stage_WaitCdAndSpawn,
    Display_TransitionTask,
    Display_TaskLoadStep,
    Stage_WaitCdEntry,
    Stage_FinishCdFollowUp,
} };

static void Display_StepFadeOverlay(void)
{
    StageCtx* stage;
    s32       temp;
    s32       product;
    s32       otIdx;
    s32       yoff;
    u8        max;
    u8        val;
    TILE*     tile;
    DR_TPAGE* dr;

    stage = Stage_Ctx;
    if ((s8)stage->fadeFlags & STAGE_FADE_SKIP) {
        stage->fadeFlags = stage->fadeFlags & STAGE_FADE_SKIP_CLEAR;
        return;
    }

    if (gDisplayState.control.flags.flipMode != DISPLAY_FLIP_HOLD) {
        temp = (s8)stage->fadeStep;
        if (temp != 0) {
            product = temp * gDisplayState.frameTicks;
            temp    = stage->fadeLevel;
            temp    = temp + product;
            if (temp <= 0) {
                stage->fadeLevel    = 0;
                Stage_Ctx->fadeStep = 0;
            } else {
                max = stage->fadeMax;
                if (temp >= (s32)max) {
                    stage->fadeLevel    = max;
                    Stage_Ctx->fadeStep = 0;
                } else {
                    stage->fadeLevel = (u8)temp;
                }
            }
        }
    }

    if (Stage_Ctx->fadeLevel != 0) {
        otIdx = 0;
        if (Stage_Ctx->fadeFlags & STAGE_FADE_FRONT) {
            otIdx = (1 << GPU_ORDERING_TABLE_DEPTH_BITS) - 1;
            if (Stage_Ctx->fullOtReady == 0) {
                otIdx = GPU_SMALL_ORDERING_TABLE_ENTRIES - 1;
            }
        }

        tile           = gGpuPrimCursor;
        gGpuPrimCursor = tile + 1;
        yoff           = gDisplayState.vramYOffset;
        setlen(tile, 3);
        setcode(tile, 0x62);
        tile->x0 = -0xA0;
        tile->y0 = -0x78 - yoff;
        tile->w  = 0x140;
        tile->h  = 0xF0;
        val      = Stage_Ctx->fadeLevel;
        tile->b0 = val;
        tile->g0 = val;
        tile->r0 = val;

        dr             = gGpuPrimCursor;
        gGpuPrimCursor = dr + 1;
        if (!(Stage_Ctx->fadeFlags & STAGE_FADE_ADDITIVE)) {
            setlen(dr, 1);
            dr->code[0] = _get_mode(0, 1, getTPage(0, GPU_BLEND_SUBTRACT, 0, 0));
        } else {
            setlen(dr, 1);
            dr->code[0] = _get_mode(0, 1, getTPage(0, GPU_BLEND_ADD, 0, 0));
        }

        addPrim(&gGpuCurrentOt[otIdx], tile);
        addPrim(&gGpuCurrentOt[otIdx], dr);
    }
}

static s32 Display_TransitionLoad(Task* unused)
{
    RECT rect;

    switch (Stage_Ctx->transitionStep) {
        case 0:
            SetDispMask(0);
            Stage_Ctx->heldFrameBuffer = gDisplayState.frameBuffer;
            gDisplayState.keepGraphics = 1;
            gfxRestoreAreaFrame(gGameSession->location.loc.stage, gGameSession->location.loc.area, gDisplayState.frameBuffer);
            gDisplayState.control.flags.flipMode = DISPLAY_FLIP_HOLD;
            Stage_Ctx->transitionStep            = Stage_Ctx->transitionStep + 1;
            break;
        case 1:
            if (CdCmd_IsIdle() & 0xFFFF) {
                cdCmdEnqueue(CD_COMMAND_LOAD_FILE, Stage_Ctx->loadFileKey, Stage_Ctx->loadFileArgs);
                Stage_Ctx->transitionStep = Stage_Ctx->transitionStep + 1;
            }
            break;
        case 2:
            if ((CdCmd_IsIdle() & 0xFFFF) && (gDisplayState.frameBuffer != Stage_Ctx->heldFrameBuffer)) {
                gfxCaptureAreaFrame(gGameSession->location.loc.stage, gGameSession->location.loc.area, gDisplayState.frameBuffer,
                                    MEMORY_PRIMITIVE_HEAP_BYTES);
                memInitAuxHeap();
                rect.x = 0;
                rect.w = 0x140;
                rect.h = 0xF0;
                rect.y = (gDisplayState.frameBuffer ^ 1) * 0x110;
                ClearImage(&rect, 0, 0, 0);
                rect.x = 0;
                rect.w = 0x140;
                rect.h = 0xF0;
                rect.y = gDisplayState.frameBuffer * 0x110;
                ClearImage(&rect, 0, 0, 0);
                DrawSync(0);
                Stage_Ctx->loadBuffersCleared = 1;
                Stage_Ctx->transitionStep     = Stage_Ctx->transitionStep + 1;
            }
            break;
        case 3:
            gDisplayState.control.flags.flipMode    = DISPLAY_FLIP_TASK_ONLY;
            gDisplayState.control.flags.imageSource = DISPLAY_IMAGE_TRANSITION_STRIPS;
            Stage_Ctx->transitionStep               = Stage_Ctx->transitionStep + 1;
        default:
            SetDispMask(1);
            Stage_Ctx->requestFlags = Stage_Ctx->requestFlags & ~STAGE_REQUEST_FILE_LOAD;
            break;
    }
    return 1;
}

static Task* Display_SpawnFromMode(void)
{
    Task*            ret;
    Task*            slot;
    GameActor*       obj;
    GfxCoord*        ptr;
    GameLocationKey* ed;
    s32              flag;

    ret = taskSpawnFromTable(Stage_Ctx->taskDesc, 0, Stage_Ctx->spawnArg1, Stage_Ctx->spawnArg2);
    if (ret != NULL) {
        switch (Stage_Ctx->entryMode) {
            case STAGE_ENTRY_DRAW_ACTORS:
                Stage_Ctx->transitionKind = STAGE_TRANSITION_ACTORS;
                slot                      = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
                obj                       = (GameActor*)slot->work;
                flag                      = obj->collisionEnableMask & 1;
                ptr                       = slot->extra.tmd->coords;
                if (flag) {
                    func_801011D0(ptr, obj->collisionMotionContexts[0].contacts, 6, &obj->surfaceClass);
                }
                worldCollisionClearContacts(obj->collisionContacts);
                ptr->composeStamp = GRAPHICS_COORD_DIRTY;
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
            case STAGE_ENTRY_GRAY_CAPTURE + 1: // placeholder: the tree pivots on DRAW_ACTORS, so two values (or a range) above it were listed
            default:
                ed = &gGameSession->location.loc;
                gpuResetAndInvalidateModelBuffers();
                gfxCaptureAreaFrame(ed->stage, ed->area, gDisplayState.drawBuffer, MEMORY_PRIMITIVE_HEAP_BYTES);
                if (Stage_Ctx->entryMode == STAGE_ENTRY_GRAY_CAPTURE) {
                    _gfxInvertCapturedFrameGray();
                }
                memInitAuxHeap();
                gDisplayState.control.flags.flipMode    = DISPLAY_FLIP_TASK_ONLY;
                gDisplayState.control.flags.imageSource = DISPLAY_IMAGE_ROOM_SLOT;
                slot                                    = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
                obj                                     = (GameActor*)slot->work;
                flag                                    = obj->collisionEnableMask & 1;
                ptr                                     = slot->extra.tmd->coords;
                if (flag) {
                    func_801011D0(ptr, obj->collisionMotionContexts[0].contacts, 6, &obj->surfaceClass);
                }
                worldCollisionClearContacts(obj->collisionContacts);
                ptr->composeStamp = GRAPHICS_COORD_DIRTY;
                break;
        }
    }
    return ret;
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
        Pad_SetCooldown(0);
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
                    Pad_SetCooldown(0);
                    Gp_SpawnCurView(2);
                    gGameSession->viewReady = 0;
                    Task_Spawn(0, 0x1E, 2, 0);
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
                    CdCmd_ActivatePhase2();
                }
                break;
            case 2:
                gDisplayState.otBuffer = gDisplayState.frameBuffer;
                Display_FlipOtAndDispatch(0);
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
                        Display_TaskLoadStep(task);
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
        Display_TransitionLoad(task);
    } else if ((s32)flags < 0) {
        task->state = task->state + 1;
        Display_TaskLoadStep(task);
    } else if (flags & STAGE_REQUEST_CAPTURE) {
        gfxCaptureAreaFrame(gGameSession->location.loc.stage, gGameSession->location.loc.area, gDisplayState.frameBuffer,
                            MEMORY_PRIMITIVE_HEAP_BYTES);
        Stage_Ctx->requestFlags = Stage_Ctx->requestFlags & ~STAGE_REQUEST_CAPTURE;
    }

    if (Stage_Ctx->entryMode == STAGE_ENTRY_DRAW_ACTORS) {
        Display_FlipOtAndDispatch(0);
    }
}

static void Display_FlipOtAndDispatch(s32 unused)
{
    DisplayState* temp;
    u_long*       saved;
    s32           buf;
    u32           mode;

    temp           = &gDisplayState;
    saved          = gGpuCurrentOt;
    buf            = temp->otBuffer ^ 1;
    temp->otBuffer = buf;
    gpuBeginOt(buf);
    temp->control.flags.flipMode = DISPLAY_FLIP_FULL;
    temp->drawBuffer             = temp->frameBuffer;
    mode                         = Stage_Ctx->transitionKind;
    switch (mode) {
        case STAGE_TRANSITION_TASKS:
        case STAGE_TRANSITION_TASKS_ALT:
            taskExecDefaultList(&gTaskDefaultList);
            break;
        case STAGE_TRANSITION_ACTORS:
            spriteLinkViewCachedPackets();
            actorRenderComposeAndDrawActiveModels(&Gpu_OtBuffers[temp->otBuffer]);
            break;
        case STAGE_TRANSITION_FILTERED:
            taskExecListForPriority(&gTaskDefaultList, 0x62);
            spriteLinkViewCachedPackets();
            Gp_DrawActorTmdFlagged(&Gpu_OtBuffers[temp->otBuffer]);
            break;
    }
    gGpuCurrentOt = saved;
}

/// Replaces four RGB555 pixels in two writable words with inverted grey, clearing bit 15.
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

    Gpu_InitOtSmall();
    temp                         = &gDisplayState;
    temp->displayOwner           = DISPLAY_OWNER_TRANSITION;
    temp->control.flags.flipMode = DISPLAY_FLIP_HOLD;
    temp->frameBuffer            = temp->otBuffer ^ 1;
    taskInitList(&gTaskDisplayList);
    taskSpawnFromTable(&Display_ModeTaskDesc, 0, 0, 0);
}

s32 Stage_SetEndingFlag(void)
{
    Stage_Ctx->requestFlags |= STAGE_REQUEST_ENDING;
    return 0;
}

s32 Stage_BeginTransition(s32 arg0, s32 arg1)
{
    StageCtx* stage;
    s32       mask;

    mask = STAGE_REQUEST_TRANSITION;
    if (!(Stage_Ctx->requestFlags & mask)) {
        Pad_SetCooldown(0);
        stage                  = Stage_Ctx;
        stage->pendingView     = arg0;
        stage->heldFrameBuffer = 0;
        stage->transitionStep  = 0;
        stage->transitionKind  = arg1;
        stage->requestFlags   |= mask;
    }
    return gGameSession->location.loc.view;
}

s32 Stage_BeginTransitionKind7(s32 arg0)
{
    StageCtx* stage;
    s32       mask;
    s32       ret;

    mask = STAGE_REQUEST_TRANSITION;
    ret  = -1;
    if (!(Stage_Ctx->requestFlags & mask)) {
        Pad_SetCooldown(0);
        stage                    = Stage_Ctx;
        stage->pendingView       = arg0;
        stage->heldFrameBuffer   = 0;
        stage->transitionStep    = 0;
        stage->transitionKind    = STAGE_TRANSITION_KIND_7;
        stage->requestFlags     |= mask;
        ret                      = gGameSession->location.loc.view;
        Stage_Ctx->requestFlags |= STAGE_REQUEST_ENDING;
    }
    return ret;
}

s32 Stage_RequestImageCapture(void)
{
    Stage_Ctx->requestFlags |= STAGE_REQUEST_CAPTURE;
    return 0;
}

s32 Stage_SetFadeRate(s32 arg0, s32 arg1, s32 arg2, s32 arg3)
{
    if (arg2 == 0) {
        Stage_Ctx->fadeStep = STAGE_FADE_DEFAULT_STEP;
    } else {
        Stage_Ctx->fadeStep = arg2;
    }
    if (arg0 != 0) {
        Stage_Ctx->fadeStep = -Stage_Ctx->fadeStep;
    }
    Stage_Ctx->fadeFlags = 0;
    if (arg1 != 0) {
        Stage_Ctx->fadeFlags |= STAGE_FADE_ADDITIVE;
    }
    if (arg3 != 0) {
        Stage_Ctx->fadeFlags |= STAGE_FADE_FRONT;
    }
    return 0;
}

s32 Stage_GetFadeStatus(void)
{
    StageCtx* stage;
    u8        temp_a0;

    stage   = Stage_Ctx;
    temp_a0 = stage->fadeLevel;
    if (temp_a0 == 0) {
        return 0;
    }
    if (temp_a0 >= stage->fadeMax) {
        return 1;
    }
    return -1;
}

s32 Stage_HasTransitionFlags(void)
{
    return (Stage_Ctx->requestFlags & STAGE_REQUEST_BUSY) != 0;
}

void Stage_InitOtOnce(void)
{
    if (Stage_Ctx->fullOtReady == 0) {
        Gpu_InitOt();
        Stage_Ctx->fullOtReady = 1;
    }
}

void Stage_InitPrimBufOnce(void)
{
    if (Stage_Ctx->largePrimBuf == 0) {
        Display_SetPrimBufLarge();
        Stage_Ctx->largePrimBuf = 1;
    }
}

void Stage_ReleasePrimBuf(void)
{
    if (Stage_Ctx->largePrimBuf == 1) {
        Display_SetPrimBufSmall();
        Stage_Ctx->largePrimBuf = 0;
    }
}

void Stage_SetFadeMax(u8 arg0)
{
    Stage_Ctx->fadeMax = arg0;
}

void Display_SetDrawMode(s32 arg0)
{
    switch (arg0) {
        case 0:
            gDisplayState.control.flags.flipMode    = DISPLAY_FLIP_TASK_ONLY;
            gDisplayState.control.flags.imageSource = DISPLAY_IMAGE_NONE;
            displaySetClearColor(0, 0, 0);
            return;
        case 1:
            gDisplayState.control.flags.flipMode    = (u8)arg0;
            gDisplayState.control.flags.imageSource = DISPLAY_IMAGE_ROOM_SLOT;
            displaySetClearColor(DISPLAY_CLEAR_DISABLED, 0, 0);
            return;
        case 2:
            gDisplayState.control.flags.flipMode    = DISPLAY_FLIP_TASK_ONLY;
            gDisplayState.control.flags.imageSource = DISPLAY_IMAGE_TRANSITION_STRIPS;
            displaySetClearColor(DISPLAY_CLEAR_DISABLED, 0, 0);
            return;
        case 3:
            gDisplayState.control.flags.flipMode = DISPLAY_FLIP_HOLD;
            return;
    }
}

/// View transition that keeps the current view, with the transition kind fixed to 3.
static s32 Stage_BeginTransitionKind3(void)
{
    StageCtx* stage;
    u32       flags;
    s32       val;

    stage = Stage_Ctx;
    flags = stage->requestFlags;
    if (!(flags & STAGE_REQUEST_TRANSITION)) {
        stage->requestFlags    = flags | STAGE_REQUEST_KEEP_TRANSITION;
        val                    = gGameSession->location.loc.view;
        stage->heldFrameBuffer = 0;
        stage->transitionStep  = 0;
        stage->transitionKind  = STAGE_TRANSITION_TASKS;
        stage->pendingView     = val;
    }
    return 0;
}

Task* Display_InitModeObj(TaskDesc* descriptor, s32 arg1, TaskSpawnArg arg2, s32 arg3)
{
    StageCtx* stage;

    if (gDisplayState.pendingMode != DISPLAY_MODE_NONE) {
        return 0;
    }

    MEM_CLEAR(Stage_Ctx, sizeof(StageCtx));

    stage            = Stage_Ctx;
    stage->taskDesc  = descriptor;
    stage->spawnArg1 = arg1;
    stage->spawnArg2 = arg2;
    stage->entryMode = arg3;
    // A default request in the Acropolis plaza keeps the current resources.
    if (arg3 == STAGE_ENTRY_RELOAD) {
        if ((GAME_LOCATION_WORD(gGameSession->location.loc) & GAME_LOCATION_STAGE_AREA_MASK) ==
            GAME_LOCATION_KEY(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_PLAZA, 0, 0)) {
            stage->entryMode = STAGE_ENTRY_KEEP;
        }
    }
    Stage_Ctx->fadeMax        = STAGE_FADE_OPAQUE;
    gDisplayState.pendingMode = DISPLAY_MODE_DESCRIPTOR;
    return 0;
}

s32 Stage_GetModeByte12(void)
{
    return Stage_Ctx->loadBuffersCleared;
}

static void Stage_SetModeAndFlip(u8 arg0)
{
    StageCtx* stage;

    stage = Stage_Ctx;
    if (stage->otFlipArmed == 1) {
        stage->transitionKind = arg0;
        Display_FlipOtAndDispatch(0);
    }
}

void Stage_ResetFade(void)
{
    Stage_Ctx->fadeLevel = 0;
    Stage_Ctx->fadeMax   = STAGE_FADE_OPAQUE;
}

static void Stage_WaitCdActivate(Task* task)
{
    Pad_SetCooldown(0);
    if (CdCmd_ActivatePhase2() != 0) {
        task->state += 1;
    } else {
        gPadStates[0].inputBlockPolls = 1;
        Display_SpawnFromMode();
        task->state += 2;
    }
}

static void Stage_WaitCdAndSpawn(Task* task)
{
    Pad_SetCooldown(0);
    if (CdCmd_IsIdleOrOverlayPending() != 0) {
        gPadStates[0].inputBlockPolls = 1;
        Display_SpawnFromMode();
        task->state += 1;
    }
}

static void Display_TaskLoadStep(Task* task)
{
    u32 temp_v1;

    gDisplayState.control.flags.flipMode = DISPLAY_FLIP_HOLD;
    temp_v1                              = Stage_Ctx->entryMode;
    // Modes 1, 3 and 4 keep the room's current resources.
    if (temp_v1 >= 5U || (temp_v1 < 3U && temp_v1 != STAGE_ENTRY_KEEP)) {
        memConfigureImageMemory(gGameSession->location.loc.stage, gGameSession->location.loc.area);
        tmdResetAuxHeapAndRestoreBuffers();
        spriteAllocateViewCachedPackets();
    }
    CdCmd_EnqueueLoadFile(0, 0, 4);
    task->state = (s32)(task->state + 1);
    Stage_WaitCdEntry(task);
}

static void Stage_WaitCdEntry(Task* task)
{
    if (CdCmd_IsIdleOrOverlayPending() != 0) {
        task->state += 1;
    }
}

static void Stage_FinishCdFollowUp(Task* task)
{
    if (CdCmd_EnqueueFollowUp() != 0) {
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
    Display_StepFadeOverlay();
}

void Mdec_ResolveStreamBuffer(u8* arg0)
{
    u16         i;
    u16         found;
    s16         bufferKind;
    s16         neg;
    s32         key;
    s32         imageDataOffset;
    CdCmdQueue* p;
    u8*         base;

    p     = &gCdCmdQueue;
    i     = 0;
    found = 0;
    key   = *arg0;
loop:
    if (key == p->sceneImageHeaders[i].viewId) {
        goto matched;
    }
    i++;
    if (i < ARRAY_SIZE(p->sceneImageHeaders)) {
        goto loop;
    }
done:
    if ((found & 0xFFFF) != 0) {
        if (p->scenePayloadLoading == 0) {
            goto success;
        }
    }
    p->imageDecodePending = 1;
    p->imageLoadStatus    = CD_COMMAND_IMAGE_PENDING;
    neg                   = CD_COMMAND_IMAGE_WAIT_HEADER;
    p->imageDecodeStep    = neg;
    return;

matched:
    found = 1;
    goto done;

success:
    Stage_CdEntry = &p->sceneImageHeaders[i];
    bufferKind    = Stage_CdEntry->bufferKind;
    switch (bufferKind) {
        case STREAM_SCENE_BUFFER_DECODE:
            base = p->decodeBuffer;
            goto store_base;
        case STREAM_SCENE_BUFFER_ACTOR_0:
            Mdec_DecodeBase = (u8*)Fs_ActorLoadBase0;
            if (p->sceneStream->data.scene.vlcBufferKind == STREAM_VLC_BUFFER_ACTOR_0) {
                Mdec_DecodeBase = (u8*)Fs_ActorLoadBase0 + STREAM_VLC_TABLE_BYTES;
            }
            if (p->sceneStream->control.scene.timingBufferKind == STREAM_TIMING_BUFFER_ACTOR_0) {
                Mdec_DecodeBase = Mdec_DecodeBase + p->sceneStream->data.scene.timingBufferBytes;
            }
            gGameSession->field_7C = 0;
            break;
        case STREAM_SCENE_BUFFER_ACTOR_1:
            Mdec_DecodeBase = (u8*)Fs_ActorLoadBase1;
            if (p->sceneStream->data.scene.vlcBufferKind == STREAM_VLC_BUFFER_ACTOR_1) {
                Mdec_DecodeBase = (u8*)Fs_ActorLoadBase1 + STREAM_VLC_TABLE_BYTES;
            }
            if (p->sceneStream->control.scene.timingBufferKind == STREAM_TIMING_BUFFER_ACTOR_1) {
                Mdec_DecodeBase = Mdec_DecodeBase + p->sceneStream->data.scene.timingBufferBytes;
            }
            gGameSession->field_7E = 0;
            break;
        case STREAM_SCENE_BUFFER_ACTOR_2:
            Mdec_DecodeBase = (u8*)Fs_ActorLoadBase2;
            if (p->sceneStream->data.scene.vlcBufferKind == STREAM_VLC_BUFFER_ACTOR_2) {
                Mdec_DecodeBase = (u8*)Fs_ActorLoadBase2 + STREAM_VLC_TABLE_BYTES;
            }
            if (p->sceneStream->control.scene.timingBufferKind == STREAM_TIMING_BUFFER_ACTOR_2) {
                Mdec_DecodeBase = Mdec_DecodeBase + p->sceneStream->data.scene.timingBufferBytes;
            }
            gGameSession->field_80 = 0;
            break;
        case STREAM_SCENE_BUFFER_EXTERNAL:
            base = p->externalScenePayloadBuffer;
        store_base:
            Mdec_DecodeBase = base;
            break;
    }
    imageDataOffset       = Stage_CdEntry->imageDataOffset;
    D_8007A35C            = 0;
    p->imageDecodePending = 1;
    p->imageLoadStatus    = CD_COMMAND_IMAGE_PENDING;
    p->imageDecodeStep    = CD_COMMAND_IMAGE_START;
    D_8007A360            = Mdec_DecodeBase + imageDataOffset;
}

static __inline__ void mdecFinishDecode(void)
{
    CdCmdQueue* q = &gCdCmdQueue;

    if (gDisplayState.keepGraphics == 0) {
        tmdResetAuxHeapAndRestoreBuffers();
    }
    q->imageLoadStatus      = CD_COMMAND_IMAGE_COMPLETE;
    q->imageDecodePending   = 0;
    D_8007A35C              = 0;
    q->imageDecodeStep      = CD_COMMAND_IMAGE_START;
    q->scenePayloadReusable = 0;
}

/// imageDecodeStep state machine: start DCT, apply work-lists / image chunks, complete.
static void Mdec_ProcessDecode(void)
{
    enum {
        STREAM_SCENE_STRIP_X_SHIFT_PAGES = -8,
        STREAM_SCENE_CHUNK_Y_SHIFT_ROWS  = -3,
    };
    CdCmdQueue* p;
    u16         i;
    s32         r;

    p = &gCdCmdQueue;
    switch ((s16)p->imageDecodeStep) {
        case CD_COMMAND_IMAGE_WAIT_HEADER:
            Mdec_ResolveStreamBuffer(&gGameSession->location.loc.view);
            if ((u32)++D_8007A358 >= 0x5B) {
                D_8007A358 = 0;
                gpuResetAndInvalidateModelBuffers();
                if (Stage_CdEntry->bufferKind == STREAM_SCENE_BUFFER_DECODE) {
                    p->decodeBufferBytes = p->nextDecodeBufferBytes;
                }
                mdecFinishDecode();
            }
            break;
        case CD_COMMAND_IMAGE_START:
            gpuResetAndInvalidateModelBuffers();
            p->mdecOutputPending = 1;
            if (p->sceneVlcTableMode == STREAM_SCENE_VLC_IMAGE_BUFFER) {
                DecDCTvlcBuild((u16*)((u8*)Fs_ImgBuffers + FILE_SYSTEM_IMAGE_VLC_OFFSET));
                p->rebuildImageVlcTable = 0;
                p->vlcTable             = (u16*)((u8*)Fs_ImgBuffers + FILE_SYSTEM_IMAGE_VLC_OFFSET);
            }
            DecDCTReset(0);
            DecDCTvlcSize2(0);
            DecDCTvlc2((u_long*)D_8007A360, gMemActiveAuxHeap,
                       p->vlcTable);
            D_8007A35E = 1;
            DecDCToutCallback(Mdec_StripCallback);
            DecDCTin(gMemActiveAuxHeap, p->imageMdecMode);
            p->imageMdecMode = MDEC_IMAGE_MODE_RGB16;
            DecDCTout(Fs_ImgBuffers->strips[0], FILE_SYSTEM_IMAGE_STRIP_WORDS);
            D_8007A358 = 0;
            p->imageDecodeStep++;
            /* fallthrough */
        case CD_COMMAND_IMAGE_WAIT_OUTPUT:
            if (p->mdecOutputPending == 0) {
                // Upload the payload's optional strip lists and image chunks.
                for (i = 0; i < ARRAY_SIZE(Stage_CdEntry->stripListOffsets); i++) {
                    if (Stage_CdEntry->stripListOffsets[i] != 0) {
                        if (Stage_CdEntry->relocateStripLists[i] != 0) {
                            Fs_ChunkMode    = 2;
                            D5B498_8006C233 = STREAM_SCENE_STRIP_X_SHIFT_PAGES;
                        }
                        Fs_CopyWorkEntries((FsImageColumn*)(Mdec_DecodeBase + Stage_CdEntry->stripListOffsets[i]));
                        while (Fs_LoadImageStrip(1) != 1) {
                            r = Fs_LoadImageStrip(1);
                            if (r == 1) {
                                break;
                            }
                            if (r == 0x7F) {
                                Fs_CopyWorkEntries((FsImageColumn*)(Mdec_DecodeBase + Stage_CdEntry->stripListOffsets[i]));
                            }
                        }
                        Fs_ChunkMode    = 0;
                        D5B498_8006C233 = 0;
                    }
                }
                for (i = 0; i < ARRAY_SIZE(Stage_CdEntry->imageChunkOffsets); i++) {
                    if (Stage_CdEntry->imageChunkOffsets[i] != 0) {
                        if (Stage_CdEntry->relocateImageChunks[i] != 0) {
                            Fs_ChunkMode    = 2;
                            D5B498_8006C234 = STREAM_SCENE_CHUNK_Y_SHIFT_ROWS;
                        }
                        while (Fs_LoadImageChunk((FsImageChunk*)(Mdec_DecodeBase + Stage_CdEntry->imageChunkOffsets[i]), 1)) {
                        }
                        Fs_ChunkMode    = 0;
                        D5B498_8006C234 = 0;
                    }
                }
                p->imageLayout = FILE_SYSTEM_IMAGE_STRIPS;
                if (Stage_CdEntry->bufferKind == STREAM_SCENE_BUFFER_DECODE) {
                    p->decodeBufferBytes = p->nextDecodeBufferBytes;
                }
                // Refresh timing data before releasing the completed decode operation.
                if (p->sceneStream->control.scene.timingBufferKind != STREAM_TIMING_BUFFER_NONE) {
                    memCopyBytes(&Mdec_DecodeBase[Stage_CdEntry->timingDataOffset], p->timingBuffer,
                                 Stage_CdEntry->timingDataBytes);
                }
                mdecFinishDecode();
            } else if ((u32)++D_8007A358 >= 0x5B) {
                D_8007A358 = 0;
                gpuResetAndInvalidateModelBuffers();
                if (Stage_CdEntry->bufferKind == STREAM_SCENE_BUFFER_DECODE) {
                    p->decodeBufferBytes = p->nextDecodeBufferBytes;
                }
                mdecFinishDecode();
            }
            break;
    }
}

static void Mdec_DecodeToVram(void)
{
    RECT          rect;
    s32           i;
    s32           temp;
    CdCmdQueue*   p;
    CdCmdQueue*   q;
    DisplayState* d;

    p = &gCdCmdQueue;
    switch ((s16)p->imageDecodeStep) {
        case CD_COMMAND_IMAGE_START:
            gpuResetAndInvalidateModelBuffers();
            p->mdecOutputPending = 1;
            DecDCTReset(0);
            DecDCTvlcSize2(0);
            DecDCTvlc2((u_long*)D_8007A360, gMemActiveAuxHeap,
                       (u_short*)((u8*)Fs_ImgBuffers + FILE_SYSTEM_IMAGE_VLC_OFFSET));
            D_8007A35E = 1;
            DecDCToutCallback(Mdec_StripCallback);
            DecDCTin(gMemActiveAuxHeap, p->imageMdecMode);
            p->imageMdecMode = MDEC_IMAGE_MODE_RGB16;
            DecDCTout(Fs_ImgBuffers->strips[0], FILE_SYSTEM_IMAGE_STRIP_WORDS);
            p->imageDecodeStep += 1;
            /* fallthrough */
        case CD_COMMAND_IMAGE_WAIT_OUTPUT:
            i = 0;
            if (p->mdecOutputPending == 0) {
                rect.w = FILE_SYSTEM_IMAGE_STRIP_WIDTH;
                rect.h = FILE_SYSTEM_IMAGE_HEIGHT;
                rect.y = (gDisplayState.drawBuffer ^ 1) * 0x110;
                do {
                    temp   = i & 0xFFFF;
                    rect.x = temp * FILE_SYSTEM_IMAGE_STRIP_WIDTH;
                    LoadImage(&rect, Fs_ImgBuffers->strips[temp]);
                    i++;
                } while ((u32)(i & 0xFFFF) < (u32)FILE_SYSTEM_IMAGE_STRIP_COUNT);
                rect.w = FILE_SYSTEM_IMAGE_WIDTH;
                rect.x = 0;
                rect.h = FILE_SYSTEM_IMAGE_HEIGHT;
                d      = &gDisplayState;
                rect.y = (d->drawBuffer ^ 1) * 0x110;
                StoreImage(&rect, Fs_ImgBuffers->strips[0]);
                if (p->preserveDisplayAfterDecode != 0) {
                    rect.x = 0;
                    rect.w = 0x1E0;
                    rect.h = FILE_SYSTEM_IMAGE_HEIGHT;
                    rect.y = d->drawBuffer * 0x110;
                    MoveImage(&rect, 0, (d->drawBuffer ^ 1) * 0x110);
                    p->preserveDisplayAfterDecode = 0;
                } else {
                    ClearImage(&rect, 0, 0, 0);
                }
                p->imageLayout = FILE_SYSTEM_IMAGE_CONTIGUOUS;
                q              = &gCdCmdQueue;
                if (gDisplayState.keepGraphics == 0) {
                    tmdResetAuxHeapAndRestoreBuffers();
                }
                q->imageLoadStatus      = CD_COMMAND_IMAGE_COMPLETE;
                q->imageDecodePending   = 0;
                D_8007A35C              = 0;
                q->imageDecodeStep      = CD_COMMAND_IMAGE_START;
                q->scenePayloadReusable = 0;
            }
            return;
    }
}

void CdCmd_StepVlcRebuild(void)
{
    CdCmdQueue* p;

    p = &gCdCmdQueue;
    if (p->scenePayloadAvailable == 0) {
        if (p->rebuildImageVlcTable != 0) {
            DecDCTvlcBuild((u16*)((u8*)Fs_ImgBuffers + FILE_SYSTEM_IMAGE_VLC_OFFSET));
            p->rebuildImageVlcTable = 0;
        }
        if ((p->imageDecodePending != 0) && (p->rebuildImageVlcTable == 0)) {
            Mdec_DecodeToVram();
        }
    } else if (p->imageDecodePending != 0) {
        Mdec_ProcessDecode();
    }
}

void Mdec_BeginDecode(void* arg0)
{
    CdCmdQueue* p;

    D_8007A35C            = 0;
    p                     = &gCdCmdQueue;
    p->imageDecodePending = 1;
    p->imageLoadStatus    = CD_COMMAND_IMAGE_PENDING;
    p->imageDecodeStep    = CD_COMMAND_IMAGE_START;
    D_8007A360            = arg0;
    D_8007A358            = 0;
}

void mdecRequestImageVlcRebuild(void)
{
    gCdCmdQueue.rebuildImageVlcTable = true;
}

static void Mdec_StripCallback(void)
{
    s32         temp;
    CdCmdQueue* p;

    temp = FILE_SYSTEM_IMAGE_WIDTH / (D_8007A35E * FILE_SYSTEM_IMAGE_STRIP_WIDTH);
    p    = &gCdCmdQueue;
    if (D_8007A35C == temp - 1) {
        p->mdecOutputPending = 0;
        DecDCToutCallback(0);
    } else {
        D_8007A35C = D_8007A35C + 1;
        DecDCTout(Fs_ImgBuffers->strips[D_8007A35C], FILE_SYSTEM_IMAGE_STRIP_WORDS);
    }
}

void taskExitCallback(Task* task)
{
    taskCallExit(task);
}
