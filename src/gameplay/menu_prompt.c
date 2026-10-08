#include "gameplay/item_menu.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "common.h"

#include "gameplay/attachment_state.h"
#include "attachments.h"
#include "hud_sprites.h"
#include "gameplay/inventory.h"
#include "inventory.h"
#include "item_menu.h"
#include "item_use.h"
#include "gameplay/items.h"
#include "items.h"
#include "menu.h"

#include "main/display.h"
#include "main/fs.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/sound.h"
#include "main/text.h"
#include "main/ui.h"
#include "main/wipsys.h"

/// Inventory domains and P.E. menu encoding used by the label and row filters.
enum {
    ITEM_MENU_ARMOR_ITEM_FIRST             = 0x60,
    ITEM_MENU_ARMOR_ITEM_COUNT             = 0x20,
    ITEM_MENU_WEAPON_ITEM_COUNT            = 0x20,
    ITEM_MENU_PARASITE_ENERGY_ITEM_FIRST   = 0x0F,
    ITEM_MENU_PARASITE_ENERGY_ITEM_COUNT   = ATTACHMENT_SPELL_COUNT * ATTACHMENT_AREA_LEVEL_COUNT,
    ITEM_MENU_PACKED_PARASITE_ENERGY_FIRST = 0x300,
    ITEM_MENU_PACKED_PARASITE_ENERGY_LIMIT = 0x340,
    ITEM_MENU_TONFA_BATON_ITEM             = 0x92,
    ITEM_MENU_LIST_ROW_HEIGHT              = 15,
    ITEM_MENU_WEAPON_CHOICE_VISIBLE_ROWS   = 4,
    ITEM_MENU_REORDER_VISIBLE_ROWS         = 9,
    ITEM_MENU_LEVEL_MARK_CLUT              = 0x3C02,
    ITEM_MENU_FIRE_EARTH_CAPTION_CLUT      = 0x3C85,
    ITEM_MENU_WIND_WATER_CAPTION_CLUT      = 0x3C86,
    ITEM_MENU_ELEMENT_EARTH                = 3,
    ITEM_MENU_FIRE_CAPTION_WIDTH           = 24,
    ITEM_MENU_OTHER_ELEMENT_CAPTION_WIDTH  = 32,
    ITEM_MENU_METER_FRAME_CLUT             = 0x3C0B,
    ITEM_MENU_STAT_TEXT_COLOR              = 0x606060,
    ITEM_MENU_STAT_METER_COLOR             = 0x01741F
};

/// Sets a menu-stat request's style, leaving its pixel coordinates intact.
///
/// request is a side-effect-free TextDrawReq lvalue, evaluated five times;
/// panel is a readable UiPanel pointer. Other arguments are evaluated once.
/// Use as a standalone block statement; do not attach an else to the call.
#define ITEM_MENU_INIT_STATS_TEXT(request, panel, rgb, glyphs, align, mode) \
    {                                                                       \
        (request).otIndex    = (panel)->otIndex.signedValue + 1;            \
        (request).colorRgb   = (rgb);                                       \
        (request).glyphTable = (glyphs);                                    \
        (request).alignment  = (align);                                     \
        (request).drawMode   = (mode);                                      \
    }

/// Sets result to 1 when an inventory item is selected as equipment.
///
/// result must start at 0 and player must point to the live PlayerStatus.
/// itemId and player are evaluated repeatedly and must have no side effects.
/// Reads load selections even at zero quantity; none of them is modified.
/// result must be a side-effect-free writable s32 lvalue.
/// Use as a standalone block statement; do not attach an else to the call.
#define EQUIPMENT_CHECK_ACTIVE_ITEM(result, player, itemId)                                                                                                            \
    {                                                                                                                                                                  \
        if ((((u32)((itemId) - EQUIPMENT_WEAPON_ITEM_FIRST) < ITEM_MENU_WEAPON_ITEM_COUNT) && ((player)->weapon == ((itemId) - (EQUIPMENT_WEAPON_ITEM_FIRST - 1)))) || \
            (((u32)((itemId) - ITEM_MENU_ARMOR_ITEM_FIRST) < ITEM_MENU_ARMOR_ITEM_COUNT) && ((player)->armor == ((itemId) - (ITEM_MENU_ARMOR_ITEM_FIRST - 1)))) ||     \
            (((u32)((itemId) - INVENTORY_CONSUMABLE_ITEM_FIRST) < INVENTORY_CONSUMABLE_ITEM_COUNT) && ((player)->weapon != PLAYER_STATUS_EQUIPMENT_NONE) &&            \
             ((equipmentGetWeaponLoad((player)->weapon + (EQUIPMENT_WEAPON_ITEM_FIRST - 1))->primaryItemId == (itemId)) ||                                             \
              (equipmentGetWeaponLoad((player)->weapon + (EQUIPMENT_WEAPON_ITEM_FIRST - 1))->secondaryItemId == (itemId))))) {                                         \
            (result) = 1;                                                                                                                                              \
        }                                                                                                                                                              \
    }

static void _itemMenuDrawRowSprite(const UiList* list, const UiObject* object, u32 packedUvSize);

static inline void _itemMenuDrawItemLabelAt(const UiObject* object, s32 x, s32 y, s32 colorRgb, s32 itemId, s32 attachmentState);

static inline void _itemMenuDrawItemRowLabel(const UiList* list, const UiObject* object, s32 itemId, s32 attachmentState);

static inline InventoryItemRow* _inventoryFindNthReorderableRow(const InventoryItemRange* range, s32 choiceIndex);

static __inline__ void _itemMenuSetReorderableRows(UiList* list);

static inline void _itemMenuDrawPlayerSummaryContents(const UiPanel* panel);

static inline void _itemMenuAcceptPanelChild(UiObject* object, UiObject* childObject, s32 beginDestinationSelection);

static void _itemMenuInventoryListTask(Task* task);

static inline void _itemMenuSetChildPosition(UiObject* child, s32 x, s32 y);

static inline void _itemMenuSetWeaponRows(UiList* list);

static inline void _itemMenuClampArmorSelection(UiList* list, s32 visibleEndIndex);

static inline s32 _equipmentIsActiveItem(s32 itemId);

char Gp_StrEmpty[]     = "";
char Gp_StrReleasePe[] = "Release Parasite Energy.";

/// Draws a borrowed text/P.E. payload under `itemMenuDrawTaskPrompt`'s contract.
///
/// objectArg and payloadArg must be side-effect-free: both are evaluated repeatedly.
/// Arguments must not use the helper-local names secondLine or textColorRgb.
/// Captures no caller locals. Use as a standalone block statement with no else.
/// Undefined after the command dispatcher.
#define ITEM_MENU_DRAW_DISPATCH_PROMPT(objectArg, payloadArg)                                                                                                                                                                                                                              \
    {                                                                                                                                                                                                                                                                                      \
        enum { ITEM_MENU_PROMPT_INLINE_VALUE_MAX  = 0xFFFF,                                                                                                                                                                                                                                \
               ITEM_MENU_PROMPT_ABILITY_ID_COUNT  = 0x100,                                                                                                                                                                                                                                 \
               ITEM_MENU_PROMPT_ROW_HEIGHT_PIXELS = 15,                                                                                                                                                                                                                                    \
               ITEM_MENU_PROMPT_LEFT_INSET_PIXELS = 2 };                                                                                                                                                                                                                                   \
        const u8* secondLine;                                                                                                                                                                                                                                                              \
        u32       textColorRgb;                                                                                                                                                                                                                                                            \
                                                                                                                                                                                                                                                                                           \
        if ((payloadArg).value != 0) {                                                                                                                                                                                                                                                     \
            if ((payloadArg).unsignedValue > ITEM_MENU_PROMPT_INLINE_VALUE_MAX) {                                                                                                                                                                                                          \
                textColorRgb = uiGetTextColor((objectArg), USER_INTERFACE_TEXT_COLOR_NORMAL);                                                                                                                                                                                              \
                textDrawUiLine((objectArg), (objectArg)->panel.contentLeft.signedValue + ITEM_MENU_PROMPT_LEFT_INSET_PIXELS, (objectArg)->panel.contentTop.signedValue + ITEM_MENU_PROMPT_ROW_HEIGHT_PIXELS, (payloadArg).pointer, textColorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT); \
                secondLine = textSkipLines((payloadArg).pointer, 1);                                                                                                                                                                                                                       \
                textDrawUiLine((objectArg), (objectArg)->panel.contentLeft.signedValue + ITEM_MENU_PROMPT_LEFT_INSET_PIXELS, (objectArg)->panel.contentTop.signedValue + 2 * ITEM_MENU_PROMPT_ROW_HEIGHT_PIXELS, secondLine, textColorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);       \
            } else if ((u32)((payloadArg).value - ITEM_TEXT_PACKED_ID_FIRST) < (u32)ITEM_MENU_PROMPT_ABILITY_ID_COUNT) {                                                                                                                                                                   \
                itemMenuDrawAbilityDescription((objectArg), (payloadArg).value);                                                                                                                                                                                                           \
            }                                                                                                                                                                                                                                                                              \
        }                                                                                                                                                                                                                                                                                  \
    }

void itemMenuDispatchCommand(UiObject* object, Task* task)
{
    enum {
        ITEM_MENU_COMMAND_MAIN                     = 1,
        ITEM_MENU_COMMAND_INVENTORY_COUNT          = 5,
        ITEM_MENU_COMMAND_USE_ATTACH               = 6,
        ITEM_MENU_COMMAND_PARASITE_ENERGY          = 12,
        ITEM_MENU_COMMAND_WEAPON_CHOICE            = 20,
        ITEM_MENU_COMMAND_ATTACH_NOTICE            = 25,
        ITEM_MENU_COMMAND_OPTIONS                  = 36,
        ITEM_MENU_COMMAND_MAP                      = 0x100,
        ITEM_MENU_COMMAND_RETURN_FROM_MAP          = 0x101,
        ITEM_MENU_COMMAND_OPEN_DELAY_TICKS         = 8,
        ITEM_MENU_CAPTION_BOTTOM_Y                 = 104,
        ITEM_MENU_PARASITE_ENERGY_CAPTION_BOTTOM_Y = 76,
    };
    TaskSpawnArg        promptPayload;
    s32                 commandId;
    s32                 textRows;
    const UiObjectDesc* countDescriptor;

    promptPayload = task->spawnArg1;
    ITEM_MENU_DRAW_DISPATCH_PROMPT(object, promptPayload);

    // Keep drawing the caption while the previous panel finishes hiding.
    task->killCountdown--;
    if (task->killCountdown > 0) {
        return;
    }

    switch (object->resultValue) {
        case ITEM_MENU_COMMAND_INVENTORY_COUNT:
            countDescriptor = D_8010EAB4;
            uiSpawnObject(countDescriptor + ITEM_MENU_COMMAND_INVENTORY_COUNT, 0, USER_INTERFACE_PANEL_ACTIVE, ITEM_MENU_COMMAND_OPEN_DELAY_TICKS, object);
            uiSetPromptText(Gp_StrEmpty, 0, 0);
            uiStartPanelOpening(&(object)->panel, object->owner);
            break;
        case ITEM_MENU_COMMAND_WEAPON_CHOICE:
        case ITEM_MENU_COMMAND_ATTACH_NOTICE:
            uiSpawnObject(D_8010EAB4 + object->resultValue, 0, USER_INTERFACE_PANEL_ACTIVE, ITEM_MENU_COMMAND_OPEN_DELAY_TICKS, object);
            break;
        case ITEM_MENU_COMMAND_MAP:
            D_80114D88 = 1;
            uiSpawnObject(&D_8010F140, 0, USER_INTERFACE_PANEL_ACTIVE, ITEM_MENU_COMMAND_OPEN_DELAY_TICKS, object);
            break;
        case ITEM_MENU_COMMAND_RETURN_FROM_MAP:
            displaySetTaskDrawMode(DISPLAY_TASK_DRAW_ROOM);
            uiSpawnObject(&D_8010EAB4[ITEM_MENU_COMMAND_MAIN], 0, USER_INTERFACE_PANEL_ACTIVE, ITEM_MENU_COMMAND_OPEN_DELAY_TICKS, object);
            uiSetPromptText(Gp_StrEmpty, 0, 0);
            uiStartPanelOpening(&(object)->panel, object->owner);
            break;
        case ITEM_MENU_COMMAND_USE_ATTACH:
        case ITEM_MENU_COMMAND_PARASITE_ENERGY:
            displaySetTaskDrawMode(DISPLAY_TASK_DRAW_ROOM);
            uiSpawnObject(D_8010EAB4 + object->resultValue, 0, USER_INTERFACE_PANEL_INACTIVE, ITEM_MENU_COMMAND_OPEN_DELAY_TICKS, object);
            uiSetPromptText(Gp_StrEmpty, 0, 0);
            uiStartPanelOpening(&(object)->panel, object->owner);
            break;
        case ITEM_MENU_COMMAND_OPTIONS:
        default:
            displaySetTaskDrawMode(DISPLAY_TASK_DRAW_ROOM);
            uiSpawnObject(D_8010EAB4 + object->resultValue, 0, USER_INTERFACE_PANEL_ACTIVE, ITEM_MENU_COMMAND_OPEN_DELAY_TICKS, object);
            uiSetPromptText(Gp_StrEmpty, 0, 0);
            uiStartPanelOpening(&(object)->panel, object->owner);
            break;
    }

    // Return to caption updates and anchor its new height above the lower edge.
    task->state--;

    commandId = object->resultValue;
    if ((commandId == ITEM_MENU_COMMAND_MAIN) || (commandId == ITEM_MENU_COMMAND_RETURN_FROM_MAP)) {
        textRows = 1;
    } else if (commandId != ITEM_MENU_COMMAND_PARASITE_ENERGY) {
        textRows = 2;
    } else {
        uiSetPanelContentSize(&(object)->panel, 0, uiGetTextRowsHeight(2) + 1);
        object->panel.bounds.unsignedRect.y = ITEM_MENU_PARASITE_ENERGY_CAPTION_BOTTOM_Y - object->panel.bounds.unsignedRect.h;
        return;
    }
    uiSetPanelContentSize(&(object)->panel, 0, uiGetTextRowsHeight(textRows) + 1);
    object->panel.bounds.unsignedRect.y = ITEM_MENU_CAPTION_BOTTOM_Y - object->panel.bounds.unsignedRect.h;
}

#undef ITEM_MENU_DRAW_DISPATCH_PROMPT

