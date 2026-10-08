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

#include "gameplay/companion_load.h"
#include "gameplay/game_debug.h"

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
    u16           buttons;     // Buttons held this update, active high; becomes `PadState::buttons`
    u16           prevButtons; // `PadState::buttons` as the preceding update left it
    PadRawButtons rawButtons;  // Controller's button bytes as received, active low
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

static void _gameFlowResetNewSession(Task* task);

static void GameFlow_SpawnMenu(Task* task);

static void _gameFlowWaitForLoadDialog(Task* task);

static void _gameFlowWaitAfterLoadDialog(Task* task);

static void GameFlow_SpawnMainWhenReady(Task* task);

static void _gameFlowRestoreSavedLocation(Task* task);

static void GameFlow_EnqueueDefaultLoad(Task* task);

static void GameFlow_SpawnWhenIdle(Task* task);

static void _padTickVibrationRequests(PadState* pad);

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
    _gameFlowResetNewSession,
    GameFlow_SpawnMenu,
    _gameFlowWaitForLoadDialog,
    _gameFlowWaitAfterLoadDialog,
    GameFlow_SpawnMainWhenReady,
} };

static const TaskFuncTable3 GameFlow_States3 = { {
    _gameFlowRestoreSavedLocation,
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
            padStartInputBlock(0);
            if (gDisplayState.demoScene == DISPLAY_DEMO_NONE) {
                gDisplayState.demoScene = 1;
            }
            if (gDisplayState.demoScene < DISPLAY_DEMO_FIXED_REPLAY) {
                titleEnqueueAttractDemoFile(gDisplayState.demoScene - 1);
            }
            task->state = task->state + 1;
        }
        if (cdCmdIsIdle() != 0) {
            if (gDisplayState.spriteVariant == 0) {
                gDisplayState.spriteVariant = 1;
            }
            Title_RestoreDemoCard();
            MEM_CLEAR(gGameSession, sizeof(*gGameSession));
            gDisplayState.control.flags.pendingPlayerPos = 0;
            gDisplayState.gameRunning                    = 0;
            gGameSession->applySaveVariant               = 1;
            gGameSession->field_80                       = 0;
            sndVolumeSetReducedMode(1);
            gDisplayState.control.flags.pendingPlayerPos = 0;
            gDisplayState.stopTaskWalk                   = 1;
            taskKill(task);
            taskResetDefaultList();
            actorRenderResetLists();
            memInitHeaps();
            taskSpawn(0, 9, 0, 0);
        }
    } else {
        gDisplayState.demoScene = DISPLAY_DEMO_NONE;
        padStartInputBlock(0);
        if (task->spawnArg1.value == 0) {
            saved = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.vibration;
            MEM_CLEAR(gGameSession, sizeof(*gGameSession));
            gDisplayState.control.flags.pendingPlayerPos = 0;
            gDisplayState.gameRunning                    = 1;
            p->releasePauseBlockAfterFade                = 1;
            p->blockGamePause                            = 1;
            Wip_SysFlags.skipTitleIntro                  = 1;
            mcResetSaveData();
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
        actorRenderResetLists();
        memInitHeaps();
        taskSpawn(0, 9, 0, 0);
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

void gameClearSession(void)
{
    MEM_CLEAR(gGameSession, sizeof(*gGameSession));
    gDisplayState.control.flags.pendingPlayerPos = 0;
}

static void GameFlow_InitSystems(void)
{
    taskResetDefaultList();
    actorRenderResetLists();
    memInitHeaps();
    taskSpawn(0, 9, 0, 0);
}

/// Resets live session/save progress for the load-dialog path, preserving vibration.
///
/// The flow task must be in state 0. Arms the pause block until the loading fade
/// releases it, skips the title intro and advances to dialog creation. Existing
/// session handles are discarded without teardown; the task/heap reset is later.
static void _gameFlowResetNewSession(Task* task)
{
    s32         savedVibration;
    CdCmdQueue* cdQueue;

    cdQueue        = &gCdCmdQueue;
    savedVibration = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.vibration;
    MEM_CLEAR(gGameSession, sizeof(*gGameSession));
    gDisplayState.control.flags.pendingPlayerPos = 0;
    gDisplayState.gameRunning                    = 1;
    cdQueue->releasePauseBlockAfterFade          = 1;
    cdQueue->blockGamePause                      = 1;
    Wip_SysFlags.skipTitleIntro                  = 1;
    // Reset progress and option defaults, then retain the previous vibration choice.
    mcResetSaveData();
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.vibration = savedVibration;
    task->state                                        = task->state + 1;
}

static void GameFlow_SpawnMenu(Task* task)
{
    void* temp_v0;

    displaySetFrameTiming(DISPLAY_TIMING_EVERY_VBLANK);
    temp_v0                 = uiSpawnObject(Mc_TaskDescriptors, 0, 1, 0, 0);
    task->spawnArg2.pointer = temp_v0;
    if (temp_v0 != 0) {
        gDisplayState.gameMode = DISPLAY_GAME_MODAL;
        gGameSession->uiOpen   = 1;
        task->killCountdown    = 0x10;
        task->state            = task->state + 1;
    }
}

/// Closes the load dialog on its closure result and applies the live audio options.
///
/// State 2 borrows the live `UiObject` stored by dialog creation in `spawnArg2`.
/// The dialog publishes CANCEL to request closure regardless of load success.
/// Starts a twelve-callback closing delay; the UI owns the object's teardown.
static void _gameFlowWaitForLoadDialog(Task* task)
{
    enum {
        MEMORY_CARD_OPTION_SOUND_MONO    = 1,
        GAME_FLOW_DIALOG_CLOSE_CALLBACKS = 12,
    };
    UiObject* loadDialog;

    loadDialog = task->spawnArg2.pointer;
    if (loadDialog->result == USER_INTERFACE_RESULT_CANCEL) {
        uiStartTreeClosing(loadDialog, loadDialog->owner);
        gDisplayState.gameMode = DISPLAY_GAME_ACTIVE;
        gGameSession->uiOpen   = 0;
        if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.soundMode == MEMORY_CARD_OPTION_SOUND_MONO) {
            sndOutputSetStereo(SOUND_OUTPUT_MONO);
        } else {
            sndOutputSetStereo(SOUND_OUTPUT_STEREO);
        }
        midiApplyMusicVolume(MIDI_MUSIC_VOLUME_SAVED);
        task->killCountdown = GAME_FLOW_DIALOG_CLOSE_CALLBACKS;
        task->state         = task->state + 1;
    }
}

/// Counts one dialog-closing callback and advances only on the zero transition.
///
/// State 3 uses the signed halfword `killCountdown` seeded by dialog completion.
/// Decrement/narrowing happens before the test; zero or negative inputs wrap
/// rather than completing immediately. Expiry renews port 0's input block.
static void _gameFlowWaitAfterLoadDialog(Task* task)
{
    task->killCountdown--;
    if (task->killCountdown != 0) {
        return;
    }
    padStartInputBlock(0);
    task->state = task->state + 1;
}

static void GameFlow_SpawnMainWhenReady(Task* task)
{
    if (gDisplayState.control.flags.pendingPlayerPos == 0) {
        taskSpawn(0, 2, 0, 0);
        displayConfigureFramebuffers(DISPLAY_SETUP_DEFAULT | DISPLAY_SETUP_NO_CLEAR);
        taskKill(task);
        return;
    }
    gDisplayState.stopTaskWalk = 1;
    taskKill(task);
    taskResetDefaultList();
    actorRenderResetLists();
    memInitHeaps();
    taskSpawn(0, 9, 0, 0);
}

void GameFlow_DispatchTable5(Task* task)
{
    TaskFuncTable5 sp;

    sp = GameFlow_States5;
    sp.funcs[task->state](task);
}

/// Restores the full live-save location cell and restarts the required-disc check.
///
/// State 0 copies all eight bytes, including the location cell's two unknown
/// trailing bytes, then advances to disk checking and the initial file load.
static void _gameFlowRestoreSavedLocation(Task* task)
{
    enum { LOAD_UI_DISK_SWAP_INITIAL = 0 };

    gGameSession->location = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location;
    D_8007A394             = LOAD_UI_DISK_SWAP_INITIAL;
    task->state            = task->state + 1;
}

static void GameFlow_EnqueueDefaultLoad(Task* task)
{
    u8 param1[8];
    u8 param2[8];

    if ((u8)LoadUi_PollDiskSwap() == 0) {
        gameFlowBeginLoadScreen(&gGameSession->location.loc, GAME_FLOW_LOAD_CAPTION_NORMAL);
        param1[3] = 0;
        param1[2] = 0;
        param1[0] = 0;
        param2[0] = 0;
        param2[1] = 0;
        param2[2] = 0;
        param2[3] = 0;
        cdCmdEnqueue(CD_COMMAND_LOAD_FILE, param1, param2);
        task->state = task->state + 1;
    }
}

void playClockResetMinuteTicks(void)
{
    D_8005ED68 = 0;
}

static void GameFlow_SpawnWhenIdle(Task* task)
{
    if (cdCmdIsIdle() != 0) {
        taskSpawn(GAME_FLOW_RELOAD_TASK_BANK, GAME_FLOW_RELOAD_TASK_SLOT, GAME_FLOW_RELOAD_BLANK_DISPLAY, 0);
        taskKill(task);
    }
}

void gameFlowStartSessionTask(Task* task)
{
    TaskFuncTable3 states;

    states = GameFlow_States3;
    padStartInputBlock(0);
    states.funcs[task->state](task);
}

/// Counts one serviced poll of an active vibration request and expires it at zero.
///
/// The caller checks `active` before entering and mixes the final contribution
/// even if expiry clears it here. The signed halfword countdown wraps on storage.
static inline void _padAdvanceVibrationRequest(PadVibrationRequest* request)
{
    if (--request->pollsRemaining == 0) {
        request->active = PAD_VIBRATION_INACTIVE;
    }
}

/// Advances and mixes one serviced poll of a port's two vibration-request banks.
///
/// `pad` borrows writable resident controller state. Active requests decrement
/// their signed halfword countdown with wrap, expire at zero and still contribute
/// on that poll. Binary drive mixes by OR, variable drive by maximum. The saved
/// vibration preference suppresses output after all requests have advanced.
/// Writes the aligned motor pair; the caller applies legacy protocol encoding.
static void _padTickVibrationRequests(PadState* pad)
{
    enum {
        PAD_VIBRATION_MIX_STACK_BYTES   = sizeof(u32),
        MEMORY_CARD_OPTION_VIBRATION_ON = 0,
    };
    u8*                  motorDrive;
    PadVibrationRequest* request;
    s32                  requestIndex;

    // Two drive bytes use a word-sized reservation to preserve scratch alignment.
    SCRATCH_STACK_RESERVE_BYTES(PAD_VIBRATION_MIX_STACK_BYTES);
    motorDrive                               = SCRATCH_STACK_CURSOR(u8);
    motorDrive[PAD_VIBRATION_MOTOR_VARIABLE] = 0;
    motorDrive[PAD_VIBRATION_MOTOR_BINARY]   = 0;

    // Binary requests combine by logical OR, including each request's expiry poll.
    request = pad->vibrationRequests[PAD_VIBRATION_MOTOR_BINARY];
    for (requestIndex = 0; requestIndex < ARRAY_SIZE(pad->vibrationRequests[PAD_VIBRATION_MOTOR_BINARY]); requestIndex++, request++) {
        if (request->active != PAD_VIBRATION_INACTIVE) {
            _padAdvanceVibrationRequest(request);
            if (request->intensity != 0) {
                motorDrive[PAD_VIBRATION_MOTOR_BINARY] = PAD_VIBRATION_BINARY_ON;
            }
        }
    }

    // Variable motor requests combine by maximum intensity.
    request = pad->vibrationRequests[PAD_VIBRATION_MOTOR_VARIABLE];
    for (requestIndex = 0; requestIndex < ARRAY_SIZE(pad->vibrationRequests[PAD_VIBRATION_MOTOR_VARIABLE]); requestIndex++, request++) {
        if (request->active != PAD_VIBRATION_INACTIVE) {
            _padAdvanceVibrationRequest(request);
            if (motorDrive[PAD_VIBRATION_MOTOR_VARIABLE] < request->intensity) {
                motorDrive[PAD_VIBRATION_MOTOR_VARIABLE] = request->intensity;
            }
        }
    }

    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.vibration == MEMORY_CARD_OPTION_VIBRATION_ON) {
        pad->actuatorCommand[PAD_VIBRATION_MOTOR_BINARY]   = motorDrive[PAD_VIBRATION_MOTOR_BINARY];
        pad->actuatorCommand[PAD_VIBRATION_MOTOR_VARIABLE] = motorDrive[PAD_VIBRATION_MOTOR_VARIABLE];
    } else {
        pad->actuatorCommand[PAD_VIBRATION_MOTOR_BINARY]   = 0;
        pad->actuatorCommand[PAD_VIBRATION_MOTOR_VARIABLE] = 0;
    }

    SCRATCH_STACK_RELEASE_BYTES(PAD_VIBRATION_MIX_STACK_BYTES);
}

