#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/abs.h>

#include "common.h"

#include "gameplay/captions.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/sound.h"
#include "captions.h"
#include "gameplay/display.h"
#include "gameplay/evs.h"
#include "gameplay/message.h"
#include "gameplay/pad_script.h"
#include "gameplay/player_actor.h"
#include "gameplay/player_state.h"
#include "player_state.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_coords.h"
#include "world_coords.h"

#include "main/display.h"
#include "main/fs.h"
#include "main/random.h"
#include "main/loadui.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/stage.h"
#include "main/task.h"
#include "main/text.h"
#include "main/tmd.h"
#include "main/wipsys.h"

/// Request of the event-script fade of the music volume.
///
/// `EVENT_SCRIPT_OPCODE_FADE_VOLUME` fills the record and hands it to the fade
/// task as `Task::spawnArg2`. The task steps the music level linearly from the
/// level in force when the fade begins to `targetVolume`, re-applying it on
/// every update; a zero duration applies the target once.
///
/// Levels are sequence gains as the music volume command takes them (127 full).
/// Zero is not silence: applying a level of zero selects the level of the
/// options menu's music-volume setting instead, so a zero target restores the
/// player's setting, and a fade towards it jumps there on the first step that
/// rounds down to zero. The interpreter has a single record, and a new fade
/// replaces the one still running.
typedef struct {
    u16 targetVolume;   // Music level the fade ends on (0 restores the options-menu level)
    u16 durationFrames; // Task updates the fade lasts (0 applies the target immediately)
} _EvsMusicVolumeFade;
STATIC_ASSERT_SIZEOF(_EvsMusicVolumeFade, 4);

/// Request and running level of the event-script fade of one sound's attenuation.
///
/// `EVENT_SCRIPT_OPCODE_FADE_SOUND_ATTENUATION` fills the sound, target and
/// duration, then hands the record to the fade task as `Task::spawnArg2`. The
/// task steps `attenuation` linearly from its value when the fade begins to
/// `targetAttenuation`, re-sending the sound's attenuation with an unchanged pan
/// on every update; a zero duration sends the target once.
///
/// The interpreter has a single record. `attenuation` is therefore the level of
/// whichever sound a script most recently started or faded, not of `soundId`:
/// the two sound-start opcodes store their attenuation operand in it, so a fade
/// continues from the level the sound was started at only when no other sound
/// start came between. Levels are kept and interpolated as unsigned halfwords,
/// and each update sends the low byte as the signed attenuation the sound
/// events take (0 full, magnitude 127 silent).
typedef struct {
    s32 soundId;           // Sound-script request id whose attenuation is faded
    u16 attenuation;       // Level last sent by a script sound start or fade step; the next fade starts here
    u16 targetAttenuation; // Level the fade ends on
    u16 durationFrames;    // Task updates the fade lasts (0 applies the target immediately)
} _EvsSoundAttenuationFade;
STATIC_ASSERT_SIZEOF(_EvsSoundAttenuationFade, 0xC);

/// Work block of the event-script interpreter task, held in `Task::work`.
///
/// The task runs one script. `command` always names the instruction dispatched
/// next: an opcode that yields advances it before returning, and an accepted
/// skip replaces it with the script's skip target and clears `waitFrames`.
/// The commands are borrowed from the script's owner for the task's life.
///
/// A script call pushes the instruction after it on `returnStack`, and a
/// return pops it. Neither is range-checked, so a script keeps at most
/// `ARRAY_SIZE(returnStack)` calls pending and returns only from a call; a
/// skip leaves the pending returns in place.
///
/// The two effect handles let a script hold one full-screen effect across the
/// start and end of another. Neither is cleared when its task ends by itself.
/// The primary handle is cleared only by the cancel opcode, which kills
/// whatever task it names, and a primary fade is not started while the handle
/// is set; a flash overwrites it without ending the earlier task. The secondary
/// fade is killed only while its `ScreenFade` record is not done, so its handle
/// may outlive the task, and a new secondary fade may start once it is done.
typedef struct {
    EvsCommand* command;           // Instruction dispatched next
    s32         waitFrames;        // Interpreter updates left before dispatch resumes (0 not waiting)
    EvsCommand* returnStack[8];    // Instructions the pending script calls return to, oldest first
    s32         returnDepth;       // Entries of `returnStack` in use
    Task*       primaryEffectTask; // Screen flash or primary screen fade the script started (NULL none, or cancelled)
    Task*       secondaryFadeTask; // Secondary screen fade the script started (NULL none, or cancelled)
} _EvsInterpreterWork;
STATIC_ASSERT_SIZEOF(_EvsInterpreterWork, 0x34);

/// Requested 16-bit music level captured when a timed fade initializes.
///
/// Snapshots `gMidiMusicVolume`, including override bits; it is not an audible
/// gain measurement. The snapshot remains fixed through the fade's updates.
static u16 _gEvsMusicVolumeFadeStartLevel;

/// Updates elapsed in the timed music fade, excluding its initialization update.
///
/// Reset to zero on initialization, incremented before interpolation, and
/// compared with the request's unsigned 16-bit duration to end the task.
static u16 _gEvsMusicVolumeFadeElapsedUpdates;

/// Unsigned attenuation captured from the shared sound record at fade initialization.
///
/// This is the last scripted sound-start or fade level, regardless of sound ID;
/// sound requests consume its low signed byte (0 full, magnitude 127 silent).
static u16 _gEvsSoundAttenuationFadeStartLevel;

/// Updates elapsed in the timed sound fade, excluding its initialization update.
///
/// Reset to zero on initialization and incremented before interpolation;
/// reaching the unsigned 16-bit requested duration ends the task.
static u16 _gEvsSoundAttenuationFadeElapsedUpdates;

/// Whether this interpreter has issued its one area-music start request (0 no, 1 yes).
///
/// Cleared at interpreter initialization. Stopping area music does not clear it.
static u8 _gEvsAreaMusicStarted;

/// Whether this event has issued its one scene-music load request (0 no, 1 yes).
///
/// Cleared when launching an event; subsequent requests are ignored even after
/// the resident music loader finishes.
static u8 _gEvsSceneMusicRequested;

