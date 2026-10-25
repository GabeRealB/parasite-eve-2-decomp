#include "gameplay/item_menu.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/memory.h>

#include "common.h"

#include "attachments.h"
#include "gameplay/attachment_state.h"
#include "gameplay/enemy.h"
#include "gameplay/inventory.h"
#include "item_menu.h"
#include "item_use.h"
#include "gameplay/items.h"
#include "items.h"
#include "gameplay/map.h"
#include "telephone.h"

#include "main/display.h"
#include "main/fs.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/stage.h"
#include "main/task.h"
#include "main/text.h"
#include "main/ui.h"
#include "main/wipsys.h"

#include "mapui/map_akropolis.h"

#include "mapui/map_dryfield.h"

#include "mapui/map_dryfield_full.h"

#include "mapui/map_neo_ark.h"

#include "mapui/map_shelter.h"

/// Column 4 of the attachment level row: minimum HP restored by menu Healing.
///
/// The same column is hit damage for attacks; battle Healing can roll above it.
#define ATTACHMENT_LEVEL_AMOUNT 4

/// One variant of a weapon that takes add-ons: the weapon item it is carried
/// as, and the add-on that item has mounted.
///
/// Using an add-on swaps the carried weapon item for the variant that has it
/// mounted; the add-on the old variant held goes back to the inventory.
typedef struct {
    u8 weaponItemId;  // Weapon item id of this variant (0x80..0x9F).
    u8 mountedItemId; // Add-on item mounted on it (0 none).
} _ItemMenuWeaponVariant;
STATIC_ASSERT_SIZEOF(_ItemMenuWeaponVariant, 2);

/// The eight M4A1 weapon variants, in the order an add-on use scans them.
///
/// The first carried row is the weapon the player has. The first other row
/// whose mounted add-on is the item just used is the weapon it becomes.
/// M4A1(+1) is listed before M4A1(+2); both mount the Rifle Clip Holder, so
/// that add-on steps the base rifle to (+1) and (+1) to (+2).
typedef struct {
    _ItemMenuWeaponVariant variants[8]; // One row per M4A1 variant, in scan order.
} _ItemMenuM4a1VariantTable;
STATIC_ASSERT_SIZEOF(_ItemMenuM4a1VariantTable, 0x10);

/// Task work for the panel that shows a weapon created by using an add-on.
///
/// The add-on is consumed and one carried weapon item is replaced by the
/// weapon that add-on produces. The panel lists the previous weapon and the
/// add-on above "used.", then the new weapon and any add-ons returned to the
/// inventory above "created."
typedef struct {
    s32 usedAddonItemId;      // Add-on item that was used.
    s32 previousWeaponItemId; // Weapon item carried before the swap (0x80..0x9F).
    s32 createdWeaponItemId;  // Weapon item carried after the swap (0x80..0x9F).
    s32 returnedAddonItemId;  // Add-on taken off the old variant (0 none).
    s32 returnedClipItemId;   // Extra Rifle Clip Holder returned while M4A1(+2) is carried (0 none).
} _ItemMenuWeaponCreateWork;
STATIC_ASSERT_SIZEOF(_ItemMenuWeaponCreateWork, 0x14);

/// Item-panel colors, ordinary P.E. item ids and update-count delays.
enum {
    ITEM_MENU_PANEL_TEXT_COLOR           = 0x606060,
    ITEM_MENU_RECESSED_FILL_COLOR        = 0x102010,
    ITEM_MENU_PARASITE_ENERGY_ITEM_FIRST = 15,
    ITEM_MENU_PARASITE_ENERGY_ITEM_COUNT = ATTACHMENT_SPELL_COUNT * ATTACHMENT_LEVEL_COUNT,
    ITEM_MENU_DIALOG_OPEN_DELAY_TICKS    = 2,
    ITEM_MENU_COMPLETED_COUNTDOWN        = 0x7FFF,
};

/// Item-menu result-panel accents, display duration and elemental slot grouping.
enum {
    ITEM_MENU_RESULT_ITEM_COLOR            = 0x037A78,
    ITEM_MENU_RESULT_NOTICE_UPDATES        = 188,
    ITEM_MENU_INVOKE_ABILITIES_PER_ELEMENT = 3,
    ITEM_MENU_INVOKE_MAX_LEVEL             = 3
};

/// Draws one visible unmarked item row into a reusable text request.
///
/// Statement block; the caller checks visibility. Inputs must be free of side
/// effects because object, coordinates, item and color are evaluated repeatedly.
/// request is a TextDrawReq lvalue; baseY and levelIndex are s32 scratch lvalues
/// overwritten by the operation. Uses `itemMenuDrawItemRow`'s resource contract.
/// The output lvalues must not overlap the object, coordinates, id or color;
/// name text is consumed before the level mark and item icon are drawn.
#define ITEM_MENU_DRAW_UNMARKED_ROW_CONTENTS(object, request, rowX, rowY, itemId, rowColorRgb, baseY, levelIndex)                \
    {                                                                                                                            \
        (request).x          = (object)->panel.contentOriginX.unsignedValue + 0x11 + (rowX);                                     \
        (baseY)              = (object)->panel.contentOriginY.unsignedValue - 6;                                                 \
        (request).y          = (baseY) + (rowY);                                                                                 \
        (request).otIndex    = (object)->panel.otIndex.signedValue + 1;                                                          \
        (request).colorRgb   = (rowColorRgb);                                                                                    \
        (request).glyphTable = TEXT_GLYPH_TABLE_MEDIUM;                                                                          \
        (request).alignment  = TEXT_ALIGNMENT_LEFT;                                                                              \
        (request).drawMode   = TEXT_DRAW_OUTLINED;                                                                               \
        textDrawString(&(request), itemGetText((itemId), ITEM_TEXT_NAME, 0));                                                    \
        (levelIndex) = (itemId) - ITEM_MENU_PARASITE_ENERGY_ITEM_FIRST;                                                          \
        if ((u32)(levelIndex) < (u32)ITEM_MENU_PARASITE_ENERGY_ITEM_COUNT) {                                                     \
            itemMenuDrawParasiteEnergyLevel((object), (rowX), (rowY), (levelIndex) % ATTACHMENT_LEVEL_COUNT + 1, (rowColorRgb)); \
        }                                                                                                                        \
        itemMenuDrawItemIcon((object), (rowX), (rowY), (itemId), ITEM_MENU_ICON_DEFAULT);                                        \
    }

u8 gMenuMapSelectedPage;

u8 gMenuMapPictureOffset;

UiList gItemMenuPeElementLists[4];

s32 gItemMenuHotspotActionConfirmed;

s32 gItemMenuHotspotScreenX;

s32 gItemMenuHotspotScreenY;

s32 gItemMenuHotspotActionKindCopy;

static const char _gItemMenuHealingStatusTitle[];

static const char _gItemMenuInvokeTitle[];

static const char _gItemMenuPeListTitle[];

static const u8 _gItemMenuCountTextTerminator;

static const u8 _gItemMenuInventoryTotalText[];

static const char _gItemPickupQuestionTitle[];

static const char _gItemPickupInventoryFullTitle[];

static void _itemPickupPreviewTask(Task* task);

static void _itemPickupTitleTask(Task* task);

static void _itemPickupAskTask(Task* task);

static void _itemPickupInventoryFullTask(Task* task);

static void _itemPickupObtainedNoticeTask(Task* task);

static inline void _itemMenuSetPreviewItem(s32 itemId, u8 loadProfile);

/// Builds the shared popup callbacks for one selected inventory or attachment row.
///
/// The list uses the six-entry `gItemMenuCommandRowCallbacks`; writes 0..3 callbacks and
/// matching item/visible-row counts, leaving unused slots unchanged. The object
/// owner selects modes 0 inventory, 1 weapon, 2 consumable, 3 armor, 4 attachment.
/// `itemId` is an unsigned inventory byte promoted to s32. A consumable id needs
/// a live selectedRow with that id; other ids do not dereference the row.
/// Loaded consumables remain in qty, so Load needs a positive unloaded remainder.
/// Reserved ids outside the equipment ranges retain the ordinary Use branch.
static void _itemMenuBuildCommandList(UiList* list, UiObject* object, s32 itemId, const InventoryItemRow* selectedRow)
{
    enum {
        ITEM_MENU_COMMAND_MODE_INVENTORY       = 0,
        ITEM_MENU_COMMAND_MODE_WEAPON          = 1,
        ITEM_MENU_COMMAND_MODE_CONSUMABLE      = 2,
        ITEM_MENU_COMMAND_MODE_ARMOR           = 3,
        ITEM_MENU_COMMAND_MODE_ATTACHMENT      = 4,
        ITEM_MENU_COMMAND_WIDTH_PIXELS         = 96,
        ITEM_MENU_COMMAND_ARMOR_ITEM_FIRST     = 0x60,
        ITEM_MENU_COMMAND_EQUIPMENT_ITEM_COUNT = 0x20U,
        ITEM_MENU_COMMAND_TONFA_ITEM           = 0x92,
        ITEM_MENU_COMMAND_HYPERVELOCITY_ITEM   = 0x95
    };
    s32 commandCount;
    s32 commandMode;

    commandCount                        = 0;
    commandMode                         = object->owner->spawnArg1.value;
    object->panel.bounds.unsignedRect.w = ITEM_MENU_COMMAND_WIDTH_PIXELS;
    switch (commandMode) {
        case ITEM_MENU_COMMAND_MODE_INVENTORY:
            if (itemId == INVENTORY_ITEM_NONE) {
                gItemMenuCommandRowCallbacks[commandCount++] = itemMenuDrawMoveRow;
            } else if ((u32)(itemId - EQUIPMENT_WEAPON_ITEM_FIRST) < ITEM_MENU_COMMAND_EQUIPMENT_ITEM_COUNT) {
                gItemMenuCommandRowCallbacks[commandCount++] = itemMenuDrawMoveRow;
                gItemMenuCommandRowCallbacks[commandCount++] = itemMenuDrawDiscardRow;
            } else if ((u32)(itemId - ITEM_MENU_COMMAND_ARMOR_ITEM_FIRST) < ITEM_MENU_COMMAND_EQUIPMENT_ITEM_COUNT) {
                gItemMenuCommandRowCallbacks[commandCount++] = itemMenuDrawMoveRow;
                gItemMenuCommandRowCallbacks[commandCount++] = itemMenuDrawDiscardRow;
            } else if ((u32)(itemId - INVENTORY_CONSUMABLE_ITEM_FIRST) < (u32)INVENTORY_CONSUMABLE_ITEM_COUNT) {
                if ((selectedRow->qty - equipmentGetLoadedConsumableQuantity(&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems, itemId)) > 0) {
                    gItemMenuCommandRowCallbacks[commandCount++] = itemMenuDrawLoadRow;
                }
                gItemMenuCommandRowCallbacks[commandCount++] = itemMenuDrawMoveRow;
                gItemMenuCommandRowCallbacks[commandCount++] = itemMenuDrawDiscardRow;
            } else {
                gItemMenuCommandRowCallbacks[commandCount++] = itemMenuDrawUseRow;
                gItemMenuCommandRowCallbacks[commandCount++] = itemMenuDrawMoveRow;
                gItemMenuCommandRowCallbacks[commandCount++] = itemMenuDrawDiscardRow;
            }
            break;
        case ITEM_MENU_COMMAND_MODE_WEAPON:
            if (itemId != INVENTORY_ITEM_NONE) {
                if ((u32)(itemId - EQUIPMENT_WEAPON_ITEM_FIRST) < ITEM_MENU_COMMAND_EQUIPMENT_ITEM_COUNT) {
                    gItemMenuCommandRowCallbacks[commandCount++] = itemMenuDrawExchangeRow;
                }
            }
            break;
        case ITEM_MENU_COMMAND_MODE_CONSUMABLE:
            if (itemId == INVENTORY_ITEM_NONE) {
                gItemMenuCommandRowCallbacks[commandCount++] = itemMenuDrawExchangeRow;
            } else if ((u32)(itemId - INVENTORY_CONSUMABLE_ITEM_FIRST) < (u32)INVENTORY_CONSUMABLE_ITEM_COUNT) {
                gItemMenuCommandRowCallbacks[commandCount++] = itemMenuDrawExchangeRow;
            }
            break;
        case ITEM_MENU_COMMAND_MODE_ARMOR:
            if (itemId != INVENTORY_ITEM_NONE) {
                if ((u32)(itemId - ITEM_MENU_COMMAND_ARMOR_ITEM_FIRST) < ITEM_MENU_COMMAND_EQUIPMENT_ITEM_COUNT) {
                    gItemMenuCommandRowCallbacks[commandCount++] = itemMenuDrawExchangeRow;
                }
            }
            break;
        case ITEM_MENU_COMMAND_MODE_ATTACHMENT:
            if (itemId == INVENTORY_ITEM_NONE) {
                gItemMenuCommandRowCallbacks[commandCount++] = itemMenuDrawAttachmentExchangeRow;
            } else if ((u32)(itemId - EQUIPMENT_WEAPON_ITEM_FIRST) < ITEM_MENU_COMMAND_EQUIPMENT_ITEM_COUNT) {
                gItemMenuCommandRowCallbacks[commandCount++] = itemMenuDrawAttachmentExchangeRow;
                if ((itemId != ITEM_MENU_COMMAND_TONFA_ITEM) && (itemId != ITEM_MENU_COMMAND_HYPERVELOCITY_ITEM)) {
                    gItemMenuCommandRowCallbacks[commandCount++] = itemMenuDrawLoadRow;
                }
                gItemMenuCommandRowCallbacks[commandCount++] = itemMenuDrawDiscardRow;
            } else if ((u32)(itemId - ITEM_MENU_COMMAND_ARMOR_ITEM_FIRST) < ITEM_MENU_COMMAND_EQUIPMENT_ITEM_COUNT) {
            } else if ((u32)(itemId - INVENTORY_CONSUMABLE_ITEM_FIRST) < (u32)INVENTORY_CONSUMABLE_ITEM_COUNT) {
                gItemMenuCommandRowCallbacks[commandCount++] = itemMenuDrawAttachmentExchangeRow;
                if ((selectedRow->qty - equipmentGetLoadedConsumableQuantity(&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems, itemId)) > 0) {
                    gItemMenuCommandRowCallbacks[commandCount++] = itemMenuDrawLoadRow;
                }
                gItemMenuCommandRowCallbacks[commandCount++] = itemMenuDrawDiscardRow;
            } else {
                gItemMenuCommandRowCallbacks[commandCount++] = itemMenuDrawAttachmentExchangeRow;
                gItemMenuCommandRowCallbacks[commandCount++] = itemMenuDrawUseRow;
                gItemMenuCommandRowCallbacks[commandCount++] = itemMenuDrawDiscardRow;
            }
            break;
    }
    list->itemCount                     = commandCount;
    list->visibleRowCount.unsignedValue = commandCount;
}

/// Slots in the pickup's three-recipe child panel table.
enum {
    ITEM_PICKUP_CHILD_PANEL_TITLE          = 0,
    ITEM_PICKUP_CHILD_PANEL_QUESTION       = 1,
    ITEM_PICKUP_CHILD_PANEL_INVENTORY_FULL = 2,
    ITEM_PICKUP_CHILD_PANEL_COUNT          = 3
};

