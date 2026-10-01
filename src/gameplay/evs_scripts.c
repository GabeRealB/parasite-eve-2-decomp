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
#include "scene.h"
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

/// 4-byte volume-fade payload at `Task::spawnArg2` for `Gp_VolFadeTask`.
/// `field_0` is the target volume passed to `Snd_ApplyVolumeTable`.
/// `field_2` is the fade duration in frames (`0` applies immediately).
typedef struct _GpVolFade {
    /* 0x0 */ u16 field_0; // target volume
    /* 0x2 */ u16 field_2; // duration
} GpVolFade;
STATIC_ASSERT_SIZEOF(GpVolFade, 4);

/// 0xC-byte Type-A sound-param fade at `Task::spawnArg2` for `Gp_SndFadeTask`
/// (bank 9 type 0xE; live instance `D_801156E0`). `field_0` is the sound id
/// passed to `SndEvt_EnqueueTypeA`. `field_4` is the start/current param
/// (snapshotted into `D_801156C4`); `field_6` is the target; `field_8` is
/// the duration in frames (`0` applies `field_6` immediately). Completing
/// or instant-applying the fade clears `D_8010FBE8`.
typedef struct _GpSndFade {
    /* 0x0 */ s32  field_0; // sound id
    /* 0x4 */ u16  field_4; // start / current param
    /* 0x6 */ u16  field_6; // target param
    /* 0x8 */ u16  field_8; // duration
    /* 0xA */ byte pad_A[2];
} GpSndFade;
STATIC_ASSERT_SIZEOF(GpSndFade, 0xC);

/// 0x34-byte event-script interpreter state stored at `Task::work` for the
/// script task. `pc` is the current command, `wait` the frame countdown set by
/// op 4, `stack` / `sp` the call stack for ops 44 / 45. `msgTask` is the message
/// task spawned by op 2 / 24 and `fadeTask` the fade task spawned by op 35.
typedef struct _GpEvsState {
    /* 0x00 */ EvsCommand* pc;
    /* 0x04 */ s32         wait;
    /* 0x08 */ EvsCommand* stack[8];
    /* 0x28 */ s32         sp;
    /* 0x2C */ Task*       msgTask;
    /* 0x30 */ Task*       fadeTask;
} GpEvsState;
STATIC_ASSERT_SIZEOF(GpEvsState, 0x34);

/// Extended script work allocation created by Gp_ScriptInit.
typedef struct _GpState34 {
    /* 0x00 */ GpState18 script;
    /* 0x18 */ byte      pad_18[0x10];
    /* 0x28 */ s32       field_28;
    /* 0x2C */ s32       field_2C;
    /* 0x30 */ s32       field_30;
} GpState34;
STATIC_ASSERT_SIZEOF(GpState34, 0x34);

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

GpEvsAddress D_801156D0;

ScreenFade D_801156D4;

ScreenFade D_801156D8;

GpVolFade D_801156DC;

GpSndFade D_801156E0;

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

