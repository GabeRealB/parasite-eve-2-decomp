#include "item_menu.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "common.h"

#include "gameplay/attachment_state.h"
#include "gameplay/inventory.h"
#include "inventory.h"
#include "gameplay/item_menu.h"
#include "item_use.h"
#include "gameplay/items.h"
#include "items.h"
#include "weapon_data.h"

#include "main/display.h"
#include "main/fs.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/sound.h"
#include "main/text.h"
#include "main/ui.h"
#include "main/wipsys.h"

/// Comparison values for weapon items 0x7F..0x9F (row zero is unequipped).
extern u16 Gp_WeaponStats[33][4];

/// Catalogue domains and comparison graphics used by the equipment choices.
enum {
    ITEM_MENU_ARMOR_ITEM_FIRST                  = 0x60,
    ITEM_MENU_EQUIPMENT_ITEM_COUNT              = 0x20,
    ITEM_MENU_CONSUMABLE_POWER_NONE             = INVENTORY_CONSUMABLE_ITEM_FIRST - 1,
    ITEM_MENU_PARASITE_ENERGY_ITEM_FIRST        = 0xF,
    ITEM_MENU_PARASITE_ENERGY_ITEM_COUNT        = ATTACHMENT_SPELL_COUNT * ATTACHMENT_AREA_LEVEL_COUNT,
    ITEM_MENU_WEAPON_WEIGHT_STAT                = 2,
    ITEM_MENU_STATS_NEUTRAL_COLOR_RGB           = 0x606060,
    ITEM_MENU_STATS_BETTER_COLOR_RGB            = 0x01741F,
    ITEM_MENU_STATS_WORSE_COLOR_RGB             = 0x0D287F,
    ITEM_MENU_STATS_INCREASE_TEXTURE_U          = 0x30,
    ITEM_MENU_STATS_DECREASE_TEXTURE_U          = 0xA0,
    ITEM_MENU_STATS_EQUAL_TEXTURE_U             = 0x78,
    ITEM_MENU_STATS_TEXTURE_V                   = 0x60,
    ITEM_MENU_STATS_TEXTURE_CLUT                = 0x3C09,
    ITEM_MENU_STATS_SPRITE_COMMAND              = 0x64,
    ITEM_MENU_RELOAD_WEAPON_SHIFT               = 8,
    ITEM_MENU_RELOAD_NOTICE_PANEL               = 39,
    ITEM_MENU_LOAD_SLOT_CHOICE_PANEL            = 40,
    ITEM_MENU_LOAD_AFTER_EQUIP                  = 0x10000,
    ITEM_MENU_EQUIPMENT_INFO_PANEL              = 45,
    ITEM_MENU_WEAPON_CHOICE_INITIAL             = 0,
    ITEM_MENU_WEAPON_CHOICE_ACTIVE              = 1,
    ITEM_MENU_WEAPON_CHOICE_EMPTY_TIMEOUT_TICKS = 188,
    ITEM_MENU_WEAPON_CHOICE_DISMISSED_TICKS     = 0x7FFF
};

/// Opening delay for choice-row child panels, in callback ticks.
enum { ITEM_MENU_CHOICE_CHILD_OPEN_DELAY_TICKS = 1 };

/// Borrows the read-only range/rate/weight row for weapon item id 0x80..0x9F.
///
/// The synthetic id 0x7F selects the all-zero unequipped row. The four-halfword
/// row belongs to the gameplay image; only its first three values are drawn.
static inline const u16* _itemMenuGetWeaponStats(s32 weaponItemId)
{
    return Gp_WeaponStats[weaponItemId - (EQUIPMENT_WEAPON_ITEM_FIRST - 1)];
}

/// Borrows one consumable's base damage for display as ammunition power.
///
/// Ids 0xA0..0xBF select the attack catalogue; ITEM_MENU_CONSUMABLE_POWER_NONE
/// selects its zero row. Only this halfword may be read through the returned
/// pointer; subsequent attack fields are not additional display statistics.
/// The pointer belongs to the gameplay image and is not retained by the drawer.
static inline const u16* _itemMenuGetConsumablePower(s32 consumableItemId)
{
    return &Gp_IdParamLo[consumableItemId - ITEM_MENU_CONSUMABLE_POWER_NONE].amount;
}

/// Draws `item`'s name, its equipment status marker in `mode`, the variant
/// marker for items 0x0F-0x32 and its icon at (`x`, `y`) in `obj`. Nothing is
/// drawn while `obj->panel.state` is `USER_INTERFACE_PANEL_HIDDEN`.
static inline void _gpDrawItemNameAt(UiObject* obj, s32 x, s32 y, s32 color, s32 item, s32 mode);

/// Makes `item` the preview in slot `slot` of `Gp_PreviewItems`, setting the
/// other two slots to -1, and queues its load. Nothing happens when the slot
/// already shows `item`.
static inline void _gpSetPreviewItem(s32 item, u8 slot);

/// `_gpSetPreviewItem` written as a walk of a pointer over `Gp_PreviewItems`
/// rather than an indexed store.
static inline void _gpSetPreviewItemWalk(s32 item, u8 slot);

static inline void _itemMenuDrawUnmarkedItemRowAt(UiObject* object, s32 x, s32 y, s32 colorRgb, s32 itemId);

u16 Gp_WeaponStats[33][4] = {
    { 0, 0, 0, 0 },
    { 70, 80, 100, 0 },
    { 50, 110, 117, 0 },
    { 40, 90, 227, 0 },
    { 70, 80, 87, 0 },
    { 120, 90, 92, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 60, 70, 168, 0 },
    { 0, 0, 0, 0 },
    { 350, 1, 260, 0 },
    { 350, 12, 900, 0 },
    { 30, 2, 270, 0 },
    { 40, 24, 420, 0 },
    { 50, 36, 550, 0 },
    { 500, 85, 254, 0 },
    { 400, 100, 685, 0 },
    { 0, 0, 0, 0 },
    { 1, 5, 68, 0 },
    { 500, 85, 274, 0 },
    { 500, 85, 294, 0 },
    { 1000, 7, 881, 0 },
    { 100, 36, 579, 0 },
    { 0, 0, 0, 0 },
    { 500, 85, 339, 0 },
    { 500, 85, 284, 0 },
    { 500, 85, 390, 0 },
    { 500, 85, 437, 0 },
    { 500, 85, 488, 0 },
    { 55, 80, 288, 0 },
    { 55, 80, 306, 0 },
    { 55, 80, 324, 0 }
};

