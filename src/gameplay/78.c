#include "common.h"

#include <psyq/inline_c.h>
#include "gte.h"
#include "main/gfxgte.h"
#include <psyq/gtemac.h>
#include <psyq/memory.h>
#include <psyq/rand.h>
#include <psyq/stdio.h>

#include "gameplay/1A8.h"
#include "gameplay/1BC.h"
#include "gameplay/268.h"
#include "gameplay/3688.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/4CC.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/fs.h"
#include "main/gamemain.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/stage.h"
#include "main/stream.h"
#include "main/task.h"
#include "main/text.h"
#include "main/tmd.h"
#include "main/ui.h"
#include "main/wipsys.h"

extern u8           Gp_StrItemObtained[]; // "Item obtained!"
extern u8           Gp_StrBonusItem[];    // "Bonus item!!"
extern s32          Gp_ItemGrantCooldown;
extern McItemScan   D_8010CA2C;
extern UiObjectDesc D_8010CA40;
extern UiObjectDesc D_8010CA78[];
extern UiObjectDesc D_8010D6D8;
extern UiObjectDesc D_80185000;
extern TaskDesc     D_8010CAB0;
extern TaskDesc     D_8010CABC;
extern TaskDesc     D_8010D1FC;
extern TmdListHead  Gp_TmdListStash;
extern s32          D_80114A24;
extern s32          D_80114A34;
extern u16          D_8007A39C;
extern TmdListHead  Gp_TmdListAltStash;
extern Task*        Gp_TmdStashTask;
/// The coordinate a world-matrix update was most recently run for.
///
/// The entry points that refresh a coordinate record it before the ancestor
/// chain is walked, and nothing ever reads it: what the recorded coordinate is
/// kept for is not established.
extern GpCoord* _gGpCurCoord;
extern CVECTOR  D_80114BA4;
extern u16      D_80114BB0[];
extern RECT     D_80114BD0;
extern CVECTOR  D_80114BA8;
extern u8       Gp_DebugAttachLevels[];
extern s32      Pad_MaskConfirm;
extern s32      Pad_MaskCancel;
extern s16      D_80114C40;
extern DR_STP   D_80114C50;
extern s32      D_80115724;

extern McItemRec* Gp_SelItemRec;
static const char D_8009388C[];
static const char D_80093890[];
static const char D_80093894[];
static const char D_80093898[];
static const char D_800938AC[];

