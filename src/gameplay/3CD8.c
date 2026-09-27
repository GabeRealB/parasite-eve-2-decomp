#include "common.h"

#include <psyq/abs.h>
#include <psyq/inline_c.h>
#include <psyq/stdio.h>
#include <psyq/strings.h>

#include "gameplay/1A8.h"
#include "gameplay/268.h"
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

/// Read-only layout settings for caption text. `vertical` selects
/// top-to-bottom columns instead of left-to-right lines.
typedef struct {
    u8 vertical;
} _GpCapLayout;

extern TaskDesc      Gp_EvtSpawnTable[];
extern TaskDesc      D_8010FB4C[];
extern GpAnimArg     Gp_WeaponMsgRec;
extern s32           D_8010FB80;
extern s32           D_8010FB84;
extern s32           Gp_CapCaretGrey;
extern s32           Gp_CapCaretDir;
extern s32           D_8010FB90[];
extern u16           Gp_WeaponIdBase[];
extern u16           Gp_AllyIdBase[];
extern GpEvt12*      Gp_CapTable;
extern GpCmdReply    D_801155A0;
extern s16           D_801155AC;
extern u16           D_801155AE;
extern s16           D_801155B0;
extern s16           D_801155B2;
extern s16           D_801155B4;
extern s16           D_801155B6;
extern u8            D_801155B8;
extern s8            D_801155B9;
extern u8            D_801155BA;
extern u8            D_801155BB;
extern s16           D_801155BC;
extern s16           D_801155BE;
extern s16           D_801155C0;
extern GpCapChoice   D_801155D0[];
extern GlyphUvwh     D_8010FB70[];
extern u8            D_80115670;
extern Task*         Gp_CapTask;
extern s16           D_80115678;
extern s16           D_8011567A;
extern GlyphUvwh*    Gp_CapGlyphs;
extern u8            D_80115680;
extern u8            D_80115688;
extern u8            D_80115648;
extern s16           D_8011564A;
extern s16           D_80115650;
extern s16           D_80115652;
extern u16           Gp_CapCaretX;
extern u16           Gp_CapCaretY;
extern s16           D_80115654;
extern s16           D_80115656;
extern u8            Gp_CapCaretDelay;
extern u8            D_80115659;
extern u8            D_8011565A;
extern u16           D_8011565C;
extern s32           D_80115660;
extern s16           D_80115664;
extern s16           D_80115666;
extern s16           Gp_CapEventKey;
extern s16           D_8011566A;
extern u8            D_8011566C;
extern u8            D_8011566D;
extern u8            D_8011566E;
extern u8            D_8011566F;
extern s32           Gp_CapFile;
extern u8            D_80115690;
extern u8            D_80115694;
extern s16           D_80115698;
extern s16           D_8011569A;
extern u8            D_8011569C;
extern s32*          Gp_CapCmds;
extern u8            D_801156A4;
extern s32           D_801156A8;
extern s8            D_801156B0;
extern s8            D_801156B1;
extern s32           D_801156B4;
extern Task*         D_801156B8;
extern s16           D_801156BC;
extern GpOverlayIds* D_801156F4;
extern u8            D_801156F9;

s32              Stage_HasTransitionFlags(void);
s32              Stage_RequestImageCapture(void);
void             func_8001D5C4(void);
static u16       func_800E5578(s32 arg0, s32 arg1, u8 arg2, u16 arg3);
static void      func_800E62C0(void);
static void      func_800E44A0(Task* arg0);
void             func_80724120(void);
void             func_80724324(void);
void             func_807244CC(char* arg0);
void             func_8072455C(s16 arg0, s32 arg1);
void             func_807245B8(void);
void             func_80724714(void);
static void      Gp_CapExit(Task* arg0);
s32              Gp_StartCapSlot(s16 arg0, s16 arg1, s16 arg2);
s32              Gp_AbortCap(void);
void             Gp_LoadCapFile(s32 arg0);
static void      Gp_ApplyCapEvtFlags(void);
static s32       Gp_FindCapEvt(s32 arg0);
s32              Gp_LookupSlot4(s32 arg0);
extern GpAnimArg D_8010FB10;
extern GpAnimArg D_8010FB24;

void func_800E31E8(Task* arg0);

static s16  Gp_CapCenterX(u16* text);
static s16  Gp_CapCenterXLine(u16* arg0, s32 arg1);
static s16  Gp_CapTextHeight(u16* arg0);
static s16  Gp_CapTextTopY(u16* arg0);
static void Gp_DrawCapCaret(void);
static void func_800E4020(void);
static s32  func_800E6BB8(u16* arg0);
static void func_800E704C(void);

static const TaskFuncTable3 D_800974C8 = { {
    func_800E31E8,
    (TaskFunc)func_800E4020,
    taskKill,
} };

void Gp_RunCapCmd(s32 arg0, s16 arg1)
{
    register GpCapCmd* rec asm("s2");
    s32                flagId;
    s32                val;
    s32                i;

    for (;;) {
        rec    = (GpCapCmd*)Gp_CapCmds[arg0];
        flagId = rec->field_3 | (rec->field_7 << 8);
        switch (rec->field_0) {
            case 0:
                Gp_StartCapSlot(arg0, arg1, 0);
                return;
            case 1:
                if (rec->field_1 & 2) {
                    val = GameFlag_GetNibble(flagId);
                } else {
                    val = rec->field_4;
                }
                if (rec->field_1 & 4) {
                    if (rec->field_2 < val) {
                        arg0 = rec->field_8;
                        continue;
                    }
                }
                Gp_StartCapSlot(arg0, arg1, val);
                if ((val < rec->field_2) || (rec->field_1 & 4)) {
                    val++;
                } else if (rec->field_1 & 1) {
                    val = 0;
                }
                if (rec->field_1 & 2) {
                    GameFlag_SetNibble(flagId, val);
                } else {
                    rec->field_4 = val;
                }
                return;
            case 2:
                Gp_StartCapSlot(arg0, arg1, GameFlag_GetNibble(flagId));
                return;
            case 3:
                Gp_DispatchMsg(gameGetPtrSlot(7), 0x13F0, arg0, 0);
                return;
            case 4:
                i   = 0;
                val = i;
                if (rec->field_6 != 0) {
                    do {
                        if (Gp_GetCurBit2Flag(rec->field_5 + i) == 0 ||
                            Gp_GetCurBit2Flag(rec->field_5 + i) == 1 ||
                            Gp_GetCurBit2Flag(rec->field_5 + i) == 3) {
                            val++;
                        }
                        i++;
                    } while (i < rec->field_6);
                }
                if (rec->field_1 & 4) {
                    if (val == 0) {
                        arg0 = rec->field_8;
                        continue;
                    }
                }
                Gp_StartCapSlot(arg0, arg1, val);
                return;
        }
        return;
    }
}

void Gp_EvtCapWeaponTask(Task* arg0)
{
    s32        flags;
    GameActor* actor;
    s32        mode;
    GpAnimArg  recB;
    GpAnimArg  recA;

    flags = (s32)arg0->spawnArg2;
    actor = gameGetPtrSlot(3)->work;
    switch (arg0->state) {
        case 0:
            if ((flags & 1) && (flags != 0xFF)) {
                recA                 = Gp_WeaponMsgRec;
                recA.animBlock.index = Gp_WeaponIdBase[Mc_SaveData[0].characterId - 1] + Player_Status.weapon;
                Gp_DispatchMsg(gameGetPtrSlot(3), 0x3E8, (s32)&recA, 0);
            }
            recB                 = D_8010FB10;
            recB.animBlock.index = Gp_WeaponIdBase[Mc_SaveData[0].characterId - 1] + Player_Status.weapon;
            Gp_DispatchMsg(gameGetPtrSlot(3), 0x3FA, 0, 0);
            arg0->state++;
            break;
        case 1:
            arg0->state++;
            break;
        case 2:
            if (Gp_DispatchMsg(gameGetPtrSlot(3), 0x3ED, 0, 0) == 0) {
                arg0->state++;
            }
            if (actor->field_954 != 2) {
                taskKill(arg0);
            }
            break;
        case 3:
            if ((flags & 1) && (flags != 0xFF)) {
                Gp_StateF0.field_4 = 1;
            }
            if ((flags & 2) && (flags != 0xFF)) {
                Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F3, 0, 0);
            }
            if ((flags & 4) && (flags != 0xFF)) {
                mode = 2;
            } else if ((flags & 1) == 0) {
                mode = 3;
            } else {
                mode = 0;
            }
            if (flags == 0xFF) {
                Gp_DispatchMsg(gameGetPtrSlot(7), 0x13F0, arg0->spawnArg1, mode);
            } else {
                Gp_RunCapCmd(arg0->spawnArg1, mode);
            }
            arg0->state++;
            break;
        case 4:
            if (Gp_CapBusy() == 0) {
                Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F3, 1, 0);
                arg0->state++;
            }
            break;
        case 5:
            if (D_80115598 != 0) {
                Gp_DispatchMsg(gameGetPtrSlot(7), 0x13F2, (s32)arg0->spawnArg2 + 0x64, 0);
            }
            recB                 = D_8010FB24;
            recB.animBlock.index = Gp_WeaponIdBase[Mc_SaveData[0].characterId - 1] + Player_Status.weapon;
            Gp_DispatchMsg(gameGetPtrSlot(3), 0x3FA, 1, 0);
            arg0->state++;
            break;
        case 6:
            arg0->state++;
            break;
        case 7:
            if (Gp_DispatchMsg(gameGetPtrSlot(3), 0x3ED, 0, 0) == 0) {
                arg0->state++;
            }
            if (actor->field_954 != 2) {
                taskKill(arg0);
                Gp_StateF0.field_4 = 0;
            }
            break;
        case 8:
            taskKill(arg0);
            Gp_StateF0.field_4 = 0;
            Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F1, 0, 0);
            break;
    }
}