/// Draws `item`'s name, its equipment status marker in `mode`, the variant
/// marker for items 0x0F-0x32 and its icon at (`x`, `y`) in `obj`. Nothing is
/// drawn while `obj->panel.state` is `USER_INTERFACE_PANEL_HIDDEN`.
static inline void _gpDrawItemNameAt(UiObject* obj, s32 x, s32 y, s32 color, s32 item, s32 mode)
{
    TextDrawReq req;
    s32         temp;

    if (obj->panel.state != USER_INTERFACE_PANEL_HIDDEN) {
        req.x          = obj->panel.contentOriginX.unsignedValue + 0x11 + x;
        req.y          = obj->panel.contentOriginY.unsignedValue + (y - 6);
        req.otIndex    = obj->panel.otIndex.signedValue + 1;
        req.colorRgb   = color;
        req.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
        req.alignment  = TEXT_ALIGNMENT_LEFT;
        req.drawMode   = TEXT_DRAW_OUTLINED;
        textDrawString(&req, itemGetText(item, ITEM_TEXT_NAME, 0));
        itemMenuDrawEquipmentMarker(obj, x, y, item, mode);
        temp = item - 0xF;
        if ((u32)temp < 0x24U) {
            itemMenuDrawParasiteEnergyLevel(obj, x, y, temp % 3 + 1, color);
        }
        itemMenuDrawItemIcon(obj, x, y, item, ITEM_MENU_ICON_DEFAULT);
    }
}

