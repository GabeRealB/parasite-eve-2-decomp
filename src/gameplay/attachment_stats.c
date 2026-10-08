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

static void _hudDrawWeaponSupplyPrompt(s32 unusedPanelX, s32 unusedPanelY);

static __inline__ s32 _attachmentGetEffectiveLevel(s32 abilityIndex);

static __inline__ s32 _sceneIsBattleActive(void);

static __inline__ u16 _attachmentGetLevelValue(s32 abilityIndex, s32 level, s32 column);

static s32 _attachmentIsCastBlocked(s32 abilityIndex);

static void _attachmentArmCast(s32 abilityIndex);

static __inline__ s32 _attachmentStepLearnedSpellWithSave(s32 abilityIndex, s32 steps, const McSaveData* eligibilitySave);

static __inline__ s32 _attachmentStepLearnedSpell(s32 abilityIndex, s32 steps);

static __inline__ u16 _attachmentGetEffectiveLevelValue(s32 abilityIndex, s32 column);

static s32 _attachmentUpdateAndDrawWheel(HudState* hud, s32 panelX, s32 panelY);

static void _hudDrawAttachmentCastGauge(HudState* unusedHud, s32 panelX, s32 panelY);

static __inline__ const u8* _attachmentGetLearnedLevels(void);

static __inline__ s32 _hudCanSwitchCategory(s32 ignoreSwapLock);

static __inline__ s32 _attachmentIsBattleSoundLoadReady(void);

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
                attachmentPreviewProjectile(arg0, radiusWorld, extentWorld);
                if (hud != NULL) {
                    hud->radarRangeIcon = HUD_RADAR_RANGE_PROJECTILE;
                    hud->radarRange     = extentWorld;
                }
                break;
            case ATTACHMENT_AREA_ELLIPSOID:
                attachmentTargetEllipsoid(arg0, radiusWorld, extentWorld, area->ahead);
                if (hud != NULL) {
                    ahead               = area->ahead;
                    hud->radarRange     = radiusWorld;
                    hud->radarRangeIcon = ahead + HUD_RADAR_RANGE_AROUND;
                }
                break;
            case ATTACHMENT_AREA_CYLINDER:
                attachmentTargetCylinder(arg0, radiusWorld, extentWorld, area->ahead);
                if (hud != NULL) {
                    ahead               = area->ahead;
                    hud->radarRange     = radiusWorld;
                    hud->radarRangeIcon = ahead + HUD_RADAR_RANGE_AROUND;
                }
                break;
            case ATTACHMENT_AREA_ALL:
                attachmentTargetAll(arg0);
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
#define DRAW_PROMPT_LABEL(req, dx, line, color, str)                                         \
    {                                                                                        \
        req.x          = promptPanel.panel.contentOriginX.unsignedValue + (dx) + promptLeft; \
        req.y          = (promptPanel.panel.contentOriginY.unsignedValue + 9) + (line);      \
        req.otIndex    = promptPanel.panel.otIndex.signedValue + 1;                          \
        req.colorRgb   = (color);                                                            \
        req.glyphTable = TEXT_GLYPH_TABLE_SMALL;                                             \
        req.alignment  = TEXT_ALIGNMENT_LEFT;                                                \
        req.drawMode   = TEXT_DRAW_OUTLINED;                                                 \
        textDrawString(&req, (str));                                                         \
    }

/// Draws a quantity right-aligned on line `line`; an empty count sets `missingSupply`.
#define DRAW_PROMPT_COUNT(req, line, count)                                             \
    {                                                                                   \
        req.colorRgb   = 0x606060;                                                      \
        req.glyphTable = TEXT_GLYPH_TABLE_SMALL;                                        \
        req.alignment  = TEXT_ALIGNMENT_RIGHT;                                          \
        req.drawMode   = TEXT_DRAW_FILL_ONLY;                                           \
        req.x          = promptPanel.panel.contentOriginX.unsignedValue + 0x94;         \
        req.y          = (promptPanel.panel.contentOriginY.unsignedValue + 9) + (line); \
        req.otIndex    = promptPanel.panel.otIndex.signedValue + 1;                     \
        textDrawString(&req, textItoaSigned(quantityDigits, (count)));                  \
        if ((count) == 0) {                                                             \
            missingSupply = 1;                                                          \
        }                                                                               \
    }

