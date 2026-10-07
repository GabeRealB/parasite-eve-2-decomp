#include "attachments.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "common.h"

#include "gameplay/area_entry.h"
#include "area_entry.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/captions.h"
#include "gameplay/actor_presentation.h"
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
#include "gameplay/scene_combat.h"
#include "weapon_data.h"
#include "world_targets.h"

#include "main/display.h"
#include "main/fs.h"
#include "main/mc.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/text.h"
#include "main/ui.h"
#include "main/wipsys.h"

/// Marks an `_AttachmentWheelPoint` that is not waiting to be drawn.
///
/// It lies below every cosine, so the search for the nearest remaining point
/// never picks it.
#define ATTACHMENT_WHEEL_POINT_RETIRED (-0x7FFF)

/// Where one spell's icon sits on the Parasite Energy selection wheel.
///
/// The learned spells are spread evenly round a unit circle seen almost edge
/// on, the highlighted one at angle 0, nearest the viewer. Both coordinates
/// are 4.12 fixed point (4096 is the radius), and the wheel is flattened into
/// screen pixels only when an icon is drawn. Icons are drawn nearest first, so
/// `y` is also the key that picks the next one; a point is retired with
/// `ATTACHMENT_WHEEL_POINT_RETIRED` once drawn.
typedef struct {
    s16 x; // Sine of the icon's angle: its offset to the right of the wheel's centre
    s16 y; // Cosine of the icon's angle: its nearness, shown as an offset down the screen; `ATTACHMENT_WHEEL_POINT_RETIRED` once drawn
} _AttachmentWheelPoint;
STATIC_ASSERT_SIZEOF(_AttachmentWheelPoint, 4);

/// Stack storage the Parasite Energy wheel uses twice while drawing one frame.
///
/// The highlighted spell's caption is drawn first and needs `text`. It is
/// finished before any icon is placed, so the same bytes then hold `points`:
/// one entry per learned spell, counted in steps round the wheel from the
/// highlighted one, with the entries past the learned count retired. The two
/// uses never overlap in time.
typedef union {
    struct {
        u8          costDigits[0x20];                     // The spell's cast cost as decimal text
        TextDrawReq nameRequest;                          // Placement and style of the spell's name
    } text;
    _AttachmentWheelPoint points[ATTACHMENT_SPELL_COUNT]; // Where each icon sits
} _AttachmentWheelScratch;
STATIC_ASSERT_SIZEOF(_AttachmentWheelScratch, 0x30);

/// Shared player-control flag, independently addressed by gameplay and overlays.
u8 D_80115768;

extern const char D_8009388C[];

extern const char D_80093890[];

extern const char D_80093894[];

extern const char D_80093898[];

static void Gp_DrawItemPrompt(s32 arg0, s32 arg1);

/// Inline copy of `attachmentGetEffectiveLevel`.
static __inline__ s32 getAttachLevel(s32 idx);

/// Inline copy of `sceneIsBattleActive`.
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

static s32 func_800A2104(HudState* hud, s32 arg1, s32 arg2);

static void Gp_DrawPeGauge(HudState* hud, s32 arg1, s32 arg2);

/// Inline copy of `attachmentGetLearnedLevels`.
static __inline__ u8* getAttachLevels(void);

/// Inline copy of `_hudCanSwitchCategory`: normal or aimed locomotion permits
/// a HUD category switch after input and battle-end delays expire, provided no
/// direction action or interaction press is active. `ignoreSwapLock` bypasses
/// `ATTACHMENT_FLAG_SWAP_LOCK`, as `Gp_HudTask` does for START in battle.
static __inline__ s32 hudSwapReady(s32 ignoreSwapLock);

/// Inline copy of `_attachmentIsBattleSoundLoadReady`.
static __inline__ s32 cdIdleIfF0Active_(void);

/// Inline copy of `attachmentSoundLoadStub`, which evaluates the same gate as
/// `sceneIsBattleActive` but always returns 0.
static __inline__ u8 stateF0Gate_(void);

static void Gp_UseItemTask(HudState* hud);

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

u16 D_80113D38[4] = {
    16,
    4,
    6,
    8,
};