void itemMenuDrawEquipmentStats(const UiObject* object, s32 itemId, s32 displayMode, s32 unused)
{
    u8                         numberText[8];
    u16                        candidateArmorStats[3];
    u16                        equippedArmorStats[3];
    TextDrawReq                captionRequest;
    TextDrawReq                valueRequest;
    s32                        columnX;
    s32                        firstStatY;
    s32                        statY;
    s32                        statX;
    s32                        captionOriginX;
    s32                        otIndex;
    const u16*                 candidateStats;
    const u16*                 equippedStats;
    const u16*                 candidateStat;
    const u16*                 equippedStat;
    const UiList*              statList;
    SPRT*                      comparisonSprite;
    const ArmorStats*          armorStats;
    const EquipmentWeaponLoad* weaponLoad;
    const PlayerStatus*        player;
    s16                        contentTop;
    s32                        comparisonColorRgb;
    s32                        lowerIsBetter;
    s32                        statIndex;
    s32                        statCount;
    s32                        rightAlignment;
    u32                        equippedValue;
    u32                        candidateValue;
    s32                        spriteValue;

    contentTop = object->panel.contentTop.signedValue;
    columnX    = object->panel.contentLeft.signedValue + 0x60;
    firstStatY = contentTop + 8;
    player     = &gPlayerStatus;
    if (displayMode == ITEM_MENU_EQUIPMENT_STATS_SUMMARY) {
        firstStatY = contentTop + 0x1C;
    }
    if ((u32)(itemId - EQUIPMENT_WEAPON_ITEM_FIRST) < (u32)ITEM_MENU_EQUIPMENT_ITEM_COUNT) {
        statList       = &D_8010E9A4;
        candidateStats = _itemMenuGetWeaponStats(itemId);
        statCount      = 3;
        D_80114D80     = D_8010E984;
        equippedStats  = Gp_WeaponStats[player->weapon];
    } else if ((u32)(itemId - INVENTORY_CONSUMABLE_ITEM_FIRST) < (u32)INVENTORY_CONSUMABLE_ITEM_COUNT) {
        weaponLoad     = equipmentGetWeaponLoad(player->weapon + (EQUIPMENT_WEAPON_ITEM_FIRST - 1));
        statList       = &D_8010E9CC;
        candidateStats = _itemMenuGetConsumablePower(itemId);
        if (Gp_ReloadMode == EQUIPMENT_CLEAR_LOAD_SECONDARY) {
            if (weaponLoad->secondaryItemId == INVENTORY_ITEM_NONE) {
                equippedStats = _itemMenuGetConsumablePower(ITEM_MENU_CONSUMABLE_POWER_NONE);
            } else {
                equippedStats = _itemMenuGetConsumablePower(weaponLoad->secondaryItemId);
            }
        } else {
            if (weaponLoad->primaryItemId == INVENTORY_ITEM_NONE) {
                equippedStats = _itemMenuGetConsumablePower(ITEM_MENU_CONSUMABLE_POWER_NONE);
            } else {
                equippedStats = _itemMenuGetConsumablePower(weaponLoad->primaryItemId);
            }
        }
        D_80114D80 = D_8010E990;
        statCount  = 1;
    } else if ((u32)(itemId - ITEM_MENU_ARMOR_ITEM_FIRST) < (u32)ITEM_MENU_EQUIPMENT_ITEM_COUNT) {

        statList               = &D_8010E9F4;
        armorStats             = &Gp_ModStatAttrs[itemId - ITEM_MENU_ARMOR_ITEM_FIRST];
        candidateArmorStats[0] = armorStats->hpBonus;
        candidateArmorStats[1] = armorStats->mpBonus;
        candidateArmorStats[2] = equipmentGetArmorAttachmentSlotCount(itemId);
        candidateStats         = candidateArmorStats;
        equippedStats          = equippedArmorStats;
        armorStats             = &Gp_ModStatAttrs[(player->armor + (ITEM_MENU_ARMOR_ITEM_FIRST - 1)) - ITEM_MENU_ARMOR_ITEM_FIRST];
        statCount              = 3;
        equippedArmorStats[0]  = armorStats->hpBonus;
        equippedArmorStats[1]  = armorStats->mpBonus;
        equippedArmorStats[2]  = equipmentGetArmorAttachmentSlotCount(player->armor + (ITEM_MENU_ARMOR_ITEM_FIRST - 1));
        D_80114D80             = D_8010E994;
    } else {
        return;
    }

    // Compare catalogue display values; only lower weapon weight is favorable.
    statX     = columnX;
    statY     = firstStatY;
    statIndex = 0;
    if (statCount != 0) {
        rightAlignment = TEXT_ALIGNMENT_RIGHT;
        candidateStat  = candidateStats;
        equippedStat   = equippedStats;
        do {
            captionOriginX            = object->panel.contentOriginX.unsignedValue - 0xA;
            captionRequest.x          = captionOriginX + statX;
            captionRequest.y          = object->panel.contentOriginY.unsignedValue + statY;
            otIndex                   = object->panel.otIndex.signedValue + 1;
            captionRequest.otIndex    = otIndex;
            captionRequest.colorRgb   = ITEM_MENU_STATS_NEUTRAL_COLOR_RGB;
            captionRequest.glyphTable = TEXT_GLYPH_TABLE_SMALL;
            captionRequest.alignment  = TEXT_ALIGNMENT_LEFT;
            captionRequest.drawMode   = TEXT_DRAW_OUTLINED;
            textDrawString(&captionRequest, D_80114D80[statIndex]);
            lowerIsBetter = 0;
            // Weight is column two, the same numeric value as right alignment.
            if (statIndex == rightAlignment) {
                lowerIsBetter = statList == &D_8010E9A4;
            }
            if (displayMode == ITEM_MENU_EQUIPMENT_STATS_COMPARE) {
                equippedValue  = *equippedStat;
                candidateValue = *candidateStat;
                if (equippedValue < candidateValue) {
                    comparisonColorRgb = ITEM_MENU_STATS_BETTER_COLOR_RGB;
                    if (lowerIsBetter != 0) {
                        comparisonColorRgb = ITEM_MENU_STATS_WORSE_COLOR_RGB;
                    }
                } else if (candidateValue < equippedValue) {
                    comparisonColorRgb = ITEM_MENU_STATS_WORSE_COLOR_RGB;
                    if (lowerIsBetter != 0) {
                        comparisonColorRgb = ITEM_MENU_STATS_BETTER_COLOR_RGB;
                    }
                } else {
                    comparisonColorRgb = ITEM_MENU_STATS_NEUTRAL_COLOR_RGB;
                }
            } else {
                comparisonColorRgb = ITEM_MENU_STATS_NEUTRAL_COLOR_RGB;
            }
            if (((u32)(itemId - ITEM_MENU_ARMOR_ITEM_FIRST) < (u32)ITEM_MENU_EQUIPMENT_ITEM_COUNT) && (statIndex < 2)) {
                {
                    s32 valueOriginX;
                    valueOriginX   = object->panel.contentOriginX.unsignedValue - 2;
                    valueRequest.x = object->panel.contentRight.unsignedValue + valueOriginX;
                }
                valueRequest.y          = object->panel.contentOriginY.unsignedValue + 0xB + statY;
                valueRequest.otIndex    = object->panel.otIndex.signedValue + 1;
                valueRequest.colorRgb   = comparisonColorRgb;
                valueRequest.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
                valueRequest.alignment  = rightAlignment;
                valueRequest.drawMode   = TEXT_DRAW_TRANSLUCENT_OUTLINED;
                textDrawString(&valueRequest, textItoaSignPrefixed(numberText, *candidateStat));
            } else {
                {
                    s32 valueOriginX;
                    valueOriginX   = object->panel.contentOriginX.unsignedValue - 2;
                    valueRequest.x = object->panel.contentRight.unsignedValue + valueOriginX;
                }
                valueRequest.y          = object->panel.contentOriginY.unsignedValue + 0xB + statY;
                valueRequest.otIndex    = object->panel.otIndex.signedValue + 1;
                valueRequest.colorRgb   = comparisonColorRgb;
                valueRequest.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
                valueRequest.alignment  = rightAlignment;
                valueRequest.drawMode   = TEXT_DRAW_TRANSLUCENT_OUTLINED;
                textDrawString(&valueRequest, textItoaSigned(numberText, *candidateStat));
            }
            statY         += 0x18;
            candidateStat += 1;
            statIndex     += 1;
            equippedStat  += 1;
        } while (statIndex < statCount);
    }

    // Draw numerical direction independently of the favorable/unfavorable color.
    statIndex = 0;
    if (displayMode == ITEM_MENU_EQUIPMENT_STATS_COMPARE) {
        statY = firstStatY;
        statX = columnX;
        if (statCount != 0) {
            s32 weightStatIndex;
            weightStatIndex = ITEM_MENU_WEAPON_WEIGHT_STAT;

            do {
                lowerIsBetter = 0;
                if (statIndex == weightStatIndex) {
                    lowerIsBetter = statList == &D_8010E9A4;
                }
                comparisonSprite     = gGpuPrimCursor;
                comparisonSprite->x0 = object->panel.contentOriginX.unsignedValue + statX;
                comparisonSprite->y0 = object->panel.contentOriginY.unsignedValue + statY + 5;
                spriteValue          = 8;
                comparisonSprite->w  = spriteValue;
                comparisonSprite->h  = spriteValue;
                equippedValue        = equippedStats[statIndex];
                candidateValue       = candidateStats[statIndex];
                gGpuPrimCursor       = comparisonSprite + 1;
                if (equippedValue < candidateValue) {
                    comparisonSprite->u0                          = ITEM_MENU_STATS_INCREASE_TEXTURE_U;
                    GPU_PRIMITIVE_COLOR_WORD(comparisonSprite, 0) = ITEM_MENU_STATS_BETTER_COLOR_RGB;
                    if (lowerIsBetter != 0) {
                        GPU_PRIMITIVE_COLOR_WORD(comparisonSprite, 0) = ITEM_MENU_STATS_WORSE_COLOR_RGB;
                    }
                } else if (candidateValue < equippedValue) {
                    comparisonSprite->u0                          = ITEM_MENU_STATS_DECREASE_TEXTURE_U;
                    GPU_PRIMITIVE_COLOR_WORD(comparisonSprite, 0) = ITEM_MENU_STATS_WORSE_COLOR_RGB;
                    comparisonSprite->y0                          = comparisonSprite->y0 - 1;
                    if (lowerIsBetter != 0) {
                        GPU_PRIMITIVE_COLOR_WORD(comparisonSprite, 0) = ITEM_MENU_STATS_BETTER_COLOR_RGB;
                    }
                } else {
                    comparisonSprite->u0                          = ITEM_MENU_STATS_EQUAL_TEXTURE_U;
                    spriteValue                                   = ITEM_MENU_STATS_NEUTRAL_COLOR_RGB;
                    GPU_PRIMITIVE_COLOR_WORD(comparisonSprite, 0) = spriteValue;
                }
                statY                 += 0x18;
                comparisonSprite->v0   = ITEM_MENU_STATS_TEXTURE_V;
                comparisonSprite->clut = ITEM_MENU_STATS_TEXTURE_CLUT;
                setlen(comparisonSprite, sizeof(*comparisonSprite) / sizeof(u32) - 1);
                setcode(comparisonSprite, ITEM_MENU_STATS_SPRITE_COMMAND);
                addPrim(gGpuCurrentOt + object->panel.otIndex.signedValue + 1, comparisonSprite);
                statIndex += 1;
            } while (statIndex < statCount);
        }
        uiQueueTexturePage(object->panel.otIndex.signedValue + 1, 0);
    }
}