void Gp_DrawItemIcon(UiObject* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

void        func_80108874(void);
static void func_800A57B0(GpIdMapC* arg0);
static void Gp_UseItemTask(GpIdMapC* arg0);
static s32  func_800A2104(GpIdMapC* arg0, s32 arg1, s32 arg2);
static s32  func_800A7550(void);

static void func_800A4904(s32 arg0);
static void Gp_DrawAimCircle(s32 arg0, s32 arg1, s32 arg2, s32 arg3);
static void Gp_InitSlot18(s32 arg0, s32 arg1, s32 arg2, s32 arg3);
static void func_800A5574(s32 arg0, s32 arg1, s32 arg2, s32 arg3);
static void func_800A7824(s32 arg0, s32 arg1, s32 arg2);
static void Gp_DrawHudNumbers(s32 arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);
static s32  Gp_SpawnViewCoordTask(GpCoord* arg0, VECTOR* arg1);
void        Gp_FinishLoadWait(Task* task);
void        func_807150F8(s32 arg0);
void        func_80715198(void);

/// Working state of a relative transform between two coordinate frames, carved
/// from the scratch arena that `G_SCRATCH_HEAD` heads.
///
/// `rot` is the source frame's rotation transposed, so that multiplying a
/// matrix by it yields that matrix's orientation relative to the source;
/// `delta` is the target origin relative to the source, which the same
/// rotation turns into the destination translation.
typedef struct {
    MATRIX rot;   // source frame's rotation, transposed
    VECTOR delta; // target origin minus source origin
} _GpRelMatScratch;
STATIC_ASSERT_SIZEOF(_GpRelMatScratch, 0x30);

static const GpHudStatusBits D_8009389C;
static s32                   Gp_CdIdleIfF0Active(void);
static void                  Gp_DrawHudSprites(GpIdMapC* arg0);
static s32                   Gp_GetAttachLevel(s32 arg0);
static void                  Gp_HudTrackSlot0(GpHudTrack* arg0);
static void                  Gp_UpdateAttachCombo(s32 arg0);
static void                  Gp_UpdateLinkXforms(void);
static s32                   func_800A7E5C(s32 arg0);

void Gp_AreaEnterTask(Task* arg0)
{
    u32          key;
    GpEndWork*   work;
    s32          i;
    Task*        slot;
    GameSession* session;
    GpSndParam*  pair;
    McItemScan*  scan;

    if (arg0->state == 0) {
        work = arg0->spawnArg2;
        key  = GP_LOC_WORD(gGameSession->at4.loc);
        key &= GP_LOC_STAGE_AREA;
        Stage_InitPrimBufOnce();
        for (i = 0; i < 2; i++) {
            slot = Gp_ActorSlots[i];
            if (slot != NULL) {
                ((GameActor*)slot->work)->field_90C = NULL;
            }
        }
        SndEvt_EnqueueType8(0xD);
        Gp_EnqueueSndCd((Gp_GetAttachLevel(7) + 0x15) & 0xFF);
        if (key == GP_LOC_KEY(1, 20, 0, 0)) {
            arg0->spawnArg2 = Ui_SpawnFromDesc(&D_80185000, arg0->spawnArg1, 1, 4, NULL);
        } else {
            arg0->spawnArg2 = Ui_SpawnFromDesc(&D_8010CA40, arg0->spawnArg1, 1, 1, NULL);
            if (arg0->spawnArg1 == 0) {
                work->field_4 = 0;
                work->field_0 = 0;
                Gp_SetAreaFlag2(1, (GpAreaKey*)&gGameSession->at4.loc);
                gGameSession->field_126 = 1;
                if (!((key == GP_LOC_KEY(5, 11, 0, 0) || key == GP_LOC_KEY(5, 29, 0, 0)) &&
                      gGameSession->at4.loc.place - 1 < 3U)) {
                    if (Mc_SaveData[0].field_6CC < 0x270FU) {
                        Mc_SaveData[0].field_6CC++;
                    }
                }
                scan = &D_8010CA2C;
                Gp_ClearScanItems(scan);
                arg0->status = Gp_GrantLocationItems(scan);
                if (arg0->status != 0) {
                    Ui_SpawnFromDesc(D_8010CA78, 1, 0, 0x11, arg0->spawnArg2);
                    if (arg0->status == 2) {
                        Ui_SpawnFromDesc(D_8010CA78 + 1, 2, 0, 0x21, arg0->spawnArg2);
                    }
                }
            } else {
                arg0->status = 0;
                if (Mc_SaveData[0].field_6CE < 0x270FU) {
                    Mc_SaveData[0].field_6CE++;
                }
            }
        }
        GameMain_SetFrameTiming(0);
        arg0->state++;
    } else if (arg0->state == 1) {
        session = gGameSession;
        if (!(session->flowFlags & 2)) {
            session->viewReady = 1;
            pair               = (GpSndParam*)&D_8007A39C;
            pair->field_0      = 0;
            pair->field_2      = 0;
            if (!(gGameSession->flowFlags & 8)) {
                Task_SpawnFromTable(&D_80062774, 0, 1, 0);
            } else {
                Task_SpawnFromTable(&D_80062774, 0, 3, 0);
            }
        } else {
            gStageMusicLoadState = 0xFF;
        }
        arg0->state++;
    } else if (arg0->state == 2) {
        UiObject* obj;

        obj = arg0->spawnArg2;
        if (gStageMusicLoadState == 0xFF) {
            if (CdCmd_IsIdle() & 0xFFFF) {
                if (obj->field_2E == 6) {
                    Ui_TeardownTree(obj, obj->owner);
                    if (arg0->status != 0) {
                        Gp_PubItemLoc   = 0x700;
                        arg0->spawnArg2 = Ui_SpawnFromDesc(&D_8010D6D8, 1, 1, 1, NULL);
                        arg0->state++;
                    } else {
                        arg0->killCountdown = 0xA;
                        arg0->state         = 0x10;
                    }
                }
            }
        }
    } else if (arg0->state == 3) {
        UiObject* obj;

        obj = arg0->spawnArg2;
        if ((obj->field_2E == 6) || (obj->field_2E == -1)) {
            Ui_TeardownTree(obj, obj->owner);
            arg0->killCountdown = 0xA;
            arg0->state         = 0x10;
        }
    } else if (arg0->state == 0x10) {
        arg0->killCountdown--;
        if (arg0->killCountdown <= 0) {
            arg0->state = 0x11;
        }
    }

    if (arg0->state >= 0x11) {
        if (gStageMusicLoadState == 0xFF) {
            if (CdCmd_IsIdle() & 0xFFFF) {
                GameMain_SetFrameTiming(1);
                SndEvt_EnqueueType9(0xD);
                taskKill(arg0);
                Stage_ReleasePrimBuf();
                Stage_SetEndingFlag();
            }
        }
    }
}

static u16 Gp_GetAttachParam(s32 arg0)
{
    PlayerStatus* p;
    s32           cond;
    s32           ret;
    u8*           table;
    s32           idx;
    GpRec16*      recs;
    register s32  off asm("v1");

    recs = Gp_IdParamHi;
    idx  = Gp_StateC08.field_5;
    if (idx >= 0xC) {
        ret = 1;
    } else {
        p = &Player_Status;
        if ((GP_LOC_WORD(gGameSession->at4.loc) & GP_LOC_STAGE_AREA) != GP_LOC_KEY(1, 20, 0, 0)) {
            cond = 0;
        } else {
            cond = p->field_26 == 4;
        }
        if (cond == 0) {
            table = Mc_SaveData[0].attachLevels;
        } else {
            table = Gp_DebugAttachLevels;
        }
        ret = table[idx];
        if (ret == 0) {
            ret = 1;
        }
        if (p->peStateFlags & 0x80) {
            if (ret < 3) {
                ret++;
            }
        }
    }
    off  = arg0 * 2;
    off += (Gp_StateC08.field_5 * 3 + ret) * 16;
    off  = (s32)recs + off;
    return *(u16*)off;
}

static void Gp_ApplyAttachStats(s32 arg0, GpIdMapC* arg1)
{
    PlayerStatus* p;
    GpStateF0*    state;
    GpRec8*       rec;
    s32           cond;
    s32           ret;
    u8*           table;
    s32           idx;
    s32           val1;
    register s32  val2 asm("s2");
    s32           flag;
    s32           temp2;
    s32           temp4;
    u8            kind;
    register s32  off asm("v1");
    register s32  scaled asm("v0");

    idx = Gp_StateC08.field_B;
    if (arg0 == 1) {
        idx = Gp_StateC08.field_5;
    }
    if (idx >= 0xC) {
        ret = 1;
    } else {
        p = &Player_Status;
        if ((GP_LOC_WORD(gGameSession->at4.loc) & GP_LOC_STAGE_AREA) != GP_LOC_KEY(1, 20, 0, 0)) {
            cond = 0;
        } else {
            cond = p->field_26 == 4;
        }
        if (cond == 0) {
            table = Mc_SaveData[0].attachLevels;
        } else {
            table = Gp_DebugAttachLevels;
        }
        ret = table[idx];
        if (ret == 0) {
            ret = 1;
        }
        if (p->peStateFlags & 0x80) {
            if (ret < 3) {
                ret++;
            }
        }
    }
    off             = idx * 3;
    off             = off * 8;
    off            += (s32)D_80113D38;
    scaled          = ret * 8;
    rec             = (GpRec8*)(off + scaled);
    state           = &Gp_StateF0;
    temp2           = rec->field_2;
    temp4           = rec->field_4;
    state->field_5  = 0;
    state->field_14 = 0;
    scaled          = (temp2 << 1) + temp2;
    scaled          = (scaled << 3) + temp2;
    val1            = scaled << 2;
    scaled          = (temp4 << 1) + temp4;
    scaled          = (scaled << 3) + temp4;
    val2            = scaled << 2;
    if ((Gp_StateF0.field_0 == 1 && state->field_6 != 0) || state->field_1 != 0) {
        flag = 1;
    } else {
        flag = 0;
    }
    if (flag != 0) {
        switch (rec->field_0) {
            case 0:
                Gp_UpdateAttachCombo(arg0);
                break;
            case 1:
                func_800A7824(arg0, val1, val2);
                if (arg1 != NULL) {
                    arg1->field_16 = 4;
                    arg1->field_18 = val2;
                }
                break;
            case 2:
                Gp_InitSlot18(arg0, val1, val2, rec->field_6);
                goto after_23;
            case 3:
                func_800A5574(arg0, val1, val2, rec->field_6);
            after_23:
                if (arg1 != NULL) {
                    kind           = (u8)rec->field_6;
                    arg1->field_18 = val1;
                    arg1->field_16 = kind + 2;
                }
                break;
            case 4:
                func_800A4904(arg0);
                if (arg1 != NULL) {
                    arg1->field_16 = 2;
                    arg1->field_18 = 0x3FFF;
                }
                break;
        }
        if (arg1 != NULL) {
            if (idx >= 0xC) {
                arg1->field_16 = -1;
            }
        }
    } else if (idx == 7) {
        Gp_UpdateAttachCombo(arg0);
    }
}

/// Draws one of the prompt's button labels on line `line`, `dx` pixels right of
/// the prompt's left edge.
#define DRAW_PROMPT_LABEL(req, dx, line, color, str) \
    {                                                \
        req.x          = obj.baseX + (dx) + xBase;   \
        req.y          = (obj.baseY + 9) + (line);   \
        req.otIndex    = obj.drawOrder + 1;          \
        req.field_8    = (color);                    \
        req.glyphTable = 5;                          \
        req.centerMode = 0;                          \
        req.field_E    = 1;                          \
        func_8002E53C(&req, (str));                  \
    }

/// Draws a quantity right-aligned on line `line`; an empty count sets `flag`.
#define DRAW_PROMPT_COUNT(req, line, count)                 \
    {                                                       \
        req.field_8    = 0x606060;                          \
        req.glyphTable = 5;                                 \
        req.centerMode = 2;                                 \
        req.field_E    = 0;                                 \
        req.x          = obj.baseX + 0x94;                  \
        req.y          = (obj.baseY + 9) + (line);          \
        req.otIndex    = obj.drawOrder + 1;                 \
        func_8002E53C(&req, Text_ItoaSigned(buf, (count))); \
        if ((count) == 0) {                                 \
            flag = 1;                                       \
        }                                                   \
    }

static void Gp_DrawItemPrompt(s32 arg0, s32 arg1)
{
    u8            buf[0x10];
    UiObject      obj;
    TextDrawReq   req;
    TextDrawReq   req2;
    RECT          rect;
    PlayerStatus* cfg;
    McItemSlot*   slot;
    s32           item;
    s32           count2;
    s32           count1;
    s32           height;
    s32           flag;
    s32           xBase;
    s32           y;

    cfg    = &Player_Status;
    slot   = Gp_GetItemSlot(cfg->weapon + 0x7F);
    count2 = -1;
    if (Pad_RemapState->field_A != 0) {
        return;
    }
    if (Gp_CapBusy() != 0) {
        return;
    }
    if (gGameSession->hideHud != 0) {
        return;
    }
    if (cfg->weapon == 0) {
        return;
    }
    item = cfg->weapon + 0x7F;
    if (item == 0x92) {
        return;
    }
    count1 = slot->ammoQty;
    if (slot->attachId != 0 && slot->attachId != 0xFF) {
        count2 = slot->attachQty;
    }
    height        = 0xE;
    flag          = 0;
    obj.baseX     = 0;
    obj.baseY     = 0;
    obj.drawOrder = -3;
    obj.mode      = 0;
    xBase         = 0x5F;
    if (slot->attachId != 0xFF) {
        height = 0x18;
    }
    y = 0x64 - height;
    y = y - gDisplayState.vramYOffset;
    if (Mc_SaveData[0].buttonLayout != 2) {
        if (item != 0x96) {
            DRAW_PROMPT_LABEL(req, 4, y, 0x606060, D_8009388C);
        } else {
            DRAW_PROMPT_LABEL(req, 4, y, 0x606060, D_80093890);
        }
    } else {
        if (item != 0x96) {
            DRAW_PROMPT_LABEL(req, 6, y, 0x503060, D_80093894);
        } else {
            DRAW_PROMPT_LABEL(req, 6, y, 0x506030, D_80093898);
        }
    }
    if (slot->ammoId != 0) {
        DRAW_PROMPT_COUNT(req, y, count1);
    } else {
        flag = 1;
    }
    Ui_LayoutWithMode0(&obj, 0x79, (y + 4), 0x1B, 7, 0x102010);
    if (slot->attachId != 0xFF) {
        flag = 0;
        y   += 0xA;
        if (Mc_SaveData[0].buttonLayout != 2) {
            DRAW_PROMPT_LABEL(req2, 4, y, 0x606060, D_80093890);
        } else {
            DRAW_PROMPT_LABEL(req2, 6, y, 0x506030, D_80093898);
        }
        if (slot->attachId != 0) {
            DRAW_PROMPT_COUNT(req2, y, count2);
        } else {
            flag = 1;
        }
        Ui_LayoutWithMode0(&obj, 0x79, (y + 4), 0x1B, 7, 0x102010);
        y -= 0xA;
    }
    rect.w = 0x39;
    rect.x = xBase;
    rect.y = y;
    rect.h = height;
    if (flag == 1) {
        Ui_DrawTextInRect(&rect, -1, 0x40004, NULL);
    } else {
        Ui_DrawTextInRect(&rect, -1, 0x40002, NULL);
    }
}

#undef DRAW_PROMPT_LABEL
#undef DRAW_PROMPT_COUNT

/// Inline copy of `Gp_GetAttachLevel`.
static __inline__ s32 getAttachLevel(s32 idx)
{
    PlayerStatus* p;
    u8*           table;
    s32           cond;
    s32           lvl;

    if (idx >= 0xC) {
        lvl = 1;
    } else {
        p = &Player_Status;
        if ((GP_LOC_WORD(gGameSession->at4.loc) & GP_LOC_STAGE_AREA) != GP_LOC_KEY(1, 20, 0, 0)) {
            cond = 0;
        } else {
            cond = p->field_26 == 4;
        }
        if (cond == 0) {
            table = Mc_SaveData[0].attachLevels;
        } else {
            table = Gp_DebugAttachLevels;
        }
        lvl = table[idx];
        if (lvl == 0) {
            lvl = 1;
        }
        if ((p->peStateFlags & 0x80) && lvl < 3) {
            lvl++;
        }
    }
    return lvl;
}

/// Inline copy of `Gp_IsStateF0Active`.
static __inline__ s32 isStateF0Active_(void)
{
    GpStateF0* p;

    p = &Gp_StateF0;
    if ((p->field_0 == 1 && p->field_6 != 0) || p->field_1 != 0) {
        return 1;
    }
    return 0;
}

/// Reads column `field` of the `Gp_IdParamHi` row that attach `idx` uses at
/// level `lvl`.
static __inline__ u16 _gpAttachParam(s32 idx, s32 lvl, s32 field)
{
    return Gp_IdParamHi[idx * 3 + lvl].field[field];
}

static s32 Gp_CheckAttachThreshold(s32 arg0)
{
    PlayerStatus* cfg;
    s32           result;
    s32           n;

    cfg    = &Player_Status;
    result = 0;
    n      = getAttachLevel(arg0);

    if (!isStateF0Active_()) {
        if (cfg->mp < _gpAttachParam(arg0, n, 2) || arg0 != 7 || cfg->hpMax == cfg->hp) {
            result = 1;
        }
    } else if (arg0 < 0xC) {
        if ((cfg->peStateFlags & 0x10) || (!(cfg->peStateFlags & 0x80) && cfg->mp < _gpAttachParam(arg0, n, 2) && Mc_SaveData[0].cheatMode == 0) || (arg0 == 6 && Gp_StateC08.field_16 != 0 && Gp_StateC08.field_17 != 0) || (arg0 == 7 && cfg->hpMax == cfg->hp && Mc_SaveData[0].cheatMode == 0) || (arg0 == 0xB && D_80115724 >= 3) || ((cfg->peStateFlags & 0x80) && (arg0 >= 6 || _gpAttachParam(arg0, n, 2) * 2 >= cfg->hp))) {
            result = 1;
        }
    }
    return result;
}

static void Gp_SetAttachState(s32 arg0)
{
    GpStateC08*   p;
    PlayerStatus* cfg;
    s32           cond;
    s32           ret;
    u8*           table;
    s32           n;
    register s32  val asm("a0");
    s32           t;
    s32           tmp;
    s8            temp;
    s32           neg;

    Gp_StateC08.field_E = 0;
    if (Gp_StateC08.field_6 & 1) {
        return;
    }
    n                   = (s8)arg0;
    Gp_StateC08.field_5 = arg0;
    val                 = n / 3;
    t                   = (s8)val + 1;
    tmp                 = t * 10 + 1;
    SCHED_BARRIER();
    t   = (s8)(n - val * 3);
    val = tmp + t;
    t   = (val << 2) + val;
    val = t << 1;
    ret = 1;
    if (n < 0xC) {
        cfg = &Player_Status;
        if ((GP_LOC_WORD(gGameSession->at4.loc) & GP_LOC_STAGE_AREA) != GP_LOC_KEY(1, 20, 0, 0)) {
            cond = 0;
        } else {
            cond = cfg->field_26 == 4;
        }
        if (cond == 0) {
            table = Mc_SaveData[0].attachLevels;
        } else {
            table = Gp_DebugAttachLevels;
        }
        table += n;
        ret    = *table;
        if (ret == 0) {
            ret = 1;
        }
        if (cfg->peStateFlags & 0x80) {
            if (ret < 3) {
                ret++;
            }
        }
    }
    val        = val + ret;
    p          = &Gp_StateC08;
    p->field_0 = val;
    neg        = -2;
    TOUCH_REG(neg);
    p->field_3 = neg;
    temp       = Gp_GetAttachParam(3);
    p->field_2 = temp;
    if (temp <= 0) {
        p->field_2 = 1;
    }
    p->field_A         = 2;
    D_80115768         = 0;
    Gp_StateF0.field_4 = 0;
    p->field_8         = 1;
    p->field_9         = 0;
    D_80114C34         = 0;
    p->field_6        &= 0xFE;
}

static __inline__ s32 stepAttachWheelSaved(s32 arg0, s32 arg1, McSaveData* save)
{
    PlayerStatus* p;
    s32           cond;
    u8*           table;

    p = &Player_Status;
    if ((GP_LOC_WORD(gGameSession->at4.loc) & GP_LOC_STAGE_AREA) != GP_LOC_KEY(1, 20, 0, 0)) {
        cond = 0;
    } else {
        cond = p->field_26 == 4;
    }
    if (cond == 0) {
        table = Mc_SaveData[0].attachLevels;
    } else {
        table = Gp_DebugAttachLevels;
    }
    if (arg1 != 0) {
        do {
            if (arg1 > 0) {
                do {
                    arg0++;
                    if (arg0 >= 0xC) {
                        arg0 = 0;
                    }
                } while (table[arg0] == 0 && save->cheatMode == 0);
                arg1--;
            } else {
                do {
                    arg0--;
                    if (arg0 < 0) {
                        arg0 += 0xC;
                    }
                } while (table[arg0] == 0 && save->cheatMode == 0);
                arg1++;
            }
        } while (arg1 != 0);
    }
    return arg0;
}

static __inline__ s32 stepAttachWheel(s32 arg0, s32 arg1)
{
    PlayerStatus* p;
    McSaveData*   save;
    s32           cond;
    u8*           table;

    p = &Player_Status;
    if ((GP_LOC_WORD(gGameSession->at4.loc) & GP_LOC_STAGE_AREA) != GP_LOC_KEY(1, 20, 0, 0)) {
        cond = 0;
    } else {
        cond = p->field_26 == 4;
    }
    if (cond == 0) {
        table = Mc_SaveData[0].attachLevels;
    } else {
        table = Gp_DebugAttachLevels;
    }
    if (arg1 != 0) {
        save = &Mc_SaveData[0];
        do {
            if (arg1 > 0) {
                do {
                    arg0++;
                    if (arg0 >= 0xC) {
                        arg0 = 0;
                    }
                } while (table[arg0] == 0 && save->cheatMode == 0);
                arg1--;
            } else {
                do {
                    arg0--;
                    if (arg0 < 0) {
                        arg0 += 0xC;
                    }
                } while (table[arg0] == 0 && save->cheatMode == 0);
                arg1++;
            }
        } while (arg1 != 0);
    }
    return arg0;
}

static __inline__ s32 getAttachWheelLevel(s32 idx)
{
    PlayerStatus* p;
    u8*           table;
    s32           cond;
    s32           lvl;

    if (idx >= 0xC) {
        lvl = 1;
    } else {
        p = &Player_Status;
        if ((GP_LOC_WORD(gGameSession->at4.loc) & GP_LOC_STAGE_AREA) != GP_LOC_KEY(1, 20, 0, 0)) {
            cond = 0;
        } else {
            cond = p->field_26 == 4;
        }
        if (cond == 0) {
            table = Mc_SaveData[0].attachLevels;
        } else {
            table = Gp_DebugAttachLevels;
        }
        lvl = table[idx];
        if (lvl == 0) {
            lvl = 1;
        }
        if ((p->peStateFlags & 0x80) && lvl < 3) {
            lvl++;
        }
    }
    return lvl;
}

/// Inline copy of `Gp_GetAttachParam` for an explicit slot: parameter `field`
/// of the `Gp_IdParamHi` row for `slot` at its current level.
static __inline__ u16 getAttachWheelParam(s32 slot, s32 field)
{
    s32 lvl;

    lvl = getAttachWheelLevel(slot);
    return Gp_IdParamHi[slot * 3 + lvl].field[field];
}

static s32 func_800A2104(GpIdMapC* arg0, s32 arg1, s32 arg2)
{
    GpWheelScratch s;
    s32            changed;
    s32            order;
    PlayerStatus*  cfg;
    u8*            table;
    s32            cond;
    s32            count;
    s32            xOff;
    s32            yOff;
    s32            item;
    s32            param;
    s32            ret;
    s32            color;
    GpWheelPt*     pts;
    GpWheelPt*     dest;
    GpWheelPt*     points;
    GpWheelPt*     chosen;
    McSaveData*    save;
    s32            angle;
    s32            best;
    s32            flags;
    s32            px;
    s32            py;
    s32            slot;
    s32            i;
    s32            j;
    DR_TPAGE*      dr;

    changed              = 0;
    order                = -2;
    gGameSession->uiOpen = 1;
    cfg                  = &Player_Status;
    count                = 0;
    if ((GP_LOC_WORD(gGameSession->at4.loc) & GP_LOC_STAGE_AREA) != GP_LOC_KEY(1, 20, 0, 0)) {
        cond = 0;
    } else {
        cond = cfg->field_26 == 4;
    }
    if (cond == 0) {
        table = Mc_SaveData[0].attachLevels;
    } else {
        table = Gp_DebugAttachLevels;
    }
    for (j = 11; j >= 0; j--, table++) {
        if (*table != 0) {
            count++;
        }
    }

    if (arg0->field_15 > 0) {
        arg0->field_15--;
    } else if (arg0->field_15 < 0) {
        arg0->field_15++;
    }

    if (arg0->field_15 == 0) {
        if (Pad_CheckButtons(0, 0, 0x5000) == 0) {
            if (Pad_CheckButtons(0, 1, 0x2000) != 0) {
                Gp_StateC08.field_B = stepAttachWheel(Gp_StateC08.field_B, 1);
                changed             = 1;
                arg0->field_15     += 4;
            } else if (Pad_CheckButtons(0, 1, 0x8000) != 0) {
                Gp_StateC08.field_B = stepAttachWheel(Gp_StateC08.field_B, -1);
                changed             = 1;
                arg0->field_15     -= 4;
            }
        }
    }

    if (Gp_StateC08.field_E == 0) {
        xOff           = arg1 + 2;
        yOff           = arg2 + 2;
        arg0->field_10 = getAttachWheelParam(Gp_StateC08.field_B, 2);

        item  = ((Gp_StateC08.field_B / 3) << 4) + ((Gp_StateC08.field_B % 3) << 2) + 0x300;
        param = getAttachWheelParam(Gp_StateC08.field_B, 2);
        if (cfg->peStateFlags & 0x80) {
            param <<= 1;
        }

        s.obj.drawOrder         = -3;
        s.u.text.req.x          = arg1 + 7;
        s.u.text.req.y          = arg2 + 0x22;
        s.u.text.req.otIndex    = -2;
        s.obj.baseX             = arg1;
        s.obj.baseY             = arg2;
        s.obj.mode              = 0;
        s.u.text.req.field_8    = 0x606060;
        s.u.text.req.glyphTable = 0;
        s.u.text.req.centerMode = 0;
        s.u.text.req.field_E    = 1;
        func_8002E53C(&s.u.text.req, Gp_GetItemText(item, 0, 0));

        ret   = getAttachWheelLevel(Gp_StateC08.field_B);
        color = 0x606060;
        func_800C2538(&s.obj, -0xB, 0x28, ret, color);
        Text_DrawPrompt(&s.obj, 0x8E, 0x28, Text_ItoaSigned(s.u.text.buf, param), color, 3, 2);

        s.rect.x = arg1;
        s.rect.y = arg2 + 0x17;
        s.rect.w = 0x91;
        s.rect.h = 0x13;
        Ui_DrawTextInRect(&s.rect, -1, 0x40002, NULL);

        /* Lay the equipped attachments out on a circle; unused slots are
         * parked at the sentinel height so the selection below skips them. */
        s.obj.baseX     = 0x30;
        s.obj.baseY     = 0;
        s.obj.drawOrder = -3;
        s.obj.mode      = 0;
        pts             = s.u.pts;
        for (i = 0; i < 12; i++) {
            if (i < count) {
                angle = ((i * 4 + arg0->field_15) << 12) / (count * 4);
                if (count == 1) {
                    angle = 0;
                }
                dest    = &pts[i];
                dest->x = rsin(angle);
                dest->y = rcos(angle);
            } else {
                pts[i].x = -0x7FFF;
                pts[i].y = -0x7FFF;
            }
        }

        /* Each pass draws the remaining point with the greatest y, then
         * retires it to the sentinel height so later passes skip it. */
        if (count > 0) {
            i      = 0;
            points = s.u.pts;
            save   = &Mc_SaveData[0];
            do {
                best  = 0;
                flags = 0;
                for (j = 0; j < count; j++) {
                    GpWheelPt* top = &points[best];

                    if (top->y < points[j].y) {
                        best = j;
                    }
                }
                chosen = &points[best];
                px     = chosen->x >> 7;
                py     = chosen->y >> 10;
                if (count < 6) {
                    px >>= 1;
                    py >>= 1;
                }
                {
                    s32 cx = px + 0x30;
                    s32 cy = py + 0xF;

                    px = cx + xOff;
                    py = cy + yOff;
                }
                chosen->y = -0x7FFF;

                slot = stepAttachWheelSaved(Gp_StateC08.field_B, best, save);

                if (Gp_CheckAttachThreshold(slot) != 0) {
                    flags = 4;
                }
                if (best == 0 && arg0->field_15 == 0) {
                    flags |= 8;
                }
                Gp_DrawItemIcon(&s.obj, px, py, ((slot / 3) << 4) + ((slot % 3) << 2) + 0x301, flags);
                i++;
            } while (i < count);
        }
    }

    dr             = gGpuPrimCursor;
    gGpuPrimCursor = dr + 1;
    setDrawTPage(dr, 0, 1, 0x3E);
    addPrim(gGpuCurrentOt + order, dr);
    return changed;
}

static void Gp_DrawPeGauge(s32 arg0, s32 arg1, s32 arg2)
{
    UiObject  obj;
    TILE*     tile;
    SPRT_16*  sp;
    SPRT_16*  sp2;
    POLY_FT4* poly;
    DR_TPAGE* dr;
    s32       n;
    s32       cat;
    s32       order;

    n = Gp_GetAttachParam(3);
    if (Gp_StateC08.field_5 < 0xD) {
        if (Gp_StateC08.field_2 > 0) {
            tile           = (TILE*)gGpuPrimCursor;
            gGpuPrimCursor = tile + 1;
            tile->x0       = arg1 + 0x18;
            tile->y0       = arg2 + 0x21;
            tile->w        = Gp_StateC08.field_2;
            tile->h        = 1;
            setlen(tile, 3);
            PRIM_COLOR_WORD(tile, 0) = PRIM_RGBC(0, 0xc0, 0xff, 0);
            setcode(tile, 0x60);
            addPrim(gGpuCurrentOt - 2, tile);
        }

        if (n < 0xB) {
            n = 0xB;
        }

        sp             = (SPRT_16*)gGpuPrimCursor;
        gGpuPrimCursor = sp + 1;
        sp->x0         = arg1 + 0x15;
        sp->y0         = arg2 + 0x1D;
        sp->u0         = 0x98;
        sp->v0         = 0x68;
        sp->clut       = 0x3C0B;
        setlen(sp, 3);
        setcode(sp, 0x77);
        addPrim(gGpuCurrentOt - 2, sp);

        sp2            = (SPRT_16*)gGpuPrimCursor;
        gGpuPrimCursor = sp2 + 1;
        sp2->x0        = n + arg1 + 0x13;
        sp2->y0        = arg2 + 0x1D;
        sp2->u0        = 0xA8;
        sp2->v0        = 0x68;
        sp2->clut      = 0x3C0B;
        setlen(sp2, 3);
        setcode(sp2, 0x77);
        addPrim(gGpuCurrentOt - 2, sp2);

        poly           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = poly + 1;
        poly->x0       = arg1 + 0x1D;
        poly->y0       = arg2 + 0x1D;
        poly->u0       = 0xA0;
        poly->u2       = 0xA0;
        poly->v2       = 0x70;
        poly->v3       = 0x70;
        poly->tpage    = 0x1E;
        setlen(poly, 9);
        poly->v0   = 0x68;
        poly->u1   = 0xA8;
        poly->v1   = 0x68;
        poly->u3   = 0xA8;
        poly->clut = 0x3C0B;
        setcode(poly, 0x2F);
        poly->x2 = poly->x0;
        poly->x1 = poly->x3 = (s16)(poly->x0 - 0xA) + n;
        poly->y1            = poly->y0;
        poly->y2 = poly->y3 = poly->y0 + 8;
        addPrim(gGpuCurrentOt - 2, poly);

        cat           = Gp_StateC08.field_5;
        order         = -3;
        obj.baseX     = 0;
        obj.baseY     = 0;
        obj.drawOrder = order;
        obj.mode      = 0;
        Gp_DrawItemIcon(&obj, arg1 + 4, arg2 + 0x28, ((cat / 3) << 4) + ((cat % 3) << 2) + 0x301, 0);

        dr             = gGpuPrimCursor;
        gGpuPrimCursor = dr + 1;
        setDrawTPage(dr, 0, 1, 0x1E);
        addPrim(gGpuCurrentOt - 2, dr);
    }
}

/// Inline copy of `Gp_GetAttachLevels`.
static __inline__ u8* getAttachLevels(void)
{
    PlayerStatus* p;
    s32           cond;

    p = &Player_Status;
    if ((GP_LOC_WORD(gGameSession->at4.loc) & GP_LOC_STAGE_AREA) != GP_LOC_KEY(1, 20, 0, 0)) {
        cond = 0;
    } else {
        cond = p->field_26 == 4;
    }
    if (cond == 0) {
        return Mc_SaveData[0].attachLevels;
    }
    return Gp_DebugAttachLevels;
}

/// Inline copy of `func_800A7E5C(0)`: the HUD category can be swapped only
/// while the player actor is idle, no CD request is pending and the
/// `Gp_ItemGrantCooldown` cooldown has expired.
static __inline__ s32 hudSwapReady(void)
{
    Task*         work;
    GameActor*    actor;
    PlayerStatus* p;
    s32           flag;
    s32           ret;

    flag = 0;
    work = Gp_ActorSlots[0];
    if (work != NULL) {
        actor = work->work;
        p     = &Player_Status;
        if (actor->field_954 == 0) {
            if (actor->field_956 == 0 || actor->field_956 == 2) {
                if (gGameSession->dirActionBusy == 0) {
                    if (p->field_24 == 0) {
                        flag = 1;
                    }
                }
            }
        }
    }
    if (Gp_StateC08.field_6 & 2) {
        flag = 0;
    }
    if (flag != 0) {
        if (Gp_ItemGrantCooldown <= 0) {
            ret = 1;
            if (Gp_StateF0.field_1 == 0) {
                goto done;
            }
        }
    }
    ret = 0;
done:
    return ret;
}

/// Inline copy of `Gp_CdIdleIfF0Active`.
static __inline__ s32 cdIdleIfF0Active_(void)
{
    GpStateF0* p;
    s32        cond;
    u16        ret;

    p = &Gp_StateF0;
    if ((p->field_0 == 1 && p->field_6 != 0) || p->field_1 != 0) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        ret = CdCmd_IsIdle();
    } else {
        ret = 1;
    }
    return ret;
}