/// Draws the equipped weapon's button labels and loaded supply quantities.
///
/// Uses a fixed screen-centered position, adjusted for the display's VRAM Y
/// offset; both incoming panel coordinates are ignored. Hidden HUD/captions,
/// no weapon and the tonfa suppress it. Layout C uses button glyphs; the
/// Gunblade's primary supply uses the secondary button label. The last visible
/// row selects a pulsing frame when empty. Requires live save and UI resources
/// and writable GPU packet/ordering-table storage through frame completion.
static void _hudDrawWeaponSupplyPrompt(s32 unusedPanelX, s32 unusedPanelY)
{
    enum {
        HUD_WEAPON_PROMPT_TONFA_ITEM_ID          = 0x92,
        HUD_WEAPON_PROMPT_GUNBLADE_ITEM_ID       = 0x96,
        HUD_WEAPON_PROMPT_SYMBOL_LAYOUT          = 2,
        HUD_WEAPON_PROMPT_TEXT_COLOR             = 0x606060,
        HUD_WEAPON_PROMPT_PRIMARY_SYMBOL_COLOR   = 0x503060,
        HUD_WEAPON_PROMPT_SECONDARY_SYMBOL_COLOR = 0x506030,
        HUD_WEAPON_PROMPT_NO_SECONDARY_QUANTITY  = -1,
        HUD_WEAPON_PROMPT_STEADY_STYLE           = 0x40002,
        HUD_WEAPON_PROMPT_EMPTY_STYLE            = 0x40004
    };
    u8                         quantityDigits[0x10];
    UiObject                   promptPanel;
    TextDrawReq                primaryLabel;
    TextDrawReq                secondaryLabel;
    RECT                       promptFrame;
    const PlayerStatus*        player;
    const EquipmentWeaponLoad* weaponLoad;
    s32                        weaponItemId;
    s32                        secondaryQuantity;
    s32                        primaryQuantity;
    s32                        promptHeight;
    s32                        missingSupply;
    s32                        promptLeft;
    s32                        lineY;

    player = &gPlayerStatus;
    // The load is read only after the equipped-weapon guard below.
    weaponLoad        = equipmentGetWeaponLoad(player->weapon + (EQUIPMENT_WEAPON_ITEM_FIRST - 1));
    secondaryQuantity = HUD_WEAPON_PROMPT_NO_SECONDARY_QUANTITY;
    if (Pad_RemapState->hideHud != 0) {
        return;
    }
    if (capIsBusy() != 0) {
        return;
    }
    if (gGameSession->hideHud != 0) {
        return;
    }
    if (player->weapon == PLAYER_STATUS_EQUIPMENT_NONE) {
        return;
    }
    weaponItemId = player->weapon + (EQUIPMENT_WEAPON_ITEM_FIRST - 1);
    if (weaponItemId == HUD_WEAPON_PROMPT_TONFA_ITEM_ID) {
        return;
    }
    primaryQuantity = weaponLoad->primaryQty;
    if (weaponLoad->secondaryItemId != INVENTORY_ITEM_NONE && weaponLoad->secondaryItemId != EQUIPMENT_WEAPON_SECONDARY_UNAVAILABLE) {
        secondaryQuantity = weaponLoad->secondaryQty;
    }
    promptHeight                                   = 0xE;
    missingSupply                                  = 0;
    promptPanel.panel.contentOriginX.unsignedValue = 0;
    promptPanel.panel.contentOriginY.unsignedValue = 0;
    promptPanel.panel.otIndex.signedValue          = -3;
    promptPanel.panel.state                        = USER_INTERFACE_PANEL_INITIAL;
    promptLeft                                     = 0x5F;
    if (weaponLoad->secondaryItemId != EQUIPMENT_WEAPON_SECONDARY_UNAVAILABLE) {
        promptHeight = 0x18;
    }
    lineY = 0x64 - promptHeight;
    lineY = lineY - gDisplayState.vramYOffset;
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.buttonLayout != HUD_WEAPON_PROMPT_SYMBOL_LAYOUT) {
        if (weaponItemId != HUD_WEAPON_PROMPT_GUNBLADE_ITEM_ID) {
            DRAW_PROMPT_LABEL(primaryLabel, 4, lineY, HUD_WEAPON_PROMPT_TEXT_COLOR, D_8009388C);
        } else {
            DRAW_PROMPT_LABEL(primaryLabel, 4, lineY, HUD_WEAPON_PROMPT_TEXT_COLOR, D_80093890);
        }
    } else {
        if (weaponItemId != HUD_WEAPON_PROMPT_GUNBLADE_ITEM_ID) {
            DRAW_PROMPT_LABEL(primaryLabel, 6, lineY, HUD_WEAPON_PROMPT_PRIMARY_SYMBOL_COLOR, D_80093894);
        } else {
            DRAW_PROMPT_LABEL(primaryLabel, 6, lineY, HUD_WEAPON_PROMPT_SECONDARY_SYMBOL_COLOR, D_80093898);
        }
    }
    if (weaponLoad->primaryItemId != INVENTORY_ITEM_NONE) {
        DRAW_PROMPT_COUNT(primaryLabel, lineY, primaryQuantity);
    } else {
        missingSupply = 1;
    }
    uiDrawRecessedRect(&promptPanel.panel, 0x79, (lineY + 4), 0x1B, 7, 0x102010);
    // The frame follows the last displayed supply row, including its empty state.
    if (weaponLoad->secondaryItemId != EQUIPMENT_WEAPON_SECONDARY_UNAVAILABLE) {
        missingSupply = 0;
        lineY        += 0xA;
        if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.buttonLayout != HUD_WEAPON_PROMPT_SYMBOL_LAYOUT) {
            DRAW_PROMPT_LABEL(secondaryLabel, 4, lineY, HUD_WEAPON_PROMPT_TEXT_COLOR, D_80093890);
        } else {
            DRAW_PROMPT_LABEL(secondaryLabel, 6, lineY, HUD_WEAPON_PROMPT_SECONDARY_SYMBOL_COLOR, D_80093898);
        }
        if (weaponLoad->secondaryItemId != INVENTORY_ITEM_NONE) {
            DRAW_PROMPT_COUNT(secondaryLabel, lineY, secondaryQuantity);
        } else {
            missingSupply = 1;
        }
        uiDrawRecessedRect(&promptPanel.panel, 0x79, (lineY + 4), 0x1B, 7, 0x102010);
        lineY -= 0xA;
    }
    promptFrame.w = 0x39;
    promptFrame.x = promptLeft;
    promptFrame.y = lineY;
    promptFrame.h = promptHeight;
    if (missingSupply == 1) {
        uiDrawRectFrame(&promptFrame, -1, HUD_WEAPON_PROMPT_EMPTY_STYLE, NULL);
    } else {
        uiDrawRectFrame(&promptFrame, -1, HUD_WEAPON_PROMPT_STEADY_STYLE, NULL);
    }
}

#undef DRAW_PROMPT_LABEL
#undef DRAW_PROMPT_COUNT

/// Tests whether the live shooting-gallery session uses training spell levels.
///
/// Borrows the player record; room and view do not affect this stage/area test.
static __inline__ s32 _attachmentUsesTrainingLevels(const PlayerStatus* player)
{
    enum { ATTACHMENT_TRAINING_RESOURCE_VARIANT = 4 };
    s32 usesTrainingLevels;

    if ((GAME_LOCATION_WORD(gGameSession->location.loc) & GAME_LOCATION_STAGE_AREA_MASK) != GAME_LOCATION_KEY(GAME_STAGE_ACROPOLIS, GAME_AREA_MIST_SHOOTING_GALLERY, 0, 0)) {
        usesTrainingLevels = 0;
    } else {
        usesTrainingLevels = player->resourceVariant == ATTACHMENT_TRAINING_RESOURCE_VARIANT;
    }
    return usesTrainingLevels;
}

/// Returns a spell's effective cast level, or level one for an item attachment.
///
/// `abilityIndex` must be nonnegative: spells 0..11 use the live save's learned
/// levels, or training levels in the shooting gallery; item slots 12..17 return
/// one. A learned zero becomes one without unlocking the spell. Berserker adds
/// one only below level three. Stored levels above three are retained; callers
/// indexing level tables require learned values 0..3. No state is changed.
static __inline__ s32 _attachmentGetEffectiveLevel(s32 abilityIndex)
{
    enum { ATTACHMENT_MIN_EFFECTIVE_LEVEL = 1 };
    const PlayerStatus* player;
    const u8*           learnedLevels;
    s32                 usesTrainingLevels;
    s32                 level;

    if (abilityIndex >= ATTACHMENT_SPELL_COUNT) {
        level = ATTACHMENT_MIN_EFFECTIVE_LEVEL;
    } else {
        player             = &gPlayerStatus;
        usesTrainingLevels = _attachmentUsesTrainingLevels(player);
        if (usesTrainingLevels == 0) {
            learnedLevels = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.attachLevels;
        } else {
            learnedLevels = Gp_DebugAttachLevels;
        }
        level = learnedLevels[abilityIndex];
        if (level == 0) {
            level = ATTACHMENT_MIN_EFFECTIVE_LEVEL;
        }
        if ((player->statusFlags & PLAYER_STATUS_BERSERKER) && level < ATTACHMENT_AREA_LEVEL_COUNT) {
            level++;
        }
    }
    return level;
}

