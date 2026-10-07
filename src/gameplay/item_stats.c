#include "gameplay/item_menu.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>

#include "common.h"
#include "gte.h"

#include "attachments.h"
#include "gameplay/attachment_state.h"
#include "gameplay/inventory.h"
#include "inventory.h"
#include "item_menu.h"
#include "gameplay/items.h"
#include "items.h"
#include "gameplay/message.h"
#include "weapon_data.h"

#include "main/display.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/text.h"
#include "main/ui.h"

/// Pixel size of the item preview picture, laid out as a GTE short vector so
/// `gte_gpf12` can scale both dimensions in one pass.
typedef struct {
    u16 width;  // on-screen width in pixels
    u16 height; // on-screen height in pixels
    u16 depth;  // third GTE lane; always 0, so scaling leaves it 0
} _ItemMenuPreviewSize;

/// Item-information layout, colors, catalogue ranges and task phases.
enum {
    ITEM_MENU_PANEL_TEXT_COLOR             = 0x606060,
    ITEM_MENU_INFO_BONUS_COLOR             = 0x808008,
    ITEM_MENU_INFO_SPECIAL_COLOR           = 0x0D287F,
    ITEM_MENU_INFO_METADATA_LINE_COUNT     = 5,
    ITEM_MENU_INFO_VISIBLE_ROW_LIMIT       = 6,
    ITEM_MENU_INFO_MINIMUM_ORDINARY_ROWS   = 2,
    ITEM_MENU_INFO_ARMOR_ITEM_FIRST        = 0x60,
    ITEM_MENU_INFO_EQUIPMENT_ITEM_COUNT    = 0x20,
    ITEM_MENU_INFO_FEATURE_BIT_COUNT       = 13,
    ITEM_MENU_INFO_FEATURE_ROW_LIMIT       = 2,
    ITEM_MENU_INFO_REPLACEMENT_OPEN_TICKS  = 20,
    ITEM_MENU_INFO_STATE_INITIALIZE        = 0,
    ITEM_MENU_INFO_STATE_WAIT_FOR_LOAD     = 2,
    ITEM_MENU_INFO_STATE_LAYOUT_READY      = 3,
    ITEM_MENU_INFO_STATE_DISPLAY           = 4,
    ITEM_MENU_PARASITE_ENERGY_ITEM_FIRST   = 15,
    ITEM_MENU_PARASITE_ENERGY_ITEM_COUNT   = ATTACHMENT_SPELL_COUNT * ATTACHMENT_AREA_LEVEL_COUNT,
    ITEM_MENU_NOTICE_DURATION_TICKS        = 188,
    ITEM_MENU_COMPLETED_COUNTDOWN          = 0x7FFF,
    ITEM_MENU_KEY_ITEM_USE_REFUSED_ID      = -1,
    ITEM_MENU_KEY_ITEM_USE_STATE_REQUEST   = 0,
    ITEM_MENU_KEY_ITEM_USE_STATE_NO_NOTICE = 2,
    ITEM_MENU_KEY_ITEM_USE_HIDE_TICKS      = 100,
    ITEM_MENU_KEY_ITEM_COMMAND_WIDTH       = 96
};

/// Preview texture pages, palettes, frame color and unsigned 12-fractional-bit scales.
enum {
    ITEM_MENU_PREVIEW_MENU_TEXTURE_PAGE   = 0x87,
    ITEM_MENU_PREVIEW_DIRECT_TEXTURE_PAGE = 0x8F,
    ITEM_MENU_PREVIEW_PALETTE             = 0x3F40,
    ITEM_MENU_PREVIEW_RELOCATED_PALETTE   = 0x3F80,
    ITEM_MENU_PREVIEW_RECESSED_COLOR      = 0x081008,
    ITEM_MENU_PREVIEW_EQUIPMENT_SCALE_12  = 0xA00,
    ITEM_MENU_PREVIEW_SHOP_SCALE_12       = 0xAA0
};

/// Additional catalogue ids used by the information panel's identification policy.
enum {
    ITEM_MENU_INFO_ITEM_PIERCE_MEMO      = 0x125,
    ITEM_MENU_INFO_ITEM_AERIS_MAGAZINE   = 0x127,
    ITEM_MENU_INFO_ITEM_DOUGLAS_LETTER   = 0x128,
    ITEM_MENU_INFO_ITEM_MICRO_DEVICE     = 0x129,
    ITEM_MENU_INFO_WEAPON_M93R           = 0x81,
    ITEM_MENU_INFO_WEAPON_M4A1           = 0x8F,
    ITEM_MENU_INFO_WEAPON_M4A1_UPGRADE_1 = 0x93,
    ITEM_MENU_INFO_WEAPON_M4A1_UPGRADE_2 = 0x94,
    ITEM_MENU_INFO_WEAPON_GUNBLADE       = 0x96,
    ITEM_MENU_INFO_WEAPON_M4A1_BAYONET   = 0x99
};

/// Scales a writable `_ItemMenuPreviewSize` in place using a GTE 4.12 factor.
///
/// size is evaluated twice and scale12 once; supply stable pointer/value
/// expressions without side effects. Clobbers GTE IR0..3, MAC1..3 and FLAG.
#define ITEM_MENU_SCALE_PREVIEW_SIZE(size, scale12) \
    do {                                            \
        gte_lddp(scale12);                          \
        gte_ldsv(size);                             \
        gte_gpf12();                                \
        gte_stsv(size);                             \
    } while (0)

#define D_8010EF68 D_8010EAB4[43]

WeaponAttackRow Gp_IdParamLo[47] = {
    { 0, 0, 0, 0, 0 },
    { 10, 0, 0, 1, 0 },
    { 15, 0, 0, 1, 0 },
    { 20, 0, 0, 1, 0 },
    { 9999, 0, 0, 0, 0 },
    { 9999, 0, 0, 0, 0 },
    { 9999, 0, 0, 0, 0 },
    { 40, 0, 0, 1, 0 },
    { 70, 0, 3, 2, 0 },
    { 999, 0, 3, 2, 0 },
    { 270, 0, 4, 3, 10 },
    { 220, 0, 6, 4, 3 },
    { 60, 0, 9, 0, 10 },
    { 40, 0, 6, 4, 0 },
    { 70, 0, 7, 3, 0 },
    { 90, 0, 5, 5, 0 },
    { 22, 0, 0, 1, 0 },
    { 1, 0, 2, 0, 0 },
    { 1, 0, 9, 0, 0 },
    { 9999, 0, 0, 0, 0 },
    { 9999, 0, 0, 0, 0 },
    { 10, 0, 0, 6, 11 },
    { 0, 0, 8, 0, 0 },
    { 10, 0, 0, 6, 8 },
    { 1500, 0, 6, 4, 9 },
    { 2000, 0, 7, 3, 9 },
    { 2000, 0, 1, 7, 10 },
    { 100, 0, 1, 8, 9 },
    { 80, 0, 1, 15, 0 },
    { 90, 0, 0, 8, 10 },
    { 35, 0, 7, 16, 5 },
    { 25, 0, 0, 10, 5 },
    { 2500, 0, 5, 5, 9 },
    { 60, 0, 6, 4, 2 },
    { 80, 0, 6, 4, 2 },
    { 100, 0, 6, 4, 2 },
    { 15, 0, 7, 3, 5 },
    { 30, 0, 1, 0, 1 },
    { 45, 0, 1, 10, 0 },
    { 80, 0, 1, 10, 0 },
    { 45, 0, 2, 7, 0 },
    { 45, 0, 1, 10, 0 },
    { 12, 0, 0, 1, 0 },
    { 40, 0, 1, 1, 1 },
    { 60, 0, 1, 1, 1 },
    { 60, 0, 7, 3, 1 },
    { 100, 0, 7, 3, 1 },
};