/// Recipes for the pickup title, Yes/No question and inventory-full warning.
///
/// Content is unused; handlers borrow the published pickup id. Bounds use
/// screen-centered pixels and ordering-table seeds count tags. The obtained
/// notice is a separate recipe immediately following this table in the image.
static const UiObjectDesc _gItemPickupChildPanelDescs[ITEM_PICKUP_CHILD_PANEL_COUNT] __attribute__((section(".data"))) = {
    { .style = USER_INTERFACE_PANEL_TITLE_STYLE, .bounds = { -144, -104, 144, 136 }, .otIndexSeed = 56, .taskFlags = TASK_BODY_NONE, .taskPriority = 192, .contentCallback = _itemPickupTitleTask },
    { .style = USER_INTERFACE_PANEL_TITLE_STYLE, .bounds = { -144, 32, 288, 48 }, .otIndexSeed = 44, .taskFlags = TASK_BODY_NONE, .taskPriority = 192, .contentCallback = _itemPickupAskTask },
    { .style = USER_INTERFACE_PANEL_TITLE_STYLE, .bounds = { -144, 32, 288, 48 }, .otIndexSeed = 44, .taskFlags = TASK_BODY_NONE, .taskPriority = 192, .contentCallback = _itemPickupInventoryFullTask },
};

/// Style seed for the pickup's child panels.
enum {
    ITEM_PICKUP_PREVIEW_PANEL_STYLE = 0,
};

/// Recipe for the obtained-item notice shown after a pickup is accepted.
///
/// Bounds are screen-centered pixels; the handler fits them to two text rows.
/// Content holds the catalog id in its low 16 bits; bit 16 allows timeout only.
static const UiObjectDesc _gItemPickupObtainedNoticeDesc __attribute__((section(".data"))) = {
    .style           = USER_INTERFACE_PANEL_TITLE_STYLE,
    .bounds          = { -80, -40, 160, 80 },
    .otIndexSeed     = 8,
    .taskFlags       = TASK_BODY_NONE,
    .taskPriority    = TASK_PRIORITY_CONTROL,
    .contentCallback = _itemPickupObtainedNoticeTask,
};

/// Recipe for the published pickup's equipment-scale model or picture preview.
///
/// Content is unused; the handler reads the published pickup id and waits for
/// menu-preview resources. The title parent places it below its own bounds.
/// Bounds are screen-centered pixels and the ordering-table seed counts tags.
static const UiObjectDesc _gItemPickupPreviewDesc __attribute__((section(".data"))) = {
    .style           = ITEM_PICKUP_PREVIEW_PANEL_STYLE,
    .bounds          = { -144, 0, 90, 70 },
    .otIndexSeed     = 52,
    .taskFlags       = TASK_BODY_NONE,
    .taskPriority    = TASK_PRIORITY_CONTROL,
    .contentCallback = _itemPickupPreviewTask,
};

const MenuMapAreaName* const gMenuMapAreaNameTables[MENU_MAP_STAGE_COUNT] __attribute__((section(".data"))) = {
    gMapAkropolisAreaNames,
    gMapDryfieldAreaNames,
    gMapDryfieldFullAreaNames,
    gMapShelterAreaNames,
    gMapNeoArkAreaNames,
};

const MenuMapIcon* const gMenuMapIconTables[MENU_MAP_STAGE_COUNT] __attribute__((section(".data"))) = {
    gMapAkropolisIcons,
    gMapDryfieldIcons,
    gMapDryfieldFullIcons,
    gMapShelterIcons,
    gMapNeoArkIcons,
};

const MenuMapMarker* gMenuMapMarkerTables[MENU_MAP_STAGE_COUNT] __attribute__((section(".data"))) = {
    gMapAkropolisMarkers,
    gMapDryfieldMarkers,
    gMapDryfieldFullMarkers,
    gMapShelterMarkers,
    gMapNeoArkMarkers,
};

const MenuMapArea* gMenuMapAreaTables[MENU_MAP_STAGE_COUNT] __attribute__((section(".data"))) = {
    gMapAkropolisAreas,
    gMapDryfieldAreas,
    gMapDryfieldFullAreas,
    gMapShelterAreas,
    gMapNeoArkAreas,
};

const u8* const gMenuMapPageFlagTables[MENU_MAP_STAGE_COUNT] __attribute__((section(".data"))) = {
    gMapAkropolisPageFlagIds,
    gMapDryfieldPageFlagIds,
    gMapDryfieldFullPageFlagIds,
    gMapShelterPageFlagIds,
    gMapNeoArkPageFlagIds,
};

const MenuMapAreaShape* const gMenuMapAreaShapeTables[MENU_MAP_STAGE_COUNT] __attribute__((section(".data"))) = {
    gMapAkropolisAreaShapes,
    gMapDryfieldAreaShapes,
    gMapDryfieldFullAreaShapes,
    gMapShelterAreaShapes,
    gMapNeoArkAreaShapes,
};

/// Map recipe defaults and the isolated Acropolis/MIST navigation boundary.
enum {
    MENU_MAP_ACROPOLIS_NAVIGATION_LAST_PAGE = 2,
    MENU_MAP_AREA_NAME_PANEL_UNTITLED_STYLE = 3,
    MENU_MAP_HELP_INITIAL_FILE_INDEX        = 1
};

const u8 gMenuMapNavigationLastPages[MENU_MAP_STAGE_COUNT] __attribute__((section(".data"))) = {
    MENU_MAP_ACROPOLIS_NAVIGATION_LAST_PAGE,
    MAP_DRYFIELD_PAGE_COUNT,
    MAP_DRYFIELD_FULL_PAGE_COUNT,
    MAP_SHELTER_PAGE_COUNT,
    MAP_NEO_ARK_PAGE_SHELTER_1F,
};

const u8 gMenuMapAreaShapeCounts[MENU_MAP_STAGE_COUNT] __attribute__((section(".data"))) = {
    ARRAY_SIZE(gMapAkropolisAreaShapes),
    ARRAY_SIZE(gMapDryfieldAreaShapes),
    ARRAY_SIZE(gMapDryfieldFullAreaShapes),
    ARRAY_SIZE(gMapShelterAreaShapes),
    MAP_NEO_ARK_AREA_ID_LIMIT + 1,
};

u8 gMenuMapHelpFileIndex = MENU_MAP_HELP_INITIAL_FILE_INDEX;

const UiObjectDesc gMenuMapPanelDesc __attribute__((section(".data"))) = {
    .style           = (s32)USER_INTERFACE_PANEL_NO_FRAME,
    .bounds          = { -145, -107, 290, 215 },
    .otIndexSeed     = 56,
    .taskFlags       = TASK_BODY_NONE,
    .taskPriority    = 192,
    .contentCallback = menuMapTask,
};

const UiObjectDesc gMenuMapHelpPanelDesc __attribute__((section(".data"))) = {
    .style           = USER_INTERFACE_PANEL_TITLE_STYLE,
    .bounds          = { -140, 50, 280, 50 },
    .otIndexSeed     = 20,
    .taskFlags       = TASK_BODY_NONE,
    .taskPriority    = 192,
    .contentCallback = menuMapHelpTask,
};

const UiObjectDesc gMenuMapAreaNamePanelDesc __attribute__((section(".data"))) = {
    .style           = MENU_MAP_AREA_NAME_PANEL_UNTITLED_STYLE,
    .bounds          = { -140, -107, 170, 15 },
    .otIndexSeed     = 24,
    .taskFlags       = TASK_BODY_NONE,
    .taskPriority    = 192,
    .contentCallback = menuMapAreaNameTask,
};

/// Retained Shift-JIS command text "捨てる" (Discard), with no current reader.
///
/// Includes one zero byte after the NUL terminator.
static const u8 _gItemMenuJapaneseDiscardText[8] __attribute__((section(".data"))) = "\x8E\xCC\x82\xC4\x82\xE9";

const u8 gItemMenuDiscardText[] __attribute__((section(".data")))        = "Discard";
const u8 gItemMenuHotspotItemText[5] __attribute__((section(".data")))   = "Item";
const u8 gItemMenuHotspotExamineText[] __attribute__((section(".data"))) = "Examine";
const u8 gItemMenuHotspotPushText[] __attribute__((section(".data")))    = "Push";
const u8 gItemMenuPeReviveText[] __attribute__((section(".data")))       = "Revive";
const u8 gItemMenuPeStrengthenText[] __attribute__((section(".data")))   = "Strengthen";
const u8 gItemMenuPeCancelText[12] __attribute__((section(".data")))     = "Cancel\000\000Use";
/// Notice text reporting that the required ammunition is unavailable.
static const u8 _gItemMenuWrongAmmoText[] __attribute__((section(".data"))) = "You do not habe the correct ammo.";
/// Notice text for an item whose discard restriction prevents removal.
static const u8 _gItemMenuCannotDiscardText[] __attribute__((section(".data"))) = "You cannot discard this item.";
/// Notice text for an equipped item that cannot be discarded.
static const u8 _gItemMenuCannotDiscardEquippedText[] __attribute__((section(".data"))) = "You cannot discard equipped items.";
/// Notice text for ammunition that cannot be discarded while loaded.
static const u8 _gItemMenuCannotDiscardLoadedAmmoText[] __attribute__((section(".data"))) = "You cannot discard loaded ammo.";
const u8        gItemMenuNoApplicableWeaponText[] __attribute__((section(".data")))       = "No applicable weapon equipped.";
/// Notice text reporting that an item cannot move to the chosen destination.
static const u8 _gItemMenuCannotMoveHereText[] __attribute__((section(".data"))) = "Cannot move this item here.";
/// Notice text for a transfer destination with no available capacity.
static const u8 _gItemMenuDestinationFullText[] __attribute__((section(".data"))) = "Destination full.";
/// Notice text reporting that an equipped item cannot be moved.
static const u8 _gItemMenuCannotMoveEquippedText[] __attribute__((section(".data"))) = "Cannot move equipped items.";
/// Notice text reporting that ammunition cannot be a swap partner.
static const u8 _gItemMenuCannotSwitchWithAmmoText[] __attribute__((section(".data"))) = "Cannot switch with ammunition.";
/// Notice text reporting that an equipped item cannot be swapped.
static const u8 _gItemMenuCannotSwitchEquippedText[] __attribute__((section(".data"))) = "Cannot switch equiped items.";
/// Notice text reporting that an equipped item cannot be a swap partner.
static const u8 _gItemMenuCannotSwitchWithEquippedText[] __attribute__((section(".data"))) = "Cannot switch with equipped items.";
/// Notice text reporting that loaded ammunition cannot be moved.
static const u8 _gItemMenuCannotMoveLoadedAmmoText[] __attribute__((section(".data"))) = "Cannot move loaded ammunition.";
/// Notice text for a P.E. level purchase with insufficient experience.
static const u8 _gItemMenuInsufficientExpText[] __attribute__((section(".data"))) = "Insufficient EXP.";
/// Notice text for a P.E. ability that is already at its maximum level.
static const u8 _gItemMenuMaxPeLevelText[] __attribute__((section(".data"))) = "Max level.";
/// Notice text reporting insufficient MP.
static const u8 _gItemMenuInsufficientMpText[] __attribute__((section(".data"))) = "Insufficient MP.";
/// Notice text for a cancelled save operation.
static const u8 _gItemMenuSaveCancelledText[] __attribute__((section(".data"))) = "Save cancelled.";
/// Confirmation prompt before discarding the selected inventory row.
static const u8 _gItemMenuConfirmDiscardText[] __attribute__((section(".data"))) = "Really discard?";
/// Notice text reporting a completed save.
static const u8 _gItemMenuSaveCompleteText[] __attribute__((section(".data"))) = "Save complete.";
/// Notice text for an item that cannot currently be used.
static const u8 _gItemMenuNoUseNowText[] __attribute__((section(".data"))) = "No use for this now.";
/// Stored notice text saying the item's use is unknown; no producer is known.
static const u8 _gItemMenuUnknownUseText[] __attribute__((section(".data"))) = "I don't no how to use this.";
/// Notice text when no other carried weapon is available for exchange.
static const u8 _gItemMenuNoOtherWeaponText[] __attribute__((section(".data"))) = "I have no other weapons.";
/// Notice text when no other carried armor is available for exchange.
static const u8 _gItemMenuNoOtherArmorText[] __attribute__((section(".data"))) = "I have no other armor.";
/// Notice text for an add-on use requiring a carried MP5A5 variant.
static const u8 _gItemMenuMp5a5RequiredText[] __attribute__((section(".data"))) = "MP5A5 required.";
/// Notice text for an add-on use requiring a carried M4A1 variant.
static const u8 _gItemMenuM4a1RequiredText[] __attribute__((section(".data"))) = "M4A1 required.";
/// Stored notice text requiring an AS12; no producer is known.
static const u8 _gItemMenuAs12RequiredText[] __attribute__((section(".data"))) = "AS12 required.";
/// Notice text for a Snail Magazine use requiring a carried P08.
static const u8 _gItemMenuP08RequiredText[] __attribute__((section(".data"))) = "P08 required.";
/// Notice text when the carried weapon already has the requested modification.
static const u8 _gItemMenuNoFurtherModificationsText[] __attribute__((section(".data"))) = "No further modifications.";
/// Notice text when replacing a weapon add-on needs a free inventory slot.
static const u8 _gItemMenuEmptySlotRequiredText[] __attribute__((section(".data"))) = "Empty slot required.";
/// Notice text after consuming a permanent maximum-HP boost.
static const u8 _gItemMenuMaxHpIncreasedText[] __attribute__((section(".data"))) = "Maximum HP increased.";
/// Notice text after consuming a permanent maximum-MP boost.
static const u8 _gItemMenuMaxMpIncreasedText[] __attribute__((section(".data"))) = "Maximum MP increased.";
/// Notice text when a transfer restriction blocks moving or swapping an item.
static const u8 _gItemMenuCannotMoveItemText[] __attribute__((section(".data"))) = "Cannot move this item.";
/// Stored notice text refusing to swap the source item; no producer is known.
static const u8 _gItemMenuCannotSwitchItemText[] __attribute__((section(".data"))) = "Cannot switch this item.";
/// Notice text when a transfer restriction blocks the selected swap partner.
static const u8 _gItemMenuCannotSwitchWithItemText[] __attribute__((section(".data")))              = "Cannot switch with this item.";
const u8        gItemMenuPeAreaEffectLabel[] __attribute__((section(".data")))                      = "Area of Effect";
const u8        gItemMenuPeCastingCostLabel[] __attribute__((section(".data")))                     = "Casting Cost";
const u8        gItemMenuPeAtpLossLabel[] __attribute__((section(".data")))                         = "ATP Loss";
const u8* const gItemMenuNoticeTexts[ITEM_MENU_NOTICE_TEXT_COUNT] __attribute__((section(".data"))) = {
    _gItemMenuWrongAmmoText,
    _gItemMenuCannotDiscardText,
    _gItemMenuCannotDiscardEquippedText,
    _gItemMenuCannotDiscardLoadedAmmoText,
    gItemMenuNoApplicableWeaponText,
    _gItemMenuCannotMoveHereText,
    _gItemMenuDestinationFullText,
    _gItemMenuCannotMoveEquippedText,
    _gItemMenuCannotSwitchWithAmmoText,
    _gItemMenuCannotSwitchEquippedText,
    _gItemMenuCannotSwitchWithEquippedText,
    _gItemMenuCannotMoveLoadedAmmoText,
    _gItemMenuInsufficientExpText,
    _gItemMenuMaxPeLevelText,
    _gItemMenuInsufficientMpText,
    _gItemMenuSaveCancelledText,
    _gItemMenuConfirmDiscardText,
    _gItemMenuSaveCompleteText,
    _gItemMenuNoUseNowText,
    _gItemMenuUnknownUseText,
    _gItemMenuNoOtherWeaponText,
    _gItemMenuNoOtherArmorText,
    _gItemMenuMp5a5RequiredText,
    _gItemMenuM4a1RequiredText,
    _gItemMenuAs12RequiredText,
    _gItemMenuP08RequiredText,
    _gItemMenuNoFurtherModificationsText,
    _gItemMenuEmptySlotRequiredText,
    _gItemMenuMaxHpIncreasedText,
    _gItemMenuMaxMpIncreasedText,
    _gItemMenuCannotMoveItemText,
    _gItemMenuCannotSwitchItemText,
    _gItemMenuCannotSwitchWithItemText,
};