static void Gp_InitCapTask(Task* task);
static void Gp_CapTaskState1(void);

static const char         Gp_StrCapMagic[] = "CAP";
static const _GpCapLayout D_80097518       = { 0 };
static const char         Gp_StrEvsFmt[]   = "evs%d_%d_%d.txt";

static const TaskFuncTable3 Gp_CapTaskStates = { {
    Gp_InitCapTask,
    (TaskFunc)Gp_CapTaskState1,
    taskKill,
} };

void Gp_SetNibbleIf(s32 arg0, s32 arg1)
{
    if (arg0 != 0) {
        GameFlag_SetNibble(arg0, arg1);
    }
}

void Gp_RunCapCmd1(s32 arg0)
{
    Gp_RunCapCmd(arg0, 1);
}

void Gp_MsgPlayer3F3(s32 arg0)
{
    Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F3, arg0, 0);
}

void Gp_MsgPlayerWeapon(s32 arg0)
{
    GpAnimArg sp;

    if (arg0 == 0) {
        sp                 = Gp_WeaponMsgRec;
        sp.animBlock.index = Gp_WeaponIdBase[Mc_SaveData[0].characterId - 1] + Player_Status.weapon;
        Gp_DispatchMsg(gameGetPtrSlot(3), 0x3E8, (s32)&sp, 0);
    } else {
        Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F1, 0, 0);
    }
}

void Gp_MsgSlot4Chain(s32 arg0, s32 arg1)
{
    s32 out;

    arg0 = (arg0 << 12) | (gGameSession->at4.loc.stage << 8) | gGameSession->at4.loc.area;
    Gp_DispatchMsg(gameGetPtrSlot(4), 0x7D0, arg0, (s32)&out);
    if (out != 0) {
        Gp_DispatchMsg((Task*)out, 0x7D5, arg1, 0);
    }
}

void Gp_PlayerWeaponId(s32* arg0)
{
    *arg0 = Gp_WeaponIdBase[Mc_SaveData[0].characterId - 1] + Player_Status.weapon;
}

void Gp_AllyAnimId(s32* arg0)
{
    *arg0 = Gp_AllyIdBase[Mc_SaveData[0].companionType - 1] + Mc_SaveData[0].companionVariant;
}

void Gp_FillPlayerHpMp(void)
{
    PlayerStatus* p;

    p     = &Player_Status;
    p->hp = p->hpMax;
    p->mp = p->mpMax;
}

void Gp_FillAllyHp(void)
{
    Mc_SaveData[0].companionHp = Mc_SaveData[0].companionHpMax;
}

void Gp_SpawnIfCapIdle(s32 arg0, s32 arg1)
{
    if (Gp_CapBusy() == 0) {
        Task_SpawnFromTable(Gp_EvtSpawnTable, 0, arg1, arg0);
    }
}

void Gp_EnqueueStageSnd6(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg0 & 0xF000000) {
        arg0 &= 0xF0FFFFFF;
        arg0 |= gGameSession->at4.loc.stage << 24;
    }
    SndEvt_EnqueueType6(arg0, (s8)arg1, (s8)arg2);
}

s32 Gp_PackStageSndId(s32 arg0)
{
    if (arg0 & 0xF000000) {
        arg0 &= 0xF0FFFFFF;
        arg0 |= gGameSession->at4.loc.stage << 24;
    }
    return arg0;
}

void Gp_EnqueueStageSnd7(s32 arg0, s32 arg1)
{
    if (arg0 & 0xF000000) {
        arg0 &= 0xF0FFFFFF;
        arg0 |= gGameSession->at4.loc.stage << 24;
    }
    SndEvt_EnqueueType7(arg0, arg1 & 0xFFFF);
}

void Gp_MsgAlly3F3(s32 arg0)
{
    Task* slot;

    slot = gameGetPtrSlot(0xA);
    if (slot != NULL) {
        Gp_DispatchMsg(slot, 0x3F3, arg0, 0);
    }
}

void Gp_MsgAllyWeapon(s32 arg0)
{
    Task*     slot;
    GpAnimArg sp;

    slot = gameGetPtrSlot(0xA);
    if (slot != NULL) {
        if (arg0 == 0) {
            sp                 = Gp_WeaponMsgRec;
            sp.animBlock.index = Gp_AllyIdBase[Mc_SaveData[0].companionType - 1] + Mc_SaveData[0].companionVariant;
            Gp_DispatchMsg(slot, 0x3E8, (s32)&sp, 0);
        } else {
            Gp_DispatchMsg(slot, 0x3F1, 0, 0);
        }
    }
}

void func_800E3FAC(s32 arg0, s32 arg1)
{
    D_80073980[arg0 / 2 + 4] = arg1;
}

s32 func_800E3FCC(s32 arg0)
{
    return D_80073980[arg0 / 2 + 4];
}

/// Location-message fallback of `D_8010FAD4`, the table installed on pointer
/// slot 7: copies the requested location onto the outgoing record and answers
/// 1, leaving the decision to whoever reads the reply.
s32 func_800E3FF0(Task* task, s32 msgId, GpSaveLoc* src, GpSaveLoc* dst)
{
    *dst = *src;
    return 1;
}

s32 func_800E4018(void)
{
    return 0;
}

static void func_800E4020(void)
{
}

void func_800E4028(Task* arg0)
{
    TaskFuncTable3 sp;

    sp = D_800974C8;
    sp.funcs[arg0->state](arg0);
}

void Gp_ClearAllFlagNibbles(void)
{
    s32 i;

    for (i = 0; i < 0x1F8; i++) {
        GameFlag_SetNibble(i, 0);
    }
}

void Gp_SpawnEvt1(s32 arg0, s32 arg1)
{
    Task_SpawnFromTable(Gp_EvtSpawnTable, 1, arg0, arg1);
}

static s32 Gp_RelocCapFile(GpCapFile* file)
{
    s32            i;
    s32            count;
    s32            flag;
    GpEvt12*       rec;
    s32*           ptr;
    GpCapEvtTable* evts;
    GpCapPtrTable* ptrs;

    if (strncmp(file->magic, Gp_StrCapMagic, 3) != 0) {
        return 0;
    }

    i = 0;
    if (file->field_8 > 0) {
        file->field_8  += (s32)file;
        file->field_C  += (s32)file;
        file->field_10 += (s32)file;
        evts            = (GpCapEvtTable*)file->field_C;
        rec             = (GpEvt12*)(evts + 1);
        count           = evts->count;
        if (count > 0) {
            flag = -1;
            do {
                if (rec->field_8 != flag) {
                    rec->field_8 += (s32)file;
                } else {
                    rec++;
                }
                i++;
                rec++;
            } while (i < count);
        }
        ptrs  = (GpCapPtrTable*)file->field_10;
        i     = 0;
        count = ptrs->count;
        ptr   = ptrs->entries;
        if (count > 0) {
            do {
                if (*ptr != 0) {
                    *ptr += (s32)file;
                }
                i++;
                ptr++;
            } while (i < count);
        }
    }

    Gp_CapGlyphs = (GlyphUvwh*)file->field_8;
    Gp_CapCmds   = ((GpCapPtrTable*)file->field_10)->entries;
    return 1;
}