/// Draws an item's name, icon and ordinary P.E. level without an equipment mark.
///
/// Uses `itemMenuDrawItemRow`'s panel-relative pixels and catalogue/GPU contract.
/// Borrows object and fills the caller's writable text request for this draw;
/// hidden panels leave the request untouched. colorRgb is packed 24-bit RGB.
static inline void _itemMenuDrawUnmarkedItemRowIntoRequest(const UiObject* object, TextDrawReq* request, s32 x, s32 y, s32 colorRgb,
                                                           s32 itemId)
{
    s32 energyLevelIndex;

    if (object->panel.state != USER_INTERFACE_PANEL_HIDDEN) {
        request->x          = object->panel.contentOriginX.unsignedValue + 0x11 + x;
        request->y          = object->panel.contentOriginY.unsignedValue + (y - 6);
        request->otIndex    = object->panel.otIndex.signedValue + 1;
        request->colorRgb   = colorRgb;
        request->glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
        request->alignment  = TEXT_ALIGNMENT_LEFT;
        request->drawMode   = TEXT_DRAW_OUTLINED;
        textDrawString(request, itemGetText(itemId, ITEM_TEXT_NAME, 0));
        energyLevelIndex = itemId - ITEM_MENU_PARASITE_ENERGY_ITEM_FIRST;
        if ((u32)energyLevelIndex < (u32)ITEM_MENU_PARASITE_ENERGY_ITEM_COUNT) {
            itemMenuDrawParasiteEnergyLevel(object, x, y, energyLevelIndex % ATTACHMENT_AREA_LEVEL_COUNT + 1, colorRgb);
        }
        itemMenuDrawItemIcon(object, x, y, itemId, ITEM_MENU_ICON_DEFAULT);
    }
}

/// Counts the loaded caption's description rows for the information viewport.
///
/// The five metadata lines precede description text, ending at NUL or \Z.
/// Ordinary items reserve at least two rows for their extra specifications.
static inline s32 _itemMenuCountInfoDescriptionRows(s32 itemId)
{
    s32       lineCount;
    const u8* cursor;
    lineCount = 0;
    cursor    = textSkipLines(fsGetChunkPayload(), ITEM_MENU_INFO_METADATA_LINE_COUNT);
    while (*cursor != 0) {
        if (*cursor == '\\') {
            cursor++;
            if (*cursor == 'Z' || *cursor == 'z') {
                break;
            }
        }
        if (*cursor == '\n') {
            lineCount++;
        }
        cursor++;
    }
    if (itemId < ITEM_TEXT_KEY_ID_FIRST) {
        if (lineCount < ITEM_MENU_INFO_MINIMUM_ORDINARY_ROWS) {
            lineCount = ITEM_MENU_INFO_MINIMUM_ORDINARY_ROWS;
        }
    }
    return lineCount;
}