/// Whether any non-NULL scene key was selected during this interpreter (0 no, 1 yes).
///
/// Cleared at initialization and retained after a later NULL selection so END
/// still finishes scene streaming and restores its saved random state.
static u8 _gEvsSceneWasSelected;

/// Byte set to one at interpreter initialization and command advances; role unproven.
static u8 D_801156CB;

/// Input-skip sound policy (0 stop non-ambient scripts, nonzero keep them running).
///
/// Cleared when launching an event; the script sets it from its operand's low byte.
static u8 _gEvsSkipKeepsSound;

/// Whether the script has requested player equipment removal pending restoration.
///
/// Zero at event launch, one on HIDE_WEAPONS, zero after RESTORE_WEAPONS.
static u8 _gEvsPlayerEquipmentRestorePending;

/// Whether the script has requested companion equipment removal pending restoration.
///
/// Zero at event launch, one on HIDE_WEAPONS even without a companion task,
/// zero after RESTORE_WEAPONS. Restoration requires a live companion task.
static u8 _gEvsCompanionEquipmentRestorePending;

/// Borrowed command stream consumed by the next accepted input skip (NULL disabled).
///
/// Event launch installs the optional skip script and SET_SKIP_TARGET replaces
/// it. Keep its commands and referenced resources loaded through interpretation.
/// Accepting a skip clears this pointer before executing that stream next update.
static EvsCommand* _gEvsSkipTarget;

/// Control record for the interpreter's primary full-screen fade.
///
/// START_PRIMARY_FADE borrows this record for the display task's life, using
/// seven ramp frames for a zero duration operand. RETURN_PRIMARY_FADE requests
/// ramp-down; the work block's primary handle also holds screen flashes.
static ScreenFade _gEvsPrimaryFade;

/// Control record for the interpreter's independently held secondary screen fade.
///
/// The script supplies its ordering-table offset and selects seven ramp frames
/// for a zero start duration. Return may replace the signed 16-bit ramp length;
/// completion marks the record done so cancellation avoids the stale task handle.
static ScreenFade _gEvsSecondaryFade;

/// Singleton music-fade request borrowed by the active EVS volume-fade task.
///
/// A new request kills the previous task before overwriting its target and
/// duration. Both operands narrow to unsigned 16 bits; zero duration applies
/// the target once, and target zero selects the saved music-volume option.
static _EvsMusicVolumeFade _gEvsMusicVolumeFade;

/// Singleton sound-fade request and last level sent by scripted starts or fades.
///
/// A new fade kills the previous task before replacing sound ID, target and
/// duration; it keeps the most recent level even if it belonged to another sound.
/// Attenuation and duration narrow to unsigned 16 bits. The task borrows this
/// record and sends attenuation's low signed byte with unchanged pan.
static _EvsSoundAttenuationFade _gEvsSoundAttenuationFade;

/// Player weapon index captured at event launch for later equipment restoration.
///
/// Copies `PlayerStatus::weapon` (0 none, 1..32 weapon item ID minus 0x7F).
/// The snapshot occupies a word and is unchanged by later hide/restore commands.
static s32 _gEvsSavedPlayerWeapon;

/// Unpaused interpreter updates remaining before input skipping is eligible.
///
/// Event launch sets five; execution decrements after checking input, including
/// frame/CAP waits. The shared playback pause suspends this byte countdown.
static u8 _gEvsSkipDelayUpdates;

/* Define BSS before API headers to preserve first-declaration order. */

const EvsSceneKey* gEvsSelectedSceneKey;

/// Saved 1-based room view slot restored by RESTORE_VIEW.
///
/// Event launch and SAVE_VIEW capture the live save's location view; this is
/// the room's view slot, before any mapping to a camera or background image.
u8 _gEvsSavedView;

u8 gEvsPlaybackPaused;

#include "gameplay/evs_scripts.h"

#include "evs_scripts.h"

static void _evsExecuteInterpreterTask(Task* task);

static void _evsInitializeInterpreterTask(Task* task);

/// Previous-framebuffer blend task retained by the event interpreter (NULL none).
///
/// SET_FRAMEBUFFER_BLEND starts it; disabling, scene cleanup and END request
/// its exit and forget it. Event launch only clears the handle. Repeated enable
/// commands overwrite the handle without ending the earlier task.
static Task* _gEvsFramebufferBlendTask = NULL;

/// Active singleton event-script music-volume fade task (NULL none).
///
/// A new fade kills and replaces it; fade completion clears it. Event launch
/// clears the handle without killing an earlier fade, so events must not overlap.
static Task* _gEvsMusicVolumeFadeTask = NULL;

/// Active singleton event-script sound-attenuation fade task (NULL none).
///
/// A new fade kills and replaces it; fade completion clears it. The handle is
/// retained across event launch and scene cleanup until completion or replacement.
static Task* _gEvsSoundAttenuationFadeTask = NULL;

/// Interpreter state handlers (0 initialize, 1 execute, 2 release the task).
///
/// Copied as a three-entry value before unchecked dispatch through `Task::state`.
static const TaskFuncTable3 _gEvsInterpreterStates = { {
    _evsInitializeInterpreterTask,
    _evsExecuteInterpreterTask,
    taskKill,
} };

/// Byte-encoded demo status label while the interpreter counts a frame wait.
static const u8 _gEvsDemoWaitLabel[] = "Demo Wait";

/// Byte-encoded demo status label while the interpreter waits for a CAP cue.
static const u8 _gEvsDemoPauseLabel[] = "Demo Pause";

// Script light values gain four fractional bits before their s16 conversion.
enum { EVENT_SCRIPT_LIGHT_VALUE_SCALE = 16 };

/// Cancels the interpreter's secondary screen fade and clears its borrowed handle.
///
/// `work` must be live interpreter work. A done phase means the fade task has
/// released itself; clear that stale handle without dereferencing it.
static inline void _evsCancelSecondaryFade(_EvsInterpreterWork* work)
{
    if (work->secondaryFadeTask != NULL) {
        if (_gEvsSecondaryFade.phase != SCREEN_FADE_DONE) {
            taskKill(work->secondaryFadeTask);
        }
        work->secondaryFadeTask = NULL;
    }
}

