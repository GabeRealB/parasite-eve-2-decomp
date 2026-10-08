#include "attachments.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "types.h"

#include "gameplay/attachment_state.h"
#include "gameplay/inventory.h"
#include "gameplay/item_menu.h"
#include "item_menu.h"
#include "item_use.h"
#include "gameplay/items.h"
#include "items.h"

#include "main/fs.h"
#include "main/mc.h"
#include "main/pad.h"
#include "main/sound.h"
#include "main/text.h"
#include "main/ui.h"
#include "main/wipsys.h"

extern UiListRowCallback Gp_ItemCmdRows[2];

extern const u8 Gp_StrWrongAmmo2[];

enum {
    ITEM_MENU_ARMOR_ITEM_FIRST       = 0x60,
    ITEM_MENU_EQUIPMENT_ITEM_COUNT_U = 0x20U,
    ITEM_MENU_NOTICE_TIMEOUT_TICKS   = 188,
    ITEM_MENU_NOTICE_DISMISSED_TICKS = 0x7FFF,
    ITEM_MENU_NOTICE_TEXT_COLOR_RGB  = 0x606060,
    ITEM_MENU_NOTICE_ITEM_COLOR_RGB  = 0x037A78
};

/// Opening delay for equipment command and confirmation panels, in callback ticks.
enum { ITEM_MENU_COMMAND_OPEN_DELAY_TICKS = 1 };

/// Consumable choice phases shared by the compact list and ammunition selector.
enum {
    ITEM_MENU_CONSUMABLE_CHOICE_INITIAL   = 0,
    ITEM_MENU_CONSUMABLE_CHOICE_SELECTING = 1,
    ITEM_MENU_CONSUMABLE_CHOICE_NO_AMMO   = 2,
    ITEM_MENU_CONSUMABLE_CHOICE_EQUIPPED  = 3
};

/// Selection-panel layout, detail-panel dispatch and suspended input mode.
enum {
    ITEM_MENU_SELECTION_STATS_HEIGHT_PIXELS     = 76,
    ITEM_MENU_SELECTION_SEPARATOR_Y_PIXELS      = 74,
    ITEM_MENU_SELECTION_DETAIL_PANEL            = 14,
    ITEM_MENU_SELECTION_DETAIL_OPEN_DELAY_TICKS = 16,
    ITEM_MENU_SELECTION_SUSPENDED_CONTROL_SHIFT = 16,
    ITEM_MENU_DETAIL_CONSUMABLE                 = 1,
    ITEM_MENU_DETAIL_ARMOR                      = 2
};

/// Returns 1 for armor catalogue ids 0x60..0x7F, otherwise 0.
static inline s32 _itemIsArmorItem(u8 itemId)
{
    return (u32)(itemId - ITEM_MENU_ARMOR_ITEM_FIRST) < ITEM_MENU_EQUIPMENT_ITEM_COUNT_U;
}

#define GP_SET_PREVIEW_ITEM(item, slot)             \
    do {                                            \
        s32* _p;                                    \
        s32  _i;                                    \
                                                    \
        _p = Gp_PreviewItems;                       \
        if ((item) != _p[slot]) {                   \
            for (_i = 0; _i < 3; _i++, _p++) {      \
                if (_i == (slot)) {                 \
                    *_p = (item);                   \
                } else {                            \
                    *_p = -1;                       \
                }                                   \
            }                                       \
            itemMenuEnqueuePreviewLoad(item, slot); \
        }                                           \
    } while (0)
#define GP_FIND_SPARE_ARMOR(found, index)                                                 \
    do {                                                                                  \
        PlayerStatus*       _cfg;                                                         \
        InventoryItemRange* _scan;                                                        \
        InventoryItemRow*   _rec;                                                         \
        s32                 _i;                                                           \
        s32                 _n;                                                           \
                                                                                          \
        _scan   = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems;                 \
        _cfg    = &gPlayerStatus;                                                         \
        _n      = (index);                                                                \
        _rec    = inventoryGetRangeTable(_scan);                                          \
        (found) = _i = 0;                                                                 \
        _rec         = &_rec[_scan->firstRow];                                            \
        for (; _i < _scan->rowCount; _i++) {                                              \
            if (_itemIsArmorItem(_rec->itemId) && (_cfg->armor != _rec->itemId - 0x5F)) { \
                _n--;                                                                     \
                if (_n < 0) {                                                             \
                    (found) = _rec->itemId;                                               \
                    break;                                                                \
                }                                                                         \
            }                                                                             \
            _rec++;                                                                       \
        }                                                                                 \
    } while (0)
#define GP_COUNT_SPARE_ARMOR(count)                                                       \
    do {                                                                                  \
        PlayerStatus*       _cfg;                                                         \
        InventoryItemRange* _scan;                                                        \
        InventoryItemRow*   _rec;                                                         \
        s32                 _i;                                                           \
                                                                                          \
        (count) = 0;                                                                      \
        _scan   = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems;                 \
        _cfg    = &gPlayerStatus;                                                         \
        _rec    = inventoryGetRangeTable(_scan);                                          \
        _i      = 0;                                                                      \
        _rec    = &_rec[_scan->firstRow];                                                 \
        for (; _i < _scan->rowCount; _i++) {                                              \
            if (_itemIsArmorItem(_rec->itemId) && (_cfg->armor != _rec->itemId - 0x5F)) { \
                (count)++;                                                                \
            }                                                                             \
            _rec++;                                                                       \
        }                                                                                 \
    } while (0)
/// Returns the armor item id at a zero-based position in the unequipped list, or 0.
///
/// Scans the live carried range in row order, skipping every row of the equipped
/// armor id. The range must fit its readable table. Negative list positions
/// select the first eligible row; positions past the list return INVENTORY_ITEM_NONE.
/// Duplicate item ids remain separate rows; quantity and attachment markers do
/// not affect eligibility. No saved state is changed or pointer retained.
static inline s32 _itemMenuGetUnequippedArmorItem(s32 listIndex)
{
    const PlayerStatus*       player;
    const InventoryItemRange* carriedRange;
    const InventoryItemRow*   row;
    s32                       rowIndex;
    s32                       armorItemId;

    carriedRange = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems;
    player       = &gPlayerStatus;
    row          = inventoryGetRangeTable(carriedRange);
    armorItemId  = INVENTORY_ITEM_NONE;
    rowIndex     = 0;
    row          = &row[carriedRange->firstRow];
    for (; rowIndex < carriedRange->rowCount; rowIndex++) {
        if (_itemIsArmorItem(row->itemId) && (player->armor != row->itemId - (ITEM_MENU_ARMOR_ITEM_FIRST - 1))) {
            listIndex--;
            if (listIndex < 0) {
                armorItemId = row->itemId;
                break;
            }
        }
        row++;
    }
    return armorItemId;
}