void itemMenuInfoTask(Task* task)
{
    TextDrawReq            metadataRequest;
    TextDrawReq            labelRequest;
    u8                     bonusText[0x20];
    TextDrawReq            armorBonusRequest;
    TextDrawReq            armorDetailsRequest;
    TextDrawReq            attachmentCountRequest;
    TextDrawReq            featureLabelRequest;
    TextDrawReq            featureNameRequest;
    TextDrawReq            categoryRequest;
    u8                     quantityText[0x20];
    u8                     stackLimitText[0x20];
    TextDrawReq            powerLabelRequest;
    TextDrawReq            powerValueRequest;
    TextDrawReq            capacityLabelRequest;
    TextDrawReq            capacityValueRequest;
    TextDrawReq            extraLabelRequest;
    TextDrawReq            specialValueRequest;
    s32                    resourcesReady;
    u32                    previewFlags;
    u32                    armorFeatures;
    UiList*                descriptionList;
    UiObject*              object;
    s32                    itemId;
    s32                    displayedFeatureCount;
    s32                    bonusColorRgb;
    s32                    descriptionLineCount;
    const u8*              captionPayload;
    s32                    rowY;
    s32                    metadataLineIndex;
    s32                    weaponButtonCount;
    s32                    buttonGlyphHeight;
    s32                    rowX;
    SPRT*                  buttonGlyph;
    s32                    savedPanelControl;
    const ArmorStats*      armorStats;
    u8**                   featureName;
    const WeaponAttackRow* attack;
    const WeaponAttackRow* attackTable;
    s32                    consumableIndex;
    s32                    infoArgument;
    s32                    caliberIndex;
    s32                    attackIndex;
    s32                    buttonGlyphWidth;
    s32                    textColorRgb;
    s32                    featureIndex;
    s32                    weaponSlot;
    s32                    contentTop;
    s32                    buttonLayout;

    resourcesReady  = 0;
    previewFlags    = resourcesReady;
    descriptionList = &D_8010E910;
    infoArgument    = task->spawnArg1.value;
    object          = task->spawnArg2.pointer;
    itemId          = infoArgument & ITEM_MENU_INFO_ITEM_ID_MASK;
    if (infoArgument & ITEM_MENU_INFO_RELOCATED_PREVIEW) {
        previewFlags = ITEM_MENU_PREVIEW_TEXTURE_RELOCATED;
    } else if (infoArgument & ITEM_MENU_INFO_UNRELOCATED_PREVIEW) {
        previewFlags = ITEM_MENU_PREVIEW_TEXTURE_DIRECT;
    }
    object->result = USER_INTERFACE_RESULT_NONE;
    if (task->state == ITEM_MENU_INFO_STATE_INITIALIZE) {
        // Publish this panel and apply its item-identification side effects once.
        if ((D_80067634 != NULL) && (D_80067634 != object)) {
            taskCallExit(D_80067634->owner);
            object->panel.animationTicks = ITEM_MENU_INFO_REPLACEMENT_OPEN_TICKS;
        }
        D_80067634         = object;
        task->exitCallback = itemMenuInfoTaskExit;
        task->state        = ITEM_MENU_INFO_STATE_WAIT_FOR_LOAD;
        if ((itemId < ITEM_TEXT_KEY_ID_FIRST) || (itemId == INVENTORY_COLLECTION_ID_DRYFIELD_MAP) || (itemId == INVENTORY_COLLECTION_ID_BRONCO_MASTERKEY) || (itemId == INVENTORY_COLLECTION_ID_WIRE_ROPE) ||
            (itemId == INVENTORY_COLLECTION_ID_BOTTLECAP_MAGNET) || (itemId == INVENTORY_COLLECTION_ID_OAK_BOARD) || (itemId == ITEM_MENU_INFO_ITEM_PIERCE_MEMO) || (itemId == ITEM_MENU_INFO_ITEM_AERIS_MAGAZINE) ||
            (itemId == ITEM_MENU_INFO_ITEM_DOUGLAS_LETTER) || (itemId == ITEM_MENU_INFO_ITEM_MICRO_DEVICE) || (itemId == INVENTORY_COLLECTION_ID_MENDEL_JOURNAL)) {
            itemSetIdentified(itemId, 1);
        }
        if (itemId == ITEM_MENU_INFO_ITEM_PIERCE_MEMO) {
            gameFlagSetNibble(GAME_FLAG_ITEM_125_EXAMINED, 1);
        }
    } else {
        if (task->spawnArg1.value & ITEM_MENU_INFO_NEXT_REPLAY) {
            uiDrawPanelLabel(&object->panel, Gp_StrNextReplay);
        } else {
            uiDrawPanelLabel(&object->panel, Gp_StrSpecs);
        }
        if (object->panel.state == USER_INTERFACE_PANEL_OPEN) {
            displaySetFrameTiming(DISPLAY_TIMING_TWO_VBLANKS);
        }
        switch (task->state) {
            case ITEM_MENU_INFO_STATE_WAIT_FOR_LOAD:
                if (cdCmdIsIdle()) {
                    descriptionLineCount                                 = _itemMenuCountInfoDescriptionRows(itemId);
                    descriptionList->selectedItemIndex                   = 0;
                    descriptionList->firstVisibleItemIndex.unsignedValue = 0;
                    descriptionList->visibleRowCount.unsignedValue       = descriptionLineCount;
                    descriptionList->itemCount                           = descriptionLineCount;
                    uiInitList(descriptionList, &object->panel);
                    if (descriptionList->visibleRowCount.signedValue >= ITEM_MENU_INFO_VISIBLE_ROW_LIMIT + 1) {
                        descriptionList->visibleRowCount.unsignedValue = ITEM_MENU_INFO_VISIBLE_ROW_LIMIT;
                    }
                    descriptionList->flags    = USER_INTERFACE_LIST_SHARED_ROW_CALLBACK;
                    descriptionList->topInset = -(u8)object->panel.contentTop.unsignedValue + 7;
                    task->state               = ITEM_MENU_INFO_STATE_LAYOUT_READY;
                }
                break;
            case ITEM_MENU_INFO_STATE_LAYOUT_READY:
                task->state = ITEM_MENU_INFO_STATE_DISPLAY;
                /* fallthrough */
            case ITEM_MENU_INFO_STATE_DISPLAY:
                if (cdCmdIsIdle()) {
                    resourcesReady = 1;
                }
                break;
            // A fourth case below 2 is in the compare tree; which one is not
            // recoverable from the bytes.
            case 1:
            default:
                break;
        }
        if (itemId >= ITEM_TEXT_ENEMY_ID_FIRST) {
            previewFlags |= ITEM_MENU_PREVIEW_TALL;
            uiDrawHorizontalSeparator(&object->panel, object->panel.contentLeft.signedValue, object->panel.contentRight.signedValue, 0x25);
        } else {
            uiDrawHorizontalSeparator(&object->panel, object->panel.contentLeft.signedValue, object->panel.contentRight.signedValue, 5);
        }
        if (resourcesReady == 1) {
            // Loaded captions supply metadata; the description viewport follows it.
            rowX           = 2;
            captionPayload = fsGetChunkPayload();
            rowY           = object->panel.contentTop.signedValue + 0x1E;
            for (metadataLineIndex = 0; metadataLineIndex < ITEM_MENU_INFO_METADATA_LINE_COUNT; metadataLineIndex++) {
                metadataRequest.x          = object->panel.contentOriginX.unsignedValue + rowX;
                metadataRequest.y          = object->panel.contentOriginY.unsignedValue + rowY;
                rowY                      += 0xF;
                metadataRequest.otIndex    = object->panel.otIndex.signedValue + 1;
                metadataRequest.colorRgb   = ITEM_MENU_PANEL_TEXT_COLOR;
                metadataRequest.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
                metadataRequest.alignment  = TEXT_ALIGNMENT_LEFT;
                metadataRequest.drawMode   = TEXT_DRAW_OUTLINED;
                textDrawString(&metadataRequest, textSkipLines(captionPayload, metadataLineIndex));
            }
            if (itemId >= ITEM_TEXT_ENEMY_ID_FIRST) {
                textDrawUiLines(object, object->panel.contentLeft.signedValue + 2, 0x34, textSkipLines(captionPayload, ITEM_MENU_INFO_METADATA_LINE_COUNT),
                                ITEM_MENU_PANEL_TEXT_COLOR, TEXT_DRAW_TRANSLUCENT_OUTLINED, TEXT_ALIGNMENT_LEFT);
            } else {
                // Draw scrolling rows without letting the list consume panel input.
                savedPanelControl          = object->panel.control.word;
                object->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
                uiUpdateList(descriptionList, &object->panel);
                object->panel.control.word = savedPanelControl;
                if ((savedPanelControl == USER_INTERFACE_PANEL_ACTIVE) && (descriptionList->itemCount > descriptionList->visibleRowCount.signedValue)) {
                    if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_UP) != 0) {
                        descriptionList->firstVisibleItemIndex.unsignedValue = descriptionList->firstVisibleItemIndex.unsignedValue - 1;
                        if (descriptionList->firstVisibleItemIndex.signedValue < 0) {
                            descriptionList->firstVisibleItemIndex.unsignedValue = 0;
                        } else {
                            descriptionList->scrollDirection       = USER_INTERFACE_LIST_STEP_PREVIOUS;
                            descriptionList->scrollPixelsRemaining = descriptionList->rowHeight;
                        }
                        descriptionList->selectedItemIndex = descriptionList->firstVisibleItemIndex.signedValue;
                    } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_DOWN) != 0) {
                        descriptionList->selectedItemIndex = descriptionList->firstVisibleItemIndex.signedValue + descriptionList->visibleRowCount.signedValue;
                        if (descriptionList->selectedItemIndex < descriptionList->itemCount) {
                            descriptionList->scrollDirection       = savedPanelControl;
                            descriptionList->scrollPixelsRemaining = descriptionList->rowHeight;
                        } else {
                            descriptionList->selectedItemIndex = descriptionList->itemCount - 1;
                        }
                    } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_L1) != 0) {
                        if (descriptionList->firstVisibleItemIndex.signedValue > 0) {
                            descriptionList->firstVisibleItemIndex.unsignedValue = descriptionList->firstVisibleItemIndex.unsignedValue - descriptionList->visibleRowCount.unsignedValue;
                            if (descriptionList->firstVisibleItemIndex.signedValue < 0) {
                                descriptionList->firstVisibleItemIndex.unsignedValue = 0;
                            }
                            descriptionList->selectedItemIndex = descriptionList->firstVisibleItemIndex.signedValue;
                        }
                    } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_R1) != 0) {
                        if (descriptionList->firstVisibleItemIndex.signedValue < (descriptionList->itemCount - descriptionList->visibleRowCount.signedValue)) {
                            descriptionList->firstVisibleItemIndex.unsignedValue += descriptionList->visibleRowCount.unsignedValue;
                            if (descriptionList->firstVisibleItemIndex.signedValue > (descriptionList->itemCount - descriptionList->visibleRowCount.signedValue)) {
                                descriptionList->firstVisibleItemIndex.unsignedValue = descriptionList->itemCount - descriptionList->visibleRowCount.unsignedValue;
                            }
                            descriptionList->selectedItemIndex = (descriptionList->firstVisibleItemIndex.signedValue + descriptionList->visibleRowCount.signedValue) - 1;
                        }
                    }
                }
            }
            itemMenuDrawPreview(object, object->panel.contentLeft.signedValue + 2, object->panel.contentTop.signedValue + 2, previewFlags);
            if ((u32)(itemId - EQUIPMENT_WEAPON_ITEM_FIRST) < (u32)ITEM_MENU_INFO_EQUIPMENT_ITEM_COUNT) {
                // Draw the main/sub weapon buttons for the saved controller layout.
                weaponButtonCount = 1;
                if ((equipmentGetWeaponLoad(itemId)->secondaryItemId != EQUIPMENT_WEAPON_SECONDARY_UNAVAILABLE) || (itemId == ITEM_MENU_INFO_WEAPON_M4A1) || (itemId == ITEM_MENU_INFO_WEAPON_M4A1_UPGRADE_1) ||
                    (itemId == ITEM_MENU_INFO_WEAPON_M4A1_UPGRADE_2) || (itemId == ITEM_MENU_INFO_WEAPON_GUNBLADE) || (itemId == ITEM_MENU_INFO_WEAPON_M4A1_BAYONET) || (itemId == ITEM_MENU_INFO_WEAPON_M93R)) {
                    weaponButtonCount = 2;
                }
                rowY              = 0x4E;
                buttonGlyphHeight = 0xF;
                buttonGlyphWidth  = buttonGlyphHeight;
                weaponSlot        = 0;
                rowX              = object->panel.contentLeft.signedValue + 2;
                if (weaponButtonCount != 0) {
                    do {
                        buttonGlyph     = gGpuPrimCursor;
                        gGpuPrimCursor  = buttonGlyph + 1;
                        buttonGlyph->x0 = rowX;
                        buttonGlyph->y0 = rowY - 8;
                        buttonLayout    = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.buttonLayout;
                        if (buttonLayout != 2) {
                            buttonGlyphHeight = 8;
                            buttonGlyph->y0   = rowY - 4;
                            if (weaponSlot == 0) {
                                buttonGlyph->u0 = 0x90;
                                buttonGlyph->v0 = 0x58;
                            } else {
                                buttonGlyph->u0 = 0xB0;
                                buttonGlyph->v0 = 0x58;
                            }
                        } else if (weaponSlot == 0) {
                            buttonGlyph->u0 = 0x10;
                            buttonGlyph->v0 = 0x60;
                        } else {
                            buttonGlyph->u0 = 0x20;
                            buttonGlyph->v0 = 0x70;
                        }
                        buttonGlyph->clut = 0x3C00;
                        setlen(buttonGlyph, 4);
                        rowY          += 0xF;
                        buttonGlyph->w = buttonGlyphWidth;
                        buttonGlyph->h = buttonGlyphHeight;
                        setcode(buttonGlyph, 0x65);
                        addPrim(gGpuCurrentOt + object->panel.otIndex.signedValue + 1, buttonGlyph);
                        weaponSlot++;
                    } while (weaponSlot < weaponButtonCount);
                }
                uiQueueTexturePage(object->panel.otIndex.signedValue + 1, 0);
                rowX                    = object->panel.contentLeft.signedValue + 2;
                labelRequest.x          = object->panel.contentOriginX.unsignedValue + rowX;
                labelRequest.y          = object->panel.contentOriginY.unsignedValue + 0x40;
                labelRequest.otIndex    = object->panel.otIndex.signedValue + 1;
                labelRequest.glyphTable = TEXT_GLYPH_TABLE_SMALL;
                labelRequest.colorRgb   = ITEM_MENU_PANEL_TEXT_COLOR;
                labelRequest.alignment  = TEXT_ALIGNMENT_LEFT;
                labelRequest.drawMode   = TEXT_DRAW_OUTLINED;
                textDrawString(&labelRequest, (const u8*)Gp_StrOperation);
            } else if ((u32)(itemId - ITEM_MENU_INFO_ARMOR_ITEM_FIRST) < (u32)ITEM_MENU_INFO_EQUIPMENT_ITEM_COUNT) {
                armorStats            = &Gp_ModStatAttrs[(itemId)-ITEM_MENU_INFO_ARMOR_ITEM_FIRST];
                displayedFeatureCount = 0;
                bonusColorRgb         = ITEM_MENU_INFO_BONUS_COLOR;
                rowX                  = 2;
                armorFeatures         = (u32)armorStats->features;
                rowY                  = object->panel.contentTop.signedValue + 0x1E;
                labelRequest.x        = object->panel.contentOriginX.unsignedValue + rowX;
                labelRequest.y        = object->panel.contentOriginY.unsignedValue + (rowY - 2);
                labelRequest.otIndex  = object->panel.otIndex.signedValue + 1;
                // Keep the color-before-font order of this label request.
                labelRequest.colorRgb   = ITEM_MENU_PANEL_TEXT_COLOR;
                labelRequest.glyphTable = TEXT_GLYPH_TABLE_SMALL;
                labelRequest.alignment  = TEXT_ALIGNMENT_LEFT;
                labelRequest.drawMode   = TEXT_DRAW_OUTLINED;
                textDrawString(&labelRequest, (const u8*)Gp_StrAddHp);
                if (armorStats->hpBonus == 0) {
                    armorBonusRequest.x          = object->panel.contentOriginX.unsignedValue + 0x78;
                    armorBonusRequest.y          = object->panel.contentOriginY.unsignedValue + rowY;
                    armorBonusRequest.otIndex    = object->panel.otIndex.signedValue + 1;
                    armorBonusRequest.colorRgb   = ITEM_MENU_PANEL_TEXT_COLOR;
                    armorBonusRequest.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
                    armorBonusRequest.alignment  = TEXT_ALIGNMENT_RIGHT;
                    armorBonusRequest.drawMode   = TEXT_DRAW_TRANSLUCENT_OUTLINED;
                    textDrawString(&armorBonusRequest, (const u8*)D_8009707C);
                } else {
                    armorBonusRequest.x          = object->panel.contentOriginX.unsignedValue + 0x7A;
                    armorBonusRequest.y          = object->panel.contentOriginY.unsignedValue + rowY;
                    armorBonusRequest.otIndex    = object->panel.otIndex.signedValue + 1;
                    armorBonusRequest.colorRgb   = bonusColorRgb;
                    armorBonusRequest.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
                    armorBonusRequest.alignment  = TEXT_ALIGNMENT_RIGHT;
                    armorBonusRequest.drawMode   = TEXT_DRAW_TRANSLUCENT_OUTLINED;
                    textDrawString(&armorBonusRequest, textItoaSignPrefixed(bonusText, armorStats->hpBonus));
                }
                rowY += 0xF;

                armorBonusRequest.x          = object->panel.contentOriginX.unsignedValue + rowX;
                armorBonusRequest.y          = object->panel.contentOriginY.unsignedValue + (rowY - 2);
                armorBonusRequest.otIndex    = object->panel.otIndex.signedValue + 1;
                armorBonusRequest.glyphTable = TEXT_GLYPH_TABLE_SMALL;
                armorBonusRequest.colorRgb   = ITEM_MENU_PANEL_TEXT_COLOR;
                armorBonusRequest.alignment  = TEXT_ALIGNMENT_LEFT;
                armorBonusRequest.drawMode   = TEXT_DRAW_OUTLINED;
                textDrawString(&armorBonusRequest, (const u8*)Gp_StrAddMp);
                if (armorStats->mpBonus == 0) {
                    armorDetailsRequest.x          = object->panel.contentOriginX.unsignedValue + 0x76 + rowX;
                    armorDetailsRequest.y          = object->panel.contentOriginY.unsignedValue + rowY;
                    armorDetailsRequest.otIndex    = object->panel.otIndex.signedValue + 1;
                    armorDetailsRequest.alignment  = TEXT_ALIGNMENT_RIGHT;
                    armorDetailsRequest.colorRgb   = ITEM_MENU_PANEL_TEXT_COLOR;
                    armorDetailsRequest.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
                    armorDetailsRequest.drawMode   = TEXT_DRAW_TRANSLUCENT_OUTLINED;
                    textDrawString(&armorDetailsRequest, (const u8*)D_8009707C);
                } else {
                    armorDetailsRequest.x          = object->panel.contentOriginX.unsignedValue + 0x78 + rowX;
                    armorDetailsRequest.y          = object->panel.contentOriginY.unsignedValue + rowY;
                    armorDetailsRequest.otIndex    = object->panel.otIndex.signedValue + 1;
                    armorDetailsRequest.alignment  = TEXT_ALIGNMENT_RIGHT;
                    armorDetailsRequest.colorRgb   = bonusColorRgb;
                    armorDetailsRequest.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
                    armorDetailsRequest.drawMode   = TEXT_DRAW_TRANSLUCENT_OUTLINED;
                    textDrawString(&armorDetailsRequest, textItoaSignPrefixed(bonusText, armorStats->mpBonus));
                }
                rowY += 0xF;

                armorDetailsRequest.x          = object->panel.contentOriginX.unsignedValue + rowX;
                armorDetailsRequest.y          = object->panel.contentOriginY.unsignedValue + (rowY - 2);
                armorDetailsRequest.otIndex    = object->panel.otIndex.signedValue + 1;
                armorDetailsRequest.colorRgb   = ITEM_MENU_PANEL_TEXT_COLOR;
                armorDetailsRequest.glyphTable = TEXT_GLYPH_TABLE_SMALL;
                armorDetailsRequest.alignment  = TEXT_ALIGNMENT_LEFT;
                armorDetailsRequest.drawMode   = TEXT_DRAW_OUTLINED;
                textDrawString(&armorDetailsRequest, (const u8*)Gp_StrAttachments3);
                featureIndex                      = 0;
                attachmentCountRequest.x          = object->panel.contentOriginX.unsignedValue + 0x78 + rowX;
                attachmentCountRequest.y          = object->panel.contentOriginY.unsignedValue + rowY;
                attachmentCountRequest.otIndex    = object->panel.otIndex.signedValue + 1;
                rowY                             += 0xF;
                attachmentCountRequest.alignment  = TEXT_ALIGNMENT_RIGHT;
                attachmentCountRequest.colorRgb   = bonusColorRgb;
                attachmentCountRequest.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
                attachmentCountRequest.drawMode   = TEXT_DRAW_TRANSLUCENT_OUTLINED;
                textDrawString(&attachmentCountRequest, textItoaUnsigned(bonusText, equipmentGetArmorAttachmentSlotCount(itemId)));
                featureLabelRequest.x          = object->panel.contentOriginX.unsignedValue + rowX;
                featureLabelRequest.y          = object->panel.contentOriginY.unsignedValue + (rowY - 2);
                featureLabelRequest.otIndex    = object->panel.otIndex.signedValue + 1;
                rowY                          += 8;
                featureLabelRequest.colorRgb   = ITEM_MENU_PANEL_TEXT_COLOR;
                featureLabelRequest.glyphTable = TEXT_GLYPH_TABLE_SMALL;
                featureLabelRequest.alignment  = TEXT_ALIGNMENT_LEFT;
                featureLabelRequest.drawMode   = TEXT_DRAW_OUTLINED;
                textDrawString(&featureLabelRequest, (const u8*)Gp_StrSpecialFeat);
                featureName = Gp_FeatNameTbl;
                do {
                    if (armorFeatures & 1) {
                        featureNameRequest.x          = object->panel.contentOriginX.unsignedValue + 8 + rowX;
                        featureNameRequest.y          = object->panel.contentOriginY.unsignedValue + rowY;
                        featureNameRequest.otIndex    = object->panel.otIndex.signedValue + 1;
                        featureNameRequest.colorRgb   = ITEM_MENU_PANEL_TEXT_COLOR;
                        featureNameRequest.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
                        featureNameRequest.alignment  = TEXT_ALIGNMENT_LEFT;
                        featureNameRequest.drawMode   = TEXT_DRAW_TRANSLUCENT_OUTLINED;
                        textDrawString(&featureNameRequest, *featureName);
                        displayedFeatureCount++;
                        rowY += 0xB;
                        if (displayedFeatureCount >= ITEM_MENU_INFO_FEATURE_ROW_LIMIT) {
                            break;
                        }
                    }
                    armorFeatures >>= 1;
                    featureIndex++;
                    featureName++;
                } while (featureIndex < ITEM_MENU_INFO_FEATURE_BIT_COUNT);
                rowX                       = object->panel.contentLeft.signedValue + 2;
                categoryRequest.x          = object->panel.contentOriginX.unsignedValue + rowX;
                categoryRequest.y          = object->panel.contentOriginY.unsignedValue + 0x40;
                categoryRequest.otIndex    = object->panel.otIndex.signedValue + 1;
                categoryRequest.glyphTable = TEXT_GLYPH_TABLE_SMALL;
                categoryRequest.colorRgb   = ITEM_MENU_PANEL_TEXT_COLOR;
                categoryRequest.alignment  = TEXT_ALIGNMENT_LEFT;
                categoryRequest.drawMode   = TEXT_DRAW_OUTLINED;
                textDrawString(&categoryRequest, (const u8*)Gp_StrSpecialFeat);
            } else {
                consumableIndex = itemId - INVENTORY_CONSUMABLE_ITEM_FIRST;
                if ((u32)consumableIndex < (u32)INVENTORY_CONSUMABLE_ITEM_COUNT) {
                    caliberIndex            = Gp_ItemDescs[itemId].classification & ITEM_SUBTYPE_MASK;
                    contentTop              = object->panel.contentTop.signedValue;
                    categoryRequest.x       = object->panel.contentOriginX.unsignedValue + 2;
                    categoryRequest.y       = object->panel.contentOriginY.unsignedValue + contentTop + 0x1C;
                    categoryRequest.otIndex = object->panel.otIndex.signedValue + 1;
                    // Keep the color-before-font order of this label request.
                    textColorRgb               = ITEM_MENU_PANEL_TEXT_COLOR;
                    categoryRequest.colorRgb   = textColorRgb;
                    categoryRequest.glyphTable = TEXT_GLYPH_TABLE_SMALL;
                    categoryRequest.alignment  = TEXT_ALIGNMENT_LEFT;
                    categoryRequest.drawMode   = TEXT_DRAW_OUTLINED;
                    textDrawString(&categoryRequest, Gp_CaliberNameTbl[caliberIndex]);
                    attackTable = Gp_IdParamLo;
                    attackIndex = itemId - (INVENTORY_CONSUMABLE_ITEM_FIRST - 1);
                    attack      = attackTable + attackIndex;
                    textItoaSigned(quantityText, attack->amount);
                    rowY                         = contentTop + 0x2D;
                    powerLabelRequest.x          = object->panel.contentOriginX.unsignedValue + 2;
                    powerLabelRequest.y          = object->panel.contentOriginY.unsignedValue + (rowY - 2);
                    powerLabelRequest.otIndex    = object->panel.otIndex.signedValue + 1;
                    powerLabelRequest.glyphTable = TEXT_GLYPH_TABLE_SMALL;
                    powerLabelRequest.colorRgb   = textColorRgb;
                    powerLabelRequest.alignment  = TEXT_ALIGNMENT_LEFT;
                    powerLabelRequest.drawMode   = TEXT_DRAW_OUTLINED;
                    textDrawString(&powerLabelRequest, (const u8*)Gp_StrPowerCaps);
                    powerValueRequest.x          = object->panel.contentOriginX.unsignedValue + 0x4C;
                    powerValueRequest.y          = object->panel.contentOriginY.unsignedValue + rowY;
                    powerValueRequest.otIndex    = object->panel.otIndex.signedValue + 1;
                    powerValueRequest.colorRgb   = textColorRgb;
                    powerValueRequest.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
                    powerValueRequest.alignment  = TEXT_ALIGNMENT_LEFT;
                    powerValueRequest.drawMode   = TEXT_DRAW_TRANSLUCENT_OUTLINED;
                    textDrawString(&powerValueRequest, quantityText);
                    textItoaSigned(quantityText, inventoryGetConsumableStackQuantity(&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems, itemId));
                    textItoaSigned(stackLimitText, Gp_StackLimits[consumableIndex].maxHeld);
                    textAppendString(quantityText, (const u8*)Gp_StrSlash);
                    textAppendString(quantityText, stackLimitText);
                    rowY                            = contentTop + 0x3C;
                    capacityLabelRequest.x          = object->panel.contentOriginX.unsignedValue + 2;
                    capacityLabelRequest.y          = object->panel.contentOriginY.unsignedValue + (rowY - 2);
                    capacityLabelRequest.otIndex    = object->panel.otIndex.signedValue + 1;
                    capacityLabelRequest.glyphTable = TEXT_GLYPH_TABLE_SMALL;
                    capacityLabelRequest.colorRgb   = textColorRgb;
                    capacityLabelRequest.alignment  = TEXT_ALIGNMENT_LEFT;
                    capacityLabelRequest.drawMode   = TEXT_DRAW_OUTLINED;
                    textDrawString(&capacityLabelRequest, (const u8*)Gp_StrCapacity);
                    capacityValueRequest.x          = object->panel.contentOriginX.unsignedValue + 0x4C;
                    capacityValueRequest.y          = object->panel.contentOriginY.unsignedValue + rowY;
                    capacityValueRequest.otIndex    = object->panel.otIndex.signedValue + 1;
                    capacityValueRequest.colorRgb   = textColorRgb;
                    capacityValueRequest.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
                    capacityValueRequest.alignment  = TEXT_ALIGNMENT_LEFT;
                    capacityValueRequest.drawMode   = TEXT_DRAW_TRANSLUCENT_OUTLINED;
                    textDrawString(&capacityValueRequest, quantityText);
                    rowY = contentTop + 0x4B;
                    if (attack->hitReaction != 0) {
                        extraLabelRequest.x          = object->panel.contentOriginX.unsignedValue + 2;
                        extraLabelRequest.y          = object->panel.contentOriginY.unsignedValue + (rowY - 2);
                        extraLabelRequest.otIndex    = object->panel.otIndex.signedValue + 1;
                        extraLabelRequest.glyphTable = TEXT_GLYPH_TABLE_SMALL;
                        extraLabelRequest.colorRgb   = textColorRgb;
                        extraLabelRequest.alignment  = TEXT_ALIGNMENT_LEFT;
                        extraLabelRequest.drawMode   = TEXT_DRAW_OUTLINED;
                        textDrawString(&extraLabelRequest, (const u8*)Gp_StrSpecial);
                        specialValueRequest.x          = object->panel.contentOriginX.unsignedValue + 0x4C;
                        specialValueRequest.y          = object->panel.contentOriginY.unsignedValue + rowY;
                        specialValueRequest.otIndex    = object->panel.otIndex.signedValue + 1;
                        specialValueRequest.colorRgb   = ITEM_MENU_INFO_SPECIAL_COLOR;
                        specialValueRequest.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
                        specialValueRequest.alignment  = TEXT_ALIGNMENT_LEFT;
                        specialValueRequest.drawMode   = TEXT_DRAW_TRANSLUCENT_OUTLINED;
                        textDrawString(&specialValueRequest, D_8010E7C0[attack->hitReaction]);
                    }
                    rowX                         = object->panel.contentLeft.signedValue + 2;
                    extraLabelRequest.x          = object->panel.contentOriginX.unsignedValue + rowX;
                    extraLabelRequest.y          = object->panel.contentOriginY.unsignedValue + 0x40;
                    extraLabelRequest.otIndex    = object->panel.otIndex.signedValue + 1;
                    extraLabelRequest.glyphTable = TEXT_GLYPH_TABLE_SMALL;
                    extraLabelRequest.colorRgb   = textColorRgb;
                    extraLabelRequest.alignment  = TEXT_ALIGNMENT_LEFT;
                    extraLabelRequest.drawMode   = TEXT_DRAW_OUTLINED;
                    textDrawString(&extraLabelRequest, (const u8*)Gp_StrApplicableWpn);
                }
            }
        } else {
            itemMenuDrawPreview(object, object->panel.contentLeft.signedValue + 2, object->panel.contentTop.signedValue + 2, previewFlags | ITEM_MENU_PREVIEW_HIDDEN);
        }
        _itemMenuDrawUnmarkedItemRowIntoRequest(object, &labelRequest, 2, object->panel.contentTop.signedValue + 0xF, ITEM_MENU_PANEL_TEXT_COLOR, itemId);
        if ((object->panel.control.word == USER_INTERFACE_PANEL_ACTIVE) && (cdCmdIsIdle())) {
            if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskCancel | Pad_MaskConfirm | PAD_BUTTON_TRIANGLE) != 0) {
                if (!(task->spawnArg1.value & ITEM_MENU_INFO_NEXT_REPLAY)) {
                    sndEvtRequestScriptStart(SOUND_MENU_CANCEL, 0, 0);
                }
                displaySetFrameTiming(DISPLAY_TIMING_EVERY_VBLANK);
                object->result = USER_INTERFACE_RESULT_CONFIRM;
            } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskMenu) != 0) {
                displaySetFrameTiming(DISPLAY_TIMING_EVERY_VBLANK);
                object->result = USER_INTERFACE_RESULT_CANCEL;
            }
        }
    }
}

