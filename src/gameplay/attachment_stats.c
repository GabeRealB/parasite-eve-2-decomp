#include "attachments.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "common.h"

#include "gameplay/area_entry.h"
#include "area_entry.h"
#include "gameplay/attachment_state.h"
#include "attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/captions.h"
#include "hud.h"
#include "hud_sprites.h"
#include "item_menu.h"
#include "gameplay/items.h"
#include "items.h"
#include "linked_actors.h"
#include "gameplay/message.h"
#include "gameplay/player_actor.h"
#include "player_actor.h"
#include "gameplay/player_state.h"
#include "gameplay/room_effects.h"
#include "scene_runtime.h"
#include "gameplay/weapon_data.h"
#include "weapon_data.h"
#include "gameplay/world_state.h"
#include "gameplay/world_targets.h"
#include "world_targets.h"

#include "main/display.h"
#include "main/fs.h"
#include "main/mc.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/text.h"
#include "main/ui.h"
#include "main/wipsys.h"

/// Coordinates of one icon in the attachment selection wheel.
typedef struct {
    /* 0x0 */ s16 x;
    /* 0x2 */ s16 y;
} GpWheelPt;
STATIC_ASSERT_SIZEOF(GpWheelPt, 4);

/// Text scratch is reused for the wheel coordinates after drawing the prompt.
typedef struct {
    /* 0x00 */ UiObject obj;
    /* 0x30 */ union {
        struct {
            /* 0x00 */ u8          buf[0x20];
            /* 0x20 */ TextDrawReq req;
        } text;
        GpWheelPt pts[12];
    } u;
    /* 0x60 */ RECT rect;
} GpWheelScratch;
STATIC_ASSERT_SIZEOF(GpWheelScratch, 0x68);

/// Shared player-control flag, independently addressed by gameplay and overlays.
u8 D_80115768;

extern const char D_8009388C[];

extern const char D_80093890[];

extern const char D_80093894[];

extern const char D_80093898[];

static void Gp_DrawItemPrompt(s32 arg0, s32 arg1);

/// Inline copy of `Gp_GetAttachLevel`.
static __inline__ s32 getAttachLevel(s32 idx);

/// Inline copy of `Gp_IsStateF0Active`.
static __inline__ s32 isStateF0Active_(void);

/// Reads column `field` of the `Gp_IdParamHi` row that attach `idx` uses at
/// level `lvl`.
static __inline__ u16 _gpAttachParam(s32 idx, s32 lvl, s32 field);

static s32 Gp_CheckAttachThreshold(s32 arg0);

static void Gp_SetAttachState(s32 arg0);

static __inline__ s32 stepAttachWheelSaved(s32 arg0, s32 arg1, McSaveData* save);

static __inline__ s32 stepAttachWheel(s32 arg0, s32 arg1);

static __inline__ s32 getAttachWheelLevel(s32 idx);

/// Inline copy of `Gp_GetAttachParam` for an explicit slot: parameter `field`
/// of the `Gp_IdParamHi` row for `slot` at its current level.
static __inline__ u16 getAttachWheelParam(s32 slot, s32 field);

static s32 func_800A2104(GpIdMapC* arg0, s32 arg1, s32 arg2);

static void Gp_DrawPeGauge(GpIdMapC* arg0, s32 arg1, s32 arg2);

/// Inline copy of `Gp_GetAttachLevels`.
static __inline__ u8* getAttachLevels(void);

/// Inline copy of `func_800A7E5C(0)`: the HUD category can be swapped only
/// while the player actor is idle, no CD request is pending and the
/// `Gp_ItemGrantCooldown` cooldown has expired.
static __inline__ s32 hudSwapReady(void);

/// Inline copy of `Gp_CdIdleIfF0Active`.
static __inline__ s32 cdIdleIfF0Active_(void);

/// Inline copy of `func_800A7CB0`, which evaluates the same gate as
/// `Gp_IsStateF0Active` but always returns 0.
static __inline__ u8 stateF0Gate_(void);

static void Gp_UseItemTask(GpIdMapC* arg0);

u16 D_80113CFC[8] = {
    100,
    75,
    70,
    70,
    60,
    60,
    50,
    0,
};
u16 D_80113D0C[7][2] = {
    { 100, 100 },
    { 130, 125 },
    { 140, 140 },
    { 140, 125 },
    { 150, 140 },
    { 150, 125 },
    { 160, 140 },
};
u16 D_80113D28[4] = {
    120,
    80,
    80,
    80,
};
u16 D_80113D30[4] = {
    200,
    100,
    120,
    150,
};