/// Use/Attach and Key Item row dispatch borrowed by the shared Item command list.
///
/// Entries are never replaced; the writable pointer type follows `UiList`.
static UiListRowCallback _gItemMenuItemCommandRowCallbacks[] = { itemMenuDrawUseAttachCommandRow, itemMenuDrawKeyItemCommandRow };

UiList gItemMenuItemCommandList = {
    .rowCallbacks    = _gItemMenuItemCommandRowCallbacks,
    .itemCount       = ARRAY_SIZE(_gItemMenuItemCommandRowCallbacks),
    .visibleRowCount = { .unsignedValue = ARRAY_SIZE(_gItemMenuItemCommandRowCallbacks) },
    .wrapNavigation  = 1,
    .rowHeight       = 10,
};

/// Writable Upgrade/Cancel row dispatch for the P.E. command list.
///
/// The command task restores these callbacks on entry through the shared list.
static UiListRowCallback _gItemMenuPeCommandRowCallbacks[] = { itemMenuDrawPeUpgradeRow, itemMenuDrawPeCancelRow };

UiList gItemMenuPeCommandList = {
    .rowCallbacks    = _gItemMenuPeCommandRowCallbacks,
    .itemCount       = ARRAY_SIZE(_gItemMenuPeCommandRowCallbacks),
    .visibleRowCount = { .unsignedValue = ARRAY_SIZE(_gItemMenuPeCommandRowCallbacks) },
    .wrapNavigation  = 1,
    .rowHeight       = 10,
};

UiListRowCallback gItemMenuPeAbilityRowCallbacks[1] = {
    itemMenuDrawPeAbilityRow,
};

/// Title of the Fire P.E. ability list.
static const char _gItemMenuPeFireTitle[] __attribute__((section(".data"))) = "Fire";

/// Title of the Wind P.E. ability list.
static const char _gItemMenuPeWindTitle[] __attribute__((section(".data"))) = "Wind";

/// Title of the Water P.E. ability list.
static const char _gItemMenuPeWaterTitle[] __attribute__((section(".data"))) = "Water";

/// Title of the Earth P.E. ability list.
static const char _gItemMenuPeEarthTitle[] __attribute__((section(".data"))) = "Earth";

const char* const gItemMenuPeElementTitles[4] __attribute__((section(".data"))) = {
    _gItemMenuPeFireTitle,
    _gItemMenuPeWindTitle,
    _gItemMenuPeWaterTitle,
    _gItemMenuPeEarthTitle,
};

/// Style selector of the Item command panel recipe.
enum {
    ITEM_MENU_ITEM_COMMAND_PANEL_UNTITLED_STYLE = 3,
};

/// Recipe for the Item menu's Use/Attach and Key Item command panel.
///
/// Bounds are screen-centered pixels and the ordering-table seed counts tags.
/// No construction site is known for this retained recipe.
static UiObjectDesc _gItemMenuItemCommandPanelDesc = {
    .style           = ITEM_MENU_ITEM_COMMAND_PANEL_UNTITLED_STYLE,
    .bounds          = { -136, -50, 60, 80 },
    .otIndexSeed     = 24,
    .taskFlags       = TASK_BODY_NONE,
    .taskPriority    = TASK_PRIORITY_CONTROL,
    .contentCallback = itemMenuItemCommandTask,
};

enum {
    ITEM_MENU_PE_COMMAND_PANEL_UNTITLED_STYLE = 3,
};

const UiObjectDesc gItemMenuPeCommandPanelDesc __attribute__((section(".data"))) = {
    .style           = ITEM_MENU_PE_COMMAND_PANEL_UNTITLED_STYLE,
    .bounds          = { -136, -50, 70, 80 },
    .otIndexSeed     = 20,
    .taskFlags       = TASK_BODY_NONE,
    .taskPriority    = TASK_PRIORITY_CONTROL,
    .contentCallback = itemMenuPeCommandTask,
};

/// Carried weapon and mounted add-on catalog ids in the M4A1 scan table.
enum {
    ITEM_MENU_M4A1_VARIANT_BASE_ITEM        = 0x8F,
    ITEM_MENU_M4A1_VARIANT_ONE_HOLDER_ITEM  = 0x93,
    ITEM_MENU_M4A1_VARIANT_TWO_HOLDERS_ITEM = 0x94,
    ITEM_MENU_M4A1_VARIANT_HAMMER_ITEM      = 0x98,
    ITEM_MENU_M4A1_VARIANT_M203_ITEM        = 0x9A,
    ITEM_MENU_M4A1_VARIANT_M9_ITEM          = 0x99,
    ITEM_MENU_M4A1_VARIANT_PYKE_ITEM        = 0x9B,
    ITEM_MENU_M4A1_VARIANT_JAVELIN_ITEM     = 0x9C,
    ITEM_MENU_M4A1_ADDON_CLIP_HOLDER        = 0x0A,
    ITEM_MENU_M4A1_ADDON_HAMMER             = 0x42,
    ITEM_MENU_M4A1_ADDON_M203               = 0x45,
    ITEM_MENU_M4A1_ADDON_M9                 = 0x46,
    ITEM_MENU_M4A1_ADDON_PYKE               = 0x43,
    ITEM_MENU_M4A1_ADDON_JAVELIN            = 0x44
};

static const _ItemMenuM4a1VariantTable _gItemMenuM4a1VariantTable;

void itemMenuCommandTask(Task* task)
{
    enum { ITEM_MENU_COMMAND_INITIAL              = 0,
           ITEM_MENU_COMMAND_SCREEN_BOTTOM_PIXELS = 70 };
    Task*                   childTask;
    UiObject*               object;
    UiList*                 list;
    UiObject*               childObject;
    s32                     childResult;
    s32                     bottomAdjustment;
    const InventoryItemRow* selectedRow;
    s32                     itemId;
    s32                     listAction;

    object         = task->spawnArg2.pointer;
    object->result = USER_INTERFACE_RESULT_NONE;
    list           = &gItemMenuCommandList;
    if (task->state == ITEM_MENU_COMMAND_INITIAL) {
        selectedRow = gItemMenuSelectedRow;
        itemId      = INVENTORY_ITEM_NONE;
        if (selectedRow != NULL) {
            itemId = selectedRow->itemId;
        }
        _itemMenuBuildCommandList(list, object, itemId, selectedRow);
        uiFitPanelToList(list, &object->panel);
        bottomAdjustment = ITEM_MENU_COMMAND_SCREEN_BOTTOM_PIXELS - ((s16)object->panel.bounds.unsignedRect.y + (s16)object->panel.bounds.unsignedRect.h);
        if (bottomAdjustment < 0) {
            object->panel.bounds.unsignedRect.y += bottomAdjustment;
        }
        task->state = task->state + 1;
    } else {
        // Row callbacks may open a child; its result controls popup focus and closure.
        uiUpdateList(list, &object->panel);
        if (object->panel.control.word == USER_INTERFACE_PANEL_ACTIVE) {
            listAction = list->actionResult;
            if (listAction != USER_INTERFACE_LIST_ACTION_INPUT_CONSUMED) {
                if (listAction == USER_INTERFACE_LIST_ACTION_MOVE) {
                    object->result = listAction;
                } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, gPadMenuButtonMask) != 0) {
                    object->result = USER_INTERFACE_RESULT_CANCEL;
                } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, gPadCancelButtonMask) != 0) {
                    sndEvtRequestScriptStart(SOUND_MENU_CANCEL, 0, 0);
                    object->result = USER_INTERFACE_RESULT_CONFIRM;
                }
            }
        }
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
}

/// Caps the healing panel's stored HP and MP at their respective maxima.
///
/// Borrows the live writable status record after the effect has been applied.
/// Comparisons use the stored signed 16-bit values; negative values are retained.
static inline void _itemMenuClampHealedStats(PlayerStatus* player)
{
    if (player->hp > player->hpMax) {
        player->hp = player->hpMax;
    }
    if (player->mp > player->mpMax) {
        player->mp = player->mpMax;
    }
}

void itemMenuApplyHealingPanel(UiObject* object, Task* task, s32 healingId)
{
    enum {
        ITEM_MENU_HEALING_STATE_INIT       = 0,
        ITEM_MENU_HEALING_DISPLAY_UPDATES  = 188,
        ITEM_MENU_HEALING_RECOVERY_2       = 2,
        ITEM_MENU_HEALING_RECOVERY_3       = 3,
        ITEM_MENU_HEALING_COLA             = 5,
        ITEM_MENU_HEALING_MP_BOOST_1       = 6,
        ITEM_MENU_HEALING_MP_BOOST_2       = 7,
        ITEM_MENU_HEALING_MP_BOOST_COUNT   = 2,
        ITEM_MENU_HEALING_RINGERS_SOLUTION = 61,
        ITEM_MENU_HEALING_USE_COUNT_MAX    = 9999
    };
    PlayerStatus* player;
    HudHpMp*      displayedStats;
    McSaveData*   save;
    s32           previousHp;
    s32           previousMp;
    s32           panelWidth;
    s32           panelHeight;

    player         = &gPlayerStatus;
    object->result = USER_INTERFACE_RESULT_NONE;
    uiDrawPanelLabel(&object->panel, _gItemMenuHealingStatusTitle);
    // Apply the effect once, retaining the old values for the rising stat display.
    if (task->state == ITEM_MENU_HEALING_STATE_INIT) {
        uiSetPanelContentSize(&object->panel, 0xB0, 0x2F);
        panelWidth                          = (s16)object->panel.bounds.unsignedRect.w;
        panelHeight                         = (s16)object->panel.bounds.unsignedRect.h;
        object->panel.bounds.unsignedRect.x = -(panelWidth >> 1);
        object->panel.bounds.unsignedRect.y = -(panelHeight >> 1) - 0x10;
        previousHp                          = player->hp;
        displayedStats                      = &gHudDisplayedVitals;
        displayedStats->hp                  = previousHp;
        previousMp                          = player->mp;
        displayedStats->mp                  = previousMp;
        if (healingId < ITEM_TEXT_KEY_ID_FIRST) {
            if (healingId < ITEM_MENU_HEALING_RECOVERY_3 + 1) {
                if (previousHp < player->hpMax) {
                    inventoryRemoveItemRow(0, gItemMenuSelectedRow, 1);
                }
                if (healingId == ITEM_MENU_HEALING_RECOVERY_3) {
                    player->hp = player->hpMax;
                } else if (healingId == ITEM_MENU_HEALING_RECOVERY_2) {
                    player->hp = player->hp + 0x64;
                } else {
                    player->hp = player->hp + 0x32;
                }
            } else if (healingId == ITEM_MENU_HEALING_COLA) {
                if ((previousMp < player->mpMax) || (previousHp < player->hpMax)) {
                    inventoryRemoveItemRow(0, gItemMenuSelectedRow, 1);
                }
                player->mp = player->mp + 0x50;
                player->hp = player->hp + 0x14;
            } else if ((u32)(healingId - ITEM_MENU_HEALING_MP_BOOST_1) < (u32)ITEM_MENU_HEALING_MP_BOOST_COUNT) {
                if (previousMp < player->mpMax) {
                    inventoryRemoveItemRow(0, gItemMenuSelectedRow, 1);
                }
                if (healingId == ITEM_MENU_HEALING_MP_BOOST_2) {
                    player->mp = player->mpMax;
                } else {
                    player->mp = player->mp + 0x1E;
                }
            } else if (healingId == ITEM_MENU_HEALING_RINGERS_SOLUTION) {
                if ((previousMp < player->mpMax) || (previousHp < player->hpMax)) {
                    inventoryRemoveItemRow(0, gItemMenuSelectedRow, 1);
                }
                player->mp = player->mpMax;
                player->hp = player->hpMax;
            }
        } else if (previousHp < player->hpMax) {
            player->mp         = player->mp - attachmentGetPackedLevelValue(healingId, ATTACHMENT_LEVEL_CAST_COST);
            displayedStats->mp = player->mp;
            player->hp         = player->hp + attachmentGetPackedLevelValue(healingId, ATTACHMENT_LEVEL_AMOUNT);
            save               = &gMcSaveData[MEMORY_CARD_SAVE_LIVE];
            if (save->state.attachUseCounts[ATTACHMENT_INDEX_HEALING] < ITEM_MENU_HEALING_USE_COUNT_MAX) {
                save->state.attachUseCounts[ATTACHMENT_INDEX_HEALING] = save->state.attachUseCounts[ATTACHMENT_INDEX_HEALING] + 1;
            }
        }
        _itemMenuClampHealedStats(player);
        task->killCountdown = ITEM_MENU_HEALING_DISPLAY_UPDATES;
        task->state         = task->state + 1;
    }
    // Start the dismissal countdown only after both displayed values catch up.
    itemMenuDrawPlayerStats(&object->panel, 0);
    if (object->panel.control.word == USER_INTERFACE_PANEL_ACTIVE) {
        if (gHudDisplayedVitals.hp == player->hp) {
            if (gHudDisplayedVitals.mp == player->mp) {
                task->killCountdown = task->killCountdown - 1;
            }
        }
        if ((padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, gPadConfirmButtonMask | gPadCancelButtonMask) != 0) || (task->killCountdown < 0)) {
            gHudDisplayedVitals.hp = player->hp;
            gHudDisplayedVitals.mp = player->mp;
            object->result         = USER_INTERFACE_RESULT_DISMISS;
            task->killCountdown    = ITEM_MENU_COMPLETED_COUNTDOWN;
        }
    }
}

