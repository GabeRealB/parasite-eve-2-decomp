#include "main/gameflow.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libpad.h>

#include "common.h"

#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/fs_types.h"
#include "gameflow.h"
#include "main/gamemain.h"
#include "main/loadui.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "mem.h"
#include "pad.h"
#include "main/pad_types.h"
#include "pad_types.h"
#include "main/scratch.h"
#include "main/session.h"
#include "session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/ui.h"
#include "ui.h"
#include "main/ui_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "gameplay/model_lighting.h"

#include "title/title.h"

/// Working values of one controller poll, kept in the polling function's stack frame.
///
/// The port number and its connection state are held across the libpad calls
/// that configure the controller; the other two members are the intermediate
/// values of one analog axis's normalization and are rewritten for every axis.
/// The frame leaves room for at most four more bytes after the last member
/// accessed, so the size asserted is the proven minimum.
typedef struct {
    s32  state;        // libpad connection state of the port (`PadStateDiscon` .. `PadStateStable`)
    byte unknown_4[4]; // Never accessed; type and role unproven
    s32  delta;        // Axis in progress: raw reading minus its center, then the travel past the dead zone in Q12, then that divided by `range`
    s32  range;        // Raw travel from the dead zone's edge to the full-scale reading on the deflected side; at least 1 for a clamped center
    s32  portId;       // libpad port number: 0x10 per controller port, multitap slot 0
} _PadPollWork;
STATIC_ASSERT_SIZEOF(_PadPollWork, 0x14);

/// Button words of one port's input update, held on the scratch stack while the update runs.
///
/// The controller reports its buttons as two active-low bytes. They are copied
/// into `rawButtons` in the order that makes the pair one halfword, and its
/// complement becomes `buttons`, the active-high word every consumer tests
/// (bit layout of libetc's `PADL*`/`PADR*` masks). `buttons` is then amended in
/// place - an analog controller's left stick adds the D-pad bit of each
/// direction it is pushed past half travel, and an active input override may
/// substitute a replayed word - before it is compared with `prevButtons` to
/// derive the port's pressed and released edges and stored as the port's held
/// buttons. An update that ends an input block takes the complement alone.
///
/// One block serves every port the update visits, and nothing is carried
/// between updates: each member is rewritten before it is read.
typedef struct {
    u16 buttons;     // Buttons held this update, active high; becomes `PadState::buttons`
    u16 prevButtons; // `PadState::buttons` as the preceding update left it
    union {
        u16 word;    // Both bytes as one button word, still active low
        struct {
            u8 low;  // `PadRawPort::buttonsLow`
            u8 high; // `PadRawPort::buttonsHigh`
        } bytes;
    } rawButtons;    // Controller's button bytes as received, active low
} _PadScratch;
STATIC_ASSERT_SIZEOF(_PadScratch, 0x6);

/* Define BSS before API headers to preserve first-declaration order. */
/// Resident storage for the live session exposed through `gGameSession`.
///
/// Its address stays fixed across overlay loads. New-game, load and reset paths
/// clear its contents without releasing the storage.
static GameSession _gGameSessionState;

/// Unreferenced.
static u8 D_80071600[0x20];

PadState gPadStates[PAD_PORT_COUNT];

#include "main/pad.h"

/// Unreferenced.
static s32 D_8005ED6C;

/// Unreferenced.
static s32 D_8005ED7C;

/// Unreferenced.
static s32 D_8005ED80;

static u8 D_8005ED84[];

static const TaskFuncTable5 GameFlow_States5;

static const TaskFuncTable3 GameFlow_States3;

static void GameFlow_InitSystems(void);

static void Game_ResetSessionAndBuffers(Task* task);

static void GameFlow_SpawnMenu(Task* task);

static void GameFlow_WaitMenuDone(Task* task);

static void GameFlow_CountdownAdvance(Task* task);

static void GameFlow_SpawnMainWhenReady(Task* task);

static void GameFlow_CopySaveIds(Task* task);