void itemMenuUseKeyItemTask(Task* task)
{
    UiObject* object;
    UiList*   keyItemList;
    Task*     roomTask;
    s32       itemId;
    s32       useResult;
    s32       noticeWidth;
    s32       textRight;
    s32       usedLabelWidth;
    u32       textColorRgb;
    s32       drawMode;
    const u8* itemName;

    object         = task->spawnArg2.pointer;
    object->result = USER_INTERFACE_RESULT_NONE;
    if (task->state == ITEM_MENU_KEY_ITEM_USE_STATE_REQUEST) {
        keyItemList = &D_8010E960;
        roomTask    = gameGetTaskSlot(GAME_TASK_SLOT_ROOM);
        itemId      = inventoryGetCollectedItemId(keyItemList->selectedItemIndex, 0);
        // The room may start an event; its reply selects this menu's presentation.
        useResult = taskMessageDispatch(roomTask, ROOM_MESSAGE_USE_KEY_ITEM, itemId, 0);
        if (useResult == ROOM_KEY_ITEM_USE_SHOW_USED_NOTICE) {
            task->spawnArg1.value = itemId;
            noticeWidth           = textMeasureLineWidth(itemGetText(itemId, ITEM_TEXT_NAME, 0)) + 0xB;
            usedLabelWidth        = textMeasureLineWidth((const u8*)Gp_StrUsed);
            if (noticeWidth < usedLabelWidth) {
                noticeWidth = usedLabelWidth;
            }
            uiSetPanelContentSize(&object->panel, noticeWidth + 5, uiGetTextRowsHeight(2) + 1);
            object->panel.bounds.rect.x = (-object->panel.bounds.rect.w) >> 1;
            object->panel.style        &= (s32)~USER_INTERFACE_PANEL_NO_FRAME;
        } else if (useResult == ROOM_KEY_ITEM_USE_NO_NOTICE) {
            uiStartPanelHiding(object, task);
            object->result               = USER_INTERFACE_RESULT_CANCEL;
            object->panel.animationTicks = ITEM_MENU_KEY_ITEM_USE_HIDE_TICKS;
            task->state                  = task->state + 1;
        } else {
            task->spawnArg1.value = ITEM_MENU_KEY_ITEM_USE_REFUSED_ID;
            uiSizePanelForTextDefault(&object->panel, (const u8*)Gp_StrNoUseNow);
            object->panel.style &= (s32)~USER_INTERFACE_PANEL_NO_FRAME;
        }
        task->killCountdown = ITEM_MENU_NOTICE_DURATION_TICKS;
        task->state         = task->state + 1;
    }
    if (task->state != ITEM_MENU_KEY_ITEM_USE_STATE_NO_NOTICE) {
        uiDrawPanelLabel(&object->panel, Gp_StrNotice);
        if (task->spawnArg1.value == ITEM_MENU_KEY_ITEM_USE_REFUSED_ID) {
            textColorRgb = uiGetTextColor(object, USER_INTERFACE_TEXT_COLOR_NORMAL);
            textDrawUiLine(object, object->panel.contentLeft.signedValue + 2, object->panel.contentTop.signedValue + 0xF, (const u8*)Gp_StrNoUseNow, textColorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
        } else {
            textColorRgb = uiGetTextColor(object, USER_INTERFACE_TEXT_COLOR_NORMAL);
            drawMode     = TEXT_DRAW_OUTLINED;
            textDrawUiLine(object, object->panel.contentLeft.signedValue + 2, object->panel.contentTop.signedValue + 0xF, (const u8*)Gp_StrUsed, textColorRgb, drawMode, TEXT_ALIGNMENT_LEFT);
            itemName  = itemGetText(task->spawnArg1.value, ITEM_TEXT_NAME, 0);
            textRight = textDrawUiLine(object, object->panel.contentLeft.signedValue + 2, object->panel.contentTop.signedValue + 0x1E, itemName, 0x37A78, drawMode, TEXT_ALIGNMENT_LEFT);
            textDrawUiLine(object, textRight, object->panel.contentTop.signedValue + 0x1E, (const u8*)Gp_StrDot, ITEM_MENU_PANEL_TEXT_COLOR, drawMode, TEXT_ALIGNMENT_LEFT);
        }
        task->killCountdown = task->killCountdown - gDisplayState.frameTicks;
        if (object->panel.control.word == USER_INTERFACE_PANEL_ACTIVE) {
            if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskMenu) != 0) {
                object->result = USER_INTERFACE_RESULT_CANCEL;
            } else if ((task->killCountdown <= 0) ||
                       (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm | Pad_MaskCancel) != 0)) {
                if (task->spawnArg1.value == ITEM_MENU_KEY_ITEM_USE_REFUSED_ID) {
                    if (gGameSession->cutsceneHold == 1) {
                        object->result = USER_INTERFACE_RESULT_CONFIRM;
                    } else {
                        object->result = USER_INTERFACE_RESULT_DISMISS;
                    }
                } else {
                    object->result = USER_INTERFACE_RESULT_CANCEL;
                }
                task->killCountdown = ITEM_MENU_COMPLETED_COUNTDOWN;
            }
        }
    }
}