/// Propagates child dismissal/cancellation and closes accepted child dialogs.
///
/// The live task's circular child ring must contain task-owned UiObjects in
/// spawnArg2. CONFIRM detaches and starts closing that child's subtree, then
/// reactivates the parent panel; DISMISS and CANCEL copy to the parent's result.
/// Other outcomes leave it intact. Closing defers release to later task updates.
/// Reaching the current ring head ends the pass, including after closing its
/// previous head while siblings remain.
static inline void _itemMenuApplyChildDialogResults(UiObject* object, Task* task)
{
    Task*     child;
    Task*     nextSibling;
    Task*     childHead;
    UiObject* childObject;
    s32       childResult;

    child = task->firstChild;
    if (child != NULL) {
        do {
            childObject = child->spawnArg2.pointer;
            childResult = childObject->result;
            // Closing detaches the child, so save its link before handling the result.
            nextSibling = child->nextSibling;
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
            childHead = task->firstChild;
            child     = nextSibling;
            if (child == childHead) {
                break;
            }
            if (childHead == NULL) {
                break;
            }
        } while (1);
    }
}

/// Selects a carried weapon while preserving equipment in its former armor slot.
///
/// weaponItemId must be 0x80..0x9F; a changed selection requires its carried row
/// and the previous weapon's row when the old selector is nonzero. The live
/// carried range must fit its table. Selecting the current weapon changes nothing.
/// The previous weapon inherits a positive armor slot from the new row; without
/// one, its removable load selections and quantities are cleared, preserving
/// built-in supplies. The new one-based selector is stored before detaching the
/// row so its loads survive detachment, then the item is marked identified.
/// Row pointers are borrowed only for this call; no inventory quantity changes.
static inline void _equipmentEquipCarriedWeapon(s32 weaponItemId)
{
    PlayerStatus*     player;
    InventoryItemRow* weaponRow;
    InventoryItemRow* previousWeaponRow;
    u8                previousWeaponSelector;

    player                 = &gPlayerStatus;
    weaponRow              = inventoryFindLastCarriedItemRow(weaponItemId);
    previousWeaponSelector = player->weapon;
    if (previousWeaponSelector == weaponItemId - (EQUIPMENT_WEAPON_ITEM_FIRST - 1)) {
        return;
    }
    // Leave the previous weapon in the incoming weapon's armor position, or unload it.
    if (previousWeaponSelector != PLAYER_STATUS_EQUIPMENT_NONE) {
        previousWeaponRow = inventoryFindLastCarriedItemRow(previousWeaponSelector + (EQUIPMENT_WEAPON_ITEM_FIRST - 1));
        if (weaponRow->attachSlot > INVENTORY_ATTACHMENT_NONE) {
            previousWeaponRow->attachSlot = weaponRow->attachSlot;
        } else {
            equipmentClearSelectedRemovableLoads(previousWeaponRow->itemId, EQUIPMENT_CLEAR_LOAD_BOTH);
        }
    }
    // Detachment keeps loads belonging to the weapon selected here.
    player->weapon = weaponItemId - (EQUIPMENT_WEAPON_ITEM_FIRST - 1);
    inventoryDetachItem(weaponRow);
    itemSetIdentified(weaponItemId, 1);
}

/// Advances a notice's tick counter and publishes dismissal or menu cancellation.
///
/// task and its owned object must remain live, with killCountdown initialized by
/// the notice. Decrements once per callback, narrowing to s16 even while inactive.
/// Only a fully active control word accepts input or expiry. Menu takes priority
/// and publishes CANCEL without resetting the counter. Otherwise expiry or
/// Confirm/Cancel publishes DISMISS and resets the counter to 32767, deferring
/// repeated expiry during teardown. Leaves any existing result intact otherwise;
/// the caller clears it each update and handles closing. No object is released.
static inline void _itemMenuUpdateNoticeResult(UiObject* object, Task* task)
{
    task->killCountdown--;
    if (object->panel.control.word == USER_INTERFACE_PANEL_ACTIVE) {
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskMenu) != 0) {
            object->result = USER_INTERFACE_RESULT_CANCEL;
        } else if ((task->killCountdown <= 0) || (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm | Pad_MaskCancel) != 0)) {
            object->result      = USER_INTERFACE_RESULT_DISMISS;
            task->killCountdown = ITEM_MENU_NOTICE_DISMISSED_TICKS;
        }
    }
}

UiObjectDesc D_8010F6FC    = { USER_INTERFACE_PANEL_TITLE_STYLE, { -60, -30, 120, 60 }, 8, 0, TASK_BODY_NONE, 192, itemMenuDiscardTask, 0 };
UiObjectDesc D_8010F718[4] = {
    { USER_INTERFACE_PANEL_TITLE_STYLE, { -144, -80, 144, 72 }, 292, 0, TASK_BODY_NONE, 192, itemMenuPeElementTask, 0 },
    { USER_INTERFACE_PANEL_TITLE_STYLE, { -144, -8, 144, 72 }, 288, 0, TASK_BODY_NONE, 192, itemMenuPeElementTask, 0 },
    { USER_INTERFACE_PANEL_TITLE_STYLE, { 0, -80, 144, 72 }, 284, 0, TASK_BODY_NONE, 192, itemMenuPeElementTask, 0 },
    { USER_INTERFACE_PANEL_TITLE_STYLE, { 0, -8, 144, 72 }, 280, 0, TASK_BODY_NONE, 192, itemMenuPeElementTask, 0 },
};
UiObjectDesc D_8010F788    = { USER_INTERFACE_PANEL_TITLE_STYLE, { 0, 0, 1, 1 }, 8, 0, TASK_BODY_NONE, 192, itemMenuNoticeTask, 0 };
UiObjectDesc D_8010F7A4    = { 0, { -128, 64, 256, 32 }, 264, 0, TASK_BODY_NONE, 192, itemMenuPeUpgradeTask, 0 };
UiObjectDesc D_8010F7C0[2] = {
    { USER_INTERFACE_PANEL_TITLE_STYLE, { -128, -80, 192, 144 }, 12, 0, TASK_BODY_NONE, 192, itemMenuPeNextLevelTask, 0 },
    { USER_INTERFACE_PANEL_TITLE_STYLE, { -96, -72, 176, 112 }, 8, 0, TASK_BODY_NONE, 192, itemMenuHealingTask, 0 },
};
UiObjectDesc      D_8010F7F8          = { USER_INTERFACE_PANEL_TITLE_STYLE, { -96, -104, 192, 208 }, 8, 0, TASK_BODY_NONE, 192, itemMenuPeSpecificationsTask, 0 };
UiListRowCallback Gp_ItemCmdRows[2]   = { itemMenuDrawHotspotActionRow, Gp_DrawItemCmd };
UiList            D_8010F81C          = { Gp_ItemCmdRows, 2, { 2 }, 1, 10, 0, { 0 }, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, { 0 }, 0 };
UiObjectDesc      D_8010F840          = { 3, { 0, 0, 70, 64 }, 60, 0, TASK_BODY_NONE, 192, itemMenuHotspotCommandTask, 0 };
TaskDesc          D_8010F85C          = { { { TASK_BODY_NONE, 192 } }, Gp_MapScreenTask, { NULL } };
UiObjectDesc      D_8010F868          = { 0, { 0, -104, 144, 16 }, 36, 0, TASK_BODY_NONE, 192, itemMenuPreviewPanelTask, 0 };
s32               D_8010F884          = 0;
s32               Gp_HealPending      = 0;
s32               Gp_PendingRelatedId = 0;
s32               Gp_RelatedPending   = 0;
s32               Gp_UsedItemId       = 0;
UiObjectDesc      D_8010F898          = { USER_INTERFACE_PANEL_TITLE_STYLE, { 0, 28, 152, 72 }, 56, 0, TASK_BODY_NONE, 192, func_800D6334, 0 };
UiObjectDesc      D_8010F8B4          = { USER_INTERFACE_PANEL_TITLE_STYLE, { -152, 28, 152, 72 }, 60, 0, TASK_BODY_NONE, 192, Gp_DrawWeaponLabel, 0 };