static void GameFlow_EnqueueDefaultLoad(Task* task);

static void GameFlow_SpawnWhenIdle(Task* task);

static void Pad_TickEventBanks(PadState* pad);

enum {
    PAD_DIRECTION_REPEAT_DELAY_TICKS   = 30,
    PAD_DIRECTION_REPEAT_RESTART_TICKS = 22,
    PAD_DIRECTION_BUTTON_MASK          = 0xF000,
    PAD_STICK_CENTER_WORD              = 0x80808080,
    PAD_STICK_DEAD_ZONE_RAW            = 24,
    PAD_LEGACY_VIBRATION_PREFIX        = 0x40,
};

GameSession* gGameSession = &_gGameSessionState;
s32          D_8005ED68   = 0;
/// Unreferenced.
static s32 D_8005ED6C      = 0x40;
s32        Pad_MaskConfirm = 0x40;
s32        Pad_MaskCancel  = 0xA0;
s32        Pad_MaskMenu    = 0x900;
/// Unreferenced.
static s32 D_8005ED7C = 0x10;
/// Unreferenced.
static s32 D_8005ED80   = 0x80;
static u8  D_8005ED84[] = { 0x00, 0x01, 0xFF, 0xFF, 0xFF, 0xFF };
u16        D_8005ED8A   = 0;

static const TaskFuncTable5 GameFlow_States5 = { {
    Game_ResetSessionAndBuffers,
    GameFlow_SpawnMenu,
    GameFlow_WaitMenuDone,
    GameFlow_CountdownAdvance,
    GameFlow_SpawnMainWhenReady,
} };

static const TaskFuncTable3 GameFlow_States3 = { {
    GameFlow_CopySaveIds,
    GameFlow_EnqueueDefaultLoad,
    GameFlow_SpawnWhenIdle,
} };

void GameFlow_StateByField34(Task* task)
{
    CdCmdQueue* p;
    s32         saved;

    p = &gCdCmdQueue;
    if (task->spawnArg1.value == 2) {
        if (task->state == 0) {
            Pad_SetCooldown(0);
            if (gDisplayState.demoScene == DISPLAY_DEMO_NONE) {
                gDisplayState.demoScene = 1;
            }
            if (gDisplayState.demoScene < DISPLAY_DEMO_FIXED_REPLAY) {
                Title_EnqueueDemoScene(gDisplayState.demoScene - 1);
            }
            task->state = task->state + 1;
        }
        if (CdCmd_IsIdle() != 0) {
            if (gDisplayState.spriteVariant == 0) {
                gDisplayState.spriteVariant = 1;
            }
            Title_RestoreDemoCard();
            MEM_CLEAR(gGameSession, sizeof(*gGameSession));
            gDisplayState.control.flags.pendingPlayerPos = 0;
            gDisplayState.gameRunning                    = 0;
            gGameSession->applySaveVariant               = 1;
            gGameSession->field_80                       = 0;
            Snd_SetMutedVolumes(1);
            gDisplayState.control.flags.pendingPlayerPos = 0;
            gDisplayState.stopTaskWalk                   = 1;
            taskKill(task);
            taskResetDefaultList();
            Tmd_InitLists();
            Mem_Init();
            Task_Spawn(0, 9, 0, 0);
        }
    } else {
        gDisplayState.demoScene = DISPLAY_DEMO_NONE;
        Pad_SetCooldown(0);
        if (task->spawnArg1.value == 0) {
            saved = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.vibration;
            MEM_CLEAR(gGameSession, sizeof(*gGameSession));
            gDisplayState.control.flags.pendingPlayerPos = 0;
            gDisplayState.gameRunning                    = 1;
            p->releasePauseBlockAfterFade                = 1;
            p->blockGamePause                            = 1;
            Wip_SysFlags.skipTitleIntro                  = 1;
            Mc_InitBufferSlots();
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.vibration = saved;
            task->state                                        = task->state + 1;
        } else {
            MEM_CLEAR(gGameSession, sizeof(*gGameSession));
            gDisplayState.gameRunning                    = 1;
            gDisplayState.control.flags.pendingPlayerPos = 0;
            p->releasePauseBlockAfterFade                = 1;
            p->blockGamePause                            = 1;
            Wip_SysFlags.skipTitleIntro                  = 1;
            gGameSession->applySaveVariant               = 1;
        }
        gDisplayState.stopTaskWalk = 1;
        taskKill(task);
        taskResetDefaultList();
        Tmd_InitLists();
        Mem_Init();
        Task_Spawn(0, 9, 0, 0);
    }
}