/// Draws the event interpreter's demo wait or pause label at its fixed screen position.
///
/// `request` must be a side-effect-free TextDrawReq lvalue; it is evaluated
/// eight times and overwritten. `label` is evaluated once and borrows a
/// zero-terminated byte-encoded string. Requires current frame packet storage.
#define EVENT_SCRIPT_DRAW_DEMO_STATUS(request, label)              \
    {                                                              \
        enum { EVENT_SCRIPT_DEMO_STATUS_X_PIXELS  = -140,          \
               EVENT_SCRIPT_DEMO_STATUS_Y_PIXELS  = 80,            \
               EVENT_SCRIPT_DEMO_STATUS_OT_INDEX  = 4,             \
               EVENT_SCRIPT_DEMO_STATUS_COLOR_RGB = 0x808008 };    \
        (request).x          = EVENT_SCRIPT_DEMO_STATUS_X_PIXELS;  \
        (request).y          = EVENT_SCRIPT_DEMO_STATUS_Y_PIXELS;  \
        (request).otIndex    = EVENT_SCRIPT_DEMO_STATUS_OT_INDEX;  \
        (request).colorRgb   = EVENT_SCRIPT_DEMO_STATUS_COLOR_RGB; \
        (request).glyphTable = TEXT_GLYPH_TABLE_MEDIUM;            \
        (request).alignment  = TEXT_ALIGNMENT_LEFT;                \
        (request).drawMode   = TEXT_DRAW_FILL_ONLY;                \
        textDrawString(&(request), (label));                       \
    }