/// Inline copy of `func_800A7CB0`, which evaluates the same gate as
/// `Gp_IsStateF0Active` but always returns 0.
static __inline__ u8 stateF0Gate_(void)
{
    GpStateF0* p;
    s32        cond;

    p = &Gp_StateF0;
    if ((p->field_0 == 1 && p->field_6 != 0) || p->field_1 != 0) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        return 0;
    }
    return 0;
}

static void Gp_UseItemTask(GpIdMapC* arg0)
{
    PlayerStatus* cfg;
    Task*         work;
    GameActor*    actor;
    PadState*     pad;
    s32           flag;
    s32           idx;
    s32           lvl;
    s32           sndId;
    s32           x;
    s32           ok;
    s32           y;
    u8            mode;
    u8            side;
    u16           mask;

    cfg            = &Player_Status;
    flag           = 0;
    arg0->field_10 = 0;
    if (Gp_StateC08.field_8 == 1) {
        if (++D_80114C34 > 0) {
            lvl   = getAttachLevel(Gp_StateC08.field_5);
            sndId = Gp_StateC08.field_5 * 3 + lvl;
            if (isStateF0Active_()) {
                Gp_EnqueueSndCd(sndId);
            }
            Gp_StateC08.field_8 = 2;
            Gp_StateC08.field_7 = 0;
            D_80114C34          = 0;
        }
    }

    x                    = 9;
    y                    = 0x3C;
    y                   -= gDisplayState.vramYOffset;
    arg0->field_E        = 0;
    gGameSession->uiOpen = 0;
    if (Gp_StateC08.field_6 & 8) {
        func_800A7550();
        Gp_StateC08.field_6 &= 0xF7;
    }

    if (Gp_StateC08.field_A != 1) {
        if (Gp_StateC08.field_10 <= 0 || --Gp_StateC08.field_10 <= 0) {
            Gp_StateC08.field_C = 0;
        }
        if (Gp_StateC08.field_12 <= 0 || --Gp_StateC08.field_12 <= 0) {
            Gp_StateC08.field_D = 0;
        }
        if (Gp_StateC08.field_14 <= 0 || --Gp_StateC08.field_14 <= 0) {
            Gp_StateC08.field_F = 0;
        }
    }
    if (Gp_StateC08.field_A == 1) {
        if (gGameSession->padPrev & 0x50) {
            work = gameGetPtrSlot(3);
            if (work != NULL) {
                ((GameActor*)work->work)->field_962 |= 0x40;
            }
            Gp_StateC08.field_A = 0;
            D_80115768          = 0;
            Gp_StateF0.field_4  = 0;
            Gp_StateC08.field_9 = 0;
            if (isStateF0Active_()) {
                Gp_DrawItemPrompt(x, y);
            }
            return;
        }
    }

    if (Gp_StateC08.field_A == 0 && Gp_StateC08.field_E == 0) {
        ok = hudSwapReady();
        if ((ok != 0 && (gGameSession->padPrev & 0x10) && gDisplayState.pendingMode == 0 &&
             !(Gp_StateC08.field_6 & 1)) ||
            (Gp_StateC08.field_6 & 0x10)) {
            Gp_StateC08.field_9  = 1;
            Gp_StateC08.field_6 &= 0xEF;
            side                 = Gp_StateC08.field_A ^ 1;
            Gp_StateC08.field_A  = side;
            D_80115768           = side;
            Gp_StateF0.field_4   = side;
            if (Gp_StateC08.field_B >= 0xC) {
                Gp_StateC08.field_B = 0;
            }
            if (Gp_StateC08.field_B < 0) {
                Gp_StateC08.field_B = 0;
            }
            if (!isStateF0Active_()) {
                if (getAttachLevels()[7] != 0) {
                    Gp_StateC08.field_B = 7;
                }
            }
            flag = 1;
        } else {
            if (isStateF0Active_()) {
                Gp_DrawItemPrompt(x, y);
            }
            return;
        }
    }

    mode = Gp_StateC08.field_A;
    if (mode == 2 || mode == 3) {
        if (Gp_StateC08.field_6 & 4) {
            Gp_StateC08.field_6 &= 0xFB;
            Gp_StateC08.field_3  = -1;
            Gp_StateC08.field_A  = 3;
        }
        Gp_DrawPeGauge((s32)arg0, x, y);
        if (Gp_StateC08.field_A == 3) {
            Gp_StateC08.field_2--;
        }
        if (Gp_StateC08.field_2 <= 0) {
            if (cdIdleIfF0Active_()) {
                Gp_StateC08.field_A  = 0;
                D_80115768           = 0;
                Gp_StateF0.field_4   = 0;
                Gp_StateC08.field_9  = 0;
                Gp_StateC08.field_3  = 1;
                Gp_ItemGrantCooldown = 0x14;
                CdCmd_EnqueueLoadFile(0, 0, 4);
                if (cfg->peStateFlags & 0x80) {
                    cfg->hp -= Gp_GetAttachParam(2) * 2;
                    if (cfg->hp <= 0) {
                        cfg->hp = 1;
                    }
                } else {
                    cfg->mp -= Gp_GetAttachParam(2);
                    if (cfg->mp < 0) {
                        cfg->mp = 0;
                    }
                }
                if (Gp_StateC08.field_5 >= 0xC) {
                    Gp_SetItemSeenBit(Gp_SelItemRec->itemId, 1);
                    Gp_RemoveItem(NULL, Gp_SelItemRec, 0);
                }
                if (Mc_SaveData[0].attachUseCounts[Gp_StateC08.field_5] < 0x270F) {
                    Mc_SaveData[0].attachUseCounts[Gp_StateC08.field_5]++;
                }
                Gp_StateC08.field_8 = 0;
            } else {
                Gp_StateC08.field_2 = 1;
            }
            if (Gp_StateC08.field_2 <= 0) {
                return;
            }
        }

        if ((Gp_StateC08.field_6 & 1) ||
            (Gp_StateC08.field_5 < 0xC && (gGameSession->padPrev & 0x40))) {
            gGameSession->loadedSndId = 0;
            CdCmd_EnqueueLoadFile(0, 0, 4);
            if (Gp_StateC08.field_A >= 2) {
                Gp_StateC08.field_3 = 2;
            }
            Gp_StateC08.field_E = 0;
            Gp_StateC08.field_A = 0;
            D_80115768          = 0;
            Gp_StateF0.field_4  = 0;
            Gp_StateC08.field_7 = 0;
            Gp_StateC08.field_8 = 0;
        }
        return;
    }

    if (func_800A2104(arg0, x, y) != 0) {
        flag = 1;
    }
    actor = gameGetPtrSlot(3)->work;
    if ((Gp_StateC08.field_E != 0 && actor->field_954 == 2) || (Gp_StateC08.field_6 & 1)) {
        Gp_StateC08.field_E = 0;
    }
    if ((arg0->field_15 == 0 && Pad_CheckButtons(0, 0, Pad_MaskConfirm) != 0) ||
        Gp_StateC08.field_E != 0) {
        if (cdIdleIfF0Active_()) {
            pad                    = &Pad_States[0];
            mask                   = Pad_MaskConfirm;
            pad->prevButtons      &= ~mask;
            gGameSession->padPrev &= ~mask;
            gGameSession->pad     &= ~mask;
            gGameSession->padTrig &= ~mask;
            if (Gp_StateC08.field_E != 0) {
                Gp_StateC08.field_5 = Gp_StateC08.field_E;
                Gp_StateC08.field_B = Gp_StateC08.field_E;
            } else {
                Gp_StateC08.field_5 = Gp_StateC08.field_B;
            }
            if (Gp_CheckAttachThreshold(Gp_StateC08.field_5) == 0) {
                Gp_SetAttachState(Gp_StateC08.field_5);
            }
        }
    }

    if (Gp_StateC08.field_A != 0) {
        Gp_ApplyAttachStats(0, arg0);
    }
    if (flag) {
        idx                 = getAttachLevel(Gp_StateC08.field_B);
        Gp_StateC08.field_7 = Gp_StateC08.field_B * 3 + idx;
    }
    if (Gp_StateC08.field_7 > 0) {
        if (stateF0Gate_() == 0) {
            Gp_StateC08.field_7 = 0;
        }
    }
}