GpAttachParam Gp_AttachParams[55] = {
    { .percentages = {
          16,
          4,
          6,
          8 } },
    { .dispatch = { 1, 6, 65, 0 } },
    { .dispatch = { 1, 8, 90, 0 } },
    { .dispatch = { 1, 10, 200, 0 } },
    { .dispatch = { 3, 16, 16, 1 } },
    { .dispatch = { 3, 24, 20, 1 } },
    { .dispatch = { 3, 32, 50, 1 } },
    { .dispatch = { 4, -1, -1, 0 } },
    { .dispatch = { 4, -1, -1, 0 } },
    { .dispatch = { 4, -1, -1, 0 } },
    { .dispatch = { 1, 11, 40, 0 } },
    { .dispatch = { 1, 12, 60, 0 } },
    { .dispatch = { 1, 13, 80, 0 } },
    { .dispatch = { 2, 24, 24, 0 } },
    { .dispatch = { 2, 30, 26, 0 } },
    { .dispatch = { 2, 38, 30, 0 } },
    { .dispatch = { 3, 40, 50, 0 } },
    { .dispatch = { 3, 60, 50, 0 } },
    { .dispatch = { 4, -1, -1, 0 } },
    { .dispatch = { 0, 0, 0, 600 } },
    { .dispatch = { 0, 0, 0, 600 } },
    { .dispatch = { 0, 0, 0, 600 } },
    { .dispatch = { 0, 0, 0, 0 } },
    { .dispatch = { 0, 0, 0, 0 } },
    { .dispatch = { 0, 0, 0, 0 } },
    { .dispatch = { 3, 40, 50, 0 } },
    { .dispatch = { 3, 60, 50, 0 } },
    { .dispatch = { 4, -1, -1, 0 } },
    { .dispatch = { 0, 0, 0, 2250 } },
    { .dispatch = { 0, 0, 0, 2250 } },
    { .dispatch = { 0, 0, 0, 2250 } },
    { .dispatch = { 0, 0, 0, 2250 } },
    { .dispatch = { 0, 0, 0, 2250 } },
    { .dispatch = { 0, 0, 0, 2250 } },
    { .dispatch = { 0, 0, 0, 10 } },
    { .dispatch = { 0, 0, 0, 13 } },
    { .dispatch = { 0, 0, 0, 15 } },
    { .dispatch = { 4, -1, -1, 0 } },
    { .dispatch = { 4, -1, -1, 0 } },
    { .dispatch = { 4, -1, -1, 0 } },
    { .dispatch = { 4, -1, -1, 0 } },
    { .dispatch = { 4, -1, -1, 0 } },
    { .dispatch = { 4, -1, -1, 0 } },
    { .dispatch = { 4, -1, -1, 0 } },
    { .dispatch = { 4, -1, -1, 0 } },
    { .dispatch = { 4, -1, -1, 0 } },
    { .dispatch = { 4, -1, -1, 0 } },
    { .dispatch = { 4, -1, -1, 0 } },
    { .dispatch = { 4, -1, -1, 0 } },
    { .dispatch = { 3, 20, 25, 1 } },
    { .dispatch = { 3, 16, 22, 1 } },
    { .dispatch = { 3, 14, 25, 1 } },
    { .dispatch = { 3, 38, 28, 0 } },
    { .dispatch = { 3, 16, 25, 1 } },
    { .dispatch = { 3, 16, 20, 1 } },
};
GpDmgRow Gp_DmgRows[5] = {
    { { 20, 30, 40, 50, 60 }, { 50, 60, 70, 80, 100 } },
    { { 20, 30, 40, 50, 60 }, { 100, 120, 140, 160, 200 } },
    { { 20, 30, 40, 50, 60 }, { 150, 180, 210, 240, 300 } },
    { { 20, 30, 40, 50, 60 }, { 150, 180, 210, 240, 300 } },
    { { 10, 15, 20, 25, 30 }, { 25, 30, 35, 40, 50 } },
};
u16 D_80113F54[30] = {
    0,
    0,
    1,
    2,
    3,
    3,
    4,
    4,
    4,
    4,
    4,
    4,
    4,
    4,
    4,
    4,
    4,
    4,
    4,
    4,
    4,
    4,
    4,
    4,
    4,
    4,
    4,
    4,
    4,
    4,
};
u16 D_80113F90[6] = {
    100,
    75,
    75,
    60,
    100,
    0,
};