static void Gp_ScriptTaskState1(Task* arg0)
{
    GpEvsAddress         continuation;
    GpEvsState*          st;
    GpEvsState*          st2;
    AnimationPlayRequest rec;
    SVECTOR              vec;
    TextDrawReq          req;
    Task*                slot;
    StageMusicParams*    pair;
    s32                  mode;

    st = (GpEvsState*)arg0->work;
    if (D_801156F9 != 0) {
        return;
    }

    if (gDisplayState.demoScene != DISPLAY_DEMO_NONE && Pad_CheckFlag800() != 0 && gDisplayState.pendingMode == DISPLAY_MODE_NONE) {
        gDisplayState.gameMode = DISPLAY_GAME_RESTART;
    }

    if (Pad_CheckFlag800() != 0 && D_801156D0.address != 0 && gDisplayState.pendingMode == DISPLAY_MODE_NONE && D_801156F0 == 0) {
        if (D_801156F4.sceneKey != NULL) {
            CdCmd_CancelReplaceAndActivate();
        }
        D_801156A4               = 0;
        continuation.address     = D_801156D0.address;
        st->wait                 = 0;
        D_801156D0.address       = 0;
        st->pc                   = continuation.commands;
        D_80115688               = 1;
        gGameSession->evtSkipped = 1;
        if (D_801156CC != 0) {
            return;
        }
        SndEvt_EnqueueType7(0x80000000, 0x10);
        return;
    }
    if (D_801156F0 != 0) {
        D_801156F0--;
    }

    if (st->wait != 0) {
        st->wait--;
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
        switch (st->pc->opcode) {
            case EVENT_SCRIPT_OPCODE_SEND_MESSAGE:
                // Resolve scene recipients before forwarding the untouched payload words.
                if (st->pc->operand0.value == GAME_TASK_SLOT_SCENE) {
                    slot = gameGetTaskSlot(GAME_TASK_SLOT_SCENE);
                    if (st->pc->operand1.value != EVENT_SCRIPT_MESSAGE_SELECT_SCENE_MANAGER) {
                        Gp_DispatchMsgReply(slot, SCENE_MESSAGE_FIND_PLACED_ACTOR,
                                            (st->pc->operand1.value << ENEMY_PLACE_INDEX_SHIFT) | (gGameSession->location.loc.stage << ENEMY_PLACE_STAGE_SHIFT) | gGameSession->location.loc.area,
                                            &slot);
                    }
                } else if (st->pc->operand0.value == EVENT_SCRIPT_MESSAGE_TARGET_OTHER_SCENE_CHILD) {
                    slot = gameGetTaskSlot(GAME_TASK_SLOT_SCENE);
                    Gp_DispatchMsgReply(slot, SCENE_MESSAGE_FIND_OTHER_CHILD, st->pc->operand1.value, &slot);
                } else {
                    slot = gameGetTaskSlot(st->pc->operand0.value);
                }
                if (slot != NULL) {
                    Gp_DispatchMsg(slot, st->pc->operand2.value, st->pc->operand3.message.value, st->pc->operand4.message.value);
                }
                break;

            case EVENT_SCRIPT_OPCODE_END:
                if (D_8010FBE0 != NULL) {
                    Task_CallExit(D_8010FBE0);
                    D_8010FBE0 = NULL;
                }
                D_801156F4.sceneKey      = NULL;
                gGameSession->eventState = 0;
                if (arg0->spawnArg1.value == 0) {
                    Gp_DispatchMsg(gameGetTaskSlot(GAME_TASK_SLOT_CAP_CONTROL), 0xFA5, 0, 0);
                }
                arg0->state++;
                Display_ReleaseRef();
                if (D_801156CA != 0) {
                    Gp_RestoreStreamRng();
                }
                D_8011569C = 0;
                return;

            case EVENT_SCRIPT_OPCODE_START_FLASH:
                st->msgTask = Task_Spawn(1, 0x19, st->pc->operand0.value, st->pc->operand1.value);
                break;

            case EVENT_SCRIPT_OPCODE_SET_DIRTY_VIEW:
                gGameSession->viewDirty = 1;
                /* fallthrough */

            case EVENT_SCRIPT_OPCODE_SET_VIEW:
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = (u8)st->pc->operand0.value;
                break;

            case EVENT_SCRIPT_OPCODE_WAIT_FRAMES:
                // Advance before yielding; even a zero countdown separates task updates.
                D_801156CB = 1;
                st->wait   = st->pc->operand0.value;
                st->pc     = st->pc + 1;
                return;

            case EVENT_SCRIPT_OPCODE_CANCEL_PRIMARY_FADE:
                if (st->msgTask != NULL) {
                    taskKill(st->msgTask);
                    st->msgTask = NULL;
                }
                break;

            case EVENT_SCRIPT_OPCODE_RESTORE_HUD:
                Gp_DispatchMsg(gameGetTaskSlot(GAME_TASK_SLOT_CAP_CONTROL), 0xFA8, 0, 0);
                break;

            case EVENT_SCRIPT_OPCODE_SET_EVENT_STATE:
                gGameSession->eventState = (u8)st->pc->operand0.value;
                break;

            case EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE:
                D_801156CB  = 1;
                D_801156A4 |= 0x40;
                st->pc      = st->pc + 1;
                return;

            case EVENT_SCRIPT_OPCODE_WAIT_ANIMATION:
                if (Gp_DispatchMsg(gameGetTaskSlot(st->pc->operand0.value), 0x3ED, 0, 0) == 0) {
                    break;
                }
                return;

            case EVENT_SCRIPT_OPCODE_WAIT_ACTOR_ACTION:
                if (Gp_DispatchMsg(gameGetTaskSlot(st->pc->operand0.value), 0x3F0, 0, 0) == 0) {
                    break;
                }
                return;

            case EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION:
                slot = gameGetTaskSlot(st->pc->operand0.value);
                // Resolve the weapon bank in a copy of the borrowed request.
                rec = *st->pc->operand3.animation;
                if (st->pc->operand0.value == 3) {
                    Gp_PlayerWeaponId(&rec.source.index);
                } else {
                    Gp_AllyAnimId(&rec.source.index);
                }
                if (slot != NULL) {
                    TASK_MESSAGE_DISPATCH_POINTER(slot, st->pc->operand2.value, &rec, st->pc->operand4.value);
                }
                break;

            case EVENT_SCRIPT_OPCODE_RESTORE_VIEW:
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = D_801156F8;
                break;

            case EVENT_SCRIPT_OPCODE_SELECT_SCENE:
                D_801156F4.sceneKey = st->pc->operand0.sceneKey;
                Gp_DispatchMsg(gameGetTaskSlot(GAME_TASK_SLOT_CAP_CONTROL), 0xFA6, D_801156F4.value, 0);
                if (D_801156F4.sceneKey != NULL) {
                    D_801156CA = 1;
                }
                break;

            case EVENT_SCRIPT_OPCODE_CALLBACK:
                st->pc->operand0.callback(st->pc->operand1.value);
                break;

            case EVENT_SCRIPT_OPCODE_START_VIBRATION:
                Gp_SpawnScript18(st->pc->operand0.padCommands, st->pc->operand1.vibrationSegments);
                break;

            case EVENT_SCRIPT_OPCODE_START_SOUND:
                SndEvt_EnqueueType6(st->pc->operand0.value, (s8)st->pc->operand1.value, (s8)st->pc->operand2.value);
                D_801156E0.field_4 = (u16)st->pc->operand2.value;
                break;

            case EVENT_SCRIPT_OPCODE_STOP_SOUND:
                SndEvt_EnqueueType7(st->pc->operand0.value, (u16)st->pc->operand1.value);
                break;

            case EVENT_SCRIPT_OPCODE_SET_FRAMEBUFFER_BLEND:
                if (st->pc->operand0.value != 0) {
                    D_8010FBE0 = Task_Spawn(1, 0x2D, 0, 0);
                } else if (D_8010FBE0 != NULL) {
                    Task_CallExit(D_8010FBE0);
                    D_8010FBE0 = NULL;
                }
                break;

            case EVENT_SCRIPT_OPCODE_START_AREA_MUSIC:
                if (D_801156C8 == 0) {
                    Stage_RequestFromAreaTable((s16)st->pc->operand0.value);
                    D_801156C8 = 1;
                }
                break;

            case EVENT_SCRIPT_OPCODE_STOP_AREA_MUSIC:
                Stage_RequestMidiFromMap((s16)st->pc->operand0.value);
                break;

            case EVENT_SCRIPT_OPCODE_REQUEST_SCENE_MUSIC:
                if (D_801156C9 != 0) {
                    break;
                }
                pair                                                = &gStageMusicParams;
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = (u8)st->pc->operand0.value;
                D_801156C9                                          = 1;
                pair->fadeFrames                                    = (u16)st->pc->operand1.value;
                gStageMusicLoadState                                = 0;
                pair->unusedCommandArg                              = (u16)st->pc->operand2.value;
                Task_SpawnFromTable(&Stage_MusicTaskDesc, 0, 0, 0);
                break;

            case EVENT_SCRIPT_OPCODE_WAIT_MUSIC_LOAD:
                if (gStageMusicLoadState == 0) {
                    return;
                }
                break;

            case EVENT_SCRIPT_OPCODE_SHAKE_SCREEN:
                Task_Spawn(9, 0xC, 0, (st->pc->operand0.value << 8) | st->pc->operand1.value);
                break;

            case EVENT_SCRIPT_OPCODE_CLEANUP_SCENE:
                if (arg0->spawnArg1.value == 0) {
                    Gp_DispatchMsg(gameGetTaskSlot(GAME_TASK_SLOT_CAP_CONTROL), 0xFA5, 0, 0);
                }
                arg0->spawnArg1.value = 1;
                Gp_AbortCap();
                Gp_MsgPlayer3F3(1);
                Gp_DispatchMsg(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), 0x401, 0, 0);
                if (D_8010FBE0 != NULL) {
                    Task_CallExit(D_8010FBE0);
                    D_8010FBE0 = NULL;
                }
                st2 = (GpEvsState*)arg0->work;
                if (st2->fadeTask != NULL) {
                    if (D_801156D8.phase != SCREEN_FADE_DONE) {
                        taskKill(st2->fadeTask);
                    }
                    st2->fadeTask = NULL;
                }
                break;

            case EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE:
                if (st->msgTask != NULL) {
                    break;
                }
                D_801156D4.blend = (u8)st->pc->operand0.value;
                D_801156D4.phase = SCREEN_FADE_RUNNING;
                if (st->pc->operand1.value == 0) {
                    D_801156D4.rampFrames = 7;
                } else {
                    D_801156D4.rampFrames = (u16)st->pc->operand1.value;
                }
                st->msgTask = Task_SpawnPtr(1, 0x31, 0, &D_801156D4);
                break;

            case EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE:
                D_801156D4.phase = SCREEN_FADE_RETURN;
                break;

            case EVENT_SCRIPT_OPCODE_FADE_VOLUME:
                if (D_8010FBE4 != NULL) {
                    taskKill(D_8010FBE4);
                }
                D_801156DC.field_0 = (u16)st->pc->operand0.value;
                D_801156DC.field_2 = (u16)st->pc->operand1.value;
                D_8010FBE4         = Task_SpawnPtr(9, 0xD, 0, &D_801156DC);
                break;

            case EVENT_SCRIPT_OPCODE_FADE_SOUND_ATTENUATION:
                if (D_8010FBE8 != NULL) {
                    taskKill(D_8010FBE8);
                }
                D_801156E0.field_0 = st->pc->operand0.value;
                D_801156E0.field_6 = (u16)st->pc->operand1.value;
                D_801156E0.field_8 = (u16)st->pc->operand2.value;
                D_8010FBE8         = Task_SpawnPtr(9, 0xE, 0, &D_801156E0);
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
                vec.vx = st->pc->operand0.value * EVENT_SCRIPT_LIGHT_VALUE_SCALE;
                vec.vy = st->pc->operand1.value * EVENT_SCRIPT_LIGHT_VALUE_SCALE;
                vec.vz = st->pc->operand2.value * EVENT_SCRIPT_LIGHT_VALUE_SCALE;
                Gp_SetOverrideVec(&vec);
                break;

            case EVENT_SCRIPT_OPCODE_SET_LIGHT_SCALE:
                if (st->pc->operand0.value != 0) {
                    vec.vx = st->pc->operand0.value * EVENT_SCRIPT_LIGHT_VALUE_SCALE;
                    vec.vy = st->pc->operand0.value * EVENT_SCRIPT_LIGHT_VALUE_SCALE;
                    vec.vz = st->pc->operand0.value * EVENT_SCRIPT_LIGHT_VALUE_SCALE;
                    Gp_SetOverrideVec2(&vec);
                } else {
                    Gp_SetOverrideVec2(NULL);
                }
                break;

            case EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB:
                Gp_SetOverrideVec(NULL);
                break;

            case EVENT_SCRIPT_OPCODE_FINISH_SCENE_STREAM:
                Gp_RestoreStreamRng();
                break;

            case EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE:
                if (st->fadeTask != NULL && D_801156D8.phase != SCREEN_FADE_DONE) {
                    break;
                }
                D_801156D8.blend = (u8)st->pc->operand0.value;
                D_801156D8.phase = SCREEN_FADE_RUNNING;
                if (st->pc->operand1.value == 0) {
                    D_801156D8.rampFrames = 7;
                } else {
                    D_801156D8.rampFrames = (u16)st->pc->operand1.value;
                }
                st->fadeTask = Task_SpawnPtr(1, 0x31, st->pc->operand2.value, &D_801156D8);
                break;

            case EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE:
                D_801156D8.phase = SCREEN_FADE_RETURN;
                if (st->pc->operand0.value != 0) {
                    D_801156D8.rampFrames = (u16)st->pc->operand0.value;
                }
                break;

            case EVENT_SCRIPT_OPCODE_CANCEL_SECONDARY_FADE:
                st2 = (GpEvsState*)arg0->work;
                if (st2->fadeTask != NULL) {
                    if (D_801156D8.phase != SCREEN_FADE_DONE) {
                        taskKill(st2->fadeTask);
                    }
                    st2->fadeTask = NULL;
                }
                break;

            case EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS:
                mode = st->pc->operand0.value;
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
                mode = st->pc->operand0.value;
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
                D_8011569C = (u8)st->pc->operand0.value;
                break;

            case EVENT_SCRIPT_OPCODE_START_STAGE_SOUND:
                Gp_EnqueueStageSnd6(st->pc->operand0.value, (s8)st->pc->operand1.value, (s8)st->pc->operand2.value);
                D_801156E0.field_4 = (u16)st->pc->operand2.value;
                break;

            // Each control transfer compensates for the common advance below.
            case EVENT_SCRIPT_OPCODE_JUMP:
                st->pc = st->pc->operand0.commands - 1;
                break;

            case EVENT_SCRIPT_OPCODE_CALL_SCRIPT:
                st->stack[st->sp] = st->pc + 1;
                st->sp            = st->sp + 1;
                st->pc            = st->pc->operand0.commands - 1;
                break;

            case EVENT_SCRIPT_OPCODE_RETURN:
                st->sp = st->sp - 1;
                st->pc = st->stack[st->sp] - 1;
                break;

            case EVENT_SCRIPT_OPCODE_SET_SKIP_TARGET:
                D_801156D0.commands = st->pc->operand0.commands;
                break;

            case EVENT_SCRIPT_OPCODE_SET_SKIP_KEEP_SOUND:
                D_801156CC = (u8)st->pc->operand0.value;
                break;

            case EVENT_SCRIPT_OPCODE_SAVE_VIEW:
                D_801156F8 = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view;
                break;
        }
        D_801156CB = 1;
        st->pc     = st->pc + 1;
    }
}