static s32 Gp_StartCap(s32 arg0, s16 arg1, s16 arg2)
{
    CdCmdQueue* queue;
    TaskDesc*   desc;

    queue = &CdCmd_Queue;
    if (arg0 == 0) {
        return 0;
    }

    Gp_CapEventKey = arg2;
    Gp_CapTable    = (GpEvt12*)arg0;
    D_801155AC     = 0;
    D_801155AE     = 1;
    D_801155B0     = 0;
    D_801155B2     = 0x30;
    D_801155B4     = 0xC0;
    D_801155B8     = 7;
    D_801155B2     = 0x140;
    D_80115664     = 0;
    D_8011569A     = 0;
    D_80115698     = 0;
    D_8011567A     = 0;
    D_801155C0     = 0;
    D_801156A8     = 0;
    D_801155BC     = 0;
    D_8011566E     = 0;
    D_8011566F     = 0;
    D_801155BA     = 0;
    D_801155BB     = 0;
    D_80115648     = 0;
    D_8011566A     = 0;
    D_8011565A     = 0;
    D_80115688     = 0;
    D_80115690     = 0;
    D_80115680     = 1;
    D_80115659     = 0xF;
    D_8011566C     = Mc_SaveData[0].at4.loc.view;
    D_8011565C     = queue->field_22A;
    if (gDisplayState.field_112 != 0) {
        func_807245B8();
        D_8011564A = -1;
    }

    D_801155AE = Gp_FindCapEvt((s16)D_801155AE);
    if (Gp_CapTable[(s16)D_801155AE].field_8 == -1) {
        Gp_CapTable = 0;
        return 0;
    }

    Gp_ApplyCapEvtFlags();
    D_801155B4 = Gp_CapTextTopY((u16*)Gp_CapTable[(s16)D_801155AE].field_8);
    D_801155B6 = Gp_CapTextHeight((u16*)Gp_CapTable[(s16)D_801155AE].field_8);
    D_80115666 = arg1;
    D_80115660 = 0;
    if (arg1 != 0) {
        desc       = Task_GetDesc(2, 7);
        Gp_CapTask = (Task*)Display_InitModeObj(desc, 0, 0, 0);
        if (D_80115666 != 3) {
            return 0;
        }
        Task_SpawnFromTable(D_8010FB4C, 0, 0, 0);
        D_80115666 = 1;
    } else {
        Gp_CapTask = Task_Spawn(2, 7, 0, 0);
    }
    return 0;
}