/// Queues the fade's draw mode ahead of the packets at the selected ordering-table tag.
///
/// The low two bits of `blendMode` select a `GPU_BLEND_*` mode. Dithering is
/// enabled and drawing into the displayed area is disabled. The untextured
/// fade tile does not sample the selected 4-bit texture page at VRAM (0, 0).
/// Queue the tile at the same tag first: insertion prepends this command so
/// the GPU applies the mode before drawing the tile, until another command
/// replaces it.
///
/// `otTagOffset` counts signed DMA tags from `gGpuCurrentOt`, not bytes or
/// sorting depth; the selected tag must be writable in the current table.
/// The frame packet arena must have `sizeof(DR_TPAGE)` word-aligned writable
/// bytes; the command borrows that storage until GPU drawing completes.
static inline void _fadeQueueBlendMode(s32 blendMode, s32 otTagOffset)
{
    enum { FADE_TEXTURE_DEPTH_4BIT = 0 };
    DR_TPAGE* blendCommand;

    blendCommand   = gGpuPrimCursor;
    gGpuPrimCursor = blendCommand + 1;
    setDrawTPage(blendCommand, false, true, getTPage(FADE_TEXTURE_DEPTH_4BIT, blendMode, 0, 0));
    addPrim(gGpuCurrentOt + otTagOffset, blendCommand);
}

void fadeDrawOverlay(u8 red, u8 green, u8 blue, s32 blendMode)
{
    enum {
        FADE_OVERLAY_WIDTH_PIXELS  = 320,
        FADE_OVERLAY_HEIGHT_PIXELS = 240,
        FADE_OVERLAY_OT_INDEX      = -16,
    };
    TILE* tile;
    s8    shakeY;

    tile           = gGpuPrimCursor;
    gGpuPrimCursor = tile + 1;
    setTile(tile);
    setSemiTrans(tile, true);
    tile->x0 = -FADE_OVERLAY_WIDTH_PIXELS / 2;
    tile->r0 = red;
    tile->g0 = green;
    tile->b0 = blue;
    shakeY   = gDisplayState.vramYOffset;
    tile->w  = FADE_OVERLAY_WIDTH_PIXELS;
    tile->h  = FADE_OVERLAY_HEIGHT_PIXELS;
    // Cancel the draw environment's shake so the overlay stays fixed on screen.
    tile->y0 = -FADE_OVERLAY_HEIGHT_PIXELS / 2 - shakeY;
    addPrim(gGpuCurrentOt + FADE_OVERLAY_OT_INDEX, tile);

    // OT insertion prepends: queue the blend command last so it executes first.
    _fadeQueueBlendMode(blendMode, FADE_OVERLAY_OT_INDEX);
}

void Game_ClearSession(void)
{

    MEM_CLEAR(gGameSession, sizeof(*gGameSession));
    gDisplayState.control.flags.pendingPlayerPos = 0;
}

static void GameFlow_InitSystems(void)
{
    taskResetDefaultList();
    Tmd_InitLists();
    Mem_Init();
    Task_Spawn(0, 9, 0, 0);
}

