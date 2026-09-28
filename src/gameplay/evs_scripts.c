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
#include "gameplay/sound_params.h"
#include "gameplay/world_coords.h"
#include "world_coords.h"

#include "main/display.h"
#include "main/fs.h"
#include "main/gamemain.h"
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
    /* 0x00 */ GpEvsCmd* pc;
    /* 0x04 */ s32       wait;
    /* 0x08 */ GpEvsCmd* stack[8];
    /* 0x28 */ s32       sp;
    /* 0x2C */ Task*     msgTask;
    /* 0x30 */ Task*     fadeTask;
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

GpFadeWork D_801156D4;

GpFadeWork D_801156D8;

GpVolFade D_801156DC;

GpSndFade D_801156E0;

s32 D_801156EC;

u8 D_801156F0;

// EVS overlay selection, saved view and pause gate shared with CAP/room tasks.
GpOverlayIds* D_801156F4;

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

extern s16 D_8007A396;

extern u16 D_8007A39C;

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

static void Gp_ScriptTaskState1(Task* arg0)
{
    GpEvsAddress continuation;
    GpEvsState*  st;
    GpEvsState*  st2;
    GpAnimArg    rec;
    SVECTOR      vec;
    TextDrawReq  req;
    Task*        slot;
    GpSndParam*  pair;
    s32          mode;
    GpEvsCmd*    cmd;

    st = (GpEvsState*)arg0->work;
    if (D_801156F9 != 0) {
        return;
    }

    if (gDisplayState.demoScene != 0 && Pad_CheckFlag800() != 0 && gDisplayState.pendingMode == 0) {
        gDisplayState.gameMode = 1;
    }

    if (Pad_CheckFlag800() != 0 && D_801156D0.address != 0 && gDisplayState.pendingMode == 0 && D_801156F0 == 0) {
        if (D_801156F4 != NULL) {
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
        if (Mc_SaveData[0].demoScene == 9) {
            req.x          = -0x8C;
            req.y          = 0x50;
            req.otIndex    = 4;
            req.field_8    = 0x808008;
            req.glyphTable = 0;
            req.centerMode = 0;
            req.field_E    = 0;
            func_8002E53C(&req, Gp_StrDemoWait);
        }
        return;
    }

    if (D_801156A4 & 0x40) {
        if (Mc_SaveData[0].demoScene == 9) {
            req.x          = -0x8C;
            req.y          = 0x50;
            req.otIndex    = 4;
            req.field_8    = 0x808008;
            req.glyphTable = 0;
            req.centerMode = 0;
            req.field_E    = 0;
            func_8002E53C(&req, Gp_StrDemoPause);
        }
        return;
    }

    while (1) {
        switch (st->pc->op) {
            case 1:
                if (st->pc->arg0 == 4) {
                    slot = gameGetPtrSlot(4);
                    if (st->pc->arg1 != -1) {
                        Gp_DispatchMsgReply(slot, 0x7D0,
                                            (st->pc->arg1 << 12) | (gGameSession->at4.loc.stage << 8) | gGameSession->at4.loc.area,
                                            &slot);
                    }
                } else if (st->pc->arg0 == -1) {
                    slot = gameGetPtrSlot(4);
                    Gp_DispatchMsgReply(slot, 0x7D8, st->pc->arg1, &slot);
                } else {
                    slot = gameGetPtrSlot(st->pc->arg0);
                }
                if (slot != NULL) {
                    Gp_DispatchMsg(slot, st->pc->arg2, st->pc->arg3, st->pc->arg4);
                }
                break;

            case -1:
                if (D_8010FBE0 != NULL) {
                    Task_CallExit(D_8010FBE0);
                    D_8010FBE0 = NULL;
                }
                D_801156F4               = NULL;
                gGameSession->eventState = 0;
                if (arg0->spawnArg1 == 0) {
                    Gp_DispatchMsg(gameGetPtrSlot(6), 0xFA5, 0, 0);
                }
                arg0->state++;
                Display_ReleaseRef();
                if (D_801156CA != 0) {
                    Gp_RestoreStreamRng();
                }
                D_8011569C = 0;
                return;

            case 2:
                st->msgTask = Task_Spawn(1, 0x19, st->pc->arg0, st->pc->arg1);
                break;

            case 48:
                gGameSession->viewDirty = 1;
                /* fallthrough */

            case 3:
                Mc_SaveData[0].at4.loc.view = (u8)st->pc->arg0;
                break;

            case 4:
                D_801156CB = 1;
                st->wait   = st->pc->arg0;
                st->pc     = st->pc + 1;
                return;

            case 5:
                if (st->msgTask != NULL) {
                    taskKill(st->msgTask);
                    st->msgTask = NULL;
                }
                break;

            case 6:
                Gp_DispatchMsg(gameGetPtrSlot(6), 0xFA8, 0, 0);
                break;

            case 7:
                gGameSession->eventState = (u8)st->pc->arg0;
                break;

            case 9:
                D_801156CB  = 1;
                D_801156A4 |= 0x40;
                st->pc      = st->pc + 1;
                return;

            case 8:
                if (Gp_DispatchMsg(gameGetPtrSlot(st->pc->arg0), 0x3ED, 0, 0) == 0) {
                    break;
                }
                return;

            case 29:
                if (Gp_DispatchMsg(gameGetPtrSlot(st->pc->arg0), 0x3F0, 0, 0) == 0) {
                    break;
                }
                return;

            case 10:
                slot = gameGetPtrSlot(st->pc->arg0);
                rec  = *(GpAnimArg*)st->pc->arg3;
                if (st->pc->arg0 == 3) {
                    Gp_PlayerWeaponId(&rec.animBlock.index);
                } else {
                    Gp_AllyAnimId(&rec.animBlock.index);
                }
                if (slot != NULL) {
                    Gp_DispatchMsgPtr(slot, st->pc->arg2, &rec, st->pc->arg4);
                }
                break;

            case 11:
                Mc_SaveData[0].at4.loc.view = D_801156F8;
                break;

            case 12:
                D_801156F4 = (GpOverlayIds*)st->pc->arg0;
                /* The message ABI carries this object address in one 32-bit word. */
                Gp_DispatchMsg(gameGetPtrSlot(6), 0xFA6, (s32)D_801156F4, 0);
                if (D_801156F4 != NULL) {
                    D_801156CA = 1;
                }
                break;

            case 13:
                ((void (*)(s32))st->pc->arg0)(st->pc->arg1);
                break;

            case 14:
                Gp_SpawnScript18(st->pc->arg0, st->pc->arg1);
                break;

            case 15:
                SndEvt_EnqueueType6(st->pc->arg0, (s8)st->pc->arg1, (s8)st->pc->arg2);
                D_801156E0.field_4 = (u16)st->pc->arg2;
                break;

            case 16:
                SndEvt_EnqueueType7(st->pc->arg0, (u16)st->pc->arg1);
                break;

            case 17:
                if (st->pc->arg0 != 0) {
                    D_8010FBE0 = Task_Spawn(1, 0x2D, 0, 0);
                } else if (D_8010FBE0 != NULL) {
                    Task_CallExit(D_8010FBE0);
                    D_8010FBE0 = NULL;
                }
                break;

            case 18:
                if (D_801156C8 == 0) {
                    Stage_RequestFromAreaTable((s16)st->pc->arg0);
                    D_801156C8 = 1;
                }
                break;

            case 19:
                Stage_RequestMidiFromMap((s16)st->pc->arg0);
                break;

            case 20:
                if (D_801156C9 != 0) {
                    break;
                }
                pair                      = (GpSndParam*)&D_8007A39C;
                Mc_SaveData[0].sceneEvent = (u8)st->pc->arg0;
                D_801156C9                = 1;
                pair->field_0             = (u16)st->pc->arg1;
                gStageMusicLoadState      = 0;
                pair->field_2             = (u16)st->pc->arg2;
                Task_SpawnFromTable(&D_80062774, 0, 0, 0);
                break;

            case 21:
                if (gStageMusicLoadState == 0) {
                    return;
                }
                break;

            case 22:
                Task_Spawn(9, 0xC, 0, (st->pc->arg0 << 8) | st->pc->arg1);
                break;

            case 23:
                if (arg0->spawnArg1 == 0) {
                    Gp_DispatchMsg(gameGetPtrSlot(6), 0xFA5, 0, 0);
                }
                arg0->spawnArg1 = 1;
                Gp_AbortCap();
                Gp_MsgPlayer3F3(1);
                Gp_DispatchMsg(gameGetPtrSlot(3), 0x401, 0, 0);
                if (D_8010FBE0 != NULL) {
                    Task_CallExit(D_8010FBE0);
                    D_8010FBE0 = NULL;
                }
                st2 = (GpEvsState*)arg0->work;
                if (st2->fadeTask != NULL) {
                    if (D_801156D8.field_1 != 2) {
                        taskKill(st2->fadeTask);
                    }
                    st2->fadeTask = NULL;
                }
                break;

            case 24:
                if (st->msgTask != NULL) {
                    break;
                }
                D_801156D4.field_0 = (u8)st->pc->arg0;
                D_801156D4.field_1 = 0;
                if (st->pc->arg1 == 0) {
                    D_801156D4.field_2 = 7;
                } else {
                    D_801156D4.field_2 = (u16)st->pc->arg1;
                }
                st->msgTask = Task_SpawnPtr(1, 0x31, 0, &D_801156D4);
                break;

            case 25:
                D_801156D4.field_1 = 1;
                break;

            case 26:
                if (D_8010FBE4 != NULL) {
                    taskKill(D_8010FBE4);
                }
                D_801156DC.field_0 = (u16)st->pc->arg0;
                D_801156DC.field_2 = (u16)st->pc->arg1;
                D_8010FBE4         = Task_SpawnPtr(9, 0xD, 0, &D_801156DC);
                break;

            case 27:
                if (D_8010FBE8 != NULL) {
                    taskKill(D_8010FBE8);
                }
                D_801156E0.field_0 = st->pc->arg0;
                D_801156E0.field_6 = (u16)st->pc->arg1;
                D_801156E0.field_8 = (u16)st->pc->arg2;
                D_8010FBE8         = Task_SpawnPtr(9, 0xE, 0, &D_801156E0);
                break;

            case 28:
                Gpu_ResetGraphAndOt();
                Tmd_AllocMissingBuffers();
                break;

            case 30:
                CdCmd_EnqueueOverlay81();
                break;

            case 31:
                CdCmd_EnqueueReplaceOverlay82();
                break;

            case 32:
                vec.vx = st->pc->arg0 * 16;
                vec.vy = st->pc->arg1 * 16;
                vec.vz = st->pc->arg2 * 16;
                Gp_SetOverrideVec(&vec);
                break;

            case 40:
                if (st->pc->arg0 != 0) {
                    vec.vx = st->pc->arg0 * 16;
                    vec.vy = st->pc->arg0 * 16;
                    vec.vz = st->pc->arg0 * 16;
                    Gp_SetOverrideVec2(&vec);
                } else {
                    Gp_SetOverrideVec2(NULL);
                }
                break;

            case 33:
                Gp_SetOverrideVec(NULL);
                break;

            case 34:
                Gp_RestoreStreamRng();
                break;

            case 35:
                if (st->fadeTask != NULL && D_801156D8.field_1 != 2) {
                    break;
                }
                D_801156D8.field_0 = (u8)st->pc->arg0;
                D_801156D8.field_1 = 0;
                if (st->pc->arg1 == 0) {
                    D_801156D8.field_2 = 7;
                } else {
                    D_801156D8.field_2 = (u16)st->pc->arg1;
                }
                st->fadeTask = Task_SpawnPtr(1, 0x31, st->pc->arg2, &D_801156D8);
                break;

            case 36:
                D_801156D8.field_1 = 1;
                if (st->pc->arg0 != 0) {
                    D_801156D8.field_2 = (u16)st->pc->arg0;
                }
                break;

            case 37:
                st2 = (GpEvsState*)arg0->work;
                if (st2->fadeTask != NULL) {
                    if (D_801156D8.field_1 != 2) {
                        taskKill(st2->fadeTask);
                    }
                    st2->fadeTask = NULL;
                }
                break;

            case 38:
                mode = st->pc->arg0;
                if (mode == 0 || mode == 2) {
                    if (D_801156CD != 0) {
                        Player_Status.weapon = D_801156EC;
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

            case 39:
                mode = st->pc->arg0;
                if (mode == 0 || mode == 2) {
                    D_801156CD = 1;
                    Gp_KillPlayerEffs();
                    Gp_MsgPlayerWeapon(0);
                }
                if ((u32)(mode - 1) < 2U) {
                    D_801156CE = 1;
                    slot       = gameGetPtrSlot(0xA);
                    if (slot != NULL) {
                        Gp_EndPlayerActorTask(slot);
                        Gp_MsgAllyWeapon(0);
                    }
                }
                break;

            case 41:
                D_8011569C = (u8)st->pc->arg0;
                break;

            case 42:
                Gp_EnqueueStageSnd6(st->pc->arg0, (s8)st->pc->arg1, (s8)st->pc->arg2);
                D_801156E0.field_4 = (u16)st->pc->arg2;
                break;

            case 43:
                st->pc = (GpEvsCmd*)st->pc->arg0 - 1;
                break;

            case 44:
                st->stack[st->sp] = st->pc + 1;
                st->sp            = st->sp + 1;
                st->pc            = (GpEvsCmd*)st->pc->arg0 - 1;
                break;

            case 45:
                st->sp = st->sp - 1;
                st->pc = st->stack[st->sp] - 1;
                break;

            case 46:
                D_801156D0.address = st->pc->arg0;
                break;

            case 47:
                D_801156CC = (u8)st->pc->arg0;
                break;

            case 49:
                D_801156F8 = Mc_SaveData[0].at4.loc.view;
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

    fade = arg0->spawnArg2;
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

    fade = arg0->spawnArg2;
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

void func_800E8614(s32 arg0, s32 arg1)
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
    D_801156F8               = Mc_SaveData[0].at4.loc.view;
    D_801156EC               = Player_Status.weapon;
    SndEvt_EnqueueType7(0xFF0D, 1);
    Task_Spawn(9, 7, arg1, arg0.address);
}

Task* Gp_LookupSlot4(s32 arg0)
{
    Task* out;

    arg0 = (arg0 << 12) | (gGameSession->at4.loc.stage << 8) | gGameSession->at4.loc.area;
    Gp_DispatchMsgReply(gameGetPtrSlot(4), 0x7D0, arg0, &out);
    return out;
}

static void Gp_ScriptInit(Task* arg0)
{
    GpState34*   mem;
    GpScriptCmd* script;

    mem = memCalloc(0x34, 0);
    if (mem == NULL) {
        taskKill(arg0);
        return;
    }
    D_801156F9 = 0;
    D_801156F4 = 0;
    Display_AcquireRef();
    script              = arg0->spawnArg2;
    D_801156A4          = 0;
    arg0->work          = (TaskIdMap*)mem;
    mem->script.field_4 = 0;
    D_801156C8          = 0;
    mem->script.field_0 = script;
    D_801156CA          = 0;
    if (arg0->spawnArg1 == 0) {
        Gp_DispatchMsg(gameGetPtrSlot(6), 0xFA4, 0, 0);
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
            arg0->killCountdown = 0;
            arg0->spawnArg1     = -1;
            arg0->state++;
            break;
        case 1:
            arg0->killCountdown = (u16)arg0->killCountdown - (u16)arg0->spawnArg1;
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
/// Each frame an LCG (`Gp_LcgState`) scales the remaining count into
/// `Display_ClampField126`, flipping sign on `spawnArg1` parity.
void Gp_ShakeTask(Task* arg0)
{
    s32 packed;
    s32 lo;
    s32 scaled;
    s32 val;

    packed = (s32)arg0->spawnArg2;
    lo     = packed & 0xFF;

    switch (arg0->state) {
        case 0:
            arg0->spawnArg1 = -lo;
            arg0->state++;
            break;
        case 1:
            if (lo < arg0->spawnArg1) {
                Display_ClampField126(0);
                taskKill(arg0);
            } else {
                val         = lo - ABS(arg0->spawnArg1);
                scaled      = val * (packed >> 8);
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                val         = (scaled * (s32)(Gp_LcgState >> 16)) / lo >> 16;
                if (arg0->spawnArg1 & 1) {
                    val = ABS(val);
                } else {
                    val = -ABS(val);
                }
                Display_ClampField126(val);
                arg0->spawnArg1++;
            }
            break;
    }
}