static void func_800E44A0(Task* task)
{
    Task* target;
    Task* lookupTask;
    s16   eventIndex;
    s32   viewId;
    s32   sceneText;
    s32   dialogText;
    s32   timedText;
    s32   choiceText;
    s32   nextView;
    s32   taskState;
    s32   phase;
    s32   activeViewFlags;
    s32   soundId;
    s32   confirmMask;
    u16   oldChoice;
    u8    viewPhase;
    s32   holdFrames;
    s32   eventFlags;
    u8    choiceSound;
    u8    view;
    u8    nextPhase;
    s32   firstPhase;
    s32   capFlags;
    s32   viewFlags;
    s32   activeFlags;
    s8    savedViewPhase;
    s8    spawnDelay;
    s8    viewPending;
    s32   nextChoiceIndex;
    s32   nextTextIndex;

    D_8011565A = 1;
    if (D_8011564A != -1) {
        D_8011564A = (u16)D_8011564A + 1;
    }
    taskState = task->state;
    if (taskState >= 2) {
        if (taskState >= 5) {
            Gp_CapExit(task);
            return;
        }
        task->state = taskState + 1;
        return;
    }
    if (D_801155BC == 2) {
        if (Stage_HasTransitionFlags() != 0) {
            return;
        }
        D_801155BC = 1;
    }
    if ((s8)D_801155BA > 0) {
        D_801155BA--;
        spawnDelay = D_801155BA;
        if (spawnDelay == 1) {
            return;
        }
        if (spawnDelay != 0) {
            return;
        }
        D_8011566D                  = Mc_SaveData[0].at4.loc.view;
        Mc_SaveData[0].at4.loc.view = D_80115694;
        Gp_DispatchMsg(gameGetPtrSlot(5), 0xBB8, 0, 0);
        Stage_RequestImageCapture();
        Task_Spawn(1, 0x2C, 0, (s32)&D_801155A0);
    }
    eventIndex = Gp_FindCapEvt((s32)(s16)D_801155AE);
    D_801155AE = (u16)eventIndex;
    D_801155B2 = Gp_CapCenterX((u16*)Gp_CapTable[eventIndex].field_8);
    eventFlags = Gp_CapTable[(s16)D_801155AE].field_1;
    if (D_8011567A > 0) {
        D_8011567A = (u16)D_8011567A - 1;
        return;
    }
    if (D_80115678 > 0) {
        D_80115678 = (u16)D_80115678 - 1;
    }
    nextPhase  = D_8011566E;
    firstPhase = 1;
    phase      = nextPhase & 0xFF;
    if (phase == 0) {
        goto processEvent;
    }
    if (phase == firstPhase) {
        D_8011566E = nextPhase + firstPhase;
        return;
    }
    if (phase != 2) {
        goto resumeView;
    }
    D_8011566E = nextPhase + 1;
    Gp_DispatchMsg(gameGetPtrSlot(6), 0xFA7, (s32)(s8)D_801155BB, 0);
    return;
resumeView:
    if (D_801156A4 & 0x20) {
        if (Mc_SaveData[0].demoScene == 5) {
            SndEvt_EnqueueType6(0, 0, 0);
        }
        D_801155BB  = 0;
        D_8011566E  = 0;
        D_801156A4 &= 0xDF;
        if (D_8011566A == 1) {
            D_8011566A = (u16)D_801156BC - 0x1E;
        }
    processEvent:
        savedViewPhase = D_801155BB;
        if (savedViewPhase != 0 && gGameSession->viewReady != 0) {
            D_801155BB = 0;
        }
        if (eventFlags & 0x80) {
            capFlags  = D_801156A4;
            viewFlags = capFlags ^ 0x40;
            viewFlags = viewFlags & 0x40;
            if (eventFlags & viewFlags) {
                return;
            }
            viewFlags       = capFlags & 0x40;
            activeFlags     = eventFlags & viewFlags;
            activeViewFlags = activeFlags & 0xFF;
            if (activeViewFlags != (eventFlags & 0x40)) {
                return;
            }
            if (activeViewFlags != 0) {
                D_801156A4 = capFlags & 0xBF;
            }
            if (D_8011569C == 0) {
                view   = Gp_CapTable[(s16)D_801155AE].field_0;
                viewId = view & 0xFF;
                if (viewId != 0) {
                    view = Gp_FindViewIndex(viewId);
                }
            } else {
                view = Gp_CapTable[(s16)D_801155AE].field_0;
            }
            if (eventFlags & 8) {
                Task_SpawnFromTable(D_8010FB4C, 1, Gp_CapTable[(s16)D_801155AE].field_2 | (Gp_CapTable[(s16)D_801155AE].field_3 << 8) | (Gp_CapTable[(s16)D_801155AE].field_4 << 0x10), 0);
            }
            func_800E704C();
            sceneText = Gp_CapTable[(s16)D_801155AE].field_8;
            if (sceneText != -1) {
                D_801155B4 = Gp_CapTextTopY((u16*)sceneText);
                D_801155B2 = Gp_CapCenterX((u16*)Gp_CapTable[(s16)D_801155AE].field_8);
                D_801155B6 = Gp_CapTextHeight((u16*)Gp_CapTable[(s16)D_801155AE].field_8);
                nextView   = view & 0xFF;
                D_801155BB = 0;
                if ((nextView != 0) && (nextView != Mc_SaveData[0].at4.loc.view)) {
                    if (D_80115688 == 0) {
                        Mc_SaveData[0].at4.loc.view = view;
                        D_801155BB                  = 1;
                        if (gDisplayState.field_112 != 0) {
                            if (D_8011564A == -1) {
                                D_8011564A = 0;
                            }
                            func_8072455C(D_8011564A, nextView);
                        }
                    }
                }
                if (eventFlags & 0x20) {
                    {
                        viewPending = D_801155BB;
                        if (viewPending != 0) {
                            viewPhase  = D_801155BB;
                            D_801155BB = viewPhase + 1;
                            if (D_801156F4 != NULL) {
                                func_8001D5C4();
                            }
                        } else if (!(eventFlags & 0x40)) {
                            D_8011566A = 1;
                        }
                    }
                    D_8011566E = 1;
                    return;
                }
            } else {
                task->state += 1;
                return;
            }
        } else {
            if (D_80115648 == 0) {
                if (Gp_CapTable[(s16)D_801155AE].field_4 & 0xFE) {
                    Gp_DispatchMsg(gameGetPtrSlot(7), 0x13F2, (s32)((u8)Gp_CapTable[(s16)D_801155AE].field_4 >> 1), 0);
                    D_80115648 = 1;
                }
            }

            if (Gp_CapTable[(s16)D_801155AE].field_6 != 0) {
                if (D_801155AC == 0) {
                    D_801155A0.key = Gp_CapTable[(s16)D_801155AE].field_6;
                    if (D_801155A0.key < 0x65U) {
                        if (Gp_GetCurBit2Flag((s32)D_801155A0.key) == 2) {
                            D_801155AC         = 1;
                            D_801155A0.done    = 1;
                            D_801155A0.field_3 = 1;
                            return;
                        }
                        if (D_801155A0.key < 0x65U) {
                            goto spawnDialog;
                        }
                    }
                    lookupTask = gameGetPtrSlot(4);
                    target     = lookupTask;
                    Gp_DispatchMsg(lookupTask, 0x7D8, D_801155A0.key - 0x64, (s32)&target);
                    if (target != NULL) {
                        D_801155A0.done    = 0;
                        D_801155A0.field_3 = 1;
                        Gp_DispatchMsg(target, 0x7DB, (s32)&D_801155A0, 0);
                    } else {
                        D_801155A0.done    = 1;
                        D_801155A0.field_3 = 1;
                    }
                    goto waitDialog;
                spawnDialog:
                    D_801155A0.done = 0;
                    if (D_80115666 == 1) {
                        D_8011566D                  = Mc_SaveData[0].at4.loc.view;
                        Mc_SaveData[0].at4.loc.view = D_80115694;
                        Task_Spawn(1, 0x2C, 0, (s32)&D_801155A0);
                    } else if (D_80115666 == 2) {
                        D_801155BA = 4;
                    } else {
                        Display_InitModeObj(Task_GetDesc(9U, 0xBU), 0, (s32)&D_801155A0, 0);
                    }
                waitDialog:
                    D_801155AC = 1;
                    return;
                }
                if (D_801155A0.done != 0) {
                    if (D_80115666 != 0) {
                        Mc_SaveData[0].at4.loc.view = D_8011566D;
                    }
                    D_801155AC = 0;
                    if (D_801155A0.field_3 == 0) {
                        if (Gp_CapTable[(s16)D_801155AE].field_0 != 0) {
                            Gp_CapEventKey = (s16)Gp_CapTable[(s16)D_801155AE].field_0;
                        }
                    }
                    func_800E704C();
                    D_801155AC = 0;
                    D_801155B0 = 0;
                    D_801155C0 = 0;
                    dialogText = Gp_CapTable[(s16)D_801155AE].field_8;
                    if (dialogText == -1) {
                        task->state += 1;
                        return;
                    }
                    D_801155B4 = Gp_CapTextTopY((u16*)dialogText);
                    D_801155B6 = Gp_CapTextHeight((u16*)Gp_CapTable[(s16)D_801155AE].field_8);
                    return;
                }
            } else if (D_801155AC == 1) {
                if (*(u32*)&Gp_CapTable[(s16)D_801155AE] & 0xFFFF0000) {
                    if (D_80115698 != 0) {
                        func_800E5578(Gp_CapTable[(s16)D_801155AE].field_8, 0x80, 1, Gp_CapTable[(s16)D_801155AE].field_0 | ((Gp_CapTable[(s16)D_801155AE].field_1 & 0x12) << 8));
                        D_80115698 = (u16)D_80115698 - 1;
                        return;
                    }
                    if (D_8011569A == 0) {
                        func_800E704C();
                        timedText = Gp_CapTable[(s16)D_801155AE].field_8;
                        if (timedText != -1) {
                            D_801155B4 = Gp_CapTextTopY((u16*)timedText);
                            D_801155B6 = Gp_CapTextHeight((u16*)Gp_CapTable[(s16)D_801155AE].field_8);
                            goto resetText;
                        }
                        task->state += 1;
                        goto resetText;
                    }
                    if (D_8011569A != 0xFF) {
                        D_8011569A = (u16)D_8011569A - 1;
                        return;
                    }
                } else {
                    func_800E5578(Gp_CapTable[(s16)D_801155AE].field_8, 0x80, 1, Gp_CapTable[(s16)D_801155AE].field_0 | ((Gp_CapTable[(s16)D_801155AE].field_1 & 0x12) << 8));
                    nextChoiceIndex = Gp_FindCapEvt((s16)D_801155AE + 1);
                    if ((Gp_CapTable[nextChoiceIndex].field_8 != -1) && (Gp_CapTable[nextChoiceIndex].field_6 == 0)) {
                        if (Gp_CapTable[nextChoiceIndex].field_2 == 0) {
                            if (Gp_CapTable[nextChoiceIndex].field_3 == 0) {
                                goto checkChoice;
                            }
                            goto checkCaret;
                        }
                    checkChoice:
                        if ((D_801155BE != 0) || (Gp_CapTable[nextChoiceIndex].field_1 & 0x80)) {
                            goto checkCaret;
                        }
                        goto drawCaret;
                    }
                checkCaret:
                    if (Gp_CapTable[(s16)D_801155AE].field_1 & 4) {
                    drawCaret:
                        ((void (*)(s32, s32))Gp_DrawCapCaret)(0xA0, 0xDC);
                    } else {
                        D_80115664 = 0;
                    }
                    oldChoice = (u16)D_801155C0;
                    if (Pad_CheckButtons(0, 1, 0x8000) != 0) {
                        D_801155C0 = (u16)D_801155C0 - 1;
                    }
                    if (Pad_CheckButtons(0, 1, 0x1000) != 0) {
                        D_801155C0 = (u16)D_801155C0 - D_80115680;
                    }
                    if (Pad_CheckButtons(0, 1, 0x2000) != 0) {
                        D_801155C0 = (u16)D_801155C0 + 1;
                    }
                    if (Pad_CheckButtons(0, 1, 0x4000) != 0) {
                        D_801155C0 = (u16)D_801155C0 + D_80115680;
                    }
                    if (D_801155C0 < 0) {
                        D_801155C0 = (s16)oldChoice;
                    }
                    if (D_801155C0 >= D_801155BE) {
                        D_801155C0 = (s16)oldChoice;
                    }
                    if ((D_801155C0 != (s16)oldChoice) && (D_801155BE != 0)) {
                        SndEvt_EnqueueType6(0x15, 0, 0);
                    }
                    func_800E62C0();
                    confirmMask = Pad_MaskConfirm;
                    if (D_801155BE == 0) {
                        confirmMask |= Pad_MaskCancel;
                    }
                    if (Pad_CheckButtons(0, 1, confirmMask) != 0) {
                        if (D_801155BE != 0) {
                            if (D_80115659 == 0) {
                                choiceSound = D_801155D0[D_801155C0].sound;
                                if (choiceSound != 1) {
                                    if (choiceSound == 3) {
                                        soundId = 0x15;
                                        goto playChoiceSound;
                                    }
                                } else {
                                    soundId = 0x16;
                                playChoiceSound:
                                    SndEvt_EnqueueType6(soundId, 0, 0);
                                }
                                goto confirmChoice;
                            }
                        } else {
                        confirmChoice:
                            D_801156A8 = (s32)D_801155C0;
                            if (D_801155BE != 0) {
                                Gp_CapEventKey = (s16)D_801155D0[D_801155C0].eventKey;
                            }
                            D_8011567A = (s16)(u16)D_80115678;
                            func_800E704C();
                            choiceText = Gp_CapTable[(s16)D_801155AE].field_8;
                            if (choiceText == -1) {
                                task->state += 1;
                            } else {
                                D_801155B4 = Gp_CapTextTopY((u16*)choiceText);
                                D_801155B6 = Gp_CapTextHeight((u16*)Gp_CapTable[(s16)D_801155AE].field_8);
                            }
                        resetText:
                            D_801155AC = 0;
                            D_801155B0 = 0;
                            D_801155C0 = 0;
                            return;
                        }
                    }
                }
            } else if ((Gp_CapTable[(s16)D_801155AE].field_2 != 0) && !(D_80115670 & 1)) {
                D_801155AC = func_800E5578(Gp_CapTable[(s16)D_801155AE].field_8, 0x80, 0, Gp_CapTable[(s16)D_801155AE].field_0 | ((Gp_CapTable[(s16)D_801155AE].field_1 & 0x12) << 8));
                if ((s8)D_801155B8 > D_801155B9) {
                    D_801155B9 = (u8)D_801155B9 + 1;
                } else {
                    D_801155B9 = 0;
                    D_801155B0 = (u16)D_801155B0 + 1;
                }
                if (D_80115660 != 0) {
                    ((GpCapTextCb)D_80115660)(D_80115650, D_80115652, Gp_CapTable[(s16)D_801155AE].field_8, D_801155B0, D_801155B9 == 0);
                }
                if (D_801155AC != 0) {
                    D_8011569A = (s16)Gp_CapTable[(s16)D_801155AE].field_3;
                    D_80115698 = (s16)Gp_CapTable[(s16)D_801155AE].field_2;
                    D_80115664 = 0;
                    return;
                }
            } else {
                if (*(u32*)&Gp_CapTable[(s16)D_801155AE] & 0xFFFF0000) {
                    if (Gp_CapTable[(s16)D_801155AE].field_2 != 0) {
                        func_800E5578(Gp_CapTable[(s16)D_801155AE].field_8, 0x80, 1, Gp_CapTable[(s16)D_801155AE].field_0 | ((Gp_CapTable[(s16)D_801155AE].field_1 & 0x12) << 8));
                    }
                    D_80115664 = 0;
                    D_801155AC = 1;

                    holdFrames = Gp_CapTable[(s16)D_801155AE].field_3;
                    D_8011569A = holdFrames;
                    D_80115698 = Gp_CapTable[(s16)D_801155AE].field_2;
                    if (holdFrames < D_8011566A) {
                        D_8011569A  = 0;
                        D_80115698 -= D_8011566A;
                        if (D_80115698 < 0) {
                            D_80115698 = 0;
                        }
                    } else {
                        D_8011569A = holdFrames - (u16)D_8011566A;
                    }
                    D_8011566A = 0;
                    return;
                }
                if ((Pad_CheckButtons(0, 1, Pad_MaskConfirm | Pad_MaskCancel) != 0) || (D_80115670 & 1)) {
                    D_801155AC    = func_800E5578(Gp_CapTable[(s16)D_801155AE].field_8, 0x80, 1, Gp_CapTable[(s16)D_801155AE].field_0 | ((Gp_CapTable[(s16)D_801155AE].field_1 & 0x12) << 8));
                    nextTextIndex = Gp_FindCapEvt((s16)D_801155AE + 1);
                    if (((Gp_CapTable[nextTextIndex].field_8 != -1) && (Gp_CapTable[nextTextIndex].field_6 == 0) && ((Gp_CapTable[nextTextIndex].field_2 != 0) || (Gp_CapTable[nextTextIndex].field_3 == 0))) || (Gp_CapTable[(s16)D_801155AE].field_1 & 4)) {
                        ((void (*)(s32, s32))Gp_DrawCapCaret)(0xA0, 0xDC);
                        return;
                    }
                    D_80115664 = 0;
                    return;
                }

                D_801155AC = func_800E5578(Gp_CapTable[(s16)D_801155AE].field_8, 0x80, 0, Gp_CapTable[(s16)D_801155AE].field_0 | ((Gp_CapTable[(s16)D_801155AE].field_1 & 0x12) << 8));
                if ((s8)D_801155B8 > D_801155B9) {
                    D_801155B9 = (u8)D_801155B9 + 1;
                    return;
                }
                D_801155B9 = 0;
                D_801155B0 = (u16)D_801155B0 + 1;
            }
        }
    } else {
        return;
    }
}