void itemMenuDrawItemIcon(const UiObject* object, s32 x, s32 y, s32 itemId, s32 flags)
{
    enum {
        ITEM_MENU_ICON_CATEGORY_PARASITE_ENERGY = -1,
        ITEM_MENU_ICON_CATEGORY_WEAPON          = 0,
        ITEM_MENU_ICON_CATEGORY_AMMUNITION      = 1,
        ITEM_MENU_ICON_CATEGORY_UNIDENTIFIED    = 2,
        ITEM_MENU_ICON_CATEGORY_ARMOR           = 3,
        ITEM_MENU_ICON_CATEGORY_MEDICINE        = 4,
        ITEM_MENU_ICON_CATEGORY_OTHER_ITEM      = 6,
        ITEM_MENU_ICON_CATEGORY_KEY_ITEM        = 7,
        ITEM_MENU_ICON_CATEGORY_EMPTY           = 8,
        ITEM_MENU_ICON_PALETTE_DIRECT           = -1,
        ITEM_MENU_ICON_PALETTE_MEDICINE         = 1,
        ITEM_MENU_ICON_PALETTE_FALLBACK         = 2,
        ITEM_MENU_ICON_PALETTE_9MM              = 3,
        ITEM_MENU_ICON_PALETTE_44_MAGNUM        = 4,
        ITEM_MENU_ICON_PALETTE_12_GAUGE         = 5,
        ITEM_MENU_ICON_PALETTE_556MM            = 6,
        ITEM_MENU_ICON_PALETTE_40MM             = 7,
        ITEM_MENU_ICON_PALETTE_BATTERY          = 8,
        ITEM_MENU_ICON_PALETTE_OTHER_ITEM       = 9,
        ITEM_MENU_SMG_CLIP_HOLDER_ITEM          = 0x09,
        ITEM_MENU_RIFLE_CLIP_HOLDER_ITEM        = 0x0A,
        ITEM_MENU_SNAIL_MAGAZINE_ITEM           = 0x0C,
        ITEM_MENU_M4_ATTACHMENT_ITEM_FIRST      = 0x42,
        ITEM_MENU_M4_ATTACHMENT_ITEM_LIMIT      = 0x47
    };
    POLY_FT4* iconQuad;
    TILE*     highlightTile;
    s32       paletteIndex;
    s32       categoryIndex;
    s32       textureU;
    s32       textureV;
    s32       paletteClut;
    s32       forceIdentified;
    s32       enlarge;
    s32       dimmed;
    s32       highlight;
    s32       abilityIndex;
    s32       firstConsumableItemId;
    s32       textureOffset;

    paletteClut     = 0;
    textureV        = 0;
    textureU        = 0;
    forceIdentified = flags & ITEM_MENU_ICON_FORCE_IDENTIFIED;
    enlarge         = (flags >> 1) & (ITEM_MENU_ICON_ENLARGED >> 1);
    dimmed          = (flags >> 2) & (ITEM_MENU_ICON_DIMMED >> 2);
    highlight       = (flags >> 3) & (ITEM_MENU_ICON_HIGHLIGHTED >> 3);

    // P.E. icons use their own atlas cells; ordinary items share category shapes and palettes.
    if (itemId >= ITEM_MENU_PACKED_PARASITE_ENERGY_FIRST) {
        paletteIndex  = ITEM_MENU_ICON_PALETTE_DIRECT;
        categoryIndex = ITEM_MENU_ICON_CATEGORY_PARASITE_ENERGY;
        if (itemId < ITEM_MENU_PACKED_PARASITE_ENERGY_LIMIT) {
            textureU    = ((itemId & 0xC) << 2) + 0xB0;
            textureV    = (itemId & 0xF0) + 0x80;
            paletteClut = getClut((itemId & 0xF0) + 0x40, 0xF0);
        } else {
            textureU    = 0xE0;
            textureV    = 0x40;
            paletteClut = 0x3C8B;
        }
    } else if (itemId == INVENTORY_ITEM_NONE) {
        paletteIndex  = ITEM_MENU_ICON_PALETTE_556MM;
        categoryIndex = ITEM_MENU_ICON_CATEGORY_EMPTY;
    } else if (forceIdentified == 0 && itemIsIdentified(itemId) == 0) {
        paletteIndex  = ITEM_MENU_ICON_PALETTE_FALLBACK;
        categoryIndex = ITEM_MENU_ICON_CATEGORY_UNIDENTIFIED;
    } else if (itemId < ITEM_MENU_ARMOR_ITEM_FIRST) {
        if ((u32)(itemId - ITEM_MENU_PARASITE_ENERGY_ITEM_FIRST) < ITEM_MENU_PARASITE_ENERGY_ITEM_COUNT) {
            abilityIndex  = (itemId - ITEM_MENU_PARASITE_ENERGY_ITEM_FIRST) / ATTACHMENT_AREA_LEVEL_COUNT;
            paletteIndex  = ITEM_MENU_ICON_PALETTE_DIRECT;
            categoryIndex = ITEM_MENU_ICON_CATEGORY_PARASITE_ENERGY;
            textureU      = (abilityIndex % 3) * 16 + 0xB0;
            textureV      = (abilityIndex / 3) * 16 + 0x80;
            paletteClut   = getClut((abilityIndex / 3) * 16 + 0x40, 0xF0);
        } else if ((Gp_ItemDescs[itemId].classification & ITEM_SUBTYPE_MASK) == ITEM_SUBTYPE_MEDICINE) {
            paletteIndex  = ITEM_MENU_ICON_PALETTE_MEDICINE;
            categoryIndex = ITEM_MENU_ICON_CATEGORY_MEDICINE;
        } else {
            if (itemId < ITEM_MENU_M4_ATTACHMENT_ITEM_LIMIT) {
                if (itemId < ITEM_MENU_M4_ATTACHMENT_ITEM_FIRST) {
                    switch (itemId) {
                        case ITEM_MENU_SMG_CLIP_HOLDER_ITEM:
                        case ITEM_MENU_SNAIL_MAGAZINE_ITEM:
                            paletteIndex = ITEM_MENU_ICON_PALETTE_9MM;
                            break;
                        case ITEM_MENU_RIFLE_CLIP_HOLDER_ITEM:
                            paletteIndex = ITEM_MENU_ICON_PALETTE_556MM;
                            break;
                        default:
                            paletteIndex = ITEM_MENU_ICON_PALETTE_OTHER_ITEM;
                            break;
                    }
                } else {
                    paletteIndex = ITEM_MENU_ICON_PALETTE_556MM;
                }
            } else {
                paletteIndex = ITEM_MENU_ICON_PALETTE_OTHER_ITEM;
            }
            categoryIndex = ITEM_MENU_ICON_CATEGORY_OTHER_ITEM;
        }
    } else if (itemId < EQUIPMENT_WEAPON_ITEM_FIRST) {
        paletteIndex  = ITEM_MENU_ICON_PALETTE_OTHER_ITEM;
        categoryIndex = ITEM_MENU_ICON_CATEGORY_ARMOR;
    } else if (itemId < INVENTORY_CONSUMABLE_ITEM_FIRST) {
        firstConsumableItemId = Gp_RelatedQty0.rows[itemId - EQUIPMENT_WEAPON_ITEM_FIRST].acceptedItemIds[0];
        if (firstConsumableItemId == INVENTORY_ITEM_NONE) {
            paletteIndex = ITEM_MENU_ICON_PALETTE_FALLBACK;
        } else {
            switch (Gp_ItemDescs[firstConsumableItemId].classification & ITEM_SUBTYPE_MASK) {
                case ITEM_AMMO_9MM:
                    paletteIndex = ITEM_MENU_ICON_PALETTE_9MM;
                    break;
                case ITEM_AMMO_44_MAGNUM:
                    paletteIndex = ITEM_MENU_ICON_PALETTE_44_MAGNUM;
                    break;
                case ITEM_AMMO_40MM:
                    paletteIndex = ITEM_MENU_ICON_PALETTE_40MM;
                    break;
                case ITEM_AMMO_12_GAUGE:
                    paletteIndex = ITEM_MENU_ICON_PALETTE_12_GAUGE;
                    break;
                case ITEM_AMMO_556MM:
                    paletteIndex = ITEM_MENU_ICON_PALETTE_556MM;
                    break;
                case ITEM_AMMO_BATTERY:
                    paletteIndex = ITEM_MENU_ICON_PALETTE_BATTERY;
                    break;
                default:
                    paletteIndex = ITEM_MENU_ICON_PALETTE_FALLBACK;
                    break;
            }
        }
        categoryIndex = ITEM_MENU_ICON_CATEGORY_WEAPON;
    } else if (itemId < INVENTORY_CONSUMABLE_ITEM_FIRST + INVENTORY_CONSUMABLE_ITEM_COUNT) {
        switch (Gp_ItemDescs[itemId].classification & ITEM_SUBTYPE_MASK) {
            case ITEM_AMMO_9MM:
                paletteIndex = ITEM_MENU_ICON_PALETTE_9MM;
                break;
            case ITEM_AMMO_44_MAGNUM:
                paletteIndex = ITEM_MENU_ICON_PALETTE_44_MAGNUM;
                break;
            case ITEM_AMMO_40MM:
                paletteIndex = ITEM_MENU_ICON_PALETTE_40MM;
                break;
            case ITEM_AMMO_12_GAUGE:
                paletteIndex = ITEM_MENU_ICON_PALETTE_12_GAUGE;
                break;
            case ITEM_AMMO_556MM:
                paletteIndex = ITEM_MENU_ICON_PALETTE_556MM;
                break;
            default:
                paletteIndex = ITEM_MENU_ICON_PALETTE_FALLBACK;
                break;
        }
        categoryIndex = ITEM_MENU_ICON_CATEGORY_AMMUNITION;
    } else {
        paletteIndex  = ITEM_MENU_ICON_PALETTE_MEDICINE;
        categoryIndex = ITEM_MENU_ICON_CATEGORY_KEY_ITEM;
    }

    // Queue the icon first so an optional highlight tile sits behind it in the OT.
    iconQuad       = gGpuPrimCursor;
    gGpuPrimCursor = iconQuad + 1;
    iconQuad->x2 = iconQuad->x0 = object->panel.contentOriginX.unsignedValue + x;
    iconQuad->x1 = iconQuad->x3 = iconQuad->x0 + 0xE;
    iconQuad->y1 = iconQuad->y0 = object->panel.contentOriginY.unsignedValue + y - 0xE;
    iconQuad->y2 = iconQuad->y3 = iconQuad->y0 + 0xE;
    if (enlarge != 0) {
        iconQuad->x2 = iconQuad->x0 = iconQuad->x0 - 2;
        iconQuad->x3 = iconQuad->x1 = iconQuad->x1 + 2;
        iconQuad->y1 = iconQuad->y0 = iconQuad->y0 - 2;
        iconQuad->y3 = iconQuad->y2 = iconQuad->y2 + 2;
    }
    if (categoryIndex >= 0) {
        textureOffset = (categoryIndex + 1) * 16;
        setUV4(iconQuad, -textureOffset, 0xF0, 0xE - textureOffset, 0xF0, -textureOffset, 0xFE, 0xE - textureOffset, 0xFE);
    } else {
        setUVWH(iconQuad, textureU + 1, textureV + 1, 0xE, 0xE);
    }
    if (paletteIndex >= 0) {
        iconQuad->clut = D_80096F88[paletteIndex];
    } else {
        iconQuad->clut = paletteClut;
    }
    iconQuad->tpage = 0x1E;
    if (dimmed == 0) {
        setlen(iconQuad, 9);
        setcode(iconQuad, 0x2D);
    } else {
        GPU_PRIMITIVE_COLOR_WORD(iconQuad, 0) = GPU_PACK_COLOR_WORD(0x40, 0x40, 0x40, 0);
        setlen(iconQuad, 9);
        setcode(iconQuad, 0x2C);
    }
    addPrim(gGpuCurrentOt + object->panel.otIndex.signedValue + 1, iconQuad);
    if (highlight != 0) {
        highlightTile                              = gGpuPrimCursor;
        highlightTile->x0                          = iconQuad->x0 - 1;
        highlightTile->y0                          = iconQuad->y0 - 1;
        highlightTile->w                           = iconQuad->x1 - iconQuad->x0 + 2;
        highlightTile->h                           = iconQuad->y2 - iconQuad->y0 + 2;
        gGpuPrimCursor                             = highlightTile + 1;
        GPU_PRIMITIVE_COLOR_WORD(highlightTile, 0) = GPU_PACK_COLOR_WORD(0xc0, 0xc0, 0xc0, 0);
        setlen(highlightTile, 3);
        setcode(highlightTile, 0x60);
        addPrim(gGpuCurrentOt + object->panel.otIndex.signedValue + 1, highlightTile);
    }
}

/// Draws a raw-texture sprite at the current list row, fifteen pixels above its baseline.
///
/// packedUvSize stores U, V, width and height in successive low-to-high bytes;
/// dimensions are pixels. Active rows use the active palette, all others the
/// inactive palette. Inputs are borrowed unchanged. Requires the menu texture
/// in VRAM and GPU space for one SPRT and a texture-page command at panel OT + 1.
static void _itemMenuDrawRowSprite(const UiList* list, const UiObject* object, u32 packedUvSize)
{
    enum {
        ITEM_MENU_ROW_SPRITE_ACTIVE_CLUT      = 0x3C09,
        ITEM_MENU_ROW_SPRITE_INACTIVE_CLUT    = 0x3C01,
        ITEM_MENU_ROW_SPRITE_RAW_TEXTURE_CODE = 0x65
    };
    SPRT* sprite;

    sprite         = gGpuPrimCursor;
    gGpuPrimCursor = sprite + 1;
    sprite->x0     = list->rowTextX.unsignedValue + object->panel.contentOriginX.unsignedValue;
    sprite->y0     = list->rowTextY.unsignedValue + object->panel.contentOriginY.unsignedValue - ITEM_MENU_LIST_ROW_HEIGHT;
    if (list->rowInputEnabled == USER_INTERFACE_LIST_ROW_ACTIVE) {
        sprite->clut = ITEM_MENU_ROW_SPRITE_ACTIVE_CLUT;
    } else {
        sprite->clut = ITEM_MENU_ROW_SPRITE_INACTIVE_CLUT;
    }
    sprite->v0 = packedUvSize >> 8;
    sprite->w  = (packedUvSize >> 16) & 0xFF;
    sprite->h  = packedUvSize >> 24;
    setlen(sprite, 4);
    sprite->u0 = packedUvSize;
    setcode(sprite, ITEM_MENU_ROW_SPRITE_RAW_TEXTURE_CODE);
    addPrim(gGpuCurrentOt + object->panel.otIndex.signedValue + 1, sprite);
    uiQueueTexturePage(object->panel.otIndex.signedValue + 1, 0);
}

/// Applies a child dialog's result to the main item-command panel.
///
/// DISMISS forwards the child's value as CONFIRM; CONFIRM resumes panel input
/// and starts closing the child; CANCEL forwards cancellation. Other results
/// leave both objects intact. The panel, child and child's owner must be live.
/// Closing detaches the child but does not immediately free it; the caller
/// saves its successor before this call when traversing the child ring.
static inline void _itemMenuHandleMainPanelChild(UiObject* mainPanel, UiObject* child, s32 childResult)
{
    switch (childResult) {
        case USER_INTERFACE_RESULT_DISMISS:
            mainPanel->resultValue = child->resultValue;
            mainPanel->result      = USER_INTERFACE_RESULT_CONFIRM;
            break;
        case USER_INTERFACE_RESULT_CONFIRM:
            mainPanel->panel.control.word = USER_INTERFACE_PANEL_ACTIVE;
            uiStartTreeClosing(child, child->owner);
            break;
        case USER_INTERFACE_RESULT_CANCEL:
            mainPanel->result = childResult;
            break;
    }
}

void itemMenuMainPanelTask(Task* task)
{
    enum { ITEM_MENU_PANEL_INITIAL             = 0,
           ITEM_MENU_PLAYER_SUMMARY_DESCRIPTOR = 3,
           ITEM_MENU_SUMMARY_OPEN_DELAY_TICKS  = 4 };
    UiObject* object;
    UiList*   commandList;
    Task*     childTask;
    Task*     nextChild;
    Task*     firstChild;
    UiObject* childObject;
    s32       childResult;

    commandList = &D_8010E820;
    object      = task->spawnArg2.pointer;
    if (task->state == ITEM_MENU_PANEL_INITIAL) {
        uiSpawnObject(&D_8010EAB4[ITEM_MENU_PLAYER_SUMMARY_DESCRIPTOR], 0, USER_INTERFACE_PANEL_INACTIVE, ITEM_MENU_SUMMARY_OPEN_DELAY_TICKS, object);
        uiFitPanelToList(commandList, &(object)->panel);
        task->state = task->state + 1;
    } else {
        object->result = USER_INTERFACE_RESULT_NONE;
        uiUpdateList(commandList, &object->panel);
        if (object->panel.control.word == USER_INTERFACE_PANEL_ACTIVE) {
            if (object->result == USER_INTERFACE_RESULT_NONE) {
                if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskCancel) != 0) {
                    object->result = USER_INTERFACE_RESULT_CANCEL;
                } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskMenu) != 0) {
                    object->result = USER_INTERFACE_RESULT_CANCEL;
                }
            }
        }
        // Closing a child detaches it, so save its successor before handling the result.
        firstChild = task->firstChild;
        if (firstChild != NULL) {
            childTask = firstChild;
            do {
                childObject = childTask->spawnArg2.pointer;
                childResult = childObject->result;
                nextChild   = childTask->nextSibling;
                _itemMenuHandleMainPanelChild(object, childObject, childResult);
                childTask = nextChild;
            } while (childTask != task->firstChild);
        }
    }
}