void itemMenuKeyItemCommandTask(Task* task)
{
    Task*     childTask;
    UiObject* object;
    UiList*   commandList;
    UiObject* childObject;
    s32       childResult;

    object         = task->spawnArg2.pointer;
    object->result = USER_INTERFACE_RESULT_NONE;
    commandList    = &D_8010E938;
    if (task->state == 0) {
        object->panel.bounds.unsignedRect.w = ITEM_MENU_KEY_ITEM_COMMAND_WIDTH;
        uiFitPanelToList(commandList, &object->panel);
        task->state = task->state + 1;
    }
    uiUpdateList(commandList, &object->panel);
    if (object->panel.control.word == USER_INTERFACE_PANEL_ACTIVE) {
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskMenu) != 0) {
            object->result = USER_INTERFACE_RESULT_CANCEL;
        } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskCancel) != 0) {
            sndEvtRequestScriptStart(SOUND_MENU_CANCEL, 0, 0);
            object->result = USER_INTERFACE_RESULT_CONFIRM;
        }
    }
    // Resolve the use notice before restoring command input.
    childTask = task->firstChild;
    if (childTask != NULL) {
        childObject = childTask->spawnArg2.pointer;
        childResult = childObject->result;
        switch (childResult) {
            case USER_INTERFACE_RESULT_CANCEL:
                object->result = childResult;
                break;
            case USER_INTERFACE_RESULT_CONFIRM:
                uiStartTreeClosing(childObject, childObject->owner);
                object->panel.control.word = USER_INTERFACE_PANEL_ACTIVE;
                break;
            case USER_INTERFACE_RESULT_DISMISS:
                object->result = USER_INTERFACE_RESULT_CONFIRM;
                break;
        }
    }
}