/// Makes `item` the preview in slot `slot` of `Gp_PreviewItems`, setting the
/// other two slots to -1, and queues its load. Nothing happens when the slot
/// already shows `item`.
static inline void _gpSetPreviewItem(s32 item, u8 slot)
{
    s32 i;

    if (item != Gp_PreviewItems[slot]) {
        for (i = 0; i < 3; i++) {
            if (i == slot) {
                Gp_PreviewItems[i] = item;
            } else {
                Gp_PreviewItems[i] = -1;
            }
        }
        itemMenuEnqueuePreviewLoad(item, slot);
    }
}

/// `_gpSetPreviewItem` written as a walk of a pointer over `Gp_PreviewItems`
/// rather than an indexed store.
static inline void _gpSetPreviewItemWalk(s32 item, u8 slot)
{
    s32  i;
    s32* p;

    p = Gp_PreviewItems;
    if (item != p[slot]) {
        for (i = 0; i < 3; i++) {
            if (i == slot) {
                *p++ = item;
            } else {
                *p++ = -1;
            }
        }
        itemMenuEnqueuePreviewLoad(item, slot);
    }
}

void Gp_EquipSummaryTask(Task* arg0)
{
    PlayerStatus*        cfg;
    UiObject*            obj;
    EquipmentWeaponLoad* slotp;
    s32*                 stored;
    s32                  mode;
    s32                  item;
    s32                  skip;
    s32                  slot;
    s32                  flags;

    item   = 0;
    skip   = 0;
    cfg    = &gPlayerStatus;
    stored = (s32*)arg0->work;
    mode   = arg0->spawnArg1.value;
    obj    = arg0->spawnArg2.pointer;
    slot   = 0;
    if (mode == 0) {
        uiDrawPanelLabel(&(obj)->panel, Gp_StrWeaponTitle);
        item = cfg->weapon + 0x7F;
        if (item < 0x80) {
            item = 0;
        }
    } else if (mode == 1) {
        uiDrawPanelLabel(&(obj)->panel, Gp_StrAmmoCaps);
        slotp = equipmentGetWeaponLoad(cfg->weapon + 0x7F);
        item  = slotp->primaryItemId;
        if (Gp_ReloadMode == 2) {
            item = slotp->secondaryItemId;
        }
    } else if (mode == 2) {
        uiDrawPanelLabel(&(obj)->panel, Gp_StrArmor);
        item = cfg->armor + 0x5F;
    } else {
        uiDrawPanelLabel(&(obj)->panel, Gp_StrAttachments);
        skip = 1;
        if (Gp_SelItemRec != NULL) {
            item = Gp_SelItemRec->itemId;
        }
    }

    if (arg0->state == 0) {
        stored           = memCalloc(4, 0);
        Gp_ItemCountShow = 1;
        arg0->work       = stored;
        *stored          = item;
        arg0->state      = 2;
    }

    if (*stored != item) {
        _gpSetPreviewItem(item, slot);
        *stored     = item;
        arg0->state = 2;
    }

    if (item != 0) {
        _gpDrawItemNameAt(obj, obj->panel.contentLeft.signedValue + 2, obj->panel.contentTop.signedValue + 0xF, uiGetTextColor(obj, USER_INTERFACE_TEXT_COLOR_NORMAL), item, 1);
    }

    uiDrawHorizontalSeparator(&(obj)->panel, obj->panel.contentLeft.signedValue, obj->panel.contentRight.signedValue, obj->panel.contentTop.signedValue + 0x11);
    if (skip == 0) {
        itemMenuDrawEquipmentStats(obj, item, ITEM_MENU_EQUIPMENT_STATS_SUMMARY, 0);
    }

    flags = slot + ITEM_MENU_PREVIEW_SCALE_EQUIPMENT;
    if ((arg0->state != 1) || (item == 0)) {
        flags |= ITEM_MENU_PREVIEW_HIDDEN;
    }
    itemMenuDrawPreview(obj, obj->panel.contentLeft.signedValue + 2, obj->panel.contentTop.signedValue + 0x16, flags);

    if (arg0->state == 2) {
        if (cdCmdIsIdle()) {
            arg0->state = 1;
        }
    }

    if (obj->panel.state == USER_INTERFACE_PANEL_CLOSING) {
        Gp_ItemCountShow = 0;
    }
}