void itemMenuApplyWeaponAddonPanel(UiObject* object, Task* task)
{
    enum {
        ITEM_MENU_WEAPON_ADDON_STATE_INIT            = 0,
        ITEM_MENU_WEAPON_ADDON_SMG_CLIP_HOLDER       = 9,
        ITEM_MENU_WEAPON_ADDON_ACCEPTED              = 0xFF,
        ITEM_MENU_WEAPON_ADDON_NO_MORE_MODIFICATIONS = 0x1A,
        ITEM_MENU_WEAPON_ADDON_NEEDS_MP5A5           = 0x16,
        ITEM_MENU_WEAPON_ADDON_NEEDS_P08             = 0x19,
        ITEM_MENU_WEAPON_ADDON_NEEDS_EMPTY_SLOT      = 0x1B,
        ITEM_MENU_WEAPON_ADDON_NEEDS_M4A1            = 0x17,
        ITEM_MENU_WEAPON_ADDON_MP5A5_TWO_HOLDERS     = 0x9F,
        ITEM_MENU_WEAPON_ADDON_MP5A5_ONE_HOLDER      = 0x9E,
        ITEM_MENU_WEAPON_ADDON_MP5A5                 = 0x9D,
        ITEM_MENU_WEAPON_ADDON_P08_SNAIL             = 0x80,
        ITEM_MENU_WEAPON_ADDON_P08                   = 0x83,
        ITEM_MENU_WEAPON_ADDON_RIFLE_CLIP_HOLDER     = 0xA,
        ITEM_MENU_WEAPON_ADDON_SNAIL_MAGAZINE        = 0xC,
        ITEM_MENU_WEAPON_ADDON_HAMMER                = 0x42,
        ITEM_MENU_WEAPON_ADDON_PYKE                  = 0x43,
        ITEM_MENU_WEAPON_ADDON_JAVELIN               = 0x44,
        ITEM_MENU_WEAPON_ADDON_M203                  = 0x45,
        ITEM_MENU_WEAPON_ADDON_M9                    = 0x46,
        ITEM_MENU_WEAPON_ADDON_M4A1_ONE_HOLDER       = 0x93,
        ITEM_MENU_WEAPON_ADDON_M4A1_TWO_HOLDERS      = 0x94,
    };
    union {
        TextDrawReq               textRequest;
        _ItemMenuM4a1VariantTable variantTable;
    } scratch;
    s32                        createdWeaponItemId;
    s32                        returnedClipItemId;
    s32                        previousWeaponItemId;
    s32                        returnedAddonItemId;
    s32                        itemId;
    s32                        x;
    s32                        itemColorRgb;
    _ItemMenuWeaponCreateWork* work;
    s32                        y;
    s32                        replacementVariantIndex;
    s32                        carriedVariantIndex;
    s32                        energyLevelIndex;
    s32                        rowCount;
    s32                        savedAddonItemId;
    s32                        clipHolderItemId;
    s32                        hiddenState;
    s32                        textBaseY;
    u16                        remainingUpdates;
    InventoryItemRange*        carriedItems;
    InventoryItemRange*        carriedItemsForSwap;
    _ItemMenuWeaponCreateWork* newWork;
    _ItemMenuWeaponVariant*    variants;
    EquipmentWeaponLoad*       previousWeaponLoad;
    EquipmentWeaponLoad*       createdWeaponLoad;
    InventoryItemRow*          weaponRow;
    PlayerStatus*              player;

    if (task->state == ITEM_MENU_WEAPON_ADDON_STATE_INIT) {
        previousWeaponItemId = 0;
        createdWeaponItemId  = 0;
        returnedClipItemId   = 0;
        itemId               = task->spawnArg1.value;
        task->status         = ITEM_MENU_WEAPON_ADDON_ACCEPTED;
        returnedAddonItemId  = previousWeaponItemId;
        switch (itemId) {
            case ITEM_MENU_WEAPON_ADDON_SMG_CLIP_HOLDER:
                carriedItems = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems;
                if (inventoryGetItemQuantity(carriedItems, ITEM_MENU_WEAPON_ADDON_MP5A5_TWO_HOLDERS) != 0) {
                    task->status = ITEM_MENU_WEAPON_ADDON_NO_MORE_MODIFICATIONS;
                } else if (inventoryGetItemQuantity(carriedItems, ITEM_MENU_WEAPON_ADDON_MP5A5_ONE_HOLDER) != 0) {
                    previousWeaponItemId = ITEM_MENU_WEAPON_ADDON_MP5A5_ONE_HOLDER;
                    createdWeaponItemId  = ITEM_MENU_WEAPON_ADDON_MP5A5_TWO_HOLDERS;
                } else if (inventoryGetItemQuantity(carriedItems, ITEM_MENU_WEAPON_ADDON_MP5A5) != 0) {
                    previousWeaponItemId = ITEM_MENU_WEAPON_ADDON_MP5A5;
                    createdWeaponItemId  = ITEM_MENU_WEAPON_ADDON_MP5A5_ONE_HOLDER;
                } else {
                    task->status = ITEM_MENU_WEAPON_ADDON_NEEDS_MP5A5;
                }
                break;
            case ITEM_MENU_WEAPON_ADDON_SNAIL_MAGAZINE:
                carriedItems = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems;
                if (inventoryGetItemQuantity(carriedItems, ITEM_MENU_WEAPON_ADDON_P08_SNAIL) != 0) {
                    task->status = ITEM_MENU_WEAPON_ADDON_NO_MORE_MODIFICATIONS;
                } else if (inventoryGetItemQuantity(carriedItems, ITEM_MENU_WEAPON_ADDON_P08) != 0) {
                    previousWeaponItemId = ITEM_MENU_WEAPON_ADDON_P08;
                    createdWeaponItemId  = ITEM_MENU_WEAPON_ADDON_P08_SNAIL;
                } else {
                    task->status = ITEM_MENU_WEAPON_ADDON_NEEDS_P08;
                }
                break;
            case ITEM_MENU_WEAPON_ADDON_RIFLE_CLIP_HOLDER:
            case ITEM_MENU_WEAPON_ADDON_HAMMER:
            case ITEM_MENU_WEAPON_ADDON_PYKE:
            case ITEM_MENU_WEAPON_ADDON_JAVELIN:
            case ITEM_MENU_WEAPON_ADDON_M203:
            case ITEM_MENU_WEAPON_ADDON_M9:
                carriedItems = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems;
                if (inventoryGetItemQuantity(carriedItems, ITEM_MENU_WEAPON_ADDON_M4A1_TWO_HOLDERS) != 0) {
                    if (itemId == ITEM_MENU_WEAPON_ADDON_RIFLE_CLIP_HOLDER) {
                        task->status = ITEM_MENU_WEAPON_ADDON_NO_MORE_MODIFICATIONS;
                    } else if (inventoryCanAddItem(carriedItems, ITEM_MENU_WEAPON_ADDON_RIFLE_CLIP_HOLDER) != 0) {
                        returnedClipItemId = ITEM_MENU_WEAPON_ADDON_RIFLE_CLIP_HOLDER;
                    } else {
                        task->status = ITEM_MENU_WEAPON_ADDON_NEEDS_EMPTY_SLOT;
                    }
                }
                if (task->status == ITEM_MENU_WEAPON_ADDON_ACCEPTED) {
                    clipHolderItemId = ITEM_MENU_WEAPON_ADDON_RIFLE_CLIP_HOLDER;
                    // Copied into the slot the row text reuses once this carriedItems is done.
                    variants             = scratch.variantTable.variants;
                    scratch.variantTable = _gItemMenuM4a1VariantTable;
                    task->status         = ITEM_MENU_WEAPON_ADDON_NEEDS_M4A1;
                    // Find the variant being carriedVariantIndex; the add-on it has mounted comes back to the inventory.
                    for (carriedVariantIndex = 0; carriedVariantIndex < ARRAY_SIZE(scratch.variantTable.variants); carriedVariantIndex++) {
                        if (inventoryGetItemQuantity(&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems, variants[carriedVariantIndex].weaponItemId) != 0) {
                            returnedAddonItemId = variants[carriedVariantIndex].mountedItemId;
                            if (itemId == clipHolderItemId && variants[carriedVariantIndex].weaponItemId == ITEM_MENU_WEAPON_ADDON_M4A1_ONE_HOLDER) {
                                returnedAddonItemId = 0;
                            }
                            if (returnedAddonItemId != clipHolderItemId && returnedAddonItemId == itemId) {
                                task->status = ITEM_MENU_WEAPON_ADDON_NO_MORE_MODIFICATIONS;
                            } else {
                                // The weapon becomes the other variant that has the used itemId mounted.
                                previousWeaponItemId = variants[carriedVariantIndex].weaponItemId;
                                for (replacementVariantIndex = 0; replacementVariantIndex < ARRAY_SIZE(scratch.variantTable.variants); replacementVariantIndex++) {
                                    if (itemId == variants[replacementVariantIndex].mountedItemId && previousWeaponItemId != variants[replacementVariantIndex].weaponItemId) {
                                        task->status        = ITEM_MENU_WEAPON_ADDON_ACCEPTED;
                                        createdWeaponItemId = variants[replacementVariantIndex].weaponItemId;
                                        break;
                                    }
                                }
                            }
                            break;
                        }
                    }
                }
                break;
        }
        if (task->status == ITEM_MENU_WEAPON_ADDON_ACCEPTED) {
            // Replace the carried variant while transferring ammunition and returning displaced add-ons.
            player              = &gPlayerStatus;
            previousWeaponLoad  = equipmentGetWeaponLoad(previousWeaponItemId);
            createdWeaponLoad   = equipmentGetWeaponLoad(createdWeaponItemId);
            weaponRow           = inventoryFindLastCarriedItemRow(previousWeaponItemId);
            newWork             = memCalloc(sizeof(_ItemMenuWeaponCreateWork), 0);
            carriedItemsForSwap = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems;
            task->work          = newWork;
            inventoryRemoveItemRow(carriedItemsForSwap, gItemMenuSelectedRow, 1);
            weaponRow->itemId = createdWeaponItemId;
            equipmentClearSelectedRemovableLoads(createdWeaponItemId, EQUIPMENT_CLEAR_LOAD_BOTH);
            createdWeaponLoad->primaryItemId = previousWeaponLoad->primaryItemId;
            equipmentLoadWeaponConsumable(carriedItemsForSwap, createdWeaponItemId, createdWeaponLoad->primaryItemId, previousWeaponLoad->primaryQty);
            if ((returnedAddonItemId == 0) && (createdWeaponLoad->secondaryItemId == previousWeaponLoad->secondaryItemId)) {
                createdWeaponLoad->secondaryQty = previousWeaponLoad->secondaryQty;
            }
            equipmentClearSelectedRemovableLoads(previousWeaponItemId, EQUIPMENT_CLEAR_LOAD_BOTH);
            if (player->weapon == (previousWeaponItemId - (EQUIPMENT_WEAPON_ITEM_FIRST - 1))) {
                player->weapon = createdWeaponItemId - (EQUIPMENT_WEAPON_ITEM_FIRST - 1);
            }
            if (returnedAddonItemId != 0) {
                inventoryGiveItem(&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems, returnedAddonItemId, INVENTORY_GIVE_ONE_PACK);
            }
            if (returnedClipItemId != 0) {
                inventoryGiveItem(&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems, returnedClipItemId, INVENTORY_GIVE_ONE_PACK);
            }
            newWork->createdWeaponItemId  = createdWeaponItemId;
            newWork->previousWeaponItemId = previousWeaponItemId;
            newWork->returnedAddonItemId  = returnedAddonItemId;
            rowCount                      = 5;
            newWork->returnedClipItemId   = returnedClipItemId;
            newWork->usedAddonItemId      = itemId;
            if (returnedAddonItemId != 0) {
                rowCount = 6;
            }
            if (returnedClipItemId != 0) {
                rowCount += 1;
            }
            uiSetPanelContentSize(&(object)->panel, 0xA8, uiGetTextRowsHeight(rowCount) + 1);
            (&(object)->panel)->bounds.rect.x = (-(&(object)->panel)->bounds.rect.w) >> 1;
            (&(object)->panel)->bounds.rect.y = ((-(&(object)->panel)->bounds.rect.h) >> 1) - 0x14;
            task->killCountdown               = ITEM_MENU_RESULT_NOTICE_UPDATES;
            task->state                       = task->state + 1;
        }
    }
    if (task->status != ITEM_MENU_WEAPON_ADDON_ACCEPTED) {
        savedAddonItemId      = task->spawnArg1.value;
        task->spawnArg1.value = task->status;
        itemMenuNoticeTask(task);
        task->spawnArg1.value = savedAddonItemId;
        return;
    }
    itemColorRgb = ITEM_MENU_RESULT_ITEM_COLOR;
    y            = object->panel.contentTop.signedValue + 0xF;
    work         = task->work;
    x            = object->panel.contentLeft.signedValue + 2;
    uiDrawPanelLabel(&(object)->panel, (const char*)gItemMenuNoticePanelTitle);
    hiddenState = USER_INTERFACE_PANEL_HIDDEN;
    itemId      = work->previousWeaponItemId;
    if (object->panel.state != hiddenState) {
        ITEM_MENU_DRAW_UNMARKED_ROW_CONTENTS(object, scratch.textRequest, x, y, itemId, itemColorRgb, textBaseY, energyLevelIndex);
    }
    hiddenState = USER_INTERFACE_PANEL_HIDDEN;
    itemId      = work->usedAddonItemId;
    y          += 0xF;
    if (object->panel.state != hiddenState) {
        ITEM_MENU_DRAW_UNMARKED_ROW_CONTENTS(object, scratch.textRequest, x, y, itemId, itemColorRgb, textBaseY, energyLevelIndex);
    }
    y += 0xF;
    textDrawUiLine(object, x, y, gItemMenuAddonUsedText, ITEM_MENU_PANEL_TEXT_COLOR, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    hiddenState = USER_INTERFACE_PANEL_HIDDEN;
    itemId      = work->createdWeaponItemId;
    y          += 0xF;
    if (object->panel.state != hiddenState) {
        ITEM_MENU_DRAW_UNMARKED_ROW_CONTENTS(object, scratch.textRequest, x, y, itemId, itemColorRgb, textBaseY, energyLevelIndex);
    }
    itemId = work->returnedAddonItemId;
    // The clip row remains conditional on a returned add-on, even if a clip was returned independently.
    if (itemId != 0) {
        y += 0xF;
        if (object->panel.state != USER_INTERFACE_PANEL_HIDDEN) {
            ITEM_MENU_DRAW_UNMARKED_ROW_CONTENTS(object, scratch.textRequest, x, y, itemId, itemColorRgb, textBaseY, energyLevelIndex);
        }
        itemId = work->returnedClipItemId;
        if (itemId != 0) {
            y += 0xF;
            if (object->panel.state != USER_INTERFACE_PANEL_HIDDEN) {
                ITEM_MENU_DRAW_UNMARKED_ROW_CONTENTS(object, scratch.textRequest, x, y, itemId, itemColorRgb, textBaseY, energyLevelIndex);
            }
        }
    }
    textDrawUiLine(object, x, y + 0xF, gItemMenuAddonCreatedText, ITEM_MENU_PANEL_TEXT_COLOR, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    if (object->panel.control.word == USER_INTERFACE_PANEL_ACTIVE) {
        remainingUpdates    = task->killCountdown - 1;
        task->killCountdown = remainingUpdates;
        if ((((s32)(remainingUpdates << 0x10)) <= 0) || (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, gPadConfirmButtonMask | gPadCancelButtonMask) != 0)) {
            object->result      = USER_INTERFACE_RESULT_DISMISS;
            task->killCountdown = ITEM_MENU_COMPLETED_COUNTDOWN;
            return;
        }
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, gPadMenuButtonMask) != 0) {
            object->result      = USER_INTERFACE_RESULT_CANCEL;
            task->killCountdown = ITEM_MENU_COMPLETED_COUNTDOWN;
        }
    }
}

