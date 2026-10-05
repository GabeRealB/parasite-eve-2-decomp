#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/abs.h>

#include "common.h"

#include "gameplay/captions.h"
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

/// Ends the script's secondary screen fade and forgets its task.
///
/// The fade task exits by itself once its record reports done, so the handle
/// is only killed while the fade is still in progress.
static inline void _evsCancelSecondaryFade(Task* task)
{
    _EvsInterpreterWork* work;

    work = task->work;
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

    if (gDisplayState.demoScene != DISPLAY_DEMO_NONE && Pad_CheckFlag800() != 0 && gDisplayState.pendingMode == DISPLAY_MODE_NONE) {
        gDisplayState.gameMode = DISPLAY_GAME_RESTART;
    }

    if (Pad_CheckFlag800() != 0 && D_801156D0 != NULL && gDisplayState.pendingMode == DISPLAY_MODE_NONE && D_801156F0 == 0) {
        if (D_801156F4.sceneKey != NULL) {
            CdCmd_CancelReplaceAndActivate();
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
        SndEvt_EnqueueType7(SOUND_BANK_TYPE_ALL_NON_AMBIENT, 0x10);
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
            Text_DrawString(&req, Gp_StrDemoWait);
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
            Text_DrawString(&req, Gp_StrDemoPause);
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
                Display_ReleaseRef();
                if (D_801156CA != 0) {
                    Gp_RestoreStreamRng();
                }
                D_8011569C = 0;
                return;

            case EVENT_SCRIPT_OPCODE_START_FLASH:
                work->primaryEffectTask = Task_Spawn(1, 0x19, work->command->operand0.value, work->command->operand1.value);
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
                    Gp_PlayerWeaponId(&rec.source.index);
                } else {
                    Gp_AllyAnimId(&rec.source.index);
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
                Gp_SpawnScript18(work->command->operand0.padCommands, work->command->operand1.vibrationSegments);
                break;

            case EVENT_SCRIPT_OPCODE_START_SOUND:
                sndEvtRequestScriptStart(work->command->operand0.value, (s8)work->command->operand1.value, (s8)work->command->operand2.value);
                D_801156E0.attenuation = work->command->operand2.value;
                break;

            case EVENT_SCRIPT_OPCODE_STOP_SOUND:
                SndEvt_EnqueueType7(work->command->operand0.value, (u16)work->command->operand1.value);
                break;

            case EVENT_SCRIPT_OPCODE_SET_FRAMEBUFFER_BLEND:
                if (work->command->operand0.value != 0) {
                    D_8010FBE0 = Task_Spawn(1, 0x2D, 0, 0);
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
                Task_Spawn(9, 0xC, 0, (work->command->operand0.value << 8) | work->command->operand1.value);
                break;

            case EVENT_SCRIPT_OPCODE_CLEANUP_SCENE:
                if (arg0->spawnArg1.value == 0) {
                    taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_CAP_CONTROL), CAP_CONTROL_MESSAGE_SHOW_HUD, 0, 0);
                }
                arg0->spawnArg1.value = 1;
                Gp_AbortCap();
                Gp_MsgPlayer3F3(1);
                taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_SET_TEXTURE_SEQUENCE, 0, 0);
                if (D_8010FBE0 != NULL) {
                    taskCallExit(D_8010FBE0);
                    D_8010FBE0 = NULL;
                }
                _evsCancelSecondaryFade(arg0);
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
                work->primaryEffectTask = Task_SpawnPtr(1, 0x31, 0, &D_801156D4);
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
                D_8010FBE4                = Task_SpawnPtr(9, 0xD, 0, &D_801156DC);
                break;

            case EVENT_SCRIPT_OPCODE_FADE_SOUND_ATTENUATION:
                if (D_8010FBE8 != NULL) {
                    taskKill(D_8010FBE8);
                }
                D_801156E0.soundId           = work->command->operand0.value;
                D_801156E0.targetAttenuation = work->command->operand1.value;
                D_801156E0.durationFrames    = work->command->operand2.value;
                D_8010FBE8                   = Task_SpawnPtr(9, 0xE, 0, &D_801156E0);
                break;

            case EVENT_SCRIPT_OPCODE_REBUILD_TMD_BUFFERS:
                Gpu_ResetGraphAndOt();
                Tmd_AllocMissingBuffers();
                break;

            case EVENT_SCRIPT_OPCODE_PLAY_SCENE_AUDIO:
                CdCmd_EnqueueOverlay81();
                break;

            case EVENT_SCRIPT_OPCODE_START_SCENE_AUDIO:
                CdCmd_EnqueueReplaceOverlay82();
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
                Gp_RestoreStreamRng();
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
                work->secondaryFadeTask = Task_SpawnPtr(1, 0x31, work->command->operand2.value, &D_801156D8);
                break;

            case EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE:
                D_801156D8.phase = SCREEN_FADE_RETURN;
                if (work->command->operand0.value != 0) {
                    D_801156D8.rampFrames = (u16)work->command->operand0.value;
                }
                break;

            case EVENT_SCRIPT_OPCODE_CANCEL_SECONDARY_FADE:
                _evsCancelSecondaryFade(arg0);
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
                    Gp_KillPlayerEffs();
                    Gp_MsgPlayerWeapon(0);
                }
                if ((u32)(mode - 1) < 2U) {
                    D_801156CE = 1;
                    slot       = gameGetTaskSlot(GAME_TASK_SLOT_COMPANION);
                    if (slot != NULL) {
                        Gp_EndPlayerActorTask(slot);
                        Gp_MsgAllyWeapon(0);
                    }
                }
                break;

            case EVENT_SCRIPT_OPCODE_SET_CAP_DIRECT_VIEW_IDS:
                D_8011569C = (u8)work->command->operand0.value;
                break;

            case EVENT_SCRIPT_OPCODE_START_STAGE_SOUND:
                Gp_EnqueueStageSnd6(work->command->operand0.value, (s8)work->command->operand1.value, (s8)work->command->operand2.value);
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