void itemMenuDrawMeter(const UiPanel* panel, s32 left, s32 right, s32 centerY, s32 maximum, s32 value, u32 colorRgb)
{
    TILE*     fillTile;
    SPRT*     capSprite;
    POLY_FT4* frameQuad;
    s32       width;
    s32       interiorWidth;
    s32       fillWidth;
    s32       rightCapX;
    s32       frameClut;

    if (left < right) {
        width         = right - left;
        interiorWidth = width - 2;
        fillWidth     = (interiorWidth * value) / maximum;
        left          = left + panel->contentOriginX.signedValue;
        centerY       = centerY + panel->contentOriginY.signedValue;
        if (interiorWidth < fillWidth) {
            fillWidth = interiorWidth;
        }
        // Scale and clip the fill before drawing the two caps and stretched frame.
        if (fillWidth > 0) {
            fillTile                              = gGpuPrimCursor;
            gGpuPrimCursor                        = fillTile + 1;
            fillTile->x0                          = left + 1;
            fillTile->y0                          = centerY - 1;
            fillTile->w                           = fillWidth;
            fillTile->h                           = 2;
            GPU_PRIMITIVE_COLOR_WORD(fillTile, 0) = colorRgb;
            setlen(fillTile, 3);
            setcode(fillTile, 0x60);
            addPrim(gGpuCurrentOt + panel->otIndex.signedValue + 1, fillTile);
        }
        centerY   = centerY - 4;
        frameClut = ITEM_MENU_METER_FRAME_CLUT;

        capSprite       = gGpuPrimCursor;
        gGpuPrimCursor  = capSprite + 1;
        capSprite->x0   = left;
        capSprite->y0   = centerY;
        capSprite->u0   = 0x98;
        capSprite->v0   = 0x68;
        capSprite->clut = frameClut;
        setlen(capSprite, 3);
        setcode(capSprite, 0x75);
        addPrim(gGpuCurrentOt + panel->otIndex.signedValue + 1, capSprite);

        capSprite       = gGpuPrimCursor;
        gGpuPrimCursor  = capSprite + 1;
        rightCapX       = (left + width) - 8;
        capSprite->x0   = rightCapX;
        capSprite->y0   = centerY;
        capSprite->u0   = 0xA8;
        capSprite->v0   = 0x68;
        capSprite->clut = frameClut;
        setlen(capSprite, 3);
        setcode(capSprite, 0x75);
        addPrim(gGpuCurrentOt + panel->otIndex.signedValue + 1, capSprite);

        frameQuad        = gGpuPrimCursor;
        gGpuPrimCursor   = frameQuad + 1;
        frameQuad->x2    = left + 8;
        frameQuad->x0    = left + 8;
        frameQuad->y3    = centerY + 8;
        frameQuad->y2    = centerY + 8;
        frameQuad->u0    = 0xA0;
        frameQuad->u2    = 0xA0;
        frameQuad->v2    = 0x70;
        frameQuad->v3    = 0x70;
        frameQuad->tpage = 0x3E;
        frameQuad->x3    = rightCapX;
        frameQuad->x1    = rightCapX;
        frameQuad->y1    = centerY;
        frameQuad->y0    = centerY;
        frameQuad->v0    = 0x68;
        frameQuad->u1    = 0xA8;
        frameQuad->v1    = 0x68;
        frameQuad->u3    = 0xA8;
        frameQuad->clut  = frameClut;
        setlen(frameQuad, 9);
        setcode(frameQuad, 0x2D);
        addPrim(gGpuCurrentOt + panel->otIndex.signedValue + 1, frameQuad);
    }
}

void itemMenuDrawPlayerStats(const UiPanel* panel, s32 topOffset)
{
    u8            numberText[0x20];
    TextDrawReq   hpValueText;
    TextDrawReq   hpSeparatorText;
    TextDrawReq   hpMaximumText;
    TextDrawReq   mpValueText;
    TextDrawReq   mpSeparatorText;
    TextDrawReq   mpMaximumText;
    TextDrawReq   experienceText;
    TextDrawReq   textReq;
    TextDrawReq   mpLabelText;
    TextDrawReq   experienceLabelText;
    TextDrawReq   bpLabelText;
    PlayerStatus* player;
    s32           contentLeft;
    s32           valueX;
    s32           firstRowY;
    s32           rowY;
    s32           meterBaseX;
    s32           textColorRgb;
    s32           maximum;

    player = &gPlayerStatus;
    // Keep the panel edge for the BP column before indenting the numeric rows.
    valueX      = panel->contentLeft.signedValue;
    topOffset   = topOffset + 8;
    contentLeft = valueX;
    valueX     += 6;
    firstRowY   = panel->contentTop.signedValue + topOffset;
    // Menu healing advances the displayed values upward by one per draw.
    if (Gp_HpMpWork.hp < player->hp) {
        Gp_HpMpWork.hp = Gp_HpMpWork.hp + 1;
    }
    if (Gp_HpMpWork.mp < player->mp) {
        Gp_HpMpWork.mp = Gp_HpMpWork.mp + 1;
    }
    textColorRgb = ITEM_MENU_STAT_TEXT_COLOR;

    hpValueText.x = panel->contentOriginX.unsignedValue + 0x17 + valueX;
    hpValueText.y = panel->contentOriginY.unsignedValue + firstRowY;
    ITEM_MENU_INIT_STATS_TEXT(hpValueText, panel, textColorRgb, TEXT_GLYPH_TABLE_MEDIUM, TEXT_ALIGNMENT_LEFT, TEXT_DRAW_TRANSLUCENT_OUTLINED);
    textDrawString(&hpValueText, textItoaUnsigned(numberText, Gp_HpMpWork.hp));

    hpSeparatorText.x = panel->contentOriginX.unsignedValue + 0x32 + valueX;
    hpSeparatorText.y = panel->contentOriginY.unsignedValue + firstRowY;
    ITEM_MENU_INIT_STATS_TEXT(hpSeparatorText, panel, textColorRgb, TEXT_GLYPH_TABLE_MEDIUM, TEXT_ALIGNMENT_CENTER, TEXT_DRAW_TRANSLUCENT_OUTLINED);
    textDrawString(&hpSeparatorText, Gp_StrSlash);

    hpMaximumText.x = panel->contentOriginX.unsignedValue + 0x37 + valueX;
    hpMaximumText.y = panel->contentOriginY.unsignedValue + firstRowY;
    ITEM_MENU_INIT_STATS_TEXT(hpMaximumText, panel, textColorRgb, TEXT_GLYPH_TABLE_MEDIUM, TEXT_ALIGNMENT_LEFT, TEXT_DRAW_TRANSLUCENT_OUTLINED);
    textDrawString(&hpMaximumText, textItoaUnsigned(numberText, player->hpMax));

    maximum    = player->hpMax;
    meterBaseX = contentLeft + 7;
    itemMenuDrawMeter(panel, valueX, meterBaseX + ((maximum - 1) * 0x25) / 64, firstRowY + 5, maximum, Gp_HpMpWork.hp, ITEM_MENU_STAT_METER_COLOR);

    rowY          = firstRowY + 0x12;
    mpValueText.x = panel->contentOriginX.unsignedValue + 0x17 + valueX;
    mpValueText.y = panel->contentOriginY.unsignedValue + rowY;
    ITEM_MENU_INIT_STATS_TEXT(mpValueText, panel, textColorRgb, TEXT_GLYPH_TABLE_MEDIUM, TEXT_ALIGNMENT_LEFT, TEXT_DRAW_TRANSLUCENT_OUTLINED);
    textDrawString(&mpValueText, textItoaUnsigned(numberText, Gp_HpMpWork.mp));

    mpSeparatorText.x = panel->contentOriginX.unsignedValue + 0x32 + valueX;
    mpSeparatorText.y = panel->contentOriginY.unsignedValue + rowY;
    ITEM_MENU_INIT_STATS_TEXT(mpSeparatorText, panel, textColorRgb, TEXT_GLYPH_TABLE_MEDIUM, TEXT_ALIGNMENT_CENTER, TEXT_DRAW_TRANSLUCENT_OUTLINED);
    textDrawString(&mpSeparatorText, Gp_StrSlash);

    mpMaximumText.x = panel->contentOriginX.unsignedValue + 0x37 + valueX;
    mpMaximumText.y = panel->contentOriginY.unsignedValue + rowY;
    ITEM_MENU_INIT_STATS_TEXT(mpMaximumText, panel, textColorRgb, TEXT_GLYPH_TABLE_MEDIUM, TEXT_ALIGNMENT_LEFT, TEXT_DRAW_TRANSLUCENT_OUTLINED);
    textDrawString(&mpMaximumText, textItoaUnsigned(numberText, player->mpMax));

    maximum = player->mpMax;
    itemMenuDrawMeter(panel, valueX, meterBaseX + ((maximum - 1) * 0x25) / 64, firstRowY + 0x17, maximum, Gp_HpMpWork.mp, ITEM_MENU_STAT_METER_COLOR);

    rowY             = firstRowY + 0x24;
    experienceText.x = panel->contentOriginX.unsignedValue + 0x17 + valueX;
    experienceText.y = panel->contentOriginY.unsignedValue + rowY;
    ITEM_MENU_INIT_STATS_TEXT(experienceText, panel, textColorRgb, TEXT_GLYPH_TABLE_MEDIUM, TEXT_ALIGNMENT_LEFT, TEXT_DRAW_TRANSLUCENT_OUTLINED);
    textDrawString(&experienceText, textItoaUnsigned(numberText, player->exp));

    textReq.x = panel->contentOriginX.unsignedValue + contentLeft + 0x72;
    textReq.y = panel->contentOriginY.unsignedValue + rowY;
    ITEM_MENU_INIT_STATS_TEXT(textReq, panel, textColorRgb, TEXT_GLYPH_TABLE_MEDIUM, TEXT_ALIGNMENT_LEFT, TEXT_DRAW_TRANSLUCENT_OUTLINED);
    textDrawString(&textReq, textItoaUnsigned(numberText, player->bp));

    textReq.x = panel->contentLeft.unsignedValue + (panel->contentOriginX.unsignedValue + 2);
    textReq.y = panel->contentOriginY.unsignedValue + (firstRowY - 2);
    ITEM_MENU_INIT_STATS_TEXT(textReq, panel, textColorRgb, TEXT_GLYPH_TABLE_SMALL, TEXT_ALIGNMENT_LEFT, TEXT_DRAW_OUTLINED);
    textDrawString(&textReq, Gp_StrHp);

    mpLabelText.x = panel->contentLeft.unsignedValue + (panel->contentOriginX.unsignedValue + 2);
    mpLabelText.y = panel->contentOriginY.unsignedValue + 0x10 + firstRowY;
    ITEM_MENU_INIT_STATS_TEXT(mpLabelText, panel, textColorRgb, TEXT_GLYPH_TABLE_SMALL, TEXT_ALIGNMENT_LEFT, TEXT_DRAW_OUTLINED);
    textDrawString(&mpLabelText, Gp_StrMp);

    experienceLabelText.x = panel->contentLeft.unsignedValue + (panel->contentOriginX.unsignedValue + 2);
    experienceLabelText.y = panel->contentOriginY.unsignedValue + 0x22 + firstRowY;
    ITEM_MENU_INIT_STATS_TEXT(experienceLabelText, panel, textColorRgb, TEXT_GLYPH_TABLE_SMALL, TEXT_ALIGNMENT_LEFT, TEXT_DRAW_OUTLINED);
    textDrawString(&experienceLabelText, Gp_StrExp);

    bpLabelText.x = panel->contentLeft.unsignedValue + (panel->contentOriginX.unsignedValue + 0x57);
    bpLabelText.y = panel->contentOriginY.unsignedValue + 0x22 + firstRowY;
    ITEM_MENU_INIT_STATS_TEXT(bpLabelText, panel, textColorRgb, TEXT_GLYPH_TABLE_SMALL, TEXT_ALIGNMENT_LEFT, TEXT_DRAW_OUTLINED);
    textDrawString(&bpLabelText, Gp_StrBp);
}

/// Draws the fixed decorations and live statistics in the player-summary panel.
///
/// Borrows a readable panel; origins and bounds are screen pixels narrowed
/// into packet halfwords. Requires menu textures, live player/HUD statistics
/// and writable primitive/OT storage through GPU completion. Decorations use
/// the panel's signed OT index plus one; the side image bypasses RGB modulation.
/// The panel is unchanged and no pointer to it is retained.
static inline void _itemMenuDrawPlayerSummaryContents(const UiPanel* panel)
{
    enum { ITEM_MENU_SUMMARY_HEADING_CLUT  = 0x3C02,
           ITEM_MENU_SUMMARY_IMAGE_CLUT    = 0x3C40,
           ITEM_MENU_SUMMARY_IMAGE_TPAGE   = 0x9E,
           ITEM_MENU_SUMMARY_SPRITE_CODE   = 0x64,
           ITEM_MENU_SUMMARY_RAW_QUAD_CODE = 0x2D,
           ITEM_MENU_SUMMARY_HEADING_WORDS = sizeof(SPRT) / sizeof(u32) - 1,
           ITEM_MENU_SUMMARY_IMAGE_WORDS   = sizeof(POLY_FT4) / sizeof(u32) - 1 };
    SPRT*     headingSprite;
    POLY_FT4* playerImageQuad;
    s32       headingColorRgb;
    s32       imageX;
    s32       imageRight;
    s32       imageTop;

    // Draw the fixed heading and side image around the live player statistics.
    headingColorRgb   = ITEM_MENU_STAT_TEXT_COLOR;
    headingSprite     = gGpuPrimCursor;
    gGpuPrimCursor    = headingSprite + 1;
    headingSprite->x0 = panel->contentOriginX.unsignedValue + panel->contentRight.unsignedValue - 0x72;
    {
        s32 headingTop;
        headingTop          = panel->bounds.unsignedRect.y;
        headingSprite->u0   = 0x38;
        headingSprite->v0   = 0x60;
        headingSprite->w    = 0x40;
        headingSprite->h    = 8;
        headingSprite->clut = ITEM_MENU_SUMMARY_HEADING_CLUT;
        setlen(headingSprite, ITEM_MENU_SUMMARY_HEADING_WORDS);
        GPU_PRIMITIVE_COLOR_WORD(headingSprite, 0) = headingColorRgb;
        setcode(headingSprite, ITEM_MENU_SUMMARY_SPRITE_CODE);
        headingSprite->y0 = headingTop + 3;
        addPrim(gGpuCurrentOt + panel->otIndex.signedValue + 1, headingSprite);
    }
    uiQueueTexturePage(panel->otIndex.signedValue + 1, GPU_BLEND_AVERAGE);

    playerImageQuad     = gGpuPrimCursor;
    imageX              = panel->bounds.unsignedRect.x + panel->bounds.unsignedRect.w;
    imageRight          = imageX - 1;
    imageX              = imageX - 0x32;
    gGpuPrimCursor      = playerImageQuad + 1;
    playerImageQuad->x0 = playerImageQuad->x2 = imageX;
    playerImageQuad->x1 = playerImageQuad->x3 = imageRight;
    imageTop                                  = panel->bounds.unsignedRect.y;
    playerImageQuad->y0 = playerImageQuad->y1 = imageTop + 2;
    playerImageQuad->y2 = playerImageQuad->y3 = imageTop + 0x40;
    setUVWH(playerImageQuad, 0, 0x80, 0x31, 0x3E);
    playerImageQuad->clut  = ITEM_MENU_SUMMARY_IMAGE_CLUT;
    playerImageQuad->tpage = ITEM_MENU_SUMMARY_IMAGE_TPAGE;
    setlen(playerImageQuad, ITEM_MENU_SUMMARY_IMAGE_WORDS);
    setcode(playerImageQuad, ITEM_MENU_SUMMARY_RAW_QUAD_CODE);
    addPrim(gGpuCurrentOt + panel->otIndex.signedValue + 1, playerImageQuad);
    uiDrawVerticalSeparator(panel, panel->contentTop.signedValue - 3, panel->contentBottom.signedValue + 2, panel->contentRight.signedValue - 0x32);
    uiDrawHorizontalSeparator(panel, panel->contentLeft.signedValue - 2, panel->contentRight.signedValue - 0x32, panel->contentTop.signedValue + 8);
    itemMenuDrawPlayerStats(panel, 0xB);
}

void itemMenuPlayerSummaryTask(Task* task)
{
    enum { ITEM_MENU_PANEL_INITIAL             = 0,
           ITEM_MENU_WEAPON_SUMMARY_DESCRIPTOR = 4 };
    UiObject*           object;
    const PlayerStatus* player;

    object = task->spawnArg2.pointer;
    if (task->state == ITEM_MENU_PANEL_INITIAL) {
        uiSpawnObject(&D_8010EAB4[ITEM_MENU_WEAPON_SUMMARY_DESCRIPTOR], 0, USER_INTERFACE_PANEL_INACTIVE, 0, object);
        player         = &gPlayerStatus;
        Gp_HpMpWork.hp = player->hp;
        Gp_HpMpWork.mp = player->mp;
        task->state    = task->state + 1;
    }
    _itemMenuDrawPlayerSummaryContents(&object->panel);
}