/// Executes an event script until it ends or yields to a later task update.
///
/// State 1 requires live interpreter work and borrowed command/resource storage.
/// The shared pause latch suspends input handling and countdowns as well as dispatch.
/// Input skip consumes its target, clears frame/CAP waits and yields one update;
/// it retains the return stack. Frame/CAP waits advance before yielding, whereas
/// animation/action/music polls retry the same instruction. Calls require at most
/// eight pending returns and returns require a nonempty stack; targets are unchecked.
/// End releases the display hold and optional HUD suppression before state 2 kills
/// the task. Scene cleanup disables a later automatic HUD restore. Effect handles
/// retain the independent lifetime rules of `_EvsInterpreterWork`.
static void _evsExecuteInterpreterTask(Task* task)
{
    EvsCommand*          skipTarget;
    _EvsInterpreterWork* work;
    AnimationPlayRequest animationRequest;
    SVECTOR              lightColor;
    TextDrawReq          debugTextRequest;
    Task*                recipientTask;
    s32                  equipmentTargets;

    enum {
        EVENT_SCRIPT_STATUS_DEMO_SCENE           = 9,
        EVENT_SCRIPT_SKIP_SOUND_STOP_CONTROL     = 16,
        EVENT_SCRIPT_DEFAULT_FADE_FRAMES         = 7,
        EVENT_SCRIPT_DISPLAY_TASK_BANK           = 1,
        EVENT_SCRIPT_FLASH_TASK_SLOT             = 0x19,
        EVENT_SCRIPT_FRAMEBUFFER_BLEND_TASK_SLOT = 0x2D,
        EVENT_SCRIPT_SCREEN_FADE_TASK_SLOT       = 0x31,
        EVENT_SCRIPT_TASK_BANK                   = 9,
        EVENT_SCRIPT_SCREEN_SHAKE_TASK_SLOT      = 0xC,
        EVENT_SCRIPT_MUSIC_FADE_TASK_SLOT        = 0xD,
        EVENT_SCRIPT_SOUND_FADE_TASK_SLOT        = 0xE,
        EVENT_SCRIPT_SHAKE_AMPLITUDE_SHIFT       = 8,
        EVENT_SCRIPT_WEAPONS_PLAYER              = 0,
        EVENT_SCRIPT_WEAPONS_COMPANION           = 1,
        EVENT_SCRIPT_WEAPONS_BOTH                = 2,
    };

    work = task->work;
    if (gEvsPlaybackPaused != 0) {
        return;
    }

    if (gDisplayState.demoScene != DISPLAY_DEMO_NONE && padIsStartPressed() != 0 && gDisplayState.pendingMode == DISPLAY_MODE_NONE) {
        gDisplayState.gameMode = DISPLAY_GAME_RESTART;
    }

    // Skipping consumes the target but retains any pending script returns.
    if (padIsStartPressed() != 0 && _gEvsSkipTarget != NULL && gDisplayState.pendingMode == DISPLAY_MODE_NONE && _gEvsSkipDelayUpdates == 0) {
        if (gEvsSelectedSceneKey != NULL) {
            cdCmdCancelScene();
        }
        gCapControlFlags         = CAP_CONTROL_FLAGS_NONE;
        skipTarget               = _gEvsSkipTarget;
        work->waitFrames         = 0;
        _gEvsSkipTarget          = NULL;
        work->command            = skipTarget;
        gCapSuppressViewChanges  = CAP_SEQUENCE_VIEW_CHANGES_SUPPRESSED;
        gGameSession->evtSkipped = 1;
        if (_gEvsSkipKeepsSound != 0) {
            return;
        }
        sndEvtRequestScriptStop(SOUND_BANK_TYPE_ALL_NON_AMBIENT, EVENT_SCRIPT_SKIP_SOUND_STOP_CONTROL);
        return;
    }
    if (_gEvsSkipDelayUpdates != 0) {
        _gEvsSkipDelayUpdates--;
    }

    if (work->waitFrames != 0) {
        work->waitFrames--;
        if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.demoScene == EVENT_SCRIPT_STATUS_DEMO_SCENE) {
            EVENT_SCRIPT_DRAW_DEMO_STATUS(debugTextRequest, _gEvsDemoWaitLabel);
        }
        return;
    }

    if (gCapControlFlags & CAP_CONTROL_EVENT_SCRIPT_WAITING_FOR_CUE) {
        if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.demoScene == EVENT_SCRIPT_STATUS_DEMO_SCENE) {
            EVENT_SCRIPT_DRAW_DEMO_STATUS(debugTextRequest, _gEvsDemoPauseLabel);
        }
        return;
    }

    // Execute consecutive instructions until a wait, poll or end yields.
    while (1) {
        switch (work->command->opcode) {
            case EVENT_SCRIPT_OPCODE_SEND_MESSAGE:
                // Resolve scene recipients before forwarding the untouched payload words.
                if (work->command->operand0.value == GAME_TASK_SLOT_SCENE) {
                    recipientTask = gameGetTaskSlot(GAME_TASK_SLOT_SCENE);
                    if (work->command->operand1.value != EVENT_SCRIPT_MESSAGE_SELECT_SCENE_MANAGER) {
                        TASK_MESSAGE_DISPATCH_SECOND_POINTER(recipientTask, SCENE_MESSAGE_FIND_PLACED_ACTOR,
                                                             (work->command->operand1.value << ENEMY_PLACE_INDEX_SHIFT) | (gGameSession->location.stage << ENEMY_PLACE_STAGE_SHIFT) | gGameSession->location.area,
                                                             &recipientTask);
                    }
                } else if (work->command->operand0.value == EVENT_SCRIPT_MESSAGE_TARGET_OTHER_SCENE_CHILD) {
                    recipientTask = gameGetTaskSlot(GAME_TASK_SLOT_SCENE);
                    TASK_MESSAGE_DISPATCH_SECOND_POINTER(recipientTask, SCENE_MESSAGE_FIND_OTHER_CHILD, work->command->operand1.value, &recipientTask);
                } else {
                    recipientTask = gameGetTaskSlot(work->command->operand0.value);
                }
                if (recipientTask != NULL) {
                    taskMessageDispatch(recipientTask, work->command->operand2.value, work->command->operand3.message.value, work->command->operand4.message.value);
                }
                break;

            case EVENT_SCRIPT_OPCODE_END:
                if (_gEvsFramebufferBlendTask != NULL) {
                    taskCallExit(_gEvsFramebufferBlendTask);
                    _gEvsFramebufferBlendTask = NULL;
                }
                gEvsSelectedSceneKey     = NULL;
                gGameSession->eventState = 0;
                if (task->spawnArg1.value == EVENT_SCRIPT_HUD_HIDE_RESTORE) {
                    taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_CAP_CONTROL), CAP_CONTROL_MESSAGE_SHOW_HUD, 0, 0);
                }
                task->state++;
                displayReleaseMenuHold();
                if (_gEvsSceneWasSelected != 0) {
                    streamFinishScene();
                }
                gCapUseDirectViewIndex = CAP_VIEW_SELECTOR_LOGICAL;
                return;

            case EVENT_SCRIPT_OPCODE_START_FLASH:
                work->primaryEffectTask = taskSpawn(EVENT_SCRIPT_DISPLAY_TASK_BANK, EVENT_SCRIPT_FLASH_TASK_SLOT, work->command->operand0.value, work->command->operand1.value);
                break;

            case EVENT_SCRIPT_OPCODE_SET_DIRTY_VIEW:
                gGameSession->viewDirty = 1;
                /* fallthrough */

            case EVENT_SCRIPT_OPCODE_SET_VIEW:
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.view = (u8)work->command->operand0.value;
                break;

            case EVENT_SCRIPT_OPCODE_WAIT_FRAMES:
                // Advance before yielding; even a zero countdown separates task updates.
                D_801156CB       = 1;
                work->waitFrames = work->command->operand0.value;
                work->command    = work->command + 1;
                return;

            case EVENT_SCRIPT_OPCODE_CANCEL_PRIMARY_FADE:
                if (work->primaryEffectTask != NULL) {
                    taskKill(work->primaryEffectTask);
                    work->primaryEffectTask = NULL;
                }
                break;

            case EVENT_SCRIPT_OPCODE_RESTORE_HUD:
                taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_CAP_CONTROL), CAP_CONTROL_MESSAGE_SHOW_HUD_ABORT, 0, 0);
                break;

            case EVENT_SCRIPT_OPCODE_SET_EVENT_STATE:
                gGameSession->eventState = (u8)work->command->operand0.value;
                break;

            case EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE:
                D_801156CB        = 1;
                gCapControlFlags |= CAP_CONTROL_EVENT_SCRIPT_WAITING_FOR_CUE;
                work->command     = work->command + 1;
                return;

            case EVENT_SCRIPT_OPCODE_WAIT_ANIMATION:
                if (taskMessageDispatch(gameGetTaskSlot(work->command->operand0.value), ANIMATION_MESSAGE_IS_PLAYING, 0, 0) == 0) {
                    break;
                }
                return;

            case EVENT_SCRIPT_OPCODE_WAIT_ACTOR_ACTION:
                if (taskMessageDispatch(gameGetTaskSlot(work->command->operand0.value), GAME_ACTOR_MESSAGE_IS_SCRIPTED_MOTION_PENDING, 0, 0) == 0) {
                    break;
                }
                return;

            case EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION:
                recipientTask = gameGetTaskSlot(work->command->operand0.value);
                // Resolve the weapon bank in a copy of the borrowed request.
                animationRequest = *work->command->operand3.animation;
                if (work->command->operand0.value == GAME_TASK_SLOT_PLAYER) {
                    playerActorWriteWeaponAnimationBankIndex(&animationRequest.source.index);
                } else {
                    companionWriteAnimationBankIndex(&animationRequest.source.index);
                }
                if (recipientTask != NULL) {
                    TASK_MESSAGE_DISPATCH_POINTER(recipientTask, work->command->operand2.value, &animationRequest, work->command->operand4.value);
                }
                break;

            case EVENT_SCRIPT_OPCODE_RESTORE_VIEW:
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.view = _gEvsSavedView;
                break;

            case EVENT_SCRIPT_OPCODE_SELECT_SCENE:
                gEvsSelectedSceneKey = work->command->operand0.sceneKey;
                TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_CAP_CONTROL), CAP_CONTROL_MESSAGE_SELECT_SCENE, gEvsSelectedSceneKey, 0);
                if (gEvsSelectedSceneKey != NULL) {
                    _gEvsSceneWasSelected = true;
                }
                break;

            case EVENT_SCRIPT_OPCODE_CALLBACK:
                work->command->operand0.callback(work->command->operand1.value);
                break;

            case EVENT_SCRIPT_OPCODE_START_VIBRATION:
                padScriptSpawn(work->command->operand0.padCommands, work->command->operand1.vibrationSegments);
                break;

            case EVENT_SCRIPT_OPCODE_START_SOUND:
                sndEvtRequestScriptStart(work->command->operand0.value, (s8)work->command->operand1.value, (s8)work->command->operand2.value);
                _gEvsSoundAttenuationFade.attenuation = work->command->operand2.value;
                break;

            case EVENT_SCRIPT_OPCODE_STOP_SOUND:
                sndEvtRequestScriptStop(work->command->operand0.value, work->command->operand1.value);
                break;

            case EVENT_SCRIPT_OPCODE_SET_FRAMEBUFFER_BLEND:
                if (work->command->operand0.value != 0) {
                    _gEvsFramebufferBlendTask = taskSpawn(EVENT_SCRIPT_DISPLAY_TASK_BANK, EVENT_SCRIPT_FRAMEBUFFER_BLEND_TASK_SLOT, 0, 0);
                } else if (_gEvsFramebufferBlendTask != NULL) {
                    taskCallExit(_gEvsFramebufferBlendTask);
                    _gEvsFramebufferBlendTask = NULL;
                }
                break;

            case EVENT_SCRIPT_OPCODE_START_AREA_MUSIC:
                if (_gEvsAreaMusicStarted == 0) {
                    stageMusicRequestAreaStart((s16)work->command->operand0.value);
                    _gEvsAreaMusicStarted = true;
                }
                break;

            case EVENT_SCRIPT_OPCODE_STOP_AREA_MUSIC:
                stageMusicRequestAreaStop((s16)work->command->operand0.value);
                break;

            case EVENT_SCRIPT_OPCODE_REQUEST_SCENE_MUSIC:
                if (_gEvsSceneMusicRequested != 0) {
                    break;
                }
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = (u8)work->command->operand0.value;
                _gEvsSceneMusicRequested                            = true;
                gStageMusicParams.fadeOutTicks                      = work->command->operand1.value;
                gStageMusicLoadState                                = STAGE_MUSIC_LOAD_ACTIVE;
                gStageMusicParams.field_2                           = work->command->operand2.value;
                taskSpawnFromTable(&gStageMusicTaskDesc, 0, STAGE_MUSIC_REQUEST_ORDINARY, 0);
                break;

            case EVENT_SCRIPT_OPCODE_WAIT_MUSIC_LOAD:
                if (gStageMusicLoadState == STAGE_MUSIC_LOAD_ACTIVE) {
                    return;
                }
                break;

            case EVENT_SCRIPT_OPCODE_SHAKE_SCREEN:
                taskSpawn(EVENT_SCRIPT_TASK_BANK, EVENT_SCRIPT_SCREEN_SHAKE_TASK_SLOT, 0, (work->command->operand0.value << EVENT_SCRIPT_SHAKE_AMPLITUDE_SHIFT) | work->command->operand1.value);
                break;

            case EVENT_SCRIPT_OPCODE_CLEANUP_SCENE:
                if (task->spawnArg1.value == EVENT_SCRIPT_HUD_HIDE_RESTORE) {
                    taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_CAP_CONTROL), CAP_CONTROL_MESSAGE_SHOW_HUD, 0, 0);
                }
                task->spawnArg1.value = EVENT_SCRIPT_HUD_KEEP;
                capAbortPlayback();
                playerActorSetDrawMode(PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO);
                taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_SET_TEXTURE_SEQUENCE, 0, 0);
                if (_gEvsFramebufferBlendTask != NULL) {
                    taskCallExit(_gEvsFramebufferBlendTask);
                    _gEvsFramebufferBlendTask = NULL;
                }
                _evsCancelSecondaryFade(task->work);
                break;

            case EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE:
                if (work->primaryEffectTask != NULL) {
                    break;
                }
                _gEvsPrimaryFade.blend = (u8)work->command->operand0.value;
                _gEvsPrimaryFade.phase = SCREEN_FADE_RUNNING;
                if (work->command->operand1.value == 0) {
                    _gEvsPrimaryFade.rampFrames = EVENT_SCRIPT_DEFAULT_FADE_FRAMES;
                } else {
                    _gEvsPrimaryFade.rampFrames = (u16)work->command->operand1.value;
                }
                work->primaryEffectTask = taskSpawn(EVENT_SCRIPT_DISPLAY_TASK_BANK, EVENT_SCRIPT_SCREEN_FADE_TASK_SLOT, 0, &_gEvsPrimaryFade);
                break;

            case EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE:
                _gEvsPrimaryFade.phase = SCREEN_FADE_RETURN;
                break;

            case EVENT_SCRIPT_OPCODE_FADE_VOLUME:
                if (_gEvsMusicVolumeFadeTask != NULL) {
                    taskKill(_gEvsMusicVolumeFadeTask);
                }
                _gEvsMusicVolumeFade.targetVolume   = work->command->operand0.value;
                _gEvsMusicVolumeFade.durationFrames = work->command->operand1.value;
                _gEvsMusicVolumeFadeTask            = taskSpawn(EVENT_SCRIPT_TASK_BANK, EVENT_SCRIPT_MUSIC_FADE_TASK_SLOT, 0, &_gEvsMusicVolumeFade);
                break;

            case EVENT_SCRIPT_OPCODE_FADE_SOUND_ATTENUATION:
                if (_gEvsSoundAttenuationFadeTask != NULL) {
                    taskKill(_gEvsSoundAttenuationFadeTask);
                }
                _gEvsSoundAttenuationFade.soundId           = work->command->operand0.value;
                _gEvsSoundAttenuationFade.targetAttenuation = work->command->operand1.value;
                _gEvsSoundAttenuationFade.durationFrames    = work->command->operand2.value;
                _gEvsSoundAttenuationFadeTask               = taskSpawn(EVENT_SCRIPT_TASK_BANK, EVENT_SCRIPT_SOUND_FADE_TASK_SLOT, 0, &_gEvsSoundAttenuationFade);
                break;

            case EVENT_SCRIPT_OPCODE_REBUILD_TMD_BUFFERS:
                gpuResetAndInvalidateModelBuffers();
                tmdResetAuxHeapAndRestoreBuffers();
                break;

            case EVENT_SCRIPT_OPCODE_PLAY_SCENE_AUDIO:
                cdCmdEnqueueScenePlayback();
                break;

            case EVENT_SCRIPT_OPCODE_START_SCENE_AUDIO:
                cdCmdStageSceneAudioStart();
                break;

            case EVENT_SCRIPT_OPCODE_SET_AMBIENT_RGB:
                lightColor.vx = work->command->operand0.value * EVENT_SCRIPT_LIGHT_VALUE_SCALE;
                lightColor.vy = work->command->operand1.value * EVENT_SCRIPT_LIGHT_VALUE_SCALE;
                lightColor.vz = work->command->operand2.value * EVENT_SCRIPT_LIGHT_VALUE_SCALE;
                worldCoordSetAmbientColorOverride(&lightColor);
                break;

            case EVENT_SCRIPT_OPCODE_SET_LIGHT_SCALE:
                if (work->command->operand0.value != 0) {
                    lightColor.vx = work->command->operand0.value * EVENT_SCRIPT_LIGHT_VALUE_SCALE;
                    lightColor.vy = work->command->operand0.value * EVENT_SCRIPT_LIGHT_VALUE_SCALE;
                    lightColor.vz = work->command->operand0.value * EVENT_SCRIPT_LIGHT_VALUE_SCALE;
                    worldCoordSetLightColorScaleOverride(&lightColor);
                } else {
                    worldCoordSetLightColorScaleOverride(NULL);
                }
                break;

            case EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB:
                worldCoordSetAmbientColorOverride(NULL);
                break;

            case EVENT_SCRIPT_OPCODE_FINISH_SCENE_STREAM:
                streamFinishScene();
                break;

            case EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE:
                if (work->secondaryFadeTask != NULL && _gEvsSecondaryFade.phase != SCREEN_FADE_DONE) {
                    break;
                }
                _gEvsSecondaryFade.blend = (u8)work->command->operand0.value;
                _gEvsSecondaryFade.phase = SCREEN_FADE_RUNNING;
                if (work->command->operand1.value == 0) {
                    _gEvsSecondaryFade.rampFrames = EVENT_SCRIPT_DEFAULT_FADE_FRAMES;
                } else {
                    _gEvsSecondaryFade.rampFrames = (u16)work->command->operand1.value;
                }
                work->secondaryFadeTask = taskSpawn(EVENT_SCRIPT_DISPLAY_TASK_BANK, EVENT_SCRIPT_SCREEN_FADE_TASK_SLOT, work->command->operand2.value, &_gEvsSecondaryFade);
                break;

            case EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE:
                _gEvsSecondaryFade.phase = SCREEN_FADE_RETURN;
                if (work->command->operand0.value != 0) {
                    _gEvsSecondaryFade.rampFrames = (u16)work->command->operand0.value;
                }
                break;

            case EVENT_SCRIPT_OPCODE_CANCEL_SECONDARY_FADE:
                _evsCancelSecondaryFade(task->work);
                break;

            case EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS:
                equipmentTargets = work->command->operand0.value;
                if (equipmentTargets == EVENT_SCRIPT_WEAPONS_PLAYER || equipmentTargets == EVENT_SCRIPT_WEAPONS_BOTH) {
                    if (_gEvsPlayerEquipmentRestorePending != 0) {
                        gPlayerStatus.weapon = _gEvsSavedPlayerWeapon;
                        playerActorRestoreEquipment();
                        _gEvsPlayerEquipmentRestorePending = false;
                    }
                }
                if ((u32)(equipmentTargets - EVENT_SCRIPT_WEAPONS_COMPANION) < 2U) {
                    if (_gEvsCompanionEquipmentRestorePending != 0) {
                        companionRestoreEquipment();
                        _gEvsCompanionEquipmentRestorePending = false;
                    }
                }
                break;

            case EVENT_SCRIPT_OPCODE_HIDE_WEAPONS:
                equipmentTargets = work->command->operand0.value;
                if (equipmentTargets == EVENT_SCRIPT_WEAPONS_PLAYER || equipmentTargets == EVENT_SCRIPT_WEAPONS_BOTH) {
                    _gEvsPlayerEquipmentRestorePending = true;
                    playerActorRemoveEquipment();
                    playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
                }
                if ((u32)(equipmentTargets - EVENT_SCRIPT_WEAPONS_COMPANION) < 2U) {
                    _gEvsCompanionEquipmentRestorePending = true;
                    recipientTask                         = gameGetTaskSlot(GAME_TASK_SLOT_COMPANION);
                    if (recipientTask != NULL) {
                        companionRemoveEquipment(recipientTask);
                        companionSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
                    }
                }
                break;

            case EVENT_SCRIPT_OPCODE_SET_CAP_DIRECT_VIEW_IDS:
                gCapUseDirectViewIndex = (u8)work->command->operand0.value;
                break;

            case EVENT_SCRIPT_OPCODE_START_STAGE_SOUND:
                sndEvtRequestStageScriptStart(work->command->operand0.value, (s8)work->command->operand1.value, (s8)work->command->operand2.value);
                _gEvsSoundAttenuationFade.attenuation = work->command->operand2.value;
                break;

            // Each control transfer compensates for the common advance below.
            case EVENT_SCRIPT_OPCODE_JUMP:
                work->command = work->command->operand0.commands - 1;
                break;

            case EVENT_SCRIPT_OPCODE_CALL_SCRIPT:
                work->returnStack[work->returnDepth] = work->command + 1;
                work->returnDepth                    = work->returnDepth + 1;
                work->command                        = work->command->operand0.commands - 1;
                break;

            case EVENT_SCRIPT_OPCODE_RETURN:
                work->returnDepth = work->returnDepth - 1;
                work->command     = work->returnStack[work->returnDepth] - 1;
                break;

            case EVENT_SCRIPT_OPCODE_SET_SKIP_TARGET:
                _gEvsSkipTarget = work->command->operand0.commands;
                break;

            case EVENT_SCRIPT_OPCODE_SET_SKIP_KEEP_SOUND:
                _gEvsSkipKeepsSound = (u8)work->command->operand0.value;
                break;

            case EVENT_SCRIPT_OPCODE_SAVE_VIEW:
                _gEvsSavedView = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.view;
                break;
        }
        D_801156CB    = 1;
        work->command = work->command + 1;
    }
}