/// Returns one while an engaged battle has holds or its end delay is nonzero.
///
/// The end delay keeps the gate active regardless of battle phase. No state is
/// changed.
static __inline__ s32 _sceneIsBattleActive(void)
{
    const SceneCombatState* combat;

    combat = &gSceneCombatState;
    if ((combat->signals.bytes.battlePhase == SCENE_COMBAT_BATTLE_ENGAGED && combat->battleRefs != 0) || combat->signals.bytes.endDelayFrames != 0) {
        return 1;
    }
    return 0;
}

/// Reads one unsigned halfword parameter for an explicit ability and cast level.
///
/// `abilityIndex` is 0..17, `level` is 1..3, and `column` selects a stored
/// `ATTACHMENT_LEVEL_*` column in 0..7. The inputs are not checked or masked;
/// level zero reads the preceding row directly. No table pointer is retained.
static __inline__ u16 _attachmentGetLevelValue(s32 abilityIndex, s32 level, s32 column)
{
    return Gp_IdParamHi.rows[abilityIndex * ATTACHMENT_AREA_LEVEL_COUNT + level].value[column];
}

/// Returns one when the selected spell or item attachment cannot be cast.
///
/// `abilityIndex` must be 0..17 and learned levels must be 0..3. Outside battle,
/// only Healing with missing HP and sufficient MP is allowed. In battle, item
/// slots bypass spell checks; spells obey silence, resources, wards and Energy
/// Ball capacity. Berserker permits only the first six spells and requires HP
/// strictly greater than twice the cast cost. Cheats bypass only the ordinary
/// in-battle MP and full-HP Healing checks. This does not spend resources or
/// test whether the spell is learned; callers handle selection and release.
static s32 _attachmentIsCastBlocked(s32 abilityIndex)
{
    enum {
        ATTACHMENT_ENERGY_BALL_LIMIT            = 3,
        ATTACHMENT_BERSERKER_HP_COST_MULTIPLIER = 2
    };
    const PlayerStatus* player;
    s32                 blocked;
    s32                 level;

    player  = &gPlayerStatus;
    blocked = 0;
    level   = _attachmentGetEffectiveLevel(abilityIndex);

    if (!_sceneIsBattleActive()) {
        if (player->mp < _attachmentGetLevelValue(abilityIndex, level, ATTACHMENT_LEVEL_CAST_COST) || abilityIndex != ATTACHMENT_INDEX_HEALING || player->hpMax == player->hp) {
            blocked = 1;
        }
    } else if (abilityIndex < ATTACHMENT_SPELL_COUNT) {
        // Cheats leave status restrictions, ward redundancy and HP safety intact.
        if ((player->statusFlags & PLAYER_STATUS_SILENCE) ||
            (!(player->statusFlags & PLAYER_STATUS_BERSERKER) && player->mp < _attachmentGetLevelValue(abilityIndex, level, ATTACHMENT_LEVEL_CAST_COST) && gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.cheatMode == 0) ||
            (abilityIndex == ATTACHMENT_INDEX_METABOLISM && Gp_StateC08.mindWard != 0 && Gp_StateC08.bodyWard != 0) ||
            (abilityIndex == ATTACHMENT_INDEX_HEALING && player->hpMax == player->hp && gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.cheatMode == 0) ||
            (abilityIndex == ATTACHMENT_INDEX_ENERGY_BALL && gEnergyBallInFlightCount >= ATTACHMENT_ENERGY_BALL_LIMIT) ||
            ((player->statusFlags & PLAYER_STATUS_BERSERKER) && (abilityIndex >= ATTACHMENT_INDEX_METABOLISM || _attachmentGetLevelValue(abilityIndex, level, ATTACHMENT_LEVEL_CAST_COST) * ATTACHMENT_BERSERKER_HP_COST_MULTIPLIER >= player->hp))) {
            blocked = 1;
        }
    }
    return blocked;
}