const u8 Gp_StrWrongAmmo2[] = "You do not have the correct ammo.";

void itemMenuConsumableChoiceListTask(Task* choiceTask)
{
    enum {
        ITEM_MENU_CONSUMABLE_CHOICE_BOTTOM_PIXELS = 70,
        ITEM_MENU_TONFA_BATON_ITEM                = 0x92,
        ITEM_MENU_HYPERVELOCITY_ITEM              = 0x95,
        ITEM_MENU_GUNBLADE_ITEM                   = 0x96,
        ITEM_MENU_M4A1_BAYONET_ITEM               = 0x99
    };
    UiList*                    list;
    UiObject*                  object;
    s32                        weaponItemId;
    s32                        selectingState;
    s32                        currentState;
    const EquipmentWeaponLoad* weaponLoad;
    u8                         secondaryItemId;

    object         = choiceTask->spawnArg2.pointer;
    weaponItemId   = (u16)choiceTask->spawnArg1.value;
    list           = &D_8010E9CC;
    object->result = USER_INTERFACE_RESULT_NONE;
    currentState   = choiceTask->state;
    // Build once; an empty list becomes a timed notice rather than an input list.
    if (currentState == ITEM_MENU_CONSUMABLE_CHOICE_INITIAL) {
        itemMenuBuildConsumableChoiceList(list, weaponItemId);
        uiFitPanelToList(list, &object->panel);
        list->flags                               = USER_INTERFACE_LIST_SHARED_ROW_CALLBACK;
        list->selectedItemIndex                   = 0;
        list->firstVisibleItemIndex.unsignedValue = 0;
        choiceTask->state                         = choiceTask->state + 1;
        if (list->itemCount == 0) {
            choiceTask->state         = ITEM_MENU_CONSUMABLE_CHOICE_NO_AMMO;
            choiceTask->killCountdown = ITEM_MENU_NOTICE_TIMEOUT_TICKS;
            object->panel.style      |= USER_INTERFACE_PANEL_TITLE_STYLE;
            // A just-equipped weapon can still work without a carried load choice.
            if (choiceTask->spawnArg1.value & ITEM_MENU_LOAD_AFTER_EQUIP) {
                if (weaponItemId != INVENTORY_ITEM_NONE) {
                    weaponLoad = equipmentGetWeaponLoad(weaponItemId);
                    if ((weaponItemId == ITEM_MENU_TONFA_BATON_ITEM) || (weaponItemId == ITEM_MENU_M4A1_BAYONET_ITEM) || (weaponItemId == ITEM_MENU_GUNBLADE_ITEM)) {
                        choiceTask->state = ITEM_MENU_CONSUMABLE_CHOICE_EQUIPPED;
                    } else if (weaponItemId == ITEM_MENU_HYPERVELOCITY_ITEM) {
                        if (weaponLoad->primaryQty != 0) {
                            choiceTask->state = ITEM_MENU_CONSUMABLE_CHOICE_EQUIPPED;
                        }
                    } else {
                        secondaryItemId = weaponLoad->secondaryItemId;
                        if ((secondaryItemId != EQUIPMENT_WEAPON_SECONDARY_UNAVAILABLE) && (secondaryItemId != INVENTORY_ITEM_NONE)) {
                            if (weaponLoad->secondaryQty != 0) {
                                choiceTask->state = ITEM_MENU_CONSUMABLE_CHOICE_EQUIPPED;
                            }
                        }
                    }
                }
            }
            uiSizePanelForTextDefault(&object->panel, Gp_StrWrongAmmo2);
            if (choiceTask->state != ITEM_MENU_CONSUMABLE_CHOICE_NO_AMMO) {
                itemMenuSizeEquippedNotice(&object->panel, weaponItemId);
            }
            object->panel.animationTicks = USER_INTERFACE_PANEL_ANIMATION_TICKS;
            return;
        }
        if (object->panel.bounds.rect.y + object->panel.bounds.rect.h < ITEM_MENU_CONSUMABLE_CHOICE_BOTTOM_PIXELS + 1) {
            return;
        }
        object->panel.bounds.unsignedRect.y = ITEM_MENU_CONSUMABLE_CHOICE_BOTTOM_PIXELS - object->panel.bounds.unsignedRect.h;
        return;
    }
    // These selectors share 1: selecting phase, active input, pressed query and outline.
    selectingState = ITEM_MENU_CONSUMABLE_CHOICE_SELECTING;
    if (currentState == selectingState) {
        uiUpdateList(list, &object->panel);
        if (object->panel.control.word == selectingState) {
            if (padCheckButtons(0, selectingState, Pad_MaskMenu) != 0) {
                object->result = USER_INTERFACE_RESULT_CANCEL;
            } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskCancel) != 0) {
                sndEvtRequestScriptStart(SOUND_MENU_CANCEL, 0, 0);
                object->result = USER_INTERFACE_RESULT_CONFIRM;
            }
        }
        _itemMenuApplyChildDialogResults(object, choiceTask);
        return;
    }
    if (currentState == ITEM_MENU_CONSUMABLE_CHOICE_NO_AMMO) {
        s32 drawMode = selectingState;

        uiDrawPanelLabel(&object->panel, Gp_StrNotice);
        textDrawUiLines(object, object->panel.contentLeft.signedValue + 2, object->panel.contentTop.signedValue + 0xF, Gp_StrWrongAmmo2, ITEM_MENU_NOTICE_TEXT_COLOR_RGB, drawMode, TEXT_ALIGNMENT_LEFT);
    } else {
        uiDrawPanelLabel(&object->panel, Gp_StrEquip);
        itemMenuDrawEquippedNotice(object, weaponItemId);
    }
    // Count inactive updates too, but accept expiry or input only while active.
    choiceTask->killCountdown--;
    if (object->panel.control.word == USER_INTERFACE_PANEL_ACTIVE) {
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskMenu) != 0) {
            object->result = USER_INTERFACE_RESULT_CANCEL;
            return;
        }
        if ((choiceTask->killCountdown == 0) || (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm | Pad_MaskCancel) != 0)) {
            if ((choiceTask->spawnArg1.value & ITEM_MENU_LOAD_AFTER_EQUIP) && (choiceTask->state == ITEM_MENU_CONSUMABLE_CHOICE_NO_AMMO)) {
                object->result = USER_INTERFACE_RESULT_CONFIRM;
            } else {
                object->result = USER_INTERFACE_RESULT_DISMISS;
            }
            choiceTask->killCountdown = ITEM_MENU_NOTICE_DISMISSED_TICKS;
        }
    }
}