#undef EVENT_SCRIPT_DRAW_DEMO_STATUS

void evsMusicVolumeFadeTask(Task* task)
{
    enum {
        EVENT_SCRIPT_MUSIC_FADE_INITIALIZE = 0,
        EVENT_SCRIPT_MUSIC_FADE_UPDATE     = 1
    };
    const _EvsMusicVolumeFade* fade;
    s32                        musicLevel;

    fade = task->spawnArg2.pointer;
    switch (task->state) {
        case EVENT_SCRIPT_MUSIC_FADE_INITIALIZE:
            if (fade->durationFrames == 0) {
                midiApplyMusicVolume(fade->targetVolume);
                taskKill(task);
                _gEvsMusicVolumeFadeTask = NULL;
            } else {
                _gEvsMusicVolumeFadeElapsedUpdates = 0;
                _gEvsMusicVolumeFadeStartLevel     = gMidiMusicVolume;
            }
            task->state++;
            break;
        case EVENT_SCRIPT_MUSIC_FADE_UPDATE:
            // Count fade updates after initialization, applying the target on the last one.
            _gEvsMusicVolumeFadeElapsedUpdates++;
            musicLevel = (_gEvsMusicVolumeFadeStartLevel * (fade->durationFrames - _gEvsMusicVolumeFadeElapsedUpdates) + fade->targetVolume * _gEvsMusicVolumeFadeElapsedUpdates) / fade->durationFrames;
            midiApplyMusicVolume(musicLevel);
            if (_gEvsMusicVolumeFadeElapsedUpdates == fade->durationFrames) {
                taskKill(task);
                _gEvsMusicVolumeFadeTask = NULL;
            }
            break;
    }
}