/// Fits and centers the two-line notice for invoking a Parasite Energy item.
///
/// Requires initialized panel bounds, style and content coordinates, retaining
/// its frame insets. Borrows read-only itemName under `textMeasureLineWidth`'s
/// contract. Width covers "Invoked" or the item
/// name with room for its trailing period, plus five content-margin pixels.
/// Height covers two fifteen-pixel rows plus one margin pixel. The outer bounds
/// are centered twenty pixels above screen center; halving negative signed
/// extents rounds toward negative infinity before the halfword stores.
static inline void _itemMenuSizeInvocationPanel(UiPanel* panel, const u8* itemName)
{
    enum {
        ITEM_MENU_INVOCATION_SUFFIX_ALLOWANCE_PIXELS = 11,
        ITEM_MENU_INVOCATION_TEXT_ROWS               = 2,
        ITEM_MENU_INVOCATION_CENTER_Y_PIXELS         = -20
    };
    s32 contentWidth;
    s32 invokedTextWidth;

    contentWidth     = textMeasureLineWidth(itemName) + ITEM_MENU_INVOCATION_SUFFIX_ALLOWANCE_PIXELS;
    invokedTextWidth = textMeasureLineWidth(gItemMenuInvokedText);
    if (contentWidth < invokedTextWidth) {
        contentWidth = invokedTextWidth;
    }
    uiSetPanelContentSize(panel, contentWidth + 5, uiGetTextRowsHeight(ITEM_MENU_INVOCATION_TEXT_ROWS) + 1);
    panel->bounds.rect.x = (-panel->bounds.rect.w) >> 1;
    panel->bounds.rect.y = ((-panel->bounds.rect.h) >> 1) + ITEM_MENU_INVOCATION_CENTER_Y_PIXELS;
}

void itemMenuInvokeParasiteEnergyItem(UiObject* object, Task* task, s32 itemId)
{
    enum {
        ITEM_MENU_INVOKE_STATE_INIT = 0
    };
    const u8*     itemName;
    s32           nameEndX;
    PlayerStatus* player;
    McSaveData*   save;
    s32           level;
    s32           abilityIndex;
    s32           elementIndex;
    s32           elementAbilityIndex;

    itemName = itemGetText(itemId, ITEM_TEXT_NAME, 0);
    if (task->state == ITEM_MENU_INVOKE_STATE_INIT) {
        _itemMenuSizeInvocationPanel(&object->panel, itemName);
        inventoryRemoveItemRow(&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems, gItemMenuSelectedRow, 1);

        // Decode the ordinary item into its element, ability slot and one-based level.
        abilityIndex = (itemId - ITEM_MENU_PARASITE_ENERGY_ITEM_FIRST) / ATTACHMENT_LEVEL_COUNT;
        level        = itemId - ITEM_MENU_PARASITE_ENERGY_ITEM_FIRST;
        elementIndex = elementAbilityIndex = abilityIndex / ITEM_MENU_INVOKE_ABILITIES_PER_ELEMENT;
        elementAbilityIndex                = abilityIndex - elementIndex * ITEM_MENU_INVOKE_ABILITIES_PER_ELEMENT;
        save                               = &gMcSaveData[MEMORY_CARD_SAVE_LIVE];
        level                              = level - abilityIndex * ATTACHMENT_LEVEL_COUNT + 1;
        player                             = &gPlayerStatus;
        if (save->state.attachLevels[elementAbilityIndex + elementIndex * ITEM_MENU_INVOKE_ABILITIES_PER_ELEMENT] < level) {
            save->state.attachLevels[elementAbilityIndex + elementIndex * ITEM_MENU_INVOKE_ABILITIES_PER_ELEMENT] = level;
        }
        equipmentRecalculateMaxMp();
        player->mp             = player->mpMax;
        gHudDisplayedVitals.mp = player->mp;
        task->killCountdown    = ITEM_MENU_RESULT_NOTICE_UPDATES;
        task->state            = task->state + 1;
    }

    uiDrawPanelLabel(&(object)->panel, _gItemMenuInvokeTitle);
    textDrawUiLine(object, object->panel.contentLeft.signedValue + 2, object->panel.contentTop.signedValue + 0xF, gItemMenuInvokedText, ITEM_MENU_PANEL_TEXT_COLOR, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    nameEndX = textDrawUiLine(object, object->panel.contentLeft.signedValue + 2, object->panel.contentTop.signedValue + 0x1E, itemName, ITEM_MENU_RESULT_ITEM_COLOR, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    textDrawUiLine(object, nameEndX, object->panel.contentTop.signedValue + 0x1E, gItemMenuPeriodText, ITEM_MENU_PANEL_TEXT_COLOR, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    // Invocation notices advance their timeout even while the panel is inactive.
    task->killCountdown--;
    if (object->panel.control.word == USER_INTERFACE_PANEL_ACTIVE) {
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, gPadMenuButtonMask) != 0) {
            object->result = USER_INTERFACE_RESULT_CANCEL;
        } else if ((task->killCountdown <= 0) || (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, gPadConfirmButtonMask | gPadCancelButtonMask) != 0)) {
            object->result      = USER_INTERFACE_RESULT_DISMISS;
            task->killCountdown = ITEM_MENU_COMPLETED_COUNTDOWN;
        }
    }
}

void itemMenuInvokeElementBoostPanel(UiObject* object, Task* task)
{
    enum {
        ITEM_MENU_ELEMENT_BOOST_STATE_INIT = 0,
        ITEM_MENU_ELEMENT_BOOST_ITEM_FIRST = 0x36
    };
    McSaveData* save;
    McSaveData* levelSave;
    u8*         saveBytesForElement;
    s32         elementIndex;
    s32         abilitySelection;
    s32         abilityIndex;

    elementIndex = task->spawnArg1.value - ITEM_MENU_ELEMENT_BOOST_ITEM_FIRST;
    if (task->state == ITEM_MENU_ELEMENT_BOOST_STATE_INIT) {
        // Keep the grouped reads inside the save's byte representation, with named member offsets.
        save                = &gMcSaveData[MEMORY_CARD_SAVE_LIVE];
        saveBytesForElement = (u8*)save + elementIndex * ITEM_MENU_INVOKE_ABILITIES_PER_ELEMENT;
        abilitySelection    = saveBytesForElement[OFFSET_OF(McSaveData, state.attachLevels)] > saveBytesForElement[OFFSET_OF(McSaveData, state.attachLevels) + 1];
        if (save->state.attachLevels[abilitySelection + elementIndex * ITEM_MENU_INVOKE_ABILITIES_PER_ELEMENT] >= ITEM_MENU_INVOKE_MAX_LEVEL) {
            abilitySelection = 2;
            if (save->state.attachLevels[elementIndex * ITEM_MENU_INVOKE_ABILITIES_PER_ELEMENT + 2] >= ITEM_MENU_INVOKE_MAX_LEVEL) {
                abilitySelection = (elementIndex * ITEM_MENU_INVOKE_ABILITIES_PER_ELEMENT + 2) * ATTACHMENT_LEVEL_COUNT + ITEM_MENU_PARASITE_ENERGY_ITEM_FIRST + ITEM_MENU_INVOKE_MAX_LEVEL - 1;
                goto storeInvocation;
            }
        }
        // Reuse the selection value for the ordinary P.E. item id after choosing its group slot.
        levelSave        = &gMcSaveData[MEMORY_CARD_SAVE_LIVE];
        abilityIndex     = abilitySelection + elementIndex * ITEM_MENU_INVOKE_ABILITIES_PER_ELEMENT;
        abilitySelection = levelSave->state.attachLevels[abilityIndex] + abilityIndex * ATTACHMENT_LEVEL_COUNT + ITEM_MENU_PARASITE_ENERGY_ITEM_FIRST;
    storeInvocation:
        task->extraState.value = abilitySelection;
    }
    itemMenuInvokeParasiteEnergyItem(object, task, task->extraState.value);
}

void itemMenuDialogTask(Task* task)
{
    enum { ITEM_MENU_DIALOG_STATE_INIT = 0 };
    UiObject* object;
    UiList*   dialogList;
    s32       commandMode;
    s32       actionResult;

    object         = task->spawnArg2.pointer;
    dialogList     = &gItemMenuDialogList;
    object->result = USER_INTERFACE_RESULT_NONE;
    if (task->state == ITEM_MENU_DIALOG_STATE_INIT) {
        commandMode = task->spawnArg1.value & ITEM_MENU_DIALOG_LAYOUT_MASK;
        switch (commandMode) {
            case ITEM_MENU_DIALOG_OK:
                gItemMenuDialogRowCallbacks[0] = itemMenuDrawOkRow;
                dialogList->itemCount          = commandMode;
                break;
            case ITEM_MENU_DIALOG_CANCEL:
                gItemMenuDialogRowCallbacks[0] = itemMenuDrawCancelRow;
                dialogList->itemCount          = 1;
                break;
            case ITEM_MENU_DIALOG_NO_SELECTED:
                gItemMenuDialogRowCallbacks[0] = itemMenuDrawYesRow;
                gItemMenuDialogRowCallbacks[1] = itemMenuDrawNoRow;
                dialogList->itemCount          = ARRAY_SIZE(gItemMenuDialogRowCallbacks);
                break;
            default:
                gItemMenuDialogRowCallbacks[0] = itemMenuDrawYesRow;
                gItemMenuDialogRowCallbacks[1] = itemMenuDrawNoRow;
                dialogList->itemCount          = ARRAY_SIZE(gItemMenuDialogRowCallbacks);
                break;
        }
        dialogList->visibleRowCount.unsignedValue = dialogList->itemCount;
        uiFitPanelToList(dialogList, &object->panel);
        object->panel.bounds.unsignedRect.y -= (s16)object->panel.bounds.unsignedRect.h / 2;
        if (task->spawnArg1.value & ITEM_MENU_DIALOG_SYSTEM_CURSOR_SOUND) {
            uiSetListSystemCursorSound(dialogList, 1);
        } else {
            uiSetListSystemCursorSound(dialogList, 0);
        }
        if ((task->spawnArg1.value & ITEM_MENU_DIALOG_LAYOUT_MASK) == ITEM_MENU_DIALOG_NO_SELECTED) {
            dialogList->selectedItemIndex = 1;
        } else {
            dialogList->selectedItemIndex = 0;
        }
        task->state = task->state + 1;
    }
    uiUpdateList(dialogList, &object->panel);
    if (object->panel.control.word == USER_INTERFACE_PANEL_ACTIVE) {
        actionResult = dialogList->actionResult;
        if (actionResult == USER_INTERFACE_RESULT_CONFIRM) {
            object->result      = actionResult;
            object->resultValue = dialogList->commandResult.unsignedValue;
        }
    }
}

/// Draws the available EXP and current/maximum MP above the elemental P.E. lists.
///
/// Borrows panel coordinates and live player stats for this draw. Small labels
/// anchor the medium decimal values in content pixels, translated with the
/// unsigned origin views before signed-halfword narrowing. EXP is formatted as
/// unsigned; MP and maximum MP retain their signed values. Each formatter needs
/// at most ten bytes, fitting the reusable 32-byte buffer. Drawing requires
/// loaded font textures/palettes and primitive capacity under `textDrawString`;
/// the header uses panel OT base + 1 and base + 2, without testing visibility.
static inline void _itemMenuDrawParasiteEnergyHeader(const UiPanel* panel)
{
    u8                  numberText[32];
    TextDrawReq         expLabelRequest;
    TextDrawReq         expValueRequest;
    TextDrawReq         mpLabelRequest;
    TextDrawReq         mpValueRequest;
    TextDrawReq         slashRequest;
    TextDrawReq         maxMpRequest;
    s32                 contentLeft;
    const PlayerStatus* player;
    s32                 textColorRgb;
    s32                 labelX;
    s32                 valueY;

    /// Sets depth, color and glyph style without changing the request's position.
    ///
    /// request is a simple writable TextDrawReq lvalue, evaluated five times;
    /// other arguments are each evaluated once and must be free of side effects.
    /// panel supplies the signed OT base; selectors narrow to request bytes.
#define ITEM_MENU_SET_PARASITE_ENERGY_HEADER_TEXT_STYLE(request, panel, textColor, glyphFace, textAlignment, textDrawMode) \
    {                                                                                                                      \
        (request).otIndex    = (panel)->otIndex.signedValue + 1;                                                           \
        (request).colorRgb   = (textColor);                                                                                \
        (request).glyphTable = (glyphFace);                                                                                \
        (request).alignment  = (textAlignment);                                                                            \
        (request).drawMode   = (textDrawMode);                                                                             \
    }

    textColorRgb      = ITEM_MENU_PANEL_TEXT_COLOR;
    player            = &gPlayerStatus;
    contentLeft       = panel->contentLeft.signedValue;
    labelX            = contentLeft + 34;
    valueY            = panel->contentTop.signedValue + 8;
    expLabelRequest.x = panel->contentOriginX.unsignedValue + labelX;
    expLabelRequest.y = panel->contentOriginY.unsignedValue + (valueY - 2);
    ITEM_MENU_SET_PARASITE_ENERGY_HEADER_TEXT_STYLE(expLabelRequest, panel, textColorRgb, TEXT_GLYPH_TABLE_SMALL, TEXT_ALIGNMENT_RIGHT, TEXT_DRAW_OUTLINED);
    textDrawString(&expLabelRequest, gItemMenuExpLabel);
    expValueRequest.x = panel->contentOriginX.unsignedValue + 10 + labelX;
    expValueRequest.y = panel->contentOriginY.unsignedValue + valueY;
    ITEM_MENU_SET_PARASITE_ENERGY_HEADER_TEXT_STYLE(expValueRequest, panel, textColorRgb, TEXT_GLYPH_TABLE_MEDIUM, TEXT_ALIGNMENT_LEFT, TEXT_DRAW_TRANSLUCENT_OUTLINED);
    textDrawString(&expValueRequest, textItoaUnsigned(numberText, player->exp));
    labelX           = contentLeft + 122;
    mpLabelRequest.x = panel->contentOriginX.unsignedValue + labelX;
    mpLabelRequest.y = panel->contentOriginY.unsignedValue + (valueY - 2);
    ITEM_MENU_SET_PARASITE_ENERGY_HEADER_TEXT_STYLE(mpLabelRequest, panel, textColorRgb, TEXT_GLYPH_TABLE_SMALL, TEXT_ALIGNMENT_RIGHT, TEXT_DRAW_OUTLINED);
    textDrawString(&mpLabelRequest, gItemMenuMpLabel);
    mpValueRequest.x = panel->contentOriginX.unsignedValue + 10 + labelX;
    mpValueRequest.y = panel->contentOriginY.unsignedValue + valueY;
    ITEM_MENU_SET_PARASITE_ENERGY_HEADER_TEXT_STYLE(mpValueRequest, panel, textColorRgb, TEXT_GLYPH_TABLE_MEDIUM, TEXT_ALIGNMENT_LEFT, TEXT_DRAW_TRANSLUCENT_OUTLINED);
    textDrawString(&mpValueRequest, textItoaSigned(numberText, player->mp));
    slashRequest.x = panel->contentOriginX.unsignedValue + 37 + labelX;
    slashRequest.y = panel->contentOriginY.unsignedValue + valueY;
    ITEM_MENU_SET_PARASITE_ENERGY_HEADER_TEXT_STYLE(slashRequest, panel, textColorRgb, TEXT_GLYPH_TABLE_MEDIUM, TEXT_ALIGNMENT_CENTER, TEXT_DRAW_TRANSLUCENT_OUTLINED);
    textDrawString(&slashRequest, gItemMenuFractionSeparatorText);
    maxMpRequest.x = panel->contentOriginX.unsignedValue + 42 + labelX;
    maxMpRequest.y = panel->contentOriginY.unsignedValue + valueY;
    ITEM_MENU_SET_PARASITE_ENERGY_HEADER_TEXT_STYLE(maxMpRequest, panel, textColorRgb, TEXT_GLYPH_TABLE_MEDIUM, TEXT_ALIGNMENT_LEFT, TEXT_DRAW_TRANSLUCENT_OUTLINED);
    textDrawString(&maxMpRequest, textItoaSigned(numberText, player->mpMax));
#undef ITEM_MENU_SET_PARASITE_ENERGY_HEADER_TEXT_STYLE
}