void itemMenuAmmoSelectionTask(Task* task)
{
    UiList*   list;
    UiObject* object;
    s32       previousState;
    s32       currentState;
    s32       consumableItemId;
    s32       previewFlags;
    Task*     parentTask;

    list          = &D_8010E9CC;
    object        = task->spawnArg2.pointer;
    previousState = task->state;
    itemMenuConsumableChoiceListTask(task);
    if (object->result == USER_INTERFACE_RESULT_DISMISS) {
        object->result = USER_INTERFACE_RESULT_CONFIRM;
    }
    currentState = task->state;
    if (currentState == ITEM_MENU_CONSUMABLE_CHOICE_SELECTING) {
        uiDrawPanelLabel(&object->panel, Gp_StrSelectAmmo);
        // The base controller has finished sizing and positioning this list.
        if (previousState == ITEM_MENU_CONSUMABLE_CHOICE_INITIAL) {
            list->topInset                      += ITEM_MENU_SELECTION_STATS_HEIGHT_PIXELS;
            object->panel.bounds.unsignedRect.h += ITEM_MENU_SELECTION_STATS_HEIGHT_PIXELS;
            parentTask                           = task->parent;
            D_80114DD8                           = -1;
            uiStartPanelHiding(parentTask->spawnArg2.pointer, parentTask);
            uiSpawnObject(&D_8010EAB4[ITEM_MENU_SELECTION_DETAIL_PANEL], ITEM_MENU_DETAIL_CONSUMABLE, USER_INTERFACE_PANEL_INACTIVE, ITEM_MENU_SELECTION_DETAIL_OPEN_DELAY_TICKS, object);
        }
        uiDrawHorizontalSeparator(&object->panel, object->panel.contentLeft.signedValue, object->panel.contentRight.signedValue, object->panel.contentTop.signedValue + ITEM_MENU_SELECTION_SEPARATOR_Y_PIXELS);
        consumableItemId = Gp_AttachListIds[list->selectedItemIndex];
        if (consumableItemId != INVENTORY_ITEM_NONE) {
            itemMenuDrawEquipmentStats(object, consumableItemId, ITEM_MENU_EQUIPMENT_STATS_COMPARE, 0);
        }
        previewFlags = ITEM_MENU_PREVIEW_TEXTURE_RELOCATED | ITEM_MENU_PREVIEW_SCALE_EQUIPMENT;
        if (consumableItemId == INVENTORY_ITEM_NONE) {
            previewFlags = ITEM_MENU_PREVIEW_TEXTURE_RELOCATED | ITEM_MENU_PREVIEW_SCALE_EQUIPMENT | ITEM_MENU_PREVIEW_HIDDEN;
        } else {
            if (((object->panel.control.word >> ITEM_MENU_SELECTION_SUSPENDED_CONTROL_SHIFT) == USER_INTERFACE_PANEL_ACTIVE) || (object->panel.control.word == USER_INTERFACE_PANEL_ACTIVE)) {
                GP_SET_PREVIEW_ITEM(consumableItemId, CD_COMMAND_DISPLAY_LOAD_RELOCATED_PREVIEW);
            }
            if (cdCmdIsIdle() == 0) {
                previewFlags |= ITEM_MENU_PREVIEW_HIDDEN;
            }
        }
        itemMenuDrawPreview(object, object->panel.contentLeft.signedValue + 2, object->panel.contentTop.signedValue + 2, previewFlags);
    }
}

/// Draws an armor-choice row's outlined name, E/L/A status and normal item icon.
///
/// Borrows object for this draw. x/y are signed pixels at the icon's bottom-left
/// relative to its content origin; the name begins at x + 17, y - 6. Origin-Y
/// subtraction promotes to s32 before y is added, and text coordinates narrow
/// to s16. Hidden panels draw nothing. itemId must satisfy `itemGetText` and
/// `itemMenuDrawItemIcon`; the sole caller supplies unequipped armor or item 0.
/// Ordinary P.E. ids 15..50 retain their level 1..3 path. attachmentState follows
/// `itemMenuDrawEquipmentMarker` (only 2 enables A; E/L is always tested).
/// colorRgb supplies packed 24-bit RGB. Requires loaded menu/font textures and
/// writable primitive/OT storage, including panel otIndex + 1 and + 2 for text.
static inline void _itemMenuDrawArmorChoiceContents(const UiObject* object, s32 x, s32 y, s32 itemId, s32 colorRgb, s32 attachmentState)
{
    enum {
        ITEM_MENU_PARASITE_ENERGY_ITEM_FIRST = 15,
        ITEM_MENU_PARASITE_ENERGY_ITEM_COUNT = ATTACHMENT_SPELL_COUNT * ATTACHMENT_AREA_LEVEL_COUNT
    };
    TextDrawReq nameRequest;
    s32         abilityItemOffset;
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
        itemMenuDrawEquipmentMarker(object, x, y, itemId, attachmentState);
        abilityItemOffset = itemId - ITEM_MENU_PARASITE_ENERGY_ITEM_FIRST;
        if ((u32)abilityItemOffset < (u32)ITEM_MENU_PARASITE_ENERGY_ITEM_COUNT) {
            itemMenuDrawParasiteEnergyLevel(object, x, y, abilityItemOffset % ATTACHMENT_AREA_LEVEL_COUNT + 1, colorRgb);
        }
        itemMenuDrawItemIcon(object, x, y, itemId, ITEM_MENU_ICON_DEFAULT);
    }
}