void evsSoundAttenuationFadeTask(Task* task)
{
    enum {
        EVENT_SCRIPT_SOUND_FADE_INITIALIZE      = 0,
        EVENT_SCRIPT_SOUND_FADE_UPDATE          = 1,
        EVENT_SCRIPT_SOUND_FADE_BASE_PAN_OFFSET = 0
    };
    _EvsSoundAttenuationFade* fade;
    s32                       attenuation;

    fade = task->spawnArg2.pointer;
    switch (task->state) {
        case EVENT_SCRIPT_SOUND_FADE_INITIALIZE:
            if (fade->durationFrames == 0) {
                sndEvtRequestScriptMix(fade->soundId, EVENT_SCRIPT_SOUND_FADE_BASE_PAN_OFFSET, (s8)fade->targetAttenuation);
                fade->attenuation = fade->targetAttenuation;
                taskKill(task);
                _gEvsSoundAttenuationFadeTask = NULL;
            } else {
                _gEvsSoundAttenuationFadeElapsedUpdates = 0;
                _gEvsSoundAttenuationFadeStartLevel     = fade->attenuation;
            }
            task->state++;
            break;
        case EVENT_SCRIPT_SOUND_FADE_UPDATE:
            // Retain the halfword level while the sound request consumes its signed low byte.
            _gEvsSoundAttenuationFadeElapsedUpdates++;
            attenuation = (_gEvsSoundAttenuationFadeStartLevel * (fade->durationFrames - _gEvsSoundAttenuationFadeElapsedUpdates) + fade->targetAttenuation * _gEvsSoundAttenuationFadeElapsedUpdates) / fade->durationFrames;
            sndEvtRequestScriptMix(fade->soundId, EVENT_SCRIPT_SOUND_FADE_BASE_PAN_OFFSET, (s8)attenuation);
            fade->attenuation = attenuation;
            if (_gEvsSoundAttenuationFadeElapsedUpdates == fade->durationFrames) {
                taskKill(task);
                _gEvsSoundAttenuationFadeTask = NULL;
            }
            break;
    }
}