void itemMenuArmorSummaryTask(Task* task)
{
    enum { ITEM_MENU_ARMOR_ATTACHMENT_COLUMNS = 5,
           ITEM_MENU_ARMOR_EMPTY_SLOT_COLOR   = 0x102010 };

    u8                  bonusText[0x20];
    TextDrawReq         hpBonusReq;
    TextDrawReq         mpBonusReq;
    TextDrawReq         hpLabelReq;
    TextDrawReq         mpLabelReq;
    TextDrawReq         attachmentsLabelReq;
    s32                 contentLeft;
    s32                 columnWidth;
    UiObject*           object;
    const PlayerStatus* player;
    s32                 itemId;
    s32                 colorRgb;
    s32                 rowX;
    s32                 rowY;
    s32                 topY;
    s32                 attachmentIndex;
    const ArmorStats*   armorStats;

    object         = task->spawnArg2.pointer;
    player         = &gPlayerStatus;
    object->result = USER_INTERFACE_RESULT_NONE;
    uiDrawHorizontalSeparator(&(object)->panel, object->panel.contentLeft.signedValue, object->panel.contentRight.signedValue, object->panel.contentTop.signedValue + 0x11);

    contentLeft = object->panel.contentLeft.signedValue;
    rowX        = contentLeft + 2;
    itemId      = player->armor;
    topY        = object->panel.contentTop.signedValue;
    rowY        = topY + ITEM_MENU_LIST_ROW_HEIGHT;
    columnWidth = (object->panel.contentRight.signedValue - rowX) / 2;
    uiDrawTitle(&(object)->panel, Gp_StrArmor);

    // Convert the one-based equipment selector to the armour catalogue id.
    if (itemId > PLAYER_STATUS_EQUIPMENT_NONE) {
        itemId    += ITEM_MENU_ARMOR_ITEM_FIRST - 1;
        colorRgb   = ITEM_MENU_STAT_TEXT_COLOR;
        armorStats = &Gp_ModStatAttrs[itemId - ITEM_MENU_ARMOR_ITEM_FIRST];
        itemMenuDrawItemRow(object, rowX, rowY, itemId, colorRgb, 0);

        rowY         = topY + 0x1D;
        hpBonusReq.x = object->panel.contentOriginX.unsignedValue + 0x20 + rowX;
        hpBonusReq.y = object->panel.contentOriginY.unsignedValue + rowY;
        ITEM_MENU_INIT_STATS_TEXT(hpBonusReq, &object->panel, colorRgb, TEXT_GLYPH_TABLE_MEDIUM, TEXT_ALIGNMENT_LEFT, TEXT_DRAW_TRANSLUCENT_OUTLINED);
        textDrawString(&hpBonusReq, textItoaSignPrefixed(bonusText, armorStats->hpBonus));

        mpBonusReq.x = object->panel.contentOriginX.unsignedValue + (rowX + (columnWidth + 0x1E));
        mpBonusReq.y = object->panel.contentOriginY.unsignedValue + rowY;
        ITEM_MENU_INIT_STATS_TEXT(mpBonusReq, &object->panel, colorRgb, TEXT_GLYPH_TABLE_MEDIUM, TEXT_ALIGNMENT_LEFT, TEXT_DRAW_TRANSLUCENT_OUTLINED);
        textDrawString(&mpBonusReq, textItoaSignPrefixed(bonusText, armorStats->mpBonus));

        hpLabelReq.x = object->panel.contentOriginX.unsignedValue + 2 + rowX;
        hpLabelReq.y = object->panel.contentOriginY.unsignedValue + (rowY - 2);
        ITEM_MENU_INIT_STATS_TEXT(hpLabelReq, &object->panel, colorRgb, TEXT_GLYPH_TABLE_SMALL, TEXT_ALIGNMENT_LEFT, TEXT_DRAW_OUTLINED);
        textDrawString(&hpLabelReq, Gp_StrHp);

        mpLabelReq.x = object->panel.contentOriginX.unsignedValue + (rowX + columnWidth);
        mpLabelReq.y = object->panel.contentOriginY.unsignedValue + (rowY - 2);
        rowX         = contentLeft + 4;
        ITEM_MENU_INIT_STATS_TEXT(mpLabelReq, &object->panel, colorRgb, TEXT_GLYPH_TABLE_SMALL, TEXT_ALIGNMENT_LEFT, TEXT_DRAW_OUTLINED);
        textDrawString(&mpLabelReq, Gp_StrMp);

        rowY                  = topY + 0x3D;
        attachmentsLabelReq.x = object->panel.contentOriginX.unsignedValue + rowX;
        attachmentsLabelReq.y = object->panel.contentOriginY.unsignedValue + topY + 0x2C;
        ITEM_MENU_INIT_STATS_TEXT(attachmentsLabelReq, &object->panel, colorRgb, TEXT_GLYPH_TABLE_SMALL, TEXT_ALIGNMENT_LEFT, TEXT_DRAW_OUTLINED);
        textDrawString(&attachmentsLabelReq, Gp_StrAttachments);

        // Attachment slots are one-based in carried rows and displayed five per line.
        for (attachmentIndex = 0; attachmentIndex < equipmentGetArmorAttachmentSlotCount(itemId); attachmentIndex++) {
            const InventoryItemRange* carriedRange;
            const InventoryItemRow*   inventoryRow;
            const InventoryItemRow*   attachmentRow;
            s32                       columnIndex;
            s32                       lineIndex;
            s32                       inventoryRowIndex;
            s32                       attachmentItemId;

            columnIndex   = attachmentIndex % ITEM_MENU_ARMOR_ATTACHMENT_COLUMNS;
            lineIndex     = attachmentIndex / ITEM_MENU_ARMOR_ATTACHMENT_COLUMNS;
            carriedRange  = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems;
            inventoryRow  = inventoryGetRangeTable(carriedRange);
            attachmentRow = NULL;
            inventoryRow  = &inventoryRow[carriedRange->firstRow];
            for (inventoryRowIndex = 0; inventoryRowIndex < carriedRange->rowCount; inventoryRowIndex++, inventoryRow++) {
                if (inventoryRow->attachSlot == attachmentIndex + 1) {
                    attachmentRow = inventoryRow;
                    break;
                }
            }
            attachmentItemId = INVENTORY_ITEM_NONE;
            if (attachmentRow != NULL) {
                attachmentItemId = attachmentRow->itemId;
            }
            if (attachmentItemId != INVENTORY_ITEM_NONE) {
                itemMenuDrawItemIcon(object, rowX + columnIndex * 16, rowY + lineIndex * 16, attachmentItemId, ITEM_MENU_ICON_DEFAULT);
            }
            uiDrawRecessedRect(&object->panel, rowX + columnIndex * 16, rowY + lineIndex * 16 - 0xE, 0xE, 0xE, ITEM_MENU_ARMOR_EMPTY_SLOT_COLOR);
        }
    }
}

void itemMenuParasiteEnergySummaryTask(Task* task)
{
    enum { ITEM_MENU_ABILITIES_PER_ELEMENT = 3 };

    u8          numberText[8];
    TextDrawReq levelText;
    UiObject*   object;
    SPRT*       sprite;
    const u8*   abilityLevel;
    s32         firstColumnX;
    s32         columnWidth;
    s32         abilityRow;
    s32         element;
    s32         elementLastAbility;
    s32         columnX;
    s32         numberY;
    s32         numberRowOffset;
    s32         showThirdAbility;
    s32         bottomY;
    s32         learnedLevel;
    s32         maxLevel;
    s32         markY;
    const u8*   elementLevels;
    s32         captionElement;
    s32         lastAbilityIndex;
    s32         elementLastAbilityIndex;
    s32         markRowOffset;

    object         = task->spawnArg2.pointer;
    object->result = USER_INTERFACE_RESULT_NONE;
    firstColumnX   = object->panel.contentLeft.signedValue + 3;
    columnWidth    = (object->panel.contentRight.signedValue - object->panel.contentLeft.signedValue) / (s32)ARRAY_SIZE(D_8010E844);
    uiDrawTitle(&(object)->panel, Gp_StrPEnergy);

    // Draw levels from the third ability upward; the third appears once it can be learned.
    abilityRow      = 0;
    maxLevel        = ATTACHMENT_AREA_LEVEL_COUNT;
    numberRowOffset = 2;
    bottomY         = object->panel.contentBottom.signedValue;
    for (; abilityRow < ITEM_MENU_ABILITIES_PER_ELEMENT; abilityRow++) {
        for (element = 0, numberY = bottomY - numberRowOffset, columnX = firstColumnX, elementLastAbility = ITEM_MENU_ABILITIES_PER_ELEMENT - 1; element < (s32)ARRAY_SIZE(D_8010E844); element++) {
            abilityLevel     = attachmentGetLearnedLevels() + (elementLastAbility - abilityRow);
            showThirdAbility = 0;
            if (abilityRow == 0) {
                learnedLevel = abilityLevel[0];
                if ((learnedLevel > 0) || ((abilityLevel[-1] == maxLevel) && (abilityLevel[-2] == maxLevel))) {
                    showThirdAbility = 1;
                }
            }
            if ((abilityRow != 0) || (showThirdAbility != 0)) {
                levelText.x          = object->panel.contentOriginX.unsignedValue + 0xC + columnX;
                levelText.y          = object->panel.contentOriginY.unsignedValue + numberY;
                levelText.otIndex    = object->panel.otIndex.signedValue + 1;
                levelText.colorRgb   = 0x606060;
                levelText.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
                levelText.alignment  = TEXT_ALIGNMENT_LEFT;
                // The level-limit local also supplies mode 3 (translucent outlined text).
                levelText.drawMode = maxLevel;
                textDrawString(&levelText, textItoaSigned(numberText, abilityLevel[0]));
            }
            columnX            += columnWidth;
            elementLastAbility += ITEM_MENU_ABILITIES_PER_ELEMENT;
        }
        numberRowOffset += 9;
    }

    // Draw each element caption and one level mark for every visible ability.
    for (captionElement = 0; captionElement < (s32)ARRAY_SIZE(D_8010E844); captionElement++) {
        sprite         = gGpuPrimCursor;
        gGpuPrimCursor = sprite + 1;
        setlen(sprite, 4);
        GPU_PRIMITIVE_COLOR_WORD(sprite, 0) = GPU_PACK_COLOR_WORD(0x60, 0x60, 0x60, 0);
        setcode(sprite, 0x64);
        addPrim(gGpuCurrentOt + object->panel.otIndex.signedValue + 1, sprite);
        sprite->x0   = D_8010E844[captionElement].xOffset + (object->panel.contentOriginX.unsignedValue + firstColumnX + captionElement * columnWidth);
        sprite->y0   = object->panel.contentOriginY.unsignedValue + bottomY - 0x23;
        sprite->w    = (captionElement == 0) ? ITEM_MENU_FIRE_CAPTION_WIDTH : ITEM_MENU_OTHER_ELEMENT_CAPTION_WIDTH;
        sprite->h    = 8;
        sprite->u0   = D_8010E844[captionElement].u;
        sprite->v0   = D_8010E844[captionElement].v;
        sprite->clut = ((captionElement == 0) || (captionElement == ITEM_MENU_ELEMENT_EARTH)) ? ITEM_MENU_FIRE_EARTH_CAPTION_CLUT : ITEM_MENU_WIND_WATER_CAPTION_CLUT;

        lastAbilityIndex = (captionElement + 1) * ITEM_MENU_ABILITIES_PER_ELEMENT - 1;
        for (abilityRow = 0, elementLastAbilityIndex = lastAbilityIndex, markRowOffset = 6; abilityRow < ITEM_MENU_ABILITIES_PER_ELEMENT; abilityRow++) {
            elementLevels    = attachmentGetLearnedLevels();
            showThirdAbility = 0;
            if (abilityRow == 0) {
                elementLevels += elementLastAbilityIndex;
                if ((elementLevels[0] != 0) ||
                    ((elementLevels[-1] == ATTACHMENT_AREA_LEVEL_COUNT) && (elementLevels[-2] == ATTACHMENT_AREA_LEVEL_COUNT))) {
                    showThirdAbility = 1;
                }
            }
            if ((abilityRow != 0) || (showThirdAbility != 0)) {
                sprite         = gGpuPrimCursor;
                gGpuPrimCursor = sprite + 1;
                sprite->x0     = object->panel.contentOriginX.unsignedValue + firstColumnX + captionElement * columnWidth;
                markY          = object->panel.contentOriginY.unsignedValue + bottomY - markRowOffset;
                sprite->w      = 8;
                sprite->h      = 8;
                sprite->u0     = 0xA8;
                sprite->v0     = 0x88;
                sprite->clut   = ITEM_MENU_LEVEL_MARK_CLUT;
                setlen(sprite, 4);
                GPU_PRIMITIVE_COLOR_WORD(sprite, 0) = GPU_PACK_COLOR_WORD(0x60, 0x60, 0x60, 0);
                setcode(sprite, 0x64);
                sprite->y0 = markY;
                addPrim(gGpuCurrentOt + object->panel.otIndex.signedValue + 1, sprite);
            }
            markRowOffset += 9;
        }
    }
    uiQueueTexturePage(object->panel.otIndex.signedValue + 1, 0);
}

void itemMenuDrawWeaponSummary(const UiObject* object, s32 x, s32 rowY, s32 unused)
{
    TextDrawReq                ammoLabelReq;
    const EquipmentWeaponLoad* load;
    s32                        weaponItemId;
    s32                        colorRgb;
    s32                        loadedItemId;
    s32                        loadedQuantity;

    weaponItemId = gPlayerStatus.weapon;
    if (weaponItemId > PLAYER_STATUS_EQUIPMENT_NONE) {
        weaponItemId += EQUIPMENT_WEAPON_ITEM_FIRST - 1;
    }
    colorRgb = ITEM_MENU_STAT_TEXT_COLOR;
    itemMenuDrawItemRow(object, x, rowY, weaponItemId, colorRgb, 0);
    uiDrawHorizontalSeparator(&object->panel, object->panel.contentLeft.signedValue, object->panel.contentRight.signedValue, object->panel.contentTop.signedValue + 0x11);
    rowY          += 7;
    ammoLabelReq.x = object->panel.contentOriginX.unsignedValue + x;
    ammoLabelReq.y = object->panel.contentOriginY.unsignedValue + 2 + rowY;
    ITEM_MENU_INIT_STATS_TEXT(ammoLabelReq, &object->panel, colorRgb, TEXT_GLYPH_TABLE_SMALL, TEXT_ALIGNMENT_LEFT, TEXT_DRAW_OUTLINED);
    textDrawString(&ammoLabelReq, Gp_StrAmmoCaps);
    rowY += 0x13;
    // Empty loads still draw their slot; unavailable secondary loads have no row.
    if (weaponItemId > PLAYER_STATUS_EQUIPMENT_NONE) {
        if (weaponItemId != ITEM_MENU_TONFA_BATON_ITEM) {
            load           = equipmentGetWeaponLoad(weaponItemId);
            loadedItemId   = load->primaryItemId;
            loadedQuantity = load->primaryQty;
            if (loadedItemId != INVENTORY_ITEM_NONE) {
                itemMenuDrawQuantity(object, x, rowY, loadedQuantity, colorRgb);
            }
            itemMenuDrawItemSlotRow(object, x, rowY, loadedItemId, colorRgb, 0);
            if (load->secondaryItemId != EQUIPMENT_WEAPON_SECONDARY_UNAVAILABLE) {
                loadedItemId   = load->secondaryItemId;
                loadedQuantity = load->secondaryQty;
                rowY          += 0x10;
                if (loadedItemId != INVENTORY_ITEM_NONE) {
                    itemMenuDrawQuantity(object, x, rowY, loadedQuantity, colorRgb);
                }
                itemMenuDrawItemSlotRow(object, x, rowY, loadedItemId, colorRgb, 0);
            }
        }
    }
}

#undef ITEM_MENU_INIT_STATS_TEXT

void itemMenuDrawEquipmentMarker(const UiObject* object, s32 x, s32 y, s32 itemId, s32 attachmentState)
{
    u8                   markerText[2];
    TextDrawReq          markerReq;
    s32                  active;
    s32                  hasCarriedLoad;
    PlayerStatus*        player;
    EquipmentWeaponLoad* load;
    s32                  markerColorRgb;
    s32                  markerX;
    s32                  markerY;

    active        = 0;
    player        = &gPlayerStatus;
    markerText[0] = 0;
    markerText[1] = 0;
    EQUIPMENT_CHECK_ACTIVE_ITEM(active, player, itemId);
    // Equipped status takes priority, then a carried load, then the requested attachment mark.
    if (active != 0) {
        markerText[0]        = 'E';
        markerColorRgb       = 0x606060;
        markerX              = object->panel.contentOriginX.unsignedValue - 1;
        markerReq.x          = markerX + x;
        markerY              = object->panel.contentOriginY.unsignedValue - 2;
        markerReq.y          = markerY + y;
        markerReq.otIndex    = object->panel.otIndex.signedValue + 1;
        markerReq.colorRgb   = markerColorRgb;
        markerReq.glyphTable = TEXT_GLYPH_TABLE_SMALL;
        markerReq.alignment  = TEXT_ALIGNMENT_LEFT;
        markerReq.drawMode   = TEXT_DRAW_OUTLINED_SINGLE_ENTRY;
        textDrawString(&markerReq, Gp_StrE);
    } else {
        hasCarriedLoad = 0;
        if ((u32)(itemId - EQUIPMENT_WEAPON_ITEM_FIRST) < ITEM_MENU_WEAPON_ITEM_COUNT) {
            load = equipmentGetWeaponLoad(itemId);
            if (((load->primaryQty != 0) && (inventoryFindLastCarriedItemRow(load->primaryItemId) != NULL)) ||
                ((load->secondaryQty != 0) && (inventoryFindLastCarriedItemRow(load->secondaryItemId) != NULL))) {
                hasCarriedLoad = 1;
            }
        }
        if (hasCarriedLoad != 0) {
            markerText[0] = 'L';
        } else if (attachmentState == ITEM_MENU_ATTACHMENT_MARK_ATTACHED) {
            markerText[0] = 'A';
        }
    }
    // The equipped path also draws the constant E above; retain both submissions.
    if (markerText[0] != 0) {
        markerColorRgb       = 0x606060;
        markerX              = object->panel.contentOriginX.unsignedValue - 1;
        markerReq.x          = markerX + x;
        markerY              = object->panel.contentOriginY.unsignedValue - 2;
        markerReq.y          = markerY + y;
        markerReq.otIndex    = object->panel.otIndex.signedValue + 1;
        markerReq.colorRgb   = markerColorRgb;
        markerReq.glyphTable = TEXT_GLYPH_TABLE_SMALL;
        markerReq.alignment  = TEXT_ALIGNMENT_LEFT;
        markerReq.drawMode   = TEXT_DRAW_OUTLINED_SINGLE_ENTRY;
        textDrawString(&markerReq, markerText);
    }
}