void itemMenuDrawArmorChoiceRow(UiList* list, UiObject* object)
{
    enum {
        ITEM_MENU_ARMOR_EQUIP_NOTICE_PANEL = 41,
        ITEM_MENU_ARMOR_INFO_PANEL         = 45
    };
    s32 armorItemId;
    s32 panelControl;
    s32 rowX;
    s32 rowY;
    s32 textColorRgb;
    s32 attachmentState;

    armorItemId  = _itemMenuGetUnequippedArmorItem(list->currentItemIndex);
    panelControl = object->panel.control.word;
    // Keep the selected description while the active panel is suspended by a child.
    if (((panelControl >> 16) == USER_INTERFACE_PANEL_ACTIVE) || (panelControl == USER_INTERFACE_PANEL_ACTIVE)) {
        if (list->selectedItemIndex == list->currentItemIndex) {
            if (armorItemId == INVENTORY_ITEM_NONE) {
                uiSetPromptText((const u8*)Gp_StrEmpty, 0, 0);
            } else {
                uiSetPromptText(itemGetText(armorItemId, ITEM_TEXT_DESCRIPTION_FIRST, 0), 0, 0);
            }
        }
    }

    rowX            = list->rowTextX.signedValue;
    rowY            = list->rowTextY.signedValue;
    textColorRgb    = list->colorRgb;
    attachmentState = ITEM_MENU_ATTACHMENT_MARK_UNATTACHED;
    _itemMenuDrawArmorChoiceContents(object, rowX, rowY, armorItemId, textColorRgb, attachmentState);

    if (list->rowInputEnabled == USER_INTERFACE_LIST_ROW_ACTIVE) {
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm) != 0) {
            uiSpawnObject(&D_8010EAB4[ITEM_MENU_ARMOR_EQUIP_NOTICE_PANEL], armorItemId, USER_INTERFACE_PANEL_ACTIVE, ITEM_MENU_COMMAND_OPEN_DELAY_TICKS, object);
            object->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
        } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_TRIANGLE) != 0) {
            sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
            uiSpawnObject(&D_8010EAB4[ITEM_MENU_ARMOR_INFO_PANEL], armorItemId | ITEM_MENU_INFO_RELOCATED_PREVIEW, USER_INTERFACE_PANEL_ACTIVE, ITEM_MENU_COMMAND_OPEN_DELAY_TICKS, object);
            object->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
        }
    }
}

void itemMenuArmorSelectionTask(Task* task)
{
    enum {
        ITEM_MENU_ARMOR_SELECTION_INITIAL      = 0,
        ITEM_MENU_ARMOR_SELECTION_VISIBLE_ROWS = 4
    };
    UiList*   list;
    UiObject* object;
    Task*     parentTask;
    s32       armorRowCount;
    s32       foundArmorItemId;
    s32       armorItemId;
    s32       previewFlags;

    list           = &D_8010E9F4;
    object         = task->spawnArg2.pointer;
    object->result = USER_INTERFACE_RESULT_NONE;
    uiDrawPanelLabel(&object->panel, Gp_StrSelectArmor);

    // Reserve the comparison area before rows and hide the previous command panel.
    if (task->state == ITEM_MENU_ARMOR_SELECTION_INITIAL) {
        GP_COUNT_SPARE_ARMOR(armorRowCount);
        list->itemCount                     = armorRowCount;
        list->visibleRowCount.unsignedValue = ITEM_MENU_ARMOR_SELECTION_VISIBLE_ROWS;
        uiFitPanelToList(list, &object->panel);
        list->flags                               = USER_INTERFACE_LIST_SHARED_ROW_CALLBACK;
        list->topInset                           += ITEM_MENU_SELECTION_STATS_HEIGHT_PIXELS;
        object->panel.bounds.unsignedRect.h      += ITEM_MENU_SELECTION_STATS_HEIGHT_PIXELS;
        list->selectedItemIndex                   = 0;
        list->firstVisibleItemIndex.unsignedValue = 0;
        parentTask                                = task->parent;
        uiStartPanelHiding(parentTask->spawnArg2.pointer, parentTask);
        uiSpawnObject(&D_8010EAB4[ITEM_MENU_SELECTION_DETAIL_PANEL], ITEM_MENU_DETAIL_ARMOR, USER_INTERFACE_PANEL_INACTIVE, ITEM_MENU_SELECTION_DETAIL_OPEN_DELAY_TICKS, object);
        task->state++;
    }

    uiDrawHorizontalSeparator(&object->panel, object->panel.contentLeft.signedValue, object->panel.contentRight.signedValue, object->panel.contentTop.signedValue + ITEM_MENU_SELECTION_SEPARATOR_Y_PIXELS);
    uiUpdateList(list, &object->panel);

    GP_FIND_SPARE_ARMOR(foundArmorItemId, list->selectedItemIndex);
    armorItemId = foundArmorItemId;
    itemMenuDrawEquipmentStats(object, armorItemId, ITEM_MENU_EQUIPMENT_STATS_COMPARE, 0);

    previewFlags = ITEM_MENU_PREVIEW_TEXTURE_RELOCATED | ITEM_MENU_PREVIEW_SCALE_EQUIPMENT;
    if (armorItemId == INVENTORY_ITEM_NONE) {
        previewFlags = ITEM_MENU_PREVIEW_TEXTURE_RELOCATED | ITEM_MENU_PREVIEW_SCALE_EQUIPMENT | ITEM_MENU_PREVIEW_HIDDEN;
    } else {
        if (((object->panel.control.word >> ITEM_MENU_SELECTION_SUSPENDED_CONTROL_SHIFT) == USER_INTERFACE_PANEL_ACTIVE) || (object->panel.control.word == USER_INTERFACE_PANEL_ACTIVE)) {
            GP_SET_PREVIEW_ITEM(armorItemId, CD_COMMAND_DISPLAY_LOAD_RELOCATED_PREVIEW);
        }
        if (cdCmdIsIdle() == 0) {
            previewFlags |= ITEM_MENU_PREVIEW_HIDDEN;
        }
    }
    itemMenuDrawPreview(object, object->panel.contentLeft.signedValue + 2, object->panel.contentTop.signedValue + 2, previewFlags);

    if (object->panel.control.word == USER_INTERFACE_PANEL_ACTIVE) {
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskMenu) != 0) {
            object->result = USER_INTERFACE_RESULT_CANCEL;
        } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskCancel) != 0) {
            sndEvtRequestScriptStart(SOUND_MENU_CANCEL, 0, 0);
            object->result = USER_INTERFACE_RESULT_DISMISS;
        }
    }

    // Closing accepted children restores focus; dismissal exits this picker.
    _itemMenuApplyChildDialogResults(object, task);

    if (object->result == USER_INTERFACE_RESULT_DISMISS) {
        object->result = USER_INTERFACE_RESULT_CONFIRM;
    }
}