void Gp_DrawCollectedRow(UiList* arg0, UiObject* arg1)
{
    TextDrawReq req;
    s32         item;
    s32         x;
    s32         y;
    s32         color;
    s32         temp;
    s32         status;
    s32         one;
    s32         flag;
    s32         i;
    s32         minusOne;
    UiObject*   obj;
    s32         baseY;

    item  = inventoryGetCollectedItemId(arg0->currentItemIndex, 0);
    x     = arg0->rowTextX.signedValue;
    y     = arg0->rowTextY.signedValue;
    color = arg0->colorRgb;
    if (arg1->panel.state != USER_INTERFACE_PANEL_HIDDEN) {
        req.x          = arg1->panel.contentOriginX.unsignedValue + 0x11 + x;
        baseY          = arg1->panel.contentOriginY.unsignedValue - 6;
        req.y          = baseY + y;
        req.otIndex    = arg1->panel.otIndex.signedValue + 1;
        req.colorRgb   = color;
        req.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
        req.alignment  = TEXT_ALIGNMENT_LEFT;
        req.drawMode   = TEXT_DRAW_OUTLINED;
        textDrawString(&req, itemGetText(item, ITEM_TEXT_NAME, 0));
        temp = item - 0xF;
        if ((u32)temp < 0x24U) {
            itemMenuDrawParasiteEnergyLevel(arg1, x, y, temp % 3 + 1, color);
        }
        itemMenuDrawItemIcon(arg1, x, y, item, ITEM_MENU_ICON_DEFAULT);
    }

    status = arg1->panel.control.word;
    one    = 1;
    if (((status >> 16) == one) || (status == one)) {
        if (arg0->selectedItemIndex == arg0->currentItemIndex) {
            if (item != Gp_PreviewItems[0]) {
                i        = 0;
                minusOne = -1;
                for (; i < 3; i++) {
                    if (i == 0) {
                        Gp_PreviewItems[0] = item;
                    } else {
                        Gp_PreviewItems[i] = minusOne;
                    }
                }
                itemMenuEnqueuePreviewLoad(item, 0);
            }
            if (item == 0) {
                uiSetPromptText(Gp_StrEmpty, 0, 0);
            } else {
                uiSetPromptText(itemGetText(item, ITEM_TEXT_DESCRIPTION_FIRST, 0), 0, 0);
            }
        }
    }

    flag = arg0->rowInputEnabled;
    if (flag == 1) {
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm) != 0) {
            sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
            if (gGameSession->cutsceneHold == flag) {
                uiSpawnObject(&D_8010EF84, 0, 1, 1, arg1);
                arg1->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
            } else {
                obj = uiSpawnObject(&D_8010EF68, item, 1, 1, arg1);
                if (obj != NULL) {
                    uiPositionRowDialog(&(obj)->panel, arg0, &(arg1)->panel);
                    arg1->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
                }
            }
        } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_TRIANGLE) != 0) {
            sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
            uiSpawnObject(&D_8010EFA0, item, 1, 1, arg1);
            arg1->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
        }
    }
}