void itemMenuDrawParasiteEnergyLevel(const UiObject* object, s32 x, s32 y, s32 level, s32 colorRgb)
{
    u8          levelText[0x20];
    TextDrawReq levelReq;
    SPRT*       markSprite;
    s32         originY;
    s32         textOriginY;
    s32         markColorRgb;

    markSprite       = gGpuPrimCursor;
    gGpuPrimCursor   = markSprite + 1;
    markSprite->x0   = object->panel.contentOriginX.unsignedValue + x + 0x6C;
    originY          = object->panel.contentOriginY.unsignedValue;
    markColorRgb     = colorRgb;
    markSprite->w    = 8;
    markSprite->h    = 8;
    markSprite->u0   = 0xA8;
    markSprite->v0   = 0x88;
    markSprite->clut = ITEM_MENU_LEVEL_MARK_CLUT;
    setlen(markSprite, 4);
    GPU_PRIMITIVE_COLOR_WORD(markSprite, 0) = markColorRgb;
    setcode(markSprite, 0x64);
    markSprite->y0 = originY + y - 7;
    addPrim(gGpuCurrentOt + object->panel.otIndex.signedValue + 1, markSprite);
    uiQueueTexturePage(object->panel.otIndex.signedValue + 1, 0);

    levelReq.x          = object->panel.contentOriginX.unsignedValue + x + 0x7C;
    textOriginY         = object->panel.contentOriginY.unsignedValue - 3;
    levelReq.y          = textOriginY + y;
    levelReq.otIndex    = object->panel.otIndex.signedValue + 1;
    levelReq.colorRgb   = markColorRgb;
    levelReq.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
    levelReq.alignment  = TEXT_ALIGNMENT_RIGHT;
    levelReq.drawMode   = TEXT_DRAW_TRANSLUCENT_OUTLINED;
    textDrawString(&levelReq, textItoaSigned(levelText, level));
}

/// Draws an item name, equipment mark, P.E. level and icon at a panel-relative row.
///
/// Coordinates are pixels at the icon's bottom-left; hidden panels submit no packets.
/// `attachmentState` requests A only for ITEM_MENU_ATTACHMENT_MARK_ATTACHED.
static inline void _itemMenuDrawItemLabelAt(const UiObject* object, s32 x, s32 y, s32 colorRgb, s32 itemId, s32 attachmentState)
{
    TextDrawReq nameReq;
    s32         abilityItemOffset;

    if (object->panel.state != USER_INTERFACE_PANEL_HIDDEN) {
        nameReq.x          = object->panel.contentOriginX.unsignedValue + 0x11 + x;
        nameReq.y          = object->panel.contentOriginY.unsignedValue + (y - 6);
        nameReq.otIndex    = object->panel.otIndex.signedValue + 1;
        nameReq.colorRgb   = colorRgb;
        nameReq.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
        nameReq.alignment  = TEXT_ALIGNMENT_LEFT;
        nameReq.drawMode   = TEXT_DRAW_OUTLINED;
        textDrawString(&nameReq, itemGetText(itemId, ITEM_TEXT_NAME, 0));
        itemMenuDrawEquipmentMarker(object, x, y, itemId, attachmentState);
        abilityItemOffset = itemId - ITEM_MENU_PARASITE_ENERGY_ITEM_FIRST;
        if ((u32)abilityItemOffset < ITEM_MENU_PARASITE_ENERGY_ITEM_COUNT) {
            itemMenuDrawParasiteEnergyLevel(object, x, y, abilityItemOffset % ATTACHMENT_AREA_LEVEL_COUNT + 1, colorRgb);
        }
        itemMenuDrawItemIcon(object, x, y, itemId, ITEM_MENU_ICON_DEFAULT);
    }
}

/// Draws the item label at the list's current row position and text color.
static inline void _itemMenuDrawItemRowLabel(const UiList* list, const UiObject* object, s32 itemId, s32 attachmentState)
{
    _itemMenuDrawItemLabelAt(object, list->rowTextX.signedValue, list->rowTextY.signedValue, list->colorRgb, itemId, attachmentState);
}

/// Borrows the zero-based reorderable row in a range, or NULL if absent.
///
/// The range must fit its backing table and choiceIndex must be nonnegative.
/// Attached items and the equipped armor and weapon are excluded. Sorting,
/// transfers or resetting the table can replace the returned row's contents.
static inline InventoryItemRow* _inventoryFindNthReorderableRow(const InventoryItemRange* range, s32 choiceIndex)
{
    PlayerStatus*     player;
    InventoryItemRow* row;
    InventoryItemRow* foundRow;
    s32               rowIndex;
    s32               reorderable;
    s32               itemId;
    s32               eligibleValue;
    s32               rangeCount;
    s32               rowLimit;

    row        = inventoryGetRangeTable(range);
    foundRow   = NULL;
    rowIndex   = 0;
    rangeCount = range->rowCount;
    row        = &row[range->firstRow];
    if (rangeCount != 0) {
        player        = &gPlayerStatus;
        eligibleValue = 1;
        rowLimit      = rangeCount;
        do {
            itemId      = row->itemId;
            reorderable = 1;
            if ((row->attachSlot != INVENTORY_ATTACHMENT_NONE) ||
                (((u32)(itemId - ITEM_MENU_ARMOR_ITEM_FIRST) < ITEM_MENU_ARMOR_ITEM_COUNT) && (player->armor == itemId - (ITEM_MENU_ARMOR_ITEM_FIRST - 1))) ||
                (((u32)(itemId - EQUIPMENT_WEAPON_ITEM_FIRST) < ITEM_MENU_WEAPON_ITEM_COUNT) && (player->weapon == itemId - (EQUIPMENT_WEAPON_ITEM_FIRST - 1)))) {
                reorderable = 0;
            }
            if (reorderable == eligibleValue) {
                choiceIndex--;
            }
            if (choiceIndex < 0) {
                foundRow = row;
                break;
            }
            rowIndex++;
            row++;
        } while (rowIndex < rowLimit);
    }
    return foundRow;
}

/// Shows `item`'s name in the holder (the empty-slot text for item 0) and
/// makes it the preview in slot 0.
#define GP_SHOW_ITEM_IN_HOLDER(item)                                                    \
    do {                                                                                \
        if ((item) == 0) {                                                              \
            uiSetPromptText(Gp_StrEmpty, 0, 0);                                         \
        } else {                                                                        \
            uiSetPromptText(itemGetText((item), ITEM_TEXT_DESCRIPTION_FIRST, 0), 0, 0); \
        }                                                                               \
        itemMenuSetPreviewItem((item), CD_COMMAND_DISPLAY_LOAD_MENU);                   \
    } while (0)

void Gp_DrawItemOrderRow(UiList* arg0, UiObject* arg1)
{
    InventoryItemRow* sel;
    s32               item;
    s32               status;
    s32               idx1;
    s32               idx2;
    UiObject*         obj;

    sel = _inventoryFindNthReorderableRow(&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems, arg0->currentItemIndex);
    if (sel == NULL) {
        itemMenuDrawSortRow(arg0, arg1);
        return;
    }

    item   = sel->itemId;
    status = arg1->panel.control.word;
    if (((status >> 16) == 1) || (status == 1)) {
        if (arg0->selectedItemIndex == arg0->currentItemIndex) {
            if (Gp_ItemOrderMode == 0) {
                GP_SHOW_ITEM_IN_HOLDER(item);
            } else {
                uiSetPromptText(Gp_StrSelectDest, 0, 0);
            }
        }
    }

    if (Gp_ItemOrderMode == 1) {
        if (arg0->rowInputEnabled != USER_INTERFACE_LIST_ROW_ACTIVE) {
            if (sel == Gp_SelItemRec) {
                arg0->colorRgb = 0x37A78;
            }
        }
    }

    {
        s32         x;
        s32         y;
        s32         color;
        u8          buf[0x20];
        TextDrawReq req;
        s32         qty;
        s32         baseY;

        x     = arg0->rowTextX.signedValue;
        y     = arg0->rowTextY.signedValue;
        color = arg0->colorRgb;
        if (sel != NULL) {
            if ((u32)(sel->itemId - 0xA0) < 0x20U) {
                qty            = sel->qty - equipmentGetLoadedConsumableQuantity(&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems, sel->itemId);
                req.x          = arg1->panel.contentOriginX.unsignedValue + 0x84 + x;
                baseY          = arg1->panel.contentOriginY.unsignedValue - 3;
                req.y          = baseY + y;
                req.otIndex    = arg1->panel.otIndex.signedValue + 1;
                req.colorRgb   = color;
                req.glyphTable = TEXT_GLYPH_TABLE_SMALL;
                req.alignment  = TEXT_ALIGNMENT_RIGHT;
                req.drawMode   = TEXT_DRAW_FILL_ONLY;
                textDrawString(&req, textItoaSigned(buf, qty));
                uiDrawRecessedRect(&arg1->panel, (x + 0x69), (y - 8), 0x1B, 7,
                                   0x102010);
            }
        }
    }

    _itemMenuDrawItemRowLabel(arg0, arg1, item, ITEM_MENU_ATTACHMENT_MARK_UNATTACHED);

    if (arg0->rowInputEnabled == USER_INTERFACE_LIST_ROW_ACTIVE) {
        if (Gp_ItemOrderMode == 0) {
            Gp_SelItemRec = sel;
            if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm) != 0) {
                sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
                obj = uiSpawnObject(&D_8010EAB4[34], 0, 1, 1, arg1);
                if (obj != NULL) {
                    uiPositionRowDialog(&(obj)->panel, arg0, &(arg1)->panel);
                    arg1->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
                }
            } else {
                itemMenuOpenInfoOnTriangle(arg1);
            }
        } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm) != 0) {
            InventoryItemRange* scan2;
            scan2 = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems;
            idx1  = inventoryIndexOfRow(scan2, Gp_SelItemRec);
            idx2  = inventoryIndexOfRow(scan2, sel);
            sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
            if (idx1 >= 0) {
                if (idx2 >= 0) {
                    inventoryMoveItemRow(scan2, idx1, idx2);
                }
            }
            Gp_ItemOrderMode = 0;
        }
    }
}

void itemMenuSetWeaponChoiceRows(UiList* list, s32 consumableItemId)
{
    InventoryItemRow*   rows;
    InventoryItemRange* carried;
    PlayerStatus*       player;
    s32                 choiceCount = 0;
    s32                 rowIndex;
    s32                 scannedRows = 0;
    s32                 acceptedItemIndex;

    rows     = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.itemRows;
    carried  = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems;
    player   = &gPlayerStatus;
    rowIndex = carried->firstRow;
    for (scannedRows = 0; scannedRows < carried->rowCount; scannedRows++) {
        if ((u8)(rows[rowIndex].itemId - EQUIPMENT_WEAPON_ITEM_FIRST) >= ARRAY_SIZE(Gp_RelatedQty0.rows)) {
            rowIndex++;
            continue;
        }
        if (consumableItemId == INVENTORY_ITEM_NONE) {
            choiceCount++;
        } else {
            // Search the primary, then the secondary load choices of this weapon.
            for (acceptedItemIndex = 0; acceptedItemIndex < ARRAY_SIZE(Gp_RelatedQty0.rows[0].acceptedItemIds); acceptedItemIndex++) {
                if (Gp_RelatedQty0.rows[rows[rowIndex].itemId - EQUIPMENT_WEAPON_ITEM_FIRST].acceptedItemIds[acceptedItemIndex] == consumableItemId) {
                    if (rows[rowIndex].attachSlot > INVENTORY_ATTACHMENT_NONE || player->weapon == rows[rowIndex].itemId - (EQUIPMENT_WEAPON_ITEM_FIRST - 1)) {
                        choiceCount++;
                    }
                    break;
                }
            }
            for (acceptedItemIndex = 0; acceptedItemIndex < ARRAY_SIZE(Gp_RelatedQty1.rows[0].acceptedItemIds); acceptedItemIndex++) {
                if (Gp_RelatedQty1.rows[rows[rowIndex].itemId - EQUIPMENT_WEAPON_ITEM_FIRST].acceptedItemIds[acceptedItemIndex] == consumableItemId) {
                    if (rows[rowIndex].attachSlot > INVENTORY_ATTACHMENT_NONE || player->weapon == rows[rowIndex].itemId - (EQUIPMENT_WEAPON_ITEM_FIRST - 1)) {
                        choiceCount++;
                    }
                    break;
                }
            }
        }
        rowIndex++;
    }

    list->itemCount                     = choiceCount;
    list->visibleRowCount.unsignedValue = choiceCount;
    if (consumableItemId == INVENTORY_ITEM_NONE) {
        // The weapon list shows four rows however many weapons are carried.
        list->rowHeight                     = ITEM_MENU_LIST_ROW_HEIGHT;
        list->visibleRowCount.unsignedValue = ITEM_MENU_WEAPON_CHOICE_VISIBLE_ROWS;
    } else {
        list->rowHeight = ITEM_MENU_LIST_ROW_HEIGHT;
    }
}

/// Sets list counts for loose carried items plus the Sort command row.
///
/// Attached items and equipped armor/weapon rows are excluded from the scan.
/// At most nine rows are visible; stored counts retain their byte narrowing.
static __inline__ void _itemMenuSetReorderableRows(UiList* list)
{
    InventoryItemRange* range;
    InventoryItemRow*   row;
    s32                 rowIndex;
    u16                 rowCount;
    s32                 reorderable;
    s32                 itemId;

    range           = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems;
    row             = inventoryGetRangeTable(range);
    row             = &row[range->firstRow];
    list->itemCount = range->rowCount;
    rowCount        = range->rowCount;
    for (rowIndex = 0; rowIndex < rowCount; rowIndex++, row++) {
        itemId      = row->itemId;
        reorderable = 1;
        if ((row->attachSlot != INVENTORY_ATTACHMENT_NONE) ||
            (((u32)(itemId - ITEM_MENU_ARMOR_ITEM_FIRST) < ITEM_MENU_ARMOR_ITEM_COUNT) && (gPlayerStatus.armor == itemId - (ITEM_MENU_ARMOR_ITEM_FIRST - 1))) ||
            (((u32)(itemId - EQUIPMENT_WEAPON_ITEM_FIRST) < ITEM_MENU_WEAPON_ITEM_COUNT) && (gPlayerStatus.weapon == itemId - (EQUIPMENT_WEAPON_ITEM_FIRST - 1)))) {
            reorderable = 0;
        }
        if (reorderable == 0) {
            list->itemCount--;
        }
    }
    list->itemCount                     = list->itemCount + 1;
    list->visibleRowCount.unsignedValue = list->itemCount;
    if (list->visibleRowCount.signedValue > ITEM_MENU_REORDER_VISIBLE_ROWS) {
        list->visibleRowCount.unsignedValue = ITEM_MENU_REORDER_VISIBLE_ROWS;
    }
}

/// Closes an accepted dialog, restores its inventory/equipment pane's input and undims it.
///
/// beginDestinationSelection != 0 selects a destination for the selected carried row;
/// the row is not exchanged here. Both objects and the child's owner must be
/// live. Closing detaches the child from its parent's ring and retains it for
/// animated teardown, so a traversal must save the next sibling before calling.
/// Zero leaves the current item-order mode intact.
static inline void _itemMenuAcceptPanelChild(UiObject* object, UiObject* childObject, s32 beginDestinationSelection)
{
    enum { ITEM_MENU_MODE_DESTINATION_SELECT = 1 };

    uiStartTreeClosing(childObject, childObject->owner);
    object->panel.control.word = USER_INTERFACE_PANEL_ACTIVE;
    if (beginDestinationSelection != 0) {
        Gp_ItemOrderMode = ITEM_MENU_MODE_DESTINATION_SELECT;
    }
    object->panel.style &= (u32)~USER_INTERFACE_PANEL_DIMMED;
}

