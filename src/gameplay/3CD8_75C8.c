#include "common.h"

#include <psyq/abs.h>
#include <psyq/inline_c.h>
#include <psyq/stdio.h>
#include <psyq/strings.h>

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/stage.h"
#include "main/task.h"
#include "main/text.h"
#include "main/wipsys.h"

extern s32            Gp_LcgState;
extern u8             D_80062734;
extern u16            D_8007A396;
extern u16            D_8007A39C;
extern u8             Gp_StrDemoWait[];
extern u8             Gp_StrDemoPause[];
extern TaskFuncTable3 Gp_ScriptTaskStates;
extern TaskFuncTable3 Gp_Script18States;
extern TaskFuncTable5 Gp_ScriptAStates;
extern TaskFuncTable5 Gp_ScriptBStates;
extern Task*          D_8010FBE0;
extern Task*          D_8010FBE4;
extern Task*          D_8010FBE8;
extern u8             Gp_BtnMap0[];
extern u8             Gp_BtnMap1[];
extern u8             Gp_BtnMap2[];
extern u8             Gp_BtnMap2Alt[];
extern s16            D_80114D08;
extern u8             D_80115688;
extern u8             D_8011569C;
extern u8             D_801156A4;
extern u16            D_801156C0;
extern u16            D_801156C2;
extern u16            D_801156C4;
extern u16            D_801156C6;
extern u8             D_801156C8;
extern u8             D_801156C9;
extern u8             D_801156CA;
extern u8             D_801156CB;
extern u8             D_801156CC;
extern u8             D_801156CD;
extern u8             D_801156CE;
extern s32            D_801156D0;
extern GpStateBD8     D_801156D4;
extern GpStateBD8     D_801156D8;
extern GpVolFade      D_801156DC;
extern GpSndFade      D_801156E0;
extern s32            D_801156EC;
extern u8             D_801156F0;
extern GpOverlayIds*  D_801156F4;
extern u8             D_801156F8;
extern u8             D_801156F9;
extern u8             Gp_PadScriptHalt;
extern u8             Gp_PadHoldHalt;
extern u8             Gp_PadLerpHalt;
extern u8             Gp_MenuLockNow;
extern u8             Gp_MenuLockPrev;
extern u16            Gp_PadSuppressMask;
extern u16            Gp_PadSuppressPrev;
extern u16            Gp_PadSuppressRise;
extern u16            Gp_PadSuppressFall;
extern u16            Gp_PadSuppressRefs;
extern u8             Gp_MenuLockHold;
extern s16            Gp_MenuLockDelay;
extern s16            Gp_PadSuppressTimer;

s32  Gp_AbortCap(void);
s32  Gp_LookupSlot4(s32 arg0);
void Gp_StepScriptA(Task* task);
void Gp_StepScriptB(Task* task);
void Gp_SpawnPadHold(s16 arg0);
void Gp_SpawnPadLerpScaled(s16 arg0, u8 arg1, u8 arg2, s16 arg3);