void Gp_KeyItemMenuTask(Task* arg0)
{
    UiObject* obj;
    UiList*   menu;
    Task*     child;
    Task*     next;
    Task*     head;
    UiObject* childObj;
    s32       flag;

    obj         = arg0->spawnArg2.pointer;
    menu        = &D_8010E960;
    obj->result = USER_INTERFACE_RESULT_NONE;
    uiDrawPanelLabel(&(obj)->panel, Gp_StrKeyItem);
    if (arg0->state == 0) {
        menu->visibleRowCount.unsignedValue = menu->itemCount = inventoryCountCollectedBits();
        if (menu->itemCount < menu->selectedItemIndex) {
            menu->selectedItemIndex = menu->itemCount;
        }
        uiInitList(menu, &(obj)->panel);
        menu->flags = USER_INTERFACE_LIST_SHARED_ROW_CALLBACK;
        if (arg0->spawnArg1.value == 0) {
            uiSetPanelContentSize(&(obj)->panel, 0, uiGetTextRowsHeight(0xA) + 1);
            uiSpawnObject(&D_8010F868, 0, 0, 1, obj);
        }
        menu->selectedItemIndex                   = 0;
        menu->firstVisibleItemIndex.unsignedValue = 0;
        arg0->state                               = arg0->state + 1;
    } else {
        menu->visibleRowCount.unsignedValue = menu->itemCount = inventoryCountCollectedBits();
        if (menu->itemCount < menu->selectedItemIndex) {
            menu->selectedItemIndex = menu->itemCount;
        }
        uiRefreshListViewport(menu, &(obj)->panel);
        menu->flags = USER_INTERFACE_LIST_SHARED_ROW_CALLBACK;
        if (menu->selectedItemIndex >= menu->itemCount) {
            menu->selectedItemIndex = menu->itemCount - 1;
        }
        uiUpdateList(menu, &obj->panel);
        if (obj->panel.control.word == USER_INTERFACE_PANEL_ACTIVE) {
            if (obj->result == USER_INTERFACE_RESULT_NONE) {
                if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskMenu) != 0) {
                    obj->result = USER_INTERFACE_RESULT_CANCEL;
                } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskCancel) != 0) {
                    if (gGameSession->cutsceneHold == 1) {
                        sndEvtRequestScriptStart(SOUND_MENU_CANCEL, 0, 0);
                        obj->result = USER_INTERFACE_RESULT_CANCEL;
                    } else {
                        sndEvtRequestScriptStart(SOUND_MENU_CANCEL, 0, 0);
                        obj->resultValue = 1;
                        obj->result      = USER_INTERFACE_RESULT_CONFIRM;
                    }
                } else {
                    padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_L2 | PAD_BUTTON_R2);
                }
            }
        } else if (obj->panel.control.word >= USER_INTERFACE_PANEL_REQUEST_MIN) {
            obj->panel.control.word = USER_INTERFACE_PANEL_ACTIVE;
        }
    }
    head = arg0->firstChild;
    if (head != NULL) {
        child = head;
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
                    obj->panel.control.word = USER_INTERFACE_PANEL_ACTIVE;
                    break;
            }
            head  = arg0->firstChild;
            child = next;
            if (head == NULL) {
                break;
            }
        } while (child != head);
    }
}