/// Arms a selected Parasite Energy spell or item attachment and queues its sound.
///
/// `abilityIndex` must be 0..17 and spell levels 0..3. Clears the queued index
/// even when event-locked; otherwise publishes the held phase, effective packed
/// id and a minimum one-frame cast duration. The duration parameter first
/// narrows to signed eight bits. Closes the wheel and resumes actor updates;
/// no HP/MP is spent and the effect is not released here.
static void _attachmentArmCast(s32 abilityIndex)
{
    AttachmentState* attachment;
    s32              level;
    s32              encodedIndex;
    s32              attachId;
    s32              familyBase;
    s8               elementRow;
    s8               energyColumn;
    s8               durationFrames;

    Gp_StateC08.queuedIndex = 0;
    if (Gp_StateC08.flags & ATTACHMENT_FLAG_EVENT_LOCK) {
        return;
    }
    // Encode the element, energy and effective level as decimal family/level digits.
    encodedIndex            = (s8)abilityIndex;
    Gp_StateC08.activeIndex = abilityIndex;
    elementRow              = encodedIndex / 3;
    familyBase              = (elementRow + 1) * 10 + 1;
    energyColumn            = encodedIndex % 3;
    attachId                = familyBase + energyColumn;
    attachId               *= 10;
    level                   = _attachmentGetEffectiveLevel(encodedIndex);
    attachId               += level;

    attachment              = &Gp_StateC08;
    attachment->attachId    = attachId;
    attachment->effectPhase = ATTACHMENT_EFFECT_HELD;
    durationFrames          = attachmentGetActiveLevelValue(ATTACHMENT_LEVEL_ATP_LOSS);
    attachment->duration    = durationFrames;
    if (durationFrames <= 0) {
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

/// Traverses eligible wheel spells using a borrowed save's cheat setting.
///
/// `abilityIndex` is 0..11; signed `steps` counts eligible positions and wraps
/// within the twelve spells. Zero retains the index. Levels always come from
/// the live save or shooting-gallery training table; `eligibilitySave` affects
/// only whether cheats admit unlearned spells. Nonzero steps require a learned
/// spell or cheats, or the search never ends. No input or selection is changed.
static __inline__ s32 _attachmentStepLearnedSpellWithSave(s32 abilityIndex, s32 steps, const McSaveData* eligibilitySave)
{
    const PlayerStatus* player;
    s32                 usesTrainingLevels;
    const u8*           learnedLevels;

    player             = &gPlayerStatus;
    usesTrainingLevels = _attachmentUsesTrainingLevels(player);
    if (usesTrainingLevels == 0) {
        learnedLevels = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.attachLevels;
    } else {
        learnedLevels = Gp_DebugAttachLevels;
    }
    if (steps != 0) {
        do {
            if (steps > 0) {
                do {
                    abilityIndex++;
                    if (abilityIndex >= ATTACHMENT_SPELL_COUNT) {
                        abilityIndex = 0;
                    }
                } while (learnedLevels[abilityIndex] == 0 && eligibilitySave->state.cheatMode == 0);
                steps--;
            } else {
                do {
                    abilityIndex--;
                    if (abilityIndex < 0) {
                        abilityIndex += ATTACHMENT_SPELL_COUNT;
                    }
                } while (learnedLevels[abilityIndex] == 0 && eligibilitySave->state.cheatMode == 0);
                steps++;
            }
        } while (steps != 0);
    }
    return abilityIndex;
}

/// Moves through learned wheel spells by a signed count of eligible positions.
///
/// `abilityIndex` is 0..11. Positive `steps` moves forward, negative backward,
/// wrapping within the twelve spells; zero retains the index. Uses the live
/// save or shooting-gallery training levels. Cheats make every spell eligible.
/// Nonzero steps require a learned spell or cheats, or the search never ends.
/// Returns the new index without changing selection or either level table.
static __inline__ s32 _attachmentStepLearnedSpell(s32 abilityIndex, s32 steps)
{
    const PlayerStatus* player;
    const McSaveData*   liveSave;
    s32                 usesTrainingLevels;
    const u8*           learnedLevels;

    player             = &gPlayerStatus;
    usesTrainingLevels = _attachmentUsesTrainingLevels(player);
    if (usesTrainingLevels == 0) {
        learnedLevels = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.attachLevels;
    } else {
        learnedLevels = Gp_DebugAttachLevels;
    }
    if (steps != 0) {
        liveSave = &gMcSaveData[MEMORY_CARD_SAVE_LIVE];
        // Each requested step searches past unlearned slots before consuming it.
        do {
            if (steps > 0) {
                do {
                    abilityIndex++;
                    if (abilityIndex >= ATTACHMENT_SPELL_COUNT) {
                        abilityIndex = 0;
                    }
                } while (learnedLevels[abilityIndex] == 0 && liveSave->state.cheatMode == 0);
                steps--;
            } else {
                do {
                    abilityIndex--;
                    if (abilityIndex < 0) {
                        abilityIndex += ATTACHMENT_SPELL_COUNT;
                    }
                } while (learnedLevels[abilityIndex] == 0 && liveSave->state.cheatMode == 0);
                steps++;
            }
        } while (steps != 0);
    }
    return abilityIndex;
}

/// Reads one level parameter of a spell or item attachment at its effective level.
///
/// `abilityIndex` must be 0..17, learned levels 0..3, and `column` an
/// `ATTACHMENT_LEVEL_*` index in 0..7. Uses the effective-level rules without
/// checking cast eligibility or changing state. Returns an unsigned halfword.
static __inline__ u16 _attachmentGetEffectiveLevelValue(s32 abilityIndex, s32 column)
{
    s32 level;

    level = _attachmentGetEffectiveLevel(abilityIndex);
    return _attachmentGetLevelValue(abilityIndex, level, column);
}

/// Steps the Parasite Energy selection wheel and draws its caption and icons.
///
/// Returns one when left/right input changes the selection, otherwise zero.
/// `panelX`/`panelY` are screen-centered pixels; `hud` is borrowed writable state.
/// The cursor must be 0..11 with learned levels 0..3, and navigation requires
/// at least one learned spell or cheats. A turn lasts four quarter-slot ticks.
/// Queued casts omit the drawing and cost preview but retain input/turn updates.
/// Uses live save/training levels and UI textures; generated GPU packets remain
/// live through frame completion. Berserker doubles the displayed HP cost.
static s32 _attachmentUpdateAndDrawWheel(HudState* hud, s32 panelX, s32 panelY)
{
    enum {
        ATTACHMENT_WHEEL_QUARTERS_PER_SLOT      = 4,
        ATTACHMENT_WHEEL_ANGLE_FRACTION_BITS    = 12,
        ATTACHMENT_WHEEL_HORIZONTAL_PIXEL_SHIFT = 7,
        ATTACHMENT_WHEEL_VERTICAL_PIXEL_SHIFT   = 10,
        ATTACHMENT_WHEEL_COMPACT_COUNT_LIMIT    = 6,
        ATTACHMENT_WHEEL_TEXT_COLOR             = 0x606060,
        ATTACHMENT_WHEEL_ICON_LEVEL             = 1,
        ATTACHMENT_WHEEL_TPAGE                  = 0x3E,
        ATTACHMENT_WHEEL_CAPTION_STYLE          = 0x40002
    };
    UiObject                wheelPanel;
    _AttachmentWheelScratch scratch;
    RECT                    captionFrame;
    s32                     selectionChanged;
    s32                     otIndex;
    const PlayerStatus*     player;
    const u8*               learnedLevels;
    s32                     usesTrainingLevels;
    s32                     learnedSpellCount;
    s32                     iconOriginX;
    s32                     iconOriginY;
    s32                     captionItemId;
    s32                     displayCastCost;
    s32                     effectiveLevel;
    s32                     captionColor;
    _AttachmentWheelPoint*  placementPoints;
    _AttachmentWheelPoint*  placementPoint;
    _AttachmentWheelPoint*  drawPoints;
    _AttachmentWheelPoint*  drawnPoint;
    const McSaveData*       liveSave;
    s32                     iconAngle;
    s32                     nearestPointIndex;
    s32                     iconFlags;
    s32                     screenX;
    s32                     screenY;
    s32                     abilityIndex;
    s32                     pointIndex;
    s32                     candidateIndex;
    s32                     levelBytesRemaining;
    DR_TPAGE*               drawPage;

    selectionChanged     = 0;
    otIndex              = -2;
    gGameSession->uiOpen = 1;
    player               = &gPlayerStatus;
    learnedSpellCount    = 0;
    usesTrainingLevels   = _attachmentUsesTrainingLevels(player);
    if (usesTrainingLevels == 0) {
        learnedLevels = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.attachLevels;
    } else {
        learnedLevels = Gp_DebugAttachLevels;
    }
    for (levelBytesRemaining = ATTACHMENT_SPELL_COUNT - 1; levelBytesRemaining >= 0; levelBytesRemaining--, learnedLevels++) {
        if (*learnedLevels != 0) {
            learnedSpellCount++;
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
                Gp_StateC08.wheelIndex = _attachmentStepLearnedSpell(Gp_StateC08.wheelIndex, 1);
                selectionChanged       = 1;
                hud->wheelTurn        += ATTACHMENT_WHEEL_QUARTERS_PER_SLOT;
            } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_LEFT) != 0) {
                Gp_StateC08.wheelIndex = _attachmentStepLearnedSpell(Gp_StateC08.wheelIndex, -1);
                selectionChanged       = 1;
                hud->wheelTurn        -= ATTACHMENT_WHEEL_QUARTERS_PER_SLOT;
            }
        }
    }

    if (Gp_StateC08.queuedIndex == 0) {
        iconOriginX          = panelX + 2;
        iconOriginY          = panelY + 2;
        hud->previewCastCost = _attachmentGetEffectiveLevelValue(Gp_StateC08.wheelIndex, ATTACHMENT_LEVEL_CAST_COST);

        captionItemId   = ((Gp_StateC08.wheelIndex / 3) << 4) + ((Gp_StateC08.wheelIndex % 3) << 2) + ITEM_TEXT_PACKED_ID_FIRST;
        displayCastCost = _attachmentGetEffectiveLevelValue(Gp_StateC08.wheelIndex, ATTACHMENT_LEVEL_CAST_COST);
        if (player->statusFlags & PLAYER_STATUS_BERSERKER) {
            displayCastCost <<= 1;
        }

        wheelPanel.panel.otIndex.signedValue          = -3;
        scratch.text.nameRequest.x                    = panelX + 7;
        scratch.text.nameRequest.y                    = panelY + 0x22;
        scratch.text.nameRequest.otIndex              = -2;
        wheelPanel.panel.contentOriginX.unsignedValue = panelX;
        wheelPanel.panel.contentOriginY.unsignedValue = panelY;
        wheelPanel.panel.state                        = USER_INTERFACE_PANEL_INITIAL;
        scratch.text.nameRequest.colorRgb             = ATTACHMENT_WHEEL_TEXT_COLOR;
        scratch.text.nameRequest.glyphTable           = TEXT_GLYPH_TABLE_MEDIUM;
        scratch.text.nameRequest.alignment            = TEXT_ALIGNMENT_LEFT;
        scratch.text.nameRequest.drawMode             = TEXT_DRAW_OUTLINED;
        textDrawString(&scratch.text.nameRequest, itemGetText(captionItemId, ITEM_TEXT_NAME, 0));

        effectiveLevel = _attachmentGetEffectiveLevel(Gp_StateC08.wheelIndex);
        captionColor   = ATTACHMENT_WHEEL_TEXT_COLOR;
        itemMenuDrawParasiteEnergyLevel(&wheelPanel, -0xB, 0x28, effectiveLevel, captionColor);
        textDrawUiLine(&wheelPanel, 0x8E, 0x28, textItoaSigned(scratch.text.costDigits, displayCastCost), captionColor, TEXT_DRAW_TRANSLUCENT_OUTLINED, TEXT_ALIGNMENT_RIGHT);

        captionFrame.x = panelX;
        captionFrame.y = panelY + 0x17;
        captionFrame.w = 0x91;
        captionFrame.h = 0x13;
        uiDrawRectFrame(&captionFrame, -1, ATTACHMENT_WHEEL_CAPTION_STYLE, NULL);

        // Reuse the completed caption scratch for evenly spaced 4.12 circle points.
        // The remaining quarter-slot turn offsets every icon; unused points retire.
        wheelPanel.panel.contentOriginX.unsignedValue = 0x30;
        wheelPanel.panel.contentOriginY.unsignedValue = 0;
        wheelPanel.panel.otIndex.signedValue          = -3;
        wheelPanel.panel.state                        = USER_INTERFACE_PANEL_INITIAL;
        placementPoints                               = scratch.points;
        /// Places twelve points using a learned count and signed quarter-slot turn.
        ///
        /// Inputs must have no side effects; points borrows twelve writable entries.
        /// index, angle and point are distinct scratch lvalues, changed by the loop.
        /// Unused entries retire; zero count performs no division.
        /// Uses the enclosing wheel's quarter-slot and angle-scale constants;
        /// expand as a standalone statement inside a compound block.
#define ATTACHMENT_WHEEL_PLACE_POINTS(points, learnedCount, quarterTurn, index, angle, point)                                                                                               \
    {                                                                                                                                                                                       \
        for ((index) = 0; (index) < ATTACHMENT_SPELL_COUNT; (index)++) {                                                                                                                    \
            if ((index) < (learnedCount)) {                                                                                                                                                 \
                (angle) = (((index) * ATTACHMENT_WHEEL_QUARTERS_PER_SLOT + (quarterTurn)) << ATTACHMENT_WHEEL_ANGLE_FRACTION_BITS) / ((learnedCount) * ATTACHMENT_WHEEL_QUARTERS_PER_SLOT); \
                if ((learnedCount) == 1) {                                                                                                                                                  \
                    (angle) = 0;                                                                                                                                                            \
                }                                                                                                                                                                           \
                (point)    = &(points)[(index)];                                                                                                                                            \
                (point)->x = rsin((angle));                                                                                                                                                 \
                (point)->y = rcos((angle));                                                                                                                                                 \
            } else {                                                                                                                                                                        \
                (points)[(index)].x = ATTACHMENT_WHEEL_POINT_RETIRED;                                                                                                                       \
                (points)[(index)].y = ATTACHMENT_WHEEL_POINT_RETIRED;                                                                                                                       \
            }                                                                                                                                                                               \
        }                                                                                                                                                                                   \
    }
        ATTACHMENT_WHEEL_PLACE_POINTS(placementPoints, learnedSpellCount, hud->wheelTurn, pointIndex, iconAngle, placementPoint);
#undef ATTACHMENT_WHEEL_PLACE_POINTS

        // Draw nearest-first; each point indexes learned-spell traversal from the cursor.
        if (learnedSpellCount > 0) {
            pointIndex = 0;
            drawPoints = scratch.points;
            liveSave   = &gMcSaveData[MEMORY_CARD_SAVE_LIVE];
            do {
                nearestPointIndex = 0;
                iconFlags         = ITEM_MENU_ICON_DEFAULT;
                for (candidateIndex = 0; candidateIndex < learnedSpellCount; candidateIndex++) {
                    _AttachmentWheelPoint* nearestPoint = &drawPoints[nearestPointIndex];

                    if (nearestPoint->y < drawPoints[candidateIndex].y) {
                        nearestPointIndex = candidateIndex;
                    }
                }
                drawnPoint = &drawPoints[nearestPointIndex];
                screenX    = drawnPoint->x >> ATTACHMENT_WHEEL_HORIZONTAL_PIXEL_SHIFT;
                screenY    = drawnPoint->y >> ATTACHMENT_WHEEL_VERTICAL_PIXEL_SHIFT;
                if (learnedSpellCount < ATTACHMENT_WHEEL_COMPACT_COUNT_LIMIT) {
                    screenX >>= 1;
                    screenY >>= 1;
                }
                {
                    s32 wheelX = screenX + 0x30;
                    s32 wheelY = screenY + 0xF;

                    screenX = wheelX + iconOriginX;
                    screenY = wheelY + iconOriginY;
                }
                drawnPoint->y = ATTACHMENT_WHEEL_POINT_RETIRED;

                abilityIndex = _attachmentStepLearnedSpellWithSave(Gp_StateC08.wheelIndex, nearestPointIndex, liveSave);

                if (_attachmentIsCastBlocked(abilityIndex) != 0) {
                    iconFlags = ITEM_MENU_ICON_DIMMED;
                }
                if (nearestPointIndex == 0 && hud->wheelTurn == 0) {
                    iconFlags |= ITEM_MENU_ICON_HIGHLIGHTED;
                }
                itemMenuDrawItemIcon(&wheelPanel, screenX, screenY, ((abilityIndex / 3) << 4) + ((abilityIndex % 3) << 2) + (ITEM_TEXT_PACKED_ID_FIRST + ATTACHMENT_WHEEL_ICON_LEVEL), iconFlags);
                pointIndex++;
            } while (pointIndex < learnedSpellCount);
        }
    }

    drawPage       = gGpuPrimCursor;
    gGpuPrimCursor = drawPage + 1;
    setDrawTPage(drawPage, 0, 1, ATTACHMENT_WHEEL_TPAGE);
    addPrim(gGpuCurrentOt + otIndex, drawPage);
    return selectionChanged;
}