static u16 func_800E5578(s32 arg0, s32 arg1, u8 arg2, u16 arg3)
{
    u8           title;
    u8           flagA;
    u16*         text;
    u16*         body;
    s16          lineEnd;
    u16          ret;
    u16          inChoice;
    s16          lineIdx;
    u8           centered;
    u8           selected;
    s16          nChoice;
    s16          x;
    s32          y;
    s16          i;
    s16          sel;
    u16          code;
    s16          attr;
    s16          sc;
    s16          t;
    s16          glyphY;
    s32          palette;
    s32          titleWidth;
    u16*         next;
    s32          g;
    s16          t2;
    s16          top;
    s32          base59;
    POLY_G4*     bg;
    POLY_G4*     bg2;
    DR_MODE*     dm;
    POLY_FT4*    ft;
    POLY_GT4*    gt;
    POLY_GT4*    gt2;
    GlyphUvwh*   icon;
    GpCapChoice* ch;
    GpCapChoice* p;

    const _GpCapLayout* layout;

    text     = (u16*)arg0;
    layout   = &D_80097518;
    nChoice  = 0;
    title    = arg3;
    inChoice = 0;
    lineIdx  = 0;
    selected = 0;
    centered = ((arg3 >> 9) ^ 1) & 1;
    flagA    = arg2;
    if (centered) {
        x = Gp_CapCenterXLine((u16*)arg0, 0) - 0xA0;
    } else {
        x = (u16)D_801155B2 - 0xA0;
    }
    y       = (u16)D_801155B4 - 0x78;
    body    = text;
    lineEnd = D_801155B0;
    code    = text[lineEnd];
    if (flagA == 0) {
        if ((s16)code == -1) {
            ret     = 1;
            lineEnd = lineEnd - 1;
        } else {
            ret = 0;
        }
    } else {
        ret = 1;
    }

    bg             = (POLY_G4*)gGpuPrimCursor;
    gGpuPrimCursor = bg + 1;
    setlen(bg, 8);
    setcode(bg, 0x3A);
    setRGB0(bg, 0, 0, 0);
    setRGB1(bg, 0, 0, 0);
    setRGB2(bg, 0, 0x40, 0x20);
    setRGB3(bg, 0, 0x40, 0x20);
    bg->x0 = (u16)D_801155B2 - 0xA7;
    bg->y0 = (0x59 - (u16)D_801155B6) - gDisplayState.vramYOffset;
    bg->x1 = (u16)D_801155B2 - D_801155B2 * 2 + 0xAE;
    bg->y1 = (0x59 - (u16)D_801155B6) - gDisplayState.vramYOffset;
    bg->x2 = (u16)D_801155B2 - 0xA7;
    bg->y2 = 0x59 - gDisplayState.vramYOffset;
    bg->x3 = (u16)D_801155B2 - D_801155B2 * 2 + 0xAE;
    bg->y3 = 0x59 - gDisplayState.vramYOffset;
    addPrim(&gGpuCurrentOt[3], bg);
    bg2            = (POLY_G4*)gGpuPrimCursor;
    gGpuPrimCursor = bg2 + 1;
    *bg2           = *bg;
    addPrim(&gGpuCurrentOt[3], bg2);
    dm             = (DR_MODE*)gGpuPrimCursor;
    gGpuPrimCursor = dm + 1;
    setlen(dm, 1);
    dm->code[0] = 0xE100020A;
    addPrim(&gGpuCurrentOt[3], dm);

    if (title) {
        ft             = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = ft + 1;
        setlen(ft, 9);
        setcode(ft, 0x2D);
        title      = title - 1;
        base59     = 0x59;
        top        = base59 - (u16)D_801155B6;
        ft->x0     = (u16)D_801155B2 - 0xA7;
        ft->y0     = (top - gDisplayState.vramYOffset) - Gp_CapGlyphs[title].h;
        titleWidth = Gp_CapGlyphs[title].w - 0xA7;
        ft->x1     = (u16)D_801155B2 + titleWidth;
        ft->y1     = (top - gDisplayState.vramYOffset) - Gp_CapGlyphs[title].h;
        ft->x2     = (u16)D_801155B2 - 0xA7;
        ft->y2     = (base59 - gDisplayState.vramYOffset) - (u16)D_801155B6;
        titleWidth = Gp_CapGlyphs[title].w - 0xA7;
        ft->x3     = (u16)D_801155B2 + titleWidth;
        ft->y3     = (base59 - gDisplayState.vramYOffset) - (u16)D_801155B6;
        ft->u0     = Gp_CapGlyphs[title].u;
        ft->v0     = Gp_CapGlyphs[title].v;
        ft->u1     = Gp_CapGlyphs[title].u + Gp_CapGlyphs[title].w;
        ft->v1     = Gp_CapGlyphs[title].v;
        ft->u2     = Gp_CapGlyphs[title].u;
        ft->v2     = Gp_CapGlyphs[title].v + Gp_CapGlyphs[title].h;
        ft->u3     = Gp_CapGlyphs[title].u + Gp_CapGlyphs[title].w;
        ft->v3     = Gp_CapGlyphs[title].v + Gp_CapGlyphs[title].h;
        ft->clut   = 0x3D93;
        ft->tpage  = getTPage(0, 1, D_80115654, D_80115656);
        addPrim(&gGpuCurrentOt[2], ft);
    }

    i = 0;
    while (1) {
        code = body[i];
        if (flagA == 0) {
            if (lineEnd < i) {
                break;
            }
        } else {
            if ((s16)code == -1) {
                break;
            }
        }
        if ((s16)code == -2 && inChoice == 1) {
            nChoice++;
            inChoice = 0;
            selected = 0;
        }
        sc   = code;
        attr = code;
        if (sc == -2) {
            Gp_CapCaretY = y - 2;
            Gp_CapCaretX = x + 4;
            t2           = lineIdx + 1;
            next         = &body[i + 1];
            asm("" : "=r"(g), "+m"(*next) : "r"(lineIdx));
            lineIdx = t2;
            if (layout->vertical == 0) {
                y += func_800E6BB8(next);
                if (centered != 0) {
                    x = Gp_CapCenterXLine((u16*)arg0, (s16)lineIdx) - 0xA0;
                } else {
                    x = (u16)D_801155B2 - 0xA0;
                }
            } else {
                asm("" : "+r"(i) : "r"(g));
                y  = -0x58;
                x -= func_800E6BB8(next);
            }
            i++;
            continue;
        } else {
            if (sc == -3) {
                if (layout->vertical == 0) {
                    x += 3;
                } else {
                    y += 3;
                }
                i++;
                continue;
            } else if ((code & 0x9F00) == 0x8000) {
                if (code & 0x2000) {
                    sel = code & 0xFF;
                } else {
                    sel = Gp_FindViewIndex(code & 0xFF);
                }
                if (Mc_SaveData[0].at4.loc.view != sel) {
                    if (D_80115666 != 0) {
                        Stage_BeginTransition(sel, 1);
                        D_801155BC = 2;
                    } else {
                        if (attr & 0x4000) {
                            Gp_MsgPlayer3F3(0);
                            Gp_MsgAlly3F3(0);
                        }
                        Mc_SaveData[0].at4.loc.view = sel;
                        gGameSession->hideHud       = 1;
                        Gp_StateF0.field_4          = 2;
                    }
                }
                i++;
                continue;
            } else if ((code & 0xFF00) == 0x8100 || (code & 0xFF00) == 0x8200 || (code & 0xFF00) == 0x8300) {
                if (flagA == 0) {
                    ret = 1;
                    break;
                }
                if (inChoice == 1) {
                    nChoice++;
                    selected = 0;
                }
                inChoice    = 1;
                ch          = D_801155D0;
                p           = &ch[nChoice];
                p->sound    = (attr & 0xF00) >> 8;
                p->pos[0]   = x;
                p->pos[1]   = y;
                p->eventKey = attr & 0xFF;
                if (nChoice == D_801155C0) {
                    selected = 1;
                }
                i++;
                continue;
            } else if ((code & 0xFF00) == 0x8400) {
                icon           = &D_8010FB70[code & 0xFF];
                ft             = (POLY_FT4*)gGpuPrimCursor;
                gGpuPrimCursor = ft + 1;
                setlen(ft, 9);
                setcode(ft, 0x2D);
                ft->clut  = 0x3C00;
                ft->tpage = 0x1E;
                t         = (y - gDisplayState.vramYOffset) + 1;
                ft->x0    = x;
                ft->y0    = t - icon->h;
                ft->x1    = x + icon->w;
                ft->y1    = t - icon->h;
                ft->x2    = x;
                ft->y2    = t;
                ft->x3    = x + icon->w;
                ft->y3    = t;
                ft->u0    = icon->u;
                ft->v0    = icon->v;
                ft->u1    = icon->u + icon->w;
                ft->v1    = icon->v;
                ft->u2    = icon->u;
                ft->v2    = icon->v + icon->h;
                ft->u3    = icon->u + icon->w;
                ft->v3    = icon->v + icon->h;
                addPrim(&gGpuCurrentOt[2], ft);
                x += icon->w;
                i++;
                continue;
            } else {
                D_801155B8     = ((s16)code >> 11) & 0xE;
                palette        = ((s16)code >> 10) & 3;
                code           = code & 0x3FF;
                glyphY         = y - gDisplayState.vramYOffset;
                gt             = (POLY_GT4*)gGpuPrimCursor;
                gGpuPrimCursor = gt + 1;
                setlen(gt, 12);
                setcode(gt, 0x3C);
                t = x;
                if (selected == 0) {
                    gt->clut = palette | 0x3D50;
                } else {
                    gt->clut = 0x3D52;
                }
                setShadeTex(gt, 1);
                setRGB0(gt, 0x70, 0x70, 0x70);
                setRGB1(gt, 0x70, 0x70, 0x70);
                setRGB2(gt, 0x70, 0x70, 0x70);
                setRGB3(gt, 0x70, 0x70, 0x70);
                setSemiTrans(gt, 1);
                gt->tpage = getTPage(0, 1, D_80115654, D_80115656);
                gt->x0    = t;
                gt->y0    = glyphY - Gp_CapGlyphs[(s16)code].h;
                gt->x1    = t + Gp_CapGlyphs[(s16)code].w;
                gt->y1    = glyphY - Gp_CapGlyphs[(s16)code].h;
                gt->x2    = t;
                gt->y2    = glyphY;
                gt->x3    = t + Gp_CapGlyphs[(s16)code].w;
                gt->y3    = glyphY;
                gt->u0    = Gp_CapGlyphs[(s16)code].u;
                gt->v0    = Gp_CapGlyphs[(s16)code].v;
                gt->u1    = Gp_CapGlyphs[(s16)code].u + Gp_CapGlyphs[(s16)code].w;
                gt->v1    = Gp_CapGlyphs[(s16)code].v;
                gt->u2    = Gp_CapGlyphs[(s16)code].u;
                gt->v2    = Gp_CapGlyphs[(s16)code].v + Gp_CapGlyphs[(s16)code].h;
                gt->u3    = Gp_CapGlyphs[(s16)code].u + Gp_CapGlyphs[(s16)code].w;
                gt->v3    = Gp_CapGlyphs[(s16)code].v + Gp_CapGlyphs[(s16)code].h;
                addPrim(&gGpuCurrentOt[2], gt);
                gt2            = (POLY_GT4*)gGpuPrimCursor;
                gGpuPrimCursor = gt2 + 1;
                *gt2           = *gt;
                gt2->tpage     = getTPage(0, 2, D_80115654, D_80115656);
                addPrim(&gGpuCurrentOt[2], gt2);
                if (layout->vertical == 0) {
                    x = Gp_CapGlyphs[(s16)code].w + x - 1;
                } else {
                    y = Gp_CapGlyphs[(s16)code].h + y - 1;
                }
            }
        }
        i++;
    }

    D_80115650 = x;
    D_801155BE = nChoice;
    D_80115652 = y - gDisplayState.vramYOffset;
    return ret;
}