AttachmentAreaRow Gp_AttachParams[ATTACHMENT_AREA_ABILITY_COUNT][ATTACHMENT_AREA_LEVEL_COUNT] = {
    {
        { .area = { ATTACHMENT_AREA_PROJECTILE, 6, 65, 0 } },
        { .area = { ATTACHMENT_AREA_PROJECTILE, 8, 90, 0 } },
        { .area = { ATTACHMENT_AREA_PROJECTILE, 10, 200, 0 } },
    },
    {
        { .area = { ATTACHMENT_AREA_CYLINDER, 16, 16, 1 } },
        { .area = { ATTACHMENT_AREA_CYLINDER, 24, 20, 1 } },
        { .area = { ATTACHMENT_AREA_CYLINDER, 32, 50, 1 } },
    },
    {
        { .area = { ATTACHMENT_AREA_ALL, -1, -1, 0 } },
        { .area = { ATTACHMENT_AREA_ALL, -1, -1, 0 } },
        { .area = { ATTACHMENT_AREA_ALL, -1, -1, 0 } },
    },
    {
        { .area = { ATTACHMENT_AREA_PROJECTILE, 11, 40, 0 } },
        { .area = { ATTACHMENT_AREA_PROJECTILE, 12, 60, 0 } },
        { .area = { ATTACHMENT_AREA_PROJECTILE, 13, 80, 0 } },
    },
    {
        { .area = { ATTACHMENT_AREA_ELLIPSOID, 24, 24, 0 } },
        { .area = { ATTACHMENT_AREA_ELLIPSOID, 30, 26, 0 } },
        { .area = { ATTACHMENT_AREA_ELLIPSOID, 38, 30, 0 } },
    },
    {
        { .area = { ATTACHMENT_AREA_CYLINDER, 40, 50, 0 } },
        { .area = { ATTACHMENT_AREA_CYLINDER, 60, 50, 0 } },
        { .area = { ATTACHMENT_AREA_ALL, -1, -1, 0 } },
    },
    {
        { .area = { ATTACHMENT_AREA_SELF, 0, 0, 600 } },
        { .area = { ATTACHMENT_AREA_SELF, 0, 0, 600 } },
        { .area = { ATTACHMENT_AREA_SELF, 0, 0, 600 } },
    },
    {
        { .area = { ATTACHMENT_AREA_SELF, 0, 0, 0 } },
        { .area = { ATTACHMENT_AREA_SELF, 0, 0, 0 } },
        { .area = { ATTACHMENT_AREA_SELF, 0, 0, 0 } },
    },
    {
        { .area = { ATTACHMENT_AREA_CYLINDER, 40, 50, 0 } },
        { .area = { ATTACHMENT_AREA_CYLINDER, 60, 50, 0 } },
        { .area = { ATTACHMENT_AREA_ALL, -1, -1, 0 } },
    },
    {
        { .area = { ATTACHMENT_AREA_SELF, 0, 0, 2250 } },
        { .area = { ATTACHMENT_AREA_SELF, 0, 0, 2250 } },
        { .area = { ATTACHMENT_AREA_SELF, 0, 0, 2250 } },
    },
    {
        { .area = { ATTACHMENT_AREA_SELF, 0, 0, 2250 } },
        { .area = { ATTACHMENT_AREA_SELF, 0, 0, 2250 } },
        { .area = { ATTACHMENT_AREA_SELF, 0, 0, 2250 } },
    },
    {
        { .area = { ATTACHMENT_AREA_SELF, 0, 0, 10 } },
        { .area = { ATTACHMENT_AREA_SELF, 0, 0, 13 } },
        { .area = { ATTACHMENT_AREA_SELF, 0, 0, 15 } },
    },
    {
        { .area = { ATTACHMENT_AREA_ALL, -1, -1, 0 } },
        { .area = { ATTACHMENT_AREA_ALL, -1, -1, 0 } },
        { .area = { ATTACHMENT_AREA_ALL, -1, -1, 0 } },
    },
    {
        { .area = { ATTACHMENT_AREA_ALL, -1, -1, 0 } },
        { .area = { ATTACHMENT_AREA_ALL, -1, -1, 0 } },
        { .area = { ATTACHMENT_AREA_ALL, -1, -1, 0 } },
    },
    {
        { .area = { ATTACHMENT_AREA_ALL, -1, -1, 0 } },
        { .area = { ATTACHMENT_AREA_ALL, -1, -1, 0 } },
        { .area = { ATTACHMENT_AREA_ALL, -1, -1, 0 } },
    },
    {
        { .area = { ATTACHMENT_AREA_ALL, -1, -1, 0 } },
        { .area = { ATTACHMENT_AREA_ALL, -1, -1, 0 } },
        { .area = { ATTACHMENT_AREA_ALL, -1, -1, 0 } },
    },
    {
        { .area = { ATTACHMENT_AREA_CYLINDER, 20, 25, 1 } },
        { .area = { ATTACHMENT_AREA_CYLINDER, 16, 22, 1 } },
        { .area = { ATTACHMENT_AREA_CYLINDER, 14, 25, 1 } },
    },
    {
        { .area = { ATTACHMENT_AREA_CYLINDER, 38, 28, 0 } },
        { .area = { ATTACHMENT_AREA_CYLINDER, 16, 25, 1 } },
        { .area = { ATTACHMENT_AREA_CYLINDER, 16, 20, 1 } },
    },
};
DamageReceivedScaleRow Gp_DmgRows[5] = {
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

void Gp_ApplyAttachStats(s32 arg0, HudState* hud)
{
    PlayerStatus*        p;
    SceneCombatState*    state;
    AttachmentAreaParam* area;
    s32                  cond;
    s32                  ret;
    u8*                  table;
    s32                  idx;
    s32                  radiusWorld;
    s32                  extentWorld;
    s32                  flag;
    s32                  radius;
    s32                  extent;
    u8                   ahead;

    idx = Gp_StateC08.wheelIndex;
    if (arg0 == 1) {
        idx = Gp_StateC08.activeIndex;
    }
    if (idx >= 0xC) {
        ret = 1;
    } else {
        p = &gPlayerStatus;
        if ((GAME_LOCATION_WORD(gGameSession->location.loc) & GAME_LOCATION_STAGE_AREA_MASK) != GAME_LOCATION_KEY(1, 20, 0, 0)) {
            cond = 0;
        } else {
            cond = p->resourceVariant == 4;
        }
        if (cond == 0) {
            table = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.attachLevels;
        } else {
            table = Gp_DebugAttachLevels;
        }
        ret = table[idx];
        if (ret == 0) {
            ret = 1;
        }
        if (p->statusFlags & PLAYER_STATUS_BERSERKER) {
            if (ret < 3) {
                ret++;
            }
        }
    }
    area                 = &Gp_AttachParams[idx][ret - 1].area;
    state                = &gSceneCombatState;
    radius               = area->radius;
    extent               = area->extent;
    state->peTargetCount = 0;
    state->lifeDrainHp   = 0;
    radiusWorld          = radius * 100;
    extentWorld          = extent * 100;
    if ((gSceneCombatState.signals.bytes.battlePhase == SCENE_COMBAT_BATTLE_ENGAGED && state->battleRefs != 0) || state->signals.bytes.endDelayFrames != 0) {
        flag = 1;
    } else {
        flag = 0;
    }
    if (flag != 0) {
        switch (area->shape) {
            case ATTACHMENT_AREA_SELF:
                Gp_UpdateAttachCombo(arg0);
                break;
            case ATTACHMENT_AREA_PROJECTILE:
                func_800A7824(arg0, radiusWorld, extentWorld);
                if (hud != NULL) {
                    hud->radarRangeIcon = HUD_RADAR_RANGE_PROJECTILE;
                    hud->radarRange     = extentWorld;
                }
                break;
            case ATTACHMENT_AREA_ELLIPSOID:
                Gp_InitSlot18(arg0, radiusWorld, extentWorld, area->ahead);
                if (hud != NULL) {
                    ahead               = area->ahead;
                    hud->radarRange     = radiusWorld;
                    hud->radarRangeIcon = ahead + HUD_RADAR_RANGE_AROUND;
                }
                break;
            case ATTACHMENT_AREA_CYLINDER:
                func_800A5574(arg0, radiusWorld, extentWorld, area->ahead);
                if (hud != NULL) {
                    ahead               = area->ahead;
                    hud->radarRange     = radiusWorld;
                    hud->radarRangeIcon = ahead + HUD_RADAR_RANGE_AROUND;
                }
                break;
            case ATTACHMENT_AREA_ALL:
                func_800A4904(arg0);
                if (hud != NULL) {
                    hud->radarRangeIcon = HUD_RADAR_RANGE_AROUND;
                    hud->radarRange     = 0x3FFF;
                }
                break;
        }
        if (hud != NULL) {
            if (idx >= 0xC) {
                hud->radarRangeIcon = HUD_RADAR_RANGE_NONE;
            }
        }
    } else if (idx == 7) {
        Gp_UpdateAttachCombo(arg0);
    }
}

/// Draws one of the prompt's button labels on line `line`, `dx` pixels right of
/// the prompt's left edge.
#define DRAW_PROMPT_LABEL(req, dx, line, color, str)                            \
    {                                                                           \
        req.x          = obj.panel.contentOriginX.unsignedValue + (dx) + xBase; \
        req.y          = (obj.panel.contentOriginY.unsignedValue + 9) + (line); \
        req.otIndex    = obj.panel.otIndex.signedValue + 1;                     \
        req.colorRgb   = (color);                                               \
        req.glyphTable = TEXT_GLYPH_TABLE_SMALL;                                \
        req.alignment  = TEXT_ALIGNMENT_LEFT;                                   \
        req.drawMode   = TEXT_DRAW_OUTLINED;                                    \
        textDrawString(&req, (str));                                            \
    }

/// Draws a quantity right-aligned on line `line`; an empty count sets `flag`.
#define DRAW_PROMPT_COUNT(req, line, count)                                     \
    {                                                                           \
        req.colorRgb   = 0x606060;                                              \
        req.glyphTable = TEXT_GLYPH_TABLE_SMALL;                                \
        req.alignment  = TEXT_ALIGNMENT_RIGHT;                                  \
        req.drawMode   = TEXT_DRAW_FILL_ONLY;                                   \
        req.x          = obj.panel.contentOriginX.unsignedValue + 0x94;         \
        req.y          = (obj.panel.contentOriginY.unsignedValue + 9) + (line); \
        req.otIndex    = obj.panel.otIndex.signedValue + 1;                     \
        textDrawString(&req, textItoaSigned(buf, (count)));                     \
        if ((count) == 0) {                                                     \
            flag = 1;                                                           \
        }                                                                       \
    }

static void Gp_DrawItemPrompt(s32 arg0, s32 arg1)
{
    u8                   buf[0x10];
    UiObject             obj;
    TextDrawReq          req;
    TextDrawReq          req2;
    RECT                 rect;
    PlayerStatus*        cfg;
    EquipmentWeaponLoad* slot;
    s32                  item;
    s32                  count2;
    s32                  count1;
    s32                  height;
    s32                  flag;
    s32                  xBase;
    s32                  y;

    cfg    = &gPlayerStatus;
    slot   = equipmentGetWeaponLoad(cfg->weapon + 0x7F);
    count2 = -1;
    if (Pad_RemapState->hideHud != 0) {
        return;
    }
    if (capIsBusy() != 0) {
        return;
    }
    if (gGameSession->hideHud != 0) {
        return;
    }
    if (cfg->weapon == PLAYER_STATUS_EQUIPMENT_NONE) {
        return;
    }
    item = cfg->weapon + 0x7F;
    if (item == 0x92) {
        return;
    }
    count1 = slot->primaryQty;
    if (slot->secondaryItemId != INVENTORY_ITEM_NONE && slot->secondaryItemId != EQUIPMENT_WEAPON_SECONDARY_UNAVAILABLE) {
        count2 = slot->secondaryQty;
    }
    height                                 = 0xE;
    flag                                   = 0;
    obj.panel.contentOriginX.unsignedValue = 0;
    obj.panel.contentOriginY.unsignedValue = 0;
    obj.panel.otIndex.signedValue          = -3;
    obj.panel.state                        = USER_INTERFACE_PANEL_INITIAL;
    xBase                                  = 0x5F;
    if (slot->secondaryItemId != EQUIPMENT_WEAPON_SECONDARY_UNAVAILABLE) {
        height = 0x18;
    }
    y = 0x64 - height;
    y = y - gDisplayState.vramYOffset;
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.buttonLayout != 2) {
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
    if (slot->primaryItemId != INVENTORY_ITEM_NONE) {
        DRAW_PROMPT_COUNT(req, y, count1);
    } else {
        flag = 1;
    }
    uiDrawRecessedRect(&obj.panel, 0x79, (y + 4), 0x1B, 7, 0x102010);
    if (slot->secondaryItemId != EQUIPMENT_WEAPON_SECONDARY_UNAVAILABLE) {
        flag = 0;
        y   += 0xA;
        if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.buttonLayout != 2) {
            DRAW_PROMPT_LABEL(req2, 4, y, 0x606060, D_80093890);
        } else {
            DRAW_PROMPT_LABEL(req2, 6, y, 0x506030, D_80093898);
        }
        if (slot->secondaryItemId != INVENTORY_ITEM_NONE) {
            DRAW_PROMPT_COUNT(req2, y, count2);
        } else {
            flag = 1;
        }
        uiDrawRecessedRect(&obj.panel, 0x79, (y + 4), 0x1B, 7, 0x102010);
        y -= 0xA;
    }
    rect.w = 0x39;
    rect.x = xBase;
    rect.y = y;
    rect.h = height;
    if (flag == 1) {
        uiDrawRectFrame(&rect, -1, 0x40004, NULL);
    } else {
        uiDrawRectFrame(&rect, -1, 0x40002, NULL);
    }
}