void Gp_HudTask(GpIdMapC* arg0)
{
    DisplayState* ds;
    PlayerStatus* cfg;
    GpStateC08*   c08;
    GpStateF0*    f0;
    Task*         slot;
    Task*         work;
    POLY_FT4*     poly;
    s32           kind;
    s32           bad;
    s32           state;
    s32           sub;
    s32           n;
    s32           b;

    bad   = 0;
    kind  = GP_LOC_WORD(gGameSession->at4.loc);
    kind &= GP_LOC_STAGE_AREA;
    cfg   = &Player_Status;
    ds    = &gDisplayState;
    if (ds->demoScene != 0) {
        poly           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = poly + 1;
        poly->x2       = 0x16;
        poly->x0       = 0x16;
        poly->x3       = 0x96;
        poly->x1       = 0x96;
        poly->y1       = -0x6B;
        poly->y0       = -0x6B;
        poly->y3       = -0x2C;
        poly->y2       = -0x2C;
        poly->tpage    = 0xA7;
        poly->v2       = 0xBF;
        poly->v3       = 0xBF;
        poly->clut     = 0x3F80;
        poly->u0       = 0;
        poly->v0       = 0x80;
        poly->u1       = 0x80;
        poly->v1       = 0x80;
        poly->u2       = 0;
        poly->u3       = 0x80;
        setlen(poly, 9);
        setcode(poly, 0x2D);
        addPrim(gGpuCurrentOt - 5, poly);

        poly           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = poly + 1;
        poly->x2       = 0x16;
        poly->x0       = 0x16;
        poly->x3       = 0x96;
        poly->x1       = 0x96;
        poly->y1       = -0x6B;
        poly->y0       = -0x6B;
        poly->y3       = -0x2C;
        poly->y2       = -0x2C;
        poly->b0       = 0x40;
        poly->g0       = 0x40;
        poly->r0       = 0x40;
        poly->tpage    = 0xC7;
        poly->v0       = 0xC0;
        poly->v1       = 0xC0;
        poly->v2       = 0xFF;
        poly->v3       = 0xFF;
        poly->clut     = 0x3F40;
        poly->u0       = 0;
        poly->u1       = 0x80;
        poly->u2       = 0;
        poly->u3       = 0x80;
        setlen(poly, 9);
        setcode(poly, 0x2F);
        addPrim(gGpuCurrentOt - 5, poly);
    }

    b             = arg0->field_14;
    arg0->field_D = 0;
    if (b != 0) {
        if (ds->pendingMode == 0) {
            if (ds->holdState >= 0) {
                ds->pendingMode = b;
            }
        }
        arg0->field_14 = 0;
    }

    slot = gameGetPtrSlot(1);
    if (slot != NULL) {
        if (slot->spawnArg1 != Mc_SaveData[0].at4.loc.view) {
            bad = 1;
        }
    }

    if (bad == 0) {
        DisplayState* d2;

        d2 = &gDisplayState;
        if (d2->holdState < 0) {
            goto after;
        }
        if (Gp_StateC08.field_A != 0) {
            if (d2->demoScene == 0) {
                goto after;
            }
        }
        if (Gp_ItemGrantCooldown > 0) {
            goto after;
        }
        if (gGameSession->dirActionBusy != 0) {
            goto after;
        }
        if (cfg->field_24 != 0) {
            goto after;
        }
        if (Gp_StateF0.field_1 != 0) {
            goto after;
        }
        if (d2->pendingMode != 0) {
            goto after;
        }
        if (Pad_CheckButtons(0, 1, 0x800) != 0) {
            s32 hit;
            s32 ok;

            if (arg0->field_0 == 0) {
                arg0->field_14 = 0x41;
                arg0->field_D  = 0x20;
                goto after;
            }
            hit  = 0;
            work = Gp_ActorSlots[0];
            if (work != NULL) {
                GameActor*    actor;
                PlayerStatus* p;
                s32           mode;

                actor = work->work;
                p     = &Player_Status;
                if (actor->field_954 == 0) {
                    mode = actor->field_956;
                    if (mode == 0 || mode == 2) {
                        if (gGameSession->dirActionBusy == 0) {
                            if (p->field_24 == 0) {
                                hit = 1;
                            }
                        }
                    }
                }
            }
            if (hit != 0) {
                if (Gp_ItemGrantCooldown > 0) {
                    ok = 0;
                    goto have;
                }
                if (Gp_StateF0.field_1 == 0) {
                    ok = 1;
                    goto have;
                }
            }
            ok = 0;
        have:
            if (ok == 0) {
                goto after;
            }
            if (Player_Status.armor == 0) {
                goto after;
            }
            arg0->field_14 = 0x42;
            arg0->field_D  = 0x10;
            goto after;
        }
        if (Pad_CheckButtons(0, 1, 0x100) != 0) {
            if (arg0->field_0 != 0) {
                PlayerStatus* p;
                s32           cond;

                p = &Player_Status;
                if ((GP_LOC_WORD(gGameSession->at4.loc) & GP_LOC_STAGE_AREA) != GP_LOC_KEY(1, 20, 0, 0)) {
                    cond = 0;
                } else {
                    cond = p->field_26 == 4;
                }
                if (cond != 0) {
                    goto after;
                }
                arg0->field_14 = 0x45;
            } else {
                arg0->field_14 = 0x43;
            }
            arg0->field_D = 0x20;
        }
    }

after:
    Gp_UpdateLinkXforms();
    {
        DisplayState* d3;

        c08          = &Gp_StateC08;
        d3           = &gDisplayState;
        c08->field_3 = 0;
        d3->loadBusy = 1;
        state        = arg0->field_0;
        if (state != 1) {
            goto other;
        }
        sub = arg0->field_4;
        if (sub == 0) {
            s32 k;

            if (bad != 0) {
                goto tail;
            }
            k             = GP_LOC_WORD(gGameSession->at4.loc);
            k            &= GP_LOC_STAGE_AREA;
            arg0->field_8 = 0;
            if (k != GP_LOC_KEY(1, 20, 0, 0)) {
                Display_InitModeObj(&D_8010CAB0, 0, (s32)arg0, 0x100);
            } else {
                arg0->field_4 = arg0->field_4 + 1;
            }
            Gp_StateC08.field_A = 0;
            goto tail;
        }
        if (sub == state) {
            f0           = &Gp_StateF0;
            b            = f0->field_1;
            d3->loadBusy = 0;
            if (b != 0) {
                if (f0->field_4 == 0) {
                    f0->field_1 = b - 1;
                }
                n = f0->field_1;
                if (n != 2) {
                    goto tail;
                }
                Gp_TriggerPeState(1, 0xFF);
                CdCmd_EnqueueLoadFile(0, 0, 4);
                if (c08->field_A >= 2) {
                    c08->field_3 = n;
                }
                c08->field_E = 0;
                c08->field_A = 0;
                D_80115768   = 0;
                f0->field_4  = 0;
                c08->field_7 = 0;
                c08->field_8 = 0;
                Gp_PulseState1C80();
                Gp_ClearSlotNodeFlags();
                if ((gGameSession->flowFlags & 0x80) == 0) {
                    goto inc1;
                }
                work = gameGetPtrSlot(3);
                func_80106350(work, Player_Status.weapon, 0);
                if (gGameSession->flowFlags & 0x40) {
                    Gp_MsgPlayerWeapon(0);
                }
                arg0->field_4 = arg0->field_4 + 2;
                goto tail;
            } else {
                GameSession* session;

                if (kind != GP_LOC_KEY(1, 20, 0, 0)) {
                    goto tail;
                }
                session = gGameSession;
                if (session->field_126 == 0) {
                    goto tail;
                }
                f0->field_0        = 0;
                f0->field_6        = 0;
                session->field_126 = 0;
                if (c08->field_A >= 2) {
                    c08->field_3 = 2;
                }
                c08->field_E  = 0;
                c08->field_A  = 0;
                D_80115768    = 0;
                f0->field_4   = 0;
                c08->field_9  = 0;
                arg0->field_4 = 0;
                arg0->field_0 = 0;
                goto tail;
            }
        }
        if (sub == 2) {
            GpStateF0* p;
            Task*      w;
            s32        c;

            p = &Gp_StateF0;
            c = p->field_1;
            if (c != 0) {
                if (p->field_4 == 0) {
                    p->field_1 = c - 1;
                }
            }
            w = gameGetPtrSlot(3);
            if (w == NULL) {
                goto inc1;
            }
            func_801088D4(w, 0, 2);
            goto inc1;
        }
        if (sub == 3) {
            s32           hit;
            s32           flags;
            s32           item;
            PlayerStatus* p;
            s32           cond;

            GpStateF0* p2;
            Task*      w;
            s32        c;

            w   = gameGetPtrSlot(3);
            hit = 0;
            p2  = &Gp_StateF0;
            c   = p2->field_1;
            if (c != 0) {
                if (p2->field_4 == 0) {
                    p2->field_1 = c - 1;
                }
            }
            if (w != NULL) {
                if (((GameActor*)w->work)->field_95E == 0x3E8) {
                    hit = 1;
                }
            }
            flags = gGameSession->flowFlags;
            if ((flags & 0x80) == 0) {
                if (w != NULL) {
                    if (hit == 0) {
                        goto tail;
                    }
                }
                func_80108874();
            } else {
                if (flags & 0x40) {
                    Gp_DispatchMsg((Task*)w, 0x3F1, 2, 0);
                }
            }
            p    = &Player_Status;
            item = p->weapon + 0x7F;
            Gp_FillRelated(item, 0);
            Gp_FillRelated(item, 1);
            if ((GP_LOC_WORD(gGameSession->at4.loc) & GP_LOC_STAGE_AREA) != GP_LOC_KEY(1, 20, 0, 0)) {
                cond = 0;
            } else {
                cond = p->field_26 == 4;
            }
            if (cond != 0) {
                if (gGameSession->field_126 != 0) {
                    goto inc1;
                }
            }
            arg0->field_D = 0x20;
        inc1:
            arg0->field_4 = arg0->field_4 + 1;
            goto tail;
        }
        if (sub == 4 && bad == 0) {
            PlayerStatus* p;
            s32           cond;
            DisplayState* d4;
            GpStateC08*   q;

            p = &Player_Status;
            if ((GP_LOC_WORD(gGameSession->at4.loc) & GP_LOC_STAGE_AREA) != GP_LOC_KEY(1, 20, 0, 0)) {
                cond = 0;
            } else {
                cond = p->field_26 == 4;
            }
            if (cond != 0) {
                if (gGameSession->field_126 != 0) {
                    goto zero;
                }
            }
            d4 = &gDisplayState;
            if (d4->demoScene != 0) {
                d4->gameMode = 1;
            zero:
                arg0->field_4 = 0;
                arg0->field_0 = 0;
            } else {
                Display_InitModeObj(&D_8010CABC, 0, (s32)arg0, 0);
            }
            q = &Gp_StateC08;
            if (q->field_A >= 2) {
                q->field_3 = 2;
            }
            q->field_E         = 0;
            q->field_A         = 0;
            D_80115768         = 0;
            Gp_StateF0.field_4 = 0;
            q->field_9         = 0;
        }
    }

tail:
    if (arg0->field_D < 0x11) {
        if (gGameSession->hideHud == 0) {
            func_800A57B0(arg0);
            if (func_800B9D80(0x100000) != 0) {
                if (gGameSession->field_65 == 0) {
                    Gp_DrawHudSprites(arg0);
                }
            }
        }
    }
    goto end;

other: {
    GpStateF0*  p;
    GpStateC08* q;
    s32         m;

    if (Gp_StateF0.field_1 != 0) {
        if (Gp_StateF0.field_4 == 0) {
            Gp_StateF0.field_1 = Gp_StateF0.field_1 - 1;
        }
    }
    p = &Gp_StateF0;
    m = Gp_StateF0.field_0;
    if (m == 1) {
        arg0->field_4 = 0;
        arg0->field_0 = m;
        CdCmd_EnqueueLoadFile(0, 0, 4);
        q = &Gp_StateC08;
        if (q->field_A >= 2) {
            q->field_3 = 2;
        }
        q->field_E    = 0;
        q->field_A    = 0;
        D_80115768    = 0;
        p->field_4    = 0;
        q->field_7    = 0;
        q->field_8    = 0;
        arg0->field_D = 0x20;
    } else {
        if (arg0->field_D < 0x11) {
            if (gGameSession->hideHud == 0) {
                func_800A57B0(arg0);
                goto end;
            }
        }
        arg0->field_D = 0x20;
    }
}

end:
    if (arg0->field_D <= 0) {
        if (gGameSession->eventState == 0) {
            if (func_800B9D80(0x4000) != 0) {
                GameSession* session;

                session = gGameSession;
                if (session->hideHud == 0) {
                    if (session->field_65 == 0) {
                        Gp_HudTrackSlot0(&arg0->field_1C);
                    }
                }
            }
            Gp_UseItemTask(arg0);
            Gp_StateC08.field_6 &= 0xFE;
            if (Gp_ItemGrantCooldown > 0) {
                Gp_ItemGrantCooldown = Gp_ItemGrantCooldown - 1;
            }
        }
    }
    if (gDisplayState.pendingMode == 0x43) {
        Gp_PulseState1C80();
    }
}

static void Gp_UpdateAttachCombo(s32 arg0)
{
    PlayerStatus* cfg;

    if (arg0 == 0) {
        D_80114F28 = 1;
        return;
    }

    cfg = &Player_Status;
    switch (Gp_StateC08.field_0) {
        case 411:
        case 412:
        case 413: {
            GpItemRec8* rec;
            s32         lvl;
            s32         count;
            s32         time;

            lvl                  = Gp_StateC08.field_0 % 10;
            rec                  = &D_80113E10[lvl];
            count                = Gp_StateC08.field_C & 0xF;
            time                 = rec->field_6;
            Gp_StateC08.field_C  = count;
            Gp_StateC08.field_10 = time;
            if (Gp_StateC08.field_C < 2) {
                Gp_StateC08.field_C++;
            }
            Gp_StateC08.field_C |= lvl << 4;
            break;
        }
        case 421:
        case 422:
        case 423: {
            GpItemRec8* rec;
            s32         lvl;
            s32         count;
            s32         time;

            lvl                  = Gp_StateC08.field_0 % 10;
            rec                  = &D_80113E28[lvl];
            count                = Gp_StateC08.field_D & 0xF;
            time                 = rec->field_6;
            Gp_StateC08.field_D  = count;
            Gp_StateC08.field_12 = time;
            if (Gp_StateC08.field_D < 2) {
                Gp_StateC08.field_D++;
            }
            Gp_StateC08.field_D |= lvl << 4;
            break;
        }
        case 311:
        case 312:
        case 313: {
            GpItemRec8* rec;
            s32         lvl;
            s32         count;
            s32         time;

            lvl                  = Gp_StateC08.field_0 % 10;
            rec                  = &D_80113DC8[lvl];
            count                = Gp_StateC08.field_F & 0xF;
            time                 = rec->field_6;
            Gp_StateC08.field_F  = count;
            Gp_StateC08.field_14 = time;
            if (Gp_StateC08.field_F == 0) {
                Gp_StateC08.field_F++;
            }
            Gp_StateC08.field_F |= lvl << 4;
            Gp_TriggerPeState(1, 0xFF);
            break;
        }
        case 321:
        case 322:
        case 323: {
            GpRec16* params;
            s32      row;
            s32      min;
            s32      max;
            s32      heal;

            /* The parameter rows attach 7 uses at levels 1 to 3. */
            params = Gp_IdParamHi;
            row    = 7 * 3 + 1 + Gp_StateC08.field_0 % 3;
            max    = params[row].field[5];
            min    = params[row].field[4];
            if (min >= max || !isStateF0Active_()) {
                heal = min;
            } else {
                /* A random blend between the row's two amounts. */
                heal = (rand() & 0xFF) + 1;
                heal = (max * heal + min * (0x100 - heal)) >> 8;
                if (heal <= 0) {
                    heal = 1;
                }
            }
            cfg->hp += heal;
            if (cfg->hpMax < cfg->hp) {
                cfg->hp = cfg->hpMax;
            }
            break;
        }
    }
}

static void func_800A4904(s32 arg0)
{
    GpLinkNode* node;
    GpEnemy*    enemy;
    GpEnemy*    claim;
    u16         val;
    s32         idx;

    for (node = Gp_LinkList; node != NULL; node = node->next) {
        if ((node->state.word & 5) != 1) {
            enemy = GP_NODE_ENEMY(node);
            claim = enemy;
            if (arg0 == 0) {
                enemy->colorMode |= 0x80;
            } else {
                val  = Gp_StateC08.field_0;
                idx  = (val / 100U - 1) * 9;
                idx += ((val % 100U) / 10U - 1) * 3;
                idx += val % 10U;
                idx += 0x28000;
                Gp_ClaimSlot18(claim, idx);
            }
        }
    }
}

static __inline__ void Gp_RingPointXZ(GpCircleScratch* sc, s32 ang)
{
    sc->vec.vx = (sc->radius * rcos(ang)) >> 12;
    sc->vec.vz = (sc->radius * rsin(ang)) >> 12;
}

static __inline__ void Gp_ProjectRingPt(GpCircleScratch* sc)
{
    gte_ldv0(&sc->vec);
    gte_rtps();
    gte_stsxy(&sc->sxy);
    gte_stdp(&sc->dp);
    gte_stflg(&sc->flag);
    gte_stszotz(&sc->otz);
}