static void Game_ResetSessionAndBuffers(Task* task)
{
    s32         saved;
    CdCmdQueue* p;

    p     = &gCdCmdQueue;
    saved = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.vibration;
    MEM_CLEAR(gGameSession, sizeof(*gGameSession));
    gDisplayState.control.flags.pendingPlayerPos = 0;
    gDisplayState.gameRunning                    = 1;
    p->releasePauseBlockAfterFade                = 1;
    p->blockGamePause                            = 1;
    Wip_SysFlags.skipTitleIntro                  = 1;
    Mc_InitBufferSlots();
    do {
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.vibration = saved;
    } while (0);
    task->state = task->state + 1;
}

static void GameFlow_SpawnMenu(Task* task)
{
    void* temp_v0;

    GameMain_SetFrameTiming(DISPLAY_TIMING_EVERY_VBLANK);
    temp_v0                 = Ui_SpawnFromDesc(Mc_TaskDescriptors, 0, 1, 0, 0);
    task->spawnArg2.pointer = temp_v0;
    if (temp_v0 != 0) {
        gDisplayState.gameMode = DISPLAY_GAME_MODAL;
        gGameSession->uiOpen   = 1;
        task->killCountdown    = 0x10;
        task->state            = task->state + 1;
    }
}

static void GameFlow_WaitMenuDone(Task* task)
{
    UiObject* obj;

    obj = task->spawnArg2.pointer;
    if (obj->result == USER_INTERFACE_RESULT_CANCEL) {
        Ui_TeardownTree(obj, obj->owner);
        gDisplayState.gameMode = DISPLAY_GAME_ACTIVE;
        gGameSession->uiOpen   = 0;
        if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.soundMode == 1) {
            CdVol_SetMixMode(0);
        } else {
            CdVol_SetMixMode(1);
        }
        Snd_ApplyVolumeTable(0);
        task->killCountdown = 0xC;
        task->state         = task->state + 1;
    }
}

static void GameFlow_CountdownAdvance(Task* task)
{
    task->killCountdown--;
    if (task->killCountdown != 0) {
        return;
    }
    Pad_SetCooldown(0);
    task->state = task->state + 1;
}

static void GameFlow_SpawnMainWhenReady(Task* task)
{
    if (gDisplayState.control.flags.pendingPlayerPos == 0) {
        Task_Spawn(0, 2, 0, 0);
        Display_SetMode(DISPLAY_SETUP_DEFAULT | DISPLAY_SETUP_NO_CLEAR);
        taskKill(task);
        return;
    }
    gDisplayState.stopTaskWalk = 1;
    taskKill(task);
    taskResetDefaultList();
    Tmd_InitLists();
    Mem_Init();
    Task_Spawn(0, 9, 0, 0);
}

void GameFlow_DispatchTable5(Task* task)
{
    TaskFuncTable5 sp;

    sp = GameFlow_States5;
    sp.funcs[task->state](task);
}

static void GameFlow_CopySaveIds(Task* task)
{
    gGameSession->location = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location;
    D_8007A394             = 0;
    task->state            = task->state + 1;
}

static void GameFlow_EnqueueDefaultLoad(Task* task)
{
    u8 param1[8];
    u8 param2[8];

    if ((u8)LoadUi_PollDiskSwap() == 0) {
        Fs_BeginBootLoad((u8*)&gGameSession->location.loc, 0);
        param1[3] = 0;
        param1[2] = 0;
        param1[0] = 0;
        param2[0] = 0;
        param2[1] = 0;
        param2[2] = 0;
        param2[3] = 0;
        CdCmd_Enqueue(CD_COMMAND_LOAD_FILE, param1, param2);
        task->state = task->state + 1;
    }
}

void Game_ClearEd68(void)
{
    D_8005ED68 = 0;
}

static void GameFlow_SpawnWhenIdle(Task* task)
{
    if (CdCmd_IsIdle() != 0) {
        Task_Spawn(0, 0x11, 1, 0);
        taskKill(task);
    }
}

void GameFlow_DispatchTable(Task* task)
{
    TaskFuncTable3 sp;

    sp = GameFlow_States3;
    Pad_SetCooldown(0);
    sp.funcs[task->state](task);
}