void padPollPort0(void)
{
    enum {
        PAD_POLLED_PORT_COUNT             = 1,
        PAD_LIBPAD_PORT_STRIDE            = 0x10,
        PAD_MODE_ID_NONE                  = 0,
        PAD_MODE_ID_MOUSE                 = PAD_INPUT_FORMAT_MOUSE >> 4,
        PAD_MODE_ID_KONAMI_GUN            = 3,
        PAD_MODE_ID_ANALOG_JOYSTICK       = 5,
        PAD_MODE_ID_NAMCO_GUN             = 6,
        PAD_MODE_ID_ANALOG_CONTROLLER     = PAD_INPUT_FORMAT_ANALOG >> 4,
        PAD_MODE_ID_MULTITAP              = 8,
        PAD_ANALOG_MODE_TABLE_INDEX       = 1,
        PAD_MODE_KEEP_SELECTOR_LOCK       = 0,
        PAD_RAW_BUTTON_BYTE_RELEASED      = 0xFF,
        PAD_STICK_RAW_NEGATIVE_FULL_SCALE = 1,
        PAD_STICK_RAW_POSITIVE_FULL_SCALE = 0xFE,
        PAD_STICK_CENTER_MIN_RAW          = PAD_STICK_RAW_NEGATIVE_FULL_SCALE + PAD_STICK_DEAD_ZONE_RAW + 1,
        PAD_STICK_CENTER_MAX_RAW          = PAD_STICK_RAW_POSITIVE_FULL_SCALE - PAD_STICK_DEAD_ZONE_RAW - 1,
    };
    _PadPollWork  workStorage;
    _PadPollWork* work;
    PadState*     pad;
    s16*          normalizedAxis;
    s16           inputFormat;
    s32           libpadPort;
    s32           rawDelta;
    s32           axisIndex;
    s32           modeRequestAttempted;
    s32           port;
    PadRawPort*   rawPort;
    u32           connectionState;
    u32           modeId;
    u32           actuatorState;
    const u8*     rawAxis;
    u8            center;

    // The work block is always addressed through this pointer; direct member access generates different code.
    work = &workStorage;
    port = 0;
    do {
        libpadPort      = port * PAD_LIBPAD_PORT_STRIDE;
        pad             = &gPadStates[port];
        work->portId    = libpadPort;
        connectionState = PadGetState(libpadPort);
        work->state     = connectionState;
        switch (connectionState) {
            case PadStateReqInfo:
                break;
            case PadStateDiscon:
                pad->modeSetupPending = 1;
                // Fall through: a disconnected port also loses actuator alignment.

            case PadStateFindPad:
                pad->actuatorAlignmentReady = 0;
                break;
            default:
            case PadStateFindCTP1:
            case PadStateFindCTP2:
            case PadStateExecCmd:
            case PadStateStable:
                // A mode request and actuator alignment cannot be submitted together.
                modeRequestAttempted = 0;
                if ((pad->modeSetupPending == 1) && ((PadInfoMode(work->portId, InfoModeCurExID, 0) == 0) || (modeRequestAttempted = 1, (PadSetMainMode(work->portId, PAD_ANALOG_MODE_TABLE_INDEX, PAD_MODE_KEEP_SELECTOR_LOCK) != 0)))) {
                    pad->modeSetupPending = 0;
                }
                _padTickVibrationRequests(pad);
                if (pad->actuatorAlignmentReady == 0) {
                    actuatorState = work->state;
                    if (actuatorState == PadStateFindCTP1) {
                        // Libpad retains this buffer; encode the legacy command after registering it.
                        PadSetAct(work->portId, pad->actuatorCommand, sizeof(pad->actuatorCommand));
                        if (pad->actuatorCommand[0] != 0) {
                            pad->actuatorCommand[1] = PAD_VIBRATION_BINARY_ON;
                        } else {
                            pad->actuatorCommand[1] = 0;
                        }
                        pad->actuatorCommand[0] = PAD_LEGACY_VIBRATION_PREFIX;
                    } else if ((actuatorState == PadStateStable) && (modeRequestAttempted == 0)) {
                        PadSetAct(work->portId, pad->actuatorCommand, sizeof(pad->actuatorCommand));
                        if (PadSetActAlign(work->portId, D_8005ED84) != 0) {
                            pad->actuatorAlignmentReady = 1;
                        }
                    }
                }
                break;
        }
        modeId = PadInfoMode(work->portId, InfoModeCurID, 0);
        switch (modeId) {
            case PAD_MODE_ID_ANALOG_JOYSTICK:
            case PAD_MODE_ID_ANALOG_CONTROLLER:
                if (pad->inputFormat != PAD_INPUT_FORMAT_ANALOG) {
                    // Resident port entries are word-aligned; seed all four centers together.
                    *(u32*)pad->stickCenters = PAD_STICK_CENTER_WORD;
                    for (axisIndex = 0; axisIndex < ARRAY_SIZE(pad->stickCenters); axisIndex++) {
                        center = pad->stickCenters[axisIndex];
                        if (center < PAD_STICK_CENTER_MIN_RAW) {
                            pad->stickCenters[axisIndex] = PAD_STICK_CENTER_MIN_RAW;
                        } else if (center >= PAD_STICK_CENTER_MAX_RAW + 1U) {
                            pad->stickCenters[axisIndex] = PAD_STICK_CENTER_MAX_RAW;
                        }
                    }
                    pad->inputFormat = PAD_INPUT_FORMAT_ANALOG;
                }
                // Normalize all four wire-order axes after removing the raw dead zone.
                normalizedAxis = pad->stickAxes;
                rawAxis        = Pad_RawPorts[port].stickAxes;
                axisIndex      = 0;
                do {
                    rawDelta    = *rawAxis - pad->stickCenters[axisIndex];
                    work->delta = rawDelta;
                    if ((u32)(rawDelta + PAD_STICK_DEAD_ZONE_RAW) < 2U * PAD_STICK_DEAD_ZONE_RAW + 1) {
                        *normalizedAxis++ = 0;
                    } else {
                        if (*rawAxis < PAD_STICK_RAW_NEGATIVE_FULL_SCALE + 1U) {
                            *normalizedAxis++ = -PAD_STICK_FULL_SCALE;
                        } else if (*rawAxis >= PAD_STICK_RAW_POSITIVE_FULL_SCALE) {
                            *normalizedAxis++ = PAD_STICK_FULL_SCALE;
                        } else if (work->delta < 0) {
                            work->range       = pad->stickCenters[axisIndex] - (PAD_STICK_RAW_NEGATIVE_FULL_SCALE + PAD_STICK_DEAD_ZONE_RAW);
                            work->delta       = (-PAD_STICK_DEAD_ZONE_RAW - work->delta) << PAD_STICK_FRACTION_BITS;
                            work->delta       = work->delta / work->range;
                            *normalizedAxis++ = -work->delta;
                        } else {
                            work->range       = (PAD_STICK_RAW_POSITIVE_FULL_SCALE - PAD_STICK_DEAD_ZONE_RAW) - pad->stickCenters[axisIndex];
                            work->delta       = (work->delta - PAD_STICK_DEAD_ZONE_RAW) << PAD_STICK_FRACTION_BITS;
                            work->delta       = work->delta / work->range;
                            *normalizedAxis++ = work->delta;
                        }
                    }
                    axisIndex += 1;
                    rawAxis++;
                } while (axisIndex < ARRAY_SIZE(pad->stickAxes));
                pad->inputFormat = PAD_INPUT_FORMAT_ANALOG;
                break;
            case PAD_MODE_ID_NONE:
            case PAD_MODE_ID_MOUSE:
            case PAD_MODE_ID_KONAMI_GUN:
            case PAD_MODE_ID_NAMCO_GUN:
            case PAD_MODE_ID_MULTITAP:
                rawPort                           = &Pad_RawPorts[port];
                rawPort->buttonsHigh              = PAD_RAW_BUTTON_BYTE_RELEASED;
                rawPort->buttonsLow               = PAD_RAW_BUTTON_BYTE_RELEASED;
                inputFormat                       = PAD_INPUT_FORMAT_UNAVAILABLE;
                pad->inputFormat                  = inputFormat;
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
    } while (port < PAD_POLLED_PORT_COUNT);
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
                        gameDebugApplyInputOverride(Pad_RemapState->inputOverrideMode, &scratch->buttons);
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