void evsStartScript(EvsCommand* script, s32 hudMode)
{
    evsStartScriptWithSkip(script, hudMode, NULL);
}

void evsStartScriptWithSkip(EvsCommand* script, s32 hudMode, EvsCommand* skipScript)
{
    enum {
        EVENT_SCRIPT_INITIAL_EVENT_STATE = 1,
        EVENT_SCRIPT_SKIP_DELAY_UPDATES  = 5,
        EVENT_SCRIPT_INTERPRETER_BANK    = 9,
        EVENT_SCRIPT_INTERPRETER_TYPE    = 7
    };

    // Reset the shared event controls before queuing the interpreter.
    gGameSession->eventState              = EVENT_SCRIPT_INITIAL_EVENT_STATE;
    gGameSession->evtSkipped              = 0;
    _gEvsFramebufferBlendTask             = NULL;
    _gEvsMusicVolumeFadeTask              = NULL;
    _gEvsSkipTarget                       = skipScript;
    _gEvsSceneMusicRequested              = false;
    _gEvsSkipKeepsSound                   = false;
    _gEvsSkipDelayUpdates                 = EVENT_SCRIPT_SKIP_DELAY_UPDATES;
    _gEvsPlayerEquipmentRestorePending    = false;
    _gEvsCompanionEquipmentRestorePending = false;
    _gEvsSavedView                        = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.view;
    _gEvsSavedPlayerWeapon                = gPlayerStatus.weapon;
    sndEvtRequestScriptStop(SOUND_COMMON(0x0D) | SOUND_SCRIPT_STOP_ALL_INSTANCES, SOUND_SCRIPT_STOP_KEEP_RELEASE);
    taskSpawn(EVENT_SCRIPT_INTERPRETER_BANK, EVENT_SCRIPT_INTERPRETER_TYPE, hudMode, script);
}

Task* sceneFindPlacedActor(s32 placementIndex)
{
    s32   placeKey;
    Task* actorTask;

    placeKey = (placementIndex << ENEMY_PLACE_INDEX_SHIFT) | (gGameSession->location.stage << ENEMY_PLACE_STAGE_SHIFT) | gGameSession->location.area;
    TASK_MESSAGE_DISPATCH_SECOND_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_FIND_PLACED_ACTOR, placeKey, &actorTask);
    return actorTask;
}