void itemMenuParasiteEnergyListTask(Task* task)
{
    enum {
        ITEM_MENU_PE_LIST_STATE_INIT    = 0,
        ITEM_MENU_PE_LIST_CONFIRMED     = 1,
        ITEM_MENU_PE_LIST_ELEMENT_FIRE  = 0,
        ITEM_MENU_PE_LIST_ELEMENT_WIND  = 1,
        ITEM_MENU_PE_LIST_ELEMENT_WATER = 2,
        ITEM_MENU_PE_LIST_ELEMENT_EARTH = 3,
        ITEM_MENU_PE_LIST_OPEN_DELAY    = 1
    };
    UiObject*     object;
    UiObjectDesc* elementPanelDescs;
    Task*         firstChild;
    Task*         childTask;
    UiObject*     childObject;
    s32           childResult;

    object         = task->spawnArg2.pointer;
    object->result = USER_INTERFACE_RESULT_NONE;
    uiDrawTitle(&(object)->panel, _gItemMenuPeListTitle);
    if (task->state == ITEM_MENU_PE_LIST_STATE_INIT) {
        elementPanelDescs = gItemMenuPeElementPanelDescs;
        uiSpawnObject(elementPanelDescs, ITEM_MENU_PE_LIST_ELEMENT_FIRE, USER_INTERFACE_PANEL_ACTIVE, ITEM_MENU_PE_LIST_OPEN_DELAY, object);
        uiSpawnObject(elementPanelDescs + 1, ITEM_MENU_PE_LIST_ELEMENT_WIND, USER_INTERFACE_PANEL_INACTIVE, ITEM_MENU_PE_LIST_OPEN_DELAY, object);
        uiSpawnObject(elementPanelDescs + 2, ITEM_MENU_PE_LIST_ELEMENT_WATER, USER_INTERFACE_PANEL_INACTIVE, ITEM_MENU_PE_LIST_OPEN_DELAY, object);
        uiSpawnObject(elementPanelDescs + 3, ITEM_MENU_PE_LIST_ELEMENT_EARTH, USER_INTERFACE_PANEL_INACTIVE, ITEM_MENU_PE_LIST_OPEN_DELAY, object);
        task->state = task->state + 1;
    }
    _itemMenuDrawParasiteEnergyHeader(&object->panel);
    firstChild = task->firstChild;
    if (firstChild != NULL) {
        childTask = firstChild;
        do {
            childObject = childTask->spawnArg2.pointer;
            childResult = childObject->result;
            childTask   = childTask->nextSibling;
            switch (childResult) {
                case USER_INTERFACE_RESULT_CONFIRM:
                    object->resultValue = ITEM_MENU_PE_LIST_CONFIRMED;
                    /* fallthrough */
                case USER_INTERFACE_RESULT_CANCEL:
                    object->result = childResult;
                    break;
            }
        } while (childTask != task->firstChild);
    }
}

void itemMenuInventoryCountTask(Task* task)
{
    enum {
        ITEM_MENU_INVENTORY_COUNT_SHOW       = 0,
        ITEM_MENU_INVENTORY_COUNT_HIDE       = 1,
        ITEM_MENU_INVENTORY_COUNT_OPEN_DELAY = 16
    };
    u8                  countText[0x20];
    u8                  capacityText[0x20];
    TextDrawReq         countRequest;
    TextDrawReq         labelRequest;
    UiObject*           object;
    InventoryItemRange* carriedItems;
    s32                 occupiedRows;
    s32                 rowCapacity;
    s32                 textColorRgb;
    s32                 rowY;
    s32                 countBaseX;
    s32                 countBaseY;
    s32                 labelBaseY;

    object         = task->spawnArg2.pointer;
    object->result = USER_INTERFACE_RESULT_NONE;
    if ((gItemMenuEquipmentDetailOpen == ITEM_MENU_INVENTORY_COUNT_HIDE) && (uiIsPanelHidingOrHidden(object) == 0)) {
        uiStartPanelHiding(object, object->owner);
    } else if ((gItemMenuEquipmentDetailOpen == ITEM_MENU_INVENTORY_COUNT_SHOW) && (uiIsPanelHidingOrHidden(object) == 1)) {
        uiLimitHiddenDelayOrOpen(&(object)->panel, object->owner, ITEM_MENU_INVENTORY_COUNT_OPEN_DELAY);
    }
    rowY         = object->panel.contentTop.signedValue + 0xD;
    countText[0] = _gItemMenuCountTextTerminator;
    memset(&countText[1], 0, sizeof(countText) - sizeof(countText[0]));
    textColorRgb = ITEM_MENU_PANEL_TEXT_COLOR;
    carriedItems = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems;
    occupiedRows = inventoryCountOccupiedRows(carriedItems);
    rowCapacity  = inventoryGetRangeCapacity(carriedItems);
    textItoaUnsigned(countText, occupiedRows);
    textAppendString(countText, gItemMenuFractionSeparatorText);
    textItoaUnsigned(capacityText, rowCapacity);
    textAppendString(countText, capacityText);
    countBaseX              = object->panel.contentOriginX.unsignedValue - 2;
    countRequest.x          = object->panel.contentRight.unsignedValue + countBaseX;
    countBaseY              = object->panel.contentOriginY.unsignedValue - 3;
    countRequest.y          = countBaseY + rowY;
    countRequest.otIndex    = object->panel.otIndex.signedValue + 1;
    countRequest.colorRgb   = textColorRgb;
    countRequest.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
    countRequest.alignment  = TEXT_ALIGNMENT_RIGHT;
    countRequest.drawMode   = TEXT_DRAW_TRANSLUCENT_OUTLINED;
    textDrawString(&countRequest, countText);
    labelRequest.x          = object->panel.contentLeft.unsignedValue + (object->panel.contentOriginX.unsignedValue + 2);
    labelBaseY              = object->panel.contentOriginY.unsignedValue - 6;
    labelRequest.y          = labelBaseY + rowY;
    labelRequest.otIndex    = object->panel.otIndex.signedValue + 1;
    labelRequest.colorRgb   = textColorRgb;
    labelRequest.glyphTable = TEXT_GLYPH_TABLE_SMALL;
    labelRequest.alignment  = TEXT_ALIGNMENT_LEFT;
    labelRequest.drawMode   = TEXT_DRAW_OUTLINED;
    textDrawString(&labelRequest, _gItemMenuInventoryTotalText);
}

void itemPickupPanelTask(Task* task)
{
    enum {
        ITEM_PICKUP_PANEL_STATE_INIT   = 0,
        ITEM_PICKUP_PANEL_TIMEOUT_ONLY = 0x10000,
        ITEM_PICKUP_PANEL_OPEN_DELAY   = 1
    };
    UiObject*           object;
    UiObject*           noticeObject;
    UiObject*           childObject;
    const UiObjectDesc* pickupPanelDescs;
    Task*               firstChild;
    Task*               childTask;
    Task*               nextChild;
    s32                 childResult;

    object         = task->spawnArg2.pointer;
    object->result = USER_INTERFACE_RESULT_NONE;
    if (task->state == ITEM_PICKUP_PANEL_STATE_INIT) {
        pickupPanelDescs = _gItemPickupChildPanelDescs;
        uiSpawnObject(pickupPanelDescs + ITEM_PICKUP_CHILD_PANEL_TITLE, 0, USER_INTERFACE_PANEL_INACTIVE, ITEM_PICKUP_PANEL_OPEN_DELAY, object);
        if (task->spawnArg1.value != 0) {
            inventorySetCollectedBit(gItemPickupPublishedKind);
            sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
            // This original one-past-table access reaches the separate adjacent notice recipe.
            noticeObject = uiSpawnObject(pickupPanelDescs + ARRAY_SIZE(_gItemPickupChildPanelDescs), gItemPickupPublishedKind | ITEM_PICKUP_PANEL_TIMEOUT_ONLY, USER_INTERFACE_PANEL_ACTIVE, ITEM_PICKUP_PANEL_OPEN_DELAY, object);
            if (noticeObject != NULL) {
                noticeObject->resultValue = USER_INTERFACE_LIST_COMMAND_YES;
            }
        } else if (inventoryCanAddItem(&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems, gItemPickupPublishedKind) != 0) {
            uiSpawnObject(pickupPanelDescs + ITEM_PICKUP_CHILD_PANEL_QUESTION, 0, USER_INTERFACE_PANEL_ACTIVE, ITEM_PICKUP_PANEL_OPEN_DELAY, object);
        } else {
            uiSpawnObject(pickupPanelDescs + ITEM_PICKUP_CHILD_PANEL_INVENTORY_FULL, 0, USER_INTERFACE_PANEL_ACTIVE, ITEM_PICKUP_PANEL_OPEN_DELAY, object);
        }
        task->state = task->state + 1;
    }
    firstChild = task->firstChild;
    if (firstChild != NULL) {
        childTask = firstChild;
        do {
            childObject = childTask->spawnArg2.pointer;
            childResult = childObject->result;
            nextChild   = childTask->nextSibling;
            if (childResult != USER_INTERFACE_RESULT_CANCEL) {
                if (childResult == USER_INTERFACE_RESULT_CONFIRM) {
                    object->panel.control.word = USER_INTERFACE_PANEL_ACTIVE;
                    uiStartTreeClosing(childObject, childObject->owner);
                }
            } else {
                object->result      = childResult;
                object->resultValue = childObject->resultValue;
            }
            childTask = nextChild;
        } while (childTask != task->firstChild);
    }
}

/// Replaces the menu previews with a pickup and requests its display resources.
///
/// Clears all five shared slots, including the two beyond the three load
/// profiles, before publishing itemId in `CD_COMMAND_DISPLAY_LOAD_MENU` (0).
/// -1 clears the ids without requesting resources; other values are published
/// even if `itemMenuEnqueuePreviewLoad` rejects the id or suppresses the load.
/// No resource is released or waited for here; the menu resource system retains
/// ownership and the caller waits for readiness before drawing the preview.
static inline void _itemPickupRequestPreview(s32 itemId)
{
    enum {
        ITEM_PICKUP_PREVIEW_PROFILE_COUNT = CD_COMMAND_DISPLAY_LOAD_RELOCATED_PREVIEW + 1
    };
    s32* previewItemIds;
    s32  profileIndex;

    previewItemIds    = gItemMenuPreviewItemIds;
    previewItemIds[2] = ITEM_MENU_PREVIEW_EMPTY;
    previewItemIds[1] = ITEM_MENU_PREVIEW_EMPTY;
    previewItemIds[0] = ITEM_MENU_PREVIEW_EMPTY;
    previewItemIds[3] = ITEM_MENU_PREVIEW_EMPTY;
    previewItemIds[4] = ITEM_MENU_PREVIEW_EMPTY;
    if (itemId != ITEM_MENU_PREVIEW_EMPTY) {
        // Republish all load-profile slots after the full reset.
        for (profileIndex = 0; profileIndex < ITEM_PICKUP_PREVIEW_PROFILE_COUNT; profileIndex++, previewItemIds++) {
            if (profileIndex == CD_COMMAND_DISPLAY_LOAD_MENU) {
                gItemMenuPreviewItemIds[CD_COMMAND_DISPLAY_LOAD_MENU] = itemId;
            } else {
                *previewItemIds = ITEM_MENU_PREVIEW_EMPTY;
            }
        }
        itemMenuEnqueuePreviewLoad(itemId, CD_COMMAND_DISPLAY_LOAD_MENU);
    }
}

/// Shows the published pickup's equipment-scale preview after its resources are ready.
///
/// spawnArg2 borrows the live task-owned UiObject; the published catalog id
/// must remain stable. State 0 resets the shared request slots and queues menu
/// profile 0. Waiting ends when a scene payload is available or the CD queue
/// is idle. Until then only the recessed preview frame is drawn. The loaded
/// preview remains owned by the menu resource system through task teardown.
static void _itemPickupPreviewTask(Task* task)
{
    enum {
        ITEM_PICKUP_PREVIEW_STATE_INIT      = 0,
        ITEM_PICKUP_PREVIEW_STATE_READY     = 1,
        ITEM_PICKUP_PREVIEW_STATE_WAIT_LOAD = 2
    };
    UiObject*   object;
    CdCmdQueue* cdQueue;
    s32         itemId;
    s32         previewFlags;

    itemId         = gItemPickupPublishedKind;
    object         = task->spawnArg2.pointer;
    cdQueue        = &gCdCmdQueue;
    object->result = USER_INTERFACE_RESULT_NONE;
    previewFlags   = ITEM_MENU_PREVIEW_SCALE_EQUIPMENT;
    if (task->state == ITEM_PICKUP_PREVIEW_STATE_INIT) {
        _itemPickupRequestPreview(itemId);
        task->state = ITEM_PICKUP_PREVIEW_STATE_WAIT_LOAD;
    }
    if (task->state == ITEM_PICKUP_PREVIEW_STATE_WAIT_LOAD) {
        if ((cdQueue->scenePayloadAvailable != 0) || cdCmdIsIdle()) {
            task->state = ITEM_PICKUP_PREVIEW_STATE_READY;
        }
    }
    if (task->state != ITEM_PICKUP_PREVIEW_STATE_READY) {
        previewFlags |= ITEM_MENU_PREVIEW_HIDDEN;
    }
    itemMenuDrawPreview(object, object->panel.contentLeft.signedValue + 2, object->panel.contentTop.signedValue + 2, previewFlags);
}

/// Draws the quantity and its recessed row box using a wrapped 16-bit Y origin.
///
/// Uses `itemMenuDrawQuantity`'s drawing contract. The u16 intermediate preserves
/// the panel-coordinate wrap before the signed row offset is added.
static inline void _itemMenuDrawQuantity(const UiObject* object, s32 x, s32 y, s32 quantity, s32 colorRgb)
{
    u8          quantityText[0x20];
    TextDrawReq textRequest;
    u16         textBaseY;

    textRequest.x          = object->panel.contentOriginX.unsignedValue + 0x84 + x;
    textBaseY              = object->panel.contentOriginY.unsignedValue - 3;
    textRequest.y          = textBaseY + y;
    textRequest.otIndex    = object->panel.otIndex.signedValue + 1;
    textRequest.colorRgb   = colorRgb;
    textRequest.glyphTable = TEXT_GLYPH_TABLE_SMALL;
    textRequest.alignment  = TEXT_ALIGNMENT_RIGHT;
    textRequest.drawMode   = TEXT_DRAW_FILL_ONLY;
    textDrawString(&textRequest, textItoaSigned(quantityText, quantity));
    uiDrawRecessedRect(&object->panel, (x + 0x69), (y - 8), 0x1B, 7, ITEM_MENU_RECESSED_FILL_COLOR);
}