void Gp_ApplyAttachStats(s32 arg0, GpIdMapC* arg1)
{
    PlayerStatus*  p;
    GpStateF0*     state;
    GpRec8*        rec;
    GpAttachParam* row;
    s32            cond;
    s32            ret;
    u8*            table;
    s32            idx;
    s32            val1;
    s32            val2;
    s32            flag;
    s32            temp2;
    s32            temp4;
    u8             kind;

    idx = Gp_StateC08.field_B;
    if (arg0 == 1) {
        idx = Gp_StateC08.field_5;
    }
    if (idx >= 0xC) {
        ret = 1;
    } else {
        p = &Player_Status;
        if ((GAME_LOCATION_WORD(gGameSession->at4.loc) & GAME_LOCATION_STAGE_AREA_MASK) != GAME_LOCATION_KEY(1, 20, 0, 0)) {
            cond = 0;
        } else {
            cond = p->field_26 == 4;
        }
        if (cond == 0) {
            table = Mc_SaveData[0].state.attachLevels;
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
    row             = &Gp_AttachParams[idx * 3];
    rec             = &row[ret].dispatch;
    state           = &Gp_StateF0;
    temp2           = rec->field_2;
    temp4           = rec->field_4;
    state->field_5  = 0;
    state->field_14 = 0;
    val1            = temp2 * 100;
    val2            = temp4 * 100;
    if ((Gp_StateF0.prefix.bytes.field_0 == 1 && state->field_6 != 0) || state->prefix.bytes.field_1 != 0) {
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
                if (arg1 != NULL) {
                    kind           = rec->field_6;
                    arg1->field_18 = val1;
                    arg1->field_16 = kind + 2;
                }
                break;
            case 3:
                func_800A5574(arg0, val1, val2, rec->field_6);
                if (arg1 != NULL) {
                    kind           = rec->field_6;
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
#define DRAW_PROMPT_LABEL(req, dx, line, color, str)          \
    {                                                         \
        req.x          = obj.panel.field_20.u + (dx) + xBase; \
        req.y          = (obj.panel.field_22.u + 9) + (line); \
        req.otIndex    = obj.panel.field_14.s + 1;            \
        req.field_8    = (color);                             \
        req.glyphTable = 5;                                   \
        req.centerMode = 0;                                   \
        req.field_E    = 1;                                   \
        Text_DrawString(&req, (str));                         \
    }

/// Draws a quantity right-aligned on line `line`; an empty count sets `flag`.
#define DRAW_PROMPT_COUNT(req, line, count)                   \
    {                                                         \
        req.field_8    = 0x606060;                            \
        req.glyphTable = 5;                                   \
        req.centerMode = 2;                                   \
        req.field_E    = 0;                                   \
        req.x          = obj.panel.field_20.u + 0x94;         \
        req.y          = (obj.panel.field_22.u + 9) + (line); \
        req.otIndex    = obj.panel.field_14.s + 1;            \
        Text_DrawString(&req, Text_ItoaSigned(buf, (count))); \
        if ((count) == 0) {                                   \
            flag = 1;                                         \
        }                                                     \
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
    height               = 0xE;
    flag                 = 0;
    obj.panel.field_20.u = 0;
    obj.panel.field_22.u = 0;
    obj.panel.field_14.s = -3;
    obj.panel.field_8    = 0;
    xBase                = 0x5F;
    if (slot->attachId != 0xFF) {
        height = 0x18;
    }
    y = 0x64 - height;
    y = y - gDisplayState.vramYOffset;
    if (Mc_SaveData[0].state.buttonLayout != 2) {
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
        if (Mc_SaveData[0].state.buttonLayout != 2) {
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
        if ((GAME_LOCATION_WORD(gGameSession->at4.loc) & GAME_LOCATION_STAGE_AREA_MASK) != GAME_LOCATION_KEY(1, 20, 0, 0)) {
            cond = 0;
        } else {
            cond = p->field_26 == 4;
        }
        if (cond == 0) {
            table = Mc_SaveData[0].state.attachLevels;
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
    if ((p->prefix.bytes.field_0 == 1 && p->field_6 != 0) || p->prefix.bytes.field_1 != 0) {
        return 1;
    }
    return 0;
}

/// Reads column `field` of the `Gp_IdParamHi` row that attach `idx` uses at
/// level `lvl`.
static __inline__ u16 _gpAttachParam(s32 idx, s32 lvl, s32 field)
{
    return Gp_IdParamHi.rows[idx * 3 + lvl].field[field];
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
        if ((cfg->peStateFlags & 0x10) || (!(cfg->peStateFlags & 0x80) && cfg->mp < _gpAttachParam(arg0, n, 2) && Mc_SaveData[0].state.cheatMode == 0) || (arg0 == 6 && Gp_StateC08.field_16 != 0 && Gp_StateC08.field_17 != 0) || (arg0 == 7 && cfg->hpMax == cfg->hp && Mc_SaveData[0].state.cheatMode == 0) || (arg0 == 0xB && D_80115724 >= 3) || ((cfg->peStateFlags & 0x80) && (arg0 >= 6 || _gpAttachParam(arg0, n, 2) * 2 >= cfg->hp))) {
            result = 1;
        }
    }
    return result;
}

static void Gp_SetAttachState(s32 arg0)
{
    GpStateC08* p;
    s32         level;
    s32         idx;
    s32         attachId;
    s32         rowPrefix;
    s8          row;
    s8          column;
    s8          duration;

    Gp_StateC08.field_E = 0;
    if (Gp_StateC08.field_6 & 1) {
        return;
    }
    idx                 = (s8)arg0;
    Gp_StateC08.field_5 = arg0;
    row                 = idx / 3;
    rowPrefix           = (row + 1) * 10 + 1;
    column              = idx % 3;
    attachId            = rowPrefix + column;
    attachId           *= 10;
    level               = getAttachLevel(idx);
    attachId           += level;

    p          = &Gp_StateC08;
    p->field_0 = attachId;
    p->field_3 = -2;
    duration   = Gp_GetAttachParam(3);
    p->field_2 = duration;
    if (duration <= 0) {
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
    if ((GAME_LOCATION_WORD(gGameSession->at4.loc) & GAME_LOCATION_STAGE_AREA_MASK) != GAME_LOCATION_KEY(1, 20, 0, 0)) {
        cond = 0;
    } else {
        cond = p->field_26 == 4;
    }
    if (cond == 0) {
        table = Mc_SaveData[0].state.attachLevels;
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
                } while (table[arg0] == 0 && save->state.cheatMode == 0);
                arg1--;
            } else {
                do {
                    arg0--;
                    if (arg0 < 0) {
                        arg0 += 0xC;
                    }
                } while (table[arg0] == 0 && save->state.cheatMode == 0);
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
    if ((GAME_LOCATION_WORD(gGameSession->at4.loc) & GAME_LOCATION_STAGE_AREA_MASK) != GAME_LOCATION_KEY(1, 20, 0, 0)) {
        cond = 0;
    } else {
        cond = p->field_26 == 4;
    }
    if (cond == 0) {
        table = Mc_SaveData[0].state.attachLevels;
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
                } while (table[arg0] == 0 && save->state.cheatMode == 0);
                arg1--;
            } else {
                do {
                    arg0--;
                    if (arg0 < 0) {
                        arg0 += 0xC;
                    }
                } while (table[arg0] == 0 && save->state.cheatMode == 0);
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
        if ((GAME_LOCATION_WORD(gGameSession->at4.loc) & GAME_LOCATION_STAGE_AREA_MASK) != GAME_LOCATION_KEY(1, 20, 0, 0)) {
            cond = 0;
        } else {
            cond = p->field_26 == 4;
        }
        if (cond == 0) {
            table = Mc_SaveData[0].state.attachLevels;
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
    return Gp_IdParamHi.rows[slot * 3 + lvl].field[field];
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
    if ((GAME_LOCATION_WORD(gGameSession->at4.loc) & GAME_LOCATION_STAGE_AREA_MASK) != GAME_LOCATION_KEY(1, 20, 0, 0)) {
        cond = 0;
    } else {
        cond = cfg->field_26 == 4;
    }
    if (cond == 0) {
        table = Mc_SaveData[0].state.attachLevels;
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

        s.obj.panel.field_14.s  = -3;
        s.u.text.req.x          = arg1 + 7;
        s.u.text.req.y          = arg2 + 0x22;
        s.u.text.req.otIndex    = -2;
        s.obj.panel.field_20.u  = arg1;
        s.obj.panel.field_22.u  = arg2;
        s.obj.panel.field_8     = 0;
        s.u.text.req.field_8    = 0x606060;
        s.u.text.req.glyphTable = 0;
        s.u.text.req.centerMode = 0;
        s.u.text.req.field_E    = 1;
        Text_DrawString(&s.u.text.req, Gp_GetItemText(item, 0, 0));

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
        s.obj.panel.field_20.u = 0x30;
        s.obj.panel.field_22.u = 0;
        s.obj.panel.field_14.s = -3;
        s.obj.panel.field_8    = 0;
        pts                    = s.u.pts;
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

static void Gp_DrawPeGauge(GpIdMapC* arg0, s32 arg1, s32 arg2)
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
            tile           = gGpuPrimCursor;
            gGpuPrimCursor = tile + 1;
            tile->x0       = arg1 + 0x18;
            tile->y0       = arg2 + 0x21;
            tile->w        = Gp_StateC08.field_2;
            tile->h        = 1;
            setlen(tile, 3);
            GPU_PRIMITIVE_COLOR_WORD(tile, 0) = PRIM_RGBC(0, 0xc0, 0xff, 0);
            setcode(tile, 0x60);
            addPrim(gGpuCurrentOt - 2, tile);
        }

        if (n < 0xB) {
            n = 0xB;
        }

        sp             = gGpuPrimCursor;
        gGpuPrimCursor = sp + 1;
        sp->x0         = arg1 + 0x15;
        sp->y0         = arg2 + 0x1D;
        sp->u0         = 0x98;
        sp->v0         = 0x68;
        sp->clut       = 0x3C0B;
        setlen(sp, 3);
        setcode(sp, 0x77);
        addPrim(gGpuCurrentOt - 2, sp);

        sp2            = gGpuPrimCursor;
        gGpuPrimCursor = sp2 + 1;
        sp2->x0        = n + arg1 + 0x13;
        sp2->y0        = arg2 + 0x1D;
        sp2->u0        = 0xA8;
        sp2->v0        = 0x68;
        sp2->clut      = 0x3C0B;
        setlen(sp2, 3);
        setcode(sp2, 0x77);
        addPrim(gGpuCurrentOt - 2, sp2);

        poly           = gGpuPrimCursor;
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

        cat                  = Gp_StateC08.field_5;
        order                = -3;
        obj.panel.field_20.u = 0;
        obj.panel.field_22.u = 0;
        obj.panel.field_14.s = order;
        obj.panel.field_8    = 0;
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
    if ((GAME_LOCATION_WORD(gGameSession->at4.loc) & GAME_LOCATION_STAGE_AREA_MASK) != GAME_LOCATION_KEY(1, 20, 0, 0)) {
        cond = 0;
    } else {
        cond = p->field_26 == 4;
    }
    if (cond == 0) {
        return Mc_SaveData[0].state.attachLevels;
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
            if (Gp_StateF0.prefix.bytes.field_1 == 0) {
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
    if ((p->prefix.bytes.field_0 == 1 && p->field_6 != 0) || p->prefix.bytes.field_1 != 0) {
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
    if ((p->prefix.bytes.field_0 == 1 && p->field_6 != 0) || p->prefix.bytes.field_1 != 0) {
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
        if ((ok != 0 && (gGameSession->padPrev & 0x10) && gDisplayState.pendingMode == DISPLAY_MODE_NONE &&
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
        Gp_DrawPeGauge(arg0, x, y);
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
                if (Mc_SaveData[0].state.attachUseCounts[Gp_StateC08.field_5] < 0x270F) {
                    Mc_SaveData[0].state.attachUseCounts[Gp_StateC08.field_5]++;
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
    kind  = GAME_LOCATION_WORD(gGameSession->at4.loc);
    kind &= GAME_LOCATION_STAGE_AREA_MASK;
    cfg   = &Player_Status;
    ds    = &gDisplayState;
    if (ds->demoScene != DISPLAY_DEMO_NONE) {
        poly           = gGpuPrimCursor;
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

        poly           = gGpuPrimCursor;
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
        if (ds->pendingMode == DISPLAY_MODE_NONE) {
            if (ds->holdState >= 0) {
                ds->pendingMode = b;
            }
        }
        arg0->field_14 = 0;
    }

    slot = gameGetPtrSlot(1);
    if (slot != NULL) {
        if (slot->spawnArg1.value != Mc_SaveData[0].state.at4.loc.view) {
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
            if (d2->demoScene == DISPLAY_DEMO_NONE) {
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
        if (Gp_StateF0.prefix.bytes.field_1 != 0) {
            goto after;
        }
        if (d2->pendingMode != DISPLAY_MODE_NONE) {
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
                if (Gp_StateF0.prefix.bytes.field_1 == 0) {
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
                if ((GAME_LOCATION_WORD(gGameSession->at4.loc) & GAME_LOCATION_STAGE_AREA_MASK) != GAME_LOCATION_KEY(1, 20, 0, 0)) {
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

        c08                         = &Gp_StateC08;
        d3                          = &gDisplayState;
        c08->field_3                = 0;
        d3->suppressDisconnectPause = 1;
        state                       = arg0->field_0;
        if (state != 1) {
            goto other;
        }
        sub = arg0->field_4;
        if (sub == 0) {
            s32 k;

            if (bad != 0) {
                goto tail;
            }
            k             = GAME_LOCATION_WORD(gGameSession->at4.loc);
            k            &= GAME_LOCATION_STAGE_AREA_MASK;
            arg0->field_8 = 0;
            if (k != GAME_LOCATION_KEY(1, 20, 0, 0)) {
                Display_InitModeObj(&D_8010CAB0, 0, arg0, 0x100);
            } else {
                arg0->field_4 = arg0->field_4 + 1;
            }
            Gp_StateC08.field_A = 0;
            goto tail;
        }
        if (sub == state) {
            f0                          = &Gp_StateF0;
            b                           = f0->prefix.bytes.field_1;
            d3->suppressDisconnectPause = 0;
            if (b != 0) {
                if (f0->field_4 == 0) {
                    f0->prefix.bytes.field_1 = b - 1;
                }
                n = f0->prefix.bytes.field_1;
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

                if (kind != GAME_LOCATION_KEY(1, 20, 0, 0)) {
                    goto tail;
                }
                session = gGameSession;
                if (session->field_126 == 0) {
                    goto tail;
                }
                f0->prefix.bytes.field_0 = 0;
                f0->field_6              = 0;
                session->field_126       = 0;
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
            c = p->prefix.bytes.field_1;
            if (c != 0) {
                if (p->field_4 == 0) {
                    p->prefix.bytes.field_1 = c - 1;
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
            c   = p2->prefix.bytes.field_1;
            if (c != 0) {
                if (p2->field_4 == 0) {
                    p2->prefix.bytes.field_1 = c - 1;
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
                func_80108874(w);
            } else {
                if (flags & 0x40) {
                    Gp_DispatchMsg(w, 0x3F1, 2, 0);
                }
            }
            p    = &Player_Status;
            item = p->weapon + 0x7F;
            Gp_FillRelated(item, 0);
            Gp_FillRelated(item, 1);
            if ((GAME_LOCATION_WORD(gGameSession->at4.loc) & GAME_LOCATION_STAGE_AREA_MASK) != GAME_LOCATION_KEY(1, 20, 0, 0)) {
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
            if ((GAME_LOCATION_WORD(gGameSession->at4.loc) & GAME_LOCATION_STAGE_AREA_MASK) != GAME_LOCATION_KEY(1, 20, 0, 0)) {
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
            if (d4->demoScene != DISPLAY_DEMO_NONE) {
                d4->gameMode = DISPLAY_GAME_RESTART;
            zero:
                arg0->field_4 = 0;
                arg0->field_0 = 0;
            } else {
                Display_InitModeObj(&D_8010CABC, 0, arg0, 0);
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

    if (Gp_StateF0.prefix.bytes.field_1 != 0) {
        if (Gp_StateF0.field_4 == 0) {
            Gp_StateF0.prefix.bytes.field_1 = Gp_StateF0.prefix.bytes.field_1 - 1;
        }
    }
    p = &Gp_StateF0;
    m = Gp_StateF0.prefix.bytes.field_0;
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

const char            D_8009388C[]  = "R1";
const char            D_80093890[]  = "R2";
const char            D_80093894[]  = "%";
const char            D_80093898[]  = "&";
const GpHudStatusBits D_8009389C    = { { 0x1, 0x2, 0x4, 0x10, 0x20, 0x40, 0x80 } };
const char            D_800938AC[8] = "????\0&!K";
