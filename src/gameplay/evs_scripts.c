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
#include "evs.h"
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

/* Define BSS before API headers to preserve first-declaration order. */
u16 D_801156C0;

u16 D_801156C2;

u16 D_801156C4;

u16 D_801156C6;

u8 D_801156C8;

u8 D_801156C9;

u8 D_801156CA;

u8 D_801156CB;

u8 D_801156CC;

u8 D_801156CD;

u8 D_801156CE;

EvsCommand* D_801156D0;

ScreenFade D_801156D4;

ScreenFade D_801156D8;

_EvsMusicVolumeFade D_801156DC;

_EvsSoundAttenuationFade D_801156E0;

s32 D_801156EC;

u8 D_801156F0;

// Scene/audio selection shared with CAP tasks.
EvsOperand D_801156F4;

u8 D_801156F8;

u8 D_801156F9;

#include "gameplay/evs_scripts.h"

#include "evs_scripts.h"

extern Task* D_8010FBE0;

extern Task* D_8010FBE4;

extern Task* D_8010FBE8;

static const TaskFuncTable3 Gp_ScriptTaskStates;

static const char Gp_StrDemoWait[];

static const char Gp_StrDemoPause[];

static void Gp_ScriptTaskState1(Task* arg0);

static void Gp_ScriptInit(Task* arg0);

Task* D_8010FBE0 = NULL;

Task* D_8010FBE4 = NULL;

Task* D_8010FBE8 = NULL;

static const TaskFuncTable3 Gp_ScriptTaskStates = { {
    Gp_ScriptInit,
    Gp_ScriptTaskState1,
    taskKill,
} };

static const char Gp_StrDemoWait[]  = "Demo Wait";
static const char Gp_StrDemoPause[] = "Demo Pause";

// Script light values gain four fractional bits before their s16 conversion.
enum { EVENT_SCRIPT_LIGHT_VALUE_SCALE = 16 };

/// Cancels the interpreter's secondary screen fade and clears its borrowed handle.
///
/// `work` must be live interpreter work. A done phase means the fade task has
/// released itself; clear that stale handle without dereferencing it.
static inline void _evsCancelSecondaryFade(_EvsInterpreterWork* work)
{
    if (work->secondaryFadeTask != NULL) {
        if (D_801156D8.phase != SCREEN_FADE_DONE) {
            taskKill(work->secondaryFadeTask);
        }
        work->secondaryFadeTask = NULL;
    }
}