static __inline__ void Gp_LinkRingSeg(GpCircleScratch* sc)
{
    LINE_F2* prim;

    prim                     = (LINE_F2*)gGpuPrimCursor;
    gGpuPrimCursor           = prim + 1;
    PRIM_COLOR_WORD(prim, 0) = PRIM_RGBC(0, 0xc0, 0x40, 0);
    PRIM_XY_WORD(prim, 0)    = *(u32*)&sc->sxyPrev;
    PRIM_XY_WORD(prim, 1)    = *(u32*)&sc->sxy;
    setlen(prim, 3);
    setcode(prim, 0x40);
    addPrim((u_long*)(((((u32)sc->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
            prim);
}

static void Gp_DrawAimCircle(s32 arg0, s32 arg1, s32 arg2, s32 arg3)
{
    Task*            slot;
    GpCoord*         coord;
    GpCoord*         other;
    GpCircleScratch* sc;
    u8*              head;
    s32              base;
    s32              limit;
    s32              ang;
    s32              i;
    s32              t;
    s32              pass;

    slot = gameGetPtrSlot(3);
    {
        register u8* newhead asm("v1");

        head                          = SCRATCH_HEAD(u8);
        newhead                       = head - 0x60;
        sc                            = (GpCircleScratch*)newhead;
        coord                         = slot->extra.tmd->coords;
        sc->rx                        = arg1;
        sc->ry                        = arg2;
        base                          = gDisplayState.animFrame << 4;
        SCRATCH_HEAD(GpCircleScratch) = sc;
    }
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    if (arg3 & 4) {
        other      = &slot->extra.tmd->coords[4];
        sc->vec.vx = 0;
        sc->vec.vy = 0x12C;
        sc->vec.vz = 0;
        gfxLoadRotSv(&coord->workm, &sc->vec);
        gte_rtv0();
        gte_stsv(&sc->vec);
        sc->trans.vx = other->workm.t[0] + sc->vec.vx;
        sc->trans.vy = other->workm.t[1] + sc->vec.vy;
        sc->trans.vz = other->workm.t[2] + sc->vec.vz;
        gte_SetTransVector(&sc->trans);
    } else if ((arg3 & 2) == 0) {
        gte_SetTransMatrix(&coord->workm);
    } else {
        arg3      &= ~2;
        sc->vec.vx = 0;
        sc->vec.vy = 0;
        sc->vec.vz = arg1;
        gfxLoadRotSv(&coord->workm, &sc->vec);
        gte_rtv0();
        gte_stsv(&sc->vec);
        sc->trans.vx = coord->workm.t[0] + sc->vec.vx;
        sc->trans.vy = coord->workm.t[1] + sc->vec.vy;
        sc->trans.vz = coord->workm.t[2] + sc->vec.vz;
        gte_SetTransVector(&sc->trans);
    }

    if (arg3 & 4) {
        sc->mat = coord->workm;
        Gfx_RotMatrixX(&sc->mat, -0x400, 0);
        arg3 &= ~4;
    } else {
        sc->mat = gGfxViewCoord.workm;
    }
    gte_SetRotMatrix(&sc->mat);

    limit = 0x400;
    if (arg3 == 0) {
        limit = 0x300;
    }

    for (i = 0; i < 12; i++) {
        ang = base + ((i << 12) / 12);
        for (t = 0; t <= limit; ang += 0x73, t += 0x80) {
            if (arg3 == 0) {
                sc->radius = (sc->rx * rcos(t)) >> 12;
                sc->vec.vy = -(sc->ry * rsin(t)) >> 12;
                Gp_RingPointXZ(sc, ang);
            } else {
                sc->vec.vy = -(sc->ry * t) >> 10;
                sc->vec.vx = (sc->rx * rcos(ang)) >> 12;
                sc->vec.vz = (sc->rx * rsin(ang)) >> 12;
            }
            Gp_ProjectRingPt(sc);
            if (t > 0) {
                Gp_LinkRingSeg(sc);
            }
            *(u32*)&sc->sxyPrev = *(u32*)&sc->sxy;
        }
    }

    base = -base;
    for (pass = 0; pass < 2; pass++) {
        if (pass == 0) {
            if (arg3 == 0) {
                sc->radius = (sc->rx * rcos(0x300)) >> 12;
                sc->vec.vy = -(sc->ry * rsin(0x300)) >> 12;
            } else {
                sc->radius = sc->rx;
                sc->vec.vy = -(u16)sc->ry;
            }
        } else {
            sc->vec.vy = 0;
            sc->radius = sc->rx;
        }
        for (i = 0; i < 25; i++) {
            register s32 prod asm("v1");
            register s32 val asm("v0");

            ang        = base + ((i << 12) / 24);
            prod       = sc->radius * rcos(ang);
            val        = prod >> 12;
            sc->vec.vx = val;
            prod       = sc->radius * rsin(ang);
            val        = prod >> 12;
            sc->vec.vz = val;
            Gp_ProjectRingPt(sc);
            if (i != 0) {
                Gp_LinkRingSeg(sc);
            }
            *(u32*)&sc->sxyPrev = *(u32*)&sc->sxy;
        }
    }

    SCRATCH_POP_BYTES(0x60);
}

static void Gp_InitSlot18(s32 arg0, s32 arg1, s32 arg2, s32 arg3)
{
    SVECTOR*    vec;
    GpLinkNode* node;
    GpEnemy*    enemy;
    u16         val;
    s32         idx;
    s32         ry2;
    s32         rx2;

    if (arg0 == 0) {
        if (arg3 == 0) {
            Gp_DrawAimCircle(0, arg1, arg2, 0);
        } else {
            Gp_DrawAimCircle(0, arg1, arg2, 2);
        }
    }

    arg2 += 0x64;
    arg1 += 0x64;
    ry2   = (arg2 * arg2) >> 8;
    rx2   = (arg1 * arg1) >> 8;
    node  = Gp_LinkList;
    SCRATCH_PUSH(SVECTOR);
    vec = SCRATCH_HEAD(SVECTOR);

    if (node != NULL) {
        do {
            if ((node->state.word & 5) != 1) {
                vec->vx = GP_NODE_ENEMY(node)->playerRelPos.vx;
                vec->vy = GP_NODE_ENEMY(node)->playerRelPos.vy;
                vec->vz = GP_NODE_ENEMY(node)->playerRelPos.vz;
                if (arg3 != 0) {
                    vec->vz -= arg1;
                }
                if (vec->vy >= -arg2 && vec->vy < 0x65 && vec->vx >= -arg1 && vec->vx <= arg1 && vec->vz >= -arg1 &&
                    vec->vz <= arg1) {
                    vec->vx >>= 4;
                    vec->vy >>= 4;
                    vec->vz >>= 4;
                    if ((u32)(ry2 * (vec->vx * vec->vx + vec->vz * vec->vz) + rx2 * (vec->vy * vec->vy)) <=
                        (u32)(ry2 * rx2)) {
                        enemy = GP_NODE_ENEMY(node);
                        if (arg0 == 0) {
                            enemy->colorMode |= 0x80;
                        } else {
                            val  = Gp_StateC08.field_0;
                            idx  = (val / 100U - 1) * 9;
                            idx += ((val % 100U) / 10U - 1) * 3;
                            idx += val % 10U;
                            idx += 0x28000;
                            Gp_ClaimSlot18(enemy, idx);
                        }
                    }
                }
            }
            node = node->next;
        } while (node != NULL);
    }

    SCRATCH_POP(SVECTOR);
}

static void func_800A5574(s32 arg0, s32 arg1, s32 arg2, s32 arg3)
{
    SVECTOR*    vec;
    GpLinkNode* node;
    GpEnemy*    enemy;
    GpEnemy*    claim;
    u16         val;
    s32         idx;
    s32         t;
    void*       work;

    if (arg0 == 0) {
        if (arg3 == 0) {
            Gp_DrawAimCircle(0, arg1, arg2, 1);
        } else {
            Gp_DrawAimCircle(0, arg1, arg2, 3);
        }
    }

    arg1 += 0x64;
    arg2 += 0x64;
    node  = Gp_LinkList;
    SCRATCH_PUSH(SVECTOR);
    vec = SCRATCH_HEAD(SVECTOR);

    if (node != NULL) {
        do {
            if ((node->state.word & 5) != 1) {
                vec->vx = (u16)GP_NODE_ENEMY(node)->playerRelPos.vx;
                vec->vy = (u16)GP_NODE_ENEMY(node)->playerRelPos.vy;
                vec->vz = (u16)GP_NODE_ENEMY(node)->playerRelPos.vz;
                if (arg3 != 0) {
                    vec->vz -= arg1;
                }
                t = vec->vy;
                if (t < 0x65 && t >= -arg2) {
                    if ((u32)(vec->vx * vec->vx + vec->vz * vec->vz) <= (u32)(arg1 * arg1)) {
                        work  = GP_NODE_ENEMY(node);
                        enemy = work;
                        claim = work;
                        if (arg0 == 0) {
                            enemy->colorMode |= 0x80;
                        } else {
                            val  = Gp_StateC08.field_0;
                            idx  = (val / 100U - 1) * 9;
                            idx += ((val % 100U) / 10U - 1) * 3;
                            idx += val % 10U;
                            idx += 0x28000;
                            Gp_ClaimSlot18(claim, idx);
                        }
                    }
                }
            }
            node = node->next;
        } while (node != NULL);
    }

    SCRATCH_POP(SVECTOR);
}

/// Draws `val`, clamped at zero, as a right-aligned number at (`x`, `y`).
static inline void _gpDrawHudValue(s32 x, s32 y, s32 color, s32 val)
{
    u8          buf[0x10];
    TextDrawReq req;

    if (val < 0) {
        val = 0;
    }
    req.x          = x;
    req.y          = y;
    req.otIndex    = -2;
    req.field_8    = color;
    req.glyphTable = 0;
    req.centerMode = 2;
    req.field_E    = 3;
    func_8002E53C(&req, Text_ItoaUnsigned(buf, val));
}

/// Draws the "HP" and "MP" captions relative to `obj`'s origin and draw order.
static inline void _gpDrawHudLabels(UiObject* obj, s32 x, s32 y, s32 color)
{
    TextDrawReq hpReq;
    TextDrawReq mpReq;

    hpReq.x          = obj->baseX + 4 + x;
    hpReq.y          = obj->baseY + 8 + y;
    hpReq.otIndex    = obj->drawOrder + 1;
    hpReq.field_8    = color;
    hpReq.glyphTable = 5;
    hpReq.centerMode = 0;
    hpReq.field_E    = 1;
    func_8002E53C(&hpReq, Gp_StrHP);

    mpReq.x          = obj->baseX + 0x2E + x;
    mpReq.y          = obj->baseY + 8 + y;
    mpReq.otIndex    = obj->drawOrder + 1;
    mpReq.field_8    = color;
    mpReq.glyphTable = 5;
    mpReq.centerMode = 0;
    mpReq.field_E    = 1;
    func_8002E53C(&mpReq, Gp_StrMP);
}

static void func_800A57B0(GpIdMapC* arg0)
{
    PadRemapState* remap;
    s32            pendingMp;
    TILE*          tile;
    SPRT *         sp1, *sp3, *sp5;
    POLY_FT4 *     poly1, *poly2;
    DR_TPAGE*      tp;
    PlayerStatus*  cfg;
    s32            pendingHp;
    s32            y;
    s32            hp;
    s32            mp;
    s32            color;
    s32            rectMode;
    s32            iconX, iconY;
    u16*           flags;
    s32            x;
    s32            w1;
    s32            w2;
    s32            i;
    GpStateBE8*    be8;

    cfg       = &Player_Status;
    remap     = Pad_RemapState;
    pendingHp = 0;
    pendingMp = 0;
    if (remap->field_A != 0) {
        return;
    }

    if (cfg->hp < Gp_HpMpWork.field_0) {
        Gp_HpMpWork.field_0 = Gp_HpMpWork.field_0 - 1;
    } else if (Gp_HpMpWork.field_0 < cfg->hp) {
        Gp_HpMpWork.field_0 = Gp_HpMpWork.field_0 + 1;
    }
    be8 = &Gp_HpMpWork;
    if (cfg->mp < be8->field_4) {
        be8->field_4 = be8->field_4 - 1;
    } else if (be8->field_4 < cfg->mp) {
        be8->field_4 = be8->field_4 + 1;
    }

    x  = -0x98;
    y  = -0x64;
    y -= gDisplayState.vramYOffset;
    if (gGameSession->hudShakeY > 0) {
        y -= gGameSession->hudShakeY * 3;
    }

    if (cfg->peStateFlags & 0x80) {
        pendingHp = arg0->field_10 << 1;
    } else {
        pendingMp = arg0->field_10;
    }

    color = 0x606060;
    hp    = Gp_HpMpWork.field_0;
    mp    = Gp_HpMpWork.field_4;

    _gpDrawHudValue(x + 0x2B, y + 0xA, color, cfg->hp);
    _gpDrawHudValue(x + 0x56, y + 0xA, color, cfg->mp);

    {
        UiObject obj;

        obj.drawOrder = -3;
        obj.baseX     = 0;
        obj.baseY     = 0;
        obj.mode      = 0;
        _gpDrawHudLabels(&obj, x, y, color);
    }

    if (hp > 0) {
        if (cfg->hpMax > 0) {
            if (hp >= pendingHp) {
                w1 = (hp - pendingHp) * 0x25 / cfg->hpMax;
                if (w1 >= 0x26) {
                    w1 = 0x25;
                } else if (w1 < 0) {
                    w1 = 0;
                }
            } else {
                w1 = 0;
            }
            w2 = hp * 0x25 / cfg->hpMax;
            if (w2 >= 0x26) {
                w2 = 0x25;
            }
            if (w1 > 0) {
                tile           = (TILE*)gGpuPrimCursor;
                gGpuPrimCursor = tile + 1;
                tile->x0       = x + 5;
                tile->y0       = y + 0xE;
                tile->h        = 2;
                setlen(tile, 3);
                PRIM_COLOR_WORD(tile, 0) = PRIM_RGBC(0x1f, 0x74, 0x01, 0);
                setcode(tile, 0x60);
                tile->w = w1;
                addPrim(gGpuCurrentOt - 2, tile);
            }
            if (w2 - w1 > 0) {
                tile           = (TILE*)gGpuPrimCursor;
                gGpuPrimCursor = tile + 1;
                {
                    s32 tileX = w1 + 5;
                    tile->x0  = x + tileX;
                }
                tile->y0                 = y + 0xE;
                tile->h                  = 2;
                PRIM_COLOR_WORD(tile, 0) = PRIM_RGBC(0xff, 0xff, 0, 0);
                setlen(tile, 3);
                setcode(tile, 0x60);
                tile->w = w2 - w1;
                addPrim(gGpuCurrentOt - 2, tile);
            }
        }
    }

    if (cfg->mpMax <= 0) {
        w1 = 0;
        w2 = w1;
    } else {
        if (mp >= pendingMp) {
            w1 = (mp - pendingMp) * 0x25 / cfg->mpMax;
            if (w1 >= 0x26) {
                w1 = 0x25;
            } else if (w1 < 0) {
                w1 = 0;
            }
        } else {
            w1 = 0;
        }
        w2 = mp * 0x25 / cfg->mpMax;
        if (w2 >= 0x26) {
            w2 = 0x25;
        }
    }
    if (w1 > 0) {
        tile           = (TILE*)gGpuPrimCursor;
        gGpuPrimCursor = tile + 1;
        tile->x0       = x + 0x30;
        tile->y0       = y + 0xE;
        tile->h        = 2;
        setlen(tile, 3);
        PRIM_COLOR_WORD(tile, 0) = PRIM_RGBC(0x1f, 0x74, 0x01, 0);
        setcode(tile, 0x60);
        tile->w = w1;
        addPrim(gGpuCurrentOt - 2, tile);
    }
    if (w2 - w1 > 0) {
        tile           = (TILE*)gGpuPrimCursor;
        gGpuPrimCursor = tile + 1;
        {
            s32 tileX = w1 + 0x30;
            tile->x0  = x + tileX;
        }
        tile->y0                 = y + 0xE;
        tile->h                  = 2;
        PRIM_COLOR_WORD(tile, 0) = PRIM_RGBC(0xff, 0xff, 0, 0);
        setlen(tile, 3);
        setcode(tile, 0x60);
        tile->w = w2 - w1;
        addPrim(gGpuCurrentOt - 2, tile);
    }

    {
        s32 left  = x + 4;
        s32 yb    = y + 0xB;
        s32 clut  = 0x3C0B;
        s32 right = x + 0x23;
        s32 y3    = y + 0x13;

        sp1            = (SPRT*)gGpuPrimCursor;
        gGpuPrimCursor = sp1 + 1;
        sp1->x0        = left;
        sp1->y0        = yb;
        sp1->u0        = 0x98;
        sp1->v0        = 0x68;
        sp1->clut      = clut;
        setlen(sp1, 3);
        setcode(sp1, 0x75);
        addPrim(gGpuCurrentOt - 2, sp1);

        sp1            = (SPRT*)gGpuPrimCursor;
        gGpuPrimCursor = sp1 + 1;
        sp1->x0        = right;
        sp1->y0        = yb;
        sp1->u0        = 0xA8;
        sp1->v0        = 0x68;
        sp1->clut      = clut;
        setlen(sp1, 3);
        setcode(sp1, 0x75);
        addPrim(gGpuCurrentOt - 2, sp1);

        poly1          = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = poly1 + 1;
        poly1->x2      = x + 0xC;
        poly1->x0      = x + 0xC;
        poly1->x3      = right;
        poly1->x1      = right;
        poly1->y1      = yb;
        poly1->y0      = yb;
        poly1->y3      = y3;
        poly1->y2      = y3;
        poly1->u0      = 0xA0;
        poly1->v0      = 0x68;
        poly1->u1      = 0xA8;
        poly1->v1      = 0x68;
        poly1->u2      = 0xA0;
        poly1->v2      = 0x70;
        poly1->u3      = 0xA8;
        poly1->v3      = 0x70;
        poly1->clut    = clut;
        poly1->tpage   = 0x3E;
        setlen(poly1, 9);
        setcode(poly1, 0x2D);
        addPrim(gGpuCurrentOt - 2, poly1);

        sp3            = (SPRT*)gGpuPrimCursor;
        sp3->x0        = left;
        gGpuPrimCursor = sp3 + 1;
        sp3->y0        = yb;
        sp3->u0        = 0x98;
        sp3->v0        = 0x68;
        sp3->clut      = clut;
        sp3->x0       += 0x2B;
        setlen(sp3, 3);
        setcode(sp3, 0x75);
        addPrim(gGpuCurrentOt - 2, sp3);

        sp3            = (SPRT*)gGpuPrimCursor;
        sp3->x0        = right;
        gGpuPrimCursor = sp3 + 1;
        sp3->u0        = 0xA8;
        sp3->v0        = 0x68;
        setlen(sp3, 3);
        setcode(sp3, 0x75);
        sp3->x0  += 0x2B;
        sp3->y0   = yb;
        sp3->clut = clut;
        addPrim(gGpuCurrentOt - 2, sp3);

        poly2          = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = poly2 + 1;
        poly2->x2      = x + 0x37;
        poly2->x0      = x + 0x37;
        poly2->x3      = x + 0x4E;
        poly2->x1      = x + 0x4E;
        poly2->y1      = yb;
        poly2->y0      = yb;
        poly2->y3      = y3;
        poly2->y2      = y3;
        poly2->u0      = 0xA0;
        poly2->v0      = 0x68;
        poly2->u1      = 0xA8;
        poly2->v1      = 0x68;
        poly2->u2      = 0xA0;
        poly2->v2      = 0x70;
        poly2->u3      = 0xA8;
        poly2->v3      = 0x70;
        poly2->clut    = clut;
        poly2->tpage   = 0x3E;
        setlen(poly2, 9);
        setcode(poly2, 0x2D);
        addPrim(gGpuCurrentOt - 2, poly2);
    }

    tp             = gGpuPrimCursor;
    gGpuPrimCursor = tp + 1;
    setlen(tp, 1);
    tp->code[0] = 0xE100023E;
    addPrim(gGpuCurrentOt - 2, tp);

    {
        RECT rect;

        rect.x   = x;
        rect.y   = y;
        rect.w   = 0x5A;
        rect.h   = 0x14;
        rectMode = 2;
        if (cfg->peStateFlags != 0) {
            rectMode = 4;
        }
        Ui_DrawTextInRect(&rect, -1, rectMode, NULL);
    }

    if (cfg->peStateFlags != 0) {
        GpHudStatusBits statusBits;

        iconX      = x;
        iconY      = y + 0x14;
        statusBits = D_8009389C;
        for (i = 0; i < 7; i++) {
            flags = statusBits.bits;
            if (cfg->peStateFlags & flags[i]) {
                sp5            = (SPRT*)gGpuPrimCursor;
                gGpuPrimCursor = sp5 + 1;
                sp5->x0        = iconX;
                iconX         += 0xD;
                sp5->y0        = iconY;
                sp5->u0        = i * 0x10 + 0x60;
                sp5->w         = 0xE;
                sp5->h         = 0xE;
                sp5->v0        = 0x40;
                sp5->clut      = 0x3C08;
                setlen(sp5, 4);
                setcode(sp5, 0x65);
                addPrim(gGpuCurrentOt - 2, sp5);
            }
        }
        Ui_InsertDrawTPage(-2, 0);
    }

    if (Gp_ActorSlots[1] != NULL) {
        if (Mc_SaveData[0].companionType != 2) {
            Gp_DrawHudNumbers(0x2D, -0x64, Mc_SaveData[0].companionHp, Mc_SaveData[0].companionHpMax, 0);
        }
    }
}

static void func_800A63B4(s32 arg0, s32 arg1, s32 arg2)
{
    SPRT_8* p;
    s32     otIdx;
    s32     u;

    otIdx          = 0;
    arg0          -= 6;
    p              = (SPRT_8*)gGpuPrimCursor;
    arg1          -= 8;
    gGpuPrimCursor = p + 1;
    p->x0          = arg0;
    p->y0          = arg1;
    if (arg2 == 1) {
        goto case1;
    }
    if (arg2 >= 2) {
        goto default_case;
    }
    if (arg2 != 0) {
        goto default_case;
    }
    p->u0 = 0xA0;
    p->v0 = 0x88;
    goto after_uv;
case1:
    u = 0xA8;
    goto store;
default_case:
    otIdx = -1;
    u     = 0xA0;
store:
    p->u0 = u;
    p->v0 = 0x80;
after_uv:
    p->clut = 0x3C0D;
    setlen(p, 3);
    setcode(p, 0x77);
    addPrim(gGpuCurrentOt + otIdx - 2, p);
}

static void Gp_DrawHudSprites(GpIdMapC* arg0)
{
    GpXformScratch* block;
    GpLinkNode*     node;
    s32             mode;
    s32             x;
    s32             cx;
    s32             cy;
    s32             y;
    s16             vx;
    s32             vz;
    s32             i;
    s32             n;
    s32             sy;
    DR_TPAGE*       tp;
    SPRT*           sp;
    SPRT*           sp2;
    POLY_GT4*       poly;

    x  = 0x61;
    y  = -0x6C;
    y -= gDisplayState.vramYOffset;
    cx = x + 0x23;
    cy = y + 0x23;
    func_800A63B4(cx, cy, 0);
    node  = Gp_LinkList;
    block = SCRATCH_PUSH(GpXformScratch);
    mode  = func_800B9D80(0x400);
    if (node != NULL) {
        do {
            if ((node->state.word & 5) != 1) {
                block->vec.vx = GP_NODE_ENEMY(node)->playerRelPos.vx;
                block->vec.vz = GP_NODE_ENEMY(node)->playerRelPos.vz;
                block->vec.vy = 0;
                if (mode == 0) {
                    gte_lddp(0x1555);
                    gte_ldsv(&block->vec);
                    gte_gpf12();
                    gte_stsv(&block->vec);
                } else {
                    gte_lddp(0xAAA);
                    gte_ldsv(&block->vec);
                    gte_gpf12();
                    gte_stsv(&block->vec);
                }
                if (node->state.b.flags & 1) {
                    goto next;
                }
                vx = block->vec.vx;
                if (vx < -0x1300 || vx > 0x1300) {
                    goto next;
                }
                vz = block->vec.vz;
                if (vz > 0x1300) {
                    goto next;
                }
                if (vz < -0x1300) {
                    goto next;
                }
                if (vx * vx + vz * vz > 0x168FFFF) {
                    goto next;
                }
                block->vec.vx = (s16)(vx + 0x80) >> 8;
                vz            = (s16)(block->vec.vz + 0x80) >> 8;
                block->vec.vz = vz;
                if (node->state.b.targeted != 0) {
                    func_800A63B4(cx + block->vec.vx, cy - vz, 2);
                } else {
                    func_800A63B4(cx + block->vec.vx, cy - vz, 1);
                }
            }
        next:
            node = node->next;
        } while (node != NULL);
    }
    sy = arg0->field_18;
    if (mode == 0) {
        sy *= 2;
    }
    tp             = gGpuPrimCursor;
    gGpuPrimCursor = tp + 1;
    setlen(tp, 1);
    tp->code[0] = 0xE100023E;
    addPrim(gGpuCurrentOt - 2, tp);
    tp             = gGpuPrimCursor;
    gGpuPrimCursor = tp + 1;
    setlen(tp, 1);
    tp->code[0] = 0xE100023E;
    addPrim(gGpuCurrentOt - 3, tp);
    if (mode == 1) {
        sp             = (SPRT*)gGpuPrimCursor;
        gGpuPrimCursor = sp + 1;
        sp->x0         = x + 0xD;
        sp->y0         = y + 0xC;
        sp->h          = 0x28;
        sp->w          = 0x28;
        sp->u0         = 0x60;
        sp->v0         = 0xC0;
        sp->clut       = 0x3C0C;
        setlen(sp, 4);
        setcode(sp, 0x65);
        addPrim(gGpuCurrentOt - 2, sp);
    }
    poly                     = (POLY_GT4*)gGpuPrimCursor;
    gGpuPrimCursor           = poly + 1;
    PRIM_COLOR_WORD(poly, 2) = PRIM_RGBC(0xc0, 0xc0, 0xc0, 0);
    PRIM_COLOR_WORD(poly, 3) = PRIM_RGBC(0x80, 0x80, 0x80, 0);
    PRIM_COLOR_WORD(poly, 0) = PRIM_RGBC(0x40, 0x40, 0x40, 0);
    PRIM_COLOR_WORD(poly, 1) = PRIM_RGBC(0x30, 0x30, 0x30, 0);
    poly->x1 = poly->x3 = x + 0x40;
    poly->y2 = poly->y3 = y + 0x40;
    poly->tpage         = 0x1E;
    poly->clut          = 0x3C0C;
    setUV4(poly, 0x60, 0x80, 0xA0, 0x80, 0x60, 0xC0, 0xA0, 0xC0);
    setPolyGT4(poly);
    poly->x0 = poly->x2 = x;
    poly->y0 = poly->y1 = y;
    addPrim(gGpuCurrentOt - 2, poly);
    if (arg0->field_16 != -1) {
        sp2            = (SPRT*)gGpuPrimCursor;
        gGpuPrimCursor = sp2 + 1;
        sp2->x0        = x + 0xD;
        sp2->y0        = y + 0xC;
        sp2->h         = 0x28;
        sp2->w         = 0x28;
        if (arg0->field_16 != 4) {
            if (arg0->field_16 == 2) {
                sp2->u0 = 0x88;
            } else {
                sp2->u0 = 0xD8;
            }
        } else {
            sp2->u0 = 0xB0;
        }
        sp2->v0   = 0xC0;
        sp2->clut = 0x3C82;
        setlen(sp2, 4);
        setcode(sp2, 0x67);
        addPrim(gGpuCurrentOt - 3, sp2);
        Ui_InsertDrawTPage(-3, 1);
        n = sy;
        if (n > 0x1300) {
            n = 0x1300;
        }
        n >>= 8;
        n  -= 4;
        if (n <= 0) {
            n = 1;
        }
        for (i = 0; i < 0x10; i++) {
            if (i < n) {
                D_80114BB0[i] = 0x9E06;
            } else {
                D_80114BB0[i] = 0;
            }
            if (i == n && i != 0xF) {
                D_80114BB0[i] = 0x8D03;
            }
        }
        D_80114BD0.x = 0x20;
        D_80114BD0.y = 0xF2;
        D_80114BD0.w = 0x10;
        D_80114BD0.h = 1;
        LoadImage(&D_80114BD0, (u_long*)D_80114BB0);
        arg0->field_16 = -1;
    }
    SCRATCH_POP(GpXformScratch);
}

static void Gp_DrawHudNumbers(s32 x, s32 y, s32 cur, s32 max, s32 kind)
{
    GpHudBarScratch s;
    TextDrawReq     req;
    TILE*           tile;
    SPRT*           sp;
    POLY_FT4*       poly;
    s32             span;
    s32             right;
    s32             w;
    s32             order;

    span = 0x25;
    if (cur < 0) {
        cur = 0;
    }
    y -= gDisplayState.vramYOffset;
    if (Pad_RemapState->field_A != 0) {
        return;
    }

    order           = -3;
    s.obj.baseX     = 0;
    s.obj.baseY     = 0;
    s.obj.drawOrder = order;
    s.obj.mode      = 0;

    req.x          = x + 4;
    req.y          = y + 8;
    req.otIndex    = -2;
    req.field_8    = 0x606060;
    req.glyphTable = 5;
    req.centerMode = 0;
    req.field_E    = 1;
    func_8002E53C(&req, Gp_StrHP);

    if (max >= 0) {
        s32 val = cur;

        if (kind == 0) {
            s32 tx = x + 0x2B;
            s32 ty = y + 0xA;

            if (val < 0) {
                val = 0;
            }
            s.text.req.x          = tx;
            s.text.req.y          = ty;
            s.text.req.otIndex    = -2;
            s.text.req.field_8    = 0x606060;
            s.text.req.glyphTable = 0;
            s.text.req.centerMode = 2;
            s.text.req.field_E    = 3;
            func_8002E53C(&s.text.req, Text_ItoaUnsigned(s.text.buf, val));
        } else {
            s32 tx = x + 0x33;
            s32 ty = y + 0xA;

            if (val < 0) {
                val = 0;
            }
            s.text.req.x          = tx;
            s.text.req.y          = ty;
            s.text.req.otIndex    = -2;
            s.text.req.field_8    = 0x606060;
            s.text.req.glyphTable = 0;
            s.text.req.centerMode = 2;
            s.text.req.field_E    = 3;
            func_8002E53C(&s.text.req, Text_ItoaUnsigned(s.text.buf, val));
            span = 0x2D;
        }

        if (max == 0) {
            w = span;
        } else {
            w = cur * span / max;
        }

        if (w > 0) {
            tile           = (TILE*)gGpuPrimCursor;
            gGpuPrimCursor = tile + 1;
            if (span < w) {
                w = span;
            }
            tile->x0 = x + 5;
            tile->y0 = y + 0xE;
            tile->w  = w;
            tile->h  = 2;
            if (kind == 0) {
                PRIM_COLOR_WORD(tile, 0) = PRIM_RGBC(0x1f, 0x74, 0x01, 0);
            } else {
                PRIM_COLOR_WORD(tile, 0) = PRIM_RGBC(0x80, 0, 0, 0);
            }
            setlen(tile, 3);
            setcode(tile, 0x60);
            addPrim(gGpuCurrentOt - 2, tile);
        }

        sp             = (SPRT*)gGpuPrimCursor;
        gGpuPrimCursor = sp + 1;
        sp->x0         = x + 4;
        sp->u0         = 0x98;
        sp->y0         = y + 0xB;
        sp->v0         = 0x68;
        sp->clut       = 0x3C0B;
        setlen(sp, 3);
        setcode(sp, 0x75);
        addPrim(gGpuCurrentOt - 2, sp);

        sp             = (SPRT*)gGpuPrimCursor;
        gGpuPrimCursor = sp + 1;
        right          = (span + x) - 2;
        sp->x0         = right;
        sp->y0         = y + 0xB;
        sp->clut       = 0x3C0B;
        sp->u0         = 0xA8;
        sp->v0         = 0x68;
        setlen(sp, 3);
        setcode(sp, 0x75);
        addPrim(gGpuCurrentOt - 2, sp);

        poly           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = poly + 1;
        poly->x0 = poly->x2 = x + 0xC;
        poly->x1 = poly->x3 = right;
        poly->y2 = poly->y3 = y + 0x13;
        poly->u2 = poly->u0 = 0xA0;
        poly->v3 = poly->v2 = 0x70;
        poly->tpage         = 0x3E;
        poly->y0 = poly->y1 = y + 0xB;
        poly->v0            = 0x68;
        poly->u1            = 0xA8;
        poly->v1            = 0x68;
        poly->clut          = 0x3C0B;
        poly->u3            = 0xA8;
        setlen(poly, 9);
        setcode(poly, 0x2D);
        addPrim(gGpuCurrentOt - 2, poly);
    } else {
        s.bar.req.x          = x + 0x33;
        s.bar.req.y          = y + 0xA;
        s.bar.req.otIndex    = -2;
        s.bar.req.field_8    = 0x37A78;
        s.bar.req.glyphTable = 0;
        s.bar.req.centerMode = 2;
        s.bar.req.field_E    = 3;
        func_8002E53C(&s.bar.req, D_800938AC);
        span = 0x2D;
    }

    s.bar.rect.x = x;
    s.bar.rect.y = y;
    s.bar.rect.w = span + 0xA;
    s.bar.rect.h = 0x14;
    Ui_DrawTextInRect(&s.bar.rect, -1, 0x40002, NULL);
}

static void Gp_HudTrackEnemy(GpEnemy* arg0, GpHudTrack* arg1)
{
    register u8*  head asm("v0");
    GpHudScratch* block;
    s32           val;

    head             = SCRATCH_HEAD(u8);
    head             = head - 0x1C;
    block            = (GpHudScratch*)head;
    SCRATCH_HEAD(u8) = head;
    if (func_800B9D80(0x100000) != 0) {
        block->field_14 = 0x6A;
        block->field_16 = -0x35;
    } else {
        block->field_14 = 0x6A;
        block->field_16 = -0x64;
    }
    if (arg1->field_0 != arg0) {
        arg1->field_0 = arg0;
        arg1->field_4 = block->field_14;
        arg1->field_4 = block->field_16;
    } else {
        block->field_18   = block->field_14 - arg1->field_4;
        block->field_1A   = block->field_16 - arg1->field_6;
        block->field_18 >>= 3;
        block->field_1A >>= 3;
        block->field_14   = arg1->field_4 + block->field_18;
        block->field_16   = arg1->field_6 + block->field_1A;
    }
    if (arg0->param != NULL) {
        val = arg0->param->hpMax;
        if (arg0->node.state.b.flags & 8) {
            val = -1;
        }
        Gp_DrawHudNumbers(block->field_14 - 8, block->field_16, arg0->hp, val, 1);
    }
    arg1->field_4 = block->field_14;
    SCRATCH_POP_BYTES(0x1C);
    arg1->field_6 = block->field_16;
}

/// Rotates `v` in place by `m` on the GTE, reading it through a copy.
static inline void _gpRotateVector(MATRIX* m, SVECTOR* v)
{
    SVECTOR tmp;

    tmp = *v;
    gte_ApplyMatrixSV(m, &tmp, v);
}

static void Gp_UpdateLinkXforms(void)
{
    GpLinkNode*     node;
    Task*           slot;
    GpCoord*        player;
    GpXformScratch* block;

    node = Gp_LinkList;
    slot = gameGetPtrSlot(3);
    if (slot == NULL) {
        return;
    }
    player = slot->extra.tmd->coords;
    block  = SCRATCH_PUSH(GpXformScratch);
    TransposeMatrix(&player->workm, &block->mat);
    for (; node != NULL; node = node->next) {
        if ((node->state.word & 5) == 1) {
            continue;
        }
        block->vec.vx = GP_NODE_ENEMY(node)->bodyPos.vx;
        block->vec.vy = GP_NODE_ENEMY(node)->bodyPos.vy;
        block->vec.vz = GP_NODE_ENEMY(node)->bodyPos.vz;
        _gpRotateVector(&GP_NODE_ENEMY(node)->coord->workm, &block->vec);
        block->vec.vx += GP_NODE_ENEMY(node)->coord->workm.t[0];
        block->vec.vy += GP_NODE_ENEMY(node)->coord->workm.t[1];
        block->vec.vz += GP_NODE_ENEMY(node)->coord->workm.t[2];
        block->vec.vx -= player->workm.t[0];
        block->vec.vy -= player->workm.t[1];
        block->vec.vz -= player->workm.t[2];
        _gpRotateVector(&block->mat, &block->vec);
        GP_NODE_ENEMY(node)->playerRelPos.vx = block->vec.vx;
        GP_NODE_ENEMY(node)->playerRelPos.vy = block->vec.vy;
        GP_NODE_ENEMY(node)->playerRelPos.vz = block->vec.vz;
    }
    SCRATCH_POP(GpXformScratch);
}

void Gp_StartAreaBgm(s16* arg0)
{
    s16*          dest;
    register s32  three asm("s2");
    GameSession*  sess;
    GameSession*  next;
    PlayerStatus* cfg;
    s8            type;
    u8            mode;

    dest  = arg0;
    cfg   = &Player_Status;
    mode  = gGameSession->restartMode;
    three = 3;
    if (mode == three) {
        return;
    }
    if (mode == 0xFF) {
        return;
    }
    if ((CdCmd_IsIdle() & 0xFFFF) == 0) {
        return;
    }
    if (*dest != 0) {
        return;
    }
    sess = gGameSession;
    if (sess->areaBgmCountdown != 0x7F) {
        sess->areaBgmCountdown = (u8)sess->areaBgmCountdown - 1;
        next                   = gGameSession;
        if (next->areaBgmCountdown >= 0) {
            return;
        }
        if (cfg->hp <= 0) {
            SndEvt_EnqueueType6((next->deathVariant << 16) | 0x70000001, 0, 0);
        } else {
            type = Mc_SaveData[0].companionType;
            if (type == 1) {
                SndEvt_EnqueueType6(((next->deathVariant + 0x31) << 16) | 0x70000001, 0, 0);
            } else if (type == three) {
                SndEvt_EnqueueType7(0x50000000, 1);
                SndEvt_EnqueueType6(0x55170008, 0, 0);
            }
        }
    }
    *dest = 1;
}

u8* Gp_GetAttachLevels(void)
{
    PlayerStatus* p;
    s32           cond;

    p = &Player_Status;
    if ((GP_LOC_WORD(gGameSession->at4.loc) & GP_LOC_STAGE_AREA) != GP_LOC_KEY(1, 20, 0, 0)) {
        cond = 0;
    } else {
        cond = p->field_26 == 4;
    }
    if (cond == 0) {
        return Mc_SaveData[0].attachLevels;
    }
    return Gp_DebugAttachLevels;
}

s32 Gp_IsDebugAttachRoom(void)
{
    PlayerStatus* p;

    p = &Player_Status;
    if ((GP_LOC_WORD(gGameSession->at4.loc) & GP_LOC_STAGE_AREA) != GP_LOC_KEY(1, 20, 0, 0)) {
        return 0;
    }
    return p->field_26 == 4;
}

s32 Gp_IsStateF0Active(void)
{
    GpStateF0* p;

    p = &Gp_StateF0;
    if ((p->field_0 == 1 && p->field_6 != 0) || p->field_1 != 0) {
        return 1;
    }
    return 0;
}

static s32 func_800A7550(void)
{
    Gp_ApplyAttachStats(1, 0);
    return 0;
}

void Gp_ResetHudFx(GpIdMapC* arg0)
{
    PlayerStatus* cfg;
    GpStateBE8*   be8;
    GpStateC08*   p;

    cfg                     = &Player_Status;
    be8                     = &Gp_HpMpWork;
    be8->field_0            = cfg->hp;
    be8->field_4            = cfg->mp;
    arg0->field_16          = -1;
    arg0->field_18          = 0;
    p                       = &Gp_StateC08;
    p->field_10             = 0;
    p->field_C              = 0;
    p->field_12             = 0;
    p->field_D              = 0;
    p->field_E              = 0;
    p->field_14             = 0;
    p->field_F              = 0;
    p->field_16             = 0;
    p->field_17             = 0;
    p->field_A              = 0;
    gGameSession->field_126 = 0;
    Gp_ItemGrantCooldown    = 0;
    gDisplayState.loadBusy  = 1;
    p->field_6             &= ~2;
}

static void Gp_StartPadReplay(void)
{
    DisplayState* ds;

    srand(1);
    Gp_LcgState              = 0;
    ds                       = &gDisplayState;
    ds->animFrame            = 0;
    gDisplayState.frameCount = 0;
    ds->gameTick             = 0;
    ds->loopCount            = 0;
    ds->vsyncCount           = 0;
    ds->field_10             = 0;
    if (ds->demoScene == 0x10) {
        Gp_ReplayCursor = (u16*)0x80600E4C;
    } else {
        Gp_ReplayCursor = (u16*)((u8*)D_8005C374 + 0xD4C);
    }
    Gp_ReplayButtons        = 0xFFFF;
    Gp_ReplayFramesLeft     = 1;
    Pad_RemapState->field_8 = -1;
}

void Gp_PlayClockState2(Task* arg0)
{
    GameSession* session;
    GpFadeWork*  p;

    arg0->killCountdown--;
    if (arg0->killCountdown <= 0) {
        arg0->killCountdown = 0;
        Gp_StartAreaBgm(&arg0->killCountdown);
        session             = gGameSession;
        Gp_StateC08.field_3 = 0;
        if (session->restartMode != 3) {
            p          = &D_80114BD8;
            p->field_0 = 0;
            p->field_1 = 0;
            p->field_2 = (s8)session->field_12E;
            Task_Spawn(1, 0x31, 0, (s32)p);
        }
        arg0->spawnArg1 = 0;
        arg0->state++;
    }
}

void Gp_PlayClockState3(Task* arg0)
{
    Gp_StartAreaBgm(&arg0->killCountdown);
    arg0->spawnArg1++;
    if (arg0->spawnArg1 == 0x40) {
        if (gGameSession->restartMode == 3) {
            gDisplayState.skipDraw = 1;
        }
        arg0->spawnArg1 = 0;
        arg0->state++;
    }
}

void func_800A77B4(Task* arg0)
{
    TaskFuncTable6 sp;

    sp = Gp_PlayClockStates;
    sp.funcs[arg0->state](arg0);
}

static void func_800A7824(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg0 == 0) {
        Gp_DrawAimCircle(0, arg1, arg2, 5);
    }
}

static void Gp_HudTrackSlot0(GpHudTrack* arg0)
{
    GpLinkNode* target;
    Task*       work;
    GameActor*  actor;
    GpLinkNode* node;

    work   = Gp_ActorSlots[0];
    target = NULL;
    if (work != NULL) {
        actor = work->work;
        if (actor != NULL) {
            target = actor->field_90C;
        }
        node = Gp_LinkList;
        if (node != NULL) {
            do {
                if (node == target) {
                    if (!(node->state.b.flags & 1)) {
                        Gp_HudTrackEnemy(GP_NODE_ENEMY(node), arg0);
                        return;
                    }
                }
                node = node->next;
            } while (node != NULL);
        }
    }
}

static s32 Gp_IsStateF0AltClear(void)
{
    return Gp_StateF0.field_1 == 0;
}

void Gp_EnqueueAttach7Cd(void)
{
    Gp_EnqueueSndCd(Gp_GetAttachLevel(7) + 0x15);
}

void Gp_DrawItemObtained(Task* arg0)
{
    UiObject* obj;

    obj = arg0->spawnArg2;
    if (arg0->spawnArg1 == 2) {
        if (arg0->state == 0) {
            Ui_UpdateLayoutSize((UiPanel*)obj, Text_MeasureWidth(Gp_StrBonusItem) + 0xA, 0);
            obj->field_C -= 0xF;
            obj->field_E += 9;
            arg0->state++;
        }
        Text_DrawPrompt(obj, obj->field_1C + 6, 7, Gp_StrBonusItem, 0x606060, 1, 0);
    } else {
        Text_DrawPrompt(obj, obj->field_1C + 6, 7, Gp_StrItemObtained, 0x606060, 1, 0);
    }
}

void Gp_DrawItemTitle(Task* arg0)
{
    UiObject* obj;

    obj           = arg0->spawnArg2;
    obj->field_2E = 0;
    Ui_DrawTitle(obj, Gp_StrItem);
    if (obj->status == 1) {
        if (Pad_CheckButtons(0, 1, Pad_MaskConfirm | Pad_MaskCancel) != 0) {
            obj->field_2E = 6;
        }
    }
}

void Gp_TriggerPeIfArmed(void)
{
    u8 state;

    state = Gp_StateF0.field_0;
    if ((state == 1) || (state == 3)) {
        if (gGameSession->field_126 == 0) {
            Gp_TriggerPeState(1, 0xFF);
            Gp_PulseState1C80();
            gDisplayState.loadBusy = 0;
            Display_InitModeObj(&D_8010CABC, 1, 0, 0x102);
        }
    }
}

static s32 func_800A7AE4(s32 arg0, s32 arg1)
{
    return (arg0 / 3) * 16 + (arg0 % 3) * 4 + arg1 + 0x300;
}

static s32 Gp_GetAttachLevel(s32 arg0)
{
    PlayerStatus* p;
    s32           cond;
    s32           ret;
    u8*           table;

    ret = 1;
    if (arg0 < 0xC) {
        p = &Player_Status;
        if ((GP_LOC_WORD(gGameSession->at4.loc) & GP_LOC_STAGE_AREA) != GP_LOC_KEY(1, 20, 0, 0)) {
            cond = 0;
        } else {
            cond = p->field_26 == 4;
        }
        if (cond == 0) {
            table = Mc_SaveData[0].attachLevels;
        } else {
            table = Gp_DebugAttachLevels;
        }
        ret = table[arg0];
        if (ret == 0) {
            ret = 1;
        }
        if (p->peStateFlags & 0x80) {
            if (ret < 3) {
                ret++;
            }
        }
    }
    return ret;
}

static s32 Gp_StepAttachSlot(s32 arg0, s32 arg1)
{
    PlayerStatus* p;
    McSaveData*   save;
    s32           cond;
    u8*           table;

    p = &Player_Status;
    if ((GP_LOC_WORD(gGameSession->at4.loc) & GP_LOC_STAGE_AREA) != GP_LOC_KEY(1, 20, 0, 0)) {
        cond = 0;
    } else {
        cond = p->field_26 == 4;
    }
    if (cond == 0) {
        table = Mc_SaveData[0].attachLevels;
    } else {
        table = Gp_DebugAttachLevels;
    }
    if (arg1 != 0) {
        save = &Mc_SaveData[0];
        do {
            if (arg1 > 0) {
                do {
                    arg0++;
                    if (arg0 >= 0xC) {
                        arg0 = 0;
                    }
                } while (table[arg0] == 0 && save->cheatMode == 0);
                arg1--;
            } else {
                do {
                    arg0--;
                    if (arg0 < 0) {
                        arg0 += 0xC;
                    }
                } while (table[arg0] == 0 && save->cheatMode == 0);
                arg1++;
            }
        } while (arg1 != 0);
    }
    return arg0;
}

s32 func_800A7CB0(void)
{
    GpStateF0* p;
    s32        cond;

    p = &Gp_StateF0;
    if ((p->field_0 == 1 && p->field_6 != 0) || p->field_1 != 0) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        return 0;
    }
    return 0;
}

static void Gp_EnqueueSndCdIfF0(u8 arg0)
{
    GpStateF0* p;
    s32        cond;

    p = &Gp_StateF0;
    if ((p->field_0 == 1 && p->field_6 != 0) || p->field_1 != 0) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        Gp_EnqueueSndCd(arg0);
    }
}

static s32 Gp_CdIdleIfF0Active(void)
{
    GpStateF0* p;
    s32        cond;

    p = &Gp_StateF0;
    if ((p->field_0 == 1 && p->field_6 != 0) || p->field_1 != 0) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        return CdCmd_IsIdle() & 0xFFFF;
    }
    return 1;
}