static void Pad_TickEventBanks(PadState* pad)
{
    u8*                  motor;
    PadVibrationRequest* request;
    s32                  i;

    SCRATCH_STACK_RESERVE_BYTES(4);
    motor    = SCRATCH_STACK_CURSOR(u8);
    motor[1] = 0;
    motor[0] = 0;

    // Binary requests combine by logical OR, including each request's expiry poll.
    request = pad->vibrationRequests[PAD_VIBRATION_MOTOR_BINARY];
    for (i = 0; i < ARRAY_SIZE(pad->vibrationRequests[PAD_VIBRATION_MOTOR_BINARY]); i++, request++) {
        if (request->active != PAD_VIBRATION_INACTIVE) {
            if (--request->pollsRemaining == 0) {
                request->active = PAD_VIBRATION_INACTIVE;
            }
            if (request->intensity != 0) {
                motor[0] = 1;
            }
        }
    }

    // Variable motor requests combine by maximum intensity.
    request = pad->vibrationRequests[PAD_VIBRATION_MOTOR_VARIABLE];
    for (i = 0; i < ARRAY_SIZE(pad->vibrationRequests[PAD_VIBRATION_MOTOR_VARIABLE]); i++, request++) {
        if (request->active != PAD_VIBRATION_INACTIVE) {
            if (--request->pollsRemaining == 0) {
                request->active = PAD_VIBRATION_INACTIVE;
            }
            if (motor[1] < request->intensity) {
                motor[1] = request->intensity;
            }
        }
    }

    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.vibration == 0) {
        pad->actuatorCommand[0] = motor[0];
        pad->actuatorCommand[1] = motor[1];
    } else {
        pad->actuatorCommand[0] = 0;
        pad->actuatorCommand[1] = 0;
    }

    SCRATCH_STACK_RELEASE_BYTES(4);
}