void Gp_ScriptTaskState1(Task* arg0)
{
    GpEvsState* st;
    GpEvsState* st2;
    GpRec14     rec;
    SVECTOR     vec;
    TextDrawReq req;
    Task*       slot;
    GpSndParam* pair;
    s32         mode;
    GpEvsCmd*   cmd;

    st = (GpEvsState*)arg0->idMap;
    if (D_801156F9 != 0) {
        return;
    }

    if (Display_State.field_12c != 0 && Pad_CheckFlag800() != 0 && Display_State.field_10d == 0) {
        Display_State.field_11e = 1;
    }

    if (Pad_CheckFlag800() != 0 && D_801156D0 != 0 && Display_State.field_10d == 0 && D_801156F0 == 0) {
        if (D_801156F4 != NULL) {
            CdCmd_CancelReplaceAndActivate();
        }
        D_801156A4             = 0;
        st->wait               = 0;
        st->pc                 = (GpEvsCmd*)D_801156D0;
        D_801156D0             = 0;
        D_80115688             = 1;
        Game_Session->field_5F = 1;
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
        if (Mc_SaveData.field_23 == 9) {
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
        if (Mc_SaveData.field_23 == 9) {
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
                    slot = Game_GetPtrSlot(4);
                    if (st->pc->arg1 != -1) {
                        Gp_DispatchMsg(slot, 0x7D0,
                                       (st->pc->arg1 << 12) | (Game_Session->field_7 << 8) | Game_Session->field_6,
                                       (s32)&slot);
                    }
                } else if (st->pc->arg0 == -1) {
                    slot = Game_GetPtrSlot(4);
                    Gp_DispatchMsg(slot, 0x7D8, st->pc->arg1, (s32)&slot);
                } else {
                    slot = Game_GetPtrSlot(st->pc->arg0);
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
                D_801156F4            = NULL;
                Game_Session->field_1 = 0;
                if (arg0->spawnArg1 == 0) {
                    Gp_DispatchMsg(Game_GetPtrSlot(6), 0xFA5, 0, 0);
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
                Game_Session->field_52 = 1;
                /* fallthrough */

            case 3:
                Mc_SaveData.field_4 = (u8)st->pc->arg0;
                break;

            case 4:
                D_801156CB = 1;
                st->wait   = st->pc->arg0;
                st->pc     = st->pc + 1;
                return;

            case 5:
                if (st->msgTask != NULL) {
                    Task_Kill(st->msgTask);
                    st->msgTask = NULL;
                }
                break;

            case 6:
                Gp_DispatchMsg(Game_GetPtrSlot(6), 0xFA8, 0, 0);
                break;

            case 7:
                Game_Session->field_1 = (u8)st->pc->arg0;
                break;

            case 9:
                D_801156CB  = 1;
                D_801156A4 |= 0x40;
                st->pc      = st->pc + 1;
                return;

            case 8:
                if (Gp_DispatchMsg(Game_GetPtrSlot(st->pc->arg0), 0x3ED, 0, 0) == 0) {
                    break;
                }
                return;

            case 29:
                if (Gp_DispatchMsg(Game_GetPtrSlot(st->pc->arg0), 0x3F0, 0, 0) == 0) {
                    break;
                }
                return;

            case 10:
                slot = Game_GetPtrSlot(st->pc->arg0);
                rec  = *(GpRec14*)st->pc->arg3;
                if (st->pc->arg0 == 3) {
                    Gp_PlayerWeaponId(&rec.field_0);
                } else {
                    Gp_AllyAnimId(&rec.field_0);
                }
                if (slot != NULL) {
                    Gp_DispatchMsg(slot, st->pc->arg2, (s32)&rec, st->pc->arg4);
                }
                break;

            case 11:
                Mc_SaveData.field_4 = D_801156F8;
                break;

            case 12:
                D_801156F4 = (GpOverlayIds*)st->pc->arg0;
                Gp_DispatchMsg(Game_GetPtrSlot(6), 0xFA6, (s32)D_801156F4, 0);
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
                pair                  = (GpSndParam*)&D_8007A39C;
                Mc_SaveData.field_5C5 = (u8)st->pc->arg0;
                D_801156C9            = 1;
                pair->field_0         = (u16)st->pc->arg1;
                D_80062734            = 0;
                pair->field_2         = (u16)st->pc->arg2;
                Task_SpawnFromTable(&D_80062774, 0, 0, 0);
                break;

            case 21:
                if (D_80062734 == 0) {
                    return;
                }
                break;

            case 22:
                Task_Spawn(9, 0xC, 0, (st->pc->arg0 << 8) | st->pc->arg1);
                break;

            case 23:
                if (arg0->spawnArg1 == 0) {
                    Gp_DispatchMsg(Game_GetPtrSlot(6), 0xFA5, 0, 0);
                }
                arg0->spawnArg1 = 1;
                Gp_AbortCap();
                Gp_MsgPlayer3F3(1);
                Gp_DispatchMsg(Game_GetPtrSlot(3), 0x401, 0, 0);
                if (D_8010FBE0 != NULL) {
                    Task_CallExit(D_8010FBE0);
                    D_8010FBE0 = NULL;
                }
                st2 = (GpEvsState*)arg0->idMap;
                if (st2->fadeTask != NULL) {
                    if (D_801156D8.field_1 != 2) {
                        Task_Kill(st2->fadeTask);
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
                st->msgTask = Task_Spawn(1, 0x31, 0, (s32)&D_801156D4);
                break;

            case 25:
                D_801156D4.field_1 = 1;
                break;

            case 26:
                if (D_8010FBE4 != NULL) {
                    Task_Kill(D_8010FBE4);
                }
                D_801156DC.field_0 = (u16)st->pc->arg0;
                D_801156DC.field_2 = (u16)st->pc->arg1;
                D_8010FBE4         = Task_Spawn(9, 0xD, 0, (s32)&D_801156DC);
                break;

            case 27:
                if (D_8010FBE8 != NULL) {
                    Task_Kill(D_8010FBE8);
                }
                D_801156E0.field_0 = st->pc->arg0;
                D_801156E0.field_6 = (u16)st->pc->arg1;
                D_801156E0.field_8 = (u16)st->pc->arg2;
                D_8010FBE8         = Task_Spawn(9, 0xE, 0, (s32)&D_801156E0);
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
                st->fadeTask = Task_Spawn(1, 0x31, st->pc->arg2, (s32)&D_801156D8);
                break;

            case 36:
                D_801156D8.field_1 = 1;
                if (st->pc->arg0 != 0) {
                    D_801156D8.field_2 = (u16)st->pc->arg0;
                }
                break;

            case 37:
                st2 = (GpEvsState*)arg0->idMap;
                if (st2->fadeTask != NULL) {
                    if (D_801156D8.field_1 != 2) {
                        Task_Kill(st2->fadeTask);
                    }
                    st2->fadeTask = NULL;
                }
                break;

            case 38:
                mode = st->pc->arg0;
                if (mode == 0 || mode == 2) {
                    if (D_801156CD != 0) {
                        Wip_SysConfig.field_21 = D_801156EC;
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
                    slot       = Game_GetPtrSlot(0xA);
                    if (slot != NULL) {
                        Gp_EndPlayerActorTask((GpActorWork*)slot);
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
                D_801156D0 = st->pc->arg0;
                break;

            case 47:
                D_801156CC = (u8)st->pc->arg0;
                break;

            case 49:
                D_801156F8 = Mc_SaveData.field_4;
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
                Task_Kill(arg0);
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
                Task_Kill(arg0);
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
                Task_Kill(arg0);
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
                Task_Kill(arg0);
                D_8010FBE8 = 0;
            }
            break;
    }
}

void func_800E8614(s32 arg0, s32 arg1)
{
    func_800E8634(arg0, arg1, 0);
}

void func_800E8634(s32 arg0, s32 arg1, s32 arg2)
{
    Game_Session->field_1  = 1;
    Game_Session->field_5F = 0;
    D_8010FBE0             = 0;
    D_8010FBE4             = 0;
    D_801156D0             = arg2;
    D_801156C9             = 0;
    D_801156CC             = 0;
    D_801156F0             = 5;
    D_801156CD             = 0;
    D_801156CE             = 0;
    D_801156F8             = Mc_SaveData.field_4;
    D_801156EC             = Wip_SysConfig.field_21;
    SndEvt_EnqueueType7(0xFF0D, 1);
    Task_Spawn(9, 7, arg1, arg0);
}

s32 Gp_LookupSlot4(s32 arg0)
{
    s32 out;

    arg0 = (arg0 << 12) | (Game_Session->field_7 << 8) | Game_Session->field_6;
    Gp_DispatchMsg(Game_GetPtrSlot(4), 0x7D0, arg0, (s32)&out);
    return out;
}

void Gp_ScriptInit(Task* arg0)
{
    GpState34*   mem;
    GpScriptCmd* script;

    mem = Mem_Calloc(0x34, 0);
    if (mem == NULL) {
        Task_Kill(arg0);
        return;
    }
    D_801156F9 = 0;
    D_801156F4 = 0;
    Display_AcquireRef();
    script       = arg0->spawnArg2;
    D_801156A4   = 0;
    arg0->idMap  = (TaskIdMap*)mem;
    mem->field_4 = 0;
    D_801156C8   = 0;
    mem->field_0 = script;
    D_801156CA   = 0;
    if (arg0->spawnArg1 == 0) {
        Gp_DispatchMsg(Game_GetPtrSlot(6), 0xFA4, 0, 0);
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
                Game_Session->field_139 = 0;
                Task_Kill(arg0);
            } else {
                Game_Session->field_139 = tmp * 2;
            }
            break;
    }
}

void Gp_ShakeTask(Task* arg0)
{
    register s32 tmp asm("v0");
    register s32 hi asm("v1");
    s32          packed;
    s32          lo;
    s32          scaled;
    s32          val;

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
                Task_Kill(arg0);
            } else {
                tmp         = ABS(arg0->spawnArg1);
                hi          = lo - tmp;
                tmp         = packed >> 8;
                scaled      = hi * tmp;
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                hi          = (u32)Gp_LcgState >> 16;
                hi          = scaled * hi;
                hi          = hi / lo;
                val         = hi >> 16;
                if (arg0->spawnArg1 & 1) {
                    val = ABS(val);
                    Display_ClampField126(val);
                } else {
                    tmp = ABS(val);
                    val = -tmp;
                    Display_ClampField126(val);
                }
                arg0->spawnArg1++;
            }
            break;
    }
}

void Gp_StepScriptA(Task* task)
{
    GpState34*   state;
    GpScriptCmd* table;
    GpScriptRec* recs;
    u16          cmd;
    s32          opcode;
    u8           tmp;

    state          = (GpState34*)task->idMap;
    table          = state->field_0;
    recs           = state->field_4;
    cmd            = table[state->field_E].field_0;
    opcode         = cmd & 0xFF;
    state->field_A = cmd;

    if (opcode != 0) {
        if (opcode == 1) {
            state->field_12 = cmd >> 8;
            state->field_10 = recs[state->field_12].field_2;
            Gp_SpawnPadHold(state->field_10);
            state->field_E++;
        } else if (opcode == 2) {
            state->field_10 = cmd >> 8;
            state->field_E++;
        } else if (opcode == 3) {
            tmp = state->field_14;
            if (tmp == 0) {
                tmp             = cmd >> 8;
                state->field_14 = tmp;
                state->field_E++;
            } else {
                tmp--;
                state->field_14 = tmp;
                state->field_E++;
            }
        } else if (opcode == 4) {
            if (state->field_14 == 0) {
                state->field_E++;
            } else {
                state->field_E = table[state->field_E].field_0 >> 8;
            }
            Gp_StepScriptA(task);
        }
    }
}

void Gp_StepScriptB(Task* task)
{
    GpState34*   state;
    GpScriptCmd* table;
    GpScriptRec* recs;
    u16          cmd;
    s32          opcode;
    u8           tmp;

    state          = (GpState34*)task->idMap;
    table          = state->field_0;
    recs           = state->field_4;
    cmd            = table[state->field_F].field_2;
    opcode         = cmd & 0xFF;
    state->field_C = cmd;

    if (opcode != 0) {
        if (opcode == 1) {
            state->field_13 = cmd >> 8;
            state->field_11 = recs[state->field_13].field_2;
            Gp_SpawnPadLerpScaled(state->field_11, recs[state->field_13].field_0, recs[state->field_13].field_1, state->field_8);
            state->field_F++;
        } else if (opcode == 2) {
            state->field_11 = cmd >> 8;
            state->field_F++;
        } else if (opcode == 3) {
            tmp = state->field_15;
            if (tmp == 0) {
                tmp             = cmd >> 8;
                state->field_15 = tmp;
                state->field_F++;
            } else {
                tmp--;
                state->field_15 = tmp;
                state->field_F++;
            }
        } else if (opcode == 4) {
            if (state->field_15 == 0) {
                state->field_F++;
            } else {
                state->field_F = table[state->field_F].field_2 >> 8;
            }
            Gp_StepScriptB(task);
        }
    }
}

void Gp_SpawnPadHold(s16 arg0)
{
    if (arg0 != 0) {
        Task_Spawn(2, 0xB, arg0, 0);
    }
}

void Gp_SpawnPadLerp(s16 arg0, u8 arg1, u8 arg2)
{
    Task*      task;
    GpState0C* mem;
    s32        start;
    s32        end;

    if (arg0 != 0) {
        mem = Mem_Calloc(0xC, 0);
        if (mem != NULL) {
            task = Task_Spawn(2, 0xC, 0, 0);
            if (task == NULL) {
                Mem_Free(mem);
            } else {
                end                 = (arg2 & 0xFF) << 8;
                start               = (arg1 & 0xFF) << 8;
                task->idMap         = (TaskIdMap*)mem;
                mem->field_8        = arg0;
                mem->field_4.as_s32 = start;
                mem->field_0        = (end - start) / arg0;
            }
        }
    }
}

void Gp_SpawnPadLerpScaled(s16 arg0, u8 arg1, u8 arg2, s16 arg3)
{
    Task*      task;
    GpState0C* mem;
    s32        start;
    s32        end;
    s16        scale;
    s32        temp;

    if (arg0 != 0) {
        mem = Mem_Calloc(0xC, 0);
        if (mem != NULL) {
            task = Task_Spawn(2, 0xC, 0, 0);
            if (task == NULL) {
                Mem_Free(mem);
            } else {
                task->idMap = (TaskIdMap*)mem;
                temp        = arg3 >> 3;
                if (temp == 0) {
                    scale = 1;
                } else {
                    scale = temp;
                }
                end                 = (arg2 & 0xFF) / scale;
                start               = (arg1 & 0xFF) / scale;
                end               <<= 8;
                start             <<= 8;
                mem->field_8        = arg0;
                mem->field_4.as_s32 = start;
                mem->field_0        = (end - start) / arg0;
            }
        }
    }
}

void Gp_HaltPadScripts(void)
{
    Gp_PadScriptHalt        = 1;
    Gp_PadHoldHalt          = 1;
    Gp_PadLerpHalt          = 1;
    Game_Session->field_13B = 0;
    Pad_ClearEvents(0);
}

Task* Gp_SpawnScript18(s32 arg0, s32 arg1)
{
    Task*      task;
    GpState18* mem;

    mem = Mem_Calloc(0x18, 0);
    if (mem != NULL) {
        task = Task_Spawn(2, 0xD, 0, 0);
        if (task != NULL) {
            task->idMap  = (TaskIdMap*)mem;
            mem->field_8 = 0;
            mem->field_0 = arg0;
            mem->field_4 = arg1;
            return task;
        }
        Mem_Free(mem);
    }
    return NULL;
}

void Gp_KickScriptAB(Task* task)
{
    Gp_StepScriptA(task);
    Gp_StepScriptB(task);
    task->state++;
}

void Gp_DispatchScript18(Task* task)
{
    TaskFuncTable5 tableA;
    TaskFuncTable5 tableB;
    GpState18*     state;

    state  = (GpState18*)task->idMap;
    tableA = Gp_ScriptAStates;
    tableB = Gp_ScriptBStates;
    tableA.funcs[state->field_A](task);
    tableB.funcs[state->field_C](task);
    if (state->field_A == 0 && state->field_C == 0) {
        task->state++;
    }
}

void Gp_ClearPadHalt(void)
{
    Gp_PadScriptHalt = 0;
    Gp_PadHoldHalt   = 0;
    Gp_PadLerpHalt   = 0;
}

Task* Gp_SpawnScript18Ex(s32 arg0, s32 arg1, s32 arg2)
{
    Task*      task;
    GpState18* mem;

    mem = Mem_Calloc(0x18, 0);
    if (mem != NULL) {
        task = Task_Spawn(2, 0xD, 0, 0);
        if (task != NULL) {
            task->idMap  = (TaskIdMap*)mem;
            mem->field_8 = arg2;
            mem->field_0 = arg0;
            mem->field_4 = arg1;
            return task;
        }
        Mem_Free(mem);
    }
    return NULL;
}

void Gp_Script18Task(Task* arg0)
{
    TaskFuncTable3 sp;

    sp = Gp_Script18States;
    if (Gp_StateF0.field_4 == 0 || (Game_Session->field_13B & 0x80)) {
        if (Gp_PadScriptHalt != 0) {
            arg0->state = 2;
        }
        sp.funcs[arg0->state](arg0);
    }
}

void Gp_ScriptAState0(void)
{
}

void Gp_TickScriptADelay(Task* task)
{
    GpState34* state;

    state = (GpState34*)task->idMap;
    if (--state->field_10 == 0) {
        Gp_StepScriptA(task);
    }
}

void Gp_ScriptAState3(Task* task)
{
    Gp_StepScriptA(task);
}

void Gp_ScriptAState4(Task* task)
{
    Gp_StepScriptA(task);
}

void Gp_ScriptBState0(void)
{
}

void Gp_TickScriptBDelay(Task* task)
{
    GpState34* state;

    state = (GpState34*)task->idMap;
    if (--state->field_11 == 0) {
        Gp_StepScriptB(task);
    }
}

void Gp_ScriptBState3(Task* task)
{
    Gp_StepScriptB(task);
}

void Gp_ScriptBState4(Task* task)
{
    Gp_StepScriptB(task);
}

void Gp_PadHoldTask(Task* task)
{
    if (Gp_StateF0.field_4 == 0 || (Game_Session->field_13B & 0x80)) {
        if (task->spawnArg1 != 0 && Gp_PadHoldHalt == 0) {
            task->spawnArg1--;
            Pad_PostEvent(0, 0, 1, 1);
            Game_Session->field_13B |= 1;
        } else {
            Game_Session->field_13B &= ~1;
            Task_Kill(task);
        }
    }
}

void Gp_PadLerpTask(Task* task)
{
    GpState0C* state;

    state = (GpState0C*)task->idMap;
    if (Gp_StateF0.field_4 == 0 || (Game_Session->field_13B & 0x80)) {
        if (state->field_8 != 0 && Gp_PadLerpHalt == 0) {
            state->field_8--;
            Pad_PostEvent(0, 1, state->field_4.bytes.as_u8, 1);
            state->field_4.as_s32   += state->field_0;
            Game_Session->field_13B |= 2;
        } else {
            Game_Session->field_13B &= ~2;
            Task_Kill(task);
        }
    }
}

void Gp_UpdatePadInput(void)
{
    PadState*     pad;
    WipSysConfig* cfg;
    GpActorWork*  work;
    GameActor*    actor;
    u16           mask;
    register u16  prev asm("s2"); // pinned: GCC otherwise gives $s2 to `actor`
    u16           trig;
    u16           tmp;
    u16           tmp2;

    pad  = (PadState*)&Pad_States[0];
    cfg  = &Wip_SysConfig;
    work = Gp_ActorSlots[0];
    if (work == NULL) {
        return;
    }
    actor = work->actor;
    Gp_ClearPadHalt();
    if (Gp_MenuLockHold == 0) {
        if (actor->field_954 == 0 && Game_Session->field_1 == 0 && Game_Session->field_66 == 0 &&
            actor->field_956 != 6 && cfg->field_18 > 0 && Game_Session->field_0 == 0) {
            if (Gp_MenuLockDelay > 0) {
                Gp_MenuLockDelay--;
                Gp_MenuLockNow = 1;
            } else {
                Gp_MenuLockNow = 0;
            }
        } else {
            Gp_MenuLockNow      = 1;
            Gp_PadSuppressTimer = 4;
            Gp_MenuLockDelay    = 8;
            D_80114D08          = 0xA;
        }
        if (Gp_MenuLockNow == 1 && Gp_MenuLockPrev == 0) {
            Gp_MenuLockHold     = 0;
            Gp_PadSuppressMask |= 0x900;
        } else if (Gp_MenuLockNow == 0 && Gp_MenuLockPrev == 1) {
            Gp_MenuLockHold     = 0;
            Gp_PadSuppressMask &= 0xF6FF;
        }
        Gp_MenuLockPrev = Gp_MenuLockNow;
    }
    Gp_PadSuppressRise = ~Gp_PadSuppressPrev & Gp_PadSuppressMask;
    Gp_PadSuppressFall = Gp_PadSuppressPrev & ~Gp_PadSuppressMask;
    Gp_PadSuppressPrev = Gp_PadSuppressMask;
    if (Display_State.field_12c == 0) {
        if (Gp_PadSuppressRise & 0x900) {
            Display_AcquireRef();
            Gp_PadSuppressRefs++;
        }
        if ((Gp_PadSuppressFall & 0x900) && Gp_PadSuppressRefs != 0) {
            do {
                Display_ReleaseRef();
                Gp_PadSuppressRefs--;
            } while (Gp_PadSuppressRefs != 0);
        }
    }
    if (pad->status == 0x73) {
        mask = pad->buttons;
        prev = pad->prevButtons;
        trig = pad->triggered;
        if (pad->field_56 < -0x800) {
            tmp  = mask | 0x1000;
            mask = tmp;
            if (actor->field_954 != 0 || actor->field_956 < 2) {
                if (pad->field_56 < -0xE80) {
                    if (Mc_SaveData.field_25 == 0) {
                        if (Mc_SaveData.field_1a8 == 1) {
                            // barrier: without it GCC cross-jumps this arm into
                            // the identical `field_25 == 1` arm below
                            SCHED_BARRIER();
                            mask = tmp | 0x80;
                        } else {
                            mask = tmp | 0x20;
                        }
                    } else {
                        if (Mc_SaveData.field_1a8 == 1) {
                            mask = tmp & 0xFF7F;
                        } else {
                            mask = tmp & 0xFFDF;
                        }
                    }
                } else if (Mc_SaveData.field_25 == 1) {
                    if (Mc_SaveData.field_1a8 == 1) {
                        mask = tmp | 0x80;
                    } else {
                        mask = tmp | 0x20;
                    }
                }
            }
            if (pad->field_54 >= 0x801) {
                mask |= 0x2000;
            } else if (pad->field_54 < -0x800) {
                mask |= 0x8000;
            }
        } else if (pad->field_56 >= 0x801) {
            tmp2 = mask | 0x4000;
            mask = tmp2;
            if (pad->field_54 >= 0x801) {
                mask = tmp2 | 0x2000;
            } else if (pad->field_54 < -0x800) {
                mask = tmp2 | 0x8000;
            }
        } else {
            if (pad->field_54 >= 0x801) {
                mask |= 0x2000;
            } else if (pad->field_54 < -0x800) {
                mask |= 0x8000;
            }
        }
    } else {
        mask = pad->buttons;
        prev = pad->prevButtons;
        trig = pad->triggered;
    }
    Game_Session->field_58 = Gp_RemapButtons(actor, mask) & ~Gp_PadSuppressMask;
    Game_Session->field_5A = Gp_RemapButtons(actor, prev) & ~Gp_PadSuppressMask;
    Game_Session->field_5C = Gp_RemapButtons(actor, trig) & ~Gp_PadSuppressMask;
    if (Gp_PadSuppressTimer != 0) {
        Gp_PadSuppressTimer--;
        Game_Session->field_58 = Gp_RemapButtons(actor, mask) & ~Gp_PadSuppressMask & ~0x10;
        Game_Session->field_5A = Gp_RemapButtons(actor, prev) & ~Gp_PadSuppressMask & ~0x10;
        Game_Session->field_5C = Gp_RemapButtons(actor, trig) & ~Gp_PadSuppressMask & ~0x10;
    }
}

u16 Gp_RemapButtons(GameActor* actor, u16 mask)
{
    u16 result;
    s32 i;

    result = 0;
    switch (Mc_SaveData.field_1a8) {
        case 0:
            for (i = 0; i < 0x10; i++) {
                if ((mask >> i) & 1) {
                    result |= 1 << Gp_BtnMap0[i];
                }
            }
            break;
        case 1:
            for (i = 0; i < 0x10; i++) {
                if ((mask >> i) & 1) {
                    result |= 1 << Gp_BtnMap1[i];
                }
            }
            break;
        case 2:
            if (actor->field_954 == 0 && actor->field_956 >= 2) {
                for (i = 0; i < 0x10; i++) {
                    if ((mask >> i) & 1) {
                        result |= 1 << Gp_BtnMap2Alt[i];
                    }
                }
            } else {
                for (i = 0; i < 0x10; i++) {
                    if ((mask >> i) & 1) {
                        result |= 1 << Gp_BtnMap2[i];
                    }
                }
            }
            break;
    }
    return result;
}