/// Allocates an event interpreter and acquires its display and optional HUD hold.
///
/// spawnArg2 borrows the entry command stream for the interpreter's lifetime;
/// spawnArg1 selects EVENT_SCRIPT_HUD_HIDE_RESTORE or EVENT_SCRIPT_HUD_KEEP.
/// Allocation failure kills the task without taking a hold. Success attaches
/// zeroed work, resets per-script controls and advances to command execution.
/// The interpreter's cleanup releases the hold and any HUD suppression.
static void _evsInitializeInterpreterTask(Task* task)
{
    _EvsInterpreterWork* work;
    EvsCommand*          entryCommand;

    work = memCalloc(sizeof(*work), false);
    if (work == NULL) {
        taskKill(task);
        return;
    }
    gEvsPlaybackPaused   = false;
    gEvsSelectedSceneKey = NULL;
    displayAcquireMenuHold();
    entryCommand          = task->spawnArg2.pointer;
    gCapControlFlags      = CAP_CONTROL_FLAGS_NONE;
    task->work            = work;
    work->waitFrames      = 0;
    _gEvsAreaMusicStarted = false;
    work->command         = entryCommand;
    _gEvsSceneWasSelected = false;
    if (task->spawnArg1.value == EVENT_SCRIPT_HUD_HIDE_RESTORE) {
        taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_CAP_CONTROL), CAP_CONTROL_MESSAGE_HIDE_HUD, 0, 0);
    }
    D_801156CB              = 1;
    work->primaryEffectTask = NULL;
    work->secondaryFadeTask = NULL;
    work->returnDepth       = 0;
    gCapUseDirectViewIndex  = CAP_VIEW_SELECTOR_LOGICAL;
    task->state++;
}

void evsInterpreterTask(Task* task)
{
    TaskFuncTable3 states;

    states = _gEvsInterpreterStates;
    states.funcs[task->state](task);
}

void capHudSlideTask(Task* task)
{
    enum {
        CAP_HUD_SLIDE_INITIALIZE            = 0,
        CAP_HUD_SLIDE_UPDATE                = 1,
        CAP_HUD_SLIDE_HIDE                  = -1,
        CAP_HUD_SLIDE_MAX_STEP              = 8,
        CAP_HUD_SLIDE_OFFSET_UNITS_PER_STEP = 2
    };
    s16 slideStep;

    switch (task->state) {
        case CAP_HUD_SLIDE_INITIALIZE:
            task->killCountdown   = 0;
            task->spawnArg1.value = CAP_HUD_SLIDE_HIDE;
            task->state++;
            break;
        case CAP_HUD_SLIDE_UPDATE:
            // CAP reverses the direction to return from the held hidden position.
            task->killCountdown -= task->spawnArg1.value;
            if (task->killCountdown >= CAP_HUD_SLIDE_MAX_STEP + 1) {
                task->killCountdown = CAP_HUD_SLIDE_MAX_STEP;
            }
            slideStep = task->killCountdown;
            if (slideStep < 0) {
                gGameSession->hudShakeY = 0;
                taskKill(task);
            } else {
                gGameSession->hudShakeY = slideStep * CAP_HUD_SLIDE_OFFSET_UNITS_PER_STEP;
            }
            break;
    }
}

void evsScreenShakeTask(Task* task)
{
    enum {
        EVENT_SCRIPT_SHAKE_INITIALIZE           = 0,
        EVENT_SCRIPT_SHAKE_UPDATE               = 1,
        EVENT_SCRIPT_SHAKE_DURATION_MASK        = 0xFF,
        EVENT_SCRIPT_SHAKE_AMPLITUDE_SHIFT      = 8,
        EVENT_SCRIPT_SHAKE_RANDOM_FRACTION_BITS = 16
    };
    s32 packedShake;
    s32 halfDurationFrames;
    s32 envelopeAmplitude;
    s32 shakeSample;

    /// Samples the triangular shake envelope and alternates sign with the task cursor.
    ///
    /// `packed` contains the signed pixel amplitude above bit 7; `halfDuration`
    /// is 1..255 frames and the task cursor is within [-halfDuration, halfDuration].
    /// The cursor starts at the negative endpoint; amplitude grows toward zero
    /// then decays. One LCG advance supplies an unsigned 16-bit fraction. Odd
    /// cursor values select positive displacement, even values negative.
    /// `sample` and `weightedAmplitude` must be distinct s32
    /// local lvalues that do not alias the input arguments. All argument
    /// evaluations must be side-effect-free. Re-reads the task
    /// cursor after advancing `gRandomLcgState`. Uses the local packing and
    /// fractional-bit constants and shared random state; no caller locals are
    /// captured. Preserves signed multiply/divide then arithmetic-shift order;
    /// inputs must keep the signed products representable. Expands to a compound statement.
#define EVENT_SCRIPT_SAMPLE_SCREEN_SHAKE(shakeTask, packed, halfDuration, sample, weightedAmplitude)                                                                      \
    {                                                                                                                                                                     \
        (sample)            = (halfDuration) - ABS((shakeTask)->spawnArg1.value);                                                                                         \
        (weightedAmplitude) = (sample) * ((packed) >> EVENT_SCRIPT_SHAKE_AMPLITUDE_SHIFT);                                                                                \
        RANDOM_LCG_ADVANCE();                                                                                                                                             \
        (sample) = ((weightedAmplitude) * (s32)(gRandomLcgState >> EVENT_SCRIPT_SHAKE_RANDOM_FRACTION_BITS)) / (halfDuration) >> EVENT_SCRIPT_SHAKE_RANDOM_FRACTION_BITS; \
        if ((shakeTask)->spawnArg1.value & 1) {                                                                                                                           \
            (sample) = ABS(sample);                                                                                                                                       \
        } else {                                                                                                                                                          \
            (sample) = -ABS(sample);                                                                                                                                      \
        }                                                                                                                                                                 \
    }

    packedShake        = task->spawnArg2.value;
    halfDurationFrames = packedShake & EVENT_SCRIPT_SHAKE_DURATION_MASK;

    switch (task->state) {
        case EVENT_SCRIPT_SHAKE_INITIALIZE:
            task->spawnArg1.value = -halfDurationFrames;
            task->state++;
            break;
        case EVENT_SCRIPT_SHAKE_UPDATE:
            if (halfDurationFrames < task->spawnArg1.value) {
                displaySetShakeY(0);
                taskKill(task);
            } else {
                // Grow and decay the envelope around the middle frame.
                EVENT_SCRIPT_SAMPLE_SCREEN_SHAKE(task, packedShake, halfDurationFrames, shakeSample, envelopeAmplitude);
                displaySetShakeY(shakeSample);
                task->spawnArg1.value++;
            }
            break;
    }
#undef EVENT_SCRIPT_SAMPLE_SCREEN_SHAKE
}