static void func_800E62C0(void)
{
    POLY_G3*     p;
    GpCapChoice* choices;
    s16*         pos;
    s32          i;
    s32          x;
    s32          y;
    s32          top;
    s32          color;

    if (D_801155BE != 0) {
        if (D_80115659 != 0) {
            D_80115659--;
        }
        p              = (POLY_G3*)gGpuPrimCursor;
        gGpuPrimCursor = p + 1;
        i              = D_801155C0;
        choices        = D_801155D0;
        pos            = choices[i].pos;
        x              = pos[0];
        y              = pos[1];
        top            = -(gDisplayState.vramYOffset + 2) + y;
        setPolyG3(p);
        color = (D_8010FB80 << 7) / 15;
        setRGB0(p, color, color, color);
        color = (D_8010FB80 * 0xC0) / 15;
        p->x0 = x;
        p->y0 = top - 5;
        p->x1 = x - 10;
        p->y1 = top - 10;
        p->x2 = x - 10;
        p->y2 = top;
        setRGB1(p, color, color, color);
        setRGB2(p, color, color, color);
        addPrim(&gGpuCurrentOt[2], p);
        if (D_8010FB84 == 0) {
            D_8010FB80++;
            if (D_8010FB80 >= 15) {
                D_8010FB84 = 1;
            }
        } else {
            D_8010FB80--;
            if (D_8010FB80 < 9) {
                D_8010FB84 = 0;
            }
        }
    }
}