#undef DRAW_PROMPT_LABEL
#undef DRAW_PROMPT_COUNT

/// Inline copy of `attachmentGetEffectiveLevel`.
static __inline__ s32 getAttachLevel(s32 idx)
{
    PlayerStatus* p;
    u8*           table;
    s32           cond;
    s32           lvl;

    if (idx >= 0xC) {
        lvl = 1;
    } else {
        p = &gPlayerStatus;
        if ((GAME_LOCATION_WORD(gGameSession->location.loc) & GAME_LOCATION_STAGE_AREA_MASK) != GAME_LOCATION_KEY(1, 20, 0, 0)) {
            cond = 0;
        } else {
            cond = p->resourceVariant == 4;
        }
        if (cond == 0) {
            table = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.attachLevels;
        } else {
            table = Gp_DebugAttachLevels;
        }
        lvl = table[idx];
        if (lvl == 0) {
            lvl = 1;
        }
        if ((p->statusFlags & PLAYER_STATUS_BERSERKER) && lvl < 3) {
            lvl++;
        }
    }
    return lvl;
}

/// Inline copy of `sceneIsBattleActive`.
static __inline__ s32 isStateF0Active_(void)
{
    SceneCombatState* combat;

    combat = &gSceneCombatState;
    if ((combat->signals.bytes.battlePhase == SCENE_COMBAT_BATTLE_ENGAGED && combat->battleRefs != 0) || combat->signals.bytes.endDelayFrames != 0) {
        return 1;
    }
    return 0;
}

/// Reads column `field` of the `Gp_IdParamHi` row that attach `idx` uses at
/// level `lvl`.
static __inline__ u16 _gpAttachParam(s32 idx, s32 lvl, s32 field)
{
    return Gp_IdParamHi.rows[idx * 3 + lvl].value[field];
}

static s32 Gp_CheckAttachThreshold(s32 arg0)
{
    PlayerStatus* cfg;
    s32           result;
    s32           n;

    cfg    = &gPlayerStatus;
    result = 0;
    n      = getAttachLevel(arg0);

    if (!isStateF0Active_()) {
        if (cfg->mp < _gpAttachParam(arg0, n, ATTACHMENT_LEVEL_CAST_COST) || arg0 != ATTACHMENT_INDEX_HEALING || cfg->hpMax == cfg->hp) {
            result = 1;
        }
    } else if (arg0 < ATTACHMENT_SPELL_COUNT) {
        if ((cfg->statusFlags & PLAYER_STATUS_SILENCE) || (!(cfg->statusFlags & PLAYER_STATUS_BERSERKER) && cfg->mp < _gpAttachParam(arg0, n, ATTACHMENT_LEVEL_CAST_COST) && gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.cheatMode == 0) || (arg0 == ATTACHMENT_INDEX_METABOLISM && Gp_StateC08.mindWard != 0 && Gp_StateC08.bodyWard != 0) || (arg0 == ATTACHMENT_INDEX_HEALING && cfg->hpMax == cfg->hp && gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.cheatMode == 0) || (arg0 == ATTACHMENT_INDEX_ENERGY_BALL && gEnergyBallInFlightCount >= 3) || ((cfg->statusFlags & PLAYER_STATUS_BERSERKER) && (arg0 >= ATTACHMENT_INDEX_METABOLISM || _gpAttachParam(arg0, n, ATTACHMENT_LEVEL_CAST_COST) * 2 >= cfg->hp))) {
            result = 1;
        }
    }
    return result;
}

static void Gp_SetAttachState(s32 arg0)
{
    AttachmentState* attachment;
    s32              level;
    s32              idx;
    s32              attachId;
    s32              rowPrefix;
    s8               row;
    s8               column;
    s8               duration;

    Gp_StateC08.queuedIndex = 0;
    if (Gp_StateC08.flags & ATTACHMENT_FLAG_EVENT_LOCK) {
        return;
    }
    idx                     = (s8)arg0;
    Gp_StateC08.activeIndex = arg0;
    row                     = idx / 3;
    rowPrefix               = (row + 1) * 10 + 1;
    column                  = idx % 3;
    attachId                = rowPrefix + column;
    attachId               *= 10;
    level                   = getAttachLevel(idx);
    attachId               += level;

    attachment              = &Gp_StateC08;
    attachment->attachId    = attachId;
    attachment->effectPhase = ATTACHMENT_EFFECT_HELD;
    duration                = Gp_GetAttachParam(ATTACHMENT_LEVEL_ATP_LOSS);
    attachment->duration    = duration;
    if (duration <= 0) {
        attachment->duration = ATTACHMENT_DURATION_MIN;
    }
    attachment->mode               = ATTACHMENT_MODE_ARMED;
    D_80115768                     = 0;
    gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_RUNNING;
    attachment->soundStep          = ATTACHMENT_SOUND_QUEUED;
    attachment->menuOpen           = ATTACHMENT_MENU_CLOSED;
    D_80114C34                     = 0;
    attachment->flags             &= ATTACHMENT_FLAG_CLEAR_EVENT_LOCK;
}