/// Draws an item's name, Parasite Energy level and icon without an equipment mark.
///
/// x/y are signed pixel offsets from the content origin, with y at the label's
/// baseline. PE item ids 15..50 show levels 1..3. Uses the current catalogue
/// name and default icon at the panel's OT layer plus one. A hidden panel draws
/// nothing. Borrows the live object and loaded menu/font resources; drawing
/// requires writable GPU storage and retains no input pointer.
static inline void _itemMenuDrawUnmarkedItemRowAt(UiObject* object, s32 x, s32 y, s32 colorRgb, s32 itemId)
{
    TextDrawReq nameRequest;
    s32         energyLevelIndex;

    if (object->panel.state != USER_INTERFACE_PANEL_HIDDEN) {
        nameRequest.x          = object->panel.contentOriginX.unsignedValue + 0x11 + x;
        nameRequest.y          = object->panel.contentOriginY.unsignedValue + (y - 6);
        nameRequest.otIndex    = object->panel.otIndex.signedValue + 1;
        nameRequest.colorRgb   = colorRgb;
        nameRequest.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
        nameRequest.alignment  = TEXT_ALIGNMENT_LEFT;
        nameRequest.drawMode   = TEXT_DRAW_OUTLINED;
        textDrawString(&nameRequest, itemGetText(itemId, ITEM_TEXT_NAME, 0));
        energyLevelIndex = itemId - ITEM_MENU_PARASITE_ENERGY_ITEM_FIRST;
        if ((u32)energyLevelIndex < (u32)ITEM_MENU_PARASITE_ENERGY_ITEM_COUNT) {
            itemMenuDrawParasiteEnergyLevel(object, x, y, energyLevelIndex % ATTACHMENT_AREA_LEVEL_COUNT + 1, colorRgb);
        }
        itemMenuDrawItemIcon(object, x, y, itemId, ITEM_MENU_ICON_DEFAULT);
    }
}

void itemMenuDrawWeaponChoiceRow(UiList* list, UiObject* object)
{
    s32       consumableItemId;
    s32       weaponItemId;
    s32       controlWord;
    UiObject* loadDialog;

    consumableItemId = object->owner->spawnArg1.value;
    // The original condition is unproven; both branches select the same weapon.
    if (consumableItemId > 0) {
        weaponItemId = inventoryGetNthWeaponForConsumable(&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems, list->currentItemIndex, consumableItemId);
    } else {
        weaponItemId = inventoryGetNthWeaponForConsumable(&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems, list->currentItemIndex, consumableItemId);
    }
    controlWord = object->panel.control.word;
    if (((controlWord >> 16) == USER_INTERFACE_PANEL_ACTIVE) || (controlWord == USER_INTERFACE_PANEL_ACTIVE)) {
        if (list->selectedItemIndex == list->currentItemIndex) {
            if (weaponItemId == 0) {
                uiSetPromptText((const u8*)Gp_StrEmpty, 0, 0);
            } else {
                uiSetPromptText(itemGetText(weaponItemId, ITEM_TEXT_DESCRIPTION_FIRST, 0), 0, 0);
            }
            if (consumableItemId != 0) {
                _gpSetPreviewItem(weaponItemId, 0);
            }
        }
    }

    if (consumableItemId == 0) {
        _gpDrawItemNameAt(object, list->rowTextX.signedValue, list->rowTextY.signedValue, list->colorRgb, weaponItemId, 1);
    } else {
        _itemMenuDrawUnmarkedItemRowAt(object, list->rowTextX.signedValue, list->rowTextY.signedValue, list->colorRgb, weaponItemId);
    }

    if (list->rowInputEnabled == USER_INTERFACE_LIST_ROW_ACTIVE) {
        object->resultValue = weaponItemId;
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm) != 0) {
            if (object->owner->spawnArg1.value == 0) {
                sndEvtRequestScriptStart(SOUND_WEAPON_EQUIP, 0, 0);
                equipmentEquipCarriedWeapon(weaponItemId);
                Gp_ReloadMode = EQUIPMENT_CLEAR_LOAD_BOTH;
                loadDialog    = uiSpawnObject(&D_8010EAB4[ITEM_MENU_LOAD_SLOT_CHOICE_PANEL], weaponItemId | ITEM_MENU_LOAD_AFTER_EQUIP, USER_INTERFACE_PANEL_ACTIVE, ITEM_MENU_CHOICE_CHILD_OPEN_DELAY_TICKS, object);
                if (loadDialog != NULL) {
                    uiPositionRowDialog(&(loadDialog)->panel, list, &(object)->panel);
                }
                object->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
            } else {
                sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
                uiSpawnObject(&D_8010EAB4[ITEM_MENU_RELOAD_NOTICE_PANEL], (weaponItemId << ITEM_MENU_RELOAD_WEAPON_SHIFT) | consumableItemId, USER_INTERFACE_PANEL_ACTIVE, ITEM_MENU_CHOICE_CHILD_OPEN_DELAY_TICKS, object);
                object->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
            }
        } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_TRIANGLE) != 0) {
            sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
            if (consumableItemId != 0) {
                uiSpawnObject(&D_8010EAB4[ITEM_MENU_EQUIPMENT_INFO_PANEL], weaponItemId, USER_INTERFACE_PANEL_ACTIVE, ITEM_MENU_CHOICE_CHILD_OPEN_DELAY_TICKS, object);
            } else {
                uiSpawnObject(&D_8010EAB4[ITEM_MENU_EQUIPMENT_INFO_PANEL], weaponItemId | ITEM_MENU_INFO_RELOCATED_PREVIEW, USER_INTERFACE_PANEL_ACTIVE, ITEM_MENU_CHOICE_CHILD_OPEN_DELAY_TICKS, object);
            }
            object->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
        }
    } else if (object->panel.control.word != USER_INTERFACE_PANEL_ACTIVE) {
        if (list->currentItemIndex == 0) {
            if (object->resultValue == 0) {
                object->resultValue = weaponItemId;
            }
        }
    }
}

/// Propagates a child result or closes its accepted panel and restores list input.
///
/// Both objects are live members of the menu's task tree. The child is retained
/// through its closing animation; this operation neither frees it nor unlinks it.
static inline void _itemMenuApplyWeaponChoiceChildResult(UiObject* object, UiObject* childObject, s32 childResult)
{
    switch (childResult) {
        case USER_INTERFACE_RESULT_DISMISS:
            object->result = childResult;
            break;
        case USER_INTERFACE_RESULT_CANCEL:
            object->result = childResult;
            break;
        case USER_INTERFACE_RESULT_CONFIRM:
            uiStartTreeClosing(childObject, childObject->owner);
            object->panel.control.word = USER_INTERFACE_PANEL_ACTIVE;
            break;
    }
}