void itemMenuDrawPreview(const UiObject* object, s32 left, s32 top, s32 flags)
{
    POLY_FT4*            quad;
    _ItemMenuPreviewSize scaledSize;
    s32                  textureWidth;
    s32                  textureHeight;
    s32                  screenX;
    s32                  screenY;
    s32                  scale12;

    textureWidth  = 0x80;
    textureHeight = 0x60;
    if (flags & ITEM_MENU_PREVIEW_SMALL) {
        textureWidth  = 0x50;
        textureHeight = 0x3C;
    } else if (flags & ITEM_MENU_PREVIEW_TALL) {
        textureHeight = 0x7F;
    }
    scaledSize.width  = textureWidth;
    scaledSize.height = textureHeight;
    scaledSize.depth  = 0;
    // Shrink the preview by a 4.12 factor on the GTE.
    if ((flags & ITEM_MENU_PREVIEW_SCALE_MASK) == ITEM_MENU_PREVIEW_SCALE_EQUIPMENT) {
        scale12 = ITEM_MENU_PREVIEW_EQUIPMENT_SCALE_12;
        ITEM_MENU_SCALE_PREVIEW_SIZE(&scaledSize, scale12);
    } else if ((flags & ITEM_MENU_PREVIEW_SCALE_MASK) == ITEM_MENU_PREVIEW_SCALE_SHOP) {
        scale12 = ITEM_MENU_PREVIEW_SHOP_SCALE_12;
        ITEM_MENU_SCALE_PREVIEW_SIZE(&scaledSize, scale12);
    }
    // Scale screen dimensions while sampling the original texture extent.
    if (!(flags & ITEM_MENU_PREVIEW_HIDDEN)) {
        quad           = gGpuPrimCursor;
        gGpuPrimCursor = quad + 1;
        setlen(quad, 9);
        setcode(quad, 0x2D);
        screenX  = object->panel.contentOriginX.unsignedValue + left;
        quad->x2 = screenX;
        quad->x0 = screenX;
        screenX  = screenX + scaledSize.width;
        quad->x3 = screenX;
        quad->x1 = screenX;
        screenY  = object->panel.contentOriginY.unsignedValue + top;
        quad->y1 = screenY;
        quad->y0 = screenY;
        screenY  = screenY + scaledSize.height;
        quad->y3 = screenY;
        quad->y2 = screenY;
        switch (flags & ITEM_MENU_PREVIEW_TEXTURE_MASK) {
            case ITEM_MENU_PREVIEW_TEXTURE_DIRECT:
                quad->tpage = ITEM_MENU_PREVIEW_DIRECT_TEXTURE_PAGE;
                quad->u0    = 0;
                quad->v0    = 0;
                quad->u1    = textureWidth;
                quad->v1    = 0;
                quad->u2    = 0;
                quad->v2    = textureHeight;
                quad->u3    = textureWidth;
                quad->v3    = textureHeight;
                quad->clut  = ITEM_MENU_PREVIEW_PALETTE;
                break;
            case ITEM_MENU_PREVIEW_TEXTURE_RELOCATED:
                quad->u0    = 0;
                quad->u1    = textureWidth;
                quad->u2    = 0;
                quad->u3    = textureWidth;
                quad->v0    = 0x80;
                quad->v1    = 0x80;
                quad->v2    = textureHeight - 0x80;
                quad->v3    = textureHeight - 0x80;
                quad->tpage = ITEM_MENU_PREVIEW_DIRECT_TEXTURE_PAGE;
                quad->clut  = ITEM_MENU_PREVIEW_RELOCATED_PALETTE;
                break;
            default:
                quad->u0    = 0;
                quad->u1    = textureWidth;
                quad->u2    = 0;
                quad->u3    = textureWidth;
                quad->v0    = 0x80;
                quad->v1    = 0x80;
                quad->v2    = textureHeight - 0x80;
                quad->v3    = textureHeight - 0x80;
                quad->tpage = ITEM_MENU_PREVIEW_MENU_TEXTURE_PAGE;
                quad->clut  = ITEM_MENU_PREVIEW_PALETTE;
                break;
        }
        addPrim(gGpuCurrentOt + object->panel.otIndex.signedValue + 1, quad);
    }
    uiDrawRecessedRect(&object->panel, (left - 1), (top - 1), ((s16)scaledSize.width + 1),
                       ((s16)scaledSize.height + 1), ITEM_MENU_PREVIEW_RECESSED_COLOR);
}

#undef ITEM_MENU_SCALE_PREVIEW_SIZE