void Pad_PollControllers(void)
{
    _PadPollWork  workStorage;
    _PadPollWork* work;
    PadState*     pad;
    s16*          axis;
    s16           status;
    s32           portId;
    s32           delta;
    s32           i;
    s32           modeRequested;
    s32           port;
    PadRawPort*   raw;
    u32           state;
    u32           mode;
    u32           savedState;
    u8*           rawAxis;
    u8            center;

    // The work block is always addressed through this pointer; direct member access generates different code.
    work = &workStorage;
    port = 0;
    do {
        portId       = port * 0x10;
        pad          = &gPadStates[port];
        work->portId = portId;
        state        = PadGetState(portId);
        work->state  = state;
        switch (state) {
            case PadStateReqInfo:
                break;
            case PadStateDiscon:
                pad->modeSetupPending = 1;

            case PadStateFindPad:
                pad->actuatorAlignmentReady = 0;
                break;
            default:
            case PadStateFindCTP1:
            case PadStateFindCTP2:
            case PadStateExecCmd:
            case PadStateStable:
                modeRequested = 0;
                if ((pad->modeSetupPending == 1) && ((PadInfoMode(work->portId, InfoModeCurExID, 0) == 0) || (modeRequested = 1, (PadSetMainMode(work->portId, 1, 0) != 0)))) {
                    pad->modeSetupPending = 0;
                }
                Pad_TickEventBanks(pad);
                if (pad->actuatorAlignmentReady == 0) {
                    savedState = work->state;
                    if (savedState == PadStateFindCTP1) {
                        // Libpad retains this buffer; encode the legacy command after registering it.
                        PadSetAct(work->portId, pad->actuatorCommand, sizeof(pad->actuatorCommand));
                        if (pad->actuatorCommand[0] != 0) {
                            pad->actuatorCommand[1] = 1;
                        } else {
                            pad->actuatorCommand[1] = 0;
                        }
                        pad->actuatorCommand[0] = PAD_LEGACY_VIBRATION_PREFIX;
                    } else if ((savedState == PadStateStable) && (modeRequested == 0)) {
                        PadSetAct(work->portId, pad->actuatorCommand, sizeof(pad->actuatorCommand));
                        if (PadSetActAlign(work->portId, D_8005ED84) != 0) {
                            pad->actuatorAlignmentReady = 1;
                        }
                    }
                }
                break;
        }
        mode = PadInfoMode(work->portId, InfoModeCurID, 0);
        switch (mode) {
            case 5:
            case 7:
                if (pad->inputFormat != PAD_INPUT_FORMAT_ANALOG) {
                    *(u32*)pad->stickCenters = PAD_STICK_CENTER_WORD;
                    for (i = 0; i < ARRAY_SIZE(pad->stickCenters); i++) {
                        center = pad->stickCenters[i];
                        if (center < 0x1AU) {
                            pad->stickCenters[i] = 0x1A;
                        } else if (center >= 0xE6U) {
                            pad->stickCenters[i] = 0xE5;
                        }
                    }
                    pad->inputFormat = PAD_INPUT_FORMAT_ANALOG;
                }
                // Normalize all four wire-order axes after removing the raw dead zone.
                axis    = pad->stickAxes;
                rawAxis = Pad_RawPorts[port].stickAxes;
                i       = 0;
                do {
                    delta       = *rawAxis - pad->stickCenters[i];
                    work->delta = delta;
                    if ((u32)(delta + PAD_STICK_DEAD_ZONE_RAW) < 2U * PAD_STICK_DEAD_ZONE_RAW + 1) {
                        *axis++ = 0;
                    } else {
                        if (*rawAxis < 2U) {
                            *axis++ = -PAD_STICK_FULL_SCALE;
                        } else if (*rawAxis >= 0xFEU) {
                            *axis++ = PAD_STICK_FULL_SCALE;
                        } else if (work->delta < 0) {
                            work->range = pad->stickCenters[i] - 0x19;
                            work->delta = (-PAD_STICK_DEAD_ZONE_RAW - work->delta) << PAD_STICK_FRACTION_BITS;
                            work->delta = work->delta / work->range;
                            *axis++     = -work->delta;
                        } else {
                            work->range = 0xE6 - pad->stickCenters[i];
                            work->delta = (work->delta - PAD_STICK_DEAD_ZONE_RAW) << PAD_STICK_FRACTION_BITS;
                            work->delta = work->delta / work->range;
                            *axis++     = work->delta;
                        }
                    }
                    i += 1;
                    rawAxis++;
                } while (i < ARRAY_SIZE(pad->stickAxes));
                pad->inputFormat = PAD_INPUT_FORMAT_ANALOG;
                break;
            case 0:
            case 1:
            case 3:
            case 6:
            case 8:
                raw                               = &Pad_RawPorts[port];
                raw->buttonsHigh                  = 0xFF;
                raw->buttonsLow                   = 0xFF;
                status                            = PAD_INPUT_FORMAT_UNAVAILABLE;
                pad->inputFormat                  = status;
                pad->stickAxes[PAD_STICK_LEFT_Y]  = 0;
                pad->stickAxes[PAD_STICK_LEFT_X]  = 0;
                pad->stickAxes[PAD_STICK_RIGHT_Y] = 0;
                pad->stickAxes[PAD_STICK_RIGHT_X] = 0;
                break;
            default:
                pad->inputFormat                  = PAD_INPUT_FORMAT_DIGITAL;
                pad->stickAxes[PAD_STICK_LEFT_Y]  = 0;
                pad->stickAxes[PAD_STICK_LEFT_X]  = 0;
                pad->stickAxes[PAD_STICK_RIGHT_Y] = 0;
                pad->stickAxes[PAD_STICK_RIGHT_X] = 0;
                break;
        }
        port += 1;
    } while (port <= 0);
}