void func_800A7DB8(s32 arg0)
{
    if (!(Gp_StateC08.field_6 & 1)) {
        Gp_StateC08.field_E = arg0;
    }
}

void func_800A7DE0(void)
{
    GpStateC08* p;

    CdCmd_EnqueueLoadFile(0, 0, 4);
    p = &Gp_StateC08;
    if (p->field_A >= 2) {
        p->field_3 = 2;
    }
    p->field_E         = 0;
    p->field_A         = 0;
    D_80115768         = 0;
    Gp_StateF0.field_4 = 0;
    p->field_7         = 0;
    p->field_8         = 0;
}

void func_800A7E4C(void)
{
    Gp_ItemGrantCooldown = 5;
}

static s32 func_800A7E5C(s32 arg0)
{
    Task*         work;
    GameActor*    actor;
    PlayerStatus* p;
    s32           flag;

    flag = 0;
    work = Gp_ActorSlots[0];
    if (work != NULL) {
        actor = work->work;
        p     = &Player_Status;
        if (actor->field_954 == 0) {
            if (actor->field_956 == 0 || actor->field_956 == 2) {
                if (gGameSession->dirActionBusy == 0) {
                    if (p->field_24 == 0) {
                        flag = 1;
                    }
                }
            }
        }
    }
    if (arg0 == 0) {
        if (Gp_StateC08.field_6 & 2) {
            flag = 0;
        }
    }
    if (flag != 0) {
        if (Gp_ItemGrantCooldown <= 0) {
            if (Gp_StateF0.field_1 == 0) {
                return 1;
            }
        }
    }
    return 0;
}