/// Runs the carried-item list, its count strip and accepted command dialogs.
///
/// spawnArg2 is the live inventory UiObject. Lists loose unequipped rows,
/// including free slots, plus Sort; at most nine rows are visible. The filtered
/// count plus Sort must fit a positive signed byte. Initialization reserves four
/// task-owned bytes whose payload role is unproven; default teardown releases them. Cancel
/// leaves for command 1 unless choosing a swap destination, when it cancels that
/// mode. Menu or a child's CANCEL propagates CANCEL; accepted children restore
/// input, and result 0x23 enters destination selection without swapping rows here.
/// Requires a valid live carried range, the shared list and menu drawing resources.
static void _itemMenuInventoryListTask(Task* task)
{
    enum { ITEM_MENU_PANEL_INITIAL              = 0,
           ITEM_MENU_MODE_SELECT                = 0,
           ITEM_MENU_COMMAND_MAIN               = 1,
           ITEM_MENU_RESULT_BEGIN_SWAP          = USER_INTERFACE_LIST_ACTION_MOVE,
           ITEM_MENU_INVENTORY_COUNT_DESCRIPTOR = 5,
           ITEM_MENU_INVENTORY_WORK_BYTES       = 4 };
    UiObject* object;
    UiList*   inventoryList;
    UiObject* countObject;
    void*     workAllocation;
    s32       controlMode;
    Task*     ownerTask;
    Task*     firstChild;
    Task*     childTask;
    Task*     nextChild;
    UiObject* childObject;
    s32       childResult;

    object         = task->spawnArg2.pointer;
    inventoryList  = &D_8010E854;
    object->result = USER_INTERFACE_RESULT_NONE;
    if (task->state == ITEM_MENU_PANEL_INITIAL) {
        Gp_ItemOrderMode = ITEM_MENU_MODE_SELECT;
        workAllocation   = memCalloc(ITEM_MENU_INVENTORY_WORK_BYTES, 0);
        if (workAllocation == NULL) {
            uiStartTreeClosing(object, task);
            return;
        }
        task->work                    = workAllocation;
        inventoryList->wrapNavigation = 0;
        _itemMenuSetReorderableRows(inventoryList);
        // Fit the outer panel to nine rows, then restore the current filtered count.
        inventoryList->visibleRowCount.unsignedValue = ITEM_MENU_REORDER_VISIBLE_ROWS;
        inventoryList->itemCount                     = ITEM_MENU_REORDER_VISIBLE_ROWS;
        uiFitPanelToList(inventoryList, &(object)->panel);
        _itemMenuSetReorderableRows(inventoryList);
        inventoryList->flags                               = USER_INTERFACE_LIST_SHARED_ROW_CALLBACK;
        inventoryList->selectedItemIndex                   = 0;
        inventoryList->firstVisibleItemIndex.unsignedValue = 0;
        countObject                                        = uiSpawnObject(&D_8010EAB4[ITEM_MENU_INVENTORY_COUNT_DESCRIPTOR], 0, USER_INTERFACE_PANEL_INACTIVE, 1, object);
        if (countObject != NULL) {
            countObject->panel.bounds.unsignedRect.y = object->panel.bounds.unsignedRect.y + object->panel.bounds.unsignedRect.h;
        }
        task->state = task->state + 1;
    }
    uiDrawPanelLabel(&(object)->panel, Gp_StrItemHdr);
    _itemMenuSetReorderableRows(inventoryList);
    uiRefreshListViewport(inventoryList, &(object)->panel);
    inventoryList->flags = USER_INTERFACE_LIST_SHARED_ROW_CALLBACK;
    if (inventoryList->selectedItemIndex >= inventoryList->itemCount) {
        inventoryList->selectedItemIndex = inventoryList->itemCount - 1;
    }
    if ((inventoryList->itemCount - inventoryList->visibleRowCount.signedValue) < inventoryList->firstVisibleItemIndex.signedValue) {
        inventoryList->firstVisibleItemIndex.unsignedValue = inventoryList->itemCount - inventoryList->visibleRowCount.unsignedValue;
    }
    uiUpdateList(inventoryList, &object->panel);
    controlMode = object->panel.control.word;
    if (controlMode == USER_INTERFACE_PANEL_ACTIVE) {
        if (object->result == USER_INTERFACE_RESULT_NONE) {
            if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskMenu) != 0) {
                object->result = USER_INTERFACE_RESULT_CANCEL;
            } else if (Gp_ItemOrderMode == ITEM_MENU_MODE_SELECT) {
                if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskCancel) != 0) {
                    sndEvtRequestScriptStart(SOUND_MENU_CANCEL, 0, 0);
                    object->resultValue = ITEM_MENU_COMMAND_MAIN;
                    object->result      = USER_INTERFACE_RESULT_CONFIRM;
                } else {
                    padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_L2 | PAD_BUTTON_R2);
                }
            } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskCancel) != 0) {
                sndEvtRequestScriptStart(SOUND_MENU_CANCEL, 0, 0);
                Gp_ItemOrderMode = ITEM_MENU_MODE_SELECT;
            }
        }
    } else if (controlMode >= USER_INTERFACE_PANEL_REQUEST_MIN) {
        object->panel.control.word = USER_INTERFACE_PANEL_ACTIVE;
    }
    // Accepted dialogs detach from the live child ring before input is restored.
    ownerTask  = object->owner;
    firstChild = ownerTask->firstChild;
    if (firstChild != NULL) {
        childTask = firstChild;
        do {
            childObject = childTask->spawnArg2.pointer;
            childResult = childObject->result;
            nextChild   = childTask->nextSibling;
            switch (childResult) {
                case USER_INTERFACE_RESULT_CANCEL:
                    object->result = childResult;
                    break;
                case USER_INTERFACE_RESULT_CONFIRM:
                    _itemMenuAcceptPanelChild(object, childObject, 0);
                    break;
                case ITEM_MENU_RESULT_BEGIN_SWAP:
                    _itemMenuAcceptPanelChild(object, childObject, 1);
                    break;
            }
            firstChild = ownerTask->firstChild;
            childTask  = nextChild;
            if (childTask == firstChild) {
                break;
            }
        } while (firstChild != NULL);
    }
}

void itemMenuInventoryPanelTask(Task* task)
{
    enum { ITEM_MENU_PANEL_INITIAL           = 0,
           ITEM_MENU_WEAPON_PANE_DESCRIPTOR  = 47,
           ITEM_MENU_DESTINATION_WEAPON_PANE = 0,
           ITEM_MENU_DESTINATION_ARMOR_PANE  = 1,
           ITEM_MENU_REOPEN_DELAY_TICKS      = 16 };
    UiObject*           object;
    UiObject*           equipmentPane;
    const UiObjectDesc* weaponDescriptor;
    s16                 cursorY;
    UiCursorPosition    cursorPosition;

    object = task->spawnArg2.pointer;
    if (task->state == ITEM_MENU_PANEL_INITIAL) {
        weaponDescriptor                              = &D_8010EAB4[ITEM_MENU_WEAPON_PANE_DESCRIPTOR];
        Gp_ItemCountShow                              = false;
        D_80114D98[ITEM_MENU_DESTINATION_WEAPON_PANE] = uiSpawnObject(weaponDescriptor, 1, USER_INTERFACE_PANEL_ACTIVE, 1, object);
        D_80114D98[ITEM_MENU_DESTINATION_ARMOR_PANE]  = uiSpawnObject(weaponDescriptor + 1, 1, USER_INTERFACE_PANEL_INACTIVE, 1, object);
    }
    _itemMenuInventoryListTask(task);
    if ((Gp_ItemCountShow == true) && (uiIsPanelHidingOrHidden(object) == 0)) {
        uiStartPanelHiding(object, object->owner);
    } else if ((Gp_ItemCountShow == false) && (uiIsPanelHidingOrHidden(object) == 1)) {
        uiLimitHiddenDelayOrOpen(&(object)->panel, object->owner, ITEM_MENU_REOPEN_DELAY_TICKS);
    }
    if (object->panel.control.word == USER_INTERFACE_PANEL_ACTIVE) {
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_LEFT) != 0) {
            // Carry signed screen Y into the equipment pane that contains that height.
            equipmentPane          = D_80114D98[ITEM_MENU_DESTINATION_ARMOR_PANE];
            *(s32*)&cursorPosition = uiGetCursorPositionWord();
            if (cursorPosition.y.signedValue < equipmentPane->panel.bounds.rect.y) {
                equipmentPane = D_80114D98[ITEM_MENU_DESTINATION_WEAPON_PANE];
            }
            sndEvtRequestScriptStart(SOUND_MENU_CURSOR, 0, 0);
            object->panel.control.word        = USER_INTERFACE_PANEL_INACTIVE;
            cursorY                           = cursorPosition.y.signedValue;
            equipmentPane->panel.control.word = USER_INTERFACE_PANEL_FOCUS_TRANSFER;
            equipmentPane->resultValue        = cursorY;
        }
    }
}

/// Sets a spawned child's panel position to the low 16 bits of x and y.
///
/// Coordinates are layout pixels in the same space as its descriptor's bounds.
static inline void _itemMenuSetChildPosition(UiObject* child, s32 x, s32 y)
{
    child->panel.bounds.unsignedRect.y = y;
    child->panel.bounds.unsignedRect.x = x;
}

void Gp_DrawWeaponSlotRow(UiList* prompt, UiObject* obj)
{
    TextDrawReq   req;
    PlayerStatus* player;
    s32           item;
    s32           status;
    s32           mode;
    s32           x;
    s32           y;
    s32           color;
    s32           temp;

    player = &gPlayerStatus;
    item   = player->weapon + 0x7F;
    if (item < 0x80) {
        item = 0;
    }

    status = obj->panel.control.word;
    if (((status >> 16) == 1 || status == 1) && prompt->selectedItemIndex == prompt->currentItemIndex) {
        if (Gp_ItemOrderMode == 0) {
            if (item == 0) {
                uiSetPromptText(Gp_StrEmpty, 0, 0);
            } else {
                uiSetPromptText(itemGetText(item, ITEM_TEXT_DESCRIPTION_FIRST, 0), 0, 0);
            }
            itemMenuSetPreviewItem(item, CD_COMMAND_DISPLAY_LOAD_MENU);
        } else {
            uiSetPromptText(Gp_StrSelectDest, 0, 0);
        }
    }

    status = prompt->rowInputEnabled;
    if (status == 1) {
        mode = Gp_ItemOrderMode;
        if (mode == 0) {
            InventoryItemRange* scan;
            InventoryItemRow*   table;
            s32                 i;
            s32                 count;

            scan  = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems;
            table = inventoryGetRangeTable(scan);
            if (item != 0) {
                table = &table[scan->firstRow];
                count = scan->rowCount;
                for (i = 0; i < count; i++, table++) {
                    if (table->itemId == item) {
                        break;
                    }
                }
                Gp_SelItemRec = table;
            } else {
                Gp_SelItemRec = NULL;
            }
            if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm) != 0) {
                itemMenuSetWeaponChoiceRows(&D_8010E9A4, 0);
                if (D_8010E9A4.itemCount >= 2U || (D_8010E9A4.itemCount == 1 && player->weapon == PLAYER_STATUS_EQUIPMENT_NONE)) {
                    UiObject* spawned;
                    sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
                    spawned = uiSpawnObject(&D_8010EAB4[20], 0, 1, 0x10, obj);
                    if (spawned != NULL) {
                        _itemMenuSetChildPosition(spawned, -8, -0x5C);
                    }
                    obj->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
                } else {
                    itemMenuSpawnNotice(obj, ITEM_MENU_NOTICE_NO_OTHER_WEAPON, 0, ITEM_MENU_NOTICE_RESULT_CONFIRM);
                    obj->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
                }
            } else {
                itemMenuOpenInfoOnTriangle(obj);
            }
        } else if (mode == status) {
            if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm) != 0) {
                if ((u8)(Gp_SelItemRec->itemId + 0x80) < 0x20) {
                    PlayerStatus*     p;
                    InventoryItemRow* rec;

                    p = &gPlayerStatus;
                    equipmentClearSelectedRemovableLoads(item, EQUIPMENT_CLEAR_LOAD_BOTH);
                    rec       = Gp_SelItemRec;
                    p->weapon = rec->itemId - 0x7F;
                    itemSetIdentified(rec->itemId, 1);
                    Gp_ItemOrderMode = 0;
                    sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
                } else {
                    Task* parent;
                    parent = obj->owner->parent;
                    if (parent != NULL) {
                        Gp_ItemOrderMode                                           = 0;
                        ((UiObject*)parent->spawnArg2.pointer)->panel.control.word = mode;
                        obj->panel.control.word                                    = USER_INTERFACE_PANEL_INACTIVE;
                        sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
                    }
                }
            }
        }
    }

    x     = prompt->rowTextX.signedValue;
    y     = prompt->rowTextY.signedValue;
    color = prompt->colorRgb;
    if (obj->panel.state != USER_INTERFACE_PANEL_HIDDEN) {
        req.x          = obj->panel.contentOriginX.unsignedValue + 0x11 + x;
        req.y          = obj->panel.contentOriginY.unsignedValue + (y - 6);
        req.otIndex    = obj->panel.otIndex.signedValue + 1;
        req.colorRgb   = color;
        req.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
        req.alignment  = TEXT_ALIGNMENT_LEFT;
        req.drawMode   = TEXT_DRAW_OUTLINED;
        textDrawString(&req, itemGetText(item, ITEM_TEXT_NAME, 0));
        temp = item - 0xF;
        if ((u32)temp < 0x24U) {
            itemMenuDrawParasiteEnergyLevel(obj, x, y, temp % 3 + 1, color);
        }
        itemMenuDrawItemIcon(obj, x, y, item, ITEM_MENU_ICON_DEFAULT);
    }

    uiDrawHorizontalSeparator(&(obj)->panel, obj->panel.contentLeft.signedValue, obj->panel.contentRight.signedValue, obj->panel.contentTop.signedValue + 0x11);

    req.x          = obj->panel.contentOriginX.unsignedValue + prompt->rowTextX.signedValue;
    req.y          = prompt->rowTextY.signedValue + (obj->panel.contentOriginY.unsignedValue + 9);
    req.otIndex    = obj->panel.otIndex.signedValue + 1;
    req.colorRgb   = 0x606060;
    req.glyphTable = TEXT_GLYPH_TABLE_SMALL;
    req.alignment  = TEXT_ALIGNMENT_LEFT;
    req.drawMode   = TEXT_DRAW_OUTLINED;
    textDrawString(&req, Gp_StrAmmoCaps);
    prompt->rowTextY.signedValue += 0xA;
}