static void Gp_ScriptTaskState1(Task* arg0)
{
    EvsCommand*          continuation;
    _EvsInterpreterWork* work;
    AnimationPlayRequest rec;
    SVECTOR              vec;
    TextDrawReq          req;
    Task*                slot;
    s32                  mode;

    work = arg0->work;
    if (D_801156F9 != 0) {
        return;
    }

    if (gDisplayState.demoScene != DISPLAY_DEMO_NONE && padIsStartPressed() != 0 && gDisplayState.pendingMode == DISPLAY_MODE_NONE) {
        gDisplayState.gameMode = DISPLAY_GAME_RESTART;
    }

    if (padIsStartPressed() != 0 && D_801156D0 != NULL && gDisplayState.pendingMode == DISPLAY_MODE_NONE && D_801156F0 == 0) {
        if (D_801156F4.sceneKey != NULL) {
            cdCmdCancelScene();
        }
        D_801156A4               = 0;
        continuation             = D_801156D0;
        work->waitFrames         = 0;
        D_801156D0               = NULL;
        work->command            = continuation;
        D_80115688               = 1;
        gGameSession->evtSkipped = 1;
        if (D_801156CC != 0) {
            return;
        }
        sndEvtRequestScriptStop(SOUND_BANK_TYPE_ALL_NON_AMBIENT, 0x10);
        return;
    }
    if (D_801156F0 != 0) {
        D_801156F0--;
    }

    if (work->waitFrames != 0) {
        work->waitFrames--;
        if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.demoScene == 9) {
            req.x          = -0x8C;
            req.y          = 0x50;
            req.otIndex    = 4;
            req.colorRgb   = 0x808008;
            req.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
            req.alignment  = TEXT_ALIGNMENT_LEFT;
            req.drawMode   = TEXT_DRAW_FILL_ONLY;
            textDrawString(&req, Gp_StrDemoWait);
        }
        return;
    }

    if (D_801156A4 & 0x40) {
        if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.demoScene == 9) {
            req.x          = -0x8C;
            req.y          = 0x50;
            req.otIndex    = 4;
            req.colorRgb   = 0x808008;
            req.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
            req.alignment  = TEXT_ALIGNMENT_LEFT;
            req.drawMode   = TEXT_DRAW_FILL_ONLY;
            textDrawString(&req, Gp_StrDemoPause);
        }
        return;
    }

    while (1) {
        switch (work->command->opcode) {
            case EVENT_SCRIPT_OPCODE_SEND_MESSAGE:
                // Resolve scene recipients before forwarding the untouched payload words.
                if (work->command->operand0.value == GAME_TASK_SLOT_SCENE) {
                    slot = gameGetTaskSlot(GAME_TASK_SLOT_SCENE);
                    if (work->command->operand1.value != EVENT_SCRIPT_MESSAGE_SELECT_SCENE_MANAGER) {
                        TASK_MESSAGE_DISPATCH_SECOND_POINTER(slot, SCENE_MESSAGE_FIND_PLACED_ACTOR,
                                                             (work->command->operand1.value << ENEMY_PLACE_INDEX_SHIFT) | (gGameSession->location.loc.stage << ENEMY_PLACE_STAGE_SHIFT) | gGameSession->location.loc.area,
                                                             &slot);
                    }
                } else if (work->command->operand0.value == EVENT_SCRIPT_MESSAGE_TARGET_OTHER_SCENE_CHILD) {
                    slot = gameGetTaskSlot(GAME_TASK_SLOT_SCENE);
                    TASK_MESSAGE_DISPATCH_SECOND_POINTER(slot, SCENE_MESSAGE_FIND_OTHER_CHILD, work->command->operand1.value, &slot);
                } else {
                    slot = gameGetTaskSlot(work->command->operand0.value);
                }
                if (slot != NULL) {
                    taskMessageDispatch(slot, work->command->operand2.value, work->command->operand3.message.value, work->command->operand4.message.value);
                }
                break;

            case EVENT_SCRIPT_OPCODE_END:
                if (D_8010FBE0 != NULL) {
                    taskCallExit(D_8010FBE0);
                    D_8010FBE0 = NULL;
                }
                D_801156F4.sceneKey      = NULL;
                gGameSession->eventState = 0;
                if (arg0->spawnArg1.value == 0) {
                    taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_CAP_CONTROL), CAP_CONTROL_MESSAGE_SHOW_HUD, 0, 0);
                }
                arg0->state++;
                displayReleaseMenuHold();
                if (D_801156CA != 0) {
                    streamFinishScene();
                }
                D_8011569C = 0;
                return;

            case EVENT_SCRIPT_OPCODE_START_FLASH:
                work->primaryEffectTask = taskSpawn(1, 0x19, work->command->operand0.value, work->command->operand1.value);
                break;

            case EVENT_SCRIPT_OPCODE_SET_DIRTY_VIEW:
                gGameSession->viewDirty = 1;
                /* fallthrough */

            case EVENT_SCRIPT_OPCODE_SET_VIEW:
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = (u8)work->command->operand0.value;
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
                D_801156CB    = 1;
                D_801156A4   |= 0x40;
                work->command = work->command + 1;
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
                slot = gameGetTaskSlot(work->command->operand0.value);
                // Resolve the weapon bank in a copy of the borrowed request.
                rec = *work->command->operand3.animation;
                if (work->command->operand0.value == 3) {
                    playerActorWriteWeaponAnimationBankIndex(&rec.source.index);
                } else {
                    companionWriteAnimationBankIndex(&rec.source.index);
                }
                if (slot != NULL) {
                    TASK_MESSAGE_DISPATCH_POINTER(slot, work->command->operand2.value, &rec, work->command->operand4.value);
                }
                break;

            case EVENT_SCRIPT_OPCODE_RESTORE_VIEW:
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = D_801156F8;
                break;

            case EVENT_SCRIPT_OPCODE_SELECT_SCENE:
                D_801156F4.sceneKey = work->command->operand0.sceneKey;
                taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_CAP_CONTROL), 0xFA6, D_801156F4.value, 0);
                if (D_801156F4.sceneKey != NULL) {
                    D_801156CA = 1;
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
                D_801156E0.attenuation = work->command->operand2.value;
                break;

            case EVENT_SCRIPT_OPCODE_STOP_SOUND:
                sndEvtRequestScriptStop(work->command->operand0.value, work->command->operand1.value);
                break;

            case EVENT_SCRIPT_OPCODE_SET_FRAMEBUFFER_BLEND:
                if (work->command->operand0.value != 0) {
                    D_8010FBE0 = taskSpawn(1, 0x2D, 0, 0);
                } else if (D_8010FBE0 != NULL) {
                    taskCallExit(D_8010FBE0);
                    D_8010FBE0 = NULL;
                }
                break;

            case EVENT_SCRIPT_OPCODE_START_AREA_MUSIC:
                if (D_801156C8 == 0) {
                    Stage_RequestFromAreaTable((s16)work->command->operand0.value);
                    D_801156C8 = 1;
                }
                break;

            case EVENT_SCRIPT_OPCODE_STOP_AREA_MUSIC:
                Stage_RequestMidiFromMap((s16)work->command->operand0.value);
                break;

            case EVENT_SCRIPT_OPCODE_REQUEST_SCENE_MUSIC:
                if (D_801156C9 != 0) {
                    break;
                }
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = (u8)work->command->operand0.value;
                D_801156C9                                          = 1;
                gStageMusicParams.fadeOutTicks                      = work->command->operand1.value;
                gStageMusicLoadState                                = 0;
                gStageMusicParams.field_2                           = work->command->operand2.value;
                taskSpawnFromTable(&Stage_MusicTaskDesc, 0, 0, 0);
                break;

            case EVENT_SCRIPT_OPCODE_WAIT_MUSIC_LOAD:
                if (gStageMusicLoadState == 0) {
                    return;
                }
                break;

            case EVENT_SCRIPT_OPCODE_SHAKE_SCREEN:
                taskSpawn(9, 0xC, 0, (work->command->operand0.value << 8) | work->command->operand1.value);
                break;

            case EVENT_SCRIPT_OPCODE_CLEANUP_SCENE:
                if (arg0->spawnArg1.value == 0) {
                    taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_CAP_CONTROL), CAP_CONTROL_MESSAGE_SHOW_HUD, 0, 0);
                }
                arg0->spawnArg1.value = 1;
                Gp_AbortCap();
                playerActorSetDrawMode(PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO);
                taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_SET_TEXTURE_SEQUENCE, 0, 0);
                if (D_8010FBE0 != NULL) {
                    taskCallExit(D_8010FBE0);
                    D_8010FBE0 = NULL;
                }
                _evsCancelSecondaryFade(arg0->work);
                break;

            case EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE:
                if (work->primaryEffectTask != NULL) {
                    break;
                }
                D_801156D4.blend = (u8)work->command->operand0.value;
                D_801156D4.phase = SCREEN_FADE_RUNNING;
                if (work->command->operand1.value == 0) {
                    D_801156D4.rampFrames = 7;
                } else {
                    D_801156D4.rampFrames = (u16)work->command->operand1.value;
                }
                work->primaryEffectTask = taskSpawn(1, 0x31, 0, &D_801156D4);
                break;

            case EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE:
                D_801156D4.phase = SCREEN_FADE_RETURN;
                break;

            case EVENT_SCRIPT_OPCODE_FADE_VOLUME:
                if (D_8010FBE4 != NULL) {
                    taskKill(D_8010FBE4);
                }
                D_801156DC.targetVolume   = work->command->operand0.value;
                D_801156DC.durationFrames = work->command->operand1.value;
                D_8010FBE4                = taskSpawn(9, 0xD, 0, &D_801156DC);
                break;

            case EVENT_SCRIPT_OPCODE_FADE_SOUND_ATTENUATION:
                if (D_8010FBE8 != NULL) {
                    taskKill(D_8010FBE8);
                }
                D_801156E0.soundId           = work->command->operand0.value;
                D_801156E0.targetAttenuation = work->command->operand1.value;
                D_801156E0.durationFrames    = work->command->operand2.value;
                D_8010FBE8                   = taskSpawn(9, 0xE, 0, &D_801156E0);
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
                vec.vx = work->command->operand0.value * EVENT_SCRIPT_LIGHT_VALUE_SCALE;
                vec.vy = work->command->operand1.value * EVENT_SCRIPT_LIGHT_VALUE_SCALE;
                vec.vz = work->command->operand2.value * EVENT_SCRIPT_LIGHT_VALUE_SCALE;
                worldCoordSetAmbientColorOverride(&vec);
                break;

            case EVENT_SCRIPT_OPCODE_SET_LIGHT_SCALE:
                if (work->command->operand0.value != 0) {
                    vec.vx = work->command->operand0.value * EVENT_SCRIPT_LIGHT_VALUE_SCALE;
                    vec.vy = work->command->operand0.value * EVENT_SCRIPT_LIGHT_VALUE_SCALE;
                    vec.vz = work->command->operand0.value * EVENT_SCRIPT_LIGHT_VALUE_SCALE;
                    worldCoordSetLightColorScaleOverride(&vec);
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
                if (work->secondaryFadeTask != NULL && D_801156D8.phase != SCREEN_FADE_DONE) {
                    break;
                }
                D_801156D8.blend = (u8)work->command->operand0.value;
                D_801156D8.phase = SCREEN_FADE_RUNNING;
                if (work->command->operand1.value == 0) {
                    D_801156D8.rampFrames = 7;
                } else {
                    D_801156D8.rampFrames = (u16)work->command->operand1.value;
                }
                work->secondaryFadeTask = taskSpawn(1, 0x31, work->command->operand2.value, &D_801156D8);
                break;

            case EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE:
                D_801156D8.phase = SCREEN_FADE_RETURN;
                if (work->command->operand0.value != 0) {
                    D_801156D8.rampFrames = (u16)work->command->operand0.value;
                }
                break;

            case EVENT_SCRIPT_OPCODE_CANCEL_SECONDARY_FADE:
                _evsCancelSecondaryFade(arg0->work);
                break;

            case EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS:
                mode = work->command->operand0.value;
                if (mode == 0 || mode == 2) {
                    if (D_801156CD != 0) {
                        gPlayerStatus.weapon = D_801156EC;
                        Gp_SpawnWeaponEff();
                        D_801156CD = 0;
                    }
                }
                if ((u32)(mode - 1) < 2U) {
                    if (D_801156CE != 0) {
                        Gp_SetupAllyWeapon();
                        D_801156CE = 0;
                    }
                }
                break;

            case EVENT_SCRIPT_OPCODE_HIDE_WEAPONS:
                mode = work->command->operand0.value;
                if (mode == 0 || mode == 2) {
                    D_801156CD = 1;
                    playerActorRemoveEquipment();
                    playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
                }
                if ((u32)(mode - 1) < 2U) {
                    D_801156CE = 1;
                    slot       = gameGetTaskSlot(GAME_TASK_SLOT_COMPANION);
                    if (slot != NULL) {
                        Gp_EndPlayerActorTask(slot);
                        companionSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
                    }
                }
                break;

            case EVENT_SCRIPT_OPCODE_SET_CAP_DIRECT_VIEW_IDS:
                D_8011569C = (u8)work->command->operand0.value;
                break;

            case EVENT_SCRIPT_OPCODE_START_STAGE_SOUND:
                sndEvtRequestStageScriptStart(work->command->operand0.value, (s8)work->command->operand1.value, (s8)work->command->operand2.value);
                D_801156E0.attenuation = work->command->operand2.value;
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
                D_801156D0 = work->command->operand0.commands;
                break;

            case EVENT_SCRIPT_OPCODE_SET_SKIP_KEEP_SOUND:
                D_801156CC = (u8)work->command->operand0.value;
                break;

            case EVENT_SCRIPT_OPCODE_SAVE_VIEW:
                D_801156F8 = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view;
                break;
        }
        D_801156CB    = 1;
        work->command = work->command + 1;
    }
}

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
                D_8010FBE4 = NULL;
            } else {
                D_801156C2 = 0;
                D_801156C0 = D_8007A396;
            }
            task->state++;
            break;
        case EVENT_SCRIPT_MUSIC_FADE_UPDATE:
            // Count fade updates after initialization, applying the target on the last one.
            D_801156C2++;
            musicLevel = (D_801156C0 * (fade->durationFrames - D_801156C2) + fade->targetVolume * D_801156C2) / fade->durationFrames;
            midiApplyMusicVolume(musicLevel);
            if (D_801156C2 == fade->durationFrames) {
                taskKill(task);
                D_8010FBE4 = NULL;
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
                D_8010FBE8 = NULL;
            } else {
                D_801156C6 = 0;
                D_801156C4 = fade->attenuation;
            }
            task->state++;
            break;
        case EVENT_SCRIPT_SOUND_FADE_UPDATE:
            // Retain the halfword level while the sound request consumes its signed low byte.
            D_801156C6++;
            attenuation = (D_801156C4 * (fade->durationFrames - D_801156C6) + fade->targetAttenuation * D_801156C6) / fade->durationFrames;
            sndEvtRequestScriptMix(fade->soundId, EVENT_SCRIPT_SOUND_FADE_BASE_PAN_OFFSET, (s8)attenuation);
            fade->attenuation = attenuation;
            if (D_801156C6 == fade->durationFrames) {
                taskKill(task);
                D_8010FBE8 = NULL;
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
    gGameSession->eventState = EVENT_SCRIPT_INITIAL_EVENT_STATE;
    gGameSession->evtSkipped = 0;
    D_8010FBE0               = NULL;
    D_8010FBE4               = NULL;
    D_801156D0               = skipScript;
    D_801156C9               = 0;
    D_801156CC               = 0;
    D_801156F0               = EVENT_SCRIPT_SKIP_DELAY_UPDATES;
    D_801156CD               = 0;
    D_801156CE               = 0;
    D_801156F8               = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view;
    D_801156EC               = gPlayerStatus.weapon;
    sndEvtRequestScriptStop(SOUND_COMMON(0x0D) | SOUND_SCRIPT_STOP_ALL_INSTANCES, SOUND_SCRIPT_STOP_KEEP_RELEASE);
    taskSpawn(EVENT_SCRIPT_INTERPRETER_BANK, EVENT_SCRIPT_INTERPRETER_TYPE, hudMode, script);
}