static void Gp_CapExit(Task* arg0)
{
    CdCmdQueue* queue;
    char        buf[0x20];

    queue = &CdCmd_Queue;
    if (D_80115666 == 2) {
        Gp_DispatchMsg(gameGetPtrSlot(5), 0xBB8, 0, 0);
    }
    if (D_80115666 != 0) {
        if (Mc_SaveData[0].at4.loc.view == D_8011566C) {
            Stage_SetEndingFlag();
        } else {
            queue->field_22A = D_8011565C;
            Stage_BeginTransitionKind7(D_8011566C);
        }
        goto block_11;
    }
    if (D_80115690 == 0) {
        Gp_StateF0.field_4 = 0;
    }
    if (gGameSession->eventState == 0) {
        gGameSession->hideHud       = 0;
        Mc_SaveData[0].at4.loc.view = D_8011566C;
        Gp_MsgPlayer3F3(1);
        Gp_MsgAlly3F3(1);
        if (gDisplayState.field_112 != 0) {
            func_8072455C(D_8011564A, D_8011566C);
            goto block_11;
        }
    } else {
    block_11:
        if (gDisplayState.field_112 != 0 && D_801156F4 != 0) {
            sprintf(
                buf, Gp_StrEvsFmt, D_801156F4->field_0, D_801156F4->field_2,
                D_801156F4->field_4);
            func_807244CC(buf);
        }
    }
    Gp_CapTable = 0;
    D_8011565A  = 0;
    D_801156A4  = 0;
    taskKill(arg0);
}

/// Blinking POLY_G3 continue caret. `Gp_CapCaretDelay` is a frame delay before the
/// first draw; `Gp_CapCaretX` / `Gp_CapCaretY` are base XY; `Gp_CapCaretGrey` /
/// `Gp_CapCaretDir` pulse the vertex greys between 8 and 15.
static void Gp_DrawCapCaret(void)
{
    POLY_G3* p;
    s32      color;
    u16      x;
    u16      y;

    if (Gp_CapCaretDelay != 0) {
        Gp_CapCaretDelay--;
        return;
    }

    p              = (POLY_G3*)gGpuPrimCursor;
    gGpuPrimCursor = p + 1;
    setPolyG3(p);

    color = (Gp_CapCaretGrey << 7) / 15;
    setRGB0(p, color, color, color);

    color = (Gp_CapCaretGrey * 0xC0) / 15;
    setRGB1(p, color, color, color);
    setRGB2(p, color, color, color);

    x     = Gp_CapCaretX;
    y     = Gp_CapCaretY;
    p->x0 = x + 3;
    p->y0 = y - gDisplayState.vramYOffset;
    p->x1 = x;
    p->y1 = -(gDisplayState.vramYOffset + 7) + y;
    p->x2 = x + 7;
    p->y2 = -(gDisplayState.vramYOffset + 7) + y;
    addPrim(&gGpuCurrentOt[2], p);

    if (Gp_CapCaretDir == 0) {
        Gp_CapCaretGrey++;
        if (Gp_CapCaretGrey >= 0xF) {
            Gp_CapCaretDir = 1;
        }
    } else {
        Gp_CapCaretGrey--;
        if (Gp_CapCaretGrey < 9) {
            Gp_CapCaretDir = 0;
        }
    }
}

static s16 Gp_CapCenterX(u16* text)
{
    s16 lineW = 0;
    s16 maxW  = 0;
    s16 i     = 0;
    s16 code  = text[0];

    while (code != -1) {
        if (code == -2) {
            if (lineW > maxW) {
                maxW = lineW;
            }
            lineW = 0;
            code  = text[++i];
        } else if (code == -3) {
            lineW += 3;
            code   = text[++i];
        } else if ((code & 0xFF00) == 0x8400) {
            lineW += 0x10;
            code   = text[++i];
        } else if (code >= 0) {
            lineW += Gp_CapGlyphs[code & 0x3FF].w - 1;
            code   = text[++i];
        } else {
            code = text[++i];
        }
    }
    return (0x140 - maxW) / 2 - 5;
}

static s16 Gp_CapCenterXLine(u16* arg0, s32 arg1)
{
    s16 lineW;
    s16 selectedW;
    s16 i;
    s16 lineIndex;
    s16 code;

    lineW     = 0;
    selectedW = 0;
    i         = 0;
    lineIndex = 0;
    code      = arg0[0];
    while (code != -1) {
        if (code == -2) {
            if (lineIndex == arg1) {
                selectedW = lineW;
            }
            lineW = 0;
            i++;
            lineIndex++;
            code = arg0[i];
        } else if (code == -3) {
            lineW += 3;
            code   = arg0[++i];
        } else if ((code & 0xFF00) == 0x8400) {
            lineW += 0x10;
            code   = arg0[++i];
        } else if (code >= 0) {
            lineW += Gp_CapGlyphs[code & 0x3FF].w - 1;
            code   = arg0[++i];
        } else {
            code = arg0[++i];
        }
    }
    return (0x140 - selectedW) / 2 - 5;
}

static s16 Gp_CapTextHeight(u16* arg0)
{
    s16 lineH = 0;
    s16 total = 0;
    s16 i     = 0;
    s16 code  = arg0[0];

    while (code != -1) {
        if (code == -2) {
            if (lineH == 0) {
                lineH = 2;
            }
            total += lineH;
            lineH  = 0;
        } else if (code != -3) {
            if (code >= 0) {
                if (lineH < Gp_CapGlyphs[code & 0x3FF].h + 2) {
                    lineH = Gp_CapGlyphs[code & 0x3FF].h + 2;
                }
            }
        }
        code = arg0[++i];
    }
    if (total == 2) {
        total = 0;
    }
    return total;
}

static s16 Gp_CapTextTopY(u16* arg0)
{
    s16  lineH     = 0;
    s16  total     = 0;
    s16  i         = 0;
    s16  seenBreak = 0;
    u16* text      = arg0;
    s16  code      = text[0];

    while (code != -1) {
        if (code == -2) {
            if (seenBreak) {
                if (lineH == 0) {
                    lineH = 2;
                }
                total += lineH;
            } else {
                seenBreak = 1;
            }
            lineH = 0;
        } else if (code != -3) {
            if (code >= 0) {
                if (lineH < Gp_CapGlyphs[code & 0x3FF].h + 2) {
                    lineH = Gp_CapGlyphs[code & 0x3FF].h + 2;
                }
            }
        }
        code = text[++i];
    }
    return 0xD0 - total;
}

static s32 func_800E6BB8(u16* arg0)
{
    s16 height = 0;
    s16 i      = 0;
    s16 cont   = 1;
    s16 code   = arg0[0];

    do {
        if (code == -2) {
            cont = 0;
        } else if (code == -1) {
            cont   = 0;
            height = 0xD;
        } else if (code >= 0) {
            if (height < Gp_CapGlyphs[code & 0x3FF].h + 2) {
                height = Gp_CapGlyphs[code & 0x3FF].h + 2;
            }
            code = arg0[++i];
        } else {
            code = arg0[++i];
        }
    } while (cont);
    if (height == 0) {
        height = 2;
    }
    return height;
}

s32 Gp_StartCapSlot(s16 arg0, s16 arg1, s16 arg2)
{
    s32 entry;

    if (Gp_CapTable != 0) {
        return 0;
    }

    entry = Gp_CapCmds[arg0];
    if (entry == 0) {
        return 1;
    }
    return (s16)Gp_StartCap(entry, arg1, arg2);
}