void itemMenuWeaponChoiceListTask(Task* task)
{
    UiObject* object;
    UiList*   list;
    s32       consumableItemId;
    s32       activeState;
    Task*     childTask;
    Task*     nextChild;
    Task*     firstChild;
    UiObject* childObject;
    s32       childResult;

    object           = task->spawnArg2.pointer;
    consumableItemId = task->spawnArg1.value;
    object->result   = USER_INTERFACE_RESULT_NONE;
    list             = &D_8010E9A4;
    if (task->state == ITEM_MENU_WEAPON_CHOICE_INITIAL) {
        itemMenuSetWeaponChoiceRows(list, consumableItemId);
        uiFitPanelToList(list, &(object)->panel);
        if (consumableItemId == 0) {
            list->topInset                      += 0x4C;
            object->panel.bounds.unsignedRect.h += 0x4C;
        }
        list->flags                               = USER_INTERFACE_LIST_SHARED_ROW_CALLBACK;
        list->selectedItemIndex                   = 0;
        list->firstVisibleItemIndex.unsignedValue = 0;
        task->state                               = task->state + 1;
        if (list->itemCount == 0) {
            task->state          = task->state + 1;
            task->killCountdown  = ITEM_MENU_WEAPON_CHOICE_EMPTY_TIMEOUT_TICKS;
            object->panel.style |= USER_INTERFACE_PANEL_TITLE_STYLE;
            uiSizePanelForTextDefault(&(object)->panel, Gp_StrNoWeaponEq);
            return;
        }
        if ((s16)object->panel.bounds.unsignedRect.y + (s16)object->panel.bounds.unsignedRect.h < 0x47) {
            return;
        }
        object->panel.bounds.unsignedRect.y = 0x46 - object->panel.bounds.unsignedRect.h;
        return;
    }
    // Active state, pressed-button queries and outlined text all use value one.
    activeState = ITEM_MENU_WEAPON_CHOICE_ACTIVE;
    if (task->state == activeState) {
        uiUpdateList(list, &object->panel);
        if (object->panel.control.word == activeState) {
            if (padCheckButtons(0, activeState, Pad_MaskMenu) != 0) {
                object->result = USER_INTERFACE_RESULT_CANCEL;
            } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskCancel) != 0) {
                sndEvtRequestScriptStart(SOUND_MENU_CANCEL, 0, 0);
                object->result = USER_INTERFACE_RESULT_CONFIRM;
            }
        }
        childTask = task->firstChild;
        if (childTask != NULL) {
            do {
                childObject = childTask->spawnArg2.pointer;
                childResult = childObject->result;
                nextChild   = childTask->nextSibling;
                _itemMenuApplyWeaponChoiceChildResult(object, childObject, childResult);
                firstChild = task->firstChild;
                childTask  = nextChild;
                if (childTask == firstChild) {
                    break;
                }
                if (firstChild == NULL) {
                    break;
                }
            } while (1);
        }
        return;
    }
    uiDrawPanelLabel(&(object)->panel, Gp_StrAttention);
    {
        s32 textDrawMode = activeState;

        textDrawUiLines(object, object->panel.contentLeft.signedValue + 2, object->panel.contentTop.signedValue + 0xF, Gp_StrNoWeaponEq, ITEM_MENU_STATS_NEUTRAL_COLOR_RGB, textDrawMode, TEXT_ALIGNMENT_LEFT);
    }
    task->killCountdown--;
    if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskMenu) != 0) {
        object->result = USER_INTERFACE_RESULT_CANCEL;
        return;
    }
    if ((task->killCountdown == 0) || (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm | Pad_MaskCancel) != 0)) {
        object->result      = USER_INTERFACE_RESULT_DISMISS;
        task->killCountdown = ITEM_MENU_WEAPON_CHOICE_DISMISSED_TICKS;
    }
}

/// Sets bit 0x100 in `flags`, which makes `itemMenuDrawPreview` skip drawing the
/// item preview, while the CD queue is still busy loading it.
#define GP_HIDE_PREVIEW_WHILE_CD_BUSY(flags)     \
    do {                                         \
        if (cdCmdIsIdle() == 0) {                \
            (flags) |= ITEM_MENU_PREVIEW_HIDDEN; \
        }                                        \
    } while (0)

void Gp_SelectWeaponMenuTask(Task* arg0)
{
    UiList*       menu;
    UiObject*     obj;
    s32           val;
    PlayerStatus* cfg;
    s32           flags;
    Task*         parent;

    menu = &D_8010E9A4;
    obj  = arg0->spawnArg2.pointer;
    cfg  = &gPlayerStatus;
    uiDrawPanelLabel(&(obj)->panel, Gp_StrSelectWeapon);
    uiDrawHorizontalSeparator(&(obj)->panel, obj->panel.contentLeft.signedValue, obj->panel.contentRight.signedValue, obj->panel.contentTop.signedValue + 0x4A);
    if (arg0->state == 0) {
        parent     = arg0->parent;
        D_80114DD8 = -1;
        uiStartPanelHiding(parent->spawnArg2.pointer, parent);
        uiSpawnObject(&D_8010EAB4[14], 0, 0, 0x10, obj);
    }
    val = inventoryGetNthWeaponForConsumable(&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems, menu->selectedItemIndex, 0);
    if (((obj->panel.control.word >> 16) == USER_INTERFACE_PANEL_ACTIVE) || (obj->panel.control.word == USER_INTERFACE_PANEL_ACTIVE) || (val != cfg->weapon + 0x7F)) {
        flags = ITEM_MENU_PREVIEW_TEXTURE_RELOCATED | ITEM_MENU_PREVIEW_SCALE_EQUIPMENT;
        if (val == 0) {
            flags = ITEM_MENU_PREVIEW_TEXTURE_RELOCATED | ITEM_MENU_PREVIEW_SCALE_EQUIPMENT | ITEM_MENU_PREVIEW_HIDDEN;
            goto draw;
        }
        if (((obj->panel.control.word >> 16) == USER_INTERFACE_PANEL_ACTIVE) || (obj->panel.control.word == USER_INTERFACE_PANEL_ACTIVE)) {
            _gpSetPreviewItemWalk(val, 2);
        }
    } else {
        flags = ITEM_MENU_PREVIEW_SCALE_EQUIPMENT;
        if (val == 0) {
            flags = ITEM_MENU_PREVIEW_SCALE_EQUIPMENT | ITEM_MENU_PREVIEW_HIDDEN;
            goto draw;
        }
    }
    GP_HIDE_PREVIEW_WHILE_CD_BUSY(flags);
draw:
    itemMenuDrawPreview(obj, obj->panel.contentLeft.signedValue + 2, obj->panel.contentTop.signedValue + 2, flags);
    itemMenuDrawEquipmentStats(obj, val, ITEM_MENU_EQUIPMENT_STATS_COMPARE, 0);
    itemMenuWeaponChoiceListTask(arg0);
    obj->resultValue = 0;
    if (obj->result == USER_INTERFACE_RESULT_DISMISS) {
        obj->result = USER_INTERFACE_RESULT_CONFIRM;
    }
}