Task* sceneFindPlacedActor(s32 placementIndex)
{
    s32   placeKey;
    Task* actorTask;

    placeKey = (placementIndex << ENEMY_PLACE_INDEX_SHIFT) | (gGameSession->location.loc.stage << ENEMY_PLACE_STAGE_SHIFT) | gGameSession->location.loc.area;
    TASK_MESSAGE_DISPATCH_SECOND_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_FIND_PLACED_ACTOR, placeKey, &actorTask);
    return actorTask;
}

static void Gp_ScriptInit(Task* arg0)
{
    _EvsInterpreterWork* work;
    EvsCommand*          script;

    work = memCalloc(sizeof(*work), 0);
    if (work == NULL) {
        taskKill(arg0);
        return;
    }
    D_801156F9          = 0;
    D_801156F4.sceneKey = 0;
    displayAcquireMenuHold();
    script           = arg0->spawnArg2.pointer;
    D_801156A4       = 0;
    arg0->work       = work;
    work->waitFrames = 0;
    D_801156C8       = 0;
    work->command    = script;
    D_801156CA       = 0;
    if (arg0->spawnArg1.value == 0) {
        taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_CAP_CONTROL), CAP_CONTROL_MESSAGE_HIDE_HUD, 0, 0);
    }
    D_801156CB              = 1;
    work->primaryEffectTask = NULL;
    work->secondaryFadeTask = NULL;
    work->returnDepth       = 0;
    D_8011569C              = 0;
    arg0->state++;
}