void itemMenuReloadNoticeTask(Task* task)
{
    enum {
        ITEM_MENU_RELOAD_NOTICE_START          = 0,
        ITEM_MENU_RELOAD_NOTICE_LOADED         = 1,
        ITEM_MENU_RELOAD_NOTICE_REMOVED_ITEM   = 0x10,
        ITEM_MENU_RELOAD_NOTICE_REMOVED_LOADS  = 0x20,
        ITEM_MENU_RELOAD_ARGUMENT_ITEM_MASK    = 0xFF,
        ITEM_MENU_RELOAD_ARGUMENT_WEAPON_SHIFT = 8
    };
    UiObject*                  obj;
    const u8*                  itemText;
    s32                        consumableItemId;
    s32                        weaponItemId;
    s32                        contentWidth;
    s32                        verbWidth;
    s32                        textRows;
    s32                        textColorRgb;
    s32                        drawMode;
    s32                        textEndX;
    const EquipmentWeaponLoad* weaponLoad;

    obj              = task->spawnArg2.pointer;
    consumableItemId = task->spawnArg1.value & ITEM_MENU_RELOAD_ARGUMENT_ITEM_MASK;
    weaponItemId     = (task->spawnArg1.value >> ITEM_MENU_RELOAD_ARGUMENT_WEAPON_SHIFT) & ITEM_MENU_RELOAD_ARGUMENT_ITEM_MASK;
    obj->result      = USER_INTERFACE_RESULT_NONE;
    uiDrawPanelLabel(&obj->panel, Gp_StrReload);
    if (task->state == ITEM_MENU_RELOAD_NOTICE_START) {
        if (consumableItemId == INVENTORY_ITEM_NONE) {
            // Preserve the selected item's name before clearing its load record.
            weaponLoad  = equipmentGetWeaponLoad(weaponItemId);
            task->state = ITEM_MENU_RELOAD_NOTICE_REMOVED_ITEM;
            if (Gp_ReloadMode == EQUIPMENT_CLEAR_LOAD_PRIMARY) {
                if (weaponLoad->primaryItemId != INVENTORY_ITEM_NONE) {
                    task->spawnArg1.value |= weaponLoad->primaryItemId;
                    itemText               = itemGetText(weaponLoad->primaryItemId, ITEM_TEXT_NAME, 0);
                } else {
                    task->state = ITEM_MENU_RELOAD_NOTICE_REMOVED_LOADS;
                    itemText    = (const u8*)Gp_StrRemovedAmmo;
                }
            } else if (Gp_ReloadMode == EQUIPMENT_CLEAR_LOAD_SECONDARY) {
                if (weaponLoad->secondaryItemId != INVENTORY_ITEM_NONE) {
                    task->spawnArg1.value |= weaponLoad->secondaryItemId;
                    itemText               = itemGetText(weaponLoad->secondaryItemId, ITEM_TEXT_NAME, 0);
                } else {
                    task->state = ITEM_MENU_RELOAD_NOTICE_REMOVED_LOADS;
                    itemText    = (const u8*)Gp_StrRemovedAmmo;
                }
            } else {
                task->state = ITEM_MENU_RELOAD_NOTICE_REMOVED_LOADS;
                itemText    = (const u8*)Gp_StrRemovedAmmo;
            }
            equipmentClearSelectedRemovableLoads(weaponItemId, Gp_ReloadMode);
            verbWidth = textMeasureLineWidth((const u8*)Gp_StrRemoved);
        } else {
            itemSetIdentified(consumableItemId, 1);
            itemText = itemGetText(consumableItemId, ITEM_TEXT_NAME, 0);
            equipmentLoadWeaponConsumable(&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems, weaponItemId, consumableItemId, EQUIPMENT_WEAPON_LOAD_TO_CAPACITY);
            verbWidth   = textMeasureLineWidth((const u8*)Gp_StrLoaded);
            task->state = ITEM_MENU_RELOAD_NOTICE_LOADED;
        }
        if (task->state != ITEM_MENU_RELOAD_NOTICE_REMOVED_LOADS) {
            contentWidth = textMeasureLineWidth(itemText) + 0xB;
            if (contentWidth < verbWidth) {
                contentWidth = verbWidth;
            }
            textRows = 2;
        } else {
            contentWidth = textMeasureLineWidth(itemText);
            textRows     = 1;
        }
        uiSetPanelContentSize(&obj->panel, contentWidth + 5, uiGetTextRowsHeight(textRows) + 1);
        obj->panel.bounds.rect.x = (-obj->panel.bounds.rect.w) >> 1;
        if (task->state < ITEM_MENU_RELOAD_NOTICE_REMOVED_LOADS) {
            if (task->state < ITEM_MENU_RELOAD_NOTICE_REMOVED_ITEM) {
                sndEvtRequestScriptStart(SOUND_AMMO_LOAD, 0, 0);
            } else {
                sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
            }
        } else {
            sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
        }
        task->killCountdown = ITEM_MENU_NOTICE_TIMEOUT_TICKS;
    } else if (task->state < ITEM_MENU_RELOAD_NOTICE_REMOVED_LOADS) {
        itemText = itemGetText(consumableItemId, ITEM_TEXT_NAME, 0);
        if (task->state < ITEM_MENU_RELOAD_NOTICE_REMOVED_ITEM) {
            textDrawUiLine(obj, obj->panel.contentLeft.signedValue + 2, obj->panel.contentTop.signedValue + 0xF, (const u8*)Gp_StrLoaded, ITEM_MENU_NOTICE_TEXT_COLOR_RGB, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
        } else {
            textDrawUiLine(obj, obj->panel.contentLeft.signedValue + 2, obj->panel.contentTop.signedValue + 0xF, (const u8*)Gp_StrRemoved, ITEM_MENU_NOTICE_TEXT_COLOR_RGB, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
        }
        drawMode     = TEXT_DRAW_OUTLINED;
        textEndX     = textDrawUiLine(obj, obj->panel.contentLeft.signedValue + 2, obj->panel.contentTop.signedValue + 0x1E, itemText, ITEM_MENU_NOTICE_ITEM_COLOR_RGB, drawMode, TEXT_ALIGNMENT_LEFT);
        textColorRgb = ITEM_MENU_NOTICE_TEXT_COLOR_RGB;
        textDrawUiLine(obj, textEndX, obj->panel.contentTop.signedValue + 0x1E, (const u8*)Gp_StrDot, textColorRgb, drawMode, TEXT_ALIGNMENT_LEFT);
    } else {
        textDrawUiLine(obj, obj->panel.contentLeft.signedValue + 2, obj->panel.contentTop.signedValue + 0xF, (const u8*)Gp_StrRemovedAmmo, ITEM_MENU_NOTICE_TEXT_COLOR_RGB, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    }
    _itemMenuUpdateNoticeResult(obj, task);
}

void itemMenuAttachNoticeTask(Task* task)
{
    enum {
        ITEM_MENU_ATTACH_NOTICE_START         = 0,
        ITEM_MENU_ATTACH_NOTICE_WAIT_FOR_OPEN = 1
    };
    UiObject* obj;
    const u8* itemText;
    s32       textColorRgb;
    s32       contentWidth;
    s32       textEndX;

    obj         = task->spawnArg2.pointer;
    obj->result = USER_INTERFACE_RESULT_NONE;
    itemText    = itemGetText(task->spawnArg1.value, ITEM_TEXT_NAME, 0);
    if (task->state == ITEM_MENU_ATTACH_NOTICE_START) {
        contentWidth = textMeasureLineWidth(itemText) + 0x40;
        uiSetPanelContentSize(&obj->panel, contentWidth, uiGetTextRowsHeight(2) + 8);
        obj->panel.bounds.rect.x = (-obj->panel.bounds.rect.w) >> 1;
        task->killCountdown      = ITEM_MENU_NOTICE_TIMEOUT_TICKS;
        task->state              = task->state + 1;
    } else if (task->state == ITEM_MENU_ATTACH_NOTICE_WAIT_FOR_OPEN) {
        if (obj->panel.state == USER_INTERFACE_PANEL_OPEN) {
            sndEvtRequestScriptStart(SOUND_WEAPON_EQUIP, 0, 0);
            task->state = task->state + 1;
        }
    }
    uiDrawPanelLabel(&obj->panel, Gp_StrAttach);
    textColorRgb = ITEM_MENU_NOTICE_TEXT_COLOR_RGB;
    textDrawUiLine(obj, obj->panel.contentLeft.signedValue + 6, 0, (const u8*)Gp_StrEquipped, textColorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    textEndX = textDrawUiLine(obj, obj->panel.contentLeft.signedValue + 6, 0xE, itemText, ITEM_MENU_NOTICE_ITEM_COLOR_RGB, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    textDrawUiLine(obj, textEndX, 0xE, (const u8*)Gp_StrDot, textColorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    _itemMenuUpdateNoticeResult(obj, task);
}

void itemMenuEquipNoticeTask(Task* task)
{
    enum {
        ITEM_MENU_EQUIP_NOTICE_START         = 0,
        ITEM_MENU_EQUIP_NOTICE_WAIT_FOR_OPEN = 1
    };
    UiObject* obj;
    const u8* itemText;
    s32       textColorRgb;
    s32       contentWidth;
    s32       itemId;
    s32       verbWidth;
    s32       textEndX;

    obj         = task->spawnArg2.pointer;
    obj->result = USER_INTERFACE_RESULT_NONE;
    if (task->state == ITEM_MENU_EQUIP_NOTICE_START) {
        // Apply the selection before sizing and displaying its confirmation.
        itemId = task->spawnArg1.value;
        if ((u32)(itemId - EQUIPMENT_WEAPON_ITEM_FIRST) < ITEM_MENU_EQUIPMENT_ITEM_COUNT_U) {
            _equipmentEquipCarriedWeapon(itemId);
        } else if ((u32)(itemId - ITEM_MENU_ARMOR_ITEM_FIRST) < ITEM_MENU_EQUIPMENT_ITEM_COUNT_U) {
            equipmentEquipCarriedArmor(itemId);
        }
        contentWidth = textMeasureLineWidth(itemGetText(task->spawnArg1.value, ITEM_TEXT_NAME, 0)) + 0xB;
        verbWidth    = textMeasureLineWidth((const u8*)Gp_StrEquipped);
        if (contentWidth < verbWidth) {
            contentWidth = verbWidth;
        }
        uiSetPanelContentSize(&obj->panel, contentWidth + 5, uiGetTextRowsHeight(2) + 1);
        obj->panel.bounds.rect.x = (-obj->panel.bounds.rect.w) >> 1;
        task->killCountdown      = ITEM_MENU_NOTICE_TIMEOUT_TICKS;
        task->state              = task->state + 1;
    } else if (task->state == ITEM_MENU_EQUIP_NOTICE_WAIT_FOR_OPEN) {
        if (obj->panel.state == USER_INTERFACE_PANEL_OPEN) {
            if ((u32)(task->spawnArg1.value - EQUIPMENT_WEAPON_ITEM_FIRST) < ITEM_MENU_EQUIPMENT_ITEM_COUNT_U) {
                sndEvtRequestScriptStart(SOUND_WEAPON_EQUIP, 0, 0);
            } else {
                sndEvtRequestScriptStart(SOUND_SYSTEM_CONFIRM, 0, 0);
            }
            task->state = task->state + 1;
        }
    }
    uiDrawPanelLabel(&obj->panel, Gp_StrEquip);
    itemText     = itemGetText(task->spawnArg1.value, ITEM_TEXT_NAME, 0);
    textColorRgb = ITEM_MENU_NOTICE_TEXT_COLOR_RGB;
    textDrawUiLine(obj, obj->panel.contentLeft.signedValue + 2, obj->panel.contentTop.signedValue + 0xF, (const u8*)Gp_StrEquipped, textColorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    textEndX = textDrawUiLine(obj, obj->panel.contentLeft.signedValue + 2, obj->panel.contentTop.signedValue + 0x1E, itemText, ITEM_MENU_NOTICE_ITEM_COLOR_RGB, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    textDrawUiLine(obj, textEndX, obj->panel.contentTop.signedValue + 0x1E, (const u8*)Gp_StrDot, textColorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    _itemMenuUpdateNoticeResult(obj, task);
}

/// Draws a medium-font outlined equipment command at the current list row.
///
/// Borrows list, object and encoded label for this draw. Adds unsigned halfword
/// row coordinates to the panel's unsigned content origin in pixel units, then
/// narrows the text position to s16. Uses the row's packed RGB and left alignment.
/// Hidden panels still draw. label must satisfy `textDrawString`'s text contract;
/// font textures and palettes must be loaded, with writable primitive storage
/// and both panel otIndex + 1 and + 2 available for the fill and outline.
static inline void _itemMenuDrawEquipmentCommandLabel(const UiList* list, const UiObject* object, const u8* label)
{
    TextDrawReq labelRequest;

    labelRequest.x          = object->panel.contentOriginX.unsignedValue + list->rowTextX.unsignedValue;
    labelRequest.y          = object->panel.contentOriginY.unsignedValue + list->rowTextY.unsignedValue;
    labelRequest.otIndex    = object->panel.otIndex.signedValue + 1;
    labelRequest.colorRgb   = list->colorRgb;
    labelRequest.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
    labelRequest.alignment  = TEXT_ALIGNMENT_LEFT;
    labelRequest.drawMode   = TEXT_DRAW_OUTLINED;
    textDrawString(&labelRequest, label);
}

void itemMenuDrawLoadRow(UiList* list, UiObject* object)
{
    enum {
        ITEM_MENU_LOAD_WEAPON_CHOICE_PANEL = 38,
        ITEM_MENU_LOAD_SLOT_CHOICE_PANEL   = 40
    };
    UiObject* loadDialog;
    s32       selectedItemId;

    _itemMenuDrawEquipmentCommandLabel(list, object, (const u8*)Gp_StrLoad);
    if (list->rowInputEnabled == USER_INTERFACE_LIST_ROW_ACTIVE) {
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm) != 0) {
            // Capture the selected row before the sound request; it remains the dialog payload.
            selectedItemId = Gp_SelItemRec->itemId;
            sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
            if ((u32)(selectedItemId - EQUIPMENT_WEAPON_ITEM_FIRST) < ITEM_MENU_EQUIPMENT_ITEM_COUNT_U) {
                Gp_ReloadMode = EQUIPMENT_CLEAR_LOAD_BOTH;
                loadDialog    = uiSpawnObject(&D_8010EAB4[ITEM_MENU_LOAD_SLOT_CHOICE_PANEL], selectedItemId, USER_INTERFACE_PANEL_ACTIVE, ITEM_MENU_COMMAND_OPEN_DELAY_TICKS, object);
            } else if ((u32)(selectedItemId - INVENTORY_CONSUMABLE_ITEM_FIRST) < (u32)INVENTORY_CONSUMABLE_ITEM_COUNT) {
                loadDialog = uiSpawnObject(&D_8010EAB4[ITEM_MENU_LOAD_WEAPON_CHOICE_PANEL], selectedItemId, USER_INTERFACE_PANEL_ACTIVE, ITEM_MENU_COMMAND_OPEN_DELAY_TICKS, object);
            } else {
                return;
            }
            if (loadDialog != NULL) {
                uiPositionRowDialog(&loadDialog->panel, list, &object->panel);
            }
            // A recognized command consumes input even if its child allocation failed.
            object->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
            list->actionResult         = USER_INTERFACE_LIST_ACTION_INPUT_CONSUMED;
        }
    }
}

void itemMenuDrawExchangeRow(UiList* list, UiObject* object)
{
    enum {
        ITEM_MENU_EXCHANGE_ARMOR_CHOICE_PANEL  = 18,
        ITEM_MENU_EXCHANGE_AMMO_CHOICE_PANEL   = 19,
        ITEM_MENU_EXCHANGE_WEAPON_CHOICE_PANEL = 20,
        ITEM_MENU_EXCHANGE_OPEN_DELAY_TICKS    = 16
    };
    UiObject* exchangeDialog;
    s32       selectedItemId;

    _itemMenuDrawEquipmentCommandLabel(list, object, (const u8*)Gp_StrExchange);
    if (list->rowInputEnabled == USER_INTERFACE_LIST_ROW_ACTIVE) {
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm) != 0) {
            sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
            selectedItemId = INVENTORY_ITEM_NONE;
            if (Gp_SelItemRec != NULL) {
                selectedItemId = Gp_SelItemRec->itemId;
            }
            // An empty or consumable slot chooses ammunition for the equipped weapon.
            if (((u32)(selectedItemId - INVENTORY_CONSUMABLE_ITEM_FIRST) < (u32)INVENTORY_CONSUMABLE_ITEM_COUNT) || (selectedItemId == INVENTORY_ITEM_NONE)) {
                exchangeDialog = uiSpawnObject(&D_8010EAB4[ITEM_MENU_EXCHANGE_AMMO_CHOICE_PANEL], gPlayerStatus.weapon + (EQUIPMENT_WEAPON_ITEM_FIRST - 1), USER_INTERFACE_PANEL_ACTIVE, ITEM_MENU_EXCHANGE_OPEN_DELAY_TICKS, object);
            } else if ((u32)(selectedItemId - EQUIPMENT_WEAPON_ITEM_FIRST) < ITEM_MENU_EQUIPMENT_ITEM_COUNT_U) {
                exchangeDialog = uiSpawnObject(&D_8010EAB4[ITEM_MENU_EXCHANGE_WEAPON_CHOICE_PANEL], INVENTORY_ITEM_NONE, USER_INTERFACE_PANEL_ACTIVE, ITEM_MENU_EXCHANGE_OPEN_DELAY_TICKS, object);
            } else if ((u32)(selectedItemId - ITEM_MENU_ARMOR_ITEM_FIRST) < ITEM_MENU_EQUIPMENT_ITEM_COUNT_U) {
                exchangeDialog = uiSpawnObject(&D_8010EAB4[ITEM_MENU_EXCHANGE_ARMOR_CHOICE_PANEL], INVENTORY_ITEM_NONE, USER_INTERFACE_PANEL_ACTIVE, ITEM_MENU_EXCHANGE_OPEN_DELAY_TICKS, object);
            } else {
                return;
            }
            if (exchangeDialog != NULL) {
                exchangeDialog->panel.bounds.rect.y = -0x5C;
                exchangeDialog->panel.bounds.rect.x = -8;
            }
            // A recognized command consumes input even if its child allocation failed.
            object->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
            list->actionResult         = USER_INTERFACE_LIST_ACTION_INPUT_CONSUMED;
        }
    }
}