void Pad_UpdatePort0(void)
{
    s32           i;
    DisplayState* ds;
    _PadScratch*  scratch;
    PadState*     pad;
    u16           buttons;
    u16           prev;
    void**        head;

    head    = SCRATCH_HEAD_ADDR;
    i       = 0;
    ds      = &gDisplayState;
    scratch = SCRATCH_PUSH_AT(head, _PadScratch);

    do {
        pad = &gPadStates[i];
        if (pad->inputBlockPolls == 0) {
            scratch->rawButtons.bytes.high = Pad_RawPorts[i].buttonsHigh;
            scratch->rawButtons.bytes.low  = Pad_RawPorts[i].buttonsLow;
            buttons                        = ~scratch->rawButtons.word;
            scratch->buttons               = buttons;

            if (pad->inputFormat == PAD_INPUT_FORMAT_ANALOG) {
                if (pad->stickAxes[PAD_STICK_LEFT_X] < -PAD_STICK_DIRECTION_THRESHOLD) {
                    scratch->buttons = buttons | 0x8000;
                }
                if (pad->stickAxes[PAD_STICK_LEFT_X] >= PAD_STICK_DIRECTION_THRESHOLD + 1) {
                    scratch->buttons = scratch->buttons | 0x2000;
                }
                if (pad->stickAxes[PAD_STICK_LEFT_Y] < -PAD_STICK_DIRECTION_THRESHOLD) {
                    scratch->buttons = scratch->buttons | 0x1000;
                }
                if (pad->stickAxes[PAD_STICK_LEFT_Y] >= PAD_STICK_DIRECTION_THRESHOLD + 1) {
                    scratch->buttons = scratch->buttons | 0x4000;
                }
            }

            if (i == 0) {
                if (Pad_RemapState->inputOverrideMode != GAME_DEBUG_INPUT_OVERRIDE_NONE) {
                    if (ds->displayOwner == DISPLAY_OWNER_GAME_LOOP) {
                        pad->stickAxes[PAD_STICK_RIGHT_Y] = 0;
                        pad->stickAxes[PAD_STICK_RIGHT_X] = 0;
                        pad->stickAxes[PAD_STICK_LEFT_Y]  = 0;
                        pad->stickAxes[PAD_STICK_LEFT_X]  = 0;
                        Gp_ApplyPadReplay(Pad_RemapState->inputOverrideMode, &scratch->buttons);
                    }
                }
            }

            // Derive both edge masks before replacing the held-button sample.
            prev                 = pad->buttons;
            buttons              = scratch->buttons;
            scratch->prevButtons = prev;
            pad->pressedButtons  = buttons & (buttons ^ prev);
            pad->releasedButtons = scratch->prevButtons & (scratch->buttons ^ scratch->prevButtons);
            pad->buttons         = scratch->buttons;

            if ((s8)gGameSession->uiOpen != 0) {
                if ((scratch->prevButtons & PAD_DIRECTION_BUTTON_MASK) == (scratch->buttons & PAD_DIRECTION_BUTTON_MASK)) {
                    pad->directionRepeatTicks = pad->directionRepeatTicks + ds->frameTicks;
                } else {
                    pad->directionRepeatTicks = 0;
                }
                if (pad->directionRepeatTicks >= PAD_DIRECTION_REPEAT_DELAY_TICKS) {
                    pad->directionRepeatTicks = PAD_DIRECTION_REPEAT_RESTART_TICKS;
                    pad->pressedButtons       = pad->pressedButtons | (pad->buttons & PAD_DIRECTION_BUTTON_MASK);
                }
            }
        } else {
            pad->inputBlockPolls--;
            pad->pressedButtons  = 0;
            pad->releasedButtons = 0;
            pad->buttons         = 0;
            if (pad->inputBlockPolls == 0) {
                scratch->rawButtons.bytes.high = Pad_RawPorts[i].buttonsHigh;
                scratch->rawButtons.bytes.low  = Pad_RawPorts[i].buttonsLow;
                buttons                        = ~scratch->rawButtons.word;
                scratch->buttons               = buttons;
                pad->buttons                   = buttons;
            }
        }
        i++;
    } while (i <= 0);

    SCRATCH_STACK_RELEASE_BLOCK(_PadScratch);
}