void func_800E8830(Task* arg0)
{
    TaskFuncTable3 sp;

    sp = Gp_ScriptTaskStates;
    sp.funcs[arg0->state](arg0);
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
    /// must be nonzero. `sample` and `weightedAmplitude` must be distinct s32
    /// local lvalues that do not alias the input arguments. All argument
    /// evaluations must be side-effect-free. Re-reads the task
    /// cursor after advancing `gRandomLcgState`. Uses the local packing and
    /// fractional-bit constants and shared random state; expands to a compound statement.
#define EVENT_SCRIPT_SAMPLE_SCREEN_SHAKE(shakeTask, packed, halfDuration, sample, weightedAmplitude)                                                                                 \
    {                                                                                                                                                                                \
        (sample)            = (halfDuration) - ABS((shakeTask)->spawnArg1.value);                                                                                                    \
        (weightedAmplitude) = (sample) * ((packed) >> EVENT_SCRIPT_SHAKE_AMPLITUDE_SHIFT);                                                                                           \
        gRandomLcgState     = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;                                                                                        \
        (sample)            = ((weightedAmplitude) * (s32)(gRandomLcgState >> EVENT_SCRIPT_SHAKE_RANDOM_FRACTION_BITS)) / (halfDuration) >> EVENT_SCRIPT_SHAKE_RANDOM_FRACTION_BITS; \
        if ((shakeTask)->spawnArg1.value & 1) {                                                                                                                                      \
            (sample) = ABS(sample);                                                                                                                                                  \
        } else {                                                                                                                                                                     \
            (sample) = -ABS(sample);                                                                                                                                                 \
        }                                                                                                                                                                            \
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