/// Draws the published pickup's name, icon and stack quantity above its preview.
///
/// spawnArg2 borrows the live task-owned UiObject; published id/quantity remain
/// stable for its lifetime. State 0 spawns the preview child and places it
/// below the one-row title panel. Ids below 0x100 use Item, others Key Item;
/// only ordinary P.E. ids show levels, and only 0xA0..0xBF show quantity.
/// Hidden panels suppress name/icon drawing but still draw a consumable's
/// quantity box. Text and pictures use the item-menu resource contracts.
static void _itemPickupTitleTask(Task* task)
{
    enum {
        ITEM_PICKUP_TITLE_STATE_INIT = 0,
        ITEM_PICKUP_KEY_ITEM_FIRST   = 0x100
    };
    TextDrawReq nameRequest;
    UiObject*   previewObject;
    UiObject*   object;
    s32         textColorRgb;
    s32         x;
    s32         y;
    s32         energyLevelIndex;
    s32         textBaseY;
    s32         itemId;

    itemId         = gItemPickupPublishedKind;
    object         = task->spawnArg2.pointer;
    object->result = USER_INTERFACE_RESULT_NONE;
    if (itemId < ITEM_PICKUP_KEY_ITEM_FIRST) {
        uiDrawTitle(&(object)->panel, (const char*)gItemMenuItemTitle);
    } else {
        uiDrawTitle(&(object)->panel, (const char*)gItemMenuKeyItemTitle);
    }
    if (task->state == ITEM_PICKUP_TITLE_STATE_INIT) {
        previewObject = uiSpawnObject(&_gItemPickupPreviewDesc, 0, 0, 1, object);
        uiSetPanelContentSize(&(object)->panel, 0, uiGetTextRowsHeight(1) + 1);
        if (previewObject != NULL) {
            previewObject->panel.bounds.unsignedRect.y = object->panel.bounds.unsignedRect.y + object->panel.bounds.unsignedRect.h;
        }
        task->state++;
    }
    textColorRgb = ITEM_MENU_PANEL_TEXT_COLOR;
    x            = object->panel.contentLeft.signedValue + 2;
    y            = object->panel.contentTop.signedValue + 0xF;
    if (object->panel.state != USER_INTERFACE_PANEL_HIDDEN) {
        ITEM_MENU_DRAW_UNMARKED_ROW_CONTENTS(object, nameRequest, x, y, itemId, textColorRgb, textBaseY, energyLevelIndex);
    }
    if ((u32)(itemId - INVENTORY_CONSUMABLE_ITEM_FIRST) < (u32)INVENTORY_CONSUMABLE_ITEM_COUNT) {
        _itemMenuDrawQuantity(object, object->panel.contentLeft.signedValue + 2, object->panel.contentTop.signedValue + 0xF, gItemPickupPublishedQuantity, ITEM_MENU_PANEL_TEXT_COLOR);
    }
}

#undef ITEM_MENU_DRAW_UNMARKED_ROW_CONTENTS

/// Creates a child choice dialog anchored at the parent's lower-right content corner.
///
/// parent must be a live task-owned object with current content bounds.
/// dialogOptions carries an `ITEM_MENU_DIALOG_*` layout in its low nibble and
/// optionally `ITEM_MENU_DIALOG_SYSTEM_CURSOR_SOUND`. The child's task owns it
/// until teardown. It starts active with a two-tick opening delay; its initial
/// right edge is ten pixels past the parent's content edge. Coordinates narrow
/// to 16 bits. Success clears the parent's command value and makes it inactive;
/// allocation failure returns NULL with parent unchanged.
static inline UiObject* _itemMenuSpawnDialog(UiObject* parent, s32 dialogOptions)
{
    UiObject* dialog;

    dialog = uiSpawnObject(&gItemMenuDialogDesc, dialogOptions, USER_INTERFACE_PANEL_ACTIVE, ITEM_MENU_DIALOG_OPEN_DELAY_TICKS, parent);
    if (dialog != NULL) {
        dialog->panel.bounds.unsignedRect.x = (parent->panel.contentOriginX.unsignedValue + parent->panel.contentRight.unsignedValue + 0xA) - dialog->panel.bounds.unsignedRect.w;
        dialog->panel.bounds.unsignedRect.y = parent->panel.contentOriginY.unsignedValue + parent->panel.contentBottom.unsignedValue;
        parent->resultValue                 = USER_INTERFACE_LIST_COMMAND_NONE;
        parent->panel.control.word          = USER_INTERFACE_PANEL_INACTIVE;
    }
    return dialog;
}

/// Asks whether to collect the published item and shows its obtained notice.
///
/// spawnArg2 is the live UiObject. State 0 attempts one Yes-selected child menu;
/// state 1 accepts its answer. Ordinary ids below 0xC0 use the carried inventory,
/// with a failed capacity check changed to No; ids 0xC0..0x1FF set a collection
/// bit. A Yes answer waits for the obtained child to return CANCEL, then passes
/// that result to the root while retaining the Yes answer in resultValue.
static void _itemPickupAskTask(Task* task)
{
    enum {
        ITEM_PICKUP_ASK_STATE_INIT        = 0,
        ITEM_PICKUP_ASK_STATE_WAIT_ANSWER = 1,
        ITEM_PICKUP_COLLECTION_ID_LIMIT   = 0x200
    };
    UiObject*           object;
    UiObject*           childObject;
    Task*               childTask;
    InventoryItemRange* carriedItems;
    s32                 textColorRgb;
    s32                 drawMode;
    s32                 childResult;

    object         = task->spawnArg2.pointer;
    object->result = USER_INTERFACE_RESULT_NONE;
    if (task->state == ITEM_PICKUP_ASK_STATE_INIT) {
        _itemMenuSpawnDialog(object, ITEM_MENU_DIALOG_YES_SELECTED);
        task->state = task->state + 1;
    }
    uiDrawPanelLabelWithChildFocus(&object->panel, _gItemPickupQuestionTitle);
    textColorRgb = ITEM_MENU_PANEL_TEXT_COLOR;
    drawMode     = TEXT_DRAW_OUTLINED;
    textDrawUiLine(object, object->panel.contentLeft.signedValue + 2, object->panel.contentTop.signedValue + 0xF, gItemPickupQuestionText, textColorRgb, drawMode, TEXT_ALIGNMENT_LEFT);
    childTask = task->firstChild;
    if (childTask != NULL) {
        childObject = childTask->spawnArg2.pointer;
        childResult = childObject->result;
        if (childResult != USER_INTERFACE_RESULT_CANCEL) {
            if (childResult == USER_INTERFACE_RESULT_CONFIRM) {
                if (task->state == ITEM_PICKUP_ASK_STATE_WAIT_ANSWER) {
                    // Recheck capacity before granting the published item.
                    if (childObject->resultValue == USER_INTERFACE_LIST_COMMAND_YES) {
                        if (gItemPickupPublishedKind < (u32)(INVENTORY_CONSUMABLE_ITEM_FIRST + INVENTORY_CONSUMABLE_ITEM_COUNT)) {
                            carriedItems = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems;
                            if (inventoryCanAddItem(carriedItems, gItemPickupPublishedKind) != 0) {
                                inventoryGiveItem(carriedItems, gItemPickupPublishedKind, gItemPickupPublishedQuantity);
                            } else {
                                childObject->resultValue = USER_INTERFACE_LIST_COMMAND_NO;
                            }
                        } else if (gItemPickupPublishedKind < (u32)ITEM_PICKUP_COLLECTION_ID_LIMIT) {
                            inventorySetCollectedBit(gItemPickupPublishedKind);
                        }
                    }
                    object->resultValue = childObject->resultValue;
                    if (object->resultValue == USER_INTERFACE_LIST_COMMAND_YES) {
                        uiSpawnObject(&_gItemPickupObtainedNoticeDesc, (s32)gItemPickupPublishedKind, USER_INTERFACE_PANEL_ACTIVE, 1, object);
                        object->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
                        uiStartTreeClosing(childObject, childObject->owner);
                        task->state = task->state + 1;
                    } else {
                        object->result = USER_INTERFACE_RESULT_CANCEL;
                    }
                }
            }
        } else {
            object->result = childResult;
        }
    }
}

/// Shows an inventory-full warning and returns a No answer when OK is accepted.
///
/// spawnArg2 is the live UiObject. The warning attempts one OK child menu during
/// initialization; acknowledgment reports CANCEL with command result No.
static void _itemPickupInventoryFullTask(Task* task)
{
    enum { ITEM_PICKUP_FULL_STATE_INIT = 0 };
    Task*     childTask;
    UiObject* object;
    UiObject* childObject;
    s32       textColorRgb;
    s32       drawMode;

    object = task->spawnArg2.pointer;
    uiDrawPanelLabelWithChildFocus(&object->panel, _gItemPickupInventoryFullTitle);
    object->result = USER_INTERFACE_RESULT_NONE;
    if (task->state == ITEM_PICKUP_FULL_STATE_INIT) {
        _itemMenuSpawnDialog(object, ITEM_MENU_DIALOG_OK);
        task->state = task->state + 1;
    }
    textColorRgb = ITEM_MENU_PANEL_TEXT_COLOR;
    drawMode     = TEXT_DRAW_OUTLINED;
    textDrawUiLine(object, object->panel.contentLeft.signedValue + 2, object->panel.contentTop.signedValue + 0xF, gItemPickupInventoryFullText, textColorRgb, drawMode, TEXT_ALIGNMENT_LEFT);
    textDrawUiLine(object, object->panel.contentLeft.signedValue + 2, object->panel.contentTop.signedValue + 0x1E, gItemPickupInventoryFullSecondLineText, textColorRgb, drawMode, TEXT_ALIGNMENT_LEFT);
    childTask = task->firstChild;
    if (childTask != NULL) {
        childObject = childTask->spawnArg2.pointer;
        if (childObject->result == USER_INTERFACE_RESULT_CONFIRM) {
            object->result      = USER_INTERFACE_RESULT_CANCEL;
            object->resultValue = USER_INTERFACE_LIST_COMMAND_NO;
        }
    }
}

/// Shows the obtained item's name until timeout or permitted acknowledgment.
///
/// spawnArg2 is the live UiObject. spawnArg1's low 16 bits hold a valid catalog
/// item id; bit 16 disables Confirm/Cancel acknowledgment. The countdown advances
/// every update, including while opening, and reports CANCEL only while active.
static void _itemPickupObtainedNoticeTask(Task* task)
{
    enum {
        ITEM_PICKUP_OBTAINED_STATE_INIT     = 0,
        ITEM_PICKUP_OBTAINED_NOTICE_UPDATES = 188,
        ITEM_PICKUP_OBTAINED_ITEM_COLOR     = 0x037A78,
        ITEM_PICKUP_OBTAINED_TIMEOUT_ONLY   = 0x10000
    };
    UiObject* object;
    s32       itemId;
    s32       contentWidth;
    s32       noticeTextWidth;
    s32       nameEndX;
    s32       textColorRgb;
    s32       drawMode;
    const u8* itemName;

    object         = task->spawnArg2.pointer;
    itemId         = (u16)task->spawnArg1.value;
    object->result = USER_INTERFACE_RESULT_NONE;
    uiDrawPanelLabel(&object->panel, (const char*)gItemMenuNoticePanelTitle);
    if (task->state == ITEM_PICKUP_OBTAINED_STATE_INIT) {
        task->killCountdown = ITEM_PICKUP_OBTAINED_NOTICE_UPDATES;
        contentWidth        = textMeasureLineWidth(itemGetText(itemId, ITEM_TEXT_NAME, 0)) + 0xB;
        noticeTextWidth     = textMeasureLineWidth(gItemPickupObtainedText);
        if (contentWidth < noticeTextWidth) {
            contentWidth = noticeTextWidth;
        }
        uiSetPanelContentSize(&object->panel, contentWidth + 5, uiGetTextRowsHeight(2) + 1);
        object->panel.bounds.rect.x = (-object->panel.bounds.rect.w) >> 1;
        task->state                 = task->state + 1;
    }
    textColorRgb = ITEM_MENU_PANEL_TEXT_COLOR;
    drawMode     = TEXT_DRAW_OUTLINED;
    textDrawUiLine(object, object->panel.contentLeft.signedValue + 2, object->panel.contentTop.signedValue + 0xF, gItemPickupObtainedText, textColorRgb, drawMode, TEXT_ALIGNMENT_LEFT);
    itemName = itemGetText(itemId, ITEM_TEXT_NAME, 0);
    nameEndX = textDrawUiLine(object, object->panel.contentLeft.signedValue + 2, object->panel.contentTop.signedValue + 0x1E, itemName, ITEM_PICKUP_OBTAINED_ITEM_COLOR, drawMode, TEXT_ALIGNMENT_LEFT);
    textDrawUiLine(object, nameEndX, object->panel.contentTop.signedValue + 0x1E, gItemMenuPeriodText, textColorRgb, drawMode, TEXT_ALIGNMENT_LEFT);
    task->killCountdown--;
    if (object->panel.control.word == USER_INTERFACE_PANEL_ACTIVE) {
        if ((task->killCountdown <= 0) ||
            (((task->spawnArg1.value & ITEM_PICKUP_OBTAINED_TIMEOUT_ONLY) == 0) &&
             (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, gPadConfirmButtonMask | gPadCancelButtonMask) != 0))) {
            object->result      = USER_INTERFACE_RESULT_CANCEL;
            task->killCountdown = ITEM_MENU_COMPLETED_COUNTDOWN;
        }
    }
}

/// Opens an OK child menu below a live parent and transfers input on success.
///
/// Returns a task-owned UiObject, or NULL with parent unchanged on failure.
static UiObject* _itemMenuSpawnOkMenu(UiObject* parent)
{
    return _itemMenuSpawnDialog(parent, ITEM_MENU_DIALOG_OK);
}

/// Opens a Cancel child menu below a live parent and transfers input on success.
///
/// Returns a task-owned UiObject, or NULL with parent unchanged on failure.
static UiObject* _itemMenuSpawnCancelMenu(UiObject* parent)
{
    return _itemMenuSpawnDialog(parent, ITEM_MENU_DIALOG_CANCEL);
}

UiObject* itemMenuSpawnYesNoMenu(UiObject* parent)
{
    return _itemMenuSpawnDialog(parent, ITEM_MENU_DIALOG_YES_SELECTED);
}

UiObject* itemMenuSpawnYesNoMenuDefaultNo(UiObject* parent)
{
    return _itemMenuSpawnDialog(parent, ITEM_MENU_DIALOG_NO_SELECTED);
}