void Gp_VolFadeTask(Task* arg0)
{
    _EvsMusicVolumeFade* fade;
    s32                  volume;

    fade = arg0->spawnArg2.pointer;
    switch (arg0->state) {
        case 0:
            if (fade->durationFrames == 0) {
                Snd_ApplyVolumeTable(fade->targetVolume);
                taskKill(arg0);
                D_8010FBE4 = 0;
            } else {
                D_801156C2 = 0;
                D_801156C0 = D_8007A396;
            }
            arg0->state++;
            break;
        case 1:
            D_801156C2++;
            volume = (D_801156C0 * (fade->durationFrames - D_801156C2) + fade->targetVolume * D_801156C2) / fade->durationFrames;
            Snd_ApplyVolumeTable(volume & 0xFFFF);
            if (D_801156C2 == fade->durationFrames) {
                taskKill(arg0);
                D_8010FBE4 = 0;
            }
            break;
    }
}

void Gp_SndFadeTask(Task* arg0)
{
    _EvsSoundAttenuationFade* fade;
    s32                       volume;

    fade = arg0->spawnArg2.pointer;
    switch (arg0->state) {
        case 0:
            if (fade->durationFrames == 0) {
                SndEvt_EnqueueTypeA(fade->soundId, 0, (s8)fade->targetAttenuation);
                fade->attenuation = fade->targetAttenuation;
                taskKill(arg0);
                D_8010FBE8 = 0;
            } else {
                D_801156C6 = 0;
                D_801156C4 = fade->attenuation;
            }
            arg0->state++;
            break;
        case 1:
            D_801156C6++;
            volume = (D_801156C4 * (fade->durationFrames - D_801156C6) + fade->targetAttenuation * D_801156C6) / fade->durationFrames;
            SndEvt_EnqueueTypeA(fade->soundId, 0, (s8)volume);
            fade->attenuation = volume;
            if (D_801156C6 == fade->durationFrames) {
                taskKill(arg0);
                D_8010FBE8 = 0;
            }
            break;
    }
}

void func_800E8614(EvsCommand* arg0, s32 arg1)
{
    func_800E8634(arg0, arg1, NULL);
}

void func_800E8634(EvsCommand* arg0, s32 arg1, EvsCommand* arg2)
{
    gGameSession->eventState = 1;
    gGameSession->evtSkipped = 0;
    D_8010FBE0               = 0;
    D_8010FBE4               = 0;
    D_801156D0               = arg2;
    D_801156C9               = 0;
    D_801156CC               = 0;
    D_801156F0               = 5;
    D_801156CD               = 0;
    D_801156CE               = 0;
    D_801156F8               = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view;
    D_801156EC               = gPlayerStatus.weapon;
    SndEvt_EnqueueType7(0xFF0D, 1);
    Task_Spawn(9, 7, arg1, arg0);
}

Task* Gp_LookupSlot4(s32 arg0)
{
    Task* out;

    arg0 = (arg0 << ENEMY_PLACE_INDEX_SHIFT) | (gGameSession->location.loc.stage << ENEMY_PLACE_STAGE_SHIFT) | gGameSession->location.loc.area;
    TASK_MESSAGE_DISPATCH_SECOND_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_FIND_PLACED_ACTOR, arg0, &out);
    return out;
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
    Display_AcquireRef();
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

void func_800E8888(Task* arg0)
{
    s16 tmp;

    switch (arg0->state) {
        case 0:
            arg0->killCountdown   = 0;
            arg0->spawnArg1.value = -1;
            arg0->state++;
            break;
        case 1:
            arg0->killCountdown = (u16)arg0->killCountdown - (u16)arg0->spawnArg1.value;
            if (arg0->killCountdown >= 9) {
                arg0->killCountdown = 8;
            }
            tmp = arg0->killCountdown;
            if (tmp < 0) {
                gGameSession->hudShakeY = 0;
                taskKill(arg0);
            } else {
                gGameSession->hudShakeY = tmp * 2;
            }
            break;
    }
}

/// Screen-shake task. `spawnArg2` is a packed s32: low byte is the
/// duration bound (counter runs `-lo` .. `+lo`); `>> 8` is amplitude.
/// Each frame an LCG (`gRandomLcgState`) scales the remaining count into
/// `displaySetShakeY`, flipping sign on `spawnArg1` parity.
void Gp_ShakeTask(Task* arg0)
{
    s32 packed;
    s32 lo;
    s32 scaled;
    s32 val;

    packed = arg0->spawnArg2.value;
    lo     = packed & 0xFF;

    switch (arg0->state) {
        case 0:
            arg0->spawnArg1.value = -lo;
            arg0->state++;
            break;
        case 1:
            if (lo < arg0->spawnArg1.value) {
                displaySetShakeY(0);
                taskKill(arg0);
            } else {
                val             = lo - ABS(arg0->spawnArg1.value);
                scaled          = val * (packed >> 8);
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                val             = (scaled * (s32)(gRandomLcgState >> 16)) / lo >> 16;
                if (arg0->spawnArg1.value & 1) {
                    val = ABS(val);
                } else {
                    val = -ABS(val);
                }
                displaySetShakeY(val);
                arg0->spawnArg1.value++;
            }
            break;
    }
}