void Gp_VolFadeTask(Task* arg0)
{
    GpVolFade* fade;
    s32        volume;

    fade = arg0->spawnArg2.pointer;
    switch (arg0->state) {
        case 0:
            if (fade->field_2 == 0) {
                Snd_ApplyVolumeTable(fade->field_0);
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
            volume = (D_801156C0 * (fade->field_2 - D_801156C2) + fade->field_0 * D_801156C2) / fade->field_2;
            Snd_ApplyVolumeTable(volume & 0xFFFF);
            if (D_801156C2 == fade->field_2) {
                taskKill(arg0);
                D_8010FBE4 = 0;
            }
            break;
    }
}

void Gp_SndFadeTask(Task* arg0)
{
    GpSndFade* fade;
    s32        volume;

    fade = arg0->spawnArg2.pointer;
    switch (arg0->state) {
        case 0:
            if (fade->field_8 == 0) {
                SndEvt_EnqueueTypeA(fade->field_0, 0, (s8)fade->field_6);
                fade->field_4 = fade->field_6;
                taskKill(arg0);
                D_8010FBE8 = 0;
            } else {
                D_801156C6 = 0;
                D_801156C4 = fade->field_4;
            }
            arg0->state++;
            break;
        case 1:
            D_801156C6++;
            volume = (D_801156C4 * (fade->field_8 - D_801156C6) + fade->field_6 * D_801156C6) / fade->field_8;
            SndEvt_EnqueueTypeA(fade->field_0, 0, (s8)volume);
            fade->field_4 = volume;
            if (D_801156C6 == fade->field_8) {
                taskKill(arg0);
                D_8010FBE8 = 0;
            }
            break;
    }
}

void func_800E8614(GpEvsAddress arg0, s32 arg1)
{
    func_800E8634(arg0, arg1, 0);
}

void func_800E8634(GpEvsAddress arg0, s32 arg1, GpEvsAddress arg2)
{
    gGameSession->eventState = 1;
    gGameSession->evtSkipped = 0;
    D_8010FBE0               = 0;
    D_8010FBE4               = 0;
    D_801156D0.address       = arg2.address;
    D_801156C9               = 0;
    D_801156CC               = 0;
    D_801156F0               = 5;
    D_801156CD               = 0;
    D_801156CE               = 0;
    D_801156F8               = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view;
    D_801156EC               = gPlayerStatus.weapon;
    SndEvt_EnqueueType7(0xFF0D, 1);
    Task_Spawn(9, 7, arg1, arg0.address);
}

Task* Gp_LookupSlot4(s32 arg0)
{
    Task* out;

    arg0 = (arg0 << ENEMY_PLACE_INDEX_SHIFT) | (gGameSession->location.loc.stage << ENEMY_PLACE_STAGE_SHIFT) | gGameSession->location.loc.area;
    Gp_DispatchMsgReply(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), 0x7D0, arg0, &out);
    return out;
}

static void Gp_ScriptInit(Task* arg0)
{
    GpState34*    mem;
    PadScriptCmd* script;

    mem = memCalloc(0x34, 0);
    if (mem == NULL) {
        taskKill(arg0);
        return;
    }
    D_801156F9          = 0;
    D_801156F4.sceneKey = 0;
    Display_AcquireRef();
    script              = arg0->spawnArg2.pointer;
    D_801156A4          = 0;
    arg0->work          = mem;
    mem->script.field_4 = 0;
    D_801156C8          = 0;
    mem->script.field_0 = script;
    D_801156CA          = 0;
    if (arg0->spawnArg1.value == 0) {
        Gp_DispatchMsg(gameGetTaskSlot(GAME_TASK_SLOT_CAP_CONTROL), 0xFA4, 0, 0);
    }
    D_801156CB    = 1;
    mem->field_2C = 0;
    mem->field_30 = 0;
    mem->field_28 = 0;
    D_8011569C    = 0;
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