void itemMenuDrawConsumableChoiceRow(UiList* list, UiObject* object)
{
    s32                        consumableItemId;
    s32                        weaponItemId;
    s32                        controlWord;
    const InventoryItemRow*    carriedRow;
    s32                        availableQuantity;
    const EquipmentWeaponLoad* weaponLoad;
    // Quantity formatting and the following label reuse the same scratch bytes.
    union {
        struct {
            u8          numberText[0x20];
            TextDrawReq request;
        } quantity;
        TextDrawReq request;
    } textScratch;

    consumableItemId = Gp_AttachListIds[list->currentItemIndex];
    weaponItemId     = (u16)object->owner->spawnArg1.value;
    controlWord      = object->panel.control.word;
    if (((controlWord >> 16) == USER_INTERFACE_PANEL_ACTIVE) || (controlWord == USER_INTERFACE_PANEL_ACTIVE)) {
        if (list->selectedItemIndex == list->currentItemIndex) {
            if (consumableItemId == 0) {
                uiSetPromptText((const u8*)Gp_StrRemoveAmmoHelp, 0, 0);
            } else {
                uiSetPromptText(itemGetText(consumableItemId, ITEM_TEXT_DESCRIPTION_FIRST, 0), 0, 0);
                if (Gp_ReloadMode == EQUIPMENT_CLEAR_LOAD_BOTH) {
                    _gpSetPreviewItem(consumableItemId, 2);
                }
            }
        }
    }

    if (consumableItemId != 0) {
        // Inventory totals include loaded units; return this weapon's load in BOTH mode.
        carriedRow        = inventoryFindLastCarriedItemRow(consumableItemId);
        availableQuantity = carriedRow->qty - equipmentGetLoadedConsumableQuantity(&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems, consumableItemId);
        if (Gp_ReloadMode == EQUIPMENT_CLEAR_LOAD_BOTH) {
            weaponLoad = equipmentGetWeaponLoad(weaponItemId);
            if (weaponLoad->primaryItemId == consumableItemId) {
                availableQuantity += weaponLoad->primaryQty;
            } else if (weaponLoad->secondaryItemId == consumableItemId) {
                availableQuantity += weaponLoad->secondaryQty;
            }
        }
        {
            s32 x;
            s32 y;
            s32 colorRgb;

            x                                       = list->rowTextX.signedValue;
            y                                       = list->rowTextY.signedValue;
            colorRgb                                = list->colorRgb;
            textScratch.quantity.request.x          = object->panel.contentOriginX.unsignedValue + 0x84 + x;
            textScratch.quantity.request.y          = object->panel.contentOriginY.unsignedValue + (y - 3);
            textScratch.quantity.request.otIndex    = object->panel.otIndex.signedValue + 1;
            textScratch.quantity.request.colorRgb   = colorRgb;
            textScratch.quantity.request.glyphTable = TEXT_GLYPH_TABLE_SMALL;
            textScratch.quantity.request.alignment  = TEXT_ALIGNMENT_RIGHT;
            textScratch.quantity.request.drawMode   = TEXT_DRAW_FILL_ONLY;
            textDrawString(&textScratch.quantity.request, textItoaSigned(textScratch.quantity.numberText, availableQuantity));
            uiDrawRecessedRect(&object->panel, (x + 0x69), (y - 8), 0x1B, 7,
                               0x102010);
        }
        {
            s32 x;
            s32 y;
            s32 colorRgb;
            s32 energyLevelIndex;

            x        = list->rowTextX.signedValue;
            y        = list->rowTextY.signedValue;
            colorRgb = list->colorRgb;
            if (object->panel.state != USER_INTERFACE_PANEL_HIDDEN) {
                textScratch.request.x          = object->panel.contentOriginX.unsignedValue + 0x11 + x;
                textScratch.request.y          = object->panel.contentOriginY.unsignedValue + (y - 6);
                textScratch.request.otIndex    = object->panel.otIndex.signedValue + 1;
                textScratch.request.colorRgb   = colorRgb;
                textScratch.request.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
                textScratch.request.alignment  = TEXT_ALIGNMENT_LEFT;
                textScratch.request.drawMode   = TEXT_DRAW_OUTLINED;
                textDrawString(&textScratch.request, itemGetText(consumableItemId, ITEM_TEXT_NAME, 0));
                energyLevelIndex = consumableItemId - ITEM_MENU_PARASITE_ENERGY_ITEM_FIRST;
                if ((u32)energyLevelIndex < (u32)ITEM_MENU_PARASITE_ENERGY_ITEM_COUNT) {
                    itemMenuDrawParasiteEnergyLevel(object, x, y, energyLevelIndex % ATTACHMENT_AREA_LEVEL_COUNT + 1, colorRgb);
                }
                itemMenuDrawItemIcon(object, x, y, consumableItemId, ITEM_MENU_ICON_DEFAULT);
            }
        }
    } else {
        s32 textOriginY;

        textScratch.request.x          = object->panel.contentOriginX.unsignedValue + list->rowTextX.unsignedValue;
        textOriginY                    = object->panel.contentOriginY.unsignedValue - 6;
        textScratch.request.y          = textOriginY + list->rowTextY.unsignedValue;
        textScratch.request.otIndex    = object->panel.otIndex.signedValue + 1;
        textScratch.request.colorRgb   = list->colorRgb;
        textScratch.request.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
        textScratch.request.alignment  = TEXT_ALIGNMENT_LEFT;
        textScratch.request.drawMode   = TEXT_DRAW_OUTLINED;
        textDrawString(&textScratch.request, (const u8*)Gp_StrRemoveAmmo);
    }

    if (list->rowInputEnabled == USER_INTERFACE_LIST_ROW_ACTIVE) {
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm) != 0) {
            uiSpawnObject(&D_8010EAB4[ITEM_MENU_RELOAD_NOTICE_PANEL], (weaponItemId << ITEM_MENU_RELOAD_WEAPON_SHIFT) | consumableItemId, USER_INTERFACE_PANEL_ACTIVE, ITEM_MENU_CHOICE_CHILD_OPEN_DELAY_TICKS, object);
            object->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
        } else if ((padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_TRIANGLE) != 0) && (consumableItemId != 0)) {
            sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
            // Both branches open the same specifications panel.
            if (weaponItemId != 0) {
                uiSpawnObject(&D_8010EAB4[ITEM_MENU_EQUIPMENT_INFO_PANEL], consumableItemId | ITEM_MENU_INFO_RELOCATED_PREVIEW, USER_INTERFACE_PANEL_ACTIVE, ITEM_MENU_CHOICE_CHILD_OPEN_DELAY_TICKS, object);
            } else {
                uiSpawnObject(&D_8010EAB4[ITEM_MENU_EQUIPMENT_INFO_PANEL], consumableItemId | ITEM_MENU_INFO_RELOCATED_PREVIEW, USER_INTERFACE_PANEL_ACTIVE, ITEM_MENU_CHOICE_CHILD_OPEN_DELAY_TICKS, object);
            }
            object->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
        }
    }
}