/// Draws the active attachment's remaining cast frames and ability icon.
///
/// `panelX`/`panelY` are screen-centered pixels; `unusedHud` is ignored.
/// Requires active index 0..17 and spell levels 0..3. Only spells 0..11 and
/// item slot 12 draw. Remaining positive frames give the one-pixel-high bar's
/// width; the surrounding gauge is at least eleven pixels wide. State is read
/// unchanged. Requires the UI atlas/palette and writable GPU storage whose
/// packets and ordering-table tags stay live through frame completion.
static void _hudDrawAttachmentCastGauge(HudState* unusedHud, s32 panelX, s32 panelY)
{
    enum {
        HUD_ATTACHMENT_GAUGE_MIN_WIDTH  = 11,
        HUD_ATTACHMENT_GAUGE_CLUT       = 0x3C0B,
        HUD_ATTACHMENT_GAUGE_TPAGE      = 0x1E,
        HUD_ATTACHMENT_GAUGE_ICON_LEVEL = 1
    };
    /// Initializes an unmodulated translucent 8x8 cap in the gauge atlas.
    ///
    /// cap must be a side-effect-free writable SPRT_8 pointer; coordinates and
    /// textureLeft are read once, retaining their low sixteen/eight bits.
    /// Uses the enclosing gauge's CLUT constant; expand inside a compound block.
#define HUD_ATTACHMENT_GAUGE_INIT_CAP(cap, screenX, screenY, textureLeft) \
    {                                                                     \
        (cap)->x0   = (screenX);                                          \
        (cap)->y0   = (screenY);                                          \
        (cap)->u0   = (textureLeft);                                      \
        (cap)->v0   = 0x68;                                               \
        (cap)->clut = HUD_ATTACHMENT_GAUGE_CLUT;                          \
        setSprt8((cap));                                                  \
        setSemiTrans((cap), 1);                                           \
        setShadeTex((cap), 1);                                            \
    }
    UiObject  iconPanel;
    TILE*     remainingBar;
    SPRT_8*   leftCap;
    SPRT_8*   rightCap;
    POLY_FT4* gaugeMiddle;
    DR_TPAGE* drawPage;
    s32       gaugeWidth;
    s32       abilityIndex;
    s32       otIndex;

    // One pixel per frame remains inside a frame sized to the initial cast length.
    gaugeWidth = attachmentGetActiveLevelValue(ATTACHMENT_LEVEL_ATP_LOSS);
    if (Gp_StateC08.activeIndex < ATTACHMENT_SPELL_COUNT + 1) {
        if (Gp_StateC08.duration > 0) {
            remainingBar                              = gGpuPrimCursor;
            gGpuPrimCursor                            = remainingBar + 1;
            remainingBar->x0                          = panelX + 0x18;
            remainingBar->y0                          = panelY + 0x21;
            remainingBar->w                           = Gp_StateC08.duration;
            remainingBar->h                           = 1;
            GPU_PRIMITIVE_COLOR_WORD(remainingBar, 0) = GPU_PACK_COLOR_WORD(0, 0xc0, 0xff, 0);
            setTile(remainingBar);
            addPrim(gGpuCurrentOt - 2, remainingBar);
        }

        if (gaugeWidth < HUD_ATTACHMENT_GAUGE_MIN_WIDTH) {
            gaugeWidth = HUD_ATTACHMENT_GAUGE_MIN_WIDTH;
        }

        leftCap        = gGpuPrimCursor;
        gGpuPrimCursor = leftCap + 1;
        HUD_ATTACHMENT_GAUGE_INIT_CAP(leftCap, panelX + 0x15, panelY + 0x1D, 0x98);
        addPrim(gGpuCurrentOt - 2, leftCap);

        rightCap       = gGpuPrimCursor;
        gGpuPrimCursor = rightCap + 1;
        HUD_ATTACHMENT_GAUGE_INIT_CAP(rightCap, gaugeWidth + panelX + 0x13, panelY + 0x1D, 0xA8);
        addPrim(gGpuCurrentOt - 2, rightCap);

        gaugeMiddle        = gGpuPrimCursor;
        gGpuPrimCursor     = gaugeMiddle + 1;
        gaugeMiddle->x0    = panelX + 0x1D;
        gaugeMiddle->y0    = panelY + 0x1D;
        gaugeMiddle->u0    = 0xA0;
        gaugeMiddle->u2    = 0xA0;
        gaugeMiddle->v2    = 0x70;
        gaugeMiddle->v3    = 0x70;
        gaugeMiddle->tpage = HUD_ATTACHMENT_GAUGE_TPAGE;
        gaugeMiddle->v0    = 0x68;
        gaugeMiddle->u1    = 0xA8;
        gaugeMiddle->v1    = 0x68;
        gaugeMiddle->u3    = 0xA8;
        gaugeMiddle->clut  = HUD_ATTACHMENT_GAUGE_CLUT;
        setPolyFT4(gaugeMiddle);
        setSemiTrans(gaugeMiddle, 1);
        setShadeTex(gaugeMiddle, 1);
        gaugeMiddle->x2 = gaugeMiddle->x0;
        gaugeMiddle->x1 = gaugeMiddle->x3 = (s16)(gaugeMiddle->x0 - 0xA) + gaugeWidth;
        gaugeMiddle->y1                   = gaugeMiddle->y0;
        gaugeMiddle->y2 = gaugeMiddle->y3 = gaugeMiddle->y0 + 8;
        addPrim(gGpuCurrentOt - 2, gaugeMiddle);

        abilityIndex                                 = Gp_StateC08.activeIndex;
        otIndex                                      = -3;
        iconPanel.panel.contentOriginX.unsignedValue = 0;
        iconPanel.panel.contentOriginY.unsignedValue = 0;
        iconPanel.panel.otIndex.signedValue          = otIndex;
        iconPanel.panel.state                        = USER_INTERFACE_PANEL_INITIAL;
        itemMenuDrawItemIcon(&iconPanel, panelX + 4, panelY + 0x28, ((abilityIndex / 3) << 4) + ((abilityIndex % 3) << 2) + (ITEM_TEXT_PACKED_ID_FIRST + HUD_ATTACHMENT_GAUGE_ICON_LEVEL), ITEM_MENU_ICON_DEFAULT);

        drawPage       = gGpuPrimCursor;
        gGpuPrimCursor = drawPage + 1;
        setDrawTPage(drawPage, 0, 1, HUD_ATTACHMENT_GAUGE_TPAGE);
        addPrim(gGpuCurrentOt - 2, drawPage);
    }
#undef HUD_ATTACHMENT_GAUGE_INIT_CAP
}