void Gp_DrawWeaponSlotRow2(UiList* prompt, UiObject* obj)
{
    union {
        struct {
            u8          buf[0x20];
            TextDrawReq req;
        } qty;
        TextDrawReq name;
    } draw;
    s32                  item;
    s32                  count;
    s32                  weapon;
    s32                  status;
    s32                  rowState;
    s32                  mode;
    EquipmentWeaponLoad* slot;
    InventoryItemRow*    rec;
    UiObject*            child;
    Task*                parent;
    UiObject*            parentObj;

    item   = 0;
    count  = 0;
    weapon = gPlayerStatus.weapon + 0x7F;
    if (weapon >= 0x80) {
        slot = equipmentGetWeaponLoad(weapon);
        if (prompt->currentItemIndex == 1) {
            item  = slot->primaryItemId;
            count = slot->primaryQty;
        } else {
            item  = slot->secondaryItemId;
            count = slot->secondaryQty;
        }
    }
    status = obj->panel.control.word;
    if (((status >> 16) == 1 || status == 1) && prompt->selectedItemIndex == prompt->currentItemIndex) {
        if (Gp_ItemOrderMode == 0) {
            if (item != 0) {
                uiSetPromptText(itemGetText(item, ITEM_TEXT_DESCRIPTION_FIRST, 0), 0, 0);
                itemMenuSetPreviewItem(item, CD_COMMAND_DISPLAY_LOAD_MENU);
            } else {
                uiSetPromptText(Gp_StrAmmoNone, 0, 0);
            }
        } else {
            uiSetPromptText(Gp_StrSelectDest, 0, 0);
        }
    }
    if (item != 0) {
        s32 x;
        s32 y;
        s32 color;
        s32 off;
        x                       = prompt->rowTextX.signedValue;
        y                       = prompt->rowTextY.signedValue;
        color                   = prompt->colorRgb;
        draw.qty.req.x          = obj->panel.contentOriginX.unsignedValue + 0x84 + x;
        off                     = obj->panel.contentOriginY.unsignedValue - 3;
        draw.qty.req.y          = off + y;
        draw.qty.req.otIndex    = obj->panel.otIndex.signedValue + 1;
        draw.qty.req.colorRgb   = color;
        draw.qty.req.glyphTable = TEXT_GLYPH_TABLE_SMALL;
        draw.qty.req.alignment  = TEXT_ALIGNMENT_RIGHT;
        draw.qty.req.drawMode   = TEXT_DRAW_FILL_ONLY;
        textDrawString(&draw.qty.req, textItoaSigned(draw.qty.buf, count));
        uiDrawRecessedRect(&obj->panel, x + 0x69, y - 8, 0x1B, 7, 0x102010);
    }
    {
        s32 x;
        s32 y;
        s32 color;
        s32 off;
        s32 temp;
        x     = prompt->rowTextX.signedValue;
        y     = prompt->rowTextY.signedValue;
        color = prompt->colorRgb;
        if (item == 0) {
            uiDrawRecessedRect(&obj->panel, x, y - 0xE, 0xE, 0xE, 0x102010);
        } else {
            if (obj->panel.state != USER_INTERFACE_PANEL_HIDDEN) {
                draw.name.x          = obj->panel.contentOriginX.unsignedValue + 0x11 + x;
                off                  = obj->panel.contentOriginY.unsignedValue - 6;
                draw.name.y          = off + y;
                draw.name.otIndex    = obj->panel.otIndex.signedValue + 1;
                draw.name.colorRgb   = color;
                draw.name.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
                draw.name.alignment  = TEXT_ALIGNMENT_LEFT;
                draw.name.drawMode   = TEXT_DRAW_OUTLINED;
                textDrawString(&draw.name, itemGetText(item, ITEM_TEXT_NAME, 0));
                temp = item - 0xF;
                if ((u32)temp < 0x24U) {
                    itemMenuDrawParasiteEnergyLevel(obj, x, y, temp % 3 + 1, color);
                }
                itemMenuDrawItemIcon(obj, x, y, item, ITEM_MENU_ICON_DEFAULT);
            }
            uiDrawRecessedRect(&obj->panel, x, y - 0xE, 0xE, 0xE, 0);
        }
    }
    rowState = prompt->rowInputEnabled;
    if (rowState == 1) {
        Gp_ReloadMode = prompt->currentItemIndex;
        mode          = Gp_ItemOrderMode;
        if (mode == 0) {
            rec = NULL;
            if (item != 0) {
                rec = inventoryFindLastCarriedItemRow(item);
            }
            Gp_SelItemRec = rec;
            if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm) != 0) {
                s32 currentWeapon;
                s32 yOffset;
                s32 xOffset;
                currentWeapon = gPlayerStatus.weapon + 0x7F;
                sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
                child = uiSpawnObject(&D_8010EAB4[19], currentWeapon, 1, 0x10, obj);
                if (child != NULL) {
                    yOffset                            = -0x5C;
                    child->panel.bounds.unsignedRect.y = yOffset;
                    xOffset                            = -8;
                    child->panel.bounds.unsignedRect.x = xOffset;
                }
                obj->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
            } else {
                itemMenuOpenInfoOnTriangle(obj);
            }
        } else if (mode == rowState) {
            if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm) != 0) {
                if (equipmentLoadWeaponConsumable(&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems, weapon, Gp_SelItemRec->itemId, EQUIPMENT_WEAPON_LOAD_TO_CAPACITY) >= 0) {
                    itemSetIdentified(Gp_SelItemRec->itemId, 1);
                    sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
                    Gp_ItemOrderMode = 0;
                } else {
                    parent = obj->owner->parent;
                    if (parent != NULL) {
                        parentObj = parent->spawnArg2.pointer;
                        sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
                        Gp_ItemOrderMode              = 0;
                        parentObj->panel.control.word = mode;
                        obj->panel.control.word       = USER_INTERFACE_PANEL_INACTIVE;
                    }
                }
            }
        }
    }
}

/// Sets rows for the equipped weapon and its available load slots.
///
/// No weapon and the Tonfa Baton have one row; other weapons have two, or
/// three when a secondary load exists. The unused no-weapon load address is
/// formed before the test but never dereferenced.
static inline void _itemMenuSetWeaponRows(UiList* list)
{
    s32                  weaponItemId;
    EquipmentWeaponLoad* load;

    weaponItemId = gPlayerStatus.weapon + (EQUIPMENT_WEAPON_ITEM_FIRST - 1);
    load         = equipmentGetWeaponLoad(weaponItemId);
    if (weaponItemId < EQUIPMENT_WEAPON_ITEM_FIRST || weaponItemId == ITEM_MENU_TONFA_BATON_ITEM) {
        list->itemCount = 1;
    } else if (load->secondaryItemId != EQUIPMENT_WEAPON_SECONDARY_UNAVAILABLE) {
        list->itemCount = 3;
    } else {
        list->itemCount = 2;
    }
}

/// Caps a list selection at the last visible row and last item.
///
/// visibleEndIndex is exclusive. This applies upper limits only; an empty
/// list can select -1, and negative selections are left intact.
static inline void _itemMenuClampArmorSelection(UiList* list, s32 visibleEndIndex)
{
    if (list->selectedItemIndex >= visibleEndIndex) {
        list->selectedItemIndex = visibleEndIndex - 1;
    }
    if (list->selectedItemIndex >= list->itemCount) {
        list->selectedItemIndex = list->itemCount - 1;
    }
}

void itemMenuWeaponPanelTask(Task* task)
{
    enum {
        ITEM_MENU_WEAPON_PANEL_INITIAL = 0,
        ITEM_MENU_WEAPON_MODE_SELECT   = 0,
        ITEM_MENU_RESULT_BEGIN_SWAP    = USER_INTERFACE_LIST_ACTION_MOVE,
        ITEM_MENU_FOCUS_FIRST_ROW_Y    = -160,
        ITEM_MENU_REOPEN_DELAY_TICKS   = 16
    };

    UiObject*        object;
    UiList*          weaponList;
    s32              controlMode;
    Task*            ownerTask;
    Task*            childTask;
    Task*            nextChild;
    Task*            firstChild;
    UiObject*        childObject;
    s32              childResult;
    UiCursorPosition cursorPosition;

    object         = task->spawnArg2.pointer;
    weaponList     = &D_8010E884;
    object->result = USER_INTERFACE_RESULT_NONE;
    uiDrawPanelLabel(&(object)->panel, Gp_StrWeaponTitle);
    // Initialize the equipped-weapon rows, then refresh them as loads change.
    if (task->state == ITEM_MENU_WEAPON_PANEL_INITIAL) {
        _itemMenuSetWeaponRows(weaponList);
        weaponList->selectedItemIndex = 0;
        uiInitList(weaponList, &(object)->panel);
        task->state = task->state + 1;
    }
    _itemMenuSetWeaponRows(weaponList);
    uiRefreshListViewport(weaponList, &(object)->panel);
    uiUpdateList(weaponList, &object->panel);
    if ((Gp_ItemCountShow == 1) && (uiIsPanelHidingOrHidden(object) == 0)) {
        uiStartPanelHiding(object, object->owner);
    } else if ((Gp_ItemCountShow == 0) && (uiIsPanelHidingOrHidden(object) == 1)) {
        uiLimitHiddenDelayOrOpen(&(object)->panel, object->owner, ITEM_MENU_REOPEN_DELAY_TICKS);
    }
    // Hand focus to the armour or inventory pane without changing the cursor height.
    controlMode = object->panel.control.word;
    if (controlMode == USER_INTERFACE_PANEL_ACTIVE) {
        if (weaponList->actionResult == USER_INTERFACE_LIST_ACTION_AT_END) {
            sndEvtRequestScriptStart(SOUND_MENU_CURSOR, 0, 0);
            D_80114D98[1]->resultValue        = ITEM_MENU_FOCUS_FIRST_ROW_Y;
            D_80114D98[1]->panel.control.word = USER_INTERFACE_PANEL_FOCUS_TRANSFER;
            object->panel.control.word        = USER_INTERFACE_PANEL_INACTIVE;
        } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_RIGHT) != 0) {
            Task*     parentTask;
            UiObject* parentObject;

            parentTask = task->parent;
            if (parentTask != NULL) {
                UiList* inventoryList;
                s16     cursorRow;

                parentObject = parentTask->spawnArg2.pointer;
                sndEvtRequestScriptStart(SOUND_MENU_CURSOR, 0, 0);
                *(s32*)&cursorPosition           = uiGetCursorPositionWord();
                inventoryList                    = &D_8010E854;
                cursorRow                        = cursorPosition.y.unsignedValue - (parentObject->panel.contentOriginY.unsignedValue + parentObject->panel.contentTop.unsignedValue);
                cursorRow                        = cursorRow / inventoryList->rowHeight;
                inventoryList->selectedItemIndex = cursorRow + inventoryList->firstVisibleItemIndex.signedValue;
                _itemMenuClampArmorSelection(inventoryList, inventoryList->firstVisibleItemIndex.signedValue + inventoryList->visibleRowCount.signedValue);
                parentObject->panel.control.word = controlMode;
                object->panel.control.word       = USER_INTERFACE_PANEL_INACTIVE;
            }
        } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskCancel) != 0) {
            Task*     parentTask;
            UiObject* parentObject;

            sndEvtRequestScriptStart(SOUND_MENU_CANCEL, 0, 0);
            parentTask = task->parent;
            if (parentTask != NULL) {
                parentObject = parentTask->spawnArg2.pointer;
                if (Gp_ItemOrderMode == ITEM_MENU_WEAPON_MODE_SELECT) {
                    parentObject->resultValue = controlMode;
                    parentObject->result      = USER_INTERFACE_RESULT_CONFIRM;
                } else {
                    parentObject->panel.control.word = controlMode;
                    object->panel.control.word       = USER_INTERFACE_PANEL_INACTIVE;
                    Gp_ItemOrderMode                 = ITEM_MENU_WEAPON_MODE_SELECT;
                }
            }
        } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskMenu) != 0) {
            object->result = USER_INTERFACE_RESULT_CANCEL;
        }
    }
    // Child results close their dialog and may enter item-swapping mode.
    ownerTask  = object->owner;
    firstChild = ownerTask->firstChild;
    if (firstChild != NULL) {
        childTask = firstChild;
        do {
            childObject = childTask->spawnArg2.pointer;
            childResult = childObject->result;
            nextChild   = childTask->nextSibling;
            switch (childResult) {
                case USER_INTERFACE_RESULT_CANCEL:
                    object->result = childResult;
                    break;
                case USER_INTERFACE_RESULT_CONFIRM:
                    _itemMenuAcceptPanelChild(object, childObject, 0);
                    break;
                case ITEM_MENU_RESULT_BEGIN_SWAP:
                    _itemMenuAcceptPanelChild(object, childObject, 1);
                    break;
            }
            firstChild = ownerTask->firstChild;
            childTask  = nextChild;
            if (childTask == firstChild) {
                break;
            }
        } while (firstChild != NULL);
    }
    // Map the transferred screen Y to a weapon/load row; preserve the original thresholds.
    if (object->panel.control.word == USER_INTERFACE_PANEL_FOCUS_TRANSFER) {
        s32 rowOffsetY;

        rowOffsetY  = object->panel.contentOriginY.signedValue;
        rowOffsetY += object->panel.contentTop.signedValue;
        rowOffsetY  = object->resultValue - rowOffsetY;
        if (rowOffsetY < weaponList->rowHeight) {
            weaponList->selectedItemIndex = 0;
        } else if ((weaponList->rowHeight * 2 + 0xA) >= rowOffsetY) {
            weaponList->selectedItemIndex = 1;
        } else if (weaponList->itemCount < 2) {
            weaponList->selectedItemIndex = 1;
        } else {
            weaponList->selectedItemIndex = 2;
        }
        object->resultValue        = 0;
        object->panel.control.word = USER_INTERFACE_PANEL_ACTIVE;
    }
}

void func_800C41A4(UiList* prompt, UiObject* obj)
{
    union {
        struct {
            u8          buf[0x20];
            TextDrawReq req;
        } qty;
        TextDrawReq name;
    } draw;
    s32               item;
    s32               status;
    s32               rowState;
    s32               mode;
    InventoryItemRow* rec;
    UiObject*         child;
    UiObject*         dialog;
    Task*             parent;
    UiObject*         parentObj;
    {
        InventoryItemRange* scan;
        InventoryItemRow*   table;
        InventoryItemRow*   found;
        s32                 row;
        s32                 i;
        row   = prompt->currentItemIndex;
        scan  = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems;
        table = inventoryGetRangeTable(scan);
        found = NULL;
        i     = 0;
        item  = 0;
        table = &table[scan->firstRow];
        for (; i < scan->rowCount; i++, table++) {
            if (table->attachSlot == row + 1) {
                found = table;
                break;
            }
        }
        rec = found;
    }
    if (rec != NULL) {
        item = rec->itemId;
    }
    status = obj->panel.control.word;
    if (((status >> 16) == 1 || status == 1) && prompt->selectedItemIndex == prompt->currentItemIndex) {
        if (Gp_ItemOrderMode == 0) {
            if (item != 0) {
                uiSetPromptText(itemGetText(item, ITEM_TEXT_DESCRIPTION_FIRST, 0), 0, 0);
                itemMenuSetPreviewItem(item, CD_COMMAND_DISPLAY_LOAD_MENU);
            } else {
                uiSetPromptText(Gp_StrAttachNone, 0, 0);
            }
        } else {
            uiSetPromptText(Gp_StrSelectDest, 0, 0);
        }
    }
    if (rec != NULL) {
        s32 x;
        s32 y;
        s32 color;
        s32 off;
        s32 id;
        s32 count;
        id    = rec->itemId;
        x     = prompt->rowTextX.signedValue;
        y     = prompt->rowTextY.signedValue;
        color = prompt->colorRgb;
        if ((u32)(id - 0xA0) < 0x20U) {
            count                   = rec->qty - equipmentGetLoadedConsumableQuantity(&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems, id);
            draw.qty.req.x          = obj->panel.contentOriginX.unsignedValue + 0x84 + x;
            off                     = obj->panel.contentOriginY.unsignedValue - 3;
            draw.qty.req.y          = off + y;
            draw.qty.req.otIndex    = obj->panel.otIndex.signedValue + 1;
            draw.qty.req.colorRgb   = color;
            draw.qty.req.glyphTable = TEXT_GLYPH_TABLE_SMALL;
            draw.qty.req.alignment  = TEXT_ALIGNMENT_RIGHT;
            draw.qty.req.drawMode   = TEXT_DRAW_FILL_ONLY;
            textDrawString(&draw.qty.req, textItoaSigned(draw.qty.buf, count));
            uiDrawRecessedRect(&obj->panel, x + 0x69, y - 8, 0x1B, 7, 0x102010);
        }
    }
    {
        s32 x;
        s32 y;
        s32 color;
        s32 off;
        s32 temp;
        s32 one;
        one   = 1;
        x     = prompt->rowTextX.signedValue;
        y     = prompt->rowTextY.signedValue;
        color = prompt->colorRgb;
        if (item == 0) {
            uiDrawRecessedRect(&obj->panel, x, y - 0xE, 0xE, 0xE, 0x102010);
        } else {
            if (obj->panel.state != USER_INTERFACE_PANEL_HIDDEN) {
                draw.name.x          = obj->panel.contentOriginX.unsignedValue + 0x11 + x;
                off                  = obj->panel.contentOriginY.unsignedValue - 6;
                draw.name.y          = off + y;
                draw.name.otIndex    = obj->panel.otIndex.signedValue + 1;
                draw.name.colorRgb   = color;
                draw.name.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
                draw.name.alignment  = TEXT_ALIGNMENT_LEFT;
                draw.name.drawMode   = one;
                textDrawString(&draw.name, itemGetText(item, ITEM_TEXT_NAME, 0));
                itemMenuDrawEquipmentMarker(obj, x, y, item, one);
                temp = item - 0xF;
                if ((u32)temp < 0x24U) {
                    itemMenuDrawParasiteEnergyLevel(obj, x, y, temp % 3 + 1, color);
                }
                itemMenuDrawItemIcon(obj, x, y, item, ITEM_MENU_ICON_DEFAULT);
            }
            uiDrawRecessedRect(&obj->panel, x, y - 0xE, 0xE, 0xE, 0);
        }
    }
    rowState = prompt->rowInputEnabled;
    if (rowState == 1) {
        mode = Gp_ItemOrderMode;
        if (mode == rowState) {
            if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm) != 0) {
                const ItemDesc* desc;
                desc = &Gp_ItemDescs[Gp_SelItemRec->itemId];
                if (!(desc->flags & ITEM_FLAG_NO_ATTACHMENT)) {
                    sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
                    if (Gp_SelItemRec->itemId != INVENTORY_ITEM_NONE) {
                        Gp_SelItemRec->attachSlot = prompt->currentItemIndex + 1;
                    }
                    if (item != 0) {
                        inventoryDetachItem(rec);
                    }
                    Gp_ItemOrderMode = 0;
                } else {
                    parent = obj->owner->parent;
                    if (parent != NULL) {
                        parentObj                     = parent->spawnArg2.pointer;
                        Gp_ItemOrderMode              = 0;
                        parentObj->panel.control.word = mode;
                        obj->panel.control.word       = USER_INTERFACE_PANEL_INACTIVE;
                    }
                }
            }
        } else {
            if (rec == NULL) {
                if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm) != 0) {
                    sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
                    child = uiSpawnObject(&D_8010EAB4[21], 0, 1, 0x10, obj);
                    if (child != NULL) {
                        s32 yOffset;
                        s32 xOffset;
                        yOffset                            = -0x5C;
                        child->panel.bounds.unsignedRect.y = yOffset;
                        xOffset                            = -8;
                        child->panel.bounds.unsignedRect.x = xOffset;
                    }
                    obj->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
                }
                Gp_SelItemRec = rec;
            } else {
                Gp_SelItemRec = rec;
                if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm) != 0) {
                    sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
                    dialog = uiSpawnObject(&D_8010EAB4[34], 4, 1, 1, obj);
                    if (dialog != NULL) {
                        uiPositionRowDialog(&(dialog)->panel, prompt, &(obj)->panel);
                        obj->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
                    }
                } else {
                    itemMenuOpenInfoOnTriangle(obj);
                }
            }
        }
    }
}