s32 Gp_CapBusy(void)
{
    return Gp_CapTable != 0;
}

s32 Gp_AbortCap(void)
{
    if (Gp_CapTable != 0) {
        if (Gp_CapTask != NULL) {
            Gp_CapExit(Gp_CapTask);
            return 0;
        }
        return -1;
    }
    return -1;
}

s32 Gp_GetCapEventKey(void)
{
    return Gp_CapEventKey;
}

void func_800E6D4C(s16 arg0, s16 arg1)
{
    D_80115654 = arg0;
    D_80115656 = arg1;
}

void Gp_LoadCapFile(s32 arg0)
{
    s32 i;
    s32 count;

    count = 0;
    for (i = 0; i < 50; i++) {
        if (D_8006C338[i].field_0 == 3) {
            if (count == arg0) {
                if (gDisplayState.field_112 != 0) {
                    func_80724714();
                }
                Gp_CapFile = (s32)D_8006C338[i].field_4;
                Gp_RelocCapFile((GpCapFile*)Gp_CapFile);
                break;
            }
            count++;
        }
    }
}

void Gp_ResetCap(void)
{
    Gp_CapTable = 0;
    D_801156A8  = 0;
    D_8011565A  = 0;
    func_800E6D4C(0x180, 0);
    Gp_CapFile = 0;
    Gp_LoadCapFile(0);
    D_8011569C = 0;
}

static void func_800E6E44(s32 arg0)
{
    D_80115660 = arg0;
}

static void Gp_ApplyCapEvtFlags(void)
{
    GpEvt12* p;
    u8       field4;
    s32      base;
    s32      idx;

    idx        = (s16)D_801155AE;
    base       = (s32)Gp_CapTable;
    p          = (GpEvt12*)(idx * sizeof(GpEvt12) + base);
    field4     = p->field_4;
    D_80115670 = field4;
    if (p->field_7 != 0) {
        D_80115670 = field4 & 0xFE;
    }
    D_80115678 = p->field_7;
}

static s32 Gp_FindCapEvt(s32 arg0)
{
    s32      flag;
    s32      id;
    s32      base;
    GpEvt12* p;

    flag = -1;
    id   = Gp_CapEventKey;
    base = (s32)Gp_CapTable;
    p    = (GpEvt12*)(arg0 * sizeof(GpEvt12) + base);
loop:
    if (p->field_8 == flag) {
        goto done;
    }
    if (p->field_5 == id) {
        goto done;
    }
    p++;
    arg0++;
    goto loop;
done:
    return arg0;
}

void func_800E6EF4(Task* task)
{
    if (task->state > 0) {
        if (Gp_CapTable != 0 && D_8011565A == 0) {
            Gp_CapTable = 0;
        }
        taskKill(task);
    }
    task->state++;
}

void Gp_DelayedMsgTask(Task* task)
{
    register s32 val asm("s0");
    GpSpawnArg*  arg;
    s32          mode;
    Task*        slot;

    switch (task->state) {
        case 0:
            arg                 = (GpSpawnArg*)&task->spawnArg1;
            task->killCountdown = arg->field_1;
            task->state++;
            break;
        case 1:
            if (task->killCountdown == 0) {
                arg  = (GpSpawnArg*)&task->spawnArg1;
                mode = arg->field_2;
                val  = arg->field_0;
                if (mode == 0) {
                    Gp_DispatchMsg(gameGetPtrSlot(3), 0x401, val, 0);
                } else if (mode == 1) {
                    slot = gameGetPtrSlot(0xA);
                    if (slot != NULL) {
                        Gp_DispatchMsg(slot, 0x401, val, 0);
                    }
                } else {
                    slot = (Task*)Gp_LookupSlot4(mode - 2);
                    if (slot != NULL) {
                        Gp_DispatchMsg(slot, 0x7E0, val, 0);
                    }
                }
                taskKill(task);
            }
            task->killCountdown--;
            break;
    }
}

static void func_800E704C(void)
{
    D_801155AE++;
    D_801155AE       = Gp_FindCapEvt((s16)D_801155AE);
    D_80115648       = 0;
    Gp_CapCaretDelay = 0x1E;
    D_80115659       = 0xF;
    Gp_ApplyCapEvtFlags();
}

void func_800E70AC(Task* task)
{
    if (D_801156F9 == 0) {
        switch (task->state) {
            case 0:
                if (D_80115666 == 2) {
                    Gp_DispatchMsg(gameGetPtrSlot(5), 0xBB8, 1, 0);
                }
                task->state++;
                break;
        }
        func_800E44A0(task);
    }
}

void Gp_EndWaitTask(Task* task)
{
    GpEndWait* flag;

    flag = task->spawnArg2;
    switch (task->state) {
        case 0:
            Task_Spawn(1, 0x2C, 0, (s32)flag);
            task->state++;
            break;
        case 1:
            if (flag->field_2 != 0) {
                Stage_SetEndingFlag();
                taskKill(task);
            }
            break;
    }
}

static void Gp_InitCapTask(Task* task)
{
    void* mem;

    mem = memCalloc(4, 0);
    if (mem == NULL) {
        taskKill(task);
        return;
    }
    Gp_ResetCap();
    D_801156B8     = NULL;
    task->msgTable = D_8010FB90;
    Game_SetPtrSlot(task, 6);
    task->work = mem;
    D_801156B0 = 0;
    task->state++;
}

static void Gp_CapTaskState1(void)
{
    if (gDisplayState.field_112 != 0) {
        func_80724120();
        func_80724324();
    }
    if (Gp_CapFile != 0) {
        Gp_RelocCapFile((GpCapFile*)Gp_CapFile);
    }
    if (Gp_CapBusy() != 0 && D_801156B0 != 0) {
        D_801156BC++;
        if ((D_801156A4 & 0x20) == 0) {
            if (D_801156BC >= 0x1E) {
                D_801156A4 |= 0x20;
                D_801156B0  = 0;
            }
        }
    }
}

s32 Gp_StartCapAndClear(s32 arg0, s32 arg1, s16 arg2)
{
    Gp_StartCapSlot(arg2, 0, 0);
    D_801156B0 = 0;
    return 0;
}

s32 func_800E731C(void)
{
    D_8011569A = 0;
    D_80115698 = 0;
    return 0;
}

s32 Gp_AbortCapClear(void)
{
    D_801156B0 = 0;
    return Gp_AbortCap();
}

s32 func_800E7358(void)
{
    return Gp_CapBusy();
}

s32 func_800E7378(void)
{
    if (Mc_SaveData[0].demoScene == 9) {
        if (D_801156B8 != NULL) {
            return 0;
        }
        D_801156B8 = Task_Spawn(9, 8, 0, 0);
    } else {
        gGameSession->hideHud = 1;
    }
    return 0;
}

s32 func_800E73E8(void)
{
    Task* task;

    if (Mc_SaveData[0].demoScene == 9) {
        task = D_801156B8;
        if (task != NULL) {
            task->spawnArg1 = 1;
            D_801156B8      = NULL;
            return 0;
        }
    } else {
        gGameSession->hideHud = 0;
    }
    return 0;
}

s32 func_800E7434(void)
{
    if (Mc_SaveData[0].demoScene == 9) {
        if (D_801156B8 == NULL) {
            return 0;
        }
        taskKill(D_801156B8);
        D_801156B8 = NULL;
    } else {
        gGameSession->hideHud = 0;
    }
    return 0;
}

s32 func_800E7498(s32 arg0, s32 arg1, GpOverlayIds* arg2)
{
    if (arg2 != NULL) {
        CdCmd_StartOverlay(arg2->field_0, arg2->field_2, arg2->field_4);
    }
    D_801156B4 = 1;
    D_801156B1 = arg2 != NULL;
    return 0;
}

s32 func_800E74EC(s32 arg0, s32 arg1, s32 arg2)
{
    if (gGameSession->evtSkipped == 0) {
        if (D_801156B1 != 0) {
            func_8001D580();
            D_801156B0 = 1;
            D_801156BC = 0;
        } else {
            D_801156B0 = 1;
            if (arg2 == 2) {
                D_801156BC = 0x1E;
            } else {
                D_801156BC = 0;
            }
        }
    }
    return 0;
}

void func_800E7570(Task* arg0)
{
    TaskFuncTable3 sp;

    sp = Gp_CapTaskStates;
    sp.funcs[arg0->state](arg0);
}