void func_800A7F24(void)
{
}

static s32 func_800A7F2C(s32 arg0)
{
    return arg0 - 0x10;
}

s32 Gp_SpendMp(s32 arg0)
{
    PlayerStatus* p;
    s32           ret;

    p   = &Player_Status;
    ret = 1;
    if (p->mp >= arg0) {
        p->mp -= arg0;
    } else {
        p->mp = 0;
        ret   = 0;
    }
    return ret;
}

/// Same math as `Gp_WorldToLocal`, but inlined so the scratch-head address is
/// rematerialised on every access: `out` receives `root->workm` transposed and
/// multiplied by `arg0->workm`, with the translation delta rotated into
/// `out->t`.
static __inline__ void coordToRoot(GpCoord* arg0, GpCoord* root, MATRIX* out)
{
    _GpRelMatScratch* tmp;
    register MATRIX*  rootm asm("a3");
    register MATRIX*  world asm("a2");
    u8*               head;
    VECTOR*           vec;

    Gp_UpdateCoord(arg0);
    Gp_UpdateCoord(root);

    rootm = &root->workm;
    world = &arg0->workm;
    head  = SCRATCH_HEAD(u8);
    tmp   = (_GpRelMatScratch*)(head - 0x30);

    SCRATCH_HEAD(void) = tmp;
    TOUCH_REG3(tmp, rootm, head);

    gte_TransposeMatrix(rootm, &tmp->rot);

    gte_MulMatrix0(&tmp->rot, world, out);

    tmp->delta.vx = world->t[0] - rootm->t[0];
    tmp->delta.vy = world->t[1] - rootm->t[1];
    tmp->delta.vz = world->t[2] - rootm->t[2];
    vec           = (VECTOR*)(head - 0x10);
    ApplyMatrixLV(&tmp->rot, vec, (VECTOR*)out->t);

    SCRATCH_POP_BYTES(0x30);
}

/// Points the active view at `arg0`: the transposed rotation goes to
/// `gGfxViewRotCoord.coord` and the negated translation to `gGfxViewCoord.coord.t`, with
/// `arg1` (optional) stored as the world offset in `Gfx_ViewOffsetCoord.coord.t`.
/// Coordinates that are not direct children of the root are first folded to
/// root space with `coordToRoot`.
static void Gp_SetViewFromCoord(GpCoord* arg0, VECTOR* arg1)
{
    GpCoord* root;
    GpCoord* parent;
    GpCoord  rel;

    if (arg1 != NULL) {
        Gfx_ViewOffsetCoord.coord.t[0] = arg1->vx;
        Gfx_ViewOffsetCoord.coord.t[1] = arg1->vy;
        Gfx_ViewOffsetCoord.coord.t[2] = arg1->vz;
    } else {
        Gfx_ViewOffsetCoord.coord.t[0] = 0;
        Gfx_ViewOffsetCoord.coord.t[1] = 0;
        Gfx_ViewOffsetCoord.coord.t[2] = 0;
    }

    parent = arg0->sub;
    root   = &gGfxViewCoord;
    if (parent == root) {
        gte_TransposeMatrix(&arg0->coord, &gGfxViewRotCoord.coord);
        root->coord.t[0] = -arg0->coord.t[0];
        root->coord.t[1] = -arg0->coord.t[1];
        root->coord.t[2] = -arg0->coord.t[2];
    } else {
        coordToRoot(arg0, root, &rel.coord);
        gte_TransposeMatrix(&rel.coord, &gGfxViewRotCoord.coord);
        root->coord.t[0] = -rel.coord.t[0];
        root->coord.t[1] = -rel.coord.t[1];
        root->coord.t[2] = -rel.coord.t[2];
    }
    arg0->flg = 0;

    Gfx_ViewOffsetCoord.flg = 0;
    gGfxViewRotCoord.flg    = 0;
    gGfxViewCoord.flg       = 0;
}