void Gp_ArmorMenuTask(Task* arg0)
{
    UiObject*     obj;
    UiList*       menu;
    s32           item;
    u32           textColorRgb;
    PlayerStatus* cfg;
    s32           status;
    s32           x;
    s32           y;
    struct {
        TextDrawReq      req;
        UiCursorPosition cursor;
        s32              pad0;
        s16              x;
        s16              y;
        s32              pad[2];
    } locals;

    menu        = &D_8010E8AC;
    obj         = arg0->spawnArg2.pointer;
    cfg         = &gPlayerStatus;
    obj->result = USER_INTERFACE_RESULT_NONE;
    uiDrawPanelLabel(&(obj)->panel, Gp_StrArmor);

    if (arg0->state == 0) {
        s32 id;
        s32 temp;

        id = cfg->armor + 0x5F;
        if (id != 0) {
            menu->itemCount = equipmentGetArmorAttachmentSlotCount(id);
        }
        menu->visibleRowCount.unsignedValue = menu->itemCount;
        if ((s8)menu->itemCount >= 4) {
            menu->visibleRowCount.unsignedValue = 3;
        }
        if ((menu->itemCount - menu->visibleRowCount.signedValue) < menu->firstVisibleItemIndex.signedValue) {
            menu->firstVisibleItemIndex.unsignedValue = 0;
        }
        if (menu->scrollPixelsRemaining == 0) {
            temp = menu->firstVisibleItemIndex.signedValue + menu->visibleRowCount.signedValue;
            if (menu->selectedItemIndex >= temp) {
                menu->selectedItemIndex = temp - 1;
            }
        }
        uiInitList(menu, &(obj)->panel);
        menu->topInset = 0x1A;
        menu->flags    = USER_INTERFACE_LIST_SHARED_ROW_CALLBACK;
        arg0->state    = arg0->state + 2;
    }

    item = cfg->armor;
    if (item > 0) {
        item += 0x5F;
    }
    {
        s32 id;
        s32 temp;

        id = gPlayerStatus.armor + 0x5F;
        if (id != 0) {
            menu->itemCount = equipmentGetArmorAttachmentSlotCount(id);
        }
        {
            u8 n;
            n                                   = menu->itemCount;
            menu->visibleRowCount.unsignedValue = n;
            if ((s8)n >= 4) {
                menu->visibleRowCount.unsignedValue = 3;
            }
        }
        if ((menu->itemCount - menu->visibleRowCount.signedValue) < menu->firstVisibleItemIndex.signedValue) {
            menu->firstVisibleItemIndex.unsignedValue = 0;
        }
        if (menu->scrollPixelsRemaining == 0) {
            temp = menu->firstVisibleItemIndex.signedValue + menu->visibleRowCount.signedValue;
            if (menu->selectedItemIndex >= temp) {
                menu->selectedItemIndex = temp - 1;
            }
        }
        uiRefreshListViewport(menu, &(obj)->panel);
        menu->topInset = 0x1A;
        menu->flags    = USER_INTERFACE_LIST_SHARED_ROW_CALLBACK;
    }

    if ((Gp_ItemCountShow == 1) && (uiIsPanelHidingOrHidden(obj) == 0)) {
        uiStartPanelHiding(obj, obj->owner);
    } else if ((Gp_ItemCountShow == 0) && (uiIsPanelHidingOrHidden(obj) == 1)) {
        uiLimitHiddenDelayOrOpen(&(obj)->panel, obj->owner, 0x10);
    }

    textColorRgb = uiGetTextColor(obj, USER_INTERFACE_TEXT_COLOR_NORMAL);

    if (arg0->state == 2) {
        {
            s32 t;
            s32 one;
            t   = obj->panel.control.word;
            one = 1;
            if (((t >> 16) == one) || (t == one)) {
                if (Gp_ItemOrderMode == 0) {
                    if (item == 0) {
                        uiSetPromptText(Gp_StrEmpty, 0, 0);
                    } else {
                        uiSetPromptText(itemGetText(item, ITEM_TEXT_DESCRIPTION_FIRST, 0), 0, 0);
                    }
                    itemMenuSetPreviewItem(item, CD_COMMAND_DISPLAY_LOAD_MENU);
                } else {
                    uiSetPromptText(Gp_StrSelectDest, 0, 0);
                }
            }
        }
        status                  = obj->panel.control.word;
        obj->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
        uiUpdateList(menu, &obj->panel);
        obj->panel.control.word = status;
        if (status == 1) {
            uiEaseAndDrawCursor(&(obj)->panel, obj->panel.contentLeft.signedValue, obj->panel.contentTop.signedValue + 7);
            if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_DOWN) != 0) {
                sndEvtRequestScriptStart(SOUND_MENU_CURSOR, 0, 0);
                arg0->state             = status;
                menu->selectedItemIndex = menu->firstVisibleItemIndex.signedValue;
            } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_UP) != 0) {
                sndEvtRequestScriptStart(SOUND_MENU_CURSOR, 0, 0);
                D_80114D98[0]->panel.control.word = status;
                D_8010E884.selectedItemIndex      = D_8010E884.itemCount - 1;
                obj->panel.control.word           = USER_INTERFACE_PANEL_INACTIVE;
            } else {
                s32 flag;
                flag = Gp_ItemOrderMode;
                if (flag == 0) {
                    InventoryItemRange* scan;
                    InventoryItemRow*   table;
                    s32                 i;

                    scan  = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems;
                    table = inventoryGetRangeTable(scan);
                    table = &table[scan->firstRow];
                    for (i = 0; i < scan->rowCount; i++, table++) {
                        if (table->itemId == item) {
                            locals.x      = obj->panel.contentLeft.signedValue + 2;
                            Gp_SelItemRec = table;
                            locals.y      = obj->panel.contentTop.unsignedValue + 0xF;
                            if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm) != 0) {
                                sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
                                itemMenuSizeUnequippedArmorList(&D_8010E9F4, obj);
                                if (D_8010E9F4.itemCount != 0) {
                                    UiObject* spawned;
                                    sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
                                    spawned = uiSpawnObject(&D_8010EAB4[18], 0, 1, 0x10, obj);
                                    if (spawned != NULL) {
                                        s32 yOffset;
                                        s32 xOffset;
                                        yOffset                              = -0x5C;
                                        spawned->panel.bounds.unsignedRect.y = yOffset;
                                        xOffset                              = -8;
                                        spawned->panel.bounds.unsignedRect.x = xOffset;
                                    }
                                    obj->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
                                } else {
                                    itemMenuSpawnNotice(obj, ITEM_MENU_NOTICE_NO_OTHER_ARMOR, 0, ITEM_MENU_NOTICE_RESULT_CONFIRM);
                                    obj->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
                                }
                            } else {
                                itemMenuOpenInfoOnTriangle(obj);
                            }
                            break;
                        }
                    }
                } else if ((flag == status) && (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm) != 0)) {
                    if ((u32)(Gp_SelItemRec->itemId - 0x60) < 0x20U) {
                        sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
                        equipmentEquipCarriedArmor(Gp_SelItemRec->itemId);
                        itemSetIdentified(Gp_SelItemRec->itemId, 1);
                        Gp_ItemOrderMode = 0;
                    } else {
                        Task* parent;
                        parent = arg0->parent;
                        if (parent != 0) {
                            UiObject* po;
                            po                      = parent->spawnArg2.pointer;
                            Gp_ItemOrderMode        = 0;
                            po->panel.control.word  = flag;
                            obj->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
                        }
                    }
                }
            }
        }
    } else {
        s32 val;
        uiUpdateList(menu, &obj->panel);
        val = menu->actionResult;
        if (val == USER_INTERFACE_LIST_ACTION_AT_START) {
            sndEvtRequestScriptStart(SOUND_MENU_CURSOR, 0, 0);
            arg0->state = val;
        }
    }

    x = obj->panel.contentLeft.signedValue + 2;
    y = obj->panel.contentTop.signedValue + 0xF;
    if (obj->panel.state != USER_INTERFACE_PANEL_HIDDEN) {
        s32 off;
        s32 temp;
        locals.req.x          = obj->panel.contentOriginX.unsignedValue + 0x11 + x;
        off                   = obj->panel.contentOriginY.unsignedValue - 6;
        locals.req.y          = off + y;
        locals.req.otIndex    = obj->panel.otIndex.signedValue + 1;
        locals.req.colorRgb   = textColorRgb;
        locals.req.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
        locals.req.alignment  = TEXT_ALIGNMENT_LEFT;
        locals.req.drawMode   = TEXT_DRAW_OUTLINED;
        textDrawString(&locals.req, itemGetText(item, ITEM_TEXT_NAME, 0));
        temp = item - 0xF;
        if ((u32)temp < 0x24U) {
            itemMenuDrawParasiteEnergyLevel(obj, x, y, temp % 3 + 1, textColorRgb);
        }
        itemMenuDrawItemIcon(obj, x, y, item, ITEM_MENU_ICON_DEFAULT);
    }

    if ((obj->panel.control.word == USER_INTERFACE_PANEL_ACTIVE) && (arg0->state == 2)) {
        obj->panel.otIndex.signedValue += 1;
        uiFillRectInterior(&(obj)->panel, obj->panel.contentLeft.signedValue, obj->panel.contentTop.signedValue,
                           (obj->panel.contentRight.signedValue - obj->panel.contentLeft.signedValue) - 1, 0x10, 0x1741FU);
        obj->panel.otIndex.signedValue -= 1;
    }

    uiDrawHorizontalSeparator(&(obj)->panel, obj->panel.contentLeft.signedValue, obj->panel.contentRight.signedValue, obj->panel.contentTop.signedValue + 0x11);

    {
        s32 grey;
        grey                  = 0x606060;
        locals.req.x          = obj->panel.contentOriginX.unsignedValue + 2 + obj->panel.contentLeft.signedValue;
        locals.req.y          = obj->panel.contentOriginY.unsignedValue + 0x18 + obj->panel.contentTop.unsignedValue;
        locals.req.otIndex    = obj->panel.otIndex.signedValue + 1;
        locals.req.colorRgb   = grey;
        locals.req.glyphTable = TEXT_GLYPH_TABLE_SMALL;
        locals.req.alignment  = TEXT_ALIGNMENT_LEFT;
        locals.req.drawMode   = TEXT_DRAW_OUTLINED;
        textDrawString(&locals.req, Gp_StrAttachments2);
    }

    {
        s32 st;
        st = obj->panel.control.word;
        if (st == 1) {
            if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskMenu) != 0) {
                obj->result = USER_INTERFACE_RESULT_CANCEL;
            } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskCancel) != 0) {
                Task*     parent;
                UiObject* parentObj;
                sndEvtRequestScriptStart(SOUND_MENU_CANCEL, 0, 0);
                parent = arg0->parent;
                if (parent != 0) {
                    parentObj = parent->spawnArg2.pointer;
                    if (Gp_ItemOrderMode == 0) {
                        parentObj->resultValue = st;
                        parentObj->result      = USER_INTERFACE_RESULT_CONFIRM;
                    } else {
                        parentObj->panel.control.word = st;
                        obj->panel.control.word       = USER_INTERFACE_PANEL_INACTIVE;
                        Gp_ItemOrderMode              = 0;
                    }
                }
            } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_RIGHT) != 0) {
                Task*     parent;
                UiObject* parentObj;
                parent = arg0->parent;
                if (parent != 0) {
                    UiList* other;
                    s16     row;

                    parentObj             = parent->spawnArg2.pointer;
                    *(s32*)&locals.cursor = uiGetCursorPositionWord();
                    sndEvtRequestScriptStart(SOUND_MENU_CURSOR, 0, 0);
                    other                    = &D_8010E854;
                    row                      = locals.cursor.y.unsignedValue - (parentObj->panel.contentOriginY.unsignedValue + parentObj->panel.contentTop.unsignedValue);
                    row                      = row / other->rowHeight;
                    other->selectedItemIndex = row + other->firstVisibleItemIndex.signedValue;
                    _itemMenuClampArmorSelection(other, other->firstVisibleItemIndex.signedValue + other->visibleRowCount.signedValue);
                    parentObj->panel.control.word = st;
                    obj->panel.control.word       = USER_INTERFACE_PANEL_INACTIVE;
                }
            }
        }
    }

    if (obj->panel.control.word == USER_INTERFACE_PANEL_FOCUS_TRANSFER) {
        s32 t;

        t = obj->resultValue - (obj->panel.contentOriginY.signedValue + obj->panel.contentTop.signedValue);
        if (t < 0xF) {
            arg0->state = 2;
        } else {
            s32 h;

            arg0->state = 1;
            h           = menu->rowHeight;
            t          -= h * 2 + 0xA;
            if (t < 0) {
                menu->selectedItemIndex = menu->firstVisibleItemIndex.signedValue;
            } else {
                t                       = t / h;
                t                       = t + 1;
                menu->selectedItemIndex = t + menu->firstVisibleItemIndex.signedValue;
                _itemMenuClampArmorSelection(menu, menu->firstVisibleItemIndex.signedValue + menu->visibleRowCount.signedValue);
            }
        }
        obj->resultValue        = 0;
        obj->panel.control.word = USER_INTERFACE_PANEL_ACTIVE;
    }

    {
        Task*     owner;
        Task*     head;
        Task*     child;
        Task*     next;
        UiObject* childObj;
        s32       one;
        s32       mask;
        s32       flag;

        owner = obj->owner;
        head  = owner->firstChild;
        if (head != NULL) {
            child = head;
            one   = 1;
            mask  = (u32)~USER_INTERFACE_PANEL_DIMMED;
            do {
                childObj = child->spawnArg2.pointer;
                flag     = childObj->result;
                next     = child->nextSibling;
                switch (flag) {
                    case USER_INTERFACE_RESULT_CANCEL:
                        obj->result = flag;
                        break;
                    case USER_INTERFACE_RESULT_CONFIRM:
                        uiStartTreeClosing(childObj, childObj->owner);
                        obj->panel.control.word = one;
                        obj->panel.style       &= mask;
                        break;
                    case 0x23:
                        uiStartTreeClosing(childObj, childObj->owner);
                        obj->panel.control.word = one;
                        Gp_ItemOrderMode        = one;
                        obj->panel.style       &= mask;
                        break;
                }
                head  = owner->firstChild;
                child = next;
                if (child == head) {
                    break;
                }
            } while (head != NULL);
        }
    }
}

/// Returns 1 for the equipped armor/weapon or either selected weapon consumable.
///
/// Item ids use the inventory domains. Selected consumables count even when
/// their loaded quantity is zero; no equipment or save state is changed.
static inline s32 _equipmentIsActiveItem(s32 itemId)
{
    s32           active;
    PlayerStatus* player;

    active = 0;
    player = &gPlayerStatus;
    EQUIPMENT_CHECK_ACTIVE_ITEM(active, player, itemId);
    return active;
}

#undef EQUIPMENT_CHECK_ACTIVE_ITEM

InventoryItemRow* inventoryFindNthAttachmentCandidate(const InventoryItemRange* range, s32 choiceIndex, s32 unused)
{
    InventoryItemRow* row;
    s32               rowIndex;
    InventoryItemRow* foundRow;

    row      = inventoryGetRangeTable(range);
    foundRow = NULL;
    row      = &row[range->firstRow];
    for (rowIndex = 0; rowIndex < range->rowCount; rowIndex++, row++) {
        if ((Gp_ItemDescs[row->itemId].flags & ITEM_FLAG_NO_ATTACHMENT) || (row->itemId == INVENTORY_ITEM_NONE)) {
            continue;
        }
        if ((u8)(row->itemId + EQUIPMENT_WEAPON_ITEM_FIRST) < ITEM_MENU_WEAPON_ITEM_COUNT && _equipmentIsActiveItem(row->itemId)) {
            continue;
        }
        choiceIndex--;
        if (choiceIndex < 0) {
            foundRow = row;
            break;
        }
    }
    return foundRow;
}