static __inline__ s32 stepAttachWheelSaved(s32 arg0, s32 arg1, McSaveData* save)
{
    PlayerStatus* p;
    s32           cond;
    u8*           table;

    p = &gPlayerStatus;
    if ((GAME_LOCATION_WORD(gGameSession->location.loc) & GAME_LOCATION_STAGE_AREA_MASK) != GAME_LOCATION_KEY(1, 20, 0, 0)) {
        cond = 0;
    } else {
        cond = p->resourceVariant == 4;
    }
    if (cond == 0) {
        table = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.attachLevels;
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

    p = &gPlayerStatus;
    if ((GAME_LOCATION_WORD(gGameSession->location.loc) & GAME_LOCATION_STAGE_AREA_MASK) != GAME_LOCATION_KEY(1, 20, 0, 0)) {
        cond = 0;
    } else {
        cond = p->resourceVariant == 4;
    }
    if (cond == 0) {
        table = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.attachLevels;
    } else {
        table = Gp_DebugAttachLevels;
    }
    if (arg1 != 0) {
        save = &gMcSaveData[MEMORY_CARD_SAVE_LIVE];
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
        p = &gPlayerStatus;
        if ((GAME_LOCATION_WORD(gGameSession->location.loc) & GAME_LOCATION_STAGE_AREA_MASK) != GAME_LOCATION_KEY(1, 20, 0, 0)) {
            cond = 0;
        } else {
            cond = p->resourceVariant == 4;
        }
        if (cond == 0) {
            table = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.attachLevels;
        } else {
            table = Gp_DebugAttachLevels;
        }
        lvl = table[idx];
        if (lvl == 0) {
            lvl = 1;
        }
        if ((p->statusFlags & PLAYER_STATUS_BERSERKER) && lvl < 3) {
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
    return Gp_IdParamHi.rows[slot * 3 + lvl].value[field];
}

static s32 func_800A2104(HudState* hud, s32 arg1, s32 arg2)
{
    UiObject                obj;
    _AttachmentWheelScratch scratch;
    RECT                    rect;
    s32                     changed;
    s32                     order;
    PlayerStatus*           cfg;
    u8*                     table;
    s32                     cond;
    s32                     count;
    s32                     xOff;
    s32                     yOff;
    s32                     item;
    s32                     param;
    s32                     ret;
    s32                     color;
    _AttachmentWheelPoint*  pts;
    _AttachmentWheelPoint*  dest;
    _AttachmentWheelPoint*  points;
    _AttachmentWheelPoint*  chosen;
    McSaveData*             save;
    s32                     angle;
    s32                     best;
    s32                     flags;
    s32                     px;
    s32                     py;
    s32                     slot;
    s32                     i;
    s32                     j;
    DR_TPAGE*               dr;

    changed              = 0;
    order                = -2;
    gGameSession->uiOpen = 1;
    cfg                  = &gPlayerStatus;
    count                = 0;
    if ((GAME_LOCATION_WORD(gGameSession->location.loc) & GAME_LOCATION_STAGE_AREA_MASK) != GAME_LOCATION_KEY(1, 20, 0, 0)) {
        cond = 0;
    } else {
        cond = cfg->resourceVariant == 4;
    }
    if (cond == 0) {
        table = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.attachLevels;
    } else {
        table = Gp_DebugAttachLevels;
    }
    for (j = 11; j >= 0; j--, table++) {
        if (*table != 0) {
            count++;
        }
    }

    if (hud->wheelTurn > 0) {
        hud->wheelTurn--;
    } else if (hud->wheelTurn < 0) {
        hud->wheelTurn++;
    }

    if (hud->wheelTurn == 0) {
        if (padCheckButtons(0, PAD_BUTTON_QUERY_HELD_ANY, PAD_BUTTON_UP | PAD_BUTTON_DOWN) == 0) {
            if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_RIGHT) != 0) {
                Gp_StateC08.wheelIndex = stepAttachWheel(Gp_StateC08.wheelIndex, 1);
                changed                = 1;
                hud->wheelTurn        += 4;
            } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_LEFT) != 0) {
                Gp_StateC08.wheelIndex = stepAttachWheel(Gp_StateC08.wheelIndex, -1);
                changed                = 1;
                hud->wheelTurn        -= 4;
            }
        }
    }

    if (Gp_StateC08.queuedIndex == 0) {
        xOff                 = arg1 + 2;
        yOff                 = arg2 + 2;
        hud->previewCastCost = getAttachWheelParam(Gp_StateC08.wheelIndex, ATTACHMENT_LEVEL_CAST_COST);

        item  = ((Gp_StateC08.wheelIndex / 3) << 4) + ((Gp_StateC08.wheelIndex % 3) << 2) + 0x300;
        param = getAttachWheelParam(Gp_StateC08.wheelIndex, ATTACHMENT_LEVEL_CAST_COST);
        if (cfg->statusFlags & PLAYER_STATUS_BERSERKER) {
            param <<= 1;
        }

        obj.panel.otIndex.signedValue          = -3;
        scratch.text.nameRequest.x             = arg1 + 7;
        scratch.text.nameRequest.y             = arg2 + 0x22;
        scratch.text.nameRequest.otIndex       = -2;
        obj.panel.contentOriginX.unsignedValue = arg1;
        obj.panel.contentOriginY.unsignedValue = arg2;
        obj.panel.state                        = USER_INTERFACE_PANEL_INITIAL;
        scratch.text.nameRequest.colorRgb      = 0x606060;
        scratch.text.nameRequest.glyphTable    = TEXT_GLYPH_TABLE_MEDIUM;
        scratch.text.nameRequest.alignment     = TEXT_ALIGNMENT_LEFT;
        scratch.text.nameRequest.drawMode      = TEXT_DRAW_OUTLINED;
        textDrawString(&scratch.text.nameRequest, itemGetText(item, ITEM_TEXT_NAME, 0));

        ret   = getAttachWheelLevel(Gp_StateC08.wheelIndex);
        color = 0x606060;
        itemMenuDrawParasiteEnergyLevel(&obj, -0xB, 0x28, ret, color);
        textDrawUiLine(&obj, 0x8E, 0x28, textItoaSigned(scratch.text.costDigits, param), color, TEXT_DRAW_TRANSLUCENT_OUTLINED, TEXT_ALIGNMENT_RIGHT);

        rect.x = arg1;
        rect.y = arg2 + 0x17;
        rect.w = 0x91;
        rect.h = 0x13;
        uiDrawRectFrame(&rect, -1, 0x40002, NULL);

        /* Spread the learned spells evenly round the wheel, turned by the
         * step still in progress; the unused points are retired. */
        obj.panel.contentOriginX.unsignedValue = 0x30;
        obj.panel.contentOriginY.unsignedValue = 0;
        obj.panel.otIndex.signedValue          = -3;
        obj.panel.state                        = USER_INTERFACE_PANEL_INITIAL;
        pts                                    = scratch.points;
        for (i = 0; i < ARRAY_SIZE(scratch.points); i++) {
            if (i < count) {
                angle = ((i * 4 + hud->wheelTurn) << 12) / (count * 4);
                if (count == 1) {
                    angle = 0;
                }
                dest    = &pts[i];
                dest->x = rsin(angle);
                dest->y = rcos(angle);
            } else {
                pts[i].x = ATTACHMENT_WHEEL_POINT_RETIRED;
                pts[i].y = ATTACHMENT_WHEEL_POINT_RETIRED;
            }
        }

        /* Each pass draws the nearest point still waiting, the one with the
         * greatest y, and retires it so that later passes skip it. */
        if (count > 0) {
            i      = 0;
            points = scratch.points;
            save   = &gMcSaveData[MEMORY_CARD_SAVE_LIVE];
            do {
                best  = 0;
                flags = ITEM_MENU_ICON_DEFAULT;
                for (j = 0; j < count; j++) {
                    _AttachmentWheelPoint* nearest = &points[best];

                    if (nearest->y < points[j].y) {
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
                chosen->y = ATTACHMENT_WHEEL_POINT_RETIRED;

                slot = stepAttachWheelSaved(Gp_StateC08.wheelIndex, best, save);

                if (Gp_CheckAttachThreshold(slot) != 0) {
                    flags = ITEM_MENU_ICON_DIMMED;
                }
                if (best == 0 && hud->wheelTurn == 0) {
                    flags |= ITEM_MENU_ICON_HIGHLIGHTED;
                }
                itemMenuDrawItemIcon(&obj, px, py, ((slot / 3) << 4) + ((slot % 3) << 2) + 0x301, flags);
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

static void Gp_DrawPeGauge(HudState* hud, s32 arg1, s32 arg2)
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

    n = Gp_GetAttachParam(ATTACHMENT_LEVEL_ATP_LOSS);
    if (Gp_StateC08.activeIndex < 0xD) {
        if (Gp_StateC08.duration > 0) {
            tile           = gGpuPrimCursor;
            gGpuPrimCursor = tile + 1;
            tile->x0       = arg1 + 0x18;
            tile->y0       = arg2 + 0x21;
            tile->w        = Gp_StateC08.duration;
            tile->h        = 1;
            setlen(tile, 3);
            GPU_PRIMITIVE_COLOR_WORD(tile, 0) = GPU_PACK_COLOR_WORD(0, 0xc0, 0xff, 0);
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

        cat                                    = Gp_StateC08.activeIndex;
        order                                  = -3;
        obj.panel.contentOriginX.unsignedValue = 0;
        obj.panel.contentOriginY.unsignedValue = 0;
        obj.panel.otIndex.signedValue          = order;
        obj.panel.state                        = USER_INTERFACE_PANEL_INITIAL;
        itemMenuDrawItemIcon(&obj, arg1 + 4, arg2 + 0x28, ((cat / 3) << 4) + ((cat % 3) << 2) + 0x301, ITEM_MENU_ICON_DEFAULT);

        dr             = gGpuPrimCursor;
        gGpuPrimCursor = dr + 1;
        setDrawTPage(dr, 0, 1, 0x1E);
        addPrim(gGpuCurrentOt - 2, dr);
    }
}

/// Inline copy of `attachmentGetLearnedLevels`.
static __inline__ u8* getAttachLevels(void)
{
    PlayerStatus* p;
    s32           cond;

    p = &gPlayerStatus;
    if ((GAME_LOCATION_WORD(gGameSession->location.loc) & GAME_LOCATION_STAGE_AREA_MASK) != GAME_LOCATION_KEY(1, 20, 0, 0)) {
        cond = 0;
    } else {
        cond = p->resourceVariant == 4;
    }
    if (cond == 0) {
        return gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.attachLevels;
    }
    return Gp_DebugAttachLevels;
}

/// Inline copy of `_hudCanSwitchCategory`: normal or aimed locomotion permits
/// a HUD category switch after input and battle-end delays expire, provided no
/// direction action or interaction press is active. `ignoreSwapLock` bypasses
/// `ATTACHMENT_FLAG_SWAP_LOCK`, as `Gp_HudTask` does for START in battle.
static __inline__ s32 hudSwapReady(s32 ignoreSwapLock)
{
    Task*         work;
    GameActor*    actor;
    PlayerStatus* p;
    s32           flag;

    flag = 0;
    work = gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER];
    if (work != NULL) {
        actor = work->work;
        p     = &gPlayerStatus;
        if (actor->mode == GAME_ACTOR_MODE_NORMAL) {
            if (actor->state == 0 || actor->state == 2) {
                if (gGameSession->dirActionBusy == 0) {
                    if (p->interactionPressed == 0) {
                        flag = 1;
                    }
                }
            }
        }
    }
    if (ignoreSwapLock == 0) {
        if (Gp_StateC08.flags & ATTACHMENT_FLAG_SWAP_LOCK) {
            flag = 0;
        }
    }
    if (flag != 0) {
        if (Gp_ItemGrantCooldown > 0) {
            return 0;
        }
        if (gSceneCombatState.signals.bytes.endDelayFrames == 0) {
            return 1;
        }
    }
    return 0;
}

/// Inline copy of `_attachmentIsBattleSoundLoadReady`.
static __inline__ s32 cdIdleIfF0Active_(void)
{
    SceneCombatState* combat;
    s32               cond;
    u16               ret;

    combat = &gSceneCombatState;
    if ((combat->signals.bytes.battlePhase == SCENE_COMBAT_BATTLE_ENGAGED && combat->battleRefs != 0) || combat->signals.bytes.endDelayFrames != 0) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        ret = cdCmdIsIdle();
    } else {
        ret = 1;
    }
    return ret;
}

/// Inline copy of `attachmentSoundLoadStub`, which evaluates the same gate as
/// `sceneIsBattleActive` but always returns 0.
static __inline__ u8 stateF0Gate_(void)
{
    SceneCombatState* combat;
    s32               cond;

    combat = &gSceneCombatState;
    if ((combat->signals.bytes.battlePhase == SCENE_COMBAT_BATTLE_ENGAGED && combat->battleRefs != 0) || combat->signals.bytes.endDelayFrames != 0) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        return 0;
    }
    return 0;
}

static void Gp_UseItemTask(HudState* hud)
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

    cfg                  = &gPlayerStatus;
    flag                 = 0;
    hud->previewCastCost = 0;
    if (Gp_StateC08.soundStep == ATTACHMENT_SOUND_QUEUED) {
        if (++D_80114C34 > 0) {
            lvl   = getAttachLevel(Gp_StateC08.activeIndex);
            sndId = Gp_StateC08.activeIndex * 3 + lvl;
            if (isStateF0Active_()) {
                sndLoadEnqueuePeFile(sndId);
            }
            Gp_StateC08.soundStep    = ATTACHMENT_SOUND_PLAYED;
            Gp_StateC08.previewSound = 0;
            D_80114C34               = 0;
        }
    }

    x                    = 9;
    y                    = 0x3C;
    y                   -= gDisplayState.vramYOffset;
    hud->field_E         = 0;
    gGameSession->uiOpen = 0;
    if (Gp_StateC08.flags & ATTACHMENT_FLAG_APPLY_STATS) {
        func_800A7550();
        Gp_StateC08.flags &= ATTACHMENT_FLAG_CLEAR_APPLY_STATS;
    }

    if (Gp_StateC08.mode != ATTACHMENT_MODE_WHEEL) {
        if (Gp_StateC08.antibodyTicks <= 0 || --Gp_StateC08.antibodyTicks <= 0) {
            Gp_StateC08.antibodyCombo = 0;
        }
        if (Gp_StateC08.energyShotTicks <= 0 || --Gp_StateC08.energyShotTicks <= 0) {
            Gp_StateC08.energyShotCombo = 0;
        }
        if (Gp_StateC08.metabolismTicks <= 0 || --Gp_StateC08.metabolismTicks <= 0) {
            Gp_StateC08.metabolismCombo = 0;
        }
    }
    if (Gp_StateC08.mode == ATTACHMENT_MODE_WHEEL) {
        if (gGameSession->padPressed & 0x50) {
            work = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
            if (work != NULL) {
                ((GameActor*)work->work)->padHeld |= 0x40;
            }
            Gp_StateC08.mode               = ATTACHMENT_MODE_IDLE;
            D_80115768                     = 0;
            gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_RUNNING;
            Gp_StateC08.menuOpen           = ATTACHMENT_MENU_CLOSED;
            if (isStateF0Active_()) {
                Gp_DrawItemPrompt(x, y);
            }
            return;
        }
    }

    // Open the wheel from idle when the pad asks, or when a script forces it.
    if (Gp_StateC08.mode == ATTACHMENT_MODE_IDLE && Gp_StateC08.queuedIndex == 0) {
        ok = hudSwapReady(0);
        if ((ok != 0 && (gGameSession->padPressed & 0x10) && gDisplayState.pendingMode == DISPLAY_MODE_NONE &&
             !(Gp_StateC08.flags & ATTACHMENT_FLAG_EVENT_LOCK)) ||
            (Gp_StateC08.flags & ATTACHMENT_FLAG_OPEN_WHEEL)) {
            Gp_StateC08.menuOpen           = ATTACHMENT_MENU_OPEN;
            Gp_StateC08.flags             &= ATTACHMENT_FLAG_CLEAR_OPEN_WHEEL;
            side                           = Gp_StateC08.mode ^ ATTACHMENT_MODE_WHEEL;
            Gp_StateC08.mode               = side;
            D_80115768                     = side;
            gSceneCombatState.actorControl = side;
            if (Gp_StateC08.wheelIndex >= ATTACHMENT_SPELL_COUNT) {
                Gp_StateC08.wheelIndex = 0;
            }
            if (Gp_StateC08.wheelIndex < 0) {
                Gp_StateC08.wheelIndex = 0;
            }
            if (!isStateF0Active_()) {
                if (getAttachLevels()[ATTACHMENT_INDEX_HEALING] != 0) {
                    Gp_StateC08.wheelIndex = ATTACHMENT_INDEX_HEALING;
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

    // Armed and casting. Release starts the countdown; expiry publishes the effect.
    mode = Gp_StateC08.mode;
    if (mode == ATTACHMENT_MODE_ARMED || mode == ATTACHMENT_MODE_CAST) {
        if (Gp_StateC08.flags & ATTACHMENT_FLAG_RELEASE) {
            Gp_StateC08.flags      &= ATTACHMENT_FLAG_CLEAR_RELEASE;
            Gp_StateC08.effectPhase = ATTACHMENT_EFFECT_CHARGE;
            Gp_StateC08.mode        = ATTACHMENT_MODE_CAST;
        }
        Gp_DrawPeGauge(hud, x, y);
        if (Gp_StateC08.mode == ATTACHMENT_MODE_CAST) {
            Gp_StateC08.duration--;
        }
        if (Gp_StateC08.duration <= 0) {
            if (cdIdleIfF0Active_()) {
                Gp_StateC08.mode               = ATTACHMENT_MODE_IDLE;
                D_80115768                     = 0;
                gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_RUNNING;
                Gp_StateC08.menuOpen           = ATTACHMENT_MENU_CLOSED;
                Gp_StateC08.effectPhase        = ATTACHMENT_EFFECT_RELEASED;
                Gp_ItemGrantCooldown           = 0x14;
                cdCmdEnqueueDisplayResource(0, 0, CD_COMMAND_DISPLAY_LOAD_SEEK_CURRENT_VIEW);
                if (cfg->statusFlags & PLAYER_STATUS_BERSERKER) {
                    cfg->hp -= Gp_GetAttachParam(ATTACHMENT_LEVEL_CAST_COST) * 2;
                    if (cfg->hp <= 0) {
                        cfg->hp = 1;
                    }
                } else {
                    cfg->mp -= Gp_GetAttachParam(ATTACHMENT_LEVEL_CAST_COST);
                    if (cfg->mp < 0) {
                        cfg->mp = 0;
                    }
                }
                if (Gp_StateC08.activeIndex >= ATTACHMENT_SPELL_COUNT) {
                    itemSetIdentified(Gp_SelItemRec->itemId, 1);
                    inventoryRemoveItemRow(NULL, Gp_SelItemRec, 0);
                }
                if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.attachUseCounts[Gp_StateC08.activeIndex] < 0x270F) {
                    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.attachUseCounts[Gp_StateC08.activeIndex]++;
                }
                Gp_StateC08.soundStep = ATTACHMENT_SOUND_IDLE;
            } else {
                Gp_StateC08.duration = ATTACHMENT_DURATION_MIN;
            }
            if (Gp_StateC08.duration <= 0) {
                return;
            }
        }

        if ((Gp_StateC08.flags & ATTACHMENT_FLAG_EVENT_LOCK) ||
            (Gp_StateC08.activeIndex < ATTACHMENT_SPELL_COUNT && (gGameSession->padPressed & 0x40))) {
            gGameSession->loadedSndId = 0;
            cdCmdEnqueueDisplayResource(0, 0, CD_COMMAND_DISPLAY_LOAD_SEEK_CURRENT_VIEW);
            if (Gp_StateC08.mode >= ATTACHMENT_MODE_ARMED) {
                Gp_StateC08.effectPhase = ATTACHMENT_EFFECT_CANCELLED;
            }
            Gp_StateC08.queuedIndex        = 0;
            Gp_StateC08.mode               = ATTACHMENT_MODE_IDLE;
            D_80115768                     = 0;
            gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_RUNNING;
            Gp_StateC08.previewSound       = 0;
            Gp_StateC08.soundStep          = ATTACHMENT_SOUND_IDLE;
        }
        return;
    }

    if (func_800A2104(hud, x, y) != 0) {
        flag = 1;
    }
    actor = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->work;
    if ((Gp_StateC08.queuedIndex != 0 && actor->mode == GAME_ACTOR_MODE_SCRIPTED) || (Gp_StateC08.flags & ATTACHMENT_FLAG_EVENT_LOCK)) {
        Gp_StateC08.queuedIndex = 0;
    }
    if ((hud->wheelTurn == 0 && padCheckButtons(0, PAD_BUTTON_QUERY_HELD_ANY, Pad_MaskConfirm) != 0) ||
        Gp_StateC08.queuedIndex != 0) {
        if (cdIdleIfF0Active_()) {
            pad                        = &gPadStates[0];
            mask                       = Pad_MaskConfirm;
            pad->pressedButtons       &= ~mask;
            gGameSession->padPressed  &= ~mask;
            gGameSession->padHeld     &= ~mask;
            gGameSession->padReleased &= ~mask;
            if (Gp_StateC08.queuedIndex != 0) {
                Gp_StateC08.activeIndex = Gp_StateC08.queuedIndex;
                Gp_StateC08.wheelIndex  = Gp_StateC08.queuedIndex;
            } else {
                Gp_StateC08.activeIndex = Gp_StateC08.wheelIndex;
            }
            if (Gp_CheckAttachThreshold(Gp_StateC08.activeIndex) == 0) {
                Gp_SetAttachState(Gp_StateC08.activeIndex);
            }
        }
    }

    if (Gp_StateC08.mode != ATTACHMENT_MODE_IDLE) {
        Gp_ApplyAttachStats(0, hud);
    }
    if (flag) {
        idx                      = getAttachLevel(Gp_StateC08.wheelIndex);
        Gp_StateC08.previewSound = Gp_StateC08.wheelIndex * 3 + idx;
    }
    if (Gp_StateC08.previewSound > 0) {
        if (stateF0Gate_() == 0) {
            Gp_StateC08.previewSound = 0;
        }
    }
}

/// Battle-end step of `Gp_HudTask` that waits for the player's end action to
/// finish (or skips it when the weapon is being re-equipped), refills the
/// weapon's ammunition choices and moves on to the results.
static __inline__ void hudWaitEndAction(HudState* hud)
{
    s32           hit;
    s32           flags;
    s32           item;
    PlayerStatus* p;
    s32           cond;

    SceneCombatState* combat;
    Task*             w;
    s32               c;

    w      = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    hit    = 0;
    combat = &gSceneCombatState;
    c      = combat->signals.bytes.endDelayFrames;
    if (c != 0) {
        if (combat->actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
            combat->signals.bytes.endDelayFrames = c - 1;
        }
    }
    if (w != NULL) {
        if (((GameActor*)w->work)->statePhase == 0x3E8) {
            hit = 1;
        }
    }
    flags = gGameSession->flowFlags;
    if ((flags & GAME_SESSION_FLOW_REEQUIP_WEAPON) == 0) {
        if (w != NULL) {
            if (hit == 0) {
                return;
            }
        }
        func_80108874(w);
    } else {
        if (flags & GAME_SESSION_FLOW_HIDE_REEQUIPPED_WEAPON) {
            taskMessageDispatch(w, GAME_ACTOR_MESSAGE_END_SCRIPTED, 2, 0);
        }
    }
    p    = &gPlayerStatus;
    item = p->weapon + 0x7F;
    equipmentReloadSelectedWeaponConsumable(item, EQUIPMENT_WEAPON_SUPPLY_PRIMARY);
    equipmentReloadSelectedWeaponConsumable(item, EQUIPMENT_WEAPON_SUPPLY_SECONDARY);
    if ((GAME_LOCATION_WORD(gGameSession->location.loc) & GAME_LOCATION_STAGE_AREA_MASK) != GAME_LOCATION_KEY(1, 20, 0, 0)) {
        cond = 0;
    } else {
        cond = p->resourceVariant == 4;
    }
    if (cond == 0 || gGameSession->battleResetPending == 0) {
        hud->suppression = HUD_SUPPRESS_ALL;
    }
    hud->battleStep = hud->battleStep + 1;
}

void Gp_HudTask(HudState* hud)
{
    DisplayState*     ds;
    PlayerStatus*     cfg;
    AttachmentState*  attachment;
    SceneCombatState* combat;
    Task*             slot;
    Task*             work;
    POLY_FT4*         poly;
    s32               stageAreaKey;
    s32               bad;
    s32               inBattle;
    s32               step;
    s32               n;
    s32               b;

    bad           = 0;
    stageAreaKey  = GAME_LOCATION_WORD(gGameSession->location.loc);
    stageAreaKey &= GAME_LOCATION_STAGE_AREA_MASK;
    cfg           = &gPlayerStatus;
    ds            = &gDisplayState;
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

    b                = hud->queuedMenuMode;
    hud->suppression = HUD_SUPPRESS_NONE;
    if (b != 0) {
        if (ds->pendingMode == DISPLAY_MODE_NONE) {
            if (ds->holdState >= 0) {
                ds->pendingMode = b;
            }
        }
        hud->queuedMenuMode = DISPLAY_MODE_NONE;
    }

    slot = gameGetTaskSlot(GAME_TASK_SLOT_VIEW_GATE);
    if (slot != NULL) {
        if (slot->spawnArg1.value != gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view) {
            bad = 1;
        }
    }

    if (bad == 0) {
        DisplayState* d2;

        d2 = &gDisplayState;
        if (d2->holdState >= 0 && (Gp_StateC08.mode == ATTACHMENT_MODE_IDLE || d2->demoScene != DISPLAY_DEMO_NONE) && Gp_ItemGrantCooldown <= 0 && gGameSession->dirActionBusy == 0 &&
            cfg->interactionPressed == 0 && gSceneCombatState.signals.bytes.endDelayFrames == 0 && d2->pendingMode == DISPLAY_MODE_NONE) {
            if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_START) != 0) {
                if (hud->inBattle == 0) {
                    hud->queuedMenuMode = 0x41;
                    hud->suppression    = HUD_SUPPRESS_ALL;
                } else if (hudSwapReady(1) != 0 && gPlayerStatus.armor != PLAYER_STATUS_EQUIPMENT_NONE) {
                    hud->queuedMenuMode = 0x42;
                    hud->suppression    = HUD_SUPPRESS_PARASITE_ENERGY;
                }
            } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_SELECT) != 0) {
                if (hud->inBattle != 0) {
                    PlayerStatus* p;
                    s32           cond;

                    p = &gPlayerStatus;
                    if ((GAME_LOCATION_WORD(gGameSession->location.loc) & GAME_LOCATION_STAGE_AREA_MASK) != GAME_LOCATION_KEY(1, 20, 0, 0)) {
                        cond = 0;
                    } else {
                        cond = p->resourceVariant == 4;
                    }
                    if (cond == 0) {
                        hud->queuedMenuMode = 0x45;
                        hud->suppression    = HUD_SUPPRESS_ALL;
                    }
                } else {
                    hud->queuedMenuMode = DISPLAY_MODE_MAP;
                    hud->suppression    = HUD_SUPPRESS_ALL;
                }
            }
        }
    }

    worldTargetUpdatePlayerRelativePositions();
    {
        DisplayState* d3;

        attachment                  = &Gp_StateC08;
        d3                          = &gDisplayState;
        attachment->effectPhase     = ATTACHMENT_EFFECT_IDLE;
        d3->suppressDisconnectPause = 1;
        inBattle                    = hud->inBattle;
        if (inBattle == 1) {
            step = hud->battleStep;
            if (step == HUD_BATTLE_STEP_START) {
                s32 currentStageAreaKey;

                if (bad == 0) {
                    currentStageAreaKey  = GAME_LOCATION_WORD(gGameSession->location.loc);
                    currentStageAreaKey &= GAME_LOCATION_STAGE_AREA_MASK;
                    hud->field_8         = 0;
                    if (currentStageAreaKey != GAME_LOCATION_KEY(1, 20, 0, 0)) {
                        displayQueueModeTask(&D_8010CAB0, 0, hud, STAGE_ENTRY_GRAY_CAPTURE);
                    } else {
                        hud->battleStep = hud->battleStep + 1;
                    }
                    Gp_StateC08.mode = ATTACHMENT_MODE_IDLE;
                }
            } else if (step == HUD_BATTLE_STEP_FIGHT) {
                combat                      = &gSceneCombatState;
                b                           = combat->signals.bytes.endDelayFrames;
                d3->suppressDisconnectPause = 0;
                if (b != 0) {
                    if (combat->actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
                        combat->signals.bytes.endDelayFrames = b - 1;
                    }
                    n = combat->signals.bytes.endDelayFrames;
                    if (n == 2) {
                        playerStateSetStatusEffects(1, PLAYER_STATUS_ALL_EFFECTS);
                        cdCmdEnqueueDisplayResource(0, 0, CD_COMMAND_DISPLAY_LOAD_SEEK_CURRENT_VIEW);
                        if (attachment->mode >= ATTACHMENT_MODE_ARMED) {
                            attachment->effectPhase = n;
                        }
                        attachment->queuedIndex  = 0;
                        attachment->mode         = ATTACHMENT_MODE_IDLE;
                        D_80115768               = 0;
                        combat->actorControl     = SCENE_COMBAT_ACTORS_RUNNING;
                        attachment->previewSound = 0;
                        attachment->soundStep    = ATTACHMENT_SOUND_IDLE;
                        roomEffectRequestCancelPe();
                        worldTargetClearActorTargetMarks();
                        if ((gGameSession->flowFlags & GAME_SESSION_FLOW_REEQUIP_WEAPON) == 0) {
                            hud->battleStep = hud->battleStep + 1;
                        } else {
                            work = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
                            playerActorResetWeaponAttack(work, gPlayerStatus.weapon, 0);
                            if (gGameSession->flowFlags & GAME_SESSION_FLOW_HIDE_REEQUIPPED_WEAPON) {
                                playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
                            }
                            hud->battleStep = hud->battleStep + 2;
                        }
                    }
                } else {
                    GameSession* session;

                    if (stageAreaKey == GAME_LOCATION_KEY(1, 20, 0, 0)) {
                        session = gGameSession;
                        if (session->battleResetPending != 0) {
                            combat->signals.bytes.battlePhase = SCENE_COMBAT_BATTLE_IDLE;
                            combat->battleRefs                = 0;
                            session->battleResetPending       = 0;
                            if (attachment->mode >= ATTACHMENT_MODE_ARMED) {
                                attachment->effectPhase = ATTACHMENT_EFFECT_CANCELLED;
                            }
                            attachment->queuedIndex = 0;
                            attachment->mode        = ATTACHMENT_MODE_IDLE;
                            D_80115768              = 0;
                            combat->actorControl    = SCENE_COMBAT_ACTORS_RUNNING;
                            attachment->menuOpen    = ATTACHMENT_MENU_CLOSED;
                            hud->battleStep         = HUD_BATTLE_STEP_START;
                            hud->inBattle           = 0;
                        }
                    }
                }
            } else if (step == HUD_BATTLE_STEP_END_ACTION) {
                SceneCombatState* combat;
                Task*             w;
                s32               c;

                combat = &gSceneCombatState;
                c      = combat->signals.bytes.endDelayFrames;
                if (c != 0) {
                    if (combat->actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
                        combat->signals.bytes.endDelayFrames = c - 1;
                    }
                }
                w = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
                if (w != NULL) {
                    func_801088D4(w, 0, 2);
                }
                hud->battleStep = hud->battleStep + 1;
            } else if (step == HUD_BATTLE_STEP_WAIT_END_ACTION) {
                hudWaitEndAction(hud);
            } else if (step == HUD_BATTLE_STEP_RESULTS && bad == 0) {
                PlayerStatus*    p;
                s32              cond;
                DisplayState*    d4;
                AttachmentState* attachment;

                p = &gPlayerStatus;
                if ((GAME_LOCATION_WORD(gGameSession->location.loc) & GAME_LOCATION_STAGE_AREA_MASK) != GAME_LOCATION_KEY(1, 20, 0, 0)) {
                    cond = 0;
                } else {
                    cond = p->resourceVariant == 4;
                }
                if (cond != 0 && gGameSession->battleResetPending != 0) {
                    hud->battleStep = HUD_BATTLE_STEP_START;
                    hud->inBattle   = 0;
                } else {
                    d4 = &gDisplayState;
                    if (d4->demoScene != DISPLAY_DEMO_NONE) {
                        d4->gameMode    = DISPLAY_GAME_RESTART;
                        hud->battleStep = HUD_BATTLE_STEP_START;
                        hud->inBattle   = 0;
                    } else {
                        displayQueueModeTask(&D_8010CABC, 0, hud, STAGE_ENTRY_RELOAD);
                    }
                }
                attachment = &Gp_StateC08;
                if (attachment->mode >= ATTACHMENT_MODE_ARMED) {
                    attachment->effectPhase = ATTACHMENT_EFFECT_CANCELLED;
                }
                attachment->queuedIndex        = 0;
                attachment->mode               = ATTACHMENT_MODE_IDLE;
                D_80115768                     = 0;
                gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_RUNNING;
                attachment->menuOpen           = ATTACHMENT_MENU_CLOSED;
            }
            if (hud->suppression <= HUD_SUPPRESS_PARASITE_ENERGY) {
                if (gGameSession->hideHud == 0) {
                    func_800A57B0(hud);
                    if (equipmentHasEffect(EQUIPMENT_EFFECT_MOTION_DETECTOR) != 0) {
                        if (gGameSession->sceneUpdatesPaused == 0) {
                            Gp_DrawHudSprites(hud);
                        }
                    }
                }
            }
        } else {
            SceneCombatState* combat;
            AttachmentState*  attachment;
            s32               m;

            if (gSceneCombatState.signals.bytes.endDelayFrames != 0) {
                if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
                    gSceneCombatState.signals.bytes.endDelayFrames = gSceneCombatState.signals.bytes.endDelayFrames - 1;
                }
            }
            combat = &gSceneCombatState;
            m      = gSceneCombatState.signals.bytes.battlePhase;
            if (m == 1) {
                hud->battleStep = HUD_BATTLE_STEP_START;
                hud->inBattle   = m;
                cdCmdEnqueueDisplayResource(0, 0, CD_COMMAND_DISPLAY_LOAD_SEEK_CURRENT_VIEW);
                attachment = &Gp_StateC08;
                if (attachment->mode >= ATTACHMENT_MODE_ARMED) {
                    attachment->effectPhase = ATTACHMENT_EFFECT_CANCELLED;
                }
                attachment->queuedIndex  = 0;
                attachment->mode         = ATTACHMENT_MODE_IDLE;
                D_80115768               = 0;
                combat->actorControl     = SCENE_COMBAT_ACTORS_RUNNING;
                attachment->previewSound = 0;
                attachment->soundStep    = ATTACHMENT_SOUND_IDLE;
                hud->suppression         = HUD_SUPPRESS_ALL;
            } else if (hud->suppression <= HUD_SUPPRESS_PARASITE_ENERGY && gGameSession->hideHud == 0) {
                func_800A57B0(hud);
            } else {
                hud->suppression = HUD_SUPPRESS_ALL;
            }
        }
    }

    if (hud->suppression <= HUD_SUPPRESS_NONE) {
        if (gGameSession->eventState == 0) {
            if (equipmentHasEffect(EQUIPMENT_EFFECT_MEDICAL_INSPECTION) != 0) {
                GameSession* session;

                session = gGameSession;
                if (session->hideHud == 0) {
                    if (session->sceneUpdatesPaused == 0) {
                        hudDrawLockedTargetHp(&hud->targetHpReadout);
                    }
                }
            }
            Gp_UseItemTask(hud);
            Gp_StateC08.flags &= ATTACHMENT_FLAG_CLEAR_EVENT_LOCK;
            if (Gp_ItemGrantCooldown > 0) {
                Gp_ItemGrantCooldown = Gp_ItemGrantCooldown - 1;
            }
        }
    }
    if (gDisplayState.pendingMode == DISPLAY_MODE_MAP) {
        roomEffectRequestCancelPe();
    }
}

const char D_8009388C[] = "R1";
const char D_80093890[] = "R2";
const char D_80093894[] = "%";
const char D_80093898[] = "&";