/// Borrows the read-only learned spell levels used by the live session.
///
/// Slots 0..11 are wheel spells. Returns the live save's table except in the
/// shooting gallery's training mode, which uses the separate training table.
/// Reads are valid while the selected save/gameplay storage remains live;
/// loading or training changes can replace its values. No pointer is retained.
static __inline__ const u8* _attachmentGetLearnedLevels(void)
{
    const PlayerStatus* player;
    s32                 usesTrainingLevels;

    player             = &gPlayerStatus;
    usesTrainingLevels = _attachmentUsesTrainingLevels(player);
    if (usesTrainingLevels == 0) {
        return gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.attachLevels;
    }
    return Gp_DebugAttachLevels;
}

/// Tests whether player action and input gates permit opening a HUD category.
///
/// An occupied player task slot must hold a live actor. Normal and aimed
/// locomotion qualify, including movement; direction actions, interaction
/// presses, the HUD input delay and battle-end delay block entry. Nonzero
/// `ignoreSwapLock` bypasses only `ATTACHMENT_FLAG_SWAP_LOCK`. Returns 0 or 1
/// without changing player, attachment or HUD state.
static __inline__ s32 _hudCanSwitchCategory(s32 ignoreSwapLock)
{
    enum {
        HUD_SWITCH_PLAYER_LOCOMOTION_STATE     = 0,
        HUD_SWITCH_PLAYER_AIM_LOCOMOTION_STATE = 2
    };
    const Task*         playerTask;
    const GameActor*    actor;
    const PlayerStatus* player;
    s32                 eligible;

    eligible   = 0;
    playerTask = gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER];
    if (playerTask != NULL) {
        actor  = playerTask->work;
        player = &gPlayerStatus;
        if (actor->mode == GAME_ACTOR_MODE_NORMAL) {
            if (actor->state == HUD_SWITCH_PLAYER_LOCOMOTION_STATE || actor->state == HUD_SWITCH_PLAYER_AIM_LOCOMOTION_STATE) {
                if (gGameSession->dirActionBusy == 0) {
                    if (player->interactionPressed == 0) {
                        eligible = 1;
                    }
                }
            }
        }
    }
    if (ignoreSwapLock == 0) {
        if (Gp_StateC08.flags & ATTACHMENT_FLAG_SWAP_LOCK) {
            eligible = 0;
        }
    }
    if (eligible != 0) {
        if (Gp_ItemGrantCooldown > 0) {
            return 0;
        }
        if (gSceneCombatState.signals.bytes.endDelayFrames == 0) {
            return 1;
        }
    }
    return 0;
}