/// Draws a visible item row's name and icon with optional status and P.E. marks.
///
/// Borrows object for this draw, using `itemMenuDrawItemRow`'s panel-relative
/// pixels and catalog/GPU-storage contract. colorRgb is packed 24-bit RGB.
/// Attachment state 0 omits status; 1 enables E/L, and 2 also allows A.
/// Only ordinary P.E. item ids 15..50 draw a level. Hidden panels draw nothing.
static inline void _itemMenuDrawItemRowContents(const UiObject* object, s32 x, s32 y, s32 itemId, s32 colorRgb, s32 attachmentState)
{
    TextDrawReq nameRequest;
    s32         energyLevelIndex;
    s32         textBaseY;

    if (object->panel.state != USER_INTERFACE_PANEL_HIDDEN) {
        nameRequest.x          = object->panel.contentOriginX.unsignedValue + 0x11 + x;
        textBaseY              = object->panel.contentOriginY.unsignedValue - 6;
        nameRequest.y          = textBaseY + y;
        nameRequest.otIndex    = object->panel.otIndex.signedValue + 1;
        nameRequest.colorRgb   = colorRgb;
        nameRequest.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
        nameRequest.alignment  = TEXT_ALIGNMENT_LEFT;
        nameRequest.drawMode   = TEXT_DRAW_OUTLINED;
        textDrawString(&nameRequest, itemGetText(itemId, ITEM_TEXT_NAME, 0));
        if (attachmentState != ITEM_MENU_ATTACHMENT_MARK_AUTOMATIC) {
            itemMenuDrawEquipmentMarker(object, x, y, itemId, attachmentState);
        }
        energyLevelIndex = itemId - ITEM_MENU_PARASITE_ENERGY_ITEM_FIRST;
        if ((u32)energyLevelIndex < (u32)ITEM_MENU_PARASITE_ENERGY_ITEM_COUNT) {
            itemMenuDrawParasiteEnergyLevel(object, x, y, energyLevelIndex % ATTACHMENT_LEVEL_COUNT + 1, colorRgb);
        }
        itemMenuDrawItemIcon(object, x, y, itemId, ITEM_MENU_ICON_DEFAULT);
    }
}

void itemMenuDrawItemRow(const UiObject* object, s32 x, s32 y, s32 itemId, s32 colorRgb, s32 attachmentState)
{
    _itemMenuDrawItemRowContents(object, x, y, itemId, colorRgb, attachmentState);
}

void itemMenuDrawItemSlotRow(const UiObject* object, s32 x, s32 y, s32 itemId, s32 colorRgb, s32 attachmentState)
{
    if (itemId == INVENTORY_ITEM_NONE) {
        uiDrawRecessedRect(&object->panel, x, (y - 0xE), 0xE, 0xE, ITEM_MENU_RECESSED_FILL_COLOR);
        return;
    }
    _itemMenuDrawItemRowContents(object, x, y, itemId, colorRgb, attachmentState);
    uiDrawRecessedRect(&object->panel, x, (y - 0xE), 0xE, 0xE, 0);
}

void itemMenuDrawQuantity(const UiObject* object, s32 x, s32 y, s32 quantity, s32 colorRgb)
{
    _itemMenuDrawQuantity(object, x, y, quantity, colorRgb);
}

void itemMenuDrawUnloadedConsumableQuantity(const UiObject* object, s32 x, s32 y, const InventoryItemRow* row, s32 colorRgb, s32 unused)
{
    u8          quantityText[0x20];
    TextDrawReq quantityRequest;
    s32         textBaseY;
    s32         unloadedQuantity;

    if (row != NULL) {
        if ((u32)(row->itemId - INVENTORY_CONSUMABLE_ITEM_FIRST) < (u32)INVENTORY_CONSUMABLE_ITEM_COUNT) {
            unloadedQuantity           = row->qty - equipmentGetLoadedConsumableQuantity(&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems, row->itemId);
            quantityRequest.x          = object->panel.contentOriginX.unsignedValue + 0x84 + x;
            textBaseY                  = object->panel.contentOriginY.unsignedValue - 3;
            quantityRequest.y          = textBaseY + y;
            quantityRequest.otIndex    = object->panel.otIndex.signedValue + 1;
            quantityRequest.colorRgb   = colorRgb;
            quantityRequest.glyphTable = TEXT_GLYPH_TABLE_SMALL;
            quantityRequest.alignment  = TEXT_ALIGNMENT_RIGHT;
            quantityRequest.drawMode   = TEXT_DRAW_FILL_ONLY;
            textDrawString(&quantityRequest, textItoaSigned(quantityText, unloadedQuantity));
            uiDrawRecessedRect(&object->panel, (x + 0x69), (y - 8), 0x1B, 7, ITEM_MENU_RECESSED_FILL_COLOR);
        }
    }
}

/// Replaces one of the three requested display-profile ids and queues its load.
///
/// loadProfile must be 0..2. The byte parameter also supplies the public
/// setter's low-byte narrowing. Uses `itemMenuSetPreviewItem`'s request and
/// readiness contract, publishing all three ids before calling the loader.
static inline void _itemMenuSetPreviewItem(s32 itemId, u8 loadProfile)
{
    enum {
        ITEM_MENU_PREVIEW_PROFILE_COUNT = 3
    };
    s32 profileIndex;

    if (itemId != gItemMenuPreviewItemIds[loadProfile]) {
        // Publish the selection before requesting resources, even if no load is queued.
        for (profileIndex = 0; profileIndex < ITEM_MENU_PREVIEW_PROFILE_COUNT; profileIndex++) {
            if (profileIndex == loadProfile) {
                gItemMenuPreviewItemIds[profileIndex] = itemId;
            } else {
                gItemMenuPreviewItemIds[profileIndex] = ITEM_MENU_PREVIEW_EMPTY;
            }
        }
        itemMenuEnqueuePreviewLoad(itemId, loadProfile);
    }
}

void itemMenuUpdateSelectionPreview(UiList* unusedList, const UiObject* object, s32 itemId, s32 loadProfile)
{
    s32 previewFlags;

    previewFlags = loadProfile + ITEM_MENU_PREVIEW_SCALE_EQUIPMENT;
    if (itemId != INVENTORY_ITEM_NONE) {
        if (((object->panel.control.word >> 16) == USER_INTERFACE_PANEL_ACTIVE) || (object->panel.control.word == USER_INTERFACE_PANEL_ACTIVE)) {
            _itemMenuSetPreviewItem(itemId, loadProfile);
        }
        if (cdCmdIsIdle() == 0) {
            previewFlags |= ITEM_MENU_PREVIEW_HIDDEN;
        }
    } else {
        previewFlags |= ITEM_MENU_PREVIEW_HIDDEN;
    }
    itemMenuDrawPreview(object, object->panel.contentLeft.signedValue + 2, object->panel.contentTop.signedValue + 2, previewFlags);
}

void itemMenuSetPreviewItem(s32 itemId, s32 loadProfile)
{
    _itemMenuSetPreviewItem(itemId, loadProfile);
}

void itemMenuClearPreviewItems(void)
{
    s32* previewItemIds;
    s32  emptyItemId;

    previewItemIds    = gItemMenuPreviewItemIds;
    emptyItemId       = ITEM_MENU_PREVIEW_EMPTY;
    previewItemIds[2] = emptyItemId;
    previewItemIds[1] = emptyItemId;
    previewItemIds[0] = emptyItemId;
    previewItemIds[3] = emptyItemId;
    previewItemIds[4] = emptyItemId;
}

/// Opens the selected item's information child and consumes the parent's input focus.
///
/// Borrows a live parent and nonempty `gItemMenuSelectedRow` with a valid item id.
/// Requests confirmation sound before reading that row, then copies the id
/// into a task-owned information child that opens after one nominal 60-Hz tick.
/// Preview/caption loading must already have been requested. Parent input is
/// deactivated even if child allocation fails; no row pointer is retained.
static inline void _itemMenuOpenSelectedInfoChild(UiObject* parentObject)
{
    enum { ITEM_MENU_INFO_PANEL_DESCRIPTOR = 45,
           ITEM_MENU_INFO_OPEN_DELAY_TICKS = 1 };

    sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
    uiSpawnObject(&gItemMenuPanelDescriptors[ITEM_MENU_INFO_PANEL_DESCRIPTOR], (s32)gItemMenuSelectedRow->itemId,
                  USER_INTERFACE_PANEL_ACTIVE, ITEM_MENU_INFO_OPEN_DELAY_TICKS, parentObject);
    parentObject->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
}

void itemMenuOpenInfoOnTriangle(UiObject* parentObject)
{
    if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_TRIANGLE) && (gItemMenuSelectedRow != NULL) && (gItemMenuSelectedRow->itemId != INVENTORY_ITEM_NONE)) {
        _itemMenuOpenSelectedInfoChild(parentObject);
    }
}

void itemPickupOpenPromptTask(Task* task)
{
    const UiObjectDesc* descriptor;
    UiObject*           rootObject;

    task->killCountdown--;
    if (task->killCountdown <= 0) {
        switch (gItemPickupPublishedKind >> 8) {
            case ITEM_PICKUP_PLACE_BANK_ITEM:
            case ITEM_PICKUP_PLACE_BANK_KEY_ITEM:
                descriptor = &gItemMenuPanelDescriptors[ITEM_PICKUP_PANEL_DESCRIPTOR];
                break;
            case ITEM_PICKUP_PLACE_BANK_SAVE_POINT:
                playerCaptureSaveState();
                descriptor                                         = &gTelephoneMenuDescriptor;
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.savePoint = gItemPickupPublishedKind;
                break;
            default:
                stageEnsureHeapTaskPrimitiveBuffer();
                descriptor = &gItemMenuTransferScreenDescriptor;
                break;
        }
        rootObject = uiSpawnObject(descriptor, 0, USER_INTERFACE_PANEL_INACTIVE, 0, NULL);
        if (rootObject != NULL) {
            task->spawnArg1.pointer = rootObject;
            gGameSession->uiOpen    = 1;
            task->state             = task->state + 1;
        }
    }
}

void itemPickupHandleResultTask(Task* task)
{
    enum {
        ITEM_PICKUP_BANK_ITEM           = 0,
        ITEM_PICKUP_BANK_KEY_ITEM       = 1,
        ITEM_PICKUP_PLACE_COLLECTED     = 2,
        ITEM_PICKUP_PLACE_REPLENISHABLE = 3,
        ITEM_PICKUP_CLOSE_DELAY_UPDATES = 12
    };
    UiObject* object;
    Enemy*    enemy;

    object = task->spawnArg1.pointer;
    enemy  = task->spawnArg2.pointer;
    switch (object->result) {
        case USER_INTERFACE_RESULT_CANCEL:
            // The obtained notice closes with CANCEL while retaining the Yes answer.
            if (object->resultValue == USER_INTERFACE_LIST_COMMAND_YES) {
                enemy->workType = INVENTORY_ITEM_NONE;
            }
            // Fall through to the common prompt-closing path.
        case USER_INTERFACE_RESULT_CONFIRM:
            switch (gItemPickupPublishedKind >> 8) {
                case ITEM_PICKUP_BANK_ITEM:
                case ITEM_PICKUP_BANK_KEY_ITEM:
                    if (object->resultValue == USER_INTERFACE_LIST_COMMAND_YES) {
                        if (areaGetCurrentObjectState((u8)enemy->placeKey) != ITEM_PICKUP_PLACE_REPLENISHABLE) {
                            areaSetCurrentObjectState((u8)enemy->placeKey, ITEM_PICKUP_PLACE_COLLECTED);
                        }
                    }
                    break;
            }

            uiStartTreeClosing(object, object->owner);
            gUiPromptObject      = NULL;
            gGameSession->uiOpen = 0;
            task->killCountdown  = ITEM_PICKUP_CLOSE_DELAY_UPDATES;
            task->state          = task->state + 1;
            break;
    }
}

void itemPickupRestoreFrameTimingTask(Task* task)
{
    task->killCountdown--;
    if (task->killCountdown <= 0) {
        displaySetFrameTiming(DISPLAY_TIMING_TWO_VBLANKS);
        task->killCountdown = 1;
        task->state         = task->state + 1;
    }
}

void itemPickupExitTask(Task* task)
{
    task->killCountdown--;
    if (task->killCountdown <= 0) {
        taskKill(task);
        stageReleaseTaskPrimitiveBuffer();
        stageRequestModeTaskExit();
    }
}

/// Title of the item-use panel showing HP and MP recovery.
static const char _gItemMenuHealingStatusTitle[] = "Status";
/// M4A1 variants scanned to replace a carried rifle when an add-on is used.
///
/// The first carried row supplies the old weapon and returned add-on. The first
/// other row mounting the used add-on supplies the new weapon. Keeping the
/// one-holder row before the two-holder row makes clip-holder use add one stage.
static const _ItemMenuM4a1VariantTable _gItemMenuM4a1VariantTable = { {
    { ITEM_MENU_M4A1_VARIANT_BASE_ITEM, INVENTORY_ITEM_NONE },
    { ITEM_MENU_M4A1_VARIANT_ONE_HOLDER_ITEM, ITEM_MENU_M4A1_ADDON_CLIP_HOLDER },
    { ITEM_MENU_M4A1_VARIANT_TWO_HOLDERS_ITEM, ITEM_MENU_M4A1_ADDON_CLIP_HOLDER },
    { ITEM_MENU_M4A1_VARIANT_HAMMER_ITEM, ITEM_MENU_M4A1_ADDON_HAMMER },
    { ITEM_MENU_M4A1_VARIANT_M203_ITEM, ITEM_MENU_M4A1_ADDON_M203 },
    { ITEM_MENU_M4A1_VARIANT_M9_ITEM, ITEM_MENU_M4A1_ADDON_M9 },
    { ITEM_MENU_M4A1_VARIANT_PYKE_ITEM, ITEM_MENU_M4A1_ADDON_PYKE },
    { ITEM_MENU_M4A1_VARIANT_JAVELIN_ITEM, ITEM_MENU_M4A1_ADDON_JAVELIN },
} };
/// Title of the result panel that unlocks or raises an ordinary P.E. item ability.
static const char _gItemMenuInvokeTitle[] = "Invoke";
/// Title of the four-element P.E. ability list.
static const char _gItemMenuPeListTitle[] = "PE LIST";
/// NUL byte seeding the cleared inventory count text before decimal formatting.
static const u8 _gItemMenuCountTextTerminator = 0;
/// Heading beside the carried-inventory occupied-row/capacity count.
static const u8 _gItemMenuInventoryTotalText[] = "TOTAL";
/// Title of the pickup question while its Yes/No child holds input.
static const char _gItemPickupQuestionTitle[] = "Message";
/// Title of the pickup warning when the carried inventory has no capacity.
static const char _gItemPickupInventoryFullTitle[] = "Warning";

const TaskFuncTable4 gMenuMapStateHandlers = { {
    menuMapOpenTask,
    menuMapWaitForPageTask,
    menuMapNavigateTask,
    menuMapCloseTask,
} };

const char gMenuMapHelpTitle[8]               = "Help\0a~\xC6";
const u8   gItemMenuUseAttachCommandText[]    = "Use";
const u8   gItemMenuKeyItemCommandText[]      = "Key Item";
const u8   gItemMenuMapCommandText[]          = "Map";
const char gItemMenuDiscardTitle[]            = "Attention";
const char gItemMenuNoticeTitle[]             = "Notice";
const char gItemMenuPeNextLevelTitle[]        = "Next Level";
const u8   gItemMenuPeUpgradeExpLabel[]       = "EXP";
const u8   gItemMenuPeUpgradeCostLabel[]      = "COST";
const u8   gItemMenuPeUpgradeBonusLabel[]     = "BONUS";
const u8   gItemMenuPeUpgradeMpLabel[]        = "MP";
const char gItemMenuPeSpecificationsTitle[16] = "Specifications\0\"";