/// Spawns the type-0xE view task and points its coordinate at the inverse of
/// `arg0` (transposed rotation, negated translation). `arg1` is the optional
/// world offset stored in the task's 0x10-byte payload.
static s32 Gp_SpawnViewCoordTask(GpCoord* arg0, VECTOR* arg1)
{
    GpCoord* coord;
    Task*    task;
    VECTOR*  pos;
    GpCoord* root;
    GpCoord* parent;
    GpCoord  rel;

    task = Task_Spawn(0, 0xE, 0, 0);
    if (task == NULL) {
        return 0;
    }
    pos = memCalloc(sizeof(VECTOR), 0);
    if (pos == NULL) {
        taskKill(task);
        return 0;
    }
    task->work = (TaskIdMap*)pos;
    coord      = task->extra.tmd->coords;
    if (arg1 != NULL) {
        pos->vx = arg1->vx;
        pos->vy = arg1->vy;
        pos->vz = arg1->vz;
    } else {
        pos->vx = 0;
        pos->vy = 0;
        pos->vz = 0;
    }

    parent = arg0->sub;
    root   = &gGfxViewCoord;
    if (parent == root) {
        gte_TransposeMatrix(&arg0->coord, &coord->coord);
        coord->coord.t[0] = -arg0->coord.t[0];
        coord->coord.t[1] = -arg0->coord.t[1];
        coord->coord.t[2] = -arg0->coord.t[2];
    } else {
        coordToRoot(arg0, root, &rel.coord);
        gte_TransposeMatrix(&rel.coord, &coord->coord);
        coord->coord.t[0] = -rel.coord.t[0];
        coord->coord.t[1] = -rel.coord.t[1];
        coord->coord.t[2] = -rel.coord.t[2];
    }
    return 1;
}

void func_800A8654(Task* task)
{
    VECTOR*    vec;
    GpCoord*   src;
    GpCoord*   c1;
    GpCoord*   c2;
    GpCoord*   c3;
    TmdObject* extra;
    s32        i;
    s32        j;

    i              = 0;
    c1             = &Gfx_ViewOffsetCoord;
    extra          = task->extra.tmd;
    vec            = (VECTOR*)task->work;
    src            = extra->coords;
    c1->coord.t[0] = vec->vx;
    c2             = &gGfxViewRotCoord;
    c1->coord.t[1] = vec->vy;
    c1->coord.t[2] = vec->vz;

    for (; i < 3; i++) {
        for (j = 0; j < 3; j++) {
            *(s16*)((i * 6 + j * 2) + (s32)c2->coord.m) = src->coord.m[i][j];
        }
    }

    c3             = &gGfxViewCoord;
    c3->coord.t[0] = src->coord.t[0];
    c3->coord.t[1] = src->coord.t[1];
    c3->coord.t[2] = src->coord.t[2];

    Gfx_ViewOffsetCoord.flg = 0;
    gGfxViewRotCoord.flg    = 0;
    gGfxViewCoord.flg       = 0;
    taskKill(task);
}

void Gp_LoadStageView(void)
{
    GpAreaKey* sess;
    GpViewTbl* tbl;
    GpViewRec* recs;
    GpViewRec* rec;
    GpCoord*   c1;
    MATRIX*    rot;
    VECTOR3*   trans;
    u8         idx;

    sess = &gGameSession->at4.loc;
    tbl  = Gp_ViewTables[sess->stage - 1];
    recs = tbl->field_0[sess->area - 1];
    idx  = Gp_GetViewIndex();

    rot   = &gGfxViewRotCoord.coord;
    trans = MATRIX_TRANS(&gGfxViewCoord.coord);
    c1    = &Gfx_ViewOffsetCoord;
    rec   = (GpViewRec*)(idx * sizeof(GpViewRec) + (s32)recs);

    *(GBytes18*)rot = *(GBytes18*)(rec - 1);
    *trans          = *(VECTOR3*)&(rec - 1)->mtx.t;

    rec--;

    c1->coord.t[0] = 0;
    c1->coord.t[1] = 0;
    c1->coord.t[2] = 0;

    gDisplayState.screenDistance = rec->field_20;
    gte_SetGeomScreen(rec->field_20);
    gte_SetGeomOffset(0, 0);

    Gfx_ViewOffsetCoord.flg                 = 0;
    PARENT_OF(rot, GpCoord, coord)->flg     = 0;
    PARENT_OF(trans, GpCoord, coord.t)->flg = 0;
}

void Gp_WorldToLocal(MATRIX* arg0, MATRIX* arg1, MATRIX* arg2)
{
    _GpRelMatScratch* tmp;

    tmp                            = SCRATCH_HEAD(_GpRelMatScratch) - 1;
    SCRATCH_HEAD(_GpRelMatScratch) = tmp;

    gte_TransposeMatrix(arg0, &tmp->rot);

    gte_MulMatrix0(&tmp->rot, arg1, arg2);

    tmp->delta.vx = arg1->t[0] - arg0->t[0];
    tmp->delta.vy = arg1->t[1] - arg0->t[1];
    tmp->delta.vz = arg1->t[2] - arg0->t[2];
    ApplyMatrixLV(&tmp->rot, &tmp->delta, (VECTOR*)arg2->t);

    SCRATCH_POP(_GpRelMatScratch);
}

s32 Gp_TrySpawnViewTask(s32 arg0)
{
    return Task_Spawn(0, 0xF, 0, arg0) != NULL;
}

void Gp_ApplyView(GpViewRec* arg0)
{
    GpCoord* c1;
    MATRIX*  rot;
    VECTOR3* trans;

    rot   = &gGfxViewRotCoord.coord;
    trans = MATRIX_TRANS(&gGfxViewCoord.coord);
    c1    = &Gfx_ViewOffsetCoord;

    *(GBytes18*)rot = *(GBytes18*)arg0;
    *trans          = *MATRIX_TRANS(&arg0->mtx);

    c1->coord.t[0] = 0;
    c1->coord.t[1] = 0;
    c1->coord.t[2] = 0;

    gDisplayState.screenDistance = arg0->field_20;
    gte_SetGeomScreen(arg0->field_20);
    gte_SetGeomOffset(0, 0);

    Gfx_ViewOffsetCoord.flg                 = 0;
    PARENT_OF(rot, GpCoord, coord)->flg     = 0;
    PARENT_OF(trans, GpCoord, coord.t)->flg = 0;
}

static void Gp_ResetView(void)
{
    MATRIX*           m;
    volatile GpCoord* c1;
    GpCoord*          c2;
    GpCoord*          c3;
    s32               one;

    c1             = &Gfx_ViewOffsetCoord;
    one            = ONE;
    c1->coord.t[0] = 0;
    c1->coord.t[1] = 0;
    c1->coord.t[2] = one;

    *(volatile s32*)&gGfxViewRotCoord.coord = one;
    m                                       = &gGfxViewRotCoord.coord;
    c2                                      = PARENT_OF(m, GpCoord, coord);
    MATRIX_PAIR(m, 1, 1)                    = one;
    m->m[2][2]                              = one;

    c3                   = &gGfxViewCoord;
    MATRIX_PAIR(m, 0, 2) = 0;
    MATRIX_PAIR(m, 2, 0) = 0;
    c3->coord.t[0]       = 0;
    c3->coord.t[1]       = 0;
    c3->coord.t[2]       = 0;
    c1->flg              = 0;
    c2->flg              = 0;
    c3->flg              = 0;
}

void Gp_SpawnViewTasks(void)
{
    GpAreaKey* sess;
    GpViewTbl* tbl;
    GpViewRec* recs;
    GpViewRec* rec;
    u8         idx;

    sess = &gGameSession->at4.loc;
    tbl  = Gp_ViewTables[sess->stage - 1];
    recs = tbl->field_0[sess->area - 1];
    idx  = Gp_GetViewIndex();
    rec  = (GpViewRec*)(idx * sizeof(GpViewRec) + (s32)recs);
    Task_Spawn(0, 0xF, 0, (s32)(rec - 1));
    Task_Spawn(0, 0x17, 0, 0);
}

GpViewRec* Gp_GetStageView(GpAreaKey* arg0)
{
    GpViewTbl* tbl;
    GpViewRec* recs;
    u8         idx;

    tbl  = Gp_ViewTables[arg0->stage - 1];
    recs = tbl->field_0[arg0->area - 1];
    idx  = Gp_GetViewIndex();
    return &recs[idx - 1];
}

void Gp_ApplyViewTask(Task* task)
{
    GpCoord*   c1;
    MATRIX*    rot;
    VECTOR3*   trans;
    GpViewRec* rec;

    rot   = &gGfxViewRotCoord.coord;
    trans = MATRIX_TRANS(&gGfxViewCoord.coord);
    c1    = &Gfx_ViewOffsetCoord;
    rec   = task->spawnArg2;

    *(GBytes18*)rot = *(GBytes18*)rec;
    *trans          = *MATRIX_TRANS(&rec->mtx);

    c1->coord.t[0] = 0;
    c1->coord.t[1] = 0;
    c1->coord.t[2] = 0;

    gDisplayState.screenDistance = rec->field_20;
    gte_SetGeomScreen(rec->field_20);
    gte_SetGeomOffset(0, 0);

    Gfx_ViewOffsetCoord.flg                 = 0;
    PARENT_OF(rot, GpCoord, coord)->flg     = 0;
    PARENT_OF(trans, GpCoord, coord.t)->flg = 0;
    taskKill(task);
}

static void func_800A8D5C(void)
{
    VECTOR  vec;
    GpCoord coord;
    s32     one;
    MATRIX* m;

    vec.vx                          = 0;
    vec.vy                          = 0;
    vec.vz                          = ONE;
    one                             = ONE;
    m                               = &coord.coord;
    coord.sub                       = &gGfxViewCoord;
    *(s32*)&coord.coord             = one;
    MATRIX_PAIR(&coord.coord, 0, 2) = 0;
    MATRIX_PAIR(m, 1, 1)            = one;
    MATRIX_PAIR(&coord.coord, 2, 0) = 0;
    m->m[2][2]                      = one;
    coord.coord.t[0]                = 0;
    coord.coord.t[1]                = 0;
    coord.coord.t[2]                = 0;
    Gp_SpawnViewCoordTask(&coord, &vec);
}

void Gp_SpawnCurView(s32 arg0)
{
    GpAreaKey* sess;
    GpViewTbl* tbl;
    GpViewRec* recs;
    GpViewRec* rec;
    u8         idx;

    sess = &gGameSession->at4.loc;
    tbl  = Gp_ViewTables[sess->stage - 1];
    recs = tbl->field_0[sess->area - 1];
    idx  = Gp_GetViewIndex();
    rec  = (GpViewRec*)(idx * sizeof(GpViewRec) + (s32)recs);
    Task_Spawn(0, 0xF, 0, (s32)(rec - 1));
    if (arg0 == 0) {
        Task_Spawn(0, 0x17, 0, 0);
    }
    if (arg0 == 1) {
        Task_SpawnOnDefaultListA(0, 0x17, 0, 0);
    }
}

void Gp_ViewGateTask(Task* task)
{
    GameSession* sess;
    McSaveData*  save;
    CdCmdQueue*  q;
    s32          loc;

    gGameSession->viewReady = 0;
    if (task->state == 0) {
        task->state = 3;
    }
    save = &Mc_SaveData[0];
    if (task->spawnArg1 != save->at4.loc.view) {
        gGameSession->viewDirty = 1;
    }
    sess = gGameSession;
    if (sess->viewDirty != 0) {
        q = &CdCmd_Queue;
        if ((q->field_214 == 0) || (q->field_218 == 0)) {
            sess->at4.loc.view = save->at4.loc.view;
            Pad_SetCooldown(0);
            Gp_SpawnViewTasks();
            if (Display_SpawnWithOtSmall(0, 0x1E, 0, 0) != 0) {
                loc                 = (u8)gGameSession->at4.loc.view;
                task->killCountdown = 2;
                task->spawnArg1     = loc;
                if (task->state == 3) {
                    task->state = 1;
                }
            }
        }
    }
    if (task->state == 1) {
        Display_AcquireRef();
        task->state += 1;
    }
    if (task->state == 2) {
        task->killCountdown--;
        if (task->killCountdown == 0) {
            Display_ReleaseRef();
            gGameSession->viewReady = 1;
            task->state             = 3;
        }
    }
}

void Gp_ViewBeginLoad(Task* task)
{
    DisplayState* ds;
    CdCmdQueue*   q;
    GpAreaKey*    sess;
    u8            param1[8];
    u8            param2[8];

    sess = &gGameSession->at4.loc;
    q    = &CdCmd_Queue;
    if (task->spawnArg1 != 0) {
        gDisplayState.at100.flags.flipMode = 2;
    }
    ds = &gDisplayState;
    if (ds->otBuffer == ds->frameBuffer) {
        DrawSync(0);
        SetDrawStp(&D_80114C50, 0);
        DrawPrim(&D_80114C50);
        ds->at100.flags.flipMode = 2;
        if (q->field_214 != 0) {
            Mdec_ResolveStreamBuffer(&gGameSession->at4.loc.view);
            task->state = 5;
        } else {
            D_80114C40 = Stream_FindSlot(&gGameSession->at4.loc.view, 0, 1);
            if (D_80114C40 >= 0) {
                Gp_FreeSlot4TmdBuffers();
                q->field_210 = 1;
            } else {
                if (q->field_210 != 0) {
                    Gp_ApplyAreaTmdFlags();
                    q->field_210 = 0;
                }
            }
            if ((CdCmd_IsIdle() & 0xFFFF) == 0) {
                CdCmd_ActivatePhase1();
                task->state += 1;
                Gp_EnqueueViewCd(task);
            } else {
                param1[3] = sess->stage;
                param1[2] = sess->area;
                param1[0] = Gp_GetViewIndex();
                param2[0] = 1;
                param2[1] = 0;
                param2[2] = 0;
                param2[3] = 0;
                CdCmd_Enqueue(0x21, param1, param2);
                task->state += 2;
                Gp_ViewLoadImage(task);
            }
        }
    }
}

void Gp_ViewLoadImage(Task* task)
{
    CdCmdQueue*   q;
    s32           i;
    s32           raw;
    s32           target;
    u8            param;
    register s32  type2 asm("v0");
    FsFolderSlot* table;
    FsFolderSlot* slot;

    q = &CdCmd_Queue;
    if (CdCmd_IsIdle() & 0xFFFF) {
        Mem_Set(&q->field_40, 0, 0x10);
        raw    = Gp_GetViewIndex();
        i      = 0;
        table  = D_8006C338;
        target = (u8)raw - 1;
        for (; (u8)i < 50; i++) {
            type2 = 2;
            if (table[(u8)i].field_0 == type2) {
                if (target == (u8)i) {
                    slot = &table[(u8)i];
                    while (Fs_LoadImageChunk(slot->field_4, 1)) {
                    }
                    break;
                }
            }
        }
        CdCmd_SelectMdecBuffer();
        if (D_80114C40 >= 0) {
            task->state++;
            param = (u8)D_80114C40;
            CdCmd_EnqueueReplace(0x61, 0, &param);
            CdCmd_CommitReplace();
            task->killCountdown = 0;
        } else {
            if ((s16)CdCmd_CommitReplace() >= 0) {
                task->state += 2;
            } else {
                task->state = -1;
                Gp_FinishLoadWait(task);
            }
        }
    }
}

static const char            D_8009388C[] = "R1";
static const char            D_80093890[] = "R2";
static const char            D_80093894[] = "%";
static const char            D_80093898[] = "&";
static const GpHudStatusBits D_8009389C   = { { 0x1, 0x2, 0x4, 0x10, 0x20, 0x40, 0x80 } };
/// "????". The three bytes after the terminator are not zero: the original
/// toolchain left them in the alignment gap, so the array is sized to hold them.
static const char D_800938AC[8] = "????\0&!K";