/// Gates attachment confirmation/release on finished battle sound loading.
///
/// Battle holds or an end delay require an idle CD queue; outside battle the
/// result is one. The CD result narrows through u16 before promotion to s32.
/// Does not submit a request or change any state.
static __inline__ s32 _attachmentIsBattleSoundLoadReady(void)
{
    s32 battleActive;
    u16 ready;

    battleActive = _sceneIsBattleActive();
    if (battleActive) {
        ready = cdCmdIsIdle();
    } else {
        ready = 1;
    }
    return ready;
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
            lvl   = _attachmentGetEffectiveLevel(Gp_StateC08.activeIndex);
            sndId = Gp_StateC08.activeIndex * 3 + lvl;
            if (_sceneIsBattleActive()) {
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
            if (_sceneIsBattleActive()) {
                _hudDrawWeaponSupplyPrompt(x, y);
            }
            return;
        }
    }

    // Open the wheel from idle when the pad asks, or when a script forces it.
    if (Gp_StateC08.mode == ATTACHMENT_MODE_IDLE && Gp_StateC08.queuedIndex == 0) {
        ok = _hudCanSwitchCategory(0);
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
            if (!_sceneIsBattleActive()) {
                if (_attachmentGetLearnedLevels()[ATTACHMENT_INDEX_HEALING] != 0) {
                    Gp_StateC08.wheelIndex = ATTACHMENT_INDEX_HEALING;
                }
            }
            flag = 1;
        } else {
            if (_sceneIsBattleActive()) {
                _hudDrawWeaponSupplyPrompt(x, y);
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
        _hudDrawAttachmentCastGauge(hud, x, y);
        if (Gp_StateC08.mode == ATTACHMENT_MODE_CAST) {
            Gp_StateC08.duration--;
        }
        if (Gp_StateC08.duration <= 0) {
            if (_attachmentIsBattleSoundLoadReady()) {
                Gp_StateC08.mode               = ATTACHMENT_MODE_IDLE;
                D_80115768                     = 0;
                gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_RUNNING;
                Gp_StateC08.menuOpen           = ATTACHMENT_MENU_CLOSED;
                Gp_StateC08.effectPhase        = ATTACHMENT_EFFECT_RELEASED;
                Gp_ItemGrantCooldown           = 0x14;
                cdCmdEnqueueDisplayResource(0, 0, CD_COMMAND_DISPLAY_LOAD_SEEK_CURRENT_VIEW);
                if (cfg->statusFlags & PLAYER_STATUS_BERSERKER) {
                    cfg->hp -= attachmentGetActiveLevelValue(ATTACHMENT_LEVEL_CAST_COST) * 2;
                    if (cfg->hp <= 0) {
                        cfg->hp = 1;
                    }
                } else {
                    cfg->mp -= attachmentGetActiveLevelValue(ATTACHMENT_LEVEL_CAST_COST);
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

    if (_attachmentUpdateAndDrawWheel(hud, x, y) != 0) {
        flag = 1;
    }
    actor = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->work;
    if ((Gp_StateC08.queuedIndex != 0 && actor->mode == GAME_ACTOR_MODE_SCRIPTED) || (Gp_StateC08.flags & ATTACHMENT_FLAG_EVENT_LOCK)) {
        Gp_StateC08.queuedIndex = 0;
    }
    if ((hud->wheelTurn == 0 && padCheckButtons(0, PAD_BUTTON_QUERY_HELD_ANY, Pad_MaskConfirm) != 0) ||
        Gp_StateC08.queuedIndex != 0) {
        if (_attachmentIsBattleSoundLoadReady()) {
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
            if (_attachmentIsCastBlocked(Gp_StateC08.activeIndex) == 0) {
                _attachmentArmCast(Gp_StateC08.activeIndex);
            }
        }
    }

    if (Gp_StateC08.mode != ATTACHMENT_MODE_IDLE) {
        Gp_ApplyAttachStats(0, hud);
    }
    if (flag) {
        idx                      = _attachmentGetEffectiveLevel(Gp_StateC08.wheelIndex);
        Gp_StateC08.previewSound = Gp_StateC08.wheelIndex * 3 + idx;
    }
    if (Gp_StateC08.previewSound > 0) {
        if (stateF0Gate_() == 0) {
            Gp_StateC08.previewSound = 0;
        }
    }
}

/// Waits for the battle-end player action, refills weapon supplies and advances.
///
/// Called in `HUD_BATTLE_STEP_WAIT_END_ACTION` with borrowed writable HUD state.
/// End-delay frames tick only while actors run. Ordinary completion is actor
/// phase 1000; weapon re-equip bypasses that wait and may restore normal
/// control while preserving the root offset. Reloading requires an equipped
/// weapon selector in 1..32. The
/// ordinary path also requires a live player task when aim exit is reached.
/// Keeps the HUD visible for a pending shooting-gallery reset; otherwise
/// suppresses it before advancing the battle step.
static __inline__ void _hudWaitForBattleEndAction(HudState* hud)
{
    enum { HUD_BATTLE_END_ACTION_COMPLETE = 1000 };
    s32                 actionComplete;
    s32                 flowFlags;
    s32                 weaponItemId;
    const PlayerStatus* player;
    s32                 usesTrainingLevels;

    SceneCombatState* combat;
    Task*             playerTask;
    s32               endDelayFrames;

    playerTask     = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    actionComplete = 0;
    combat         = &gSceneCombatState;
    endDelayFrames = combat->signals.bytes.endDelayFrames;
    if (endDelayFrames != 0) {
        if (combat->actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
            combat->signals.bytes.endDelayFrames = endDelayFrames - 1;
        }
    }
    if (playerTask != NULL) {
        const GameActor* actor = playerTask->work;

        if (actor->statePhase == HUD_BATTLE_END_ACTION_COMPLETE) {
            actionComplete = 1;
        }
    }
    flowFlags = gGameSession->flowFlags;
    if ((flowFlags & GAME_SESSION_FLOW_REEQUIP_WEAPON) == 0) {
        if (playerTask != NULL) {
            if (actionComplete == 0) {
                return;
            }
        }
        playerActorExitAim(playerTask);
    } else {
        if (flowFlags & GAME_SESSION_FLOW_HIDE_REEQUIPPED_WEAPON) {
            taskMessageDispatch(playerTask, GAME_ACTOR_MESSAGE_END_SCRIPTED, PLAYER_ACTOR_END_SCRIPTED_KEEP_ROOT_OFFSET, 0);
        }
    }
    // Refill both supply choices before advancing to the results transition.
    player       = &gPlayerStatus;
    weaponItemId = player->weapon + (EQUIPMENT_WEAPON_ITEM_FIRST - 1);
    equipmentReloadSelectedWeaponConsumable(weaponItemId, EQUIPMENT_WEAPON_SUPPLY_PRIMARY);
    equipmentReloadSelectedWeaponConsumable(weaponItemId, EQUIPMENT_WEAPON_SUPPLY_SECONDARY);
    usesTrainingLevels = _attachmentUsesTrainingLevels(player);
    if (usesTrainingLevels == 0 || gGameSession->battleResetPending == 0) {
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
                } else if (_hudCanSwitchCategory(1) != 0 && gPlayerStatus.armor != PLAYER_STATUS_EQUIPMENT_NONE) {
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
                    playerActorEnterReload(w, 0, PLAYER_ACTOR_RELOAD_BATTLE_END);
                }
                hud->battleStep = hud->battleStep + 1;
            } else if (step == HUD_BATTLE_STEP_WAIT_END_ACTION) {
                _hudWaitForBattleEndAction(hud);
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
                    hudDrawStatusBlock(hud);
                    if (equipmentHasEffect(EQUIPMENT_EFFECT_MOTION_DETECTOR) != 0) {
                        if (gGameSession->sceneUpdatesPaused == 0) {
                            hudDrawRadar(hud);
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
                hudDrawStatusBlock(hud);
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