/// Gets available rounds/supply units, including the selected load only for BOTH mode.
///
/// Borrows a readable carried range and its weapon's live load record. itemId
/// selects a consumable stack; secondary is 0 for primary, nonzero for secondary.
/// The stack query's signed-16-bit quantity promotion is preserved. No storage
/// is changed or retained, and built-in supply quantities are handled alike.
static inline s32 _itemMenuGetLoadableQuantity(const InventoryItemRange* range, const EquipmentWeaponLoad* weaponLoad, s32 loadSelection, s32 itemId, s32 secondary)
{
    s32 quantity;
    quantity  = inventoryGetConsumableStackQuantity(range, itemId);
    quantity -= equipmentGetLoadedConsumableQuantity(range, itemId);
    if (loadSelection == EQUIPMENT_CLEAR_LOAD_BOTH) {
        if (secondary) {
            if (weaponLoad->secondaryItemId == itemId)
                quantity += weaponLoad->secondaryQty;
        } else {
            if (weaponLoad->primaryItemId == itemId)
                quantity += weaponLoad->primaryQty;
        }
    }
    return quantity;
}

void itemMenuBuildConsumableChoiceList(UiList* list, s32 weaponItemId)
{
    const InventoryItemRange*  carriedItems;
    const EquipmentWeaponLoad* weaponLoad;
    s32                        loadSelection;
    s32                        consumableCount = 0;
    s32                        rowCount;
    s32                        choiceIndex = 0;
    s32                        consumableItemId;
    s32                        availableQuantity;

    carriedItems  = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems;
    loadSelection = Gp_ReloadMode;
    weaponLoad    = equipmentGetWeaponLoad(weaponItemId);
    rowCount      = 0;
    // Keep catalogue order, including separate occurrences in the two loads.
    if (loadSelection != EQUIPMENT_CLEAR_LOAD_SECONDARY) {
        for (choiceIndex = 0; choiceIndex < ARRAY_SIZE(Gp_RelatedQty0.rows[0].acceptedItemIds); choiceIndex++) {
            consumableItemId = Gp_RelatedQty0.rows[weaponItemId - EQUIPMENT_WEAPON_ITEM_FIRST].acceptedItemIds[choiceIndex];
            if (consumableItemId != INVENTORY_ITEM_NONE) {
                availableQuantity = _itemMenuGetLoadableQuantity(carriedItems, weaponLoad, loadSelection, consumableItemId, 0);
                if (availableQuantity > 0) {
                    Gp_AttachListIds[consumableCount++] = consumableItemId;
                    rowCount++;
                }
            }
        }
    }
    if (loadSelection != EQUIPMENT_CLEAR_LOAD_PRIMARY) {
        for (choiceIndex = 0; choiceIndex < ARRAY_SIZE(Gp_RelatedQty1.rows[0].acceptedItemIds); choiceIndex++) {
            consumableItemId = Gp_RelatedQty1.rows[weaponItemId - EQUIPMENT_WEAPON_ITEM_FIRST].acceptedItemIds[choiceIndex];
            if (consumableItemId != INVENTORY_ITEM_NONE) {
                availableQuantity = _itemMenuGetLoadableQuantity(carriedItems, weaponLoad, loadSelection, consumableItemId, 1);
                if (availableQuantity > 0) {
                    Gp_AttachListIds[consumableCount++] = consumableItemId;
                    rowCount++;
                }
            }
        }
    }
    if (loadSelection != EQUIPMENT_CLEAR_LOAD_BOTH && rowCount > 0) {
        Gp_AttachListIds[consumableCount] = INVENTORY_ITEM_NONE;
        rowCount++;
    }
    list->itemCount                     = rowCount;
    list->visibleRowCount.unsignedValue = rowCount;
}
